#include "app/CeliaApp.h"

#include "app/Json.h"

#include <chrono>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace celia {
namespace {

std::string make_request_id() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    return "audio-" + std::to_string(millis);
}

std::string audio_level_json(const AudioLevel& level) {
    return "{"
        "\"rms\":" + std::to_string(level.rms) + ","
        "\"peak\":" + std::to_string(level.peak) + ","
        "\"processedRms\":" + std::to_string(level.processed_rms) + ","
        "\"processedPeak\":" + std::to_string(level.processed_peak) + ","
        "\"noiseFloor\":" + std::to_string(level.noise_floor) + ","
        "\"vadProbability\":" + std::to_string(level.vad_probability) + ","
        "\"sampleRate\":" + std::to_string(level.sample_rate) + ","
        "\"channels\":" + std::to_string(level.channels) + ","
        "\"deviceName\":" + json_string(level.device_name) + ","
        "\"status\":" + json_string(level.status) + ","
        "\"vadActive\":" + std::string(level.vad_active ? "true" : "false") + ","
        "\"speechFrames\":" + std::to_string(level.speech_frames) + ","
        "\"updatedAtMs\":" + std::to_string(level.updated_at_ms) + ","
        "\"transcriptionStatus\":" + json_string(level.transcription_status) + ","
        "\"transcript\":" + json_string(level.transcript) +
        "}";
}

} // namespace

CeliaApp::CeliaApp(std::string executable_path)
    : executable_path_(std::move(executable_path)) {}

int CeliaApp::run() {
    audio_service_.load_sherpa_onnx_model(resolve_sherpa_onnx_model());

    webview::webview window(true, nullptr);
    bind_audio_api(window);

    window.set_title("Voice Embedded Verification");
    window.set_size(1100, 760, WEBVIEW_HINT_NONE);
    window.navigate(resolve_frontend_url());
    window.run();

    audio_service_.stop_recording();
    return 0;
}

void CeliaApp::bind_audio_api(webview::webview& window) {
    window.bind("celiaAudioStartRecording", [this](const std::string&) {
        audio_service_.start_recording();
        const auto request_id = make_request_id();
        return "{"
            "\"requestId\":" + json_string(request_id) + ","
            "\"message\":\"Micro desktop dang chay qua C++ miniaudio, DSP NS/VAD da bat, sherpa-onnx dang nhan stream realtime. TODO: wakeword va ECAPA.\""
            "}";
    });

    window.bind("celiaAudioStopRecording", [this](const std::string& request) {
        audio_service_.stop_recording();
        const auto request_id = first_json_string_argument(request);
        return "{"
            "\"requestId\":" + (request_id.empty() ? "null" : json_string(request_id)) + ","
            "\"message\":\"Da dung stream micro desktop qua C++.\""
            "}";
    });

    window.bind("celiaAudioGetInputLevel", [this](const std::string&) {
        return audio_level_json(audio_service_.input_level());
    });
}

std::string CeliaApp::resolve_frontend_url() {
    const auto index = resolve_frontend_index();
    if (!index.empty() && std::filesystem::exists(index)) {
        return frontend_server_.start(index.parent_path());
    }

    return "http://127.0.0.1:1420";
}

std::filesystem::path CeliaApp::resolve_frontend_index() const {
    const auto current = std::filesystem::current_path();
    const auto exe_dir = executable_dir();
    const std::vector<std::filesystem::path> candidates = {
        current / "frontend" / "dist" / "index.html",
        current.parent_path() / "frontend" / "dist" / "index.html",
        exe_dir / "frontend" / "dist" / "index.html",
        exe_dir.parent_path() / "frontend" / "dist" / "index.html",
        exe_dir.parent_path().parent_path() / "frontend" / "dist" / "index.html"
    };

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    return {};
}

SherpaOnnxModelPaths CeliaApp::resolve_sherpa_onnx_model() const {
    const auto current = std::filesystem::current_path();
    const auto exe_dir = executable_dir();
    const auto relative_model =
        std::filesystem::path("models") / "sherpa-onnx-streaming-zipformer-ar_en_id_ja_ru_th_vi_zh-2025-02-10";
    const std::vector<std::filesystem::path> candidates = {
        current / relative_model,
        current.parent_path() / relative_model,
        exe_dir / relative_model,
        exe_dir.parent_path() / relative_model,
        exe_dir.parent_path().parent_path() / relative_model
    };

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate / "encoder-epoch-75-avg-11-chunk-16-left-128.int8.onnx")) {
            return SherpaOnnxModelPaths{
                candidate / "encoder-epoch-75-avg-11-chunk-16-left-128.int8.onnx",
                candidate / "decoder-epoch-75-avg-11-chunk-16-left-128.onnx",
                candidate / "joiner-epoch-75-avg-11-chunk-16-left-128.int8.onnx",
                candidate / "tokens.txt"
            };
        }
    }

    const auto fallback = current / relative_model;
    return SherpaOnnxModelPaths{
        fallback / "encoder-epoch-75-avg-11-chunk-16-left-128.int8.onnx",
        fallback / "decoder-epoch-75-avg-11-chunk-16-left-128.onnx",
        fallback / "joiner-epoch-75-avg-11-chunk-16-left-128.int8.onnx",
        fallback / "tokens.txt"
    };
}

std::filesystem::path CeliaApp::executable_dir() const {
    if (executable_path_.empty()) {
        return std::filesystem::current_path();
    }

    std::error_code error;
    auto path = std::filesystem::absolute(executable_path_, error);
    if (error) {
        return std::filesystem::current_path();
    }

    if (std::filesystem::is_regular_file(path, error)) {
        return path.parent_path();
    }

    return std::filesystem::current_path();
}

} // namespace celia
