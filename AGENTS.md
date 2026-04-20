# Voice Embedded Verification - Bộ nhớ dự án

## Mục tiêu hiện tại

Repository này là ứng dụng xác minh giọng nói dùng C++ native shell, `webview/webview`, `miniaudio`, Vue 3, Vite và PrimeVue 4.

Mục tiêu hoạt động đầu tiên là desktop Windows 11. Kiến trúc vẫn phải giữ đường mở cho desktop, mobile và automotive, nhưng không ép một target phải link dependency riêng của target khác.

## Stack hiện tại

- C++17 làm app shell, bridge native và audio pipeline.
- `webview/webview` mở native webview và nhúng UI Vue.
- `miniaudio` mở micro và đọc audio input cross-platform.
- Vue 3 + Vite + PrimeVue 4 làm frontend.
- CMake + Ninja + MSVC Build Tools dùng cho build Windows hiện tại.

## Bố cục repository

- `src-cpp/`
  Gốc hệ thống C++.
- `src-cpp/includes/`
  Vendored headers/native third-party headers.
- `src-cpp/includes/webview.h`
  Single-header generated từ repo chính thức `webview/webview`.
- `src-cpp/includes/miniaudio.h`
  Single-header tải từ repo chính thức `mackron/miniaudio`.
- `src-cpp/includes/WebView2*.h`
  Header WebView2 lấy từ package NuGet `Microsoft.Web.WebView2`.
- `src-cpp/app/`
  App lifecycle, native webview window, bridge JS/C++ và helper JSON nhỏ.
- `src-cpp/audio/`
  Audio service dùng miniaudio.
- `frontend/`
  UI Vue 3 + Vite + PrimeVue 4. Đây là nơi duy nhất được có `node_modules`.
- `CMakeLists.txt`
  CMake root cho executable `Voice_Embedded_Verification`.
- `scripts/configure-cpp-windows.cmd`
  Configure C++ Windows bằng MSVC + Ninja.
- `scripts/build-cpp-windows.cmd`
  Build C++ Windows bằng MSVC + Ninja.
- `AGENTS.md`
  Bộ nhớ dự án cho các phiên Codex sau.

## Frontend

Frontend dùng:

- Vue `^3.5.32`
- Vite `^6.4.2`
- PrimeVue `^4.5.5`
- `@primeuix/themes` `^2.0.3`
- Pinia `^3.0.4`
- Vue Router `^4.5.1`

Các script:

```powershell
npm --prefix frontend install
npm --prefix frontend run dev
npm --prefix frontend run build
npm --prefix frontend run preview
```

Runtime target:

```text
VITE_CELIA_RUNTIME_TARGET=web | native-desktop | native-mobile | automotive
```

Nếu không đặt env, frontend tự chọn `native-desktop` khi thấy bridge C++ trong `window`; nếu không có bridge thì fallback về `web`.

Frontend không chứa logic OS/thiết bị. UI gọi `AudioFacade`, facade chọn strategy:

- `WebStrategy.ts`: dùng Web Audio API để chạy trong browser.
- `NativeBridgeStrategy.ts`: gọi các hàm C++ được bind vào webview:
  - `window.celiaAudioStartRecording()`
  - `window.celiaAudioStopRecording(requestId)`
  - `window.celiaAudioGetInputLevel()`

## C++ App

Entry point:

- `src-cpp/main.cpp`

Các module chính:

- `src-cpp/app/CeliaApp.*`
  - Tạo webview window.
  - Bind audio bridge.
  - Load `frontend/dist/index.html` nếu có.
  - Fallback về `http://127.0.0.1:1420` khi chưa build frontend.
- `src-cpp/app/Json.*`
  - Escape JSON string và đọc argument string đầu tiên từ request bridge.
- `src-cpp/audio/AudioService.*`
  - Dùng `miniaudio`.
  - Mở default capture device.
  - Capture mono `f32`, sample rate `16000`.
  - Tính `rms` và `peak` trong callback.
  - Expose `sampleRate`, `channels`, `deviceName`, `status`, `updatedAtMs`.

Không cần tách sẵn folder `windows/`, `android/`, `macos/`, `ios/`, `linux/`, `qnx/` nếu API chung của `webview` và `miniaudio` đủ dùng. Chỉ tách platform folder khi có code thật sự khác nhau, ví dụ permission, resource path, packaging hoặc backend đặc thù.

## Third-party Headers

Đã tải/generate ngày 2026-04-20:

- `src-cpp/includes/miniaudio.h`
  - Nguồn: `https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h`
  - Version trong file: `miniaudio - v0.11.25 - 2026-03-04`
- `src-cpp/includes/webview.h`
  - Nguồn: `https://github.com/webview/webview`
  - Tạo bằng script chính thức `scripts/amalgamate/amalgamate.py`.
  - Version macro trong file: `0.12.0`.
- `src-cpp/includes/WebView2.h`
- `src-cpp/includes/WebView2EnvironmentOptions.h`
- `src-cpp/includes/WebView2Interop.h`
  - Nguồn: NuGet package `Microsoft.Web.WebView2`.

Khi cập nhật webview sau này, cần generate lại `webview.h` từ source chính thức và kiểm tra lại WebView2 headers.

## CMake

Root `CMakeLists.txt` hiện:

- Project `"Voice Embedded Verification"`.
- Target `Voice_Embedded_Verification`.
- C++17.
- Glob source từ `src-cpp/*.cpp` và `src-cpp/*.h`.
- Include:
  - `src-cpp`
  - `src-cpp/includes`
- Windows libs hiện dùng:
  - `advapi32`
  - `dwmapi`
  - `ole32`
  - `shell32`
  - `version`
  - `shlwapi`
  - `user32`
  - `windowscodecs`

Output executable:

```text
cmake-build-msvc-debug/bin/Voice Embedded Verification.exe
```

## Toolchain Windows Đã Xác Nhận

Node.js:

```text
D:\NodeJS\node.exe
v20.16.0
```

npm:

```text
D:\NodeJS\npm.cmd
10.8.1
```

Git:

```text
D:\Git\setup-git\cmd\git.exe
2.45.2.windows.1
```

Python:

```text
D:\Python-3.13.3\python.exe
3.13.3
```

CLion:

```text
D:\Application\JetBrains\CLion 2026.1
```

CLion CMake:

```text
D:\Application\JetBrains\CLion 2026.1\bin\cmake\win\x64\bin\cmake.exe
cmake version 4.2.2
```

CLion Ninja:

```text
D:\Application\JetBrains\CLion 2026.1\bin\ninja\win\x64\ninja.exe
1.13.2
```

MSVC Build Tools:

```text
C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools
```

MSVC compiler sau khi gọi `vcvars64.bat`:

```text
C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\cl.exe
Microsoft C/C++ Optimizing Compiler Version 19.50.35729 for x64
```

## Lệnh Build

Build frontend:

```powershell
npm --prefix frontend run build
```

Configure C++ Windows:

```powershell
npm run cpp:configure
```

Build C++ Windows:

```powershell
npm run cpp:build
```

Lệnh CMake đầy đủ đã chạy thành công từ terminal:

```powershell
cmd.exe /c "call ""C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"" >nul && ""D:\Application\JetBrains\CLion 2026.1\bin\cmake\win\x64\bin\cmake.exe"" -S . -B cmake-build-msvc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MAKE_PROGRAM=""D:\Application\JetBrains\CLion 2026.1\bin\ninja\win\x64\ninja.exe"" && ""D:\Application\JetBrains\CLion 2026.1\bin\cmake\win\x64\bin\cmake.exe"" --build cmake-build-msvc-debug"
```

Trong Codex sandbox, configure từng timeout ở bước detect compiler ABI. Khi chạy với quyền ngoài sandbox, configure/build pass trong vài giây. Nếu gặp timeout tương tự, rerun với escalation.

Chạy executable:

```powershell
& '.\cmake-build-msvc-debug\bin\Voice Embedded Verification.exe'
```

## Trạng thái kiểm chứng

Đã pass ngày 2026-04-20:

```powershell
npm --prefix frontend run build
npm run cpp:configure
npm run cpp:build
```

Output C++:

```text
cmake-build-msvc-debug/bin/Voice Embedded Verification.exe
```

Đã mở thử executable trong vài giây bằng `Start-Process`; process sống và đã được tắt lại bằng `Stop-Process`.

## Models

Khi model có mặt trong workspace, giữ nguyên đường dẫn:

```text
models/v2_int8_fast/ecapa_int8_dynamic.onnx
models/q5_0/ggml-model-q5_0.bin
```

Lần quét ngày 2026-04-20 không thấy thư mục `models/` trong workspace hiện tại. Khi đóng gói app C++ release, cần thêm cơ chế copy/bundle hai model này nếu user đưa lại model vào workspace.

## Quy Tắc

- Không đưa logic OS/thiết bị vào Vue component.
- Native bridge là ranh giới giữa UI và C++.
- Dùng miniaudio API chung trước; chỉ tách platform-specific code khi thật sự cần.
- Không thêm ONNX Runtime hoặc whisper.cpp cho đến khi bắt đầu tích hợp model thật.
- Giữ `node_modules` bên trong `frontend/`.
- Không revert thay đổi của user nếu không được yêu cầu.
