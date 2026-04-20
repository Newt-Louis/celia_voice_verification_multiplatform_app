#include <windows.h>
#include <shellapi.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr char kMarker[] = "CELIAZIP";
constexpr std::uint64_t kMarkerSize = 8;
constexpr std::uint64_t kSizeSize = 8;

std::wstring widen(const std::string& value) {
    if (value.empty()) {
        return {};
    }

    const int length = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), length);
    return result;
}

std::filesystem::path current_exe_path() {
    std::vector<wchar_t> buffer(MAX_PATH);
    DWORD length = 0;
    while (true) {
        length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            throw std::runtime_error("Khong doc duoc duong dan installer.");
        }
        if (length < buffer.size() - 1) {
            return std::filesystem::path(std::wstring(buffer.data(), length));
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::filesystem::path local_app_data() {
    const wchar_t* value = _wgetenv(L"LOCALAPPDATA");
    if (value == nullptr || value[0] == L'\0') {
        throw std::runtime_error("Khong tim thay LOCALAPPDATA.");
    }
    return std::filesystem::path(value);
}

std::uint64_t read_le_u64(const char* bytes) {
    std::uint64_t value = 0;
    for (int index = 7; index >= 0; --index) {
        value = (value << 8) | static_cast<unsigned char>(bytes[index]);
    }
    return value;
}

void extract_embedded_zip(const std::filesystem::path& installer, const std::filesystem::path& zip_path) {
    std::ifstream input(installer, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Khong mo duoc installer de doc payload.");
    }

    input.seekg(0, std::ios::end);
    const auto file_size = static_cast<std::uint64_t>(input.tellg());
    if (file_size < kMarkerSize + kSizeSize) {
        throw std::runtime_error("Installer khong co payload zip.");
    }

    input.seekg(static_cast<std::streamoff>(file_size - kMarkerSize - kSizeSize));
    char size_bytes[kSizeSize] = {};
    char marker[kMarkerSize] = {};
    input.read(size_bytes, sizeof(size_bytes));
    input.read(marker, sizeof(marker));

    if (std::string(marker, marker + kMarkerSize) != kMarker) {
        throw std::runtime_error("Installer payload marker khong hop le.");
    }

    const std::uint64_t zip_size = read_le_u64(size_bytes);
    const std::uint64_t payload_offset = file_size - kMarkerSize - kSizeSize - zip_size;
    input.seekg(static_cast<std::streamoff>(payload_offset));

    std::ofstream output(zip_path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Khong ghi duoc payload zip tam.");
    }

    std::vector<char> buffer(1024 * 1024);
    std::uint64_t remaining = zip_size;
    while (remaining > 0) {
        const auto chunk = static_cast<std::streamsize>(remaining < buffer.size() ? remaining : buffer.size());
        input.read(buffer.data(), chunk);
        output.write(buffer.data(), input.gcount());
        remaining -= static_cast<std::uint64_t>(input.gcount());
    }
}

void run_powershell(const std::wstring& command) {
    std::wstring full_command = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"" + command + L"\"";

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};

    if (!CreateProcessW(nullptr, full_command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process)) {
        throw std::runtime_error("Khong chay duoc PowerShell.");
    }

    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exit_code = 0;
    GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);

    if (exit_code != 0) {
        throw std::runtime_error("PowerShell install command that bai.");
    }
}

void launch_app(const std::filesystem::path& exe_path) {
    ShellExecuteW(nullptr, L"open", exe_path.wstring().c_str(), nullptr, exe_path.parent_path().wstring().c_str(), SW_SHOWNORMAL);
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    try {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        bool silent = false;
        for (int index = 1; index < argc; ++index) {
            const std::wstring arg = argv[index];
            if (arg == L"/S" || arg == L"/silent" || arg == L"--silent") {
                silent = true;
            }
        }
        if (argv != nullptr) {
            LocalFree(argv);
        }

        const auto installer = current_exe_path();
        const auto install_dir = local_app_data() / "Voice Embedded Verification";
        const auto temp_dir = std::filesystem::temp_directory_path() / "voice_embedded_verification_installer";
        const auto zip_path = temp_dir / "app.zip";
        const auto app_exe = install_dir / "app" / "Voice Embedded Verification.exe";

        std::filesystem::remove_all(temp_dir);
        std::filesystem::create_directories(temp_dir);
        std::filesystem::remove_all(install_dir);
        std::filesystem::create_directories(install_dir);

        extract_embedded_zip(installer, zip_path);

        const std::wstring expand_command =
            L"Expand-Archive -LiteralPath '" + zip_path.wstring() +
            L"' -DestinationPath '" + install_dir.wstring() + L"' -Force";
        run_powershell(expand_command);

        const std::wstring shortcut_command =
            L"$desktop=[Environment]::GetFolderPath('Desktop');"
            L"$shortcut=Join-Path $desktop 'Voice Embedded Verification.lnk';"
            L"$target='" + app_exe.wstring() + L"';"
            L"$shell=New-Object -ComObject WScript.Shell;"
            L"$link=$shell.CreateShortcut($shortcut);"
            L"$link.TargetPath=$target;"
            L"$link.WorkingDirectory=(Split-Path -LiteralPath $target -Parent);"
            L"$link.Save()";
        run_powershell(shortcut_command);

        if (!silent) {
            launch_app(app_exe);
            MessageBoxW(nullptr, L"Voice Embedded Verification da duoc cai dat.", L"Voice Embedded Verification", MB_OK | MB_ICONINFORMATION);
        }
        return 0;
    } catch (const std::exception& error) {
        const auto message = widen(error.what());
        MessageBoxW(nullptr, message.c_str(), L"Voice Embedded Verification Installer", MB_OK | MB_ICONERROR);
        return 1;
    }
}
