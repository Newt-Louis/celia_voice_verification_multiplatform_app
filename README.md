# Voice Embedded Verification

Ứng dụng xác minh giọng nói cross-platform dùng C++ native shell, `webview/webview`, `miniaudio`, Vue 3, Vite và PrimeVue.

## Build Windows Package

```powershell
scripts\package-windows.cmd [WINDOWS]_TEST_V1.0.0
```

Pipeline này tự build Vue frontend, build C++ bằng MSVC/Ninja, copy `frontend/dist` cạnh executable, rồi xuất artifact vào:

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

## Test Micro Native

```powershell
$exe = (Get-Item -LiteralPath 'builds\[WINDOWS]_TEST_V1.0.0\app\Voice Embedded Verification.exe').FullName
& $exe --audio-smoke-test
```
