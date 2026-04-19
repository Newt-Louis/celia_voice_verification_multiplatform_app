# Celia Voice Verification - Bộ nhớ dự án

## Mục tiêu hiện tại

Repository này đang được định hình thành một ứng dụng xác minh giọng nói cho desktop/mobile/automotive, dùng Tauri + Rust + C++.

Mục tiêu hoạt động đầu tiên là phát triển desktop trên Windows 11. Kiến trúc phải luôn sẵn sàng cho các bản build desktop, mobile và automotive mà không buộc mọi target phải mang theo mọi dependency đặc thù của từng nền tảng.

## Các model hiện có

Giữ nguyên các file model hiện có trong `models/` và đóng gói chúng cùng ứng dụng:

- Model ECAPA-TDNN ONNX INT8 dynamic quantized:
  `models/v2_int8_fast/ecapa_int8_dynamic.onnx`
- Model Whisper small multilingual quantized GGML:
  `models/q5_0/ggml-model-q5_0.bin`

Cấu hình Tauri bundle hiện đang đưa cả hai file model vào resources.

## Bố cục repository

- `frontend/`
  UI dùng Vue 3 + Vite + PrimeVue 4. Đây là nơi duy nhất được có `node_modules`.
- `src-tauri/`
  App shell Tauri, các command Rust, facade Rust bọc native C++.
- `src-tauri/native/cpp/`
  Ranh giới core C++ cho audio/model. Code hiện tại là stub, dùng để xác thực đường dẫn model và theo dõi trạng thái pipeline.
- `models/`
  Các file model dùng lúc chạy.
- `AGENTS.md`
  File bộ nhớ này dành cho các phiên Codex sau này.

## Kiến trúc frontend

Luồng dữ liệu:

1. UI components
   Các component PrimeVue/Vue chỉ render state và gọi phương thức của facade. Chúng không được chứa logic OS hoặc thiết bị.
2. Facade layer
   `frontend/src/core/audio/AudioFacade.ts` xác định runtime target và chọn strategy phù hợp.
3. Strategy layer
   `frontend/src/core/audio/strategies/`
   - `WebStrategy.ts`: dùng Web Audio API để test trên browser.
   - `TauriDesktopStrategy.ts`: dùng Tauri `invoke()`.
   - `TauriMobileStrategy.ts`: để dành cho tích hợp native plugin trên mobile.
   - `AutomotiveStrategy.ts`: để dành cho tích hợp automotive OS.
4. Infrastructure
   Browser/server, Tauri desktop, Tauri mobile, hoặc automotive runtime.

Runtime target được điều khiển bằng:

```text
VITE_CELIA_RUNTIME_TARGET=web | tauri-desktop | tauri-mobile | automotive
```

Nếu không đặt giá trị env, facade sẽ tự phát hiện Tauri; nếu không phải Tauri thì fallback về `web`.

## Kiến trúc native

Rust chịu trách nhiệm điều phối và các command Tauri. C++ chịu trách nhiệm cho phần xử lý audio/model nặng trong tương lai.

Các command Tauri hiện tại:

- `audio_start_recording`
- `audio_stop_recording`

Các file native Rust hiện tại:

- `src-tauri/src/lib.rs`
- `src-tauri/src/model_paths.rs`
- `src-tauri/src/native_audio.rs`

Các file ABI C++ hiện tại:

- `src-tauri/native/cpp/celia_audio_core.h`
- `src-tauri/native/cpp/celia_audio_core.cpp`

API C++ hiện đang expose:

```cpp
const char* celia_core_version();
bool celia_core_initialize(const char* ecapa_model_path, const char* whisper_model_path);
bool celia_core_start_audio_pipeline();
bool celia_core_stop_audio_pipeline();
```

Implementation C++ hiện tại được cố ý giữ tối giản. Không thêm ONNX Runtime, whisper.cpp, audio OS, keyboard, mouse hoặc thư viện permission theo nền tảng cho đến khi feature đang triển khai thật sự cần chúng.

## Quy tắc target strategy

Giữ các dependency đặc thù nền tảng phía sau Rust module theo target, Cargo feature, hoặc native library riêng.

Không làm desktop build phải link dependency mobile hoặc automotive. Không làm mobile build phải link API thiết bị chỉ dành cho desktop. UI phải tiếp tục giao tiếp với các strategy thông qua facade interface.

Dạng Rust feature được khuyến nghị cho tương lai:

```toml
[features]
desktop = []
mobile = []
automotive = []
onnxruntime = []
whisper-cpp = []
```

Chỉ thêm dependency thật khi bắt đầu tích hợp chúng.

## Định hướng xử lý audio

Quyết định mặc định:

- Rust: điều phối app, target strategy, command Tauri, lifecycle, config, đường dẫn resource, điều khiển async.
- C++: tiền xử lý audio cấp thấp cần nằm gần ONNX Runtime hoặc whisper.cpp, gọi model, các filter nhạy về SIMD/native performance.

Nếu một audio filter đơn giản, an toàn và không gắn chặt với code model C++, có thể viết bằng Rust. Nếu nó phải chia sẻ buffer với ONNX Runtime hoặc whisper.cpp, đặt nó trong C++ để tránh copy lặp lại.

## Lệnh

Cài dependency frontend:

```powershell
cd frontend
npm install
```

Chạy riêng frontend:

```powershell
npm --prefix frontend run dev
```

Build frontend:

```powershell
npm --prefix frontend run build
```

Chạy Tauri dev từ repository root sau khi đã cài Rust toolchain:

```powershell
npm run tauri:dev
```

Build Tauri từ repository root sau khi đã cài Rust toolchain:

```powershell
npm run tauri:build
```

## Trạng thái toolchain từ lần setup ban đầu

Đã phát hiện:

- Node.js `20.16.0`
- Có npm

Không phát hiện trong PATH khi setup:

- `cargo`
- `rustc`
- `rustup`
- `cmake`
- `make`

Dependency frontend đã được cài vào `frontend/node_modules`.

Vite được pin ở `6.4.2` và `@vitejs/plugin-vue` ở `5.2.4` vì các phiên bản Vite mới hơn yêu cầu Node `>=20.19.0` trên máy này. `6.4.2` cũng xử lý các cảnh báo audit của Vite dev-server từng ảnh hưởng đến các bản Vite 6 cũ hơn.

Vue Router được pin ở `4.5.1` để tương thích với Vue 3 và tránh các dependency bắc cầu mới hơn yêu cầu Node `>=20.19.0`.

PrimeVue là version 4 và dùng `@primeuix/themes`.

## Trạng thái kiểm chứng

Frontend production build đã pass:

```powershell
npm run build
```

Tauri/Rust Windows build đã pass và đã tạo được app desktop + installer:

```powershell
npm run tauri:build
```

Rust/Cargo hiện được tìm thấy tại:

- `D:\.rust\.cargo\bin\cargo.exe`
- `D:\.rust\.cargo\bin\rustc.exe`

Build output cho Windows test hiện đặt trong:

```text
builds/[WINDOWS]_TEST_V1.0.0/
```

Khi build với đường dẫn có dấu `[` và `]`, PowerShell cần dùng path literal hoặc tự tạo absolute path bằng .NET, tránh để `Resolve-Path` hiểu `[` `]` như wildcard.

## Các công việc đã hoàn thành

- [SUSCESS] Đã dịch `AGENTS.md` sang tiếng Việt.
- [SUSCESS] Đã thêm Rust desktop input monitor bằng `cpal` để mở micro mặc định trên Windows và đo tín hiệu `peak`/`rms`.
- [SUSCESS] Đã thêm Tauri command `audio_get_input_level` để frontend đọc mức tín hiệu micro.
- [SUSCESS] Đã cập nhật UI để khi bấm `Bắt đầu thu`, app hiển thị mức tín hiệu micro, sample rate, số kênh và tên thiết bị.
- [SUSCESS] Đã giữ đúng phạm vi: chưa kết nối Whisper/ECAPA vào pipeline xử lý audio.
- [SUSCESS] Đã đóng gói cả hai model vào app resource:
  - `models/q5_0/ggml-model-q5_0.bin`
  - `models/v2_int8_fast/ecapa_int8_dynamic.onnx`
- [SUSCESS] Đã tạo icon tối thiểu tại `src-tauri/icons/icon.ico` để Tauri build Windows resource, MSI và NSIS installer.
- [SUSCESS] Đã sửa resolver model path để chạy được app release trực tiếp với layout resource của Tauri, bao gồm `_up_/models/...`.
- [SUSCESS] Đã build thành công Windows test tại `builds/[WINDOWS]_TEST_V1.0.0/`.
- [SUSCESS] Đã tạo được standalone executable:
  `builds/[WINDOWS]_TEST_V1.0.0/target/release/celia_voice_verification.exe`
- [SUSCESS] Đã tạo được installer MSI:
  `builds/[WINDOWS]_TEST_V1.0.0/target/release/bundle/msi/Celia Voice Verification_0.1.0_x64_en-US.msi`
- [SUSCESS] Đã tạo được installer NSIS setup.exe:
  `builds/[WINDOWS]_TEST_V1.0.0/target/release/bundle/nsis/Celia Voice Verification_0.1.0_x64-setup.exe`
- [SUSCESS] Đã mở thử app release trên Windows 11 hiện tại; app chạy được và không còn tự thoát vì lỗi model path.

## Cách gửi app Windows sang máy khác

Ưu tiên gửi file installer NSIS này sang máy khác để cài như phần mềm Windows bình thường:

```text
builds/[WINDOWS]_TEST_V1.0.0/target/release/bundle/nsis/Celia Voice Verification_0.1.0_x64-setup.exe
```

Có thể dùng file MSI này nếu muốn cài bằng Windows Installer:

```text
builds/[WINDOWS]_TEST_V1.0.0/target/release/bundle/msi/Celia Voice Verification_0.1.0_x64_en-US.msi
```

Không nên gửi riêng mỗi file `celia_voice_verification.exe` sang máy khác, vì bản standalone `.exe` cần resource đi kèm, trong đó có `_up_/models/...`. Nếu không dùng `setup.exe` hoặc `.msi`, cần nén cả thư mục release có đủ resource rồi giải nén trên máy khác:

```text
builds/[WINDOWS]_TEST_V1.0.0/target/release/
```

## Các bước triển khai tiếp theo

1. Test thủ công nút `Bắt đầu thu` trên app Windows release để xác nhận meter `Peak`/`RMS` nhảy khi nói vào micro.
2. Thay C++ stub bằng buffering input audio thật và tiền xử lý audio.
3. Chuẩn hóa luồng audio đầu vào về mono 16 kHz trước khi đưa vào model.
4. Thêm tích hợp ONNX Runtime cho ECAPA-TDNN phía sau một Cargo/native feature.
5. Thêm tích hợp whisper.cpp phía sau một feature riêng.
6. Chỉ thêm adapter theo target cho desktop, mobile và automotive khi từng target thật sự được triển khai.

## Ràng buộc quan trọng

- Giữ `node_modules` bên trong `frontend/`.
- Giữ UI không chứa logic OS/thiết bị.
- Giữ Rust làm lớp trung gian điều phối ứng dụng.
- Giữ C++ làm core xử lý model/audio, không phải code ứng dụng chính.
- Giữ nguyên các file và đường dẫn hiện có trong `models/` trừ khi được yêu cầu migrate rõ ràng.
- Tránh thêm các thư viện permission/device OS API trước khi feature thật sự cần chúng.
