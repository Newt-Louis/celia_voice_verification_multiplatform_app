#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "audio/SherpaOnnxService.h"
#include "miniaudio.h"

namespace celia {

enum class AudioProcessingMode {
    CustomDsp,
    Raw,
    SherpaOfficial
};

struct AudioProcessingConfig {
    AudioProcessingMode mode = AudioProcessingMode::CustomDsp;
    std::filesystem::path silero_vad_model;
    std::filesystem::path gtcrn_denoiser_model;
    bool diagnostics_enabled = false;
    std::filesystem::path diagnostics_dir;
    std::uint32_t diagnostics_seconds = 15;
};

std::string audio_processing_mode_id(AudioProcessingMode mode);
AudioProcessingMode audio_processing_mode_from_id(std::string value);

struct AudioLevel {
    float rms = 0.0F;
    float peak = 0.0F;
    float processed_rms = 0.0F;
    float processed_peak = 0.0F;
    float noise_floor = 0.0F;
    float vad_probability = 0.0F;
    std::uint32_t sample_rate = 0;
    std::uint32_t channels = 0;
    std::string device_name = "No active input";
    std::string status = "idle";
    bool vad_active = false;
    std::uint64_t speech_frames = 0;
    std::uint64_t updated_at_ms = 0;
    std::string transcription_status = "idle";
    std::string transcript;
    std::string processing_mode = audio_processing_mode_id(AudioProcessingMode::CustomDsp);
    std::string processing_details = "Custom DSP NS/VAD";
    std::string input_profile = "near-field";
    std::string diagnostics_status = "disabled";
    std::string raw_diagnostics_path;
    std::string processed_diagnostics_path;
};

class AudioService {
public:
    AudioService();
    ~AudioService();

    AudioService(const AudioService&) = delete;
    AudioService& operator=(const AudioService&) = delete;

    void start_recording();
    void stop_recording();
    void configure_processing(const AudioProcessingConfig& config);
    void load_sherpa_onnx_model(const SherpaOnnxModelPaths& model_paths);
    AudioLevel input_level() const;

private:
    static void data_callback(ma_device* device, void* output, const void* input, ma_uint32 frame_count);
    void update_level(const float* samples, ma_uint32 frame_count, ma_uint32 channels);
    float process_noise_suppression(float sample);
    void classify_input_device();
    void refresh_processing_details();
    void start_diagnostics_capture();
    void stop_diagnostics_capture();
    void append_diagnostics(const float* raw_samples, std::size_t raw_count, const float* processed_samples, std::size_t processed_count);
    void ensure_official_processing_ready();
    void reset_official_processing();
    void drain_official_vad_segments() const;
    static std::uint64_t current_time_millis();

    // TODO(wakeword): Add low-power standby and wakeword gate before opening full transcription.
    // TODO(ecapa): Insert ECAPA-TDNN speaker verification before sherpa-onnx when speaker verification is ready.
    // TODO(library): Extract this pipeline into a public SDK surface after app demo stabilizes.
    mutable std::mutex mutex_;
    ma_device device_{};
    bool initialized_ = false;
    bool recording_ = false;
    std::uint32_t sample_rate_ = 0;
    std::uint32_t channels_ = 0;
    std::string device_name_ = "No active input";
    float dc_last_input_ = 0.0F;
    float dc_last_output_ = 0.0F;
    float noise_floor_rms_ = 0.01F;
    int vad_hangover_callbacks_ = 0;
    std::uint64_t speech_frames_total_ = 0;
    std::string input_profile_ = "near-field";
    float dsp_output_gain_ = 1.0F;
    float dsp_gate_min_ = 0.004F;
    float dsp_gate_multiplier_ = 1.65F;
    float dsp_gate_low_gain_ = 0.18F;
    float dsp_speech_floor_min_ = 0.012F;
    float dsp_speech_floor_multiplier_ = 3.0F;
    float dsp_vad_on_probability_ = 0.52F;
    float dsp_peak_min_ = 0.018F;
    float dsp_peak_noise_multiplier_ = 4.0F;
    float dsp_noise_alpha_ = 0.015F;
    float dsp_noise_min_ = 0.0015F;
    float dsp_noise_max_ = 0.08F;
    std::atomic<float> rms_{0.0F};
    std::atomic<float> peak_{0.0F};
    std::atomic<float> processed_rms_{0.0F};
    std::atomic<float> processed_peak_{0.0F};
    std::atomic<float> noise_floor_{0.0F};
    std::atomic<float> vad_probability_{0.0F};
    std::atomic<bool> vad_active_{false};
    std::atomic<std::uint64_t> speech_frames_{0};
    std::atomic<std::uint64_t> updated_at_ms_{0};
    AudioProcessingConfig processing_config_;
    std::string processing_details_ = "Custom DSP NS/VAD";
    std::string base_processing_details_ = "Custom DSP NS/VAD";
    std::vector<float> raw_callback_buffer_;
    std::vector<float> processed_callback_buffer_;
    std::vector<float> official_denoiser_input_buffer_;
    bool diagnostics_active_ = false;
    std::filesystem::path raw_diagnostics_path_;
    std::filesystem::path processed_diagnostics_path_;
    std::ofstream raw_diagnostics_;
    std::ofstream processed_diagnostics_;
    std::uint64_t raw_diagnostics_samples_ = 0;
    std::uint64_t processed_diagnostics_samples_ = 0;
    std::uint64_t diagnostics_max_samples_ = 0;
    std::unique_ptr<sherpa_onnx::cxx::OnlineSpeechDenoiser> official_denoiser_;
    std::unique_ptr<sherpa_onnx::cxx::VoiceActivityDetector> official_vad_;
    SherpaOnnxService transcription_;
};

} // namespace celia
