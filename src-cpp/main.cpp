#include "app/CeliaApp.h"
#include "app/Json.h"
#include "audio/AudioService.h"

#include <chrono>
#include <exception>
#include <iostream>
#include <string>
#include <thread>

namespace {

std::string audio_level_json(const celia::AudioLevel& level) {
    return "{"
        "\"rms\":" + std::to_string(level.rms) + ","
        "\"peak\":" + std::to_string(level.peak) + ","
        "\"sampleRate\":" + std::to_string(level.sample_rate) + ","
        "\"channels\":" + std::to_string(level.channels) + ","
        "\"deviceName\":" + celia::json_string(level.device_name) + ","
        "\"status\":" + celia::json_string(level.status) + ","
        "\"updatedAtMs\":" + std::to_string(level.updated_at_ms) +
        "}";
}

int run_audio_smoke_test() {
    celia::AudioService audio;
    audio.start_recording();
    std::this_thread::sleep_for(std::chrono::seconds(3));
    const auto level = audio.input_level();
    audio.stop_recording();

    std::cout << audio_level_json(level) << '\n';
    return level.status == "recording" && level.sample_rate > 0 && level.channels > 0 ? 0 : 2;
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc > 1 && std::string(argv[1]) == "--audio-smoke-test") {
            return run_audio_smoke_test();
        }

        celia::CeliaApp app(argc > 0 ? argv[0] : "");
        return app.run();
    } catch (const std::exception& error) {
        std::cerr << "Voice Embedded Verification failed: " << error.what() << '\n';
        return 1;
    }
}
