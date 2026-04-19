fn main() {
    println!("cargo:rerun-if-changed=native/cpp/celia_audio_core.cpp");
    println!("cargo:rerun-if-changed=native/cpp/celia_audio_core.h");

    cc::Build::new()
        .cpp(true)
        .file("native/cpp/celia_audio_core.cpp")
        .include("native/cpp")
        .flag_if_supported("-std=c++17")
        .flag_if_supported("/std:c++17")
        .compile("celia_audio_core");

    tauri_build::build();
}
