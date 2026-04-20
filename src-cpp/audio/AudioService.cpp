#define MINIAUDIO_IMPLEMENTATION
#include "audio/AudioService.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>

namespace celia {

AudioService::AudioService() = default;

AudioService::~AudioService() {
    stop_recording();
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
    rms_.store(0.0F);
    peak_.store(0.0F);
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
    rms_.store(0.0F);
    peak_.store(0.0F);
    updated_at_ms_.store(0);
}

AudioLevel AudioService::input_level() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!recording_) {
        return {};
    }

    return AudioLevel{
        rms_.load(),
        peak_.load(),
        sample_rate_,
        channels_,
        device_name_,
        "recording",
        updated_at_ms_.load()
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
    float sum = 0.0F;
    float peak = 0.0F;

    for (ma_uint32 frame = 0; frame < frame_count; ++frame) {
        const float sample = std::clamp(samples[frame * channel_count], -1.0F, 1.0F);
        const float abs_sample = std::fabs(sample);
        peak = (std::max)(peak, abs_sample);
        sum += sample * sample;
    }

    rms_.store(std::sqrt(sum / static_cast<float>(frame_count)));
    peak_.store(peak);
    updated_at_ms_.store(current_time_millis());
}

std::uint64_t AudioService::current_time_millis() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

} // namespace celia
