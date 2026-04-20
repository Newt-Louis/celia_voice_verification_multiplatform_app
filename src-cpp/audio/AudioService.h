#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>

#include "audio/SherpaOnnxService.h"
#include "miniaudio.h"

namespace celia {

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
};

class AudioService {
public:
    AudioService();
    ~AudioService();

    AudioService(const AudioService&) = delete;
    AudioService& operator=(const AudioService&) = delete;

    void start_recording();
    void stop_recording();
    void load_sherpa_onnx_model(const SherpaOnnxModelPaths& model_paths);
    AudioLevel input_level() const;

private:
    static void data_callback(ma_device* device, void* output, const void* input, ma_uint32 frame_count);
    void update_level(const float* samples, ma_uint32 frame_count, ma_uint32 channels);
    float process_noise_suppression(float sample);
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
    std::atomic<float> rms_{0.0F};
    std::atomic<float> peak_{0.0F};
    std::atomic<float> processed_rms_{0.0F};
    std::atomic<float> processed_peak_{0.0F};
    std::atomic<float> noise_floor_{0.0F};
    std::atomic<float> vad_probability_{0.0F};
    std::atomic<bool> vad_active_{false};
    std::atomic<std::uint64_t> speech_frames_{0};
    std::atomic<std::uint64_t> updated_at_ms_{0};
    SherpaOnnxService transcription_;
};

} // namespace celia
