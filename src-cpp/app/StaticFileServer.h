#pragma once

#include <atomic>
#include <filesystem>
#include <string>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#endif

namespace celia {

class StaticFileServer {
public:
    StaticFileServer();
    ~StaticFileServer();

    StaticFileServer(const StaticFileServer&) = delete;
    StaticFileServer& operator=(const StaticFileServer&) = delete;

    std::string start(const std::filesystem::path& root);
    void stop();

private:
    void accept_loop();
    void handle_client(
#ifdef _WIN32
        SOCKET client
#else
        int client
#endif
    );
    std::filesystem::path resolve_request_path(const std::string& target) const;

    std::filesystem::path root_;
    std::atomic<bool> running_{false};
    std::thread worker_;
    std::uint16_t port_ = 0;

#ifdef _WIN32
    SOCKET listen_socket_ = INVALID_SOCKET;
#else
    int listen_socket_ = -1;
#endif
};

} // namespace celia
