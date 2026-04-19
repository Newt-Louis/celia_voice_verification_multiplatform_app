#pragma once

#ifdef _WIN32
#define CELIA_CORE_API __declspec(dllexport)
#else
#define CELIA_CORE_API
#endif

extern "C" {
CELIA_CORE_API const char* celia_core_version();
CELIA_CORE_API bool celia_core_initialize(const char* ecapa_model_path, const char* whisper_model_path);
CELIA_CORE_API bool celia_core_start_audio_pipeline();
CELIA_CORE_API bool celia_core_stop_audio_pipeline();
}
