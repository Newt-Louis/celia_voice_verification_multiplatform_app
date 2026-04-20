#pragma once

#include "audio/AudioService.h"
#include "webview.h"

#include <filesystem>
#include <string>

namespace celia {

class CeliaApp {
public:
    explicit CeliaApp(std::string executable_path);

    int run();

private:
    std::string resolve_frontend_url() const;
    std::filesystem::path resolve_frontend_index() const;
    std::filesystem::path executable_dir() const;
    void bind_audio_api(webview::webview& window);

    std::string executable_path_;
    AudioService audio_service_;
};

} // namespace celia
