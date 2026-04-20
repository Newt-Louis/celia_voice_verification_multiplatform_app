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
  App lifecycle, native webview window, static frontend server, bridge JS/C++ và helper JSON nhỏ.
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
- `scripts/package-windows.cmd`
  Pipeline build Windows đầy đủ: configure CMake, build Vue frontend, build C++, copy frontend cạnh exe, package vào `builds/[WINDOWS]_TEST_V1.0.0`.
- `tools/windows-installer/installer_stub.cpp`
  Installer `.exe` self-contained. Script package compile stub này, append portable zip vào cuối `.exe`, rồi khi chạy installer tự bung app vào `%LOCALAPPDATA%\Voice Embedded Verification`.
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

Vite config cần giữ `base: './'` và Vue Router dùng hash history để build output hoạt động tốt trong app packaged.

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
  - Start static HTTP server nội bộ để serve `frontend/dist`.
  - WebView2 load UI qua `http://127.0.0.1:<port>/`, không dùng `file:///` vì ES module build của Vite từng làm trắng màn hình khi load qua file URL.
  - Fallback về `http://127.0.0.1:1420` khi chưa build frontend.
- `src-cpp/app/StaticFileServer.*`
  - HTTP server nhỏ trên loopback.
  - Serve `index.html`, JS, CSS, fonts và SVG từ `frontend/dist`.
  - Dùng để app packaged render Vue ổn định trong WebView2.
- `src-cpp/app/Json.*`
  - Escape JSON string và đọc argument string đầu tiên từ request bridge.
- `src-cpp/audio/AudioService.*`
  - Dùng `miniaudio`.
  - Mở default capture device.
  - Capture mono `f32`, sample rate `16000`.
  - Tính raw `rms`/`peak` trong callback.
  - Có DSP layer nhẹ sau callback:
    - DC blocker / high-pass đơn giản.
    - Noise-floor estimator.
    - Soft noise gate để giảm nền.
    - Energy VAD theo `processedRms`, `processedPeak`, `noiseFloor`, có hangover ngắn để tránh nhấp nháy.
  - Expose `sampleRate`, `channels`, `deviceName`, `status`, `updatedAtMs`, `processedRms`, `processedPeak`, `noiseFloor`, `vadProbability`, `vadActive`, `speechFrames`, `transcriptionStatus`, `transcript`.
  - `transcriptionStatus` hiện phản ánh worker sherpa-onnx: `loading_sherpa_onnx`, `ready`, `listening`, `streaming`, `error`.

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
  - `third_party/sherpa-onnx/sherpa-onnx-v1.12.39-win-x64-shared-MD-Release-no-tts/include`
- Link sherpa-onnx runtime:
  - `sherpa-onnx-cxx-api.lib`
  - `sherpa-onnx-c-api.lib`
  - `onnxruntime.lib`
- Post-build copy DLL cạnh exe:
  - `sherpa-onnx-cxx-api.dll`
  - `sherpa-onnx-c-api.dll`
  - `onnxruntime.dll`
  - `onnxruntime_providers_shared.dll`
- Windows libs hiện dùng:
  - `advapi32`
  - `dwmapi`
  - `ole32`
  - `shell32`
  - `version`
  - `shlwapi`
  - `user32`
  - `windowscodecs`

Output executable hiện dùng Release build:

```text
cmake-build-msvc-release/bin/Voice Embedded Verification.exe
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

Lệnh chính cho Windows test package:

```powershell
scripts\package-windows.cmd [WINDOWS]_TEST_V1.0.0
```

Pipeline này gọi CMake, CMake gọi `npm.cmd --prefix frontend run build` qua target `frontend_build`, sau đó build C++ và chạy target `package_app`.

Cấu trúc output:

```text
builds/[WINDOWS]_TEST_V1.0.0/app/Voice Embedded Verification.exe
builds/[WINDOWS]_TEST_V1.0.0/app/frontend/dist/
builds/[WINDOWS]_TEST_V1.0.0/distribute/Voice Embedded Verification.zip
```

Nếu cần chạy từng bước:

```powershell
scripts\configure-cpp-windows.cmd [WINDOWS]_TEST_V1.0.0
scripts\build-cpp-windows.cmd
```

Lệnh CMake package đầy đủ tương đương:

```powershell
cmd.exe /c "call ""C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"" >nul && ""D:\Application\JetBrains\CLion 2026.1\bin\cmake\win\x64\bin\cmake.exe"" -S . -B cmake-build-msvc-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=""D:\Application\JetBrains\CLion 2026.1\bin\ninja\win\x64\ninja.exe"" -DCELIA_PACKAGE_LABEL=""[WINDOWS]_TEST_V1.0.0"" && ""D:\Application\JetBrains\CLion 2026.1\bin\cmake\win\x64\bin\cmake.exe"" --build cmake-build-msvc-release --target package_app"
```

Trong Codex sandbox, configure từng timeout ở bước detect compiler ABI. Khi chạy với quyền ngoài sandbox, configure/build pass trong vài giây. Nếu gặp timeout tương tự, rerun với escalation.

Chạy executable:

```powershell
& '.\cmake-build-msvc-release\bin\Voice Embedded Verification.exe'
```

Chạy executable trong package với path literal vì thư mục có dấu `[` và `]`:

```powershell
$exe = (Get-Item -LiteralPath 'builds\[WINDOWS]_TEST_V1.0.0\app\Voice Embedded Verification.exe').FullName
& $exe
```

Test micro native bằng cùng `AudioService` mà UI gọi:

```powershell
$exe = (Get-Item -LiteralPath 'builds\[WINDOWS]_TEST_V1.0.0\app\Voice Embedded Verification.exe').FullName
& $exe --audio-smoke-test
```

## Trạng thái kiểm chứng

Đã pass ngày 2026-04-20:

```powershell
scripts\package-windows.cmd [WINDOWS]_TEST_V1.0.0
```

Output package:

```text
builds/[WINDOWS]_TEST_V1.0.0/app/Voice Embedded Verification.exe
builds/[WINDOWS]_TEST_V1.0.0/distribute/Voice Embedded Verification.zip
builds/[WINDOWS]_TEST_V1.0.0/distribute/Voice Embedded Verification Setup.exe
```

Đã mở thử executable trong vài giây bằng `.NET ProcessStartInfo`; process sống và đã được tắt lại.

Native audio smoke test đã pass với output dạng:

```json
{"rms":0.009262,"peak":0.021723,"sampleRate":16000,"channels":1,"deviceName":"Microphone (C-Media(R) Audio)","status":"recording","updatedAtMs":1776674237580}
```

Sau khi thêm DSP/VAD/NS nhẹ, compile trực tiếp bằng MSVC `cl.exe` đã pass vì CMake/Ninja không có trong PATH hiện tại:

```powershell
cmd.exe /c "call ""C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"" >nul && cl /nologo /std:c++17 /EHsc /D_CRT_SECURE_NO_WARNINGS /I src-cpp /I src-cpp\includes src-cpp\main.cpp src-cpp\app\CeliaApp.cpp src-cpp\app\Json.cpp src-cpp\app\StaticFileServer.cpp src-cpp\audio\AudioService.cpp /Fe:.codex_tmp\VoiceEmbeddedVerificationCheck.exe /link advapi32.lib dwmapi.lib ole32.lib shell32.lib version.lib shlwapi.lib user32.lib windowscodecs.lib ws2_32.lib"
```

Audio smoke test mới đã pass với output dạng:

```json
{"rms":0.000878,"peak":0.001515,"processedRms":0.000158,"processedPeak":0.000283,"noiseFloor":0.001500,"vadProbability":0.000000,"sampleRate":16000,"channels":1,"deviceName":"Microphone (3- USB Audio Device)","status":"recording","vadActive":false,"speechFrames":0,"updatedAtMs":1776694678854,"transcriptionStatus":"ready","transcript":""}
```

Installer silent test đã pass:

```powershell
$setup = (Get-Item -LiteralPath 'builds\[WINDOWS]_TEST_V1.0.0\distribute\Voice Embedded Verification Setup.exe').FullName
& $setup --silent
```

Installer cài vào:

```text
%LOCALAPPDATA%\Voice Embedded Verification\
```

UI packaged từng bị trắng vì Vite build tạo `/assets/...` và WebView2 load bằng `file:///`. Đã sửa bằng:

- `frontend/vite.config.ts`: `base: './'`
- `frontend/src/router/index.ts`: hash history
- C++ `StaticFileServer`: serve `frontend/dist` qua loopback HTTP

Đã chụp kiểm chứng UI:

- `builds/[WINDOWS]_TEST_V1.0.0/ui-screenshot-foreground.png`: UI render được.
- `builds/[WINDOWS]_TEST_V1.0.0/ui-screenshot-recording-click2.png`: sau khi bấm `Bắt đầu thu`, UI chuyển sang `recording`, meter Peak/RMS nhảy và nhận `Microphone (C-Media(R) Audio)`.

## Models

Khi model có mặt trong workspace, giữ nguyên đường dẫn:

```text
models/v2_int8_fast/ecapa_int8_dynamic.onnx
models/sherpa-onnx-streaming-zipformer-ar_en_id_ja_ru_th_vi_zh-2025-02-10/
```

Lần quét ngày 2026-04-21 hiện thấy model sherpa-onnx và ECAPA trong workspace:

```text
models/sherpa-onnx-streaming-zipformer-ar_en_id_ja_ru_th_vi_zh-2025-02-10/
models/v2_int8_fast/ecapa_int8_dynamic.onnx
```

Model Whisper cũ `models/q5_0/ggml-model-q5_0.bin` nếu còn tồn tại thì chỉ là dữ liệu lịch sử, app hiện không dùng.

CMake `package_app` đã được cập nhật để copy model sherpa đang dùng và ECAPA vào:

```text
builds/[WINDOWS]_TEST_V1.0.0/app/models/
```

Nếu chạy script package mà không thấy model trong artifact, cần reconfigure CMake để nhận thay đổi `CMakeLists.txt`.

## Pipeline Cấp Bách Hiện Tại

Cập nhật ngày 2026-04-21: runtime ASR đã đổi từ Whisper sang `sherpa-onnx` streaming Zipformer. `src-cpp/whisper.cpp` và model `models/q5_0/ggml-model-q5_0.bin` có thể còn nằm trong workspace như dữ liệu lịch sử, nhưng app hiện không link `whisper` và không dùng `WhisperService` nữa.

Mục tiêu demo gần nhất:

```text
Micro realtime -> miniaudio capture -> mono f32 16 kHz -> NS nhẹ -> VAD -> sherpa-onnx streaming Zipformer -> transcript realtime
```

Model sherpa-onnx hiện dùng:

```text
models/sherpa-onnx-streaming-zipformer-ar_en_id_ja_ru_th_vi_zh-2025-02-10/encoder-epoch-75-avg-11-chunk-16-left-128.int8.onnx
models/sherpa-onnx-streaming-zipformer-ar_en_id_ja_ru_th_vi_zh-2025-02-10/decoder-epoch-75-avg-11-chunk-16-left-128.onnx
models/sherpa-onnx-streaming-zipformer-ar_en_id_ja_ru_th_vi_zh-2025-02-10/joiner-epoch-75-avg-11-chunk-16-left-128.int8.onnx
models/sherpa-onnx-streaming-zipformer-ar_en_id_ja_ru_th_vi_zh-2025-02-10/tokens.txt
```

Runtime sherpa-onnx đã vendor:

```text
third_party/sherpa-onnx/sherpa-onnx-v1.12.39-win-x64-shared-MD-Release-no-tts/
```

Cấu hình ASR hiện tại:

- `provider=cpu`
- `num_threads=1`
- `sample_rate=16000`
- `feature_dim=80`
- `decoding_method=greedy_search`
- `max_active_paths=1`
- `enable_endpoint=true`

Các phần phải tạm bỏ qua bằng TODO vì thời gian cấp bách:

- TODO(wakeword): Chưa có trạng thái chờ tiết kiệm năng lượng.
- TODO(wakeword): Chưa có wakeword/từ gọi để bật pipeline đầy đủ.
- TODO(model-lifecycle): UI mới hiển thị trạng thái `loading_sherpa_onnx/ready/listening/streaming/error`, nhưng chưa có màn hình khóa thao tác cho tới khi model load xong.
- TODO(ecapa): Chưa gắn ECAPA-TDNN vào pipeline.
- TODO(ecapa): Chưa có cửa sổ trượt xác thực 3 giây, stride 1 giây.
- TODO(ecapa): Chưa có ONNX Runtime/native feature cho `models/v2_int8_fast/ecapa_int8_dynamic.onnx`.
- TODO(library): Chưa tách pipeline thành SDK/library public cho app khác gọi.
- TODO(audio-callback): Demo hiện còn cấp phát `std::vector<float>` trong callback để đẩy frame sang recognizer; khi tối ưu cần đổi sang ring buffer/lock-free queue.

Điểm đang có sau thay đổi mới:

- [SUSCESS] Micro capture đang request mono f32 16 kHz qua `miniaudio`.
- [SUSCESS] DSP layer chạy trong `AudioService::update_level`.
- [SUSCESS] NS nhẹ đang gồm DC blocker/high-pass + noise-floor estimator + soft noise gate.
- [SUSCESS] VAD nhẹ đang dùng `processedRms`, `processedPeak`, `noiseFloor`, hangover ngắn.
- [SUSCESS] Native bridge trả thêm `processedRms`, `processedPeak`, `noiseFloor`, `vadProbability`, `vadActive`, `speechFrames`, `transcriptionStatus`, `transcript`.
- [SUSCESS] UI hiển thị meter sau lọc NS, VAD %, trạng thái VAD và textarea transcript realtime full chiều ngang.
- [SUSCESS] Textarea transcript hiện cho phép gõ tay để debug, không còn `readonly`.
- [SUSCESS] `WhisperService.*` đã bị thay bằng `SherpaOnnxService.*`.
- [SUSCESS] CMake đã bỏ `add_subdirectory(src-cpp/whisper.cpp)`, bỏ link target `whisper`, và link `sherpa-onnx-cxx-api.lib`, `sherpa-onnx-c-api.lib`, `onnxruntime.lib`.
- [SUSCESS] CMake post-build copy `sherpa-onnx-cxx-api.dll`, `sherpa-onnx-c-api.dll`, `onnxruntime.dll`, `onnxruntime_providers_shared.dll` cạnh exe.
- [SUSCESS] Audio callback không chạy inference trực tiếp; audio mono f32 16 kHz đã qua DSP được đưa sang worker `SherpaOnnxService` liên tục.
- [SUSCESS] Worker sherpa-onnx decode streaming bằng `OnlineRecognizer`/`OnlineStream`, trả partial transcript ngay trong lúc nói và commit transcript khi endpoint.
- [SUSCESS] `scripts\build-cpp-windows.cmd` pass ngày 2026-04-21.
- [SUSCESS] CLI chính thức `sherpa-onnx.exe` load model trong khoảng 3.1 giây, chạy sample `test_wavs/en.wav` với `num_threads=1`, RTF khoảng `0.11`.
- [SUSCESS] Dev smoke test `--sherpa-wav-test` trong executable của app đã feed `test_wavs/en.wav` theo frame 100 ms và in partial transcript tăng dần tới câu cuối.
- [SUSCESS] App `cmake-build-msvc-release/bin/Voice Embedded Verification.exe` mở sống sau 8 giây, working set đo nhanh khoảng `421 MB`.
- [SUSCESS] Package `[WINDOWS]_TEST_V1.0.0` tạo lại ngày 2026-04-21, artifact chỉ copy model sherpa và ECAPA, không copy `models/q5_0`.
- [SUSCESS] Artifact mới: `Voice Embedded Verification.zip` khoảng `274,458,240` bytes, `Voice Embedded Verification Setup.exe` khoảng `274,711,184` bytes.
- [SUSCESS] Bản packaged `builds/[WINDOWS]_TEST_V1.0.0/app/Voice Embedded Verification.exe` mở sống sau 8 giây, working set đo nhanh khoảng `422 MB`.
- [SUSCESS] Bản packaged chạy `--sherpa-wav-test` pass với exit code `0` và trả câu cuối từ `test_wavs/en.wav`.

Lệnh dev/test nhanh:

```powershell
scripts\configure-cpp-windows.cmd [WINDOWS]_TEST_V1.0.0
scripts\build-cpp-windows.cmd
& '.\cmake-build-msvc-release\bin\Voice Embedded Verification.exe'
& '.\cmake-build-msvc-release\bin\Voice Embedded Verification.exe' --sherpa-wav-test
```

Lưu ý build:

- Trên Windows phải để CMake dùng `npm.cmd`, không dùng `npm.ps1`; PowerShell policy có thể chặn `npm.ps1`.
- Trong Codex sandbox, Vite/esbuild có thể lỗi `spawn EPERM`; khi gặp lỗi này, rerun build với quyền ngoài sandbox.

## Quy Tắc

- Không đưa logic OS/thiết bị vào Vue component.
- Native bridge là ranh giới giữa UI và C++.
- Dùng miniaudio API chung trước; chỉ tách platform-specific code khi thật sự cần.
- Không thêm ONNX Runtime cho ECAPA cho đến khi bắt đầu tích hợp xác thực giọng nói thật.
- Giữ `node_modules` bên trong `frontend/`.
- Không revert thay đổi của user nếu không được yêu cầu.
