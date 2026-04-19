#include "celia_audio_core.h"

#include <fstream>
#include <mutex>
#include <string>

namespace {
std::mutex g_pipeline_mutex;
bool g_initialized = false;
bool g_recording = false;
std::string g_ecapa_model_path;
std::string g_whisper_model_path;

bool file_exists(const char* path) {
    if (path == nullptr || path[0] == '\0') {
        return false;
    }

    std::ifstream file(path, std::ios::binary);
    return file.good();
}
} // namespace

extern "C" {

const char* celia_core_version() {
    return "celia-audio-core/0.1.0";
}

bool celia_core_initialize(const char* ecapa_model_path, const char* whisper_model_path) {
    std::lock_guard<std::mutex> lock(g_pipeline_mutex);

    if (!file_exists(ecapa_model_path) || !file_exists(whisper_model_path)) {
        g_initialized = false;
        return false;
    }

    g_ecapa_model_path = ecapa_model_path;
    g_whisper_model_path = whisper_model_path;
    g_initialized = true;
    return true;
}

bool celia_core_start_audio_pipeline() {
    std::lock_guard<std::mutex> lock(g_pipeline_mutex);

    if (!g_initialized) {
        return false;
    }

    g_recording = true;
    return g_recording;
}

bool celia_core_stop_audio_pipeline() {
    std::lock_guard<std::mutex> lock(g_pipeline_mutex);
    g_recording = false;
    return true;
}

}
