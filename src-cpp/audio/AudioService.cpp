#define MINIAUDIO_IMPLEMENTATION
#include "audio/AudioService.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace celia {
namespace {

std::string normalize_mode_id(std::string value) {
    value.erase(
        std::remove_if(value.begin(), value.end(), [](unsigned char ch) {
            return ch == '\r' || ch == '\n' || ch == '\t' || ch == ' ';
        }),
        value.end());
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string to_utf8_path(const std::filesystem::path& path) {
    return path.u8string();
}

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool contains_any(const std::string& value, const std::vector<std::string>& needles) {
    for (const auto& needle : needles) {
        if (value.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

void write_le16(std::ostream& output, std::uint16_t value) {
    output.put(static_cast<char>(value & 0xFF));
    output.put(static_cast<char>((value >> 8) & 0xFF));
}

void write_le32(std::ostream& output, std::uint32_t value) {
    output.put(static_cast<char>(value & 0xFF));
    output.put(static_cast<char>((value >> 8) & 0xFF));
    output.put(static_cast<char>((value >> 16) & 0xFF));
    output.put(static_cast<char>((value >> 24) & 0xFF));
}

void write_wav_header(std::ostream& output, std::uint32_t sample_rate, std::uint64_t sample_count) {
    const auto data_bytes64 = sample_count * sizeof(std::int16_t);
    const auto data_bytes = static_cast<std::uint32_t>((std::min)(data_bytes64, static_cast<std::uint64_t>(0xFFFFFFFF)));
    output.seekp(0, std::ios::beg);
    output.write("RIFF", 4);
    write_le32(output, 36U + data_bytes);
    output.write("WAVE", 4);
    output.write("fmt ", 4);
    write_le32(output, 16);
    write_le16(output, 1);
    write_le16(output, 1);
    write_le32(output, sample_rate);
    write_le32(output, sample_rate * sizeof(std::int16_t));
    write_le16(output, sizeof(std::int16_t));
    write_le16(output, 16);
    output.write("data", 4);
    write_le32(output, data_bytes);
}

void append_pcm16(std::ostream& output, const float* samples, std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) {
        const float clamped = std::clamp(samples[index], -1.0F, 1.0F);
        const auto pcm = static_cast<std::int16_t>(std::lrint(clamped * 32767.0F));
        write_le16(output, static_cast<std::uint16_t>(pcm));
    }
}

} // namespace

std::string audio_processing_mode_id(AudioProcessingMode mode) {
    switch (mode) {
    case AudioProcessingMode::Raw:
        return "raw";
    case AudioProcessingMode::SherpaOfficial:
        return "sherpa-official";
    case AudioProcessingMode::CustomDsp:
    default:
        return "custom-dsp";
    }
}

AudioProcessingMode audio_processing_mode_from_id(std::string value) {
    value = normalize_mode_id(std::move(value));
    if (value == "raw" || value == "none" || value == "no-ns-vad") {
        return AudioProcessingMode::Raw;
    }
    if (value == "sherpa-official" || value == "sherpa" || value == "official" || value == "silero-gtcrn") {
        return AudioProcessingMode::SherpaOfficial;
    }
    return AudioProcessingMode::CustomDsp;
}

AudioService::AudioService() = default;

AudioService::~AudioService() {
    stop_recording();
}

void AudioService::load_sherpa_onnx_model(const SherpaOnnxModelPaths& model_paths) {
    transcription_.start(model_paths);
}

void AudioService::configure_processing(const AudioProcessingConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (recording_) {
        throw std::runtime_error("Khong the doi audio processing mode khi dang thu am.");
    }

    processing_config_ = config;
    reset_official_processing();
    switch (processing_config_.mode) {
    case AudioProcessingMode::Raw:
        base_processing_details_ = "Raw mic waveform -> sherpa-onnx ASR, no NS/VAD";
        break;
    case AudioProcessingMode::SherpaOfficial:
        base_processing_details_ = "Sherpa official GTCRN NS + Silero VAD";
        break;
    case AudioProcessingMode::CustomDsp:
    default:
        base_processing_details_ = "Custom lightweight DSP NS/VAD";
        break;
    }
    refresh_processing_details();
}

void AudioService::start_recording() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (recording_) {
        return;
    }

    ensure_official_processing_ready();

    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.format = ma_format_f32;
    config.capture.channels = 1;
    config.sampleRate = 16000;
    config.dataCallback = &AudioService::data_callback;
    config.pUserData = this;

    const ma_result init_result = ma_device_init(nullptr, &config, &device_);
    if (init_result != MA_SUCCESS) {
        throw std::runtime_error(
            std::string("Không khởi tạo được thiết bị micro qua miniaudio: ") +
            ma_result_description(init_result));
    }
    initialized_ = true;

    const ma_result start_result = ma_device_start(&device_);
    if (start_result != MA_SUCCESS) {
        ma_device_uninit(&device_);
        initialized_ = false;
        throw std::runtime_error(
            std::string("Không chạy được stream micro qua miniaudio: ") +
            ma_result_description(start_result));
    }

    char name[MA_MAX_DEVICE_NAME_LENGTH + 1] = {};
    if (ma_device_get_name(&device_, ma_device_type_capture, name, sizeof(name), nullptr) == MA_SUCCESS && name[0] != '\0') {
        device_name_ = name;
    } else {
        device_name_ = "Default capture device";
    }

    sample_rate_ = device_.sampleRate;
    channels_ = device_.capture.channels;
    classify_input_device();
    refresh_processing_details();
    dc_last_input_ = 0.0F;
    dc_last_output_ = 0.0F;
    noise_floor_rms_ = 0.01F;
    vad_hangover_callbacks_ = 0;
    speech_frames_total_ = 0;
    rms_.store(0.0F);
    peak_.store(0.0F);
    processed_rms_.store(0.0F);
    processed_peak_.store(0.0F);
    noise_floor_.store(noise_floor_rms_);
    vad_probability_.store(0.0F);
    vad_active_.store(false);
    speech_frames_.store(0);
    updated_at_ms_.store(current_time_millis());
    raw_callback_buffer_.clear();
    processed_callback_buffer_.clear();
    official_denoiser_input_buffer_.clear();
    start_diagnostics_capture();
    recording_ = true;
    transcription_.start_session();
}

void AudioService::stop_recording() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) {
        ma_device_stop(&device_);
        ma_device_uninit(&device_);
        initialized_ = false;
    }

    transcription_.stop_session();
    stop_diagnostics_capture();
    recording_ = false;
    sample_rate_ = 0;
    channels_ = 0;
    device_name_ = "No active input";
    dc_last_input_ = 0.0F;
    dc_last_output_ = 0.0F;
    noise_floor_rms_ = 0.01F;
    vad_hangover_callbacks_ = 0;
    speech_frames_total_ = 0;
    rms_.store(0.0F);
    peak_.store(0.0F);
    processed_rms_.store(0.0F);
    processed_peak_.store(0.0F);
    noise_floor_.store(0.0F);
    vad_probability_.store(0.0F);
    vad_active_.store(false);
    speech_frames_.store(0);
    updated_at_ms_.store(0);
    std::vector<float>().swap(raw_callback_buffer_);
    std::vector<float>().swap(processed_callback_buffer_);
    std::vector<float>().swap(official_denoiser_input_buffer_);
    reset_official_processing();
}

AudioLevel AudioService::input_level() const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto transcription_snapshot = transcription_.snapshot();
    if (!recording_) {
        AudioLevel idle_level{};
        idle_level.transcription_status = transcription_snapshot.status;
        idle_level.transcript = transcription_snapshot.transcript;
        idle_level.processing_mode = audio_processing_mode_id(processing_config_.mode);
        idle_level.processing_details = processing_details_;
        idle_level.input_profile = input_profile_;
        idle_level.diagnostics_status = processing_config_.diagnostics_enabled ? "enabled" : "disabled";
        idle_level.raw_diagnostics_path = to_utf8_path(raw_diagnostics_path_);
        idle_level.processed_diagnostics_path = to_utf8_path(processed_diagnostics_path_);
        return idle_level;
    }

    return AudioLevel{
        rms_.load(),
        peak_.load(),
        processed_rms_.load(),
        processed_peak_.load(),
        noise_floor_.load(),
        vad_probability_.load(),
        sample_rate_,
        channels_,
        device_name_,
        "recording",
        vad_active_.load(),
        speech_frames_.load(),
        updated_at_ms_.load(),
        transcription_snapshot.status,
        transcription_snapshot.transcript,
        audio_processing_mode_id(processing_config_.mode),
        processing_details_,
        input_profile_,
        diagnostics_active_ ? "recording" : (processing_config_.diagnostics_enabled ? "ready" : "disabled"),
        to_utf8_path(raw_diagnostics_path_),
        to_utf8_path(processed_diagnostics_path_)
    };
}

void AudioService::data_callback(ma_device* device, void* output, const void* input, ma_uint32 frame_count) {
    (void)output;
    if (device == nullptr || input == nullptr || device->pUserData == nullptr) {
        return;
    }

    auto* service = static_cast<AudioService*>(device->pUserData);
    service->update_level(static_cast<const float*>(input), frame_count, device->capture.channels);
}

void AudioService::update_level(const float* samples, ma_uint32 frame_count, ma_uint32 channels) {
    if (samples == nullptr || frame_count == 0) {
        return;
    }

    const ma_uint32 channel_count = channels > 1 ? channels : 1;
    float raw_sum = 0.0F;
    float raw_peak = 0.0F;
    float processed_sum = 0.0F;
    float processed_peak = 0.0F;
    raw_callback_buffer_.resize(frame_count);
    processed_callback_buffer_.resize(frame_count);

    for (ma_uint32 frame = 0; frame < frame_count; ++frame) {
        const float sample = std::clamp(samples[frame * channel_count], -1.0F, 1.0F);
        raw_callback_buffer_[frame] = sample;
        const float abs_sample = std::fabs(sample);
        raw_peak = (std::max)(raw_peak, abs_sample);
        raw_sum += sample * sample;
    }

    const auto mode = processing_config_.mode;
    const float* asr_samples = raw_callback_buffer_.data();
    std::size_t asr_sample_count = raw_callback_buffer_.size();
    bool vad_active = false;
    float probability = 0.0F;

    if (mode == AudioProcessingMode::CustomDsp) {
        for (ma_uint32 frame = 0; frame < frame_count; ++frame) {
            const float filtered = process_noise_suppression(raw_callback_buffer_[frame]);
            processed_callback_buffer_[frame] = filtered;
            const float abs_filtered = std::fabs(filtered);
            processed_peak = (std::max)(processed_peak, abs_filtered);
            processed_sum += filtered * filtered;
        }

        const float filtered_rms = std::sqrt(processed_sum / static_cast<float>(frame_count));
        const float speech_floor = (std::max)(dsp_speech_floor_min_, noise_floor_rms_ * dsp_speech_floor_multiplier_);
        const float speech_ratio = filtered_rms / (speech_floor + std::numeric_limits<float>::epsilon());
        probability = std::clamp((speech_ratio - 0.65F) / 1.35F, 0.0F, 1.0F);
        const bool speech_now =
            probability > dsp_vad_on_probability_ &&
            processed_peak > (std::max)(dsp_peak_min_, noise_floor_rms_ * dsp_peak_noise_multiplier_);

        if (speech_now) {
            vad_hangover_callbacks_ = 10;
            speech_frames_total_ += frame_count;
        } else if (vad_hangover_callbacks_ > 0) {
            --vad_hangover_callbacks_;
        }

        vad_active = speech_now || vad_hangover_callbacks_ > 0;
        if (!vad_active) {
            noise_floor_rms_ = (noise_floor_rms_ * (1.0F - dsp_noise_alpha_)) + (filtered_rms * dsp_noise_alpha_);
            noise_floor_rms_ = std::clamp(noise_floor_rms_, dsp_noise_min_, dsp_noise_max_);
        }

        asr_samples = processed_callback_buffer_.data();
        asr_sample_count = processed_callback_buffer_.size();
    } else if (mode == AudioProcessingMode::SherpaOfficial && official_denoiser_ != nullptr && official_vad_ != nullptr) {
        official_vad_->AcceptWaveform(
            raw_callback_buffer_.data(),
            static_cast<int32_t>(raw_callback_buffer_.size()));
        vad_active = official_vad_->IsDetected();
        probability = vad_active ? 1.0F : 0.0F;
        if (vad_active) {
            speech_frames_total_ += frame_count;
        }
        drain_official_vad_segments();

        processed_callback_buffer_.clear();
        official_denoiser_input_buffer_.insert(
            official_denoiser_input_buffer_.end(),
            raw_callback_buffer_.begin(),
            raw_callback_buffer_.end());

        const auto frame_shift = (std::max)(1, official_denoiser_->GetFrameShiftInSamples());
        while (official_denoiser_input_buffer_.size() >= static_cast<std::size_t>(frame_shift)) {
            const auto denoised = official_denoiser_->Run(
                official_denoiser_input_buffer_.data(),
                frame_shift,
                16000);
            processed_callback_buffer_.insert(
                processed_callback_buffer_.end(),
                denoised.samples.begin(),
                denoised.samples.end());
            official_denoiser_input_buffer_.erase(
                official_denoiser_input_buffer_.begin(),
                official_denoiser_input_buffer_.begin() + frame_shift);
        }

        if (!processed_callback_buffer_.empty()) {
            for (const auto sample : processed_callback_buffer_) {
                const float clamped = std::clamp(sample, -1.0F, 1.0F);
                const float abs_sample = std::fabs(clamped);
                processed_peak = (std::max)(processed_peak, abs_sample);
                processed_sum += clamped * clamped;
            }

            asr_samples = processed_callback_buffer_.data();
            asr_sample_count = processed_callback_buffer_.size();
        } else {
            asr_sample_count = 0;
        }
    } else {
        processed_callback_buffer_ = raw_callback_buffer_;
        processed_sum = raw_sum;
        processed_peak = raw_peak;
        asr_samples = raw_callback_buffer_.data();
        asr_sample_count = raw_callback_buffer_.size();
    }

    const float raw_rms = std::sqrt(raw_sum / static_cast<float>(frame_count));
    const auto processed_denominator = (std::max)(std::size_t{1}, processed_callback_buffer_.size());
    const float filtered_rms = std::sqrt(processed_sum / static_cast<float>(processed_denominator));

    rms_.store(raw_rms);
    peak_.store(raw_peak);
    processed_rms_.store(filtered_rms);
    processed_peak_.store(processed_peak);
    noise_floor_.store(noise_floor_rms_);
    vad_probability_.store(probability);
    vad_active_.store(vad_active);
    speech_frames_.store(speech_frames_total_);
    updated_at_ms_.store(current_time_millis());
    append_diagnostics(
        raw_callback_buffer_.data(),
        raw_callback_buffer_.size(),
        processed_callback_buffer_.empty() ? nullptr : processed_callback_buffer_.data(),
        processed_callback_buffer_.size());

    if (asr_samples != nullptr && asr_sample_count > 0) {
        const bool has_speech = mode == AudioProcessingMode::Raw ? true : vad_active;
        transcription_.push_audio(asr_samples, asr_sample_count, has_speech);
    }
}

float AudioService::process_noise_suppression(float sample) {
    // Lightweight real-time DSP: DC blocker/high-pass followed by a soft noise gate.
    // This is intentionally conservative and dependency-free until a production NS backend is selected.
    const float high_passed = sample - dc_last_input_ + (0.995F * dc_last_output_);
    dc_last_input_ = sample;
    dc_last_output_ = std::clamp(high_passed, -1.0F, 1.0F);

    const float gate_threshold = (std::max)(dsp_gate_min_, noise_floor_rms_ * dsp_gate_multiplier_);
    const float abs_sample = std::fabs(dc_last_output_);
    if (abs_sample <= gate_threshold) {
        return std::clamp(dc_last_output_ * dsp_gate_low_gain_ * dsp_output_gain_, -0.98F, 0.98F);
    }

    if (abs_sample <= gate_threshold * 2.5F) {
        const float mix = (abs_sample - gate_threshold) / (gate_threshold * 1.5F);
        const float gain = dsp_gate_low_gain_ + ((1.0F - dsp_gate_low_gain_) * std::clamp(mix, 0.0F, 1.0F));
        return std::clamp(dc_last_output_ * gain * dsp_output_gain_, -0.98F, 0.98F);
    }

    return std::clamp(dc_last_output_ * dsp_output_gain_, -0.98F, 0.98F);
}

void AudioService::classify_input_device() {
    const auto name = lower_copy(device_name_);
    input_profile_ = "near-field";
    dsp_output_gain_ = 1.0F;
    dsp_gate_min_ = 0.004F;
    dsp_gate_multiplier_ = 1.65F;
    dsp_gate_low_gain_ = 0.18F;
    dsp_speech_floor_min_ = 0.012F;
    dsp_speech_floor_multiplier_ = 3.0F;
    dsp_vad_on_probability_ = 0.52F;
    dsp_peak_min_ = 0.018F;
    dsp_peak_noise_multiplier_ = 4.0F;
    dsp_noise_alpha_ = 0.015F;
    dsp_noise_min_ = 0.0015F;
    dsp_noise_max_ = 0.08F;

    if (contains_any(name, {"bluetooth", "hands-free", "hands free", "handsfree", "ag audio", "hfp", "airpods", "buds"})) {
        input_profile_ = "bluetooth-speech";
        dsp_output_gain_ = 2.2F;
        dsp_gate_min_ = 0.0025F;
        dsp_gate_multiplier_ = 1.15F;
        dsp_gate_low_gain_ = 0.52F;
        dsp_speech_floor_min_ = 0.006F;
        dsp_speech_floor_multiplier_ = 2.0F;
        dsp_vad_on_probability_ = 0.34F;
        dsp_peak_min_ = 0.007F;
        dsp_peak_noise_multiplier_ = 2.1F;
        dsp_noise_alpha_ = 0.008F;
        dsp_noise_min_ = 0.001F;
        dsp_noise_max_ = 0.12F;
        return;
    }

    if (contains_any(name, {"array", "far-field", "far field", "raspberry", "webcam", "camera", "realsense"})) {
        input_profile_ = "far-field";
        dsp_output_gain_ = 1.7F;
        dsp_gate_min_ = 0.003F;
        dsp_gate_multiplier_ = 1.3F;
        dsp_gate_low_gain_ = 0.35F;
        dsp_speech_floor_min_ = 0.008F;
        dsp_speech_floor_multiplier_ = 2.35F;
        dsp_vad_on_probability_ = 0.42F;
        dsp_peak_min_ = 0.010F;
        dsp_peak_noise_multiplier_ = 2.8F;
        dsp_noise_alpha_ = 0.010F;
        dsp_noise_min_ = 0.001F;
        dsp_noise_max_ = 0.10F;
    }
}

void AudioService::refresh_processing_details() {
    processing_details_ = base_processing_details_ + " | profile=" + input_profile_;
    if (processing_config_.diagnostics_enabled) {
        processing_details_ += " | diagnostics=wav";
    }
}

void AudioService::start_diagnostics_capture() {
    stop_diagnostics_capture();
    raw_diagnostics_path_.clear();
    processed_diagnostics_path_.clear();
    if (!processing_config_.diagnostics_enabled || sample_rate_ == 0) {
        return;
    }

    const auto timestamp = std::to_string(current_time_millis());
    const auto dir = processing_config_.diagnostics_dir.empty()
        ? std::filesystem::current_path() / "audio-diagnostics"
        : processing_config_.diagnostics_dir;
    std::filesystem::create_directories(dir);

    raw_diagnostics_path_ = dir / ("raw-" + timestamp + ".wav");
    processed_diagnostics_path_ = dir / ("processed-" + timestamp + ".wav");
    raw_diagnostics_.open(raw_diagnostics_path_, std::ios::binary | std::ios::trunc);
    processed_diagnostics_.open(processed_diagnostics_path_, std::ios::binary | std::ios::trunc);
    if (!raw_diagnostics_ || !processed_diagnostics_) {
        raw_diagnostics_.close();
        processed_diagnostics_.close();
        raw_diagnostics_path_.clear();
        processed_diagnostics_path_.clear();
        return;
    }

    raw_diagnostics_samples_ = 0;
    processed_diagnostics_samples_ = 0;
    diagnostics_max_samples_ = static_cast<std::uint64_t>(sample_rate_) * (std::max)(1U, processing_config_.diagnostics_seconds);
    write_wav_header(raw_diagnostics_, sample_rate_, 0);
    write_wav_header(processed_diagnostics_, sample_rate_, 0);
    raw_diagnostics_.seekp(0, std::ios::end);
    processed_diagnostics_.seekp(0, std::ios::end);
    diagnostics_active_ = true;
}

void AudioService::stop_diagnostics_capture() {
    if (raw_diagnostics_.is_open()) {
        write_wav_header(raw_diagnostics_, sample_rate_ == 0 ? 16000 : sample_rate_, raw_diagnostics_samples_);
        raw_diagnostics_.close();
    }
    if (processed_diagnostics_.is_open()) {
        write_wav_header(processed_diagnostics_, sample_rate_ == 0 ? 16000 : sample_rate_, processed_diagnostics_samples_);
        processed_diagnostics_.close();
    }
    diagnostics_active_ = false;
}

void AudioService::append_diagnostics(
    const float* raw_samples,
    std::size_t raw_count,
    const float* processed_samples,
    std::size_t processed_count
) {
    if (!diagnostics_active_ || diagnostics_max_samples_ == 0) {
        return;
    }

    const auto raw_remaining = diagnostics_max_samples_ > raw_diagnostics_samples_
        ? diagnostics_max_samples_ - raw_diagnostics_samples_
        : 0;
    if (raw_samples != nullptr && raw_remaining > 0 && raw_diagnostics_.is_open()) {
        const auto writable = (std::min)(raw_count, static_cast<std::size_t>(raw_remaining));
        append_pcm16(raw_diagnostics_, raw_samples, writable);
        raw_diagnostics_samples_ += writable;
    }

    const auto processed_remaining = diagnostics_max_samples_ > processed_diagnostics_samples_
        ? diagnostics_max_samples_ - processed_diagnostics_samples_
        : 0;
    if (processed_samples != nullptr && processed_remaining > 0 && processed_diagnostics_.is_open()) {
        const auto writable = (std::min)(processed_count, static_cast<std::size_t>(processed_remaining));
        append_pcm16(processed_diagnostics_, processed_samples, writable);
        processed_diagnostics_samples_ += writable;
    }

    if (raw_diagnostics_samples_ >= diagnostics_max_samples_ &&
        processed_diagnostics_samples_ >= diagnostics_max_samples_) {
        stop_diagnostics_capture();
    }
}

void AudioService::ensure_official_processing_ready() {
    if (processing_config_.mode != AudioProcessingMode::SherpaOfficial) {
        return;
    }

    if (!std::filesystem::exists(processing_config_.gtcrn_denoiser_model)) {
        throw std::runtime_error("Thieu model sherpa official NS: " + processing_config_.gtcrn_denoiser_model.u8string());
    }
    if (!std::filesystem::exists(processing_config_.silero_vad_model)) {
        throw std::runtime_error("Thieu model sherpa official VAD: " + processing_config_.silero_vad_model.u8string());
    }

    namespace sx = sherpa_onnx::cxx;

    sx::OnlineSpeechDenoiserConfig denoiser_config;
    denoiser_config.model.gtcrn.model = to_utf8_path(processing_config_.gtcrn_denoiser_model);
    denoiser_config.model.num_threads = 1;
    denoiser_config.model.provider = "cpu";
    auto denoiser = std::make_unique<sx::OnlineSpeechDenoiser>(sx::OnlineSpeechDenoiser::Create(denoiser_config));
    if (denoiser->Get() == nullptr) {
        throw std::runtime_error("Khong load duoc sherpa OnlineSpeechDenoiser GTCRN.");
    }

    sx::VadModelConfig vad_config;
    vad_config.silero_vad.model = to_utf8_path(processing_config_.silero_vad_model);
    vad_config.silero_vad.threshold = 0.5F;
    vad_config.silero_vad.min_silence_duration = 0.35F;
    vad_config.silero_vad.min_speech_duration = 0.15F;
    vad_config.silero_vad.window_size = 512;
    vad_config.silero_vad.max_speech_duration = 20.0F;
    vad_config.sample_rate = 16000;
    vad_config.num_threads = 1;
    vad_config.provider = "cpu";
    auto vad = std::make_unique<sx::VoiceActivityDetector>(sx::VoiceActivityDetector::Create(vad_config, 30.0F));
    if (vad->Get() == nullptr) {
        throw std::runtime_error("Khong load duoc sherpa VoiceActivityDetector Silero.");
    }

    official_denoiser_ = std::move(denoiser);
    official_vad_ = std::move(vad);
}

void AudioService::reset_official_processing() {
    official_vad_.reset();
    official_denoiser_.reset();
    official_denoiser_input_buffer_.clear();
}

void AudioService::drain_official_vad_segments() const {
    if (official_vad_ == nullptr) {
        return;
    }

    while (!official_vad_->IsEmpty()) {
        official_vad_->Pop();
    }
}

std::uint64_t AudioService::current_time_millis() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

} // namespace celia
