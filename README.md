# Voice Embedded Verification

Ứng dụng xác minh giọng nói cross-platform dùng C++ native shell, `webview/webview`, `miniaudio`, Vue 3, Vite và PrimeVue.

## Build Windows Package

```powershell
scripts\package-windows.cmd [WINDOWS]_TEST_V1.0.0
```

Pipeline này tự build Vue frontend, build C++ bằng MSVC, copy `frontend/dist` cạnh executable, rồi xuất artifact vào:

```text
builds/[WINDOWS]_TEST_V1.0.0/
```

Cấu trúc artifact:

```text
builds/[WINDOWS]_TEST_V1.0.0/app/Voice Embedded Verification.exe
builds/[WINDOWS]_TEST_V1.0.0/app/frontend/dist/
builds/[WINDOWS]_TEST_V1.0.0/distribute/Voice Embedded Verification.zip
builds/[WINDOWS]_TEST_V1.0.0/distribute/Voice Embedded Verification Setup.exe
```

`Voice Embedded Verification Setup.exe` là installer self-contained. Nó cài app vào:

```text
%LOCALAPPDATA%\Voice Embedded Verification\
```

và tạo shortcut ngoài Desktop.

## Windows Build Tools

Các script trong `scripts/` tự dò toolchain theo thứ tự:

- MSVC `vcvars64.bat` từ Visual Studio Build Tools/Visual Studio.
- `cmake.exe` từ PATH, Visual Studio CMake tools, standalone CMake, hoặc CLion nếu có.
- `ninja.exe` từ PATH, Visual Studio CMake tools, hoặc CLion nếu bạn bật Ninja.

Mặc định script dùng `NMake Makefiles` vì `nmake.exe` đi kèm MSVC Build Tools và không cần CLion/Ninja. Trên laptop không cần cài CLion. Cần có Visual Studio Build Tools với workload `Desktop development with C++` và một nguồn CMake: standalone CMake, hoặc component CMake trong Visual Studio Build Tools. Có thể override thủ công bằng biến môi trường:

```powershell
$env:CELIA_VCVARS64='C:\Program Files\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat'
$env:CELIA_CMAKE_EXE='C:\Program Files\CMake\bin\cmake.exe'
$env:CELIA_CMAKE_GENERATOR='Ninja'
$env:CELIA_NINJA_EXE='C:\Tools\ninja\ninja.exe'
```

Nếu chỉ muốn ưu tiên Ninja khi script tự tìm thấy, đặt `$env:CELIA_PREFER_NINJA='1'`.

## Runtime Models

Trước khi package Windows, đặt model sherpa-onnx ASR vào:

```text
models/sherpa-onnx-streaming-zipformer-ar_en_id_ja_ru_th_vi_zh-2025-02-10/
```

Thư mục này phải có ít nhất:

```text
encoder-epoch-75-avg-11-chunk-16-left-128.int8.onnx
decoder-epoch-75-avg-11-chunk-16-left-128.onnx
joiner-epoch-75-avg-11-chunk-16-left-128.int8.onnx
tokens.txt
```

`models/` không được commit vào git. Package script sẽ copy model vào `builds/[...]/app/models/` và sẽ dừng build nếu thiếu file bắt buộc.

## Test Micro Native

```powershell
$exe = (Get-Item -LiteralPath 'builds\[WINDOWS]_TEST_V1.0.0\app\Voice Embedded Verification.exe').FullName
& $exe --audio-smoke-test
```
