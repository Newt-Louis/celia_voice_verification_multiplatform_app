#define MINIAUDIO_IMPLEMENTATION
#include "audio/AudioService.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace celia {

AudioService::AudioService() = default;

AudioService::~AudioService() {
    stop_recording();
}

void AudioService::load_whisper_model(const std::filesystem::path& model_path) {
    whisper_.start(model_path);
}

void AudioService::start_recording() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (recording_) {
        return;
    }

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
    recording_ = true;
}

void AudioService::stop_recording() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_) {
        recording_ = false;
        return;
    }

    ma_device_stop(&device_);
    ma_device_uninit(&device_);
    initialized_ = false;
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
}

AudioLevel AudioService::input_level() const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto whisper_snapshot = whisper_.snapshot();
    if (!recording_) {
        AudioLevel idle_level{};
        idle_level.transcription_status = whisper_snapshot.status;
        idle_level.transcript = whisper_snapshot.transcript;
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
        whisper_snapshot.status,
        whisper_snapshot.transcript
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
    std::vector<float> processed_samples;
    processed_samples.reserve(frame_count);

    for (ma_uint32 frame = 0; frame < frame_count; ++frame) {
        const float sample = std::clamp(samples[frame * channel_count], -1.0F, 1.0F);
        const float abs_sample = std::fabs(sample);
        raw_peak = (std::max)(raw_peak, abs_sample);
        raw_sum += sample * sample;

        const float filtered = process_noise_suppression(sample);
        processed_samples.push_back(filtered);
        const float abs_filtered = std::fabs(filtered);
        processed_peak = (std::max)(processed_peak, abs_filtered);
        processed_sum += filtered * filtered;
    }

    const float raw_rms = std::sqrt(raw_sum / static_cast<float>(frame_count));
    const float filtered_rms = std::sqrt(processed_sum / static_cast<float>(frame_count));
    const float speech_floor = (std::max)(0.012F, noise_floor_rms_ * 3.0F);
    const float speech_ratio = filtered_rms / (speech_floor + std::numeric_limits<float>::epsilon());
    const float probability = std::clamp((speech_ratio - 0.65F) / 1.35F, 0.0F, 1.0F);
    const bool speech_now = probability > 0.52F && processed_peak > (std::max)(0.018F, noise_floor_rms_ * 4.0F);

    if (speech_now) {
        vad_hangover_callbacks_ = 10;
        speech_frames_total_ += frame_count;
    } else if (vad_hangover_callbacks_ > 0) {
        --vad_hangover_callbacks_;
    }

    const bool vad_active = speech_now || vad_hangover_callbacks_ > 0;
    if (!vad_active) {
        noise_floor_rms_ = (noise_floor_rms_ * 0.985F) + (filtered_rms * 0.015F);
        noise_floor_rms_ = std::clamp(noise_floor_rms_, 0.0015F, 0.08F);
    }

    rms_.store(raw_rms);
    peak_.store(raw_peak);
    processed_rms_.store(filtered_rms);
    processed_peak_.store(processed_peak);
    noise_floor_.store(noise_floor_rms_);
    vad_probability_.store(probability);
    vad_active_.store(vad_active);
    speech_frames_.store(speech_frames_total_);
    updated_at_ms_.store(current_time_millis());

    if (!processed_samples.empty()) {
        whisper_.push_audio(processed_samples.data(), processed_samples.size(), vad_active);
    }
}

float AudioService::process_noise_suppression(float sample) {
    // Lightweight real-time DSP: DC blocker/high-pass followed by a soft noise gate.
    // This is intentionally conservative and dependency-free until a production NS backend is selected.
    const float high_passed = sample - dc_last_input_ + (0.995F * dc_last_output_);
    dc_last_input_ = sample;
    dc_last_output_ = std::clamp(high_passed, -1.0F, 1.0F);

    const float gate_threshold = (std::max)(0.004F, noise_floor_rms_ * 1.65F);
    const float abs_sample = std::fabs(dc_last_output_);
    if (abs_sample <= gate_threshold) {
        return dc_last_output_ * 0.18F;
    }

    if (abs_sample <= gate_threshold * 2.5F) {
        const float mix = (abs_sample - gate_threshold) / (gate_threshold * 1.5F);
        const float gain = 0.18F + (0.82F * std::clamp(mix, 0.0F, 1.0F));
        return dc_last_output_ * gain;
    }

    return dc_last_output_;
}

std::uint64_t AudioService::current_time_millis() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

} // namespace celia
