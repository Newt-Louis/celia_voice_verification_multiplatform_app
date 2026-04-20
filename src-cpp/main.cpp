#include "app/CeliaApp.h"
#include "app/Json.h"
#include "audio/AudioService.h"
#include "audio/SherpaOnnxService.h"
#include "sherpa-onnx/c-api/cxx-api.h"

#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

std::string audio_level_json(const celia::AudioLevel& level) {
    return "{"
        "\"rms\":" + std::to_string(level.rms) + ","
        "\"peak\":" + std::to_string(level.peak) + ","
        "\"processedRms\":" + std::to_string(level.processed_rms) + ","
        "\"processedPeak\":" + std::to_string(level.processed_peak) + ","
        "\"noiseFloor\":" + std::to_string(level.noise_floor) + ","
        "\"vadProbability\":" + std::to_string(level.vad_probability) + ","
        "\"sampleRate\":" + std::to_string(level.sample_rate) + ","
        "\"channels\":" + std::to_string(level.channels) + ","
        "\"deviceName\":" + celia::json_string(level.device_name) + ","
        "\"status\":" + celia::json_string(level.status) + ","
        "\"vadActive\":" + std::string(level.vad_active ? "true" : "false") + ","
        "\"speechFrames\":" + std::to_string(level.speech_frames) + ","
        "\"updatedAtMs\":" + std::to_string(level.updated_at_ms) + ","
        "\"transcriptionStatus\":" + celia::json_string(level.transcription_status) + ","
        "\"transcript\":" + celia::json_string(level.transcript) +
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

celia::SherpaOnnxModelPaths default_sherpa_model_paths() {
    const auto model_dir =
        std::filesystem::current_path() / "models" / "sherpa-onnx-streaming-zipformer-ar_en_id_ja_ru_th_vi_zh-2025-02-10";
    return celia::SherpaOnnxModelPaths{
        model_dir / "encoder-epoch-75-avg-11-chunk-16-left-128.int8.onnx",
        model_dir / "decoder-epoch-75-avg-11-chunk-16-left-128.onnx",
        model_dir / "joiner-epoch-75-avg-11-chunk-16-left-128.int8.onnx",
        model_dir / "tokens.txt"
    };
}

int run_sherpa_wav_test(const std::filesystem::path& wav_path) {
    const auto wave = sherpa_onnx::cxx::ReadWave(wav_path.u8string());
    if (wave.samples.empty()) {
        std::cerr << "Cannot read wav file: " << wav_path.u8string() << '\n';
        return 2;
    }
    if (wave.sample_rate != 16000) {
        std::cerr << "Sherpa wav smoke test expects mono 16 kHz input, got " << wave.sample_rate << " Hz\n";
        return 2;
    }

    celia::SherpaOnnxService transcription;
    transcription.start(default_sherpa_model_paths());

    for (int attempt = 0; attempt < 80; ++attempt) {
        const auto snapshot = transcription.snapshot();
        if (snapshot.status == "ready") {
            break;
        }
        if (snapshot.status == "error") {
            std::cerr << snapshot.transcript << '\n';
            return 3;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    constexpr std::size_t frame_samples = 1600;
    std::string last_transcript;
    for (std::size_t offset = 0; offset < wave.samples.size(); offset += frame_samples) {
        const auto count = (std::min)(frame_samples, wave.samples.size() - offset);
        transcription.push_audio(wave.samples.data() + offset, count, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        const auto snapshot = transcription.snapshot();
        if (!snapshot.transcript.empty() && snapshot.transcript != last_transcript) {
            last_transcript = snapshot.transcript;
            std::cout << last_transcript << '\n';
        }
    }

    const std::vector<float> trailing_silence(16000 * 2, 0.0F);
    transcription.push_audio(trailing_silence.data(), trailing_silence.size(), false);
    std::this_thread::sleep_for(std::chrono::seconds(2));

    const auto final_snapshot = transcription.snapshot();
    transcription.stop();
    if (!final_snapshot.transcript.empty() && final_snapshot.transcript != last_transcript) {
        std::cout << final_snapshot.transcript << '\n';
    }

    return final_snapshot.transcript.empty() ? 4 : 0;
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc > 1 && std::string(argv[1]) == "--audio-smoke-test") {
            return run_audio_smoke_test();
        }
        if (argc > 1 && std::string(argv[1]) == "--sherpa-wav-test") {
            const auto wav_path = argc > 2
                ? std::filesystem::path(argv[2])
                : std::filesystem::current_path() / "models" /
                    "sherpa-onnx-streaming-zipformer-ar_en_id_ja_ru_th_vi_zh-2025-02-10" / "test_wavs" / "en.wav";
            return run_sherpa_wav_test(wav_path);
        }

        celia::CeliaApp app(argc > 0 ? argv[0] : "");
        return app.run();
    } catch (const std::exception& error) {
        std::cerr << "Voice Embedded Verification failed: " << error.what() << '\n';
        return 1;
    }
}
