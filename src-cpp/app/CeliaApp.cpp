#include "app/CeliaApp.h"

#include "app/Json.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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
        "\"transcript\":" + json_string(level.transcript) + ","
        "\"processingMode\":" + json_string(level.processing_mode) + ","
        "\"processingDetails\":" + json_string(level.processing_details) + ","
        "\"inputProfile\":" + json_string(level.input_profile) + ","
        "\"diagnosticsStatus\":" + json_string(level.diagnostics_status) + ","
        "\"rawDiagnosticsPath\":" + json_string(level.raw_diagnostics_path) + ","
        "\"processedDiagnosticsPath\":" + json_string(level.processed_diagnostics_path) +
        "}";
}

std::string read_first_line(const std::filesystem::path& path) {
    std::ifstream file(path);
    std::string line;
    if (file && std::getline(file, line)) {
        return line;
    }
    return {};
}

} // namespace

CeliaApp::CeliaApp(std::string executable_path, std::string audio_mode_override)
    : executable_path_(std::move(executable_path)),
      audio_mode_override_(std::move(audio_mode_override)) {}

int CeliaApp::run() {
    audio_service_.configure_processing(resolve_audio_processing_config());
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
        const auto level = audio_service_.input_level();
        return "{"
            "\"requestId\":" + json_string(request_id) + ","
            "\"message\":\"Micro desktop dang chay qua C++ miniaudio, mode " + level.processing_mode +
                ", sherpa-onnx dang nhan stream realtime. TODO: wakeword va ECAPA.\""
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

AudioProcessingConfig CeliaApp::resolve_audio_processing_config() const {
    const auto current = std::filesystem::current_path();
    const auto exe_dir = executable_dir();
    std::string mode_text = audio_mode_override_;
    if (mode_text.empty()) {
        const std::vector<std::filesystem::path> config_candidates = {
            current / "celia_audio_mode.txt",
            exe_dir / "celia_audio_mode.txt",
            exe_dir.parent_path() / "celia_audio_mode.txt"
        };
        for (const auto& candidate : config_candidates) {
            if (std::filesystem::exists(candidate)) {
                mode_text = read_first_line(candidate);
                break;
            }
        }
    }

    const auto relative_model = std::filesystem::path("models") / "sherpa-official-ns-vad";
    const std::vector<std::filesystem::path> model_candidates = {
        current / relative_model,
        current.parent_path() / relative_model,
        exe_dir / relative_model,
        exe_dir.parent_path() / relative_model,
        exe_dir.parent_path().parent_path() / relative_model
    };

    auto model_dir = current / relative_model;
    for (const auto& candidate : model_candidates) {
        if (std::filesystem::exists(candidate / "silero_vad.onnx") &&
            std::filesystem::exists(candidate / "gtcrn_simple.onnx")) {
            model_dir = candidate;
            break;
        }
    }

    bool diagnostics_enabled = false;
    std::filesystem::path diagnostics_dir = exe_dir / "audio-diagnostics";
    const char* diagnostics_env = std::getenv("CELIA_AUDIO_DIAGNOSTICS");
    if (diagnostics_env != nullptr) {
        const std::string value = diagnostics_env;
        diagnostics_enabled = value == "1" || value == "true" || value == "TRUE" || value == "on" || value == "ON";
    }

    const std::vector<std::filesystem::path> diagnostics_candidates = {
        current / "celia_audio_diagnostics.txt",
        exe_dir / "celia_audio_diagnostics.txt",
        exe_dir.parent_path() / "celia_audio_diagnostics.txt"
    };
    for (const auto& candidate : diagnostics_candidates) {
        if (!std::filesystem::exists(candidate)) {
            continue;
        }
        const auto diagnostics_text = read_first_line(candidate);
        auto diagnostics_token = diagnostics_text;
        diagnostics_token.erase(
            std::remove_if(diagnostics_token.begin(), diagnostics_token.end(), [](unsigned char ch) {
                return ch == '\r' || ch == '\n' || ch == '\t' || ch == ' ';
            }),
            diagnostics_token.end());
        std::transform(diagnostics_token.begin(), diagnostics_token.end(), diagnostics_token.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });

        if (diagnostics_token == "0" || diagnostics_token == "false" || diagnostics_token == "off" || diagnostics_token == "disabled") {
            diagnostics_enabled = false;
        } else {
            diagnostics_enabled = true;
        }
        if (!diagnostics_text.empty() && diagnostics_enabled &&
            diagnostics_token != "1" &&
            diagnostics_token != "true" &&
            diagnostics_token != "on" &&
            diagnostics_token != "enabled") {
            diagnostics_dir = std::filesystem::path(diagnostics_text);
        }
        break;
    }

    return AudioProcessingConfig{
        audio_processing_mode_from_id(mode_text),
        model_dir / "silero_vad.onnx",
        model_dir / "gtcrn_simple.onnx",
        diagnostics_enabled,
        diagnostics_dir,
        15
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
