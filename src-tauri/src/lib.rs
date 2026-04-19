mod model_paths;
mod native_audio;

use serde::Serialize;
use std::sync::Mutex;
use std::time::{SystemTime, UNIX_EPOCH};
use tauri::{Manager, State};

#[derive(Default)]
struct AudioPipelineState {
    active_request_id: Mutex<Option<String>>,
}

#[derive(Serialize)]
struct NativeAudioResponse {
    request_id: Option<String>,
    message: String,
}

#[derive(Serialize)]
struct NativeAudioLevelResponse {
    rms: f32,
    peak: f32,
    sample_rate: u32,
    channels: u16,
    device_name: String,
    status: String,
    updated_at_ms: u64,
}

#[tauri::command]
fn audio_start_recording(
    state: State<'_, AudioPipelineState>,
) -> Result<NativeAudioResponse, String> {
    native_audio::start_input_monitor()?;

    let request_id = format!(
        "audio-{}",
        SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .map_err(|error| error.to_string())?
            .as_millis()
    );

    let mut active_request_id = state
        .active_request_id
        .lock()
        .map_err(|_| "Audio pipeline state lock failed".to_string())?;
    *active_request_id = Some(request_id.clone());

    Ok(NativeAudioResponse {
        request_id: Some(request_id),
        message: format!(
            "Micro desktop đã nhận tín hiệu qua Rust. Core hiện có: {}.",
            native_audio::core_version()
        ),
    })
}

#[tauri::command]
fn audio_stop_recording(
    request_id: Option<String>,
    state: State<'_, AudioPipelineState>,
) -> Result<NativeAudioResponse, String> {
    native_audio::stop_input_monitor()?;

    let mut active_request_id = state
        .active_request_id
        .lock()
        .map_err(|_| "Audio pipeline state lock failed".to_string())?;
    let completed_request_id = request_id.or_else(|| active_request_id.clone());
    *active_request_id = None;

    Ok(NativeAudioResponse {
        request_id: completed_request_id,
        message: "Đã dừng stream micro desktop.".to_string(),
    })
}

#[tauri::command]
fn audio_get_input_level() -> Result<NativeAudioLevelResponse, String> {
    let level = native_audio::input_level()?;

    Ok(NativeAudioLevelResponse {
        rms: level.rms,
        peak: level.peak,
        sample_rate: level.sample_rate,
        channels: level.channels,
        device_name: level.device_name,
        status: level.status.to_string(),
        updated_at_ms: level.updated_at_ms,
    })
}

pub fn run() {
    tauri::Builder::default()
        .manage(AudioPipelineState::default())
        .invoke_handler(tauri::generate_handler![
            audio_start_recording,
            audio_stop_recording,
            audio_get_input_level
        ])
        .setup(|app| {
            let model_paths = model_paths::resolve_model_paths(&app.handle())
                .map_err(|error| std::io::Error::new(std::io::ErrorKind::NotFound, error))?;
            app.manage(model_paths);
            Ok(())
        })
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
