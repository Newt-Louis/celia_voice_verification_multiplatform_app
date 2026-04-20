#pragma once

#include "audio/AudioService.h"
#include "app/StaticFileServer.h"
#include "webview.h"

#include <filesystem>
#include <string>

namespace celia {

class CeliaApp {
public:
    explicit CeliaApp(std::string executable_path);

    int run();

private:
    std::string resolve_frontend_url();
    std::filesystem::path resolve_frontend_index() const;
    SherpaOnnxModelPaths resolve_sherpa_onnx_model() const;
    std::filesystem::path executable_dir() const;
    void bind_audio_api(webview::webview& window);

    std::string executable_path_;
    AudioService audio_service_;
    StaticFileServer frontend_server_;
};

} // namespace celia
