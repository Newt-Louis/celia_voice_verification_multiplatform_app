# Hướng Dẫn Repository

## Cấu Trúc Dự Án & Tổ Chức Module

Repository này build ứng dụng desktop Windows cho xác minh giọng nói và speech-to-text. Native shell nằm trong `src-cpp/`: điều phối ở `src-cpp/app/`, thu âm và model ở `src-cpp/audio/`, header vendored ở `src-cpp/includes/`. Giao diện Vue 3 nằm trong `frontend/src/`. Runtime models đặt trong `models/`; binary sherpa-onnx nằm trong `third_party/`. `builds/`, `cmake-build-*`, và `frontend/dist/` là artifact sinh ra khi build.

## Lệnh Build, Test & Phát Triển

- `npm --prefix frontend install`: cài dependencies cho frontend.
- `npm run frontend:dev`: chạy Vite UI tại `127.0.0.1`.
- `npm run frontend:build`: chạy `vue-tsc --noEmit` và tạo `frontend/dist`.
- `npm run cpp:configure`: cấu hình build Windows bằng CMake/MSVC.
- `npm run cpp:build`: biên dịch native executable.
- `npm run windows:test`: package bản Windows test vào `builds/[WINDOWS]_TEST_V1.0.0/`.
- `scripts\package-windows.cmd [WINDOWS]_TEST_V1.0.0`: chạy pipeline package đầy đủ.

## Phong Cách Code & Quy Ước Đặt Tên

C++ dùng chuẩn C++17. Theo style hiện có: thụt lề 4 spaces, brace cùng dòng với function/control statement, `PascalCase` cho class như `CeliaApp`, và `snake_case` cho helper như `make_request_id`. Giữ native source trong module `app` và `audio`. Frontend dùng Vue SFC và TypeScript; thụt lề 2 spaces, file component `PascalCase`, biến/function `camelCase`. Ưu tiên typed interfaces trong `frontend/src/core/**/types.ts`.

## Hướng Dẫn Kiểm Thử

Hiện chưa có một test suite tự động duy nhất. Xem `npm run frontend:build` là bước kiểm tra bắt buộc cho typecheck/build frontend. Để kiểm tra native audio, hãy build/package trước, rồi chạy:

```powershell
$exe = (Get-Item -LiteralPath 'builds\[WINDOWS]_TEST_V1.0.0\app\Voice Embedded Verification.exe').FullName
& $exe --audio-smoke-test
```

Với thử nghiệm Whisper, đặt tên script theo mẫu `test_*.py`, ví dụ `test_whisper_q5_0_all_beams.py`.

## Commit & Pull Request

Các commit gần đây chủ yếu là ghi chú trạng thái không chính thức. Từ nay nên dùng summary rõ, dạng mệnh lệnh, ví dụ `fix bluetooth audio capture noise`. Pull request cần mô tả thay đổi, liệt kê lệnh đã chạy, nêu model hoặc third-party binary cần thiết, và đính kèm screenshot hoặc đoạn ghi ngắn cho thay đổi UI/audio.

## Bảo Mật & Cấu Hình

Không commit bản ghi âm riêng tư, credentials, hoặc path riêng theo máy. File model lớn nên để trong `models/` và ghi rõ tên khi cần. Tránh sửa nội dung vendored trong `src-cpp/whisper.cpp/` hoặc `third_party/` trừ khi đó là cập nhật có chủ đích.
