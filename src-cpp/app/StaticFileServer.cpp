#include "app/StaticFileServer.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace celia {
namespace {

std::string decode_url_path(const std::string& value) {
    std::string output;
    output.reserve(value.size());

    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '%' && index + 2 < value.size()) {
            const auto hex = value.substr(index + 1, 2);
            char* end = nullptr;
            const auto decoded = std::strtol(hex.c_str(), &end, 16);
            if (end != nullptr && *end == '\0') {
                output.push_back(static_cast<char>(decoded));
                index += 2;
                continue;
            }
        }

        output.push_back(value[index] == '/' ? std::filesystem::path::preferred_separator : value[index]);
    }

    return output;
}

std::string mime_type(const std::filesystem::path& path) {
    static const std::unordered_map<std::string, std::string> types = {
        {".html", "text/html; charset=utf-8"},
        {".js", "text/javascript; charset=utf-8"},
        {".css", "text/css; charset=utf-8"},
        {".json", "application/json; charset=utf-8"},
        {".svg", "image/svg+xml"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".ico", "image/x-icon"},
        {".woff", "font/woff"},
        {".woff2", "font/woff2"},
        {".ttf", "font/ttf"},
        {".eot", "application/vnd.ms-fontobject"}
    };

    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    const auto found = types.find(ext);
    if (found != types.end()) {
        return found->second;
    }

    return "application/octet-stream";
}

void close_socket(
#ifdef _WIN32
    SOCKET socket
#else
    int socket
#endif
) {
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

void send_all(
#ifdef _WIN32
    SOCKET socket,
#else
    int socket,
#endif
    const char* data,
    std::size_t size
) {
    while (size > 0) {
        const int sent = send(socket, data, static_cast<int>(size), 0);
        if (sent <= 0) {
            return;
        }
        data += sent;
        size -= static_cast<std::size_t>(sent);
    }
}

} // namespace

StaticFileServer::StaticFileServer() = default;

StaticFileServer::~StaticFileServer() {
    stop();
}

std::string StaticFileServer::start(const std::filesystem::path& root) {
    if (running_) {
        return "http://127.0.0.1:" + std::to_string(port_) + "/";
    }

    root_ = std::filesystem::canonical(root);

#ifdef _WIN32
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        throw std::runtime_error("Khong khoi tao duoc Winsock.");
    }
#endif

    listen_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#ifdef _WIN32
    if (listen_socket_ == INVALID_SOCKET) {
#else
    if (listen_socket_ < 0) {
#endif
        throw std::runtime_error("Khong tao duoc socket static file server.");
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;

    if (bind(listen_socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        close_socket(listen_socket_);
        throw std::runtime_error("Khong bind duoc static file server.");
    }

    if (listen(listen_socket_, SOMAXCONN) != 0) {
        close_socket(listen_socket_);
        throw std::runtime_error("Khong listen duoc static file server.");
    }

    sockaddr_in bound{};
    int bound_size = sizeof(bound);
    if (getsockname(listen_socket_, reinterpret_cast<sockaddr*>(&bound), &bound_size) != 0) {
        close_socket(listen_socket_);
        throw std::runtime_error("Khong lay duoc port static file server.");
    }

    port_ = ntohs(bound.sin_port);
    running_ = true;
    worker_ = std::thread(&StaticFileServer::accept_loop, this);

    return "http://127.0.0.1:" + std::to_string(port_) + "/";
}

void StaticFileServer::stop() {
    if (!running_) {
        return;
    }

    running_ = false;
#ifdef _WIN32
    if (listen_socket_ != INVALID_SOCKET) {
        shutdown(listen_socket_, SD_BOTH);
        closesocket(listen_socket_);
        listen_socket_ = INVALID_SOCKET;
    }
#else
    if (listen_socket_ >= 0) {
        shutdown(listen_socket_, SHUT_RDWR);
        close(listen_socket_);
        listen_socket_ = -1;
    }
#endif

    if (worker_.joinable()) {
        worker_.join();
    }

#ifdef _WIN32
    WSACleanup();
#endif
}

void StaticFileServer::accept_loop() {
    while (running_) {
        auto client = accept(listen_socket_, nullptr, nullptr);
#ifdef _WIN32
        if (client == INVALID_SOCKET) {
#else
        if (client < 0) {
#endif
            continue;
        }

        handle_client(client);
        close_socket(client);
    }
}

void StaticFileServer::handle_client(
#ifdef _WIN32
    SOCKET client
#else
    int client
#endif
) {
    std::array<char, 4096> buffer{};
    const int received = recv(client, buffer.data(), static_cast<int>(buffer.size() - 1), 0);
    if (received <= 0) {
        return;
    }

    std::istringstream request(std::string(buffer.data(), static_cast<std::size_t>(received)));
    std::string method;
    std::string target;
    request >> method >> target;

    if (method != "GET" && method != "HEAD") {
        const std::string response = "HTTP/1.1 405 Method Not Allowed\r\nConnection: close\r\n\r\n";
        send_all(client, response.data(), response.size());
        return;
    }

    const auto path = resolve_request_path(target);
    if (path.empty() || !std::filesystem::is_regular_file(path)) {
        const std::string response = "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n";
        send_all(client, response.data(), response.size());
        return;
    }

    std::ifstream file(path, std::ios::binary);
    std::vector<char> body((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    std::ostringstream header;
    header << "HTTP/1.1 200 OK\r\n"
           << "Content-Type: " << mime_type(path) << "\r\n"
           << "Content-Length: " << body.size() << "\r\n"
           << "Cache-Control: no-store\r\n"
           << "Connection: close\r\n\r\n";
    const auto header_text = header.str();
    send_all(client, header_text.data(), header_text.size());
    if (method == "GET" && !body.empty()) {
        send_all(client, body.data(), body.size());
    }
}

std::filesystem::path StaticFileServer::resolve_request_path(const std::string& target) const {
    const auto query_index = target.find('?');
    std::string clean_target = target.substr(0, query_index);
    if (clean_target.empty() || clean_target == "/") {
        clean_target = "/index.html";
    }

    auto relative = std::filesystem::path(decode_url_path(clean_target.substr(1))).lexically_normal();
    for (const auto& part : relative) {
        if (part == "..") {
            return {};
        }
    }

    const auto candidate = std::filesystem::weakly_canonical(root_ / relative);
    const auto root_text = root_.wstring();
    const auto candidate_text = candidate.wstring();
    if (candidate_text.rfind(root_text, 0) != 0) {
        return {};
    }

    return candidate;
}

} // namespace celia
