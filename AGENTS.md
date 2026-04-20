# Celia Voice Verification - Bộ nhớ dự án

## Mục tiêu hiện tại

Repository này đang chuyển hướng từ app Tauri + Rust + C++ sang app C++ thuần, dùng:

- `webview/webview` làm app shell/native webview để nhúng UI Vue/Vite.
- `miniaudio` làm lớp audio input cross-platform.
- Vue 3 + Vite + PrimeVue 4 tiếp tục làm frontend.

Mục tiêu hoạt động đầu tiên vẫn là desktop Windows 11. Kiến trúc C++ mới phải giữ đường mở cho desktop, mobile và automotive, nhưng không ép mỗi target phải link dependency riêng của target khác.

Chưa bắt đầu đổi code Rust/Tauri sang C++ trong lần cập nhật bộ nhớ này. Lần này đã quét toolchain, CMake, `src-tauri/`, tải/generate `webview.h` và `miniaudio.h`, rồi ghi lại trạng thái để chuẩn bị cho nhiệm vụ tiếp theo.

## Hướng thay thế Tauri/Rust

Hướng được user xác nhận để triển khai sau:

- Loại bỏ vai trò app shell của Tauri/Rust.
- Dùng C++ executable làm entrypoint chính.
- Dùng `webview/webview` để mở cửa sổ native và load frontend Vue đã build.
- Dùng `miniaudio.h` để mở micro và xử lý audio input cross-platform.
- Giữ logic OS/thiết bị ở native C++, không đưa vào UI component Vue.
- Frontend vẫn chỉ render state và gọi facade/bridge.
- `src-cpp/` là gốc C++ chính.
- `src-cpp/includes/` là nơi chứa vendored headers như `webview.h` và `miniaudio.h`.

Cách tổ chức code C++ đề xuất:

- `src-cpp/main.cpp`: entrypoint.
- `src-cpp/app/`: app lifecycle, webview window, bridge JS/C++.
- `src-cpp/audio/`: audio service dùng miniaudio.
- `src-cpp/models/`: resolver model path và tích hợp model sau này.
- `src-cpp/platform/`: chỉ thêm khi thật sự cần code riêng theo OS.

Không cần chia sẵn `windows/`, `android/`, `macos/`, `ios/`, `linux/`, `qnx/` nếu miniaudio/webview API chung đủ dùng. Nếu một target cần permission, resource path, packaging hoặc backend riêng, lúc đó mới tách platform folder để tránh làm kiến trúc rối sớm.

`miniaudio` là thư viện một header `miniaudio.h`, có sẵn adapter nền tảng qua `#ifdef` cho Windows, macOS, Linux, iOS, Android, QNX. API audio native tương lai nên đi qua miniaudio thay vì CPAL/Rust.

`webview/webview` được dùng làm shell thay Tauri. Khi triển khai cần quyết định cách bridge JS <-> C++ và cách load asset Vue:

- Dev: có thể load Vite dev server.
- Release: nên load static `frontend/dist` hoặc resource được bundle cùng executable.

## Các model

Các model cần được giữ nguyên đường dẫn khi có mặt trong workspace và phải được đóng gói cùng ứng dụng:

- ECAPA-TDNN ONNX INT8 dynamic quantized:
  `models/v2_int8_fast/ecapa_int8_dynamic.onnx`
- Whisper small multilingual quantized GGML:
  `models/q5_0/ggml-model-q5_0.bin`

Kết quả quét ngày 2026-04-20: trong workspace hiện tại không thấy thư mục `models/`. Tuy vậy `src-tauri/tauri.conf.json` cũ vẫn đang tham chiếu hai resource model trên. Khi chuyển sang C++ thuần, cần tạo cơ chế copy/bundle resource tương đương và giữ nguyên đường dẫn model trừ khi user yêu cầu migrate rõ ràng.

## Bố cục repository hiện tại

- `frontend/`
  UI Vue 3 + Vite + PrimeVue 4. Đây vẫn là nơi duy nhất được có `node_modules`.
- `src-tauri/`
  Code Tauri/Rust cũ. Đã quét để chuẩn bị thay thế, nhưng chưa xóa.
- `src-tauri/native/cpp/`
  C++ stub cũ được Rust build qua `cc`.
- `src-cpp/`
  Gốc của hệ thống C++ mới. Code C++ chính sẽ nằm dưới cây này.
- `src-cpp/includes/`
  Nơi vendored headers/third-party headers cho C++ mới. Hiện đã có `webview.h` và `miniaudio.h`.
- `src-cpp/main.cpp`
  Entry point C++ tối thiểu hiện tại, in ra dòng khởi động.
- `src-cpp/main.h`
  Header stub hiện tại do user tạo.
- `CMakeLists.txt`
  CMake root cho hướng C++ mới, hiện build target `Voice_Embedded_Verification`.
- `cmake-build-debug-windows/`
  Build directory CMake hiện có, generator Ninja, compiler MinGW từ CLion.
- `cmake-build-release-android/`
  Build directory CMake hiện có, tên profile Android nhưng cache hiện vẫn dùng MinGW từ CLion, chưa thấy Android NDK/toolchain trong cache.
- `cmake-build-msvc-debug/`
  Build directory MSVC/Ninja đã build thành công từ terminal khi chạy ngoài sandbox.
- `AGENTS.md`
  File bộ nhớ dự án cho các phiên Codex sau.

## Frontend hiện tại

Frontend dùng:

- Vue `^3.5.32`
- Vite `^6.4.2`
- PrimeVue `^4.5.5`
- `@primeuix/themes` `^2.0.3`
- Pinia `^3.0.4`
- Vue Router `^4.5.1`
- `@tauri-apps/api` `^2.10.1` vẫn còn trong dependencies vì frontend hiện còn strategy Tauri.

Các script frontend:

```powershell
npm --prefix frontend run dev
npm --prefix frontend run build
npm --prefix frontend run preview
```

Kết quả quét ngày 2026-04-20:

- `frontend/package-lock.json` có tồn tại.
- `frontend/node_modules` hiện không tồn tại trong workspace quét được.
- Nếu cần build frontend, chạy:

```powershell
npm --prefix frontend install
npm --prefix frontend run build
```

Khi chuyển sang C++ thuần, cần thay `TauriDesktopStrategy.ts` bằng strategy/bridge mới cho `webview/webview`. Không nhét logic micro hoặc OS vào Vue component.

## CMake root hiện tại

File `CMakeLists.txt` root hiện có nội dung chính sau lần user sửa:

- `cmake_minimum_required(VERSION 3.20)`
- Project `"Voice Embedded Verification" VERSION 1.0.0 LANGUAGES CXX`
- `TARGET_NAME` là `Voice_Embedded_Verification`
- C++17 bắt buộc.
- Include path đang trỏ tới:

```cmake
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/includes)
```

- Source glob đang tìm:

```cmake
file(GLOB_RECURSE SOURCES
        "src-cpp/*.cpp"
        "src-cpp/*.h"
)
```

- Target executable:

```cmake
add_executable(${TARGET_NAME} ${SOURCES}
        src-cpp/main.cpp
        src-cpp/main.h)
```

- Output executable name:

```cmake
OUTPUT_NAME "Voice Embedded Verification"
RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
```

- Windows link libs hiện khai báo:

```cmake
ole32
version
shlwapi
windowscodecs
```

- Theo tài liệu webview, Windows thường cần thêm `advapi32`, `shell32`, `user32` và WebView2-related setup tùy cách tích hợp. Chưa sửa CMake cho phần này trong lần cập nhật bộ nhớ.
- Android link libs hiện khai báo:

```cmake
log
android
OpenSLES
```

- Apple link libs hiện khai báo:

```cmake
Cocoa
WebKit
CoreAudio
```

- ONNX Runtime mới chỉ có comment ví dụ, chưa bật.

Lưu ý quan trọng: user đã chốt `src-cpp/` là gốc C++ mới và `src-cpp/includes/` là nơi chứa `webview.h`/`miniaudio.h`. `CMakeLists.txt` hiện đã glob source từ `src-cpp/*.cpp` và `src-cpp/*.h`, nhưng include path đang là `${CMAKE_CURRENT_SOURCE_DIR}/includes` trong khi headers thực tế ở `src-cpp/includes/`.

Khi bắt đầu đổi code thật, nên sửa include path theo hướng:

```cmake
target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src-cpp/includes
)
```

Chưa sửa `CMakeLists.txt` trong lần này vì user yêu cầu cập nhật bộ nhớ và tải header trước.

## Third-party headers trong `src-cpp/includes`

Đã tải/generate ngày 2026-04-20:

- `src-cpp/includes/miniaudio.h`
  - Nguồn: `https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h`
  - Version trong file: `miniaudio - v0.11.25 - 2026-03-04`
  - Kích thước quét được: `4108168` bytes.
- `src-cpp/includes/webview.h`
  - Nguồn: repo chính thức `https://github.com/webview/webview`.
  - Được tạo bằng script chính thức `scripts/amalgamate/amalgamate.py` từ branch `master`, input `core/src`, base `core`, search `include`.
  - Version macro trong file: `WEBVIEW_VERSION_MAJOR 0`, `WEBVIEW_VERSION_MINOR 12`, `WEBVIEW_VERSION_PATCH 0`.
  - Kích thước quét được: `210657` bytes.

Lưu ý webview: repo webview hiện không phát hành sẵn asset `webview.h` ở URL release `0.12.0` mà đã kiểm tra; bản `webview.h` trong `src-cpp/includes` là amalgamated header tự tạo từ source chính thức. Trong lúc tạo header, script báo `Skipped: WebView2.h`; khi tích hợp thật trên Windows cần kiểm tra compile với MSVC và bổ sung WebView2 SDK/header nếu compiler báo thiếu.

Lưu ý miniaudio: header hiện tại có docs về capture device, `ma_device_start()`, WASAPI, Android AAudio/OpenSL ES, Linux backends. Mặc định nên dùng API chung của miniaudio trước; chỉ tách thư mục platform (`windows/`, `android/`, `macos/`, `ios/`, `linux/`, `qnx/`) nếu phát sinh code khác nhau thật sự.

## CMake build directories đã quét

`cmake-build-debug-windows/CMakeCache.txt`:

- `CMAKE_BUILD_TYPE=Debug`
- Generator: `Ninja`
- C++ compiler:
  `D:/Application/JetBrains/CLion 2026.1/bin/mingw/bin/g++.exe`
- Make program:
  `D:/Application/JetBrains/CLion 2026.1/bin/ninja/win/x64/ninja.exe`
- Project: `Celia_Core`

`cmake-build-release-android/CMakeCache.txt`:

- Generator: `Ninja`
- C++ compiler vẫn là:
  `D:/Application/JetBrains/CLion 2026.1/bin/mingw/bin/g++.exe`
- Make program:
  `D:/Application/JetBrains/CLion 2026.1/bin/ninja/win/x64/ninja.exe`
- Chưa thấy Android NDK/toolchain trong cache, dù profile tên là `Release-Android`.

`.idea/workspace.xml` có CMake profiles:

- `Debug-Windows`
- `Release-Android`

`cmake-build-msvc-debug/`:

- Được tạo bằng terminal ngày 2026-04-20.
- Generator: `Ninja`.
- C++ compiler: MSVC `cl.exe` version `19.50.35729.0`.
- Build pass và tạo:

```text
cmake-build-msvc-debug/bin/Voice Embedded Verification.exe
```

- Chạy executable pass và in:

```text
He thong Voice Embedded Verification da khoi dong!
```

## Toolchain quét ngày 2026-04-20

Các tool có trong PATH:

- Node.js:
  `D:\NodeJS\node.exe`
  version `v20.16.0`
- npm:
  `D:\NodeJS\npm.cmd`
  version `10.8.1`
- Git:
  `D:\Git\setup-git\cmd\git.exe`
  version `2.45.2.windows.1`
- Python:
  `D:\Python-3.13.3\python.exe`
  version `3.13.3`

Các tool không có trong PATH thường:

- `cl`
- `cmake`
- `ninja`
- `msbuild`
- `devenv`
- `vswhere`
- `cargo`
- `rustc`
- `rustup`
- `make`
- `gcc`
- `g++`

Rust/Cargo:

- Đường dẫn cũ `D:\.rust\.cargo\bin\cargo.exe` không còn thấy trong lần quét này.
- Đường dẫn cũ `D:\.rust\.cargo\bin\rustc.exe` không còn thấy trong lần quét này.
- `C:\Users\ASUS\.cargo\bin` cũng không thấy trong lần quét này.

Git ownership:

- `git status` thường bị lỗi `dubious ownership` vì repo thuộc user `ASUS`, còn sandbox hiện chạy dưới user `CodexSandboxOffline`.
- Dùng dạng one-shot này để đọc status mà không sửa global config:

```powershell
git -c safe.directory=D:/MyProject/celia_voice_verification_multiplatform_app status --short
```

Kết quả status đáng chú ý sau lần cập nhật này:

- `.gitignore` modified.
- `AGENTS.md` modified.
- `CMakeLists.txt` modified từ user.
- `src-cpp/main.cpp` và `src-cpp/main.h` là file user đã thêm/sửa.
- `src-cpp/includes/` có `webview.h` và `miniaudio.h` mới tải/generate.

Không được revert các thay đổi của user nếu không được yêu cầu.

## CLion toolchain

CLion được cài tại:

```text
D:\Application\JetBrains\CLion 2026.1
```

Các tool CLion chạy được bằng absolute path:

- CMake:

```text
D:\Application\JetBrains\CLion 2026.1\bin\cmake\win\x64\bin\cmake.exe
```

Version:

```text
cmake version 4.2.2
```

- Ninja:

```text
D:\Application\JetBrains\CLion 2026.1\bin\ninja\win\x64\ninja.exe
```

Version:

```text
1.13.2
```

- MinGW GCC:

```text
D:\Application\JetBrains\CLion 2026.1\bin\mingw\bin\gcc.exe
```

Version:

```text
gcc.exe (GCC) 13.1.0
```

- MinGW G++:

```text
D:\Application\JetBrains\CLion 2026.1\bin\mingw\bin\g++.exe
```

Version:

```text
g++.exe (GCC) 13.1.0
```

CLion có thư mục clang tools nhưng không thấy `clang++.exe`; chỉ thấy các tool như `clangd.exe`, `clang-tidy.exe`, `llvm-symbolizer.exe`.

## MSVC Build Tools

Visual Studio Build Tools được phát hiện tại:

```text
C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools
```

`vswhere.exe` có tại:

```text
C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe
```

Lệnh tìm VS Build Tools:

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe' -all -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
```

Kết quả:

```text
C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools
```

MSVC compiler:

```text
C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\cl.exe
```

Khi gọi qua `vcvars64.bat`, `cl` báo:

```text
Microsoft (R) C/C++ Optimizing Compiler Version 19.50.35729 for x64
```

Các tool MSVC sau chỉ sẵn sàng sau khi call `vcvars64.bat`:

```powershell
cmd.exe /c "call ""C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"" >nul && where cl && where link && where nmake"
```

Kết quả:

```text
C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\cl.exe
C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe
C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\nmake.exe
```

MSBuild:

```text
C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe
C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe
```

Version:

```text
18.5.4.18101
```

Gợi ý configure CMake bằng MSVC/Ninja sau này:

```powershell
cmd.exe /c "call ""C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"" >nul && ""D:\Application\JetBrains\CLion 2026.1\bin\cmake\win\x64\bin\cmake.exe"" -S . -B cmake-build-msvc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug"
```

## `src-tauri/` đã quét

Các file trong `src-tauri/`:

- `src-tauri/Cargo.toml`
- `src-tauri/build.rs`
- `src-tauri/tauri.conf.json`
- `src-tauri/icons/icon.ico`
- `src-tauri/native/cpp/CMakeLists.txt`
- `src-tauri/native/cpp/celia_audio_core.h`
- `src-tauri/native/cpp/celia_audio_core.cpp`
- `src-tauri/src/main.rs`
- `src-tauri/src/lib.rs`
- `src-tauri/src/model_paths.rs`
- `src-tauri/src/native_audio.rs`

`src-tauri/icons/icon.ico` là binary icon, không đọc text.

## Tauri/Rust hiện tại

`src-tauri/Cargo.toml`:

- Package `celia_voice_verification` version `0.1.0`.
- Rust edition `2021`, `rust-version = "1.77"`.
- Build dependencies: `cc`, `tauri-build`.
- Dependencies: `tauri`, `serde`, optional `cpal`.
- Features hiện có:

```toml
default = ["desktop"]
desktop = ["dep:cpal"]
mobile = []
automotive = []
onnxruntime = []
whisper-cpp = []
```

`src-tauri/build.rs`:

- Build C++ stub `native/cpp/celia_audio_core.cpp` bằng crate `cc`.
- Include `native/cpp`.
- Thử bật C++17 bằng cả `-std=c++17` và `/std:c++17`.
- Sau đó gọi `tauri_build::build()`.

`src-tauri/tauri.conf.json`:

- Product name: `Celia Voice Verification`.
- Frontend dev URL: `http://127.0.0.1:1420`.
- Frontend dist: `../frontend/dist`.
- Window: 1100x760, min 360x640.
- Bundle icon: `icons/icon.ico`.
- Bundle resources đang trỏ tới hai model trong `../models/...`.

`src-tauri/src/main.rs`:

- Chỉ gọi `celia_voice_verification_lib::run()`.

`src-tauri/src/lib.rs`:

- Khai báo Tauri commands:
  - `audio_start_recording`
  - `audio_stop_recording`
  - `audio_get_input_level`
- Quản lý `AudioPipelineState` bằng `Mutex<Option<String>>`.
- `audio_start_recording` gọi `native_audio::start_input_monitor()`, tạo `request_id`, trả message tiếng Việt.
- `audio_stop_recording` gọi `native_audio::stop_input_monitor()`.
- `audio_get_input_level` trả `rms`, `peak`, `sample_rate`, `channels`, `device_name`, `status`, `updated_at_ms`.
- `run()` tạo Tauri builder, đăng ký command, resolve model paths trong setup, rồi chạy Tauri context.

`src-tauri/src/model_paths.rs`:

- Resolve model paths bằng `app.path().resource_dir()`, `current_dir`, parent của current dir.
- Luôn thử cả root và `root/_up_`.
- Kiểm tra tồn tại:
  - `models/v2_int8_fast/ecapa_int8_dynamic.onnx`
  - `models/q5_0/ggml-model-q5_0.bin`
- Nếu không thấy thì trả lỗi tiếng Việt.

`src-tauri/src/native_audio.rs`:

- FFI sang C++:
  - `celia_core_version`
  - `celia_core_initialize`
  - `celia_core_start_audio_pipeline`
  - `celia_core_stop_audio_pipeline`
- Có wrapper `initialize`, `start_pipeline`, `stop_pipeline`, nhưng flow Tauri hiện tại chỉ dùng `core_version` trực tiếp trong message; input monitor desktop dùng CPAL riêng.
- Với feature `desktop`, dùng `cpal` để mở default input device, đọc default config, tạo input stream theo sample format `F32`, `I16`, `U16`.
- Tính `rms` và `peak` từ kênh đầu tiên, lưu qua atomic bits.
- Dùng worker thread và channel stop vì `cpal::Stream` không `Send/Sync`.

Khi chuyển sang C++ thuần, phần tương đương cần chuyển sang:

- C++ app state thay cho `AudioPipelineState`.
- C++ bridge handler thay cho Tauri command.
- `miniaudio` input device thay cho `cpal`.
- C++ model path resolver thay cho `model_paths.rs`.
- Static asset/resource resolver thay cho Tauri resource dir.

## C++ stub cũ trong `src-tauri/native/cpp/`

`src-tauri/native/cpp/CMakeLists.txt`:

- Tạo static library `celia_audio_core`.
- Source:
  - `celia_audio_core.cpp`
  - `celia_audio_core.h`
- C++17.
- Include current source dir.

`celia_audio_core.h` export C ABI:

```cpp
const char* celia_core_version();
bool celia_core_initialize(const char* ecapa_model_path, const char* whisper_model_path);
bool celia_core_start_audio_pipeline();
bool celia_core_stop_audio_pipeline();
```

`celia_audio_core.cpp` hiện chỉ là stub:

- Có mutex global.
- Kiểm tra file model bằng `std::ifstream`.
- Lưu path ECAPA/Whisper vào string.
- `start_audio_pipeline` chỉ set `g_recording = true` nếu đã initialized.
- `stop_audio_pipeline` set `g_recording = false`.
- Chưa có audio thật, chưa có ONNX Runtime, chưa có whisper.cpp.

Khi chuyển sang C++ thuần có thể tái sử dụng ý tưởng kiểm tra model path, nhưng không nên giữ ABI này nếu app C++ mới gọi trực tiếp class/module nội bộ.

## Lệnh hiện tại

Root scripts hiện tại:

```powershell
npm run frontend:dev
npm run frontend:build
npm run tauri:dev
npm run tauri:build
```

Trong đó `tauri:*` hiện sẽ không chạy vì `cargo` không có trong PATH và Rust toolchain không được tìm thấy trong lần quét này.

Lệnh frontend:

```powershell
npm --prefix frontend install
npm --prefix frontend run dev
npm --prefix frontend run build
```

Lệnh CMake bằng CLion bundled MinGW/Ninja, sau khi đã có source hợp lệ:

```powershell
& 'D:\Application\JetBrains\CLion 2026.1\bin\cmake\win\x64\bin\cmake.exe' -S . -B cmake-build-debug-windows -G Ninja -DCMAKE_BUILD_TYPE=Debug
& 'D:\Application\JetBrains\CLion 2026.1\bin\cmake\win\x64\bin\cmake.exe' --build cmake-build-debug-windows
```

Lệnh CMake bằng MSVC/Ninja đã chạy thành công từ terminal ngày 2026-04-20:

```powershell
cmd.exe /c "call ""C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"" >nul && ""D:\Application\JetBrains\CLion 2026.1\bin\cmake\win\x64\bin\cmake.exe"" -S . -B cmake-build-msvc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MAKE_PROGRAM=""D:\Application\JetBrains\CLion 2026.1\bin\ninja\win\x64\ninja.exe"" && ""D:\Application\JetBrains\CLion 2026.1\bin\cmake\win\x64\bin\cmake.exe"" --build cmake-build-msvc-debug"
```

Trong Codex sandbox, lệnh này từng timeout ở bước `Detecting CXX compiler ABI info`. Khi chạy với quyền ngoài sandbox, configure/build pass trong khoảng vài giây. Nếu gặp timeout tương tự trong phiên sau, rerun với escalation.

Chạy executable sau build:

```powershell
& '.\cmake-build-msvc-debug\bin\Voice Embedded Verification.exe'
```

Output đã xác nhận:

```text
He thong Voice Embedded Verification da khoi dong!
```

## Việc cần làm sau khi user xác nhận `AGENTS.md`

1. Sửa include path CMake sang `src-cpp/includes`.
2. Kiểm tra compile khi include thật `webview.h` và `miniaudio.h` trong source.
3. Tạo C++ app shell bằng `webview.h` thay cho Tauri.
4. Build Vue bằng Vite và load `frontend/dist` trong webview release.
5. Thiết kế bridge JS <-> C++ thay cho `@tauri-apps/api/core invoke`.
6. Chuyển `audio_start_recording`, `audio_stop_recording`, `audio_get_input_level` sang C++ handlers.
7. Chuyển CPAL input monitor sang miniaudio:
   - mở default capture device,
   - đọc sample rate/channels/device name,
   - tính `rms`/`peak`,
   - expose trạng thái cho frontend.
8. Chuyển model path resolver sang C++ và đảm bảo bundle/copy resource model.
9. Sau khi C++ shell chạy ổn, mới xóa dần Tauri/Rust nếu user yêu cầu.

## Ràng buộc quan trọng

- Chưa thêm ONNX Runtime hoặc whisper.cpp cho đến khi bước audio/model thật sự cần.
- Chưa thêm thư viện permission/device OS API riêng nếu `miniaudio` đã xử lý được.
- Không làm desktop build phải link dependency mobile hoặc automotive.
- Không làm mobile/automotive build phải link API chỉ dành cho desktop.
- Giữ `node_modules` bên trong `frontend/`.
- Giữ UI không chứa logic OS/thiết bị.
- Giữ nguyên đường dẫn model hiện có khi model được đưa lại vào workspace.
- Không revert `CMakeLists.txt` hoặc thay đổi khác của user nếu không được yêu cầu.
- Sau cập nhật bộ nhớ này, dừng lại để user đọc và xác nhận trước khi đổi code Rust/Tauri sang C++.
