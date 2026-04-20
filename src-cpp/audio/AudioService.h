#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

#include "miniaudio.h"

namespace celia {

struct AudioLevel {
    float rms = 0.0F;
    float peak = 0.0F;
    std::uint32_t sample_rate = 0;
    std::uint32_t channels = 0;
    std::string device_name = "No active input";
    std::string status = "idle";
    std::uint64_t updated_at_ms = 0;
};

class AudioService {
public:
    AudioService();
    ~AudioService();

    AudioService(const AudioService&) = delete;
    AudioService& operator=(const AudioService&) = delete;

    void start_recording();
    void stop_recording();
    AudioLevel input_level() const;

private:
    static void data_callback(ma_device* device, void* output, const void* input, ma_uint32 frame_count);
    void update_level(const float* samples, ma_uint32 frame_count, ma_uint32 channels);
    static std::uint64_t current_time_millis();

    mutable std::mutex mutex_;
    ma_device device_{};
    bool initialized_ = false;
    bool recording_ = false;
    std::uint32_t sample_rate_ = 0;
    std::uint32_t channels_ = 0;
    std::string device_name_ = "No active input";
    std::atomic<float> rms_{0.0F};
    std::atomic<float> peak_{0.0F};
    std::atomic<std::uint64_t> updated_at_ms_{0};
};

} // namespace celia
