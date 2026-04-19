use crate::model_paths::ModelPaths;
use std::ffi::{CStr, CString};
use std::os::raw::c_char;

#[cfg(feature = "desktop")]
use cpal::traits::{DeviceTrait, HostTrait, StreamTrait};
#[cfg(feature = "desktop")]
use std::sync::atomic::{AtomicU32, AtomicU64, Ordering};
#[cfg(feature = "desktop")]
use std::sync::{mpsc, Arc, Mutex, OnceLock};
#[cfg(feature = "desktop")]
use std::thread::{self, JoinHandle};
#[cfg(feature = "desktop")]
use std::time::{SystemTime, UNIX_EPOCH};

extern "C" {
    fn celia_core_version() -> *const c_char;
    fn celia_core_initialize(
        ecapa_model_path: *const c_char,
        whisper_model_path: *const c_char,
    ) -> bool;
    fn celia_core_start_audio_pipeline() -> bool;
    fn celia_core_stop_audio_pipeline() -> bool;
}

pub struct InputLevel {
    pub rms: f32,
    pub peak: f32,
    pub sample_rate: u32,
    pub channels: u16,
    pub device_name: String,
    pub status: &'static str,
    pub updated_at_ms: u64,
}

pub fn core_version() -> String {
    unsafe {
        let version = celia_core_version();
        if version.is_null() {
            return "celia-audio-core/unknown".to_string();
        }

        CStr::from_ptr(version).to_string_lossy().into_owned()
    }
}

pub fn initialize(model_paths: &ModelPaths) -> Result<(), String> {
    let ecapa_model_path = CString::new(model_paths.ecapa_onnx.to_string_lossy().as_bytes())
        .map_err(|error| error.to_string())?;
    let whisper_model_path = CString::new(model_paths.whisper_ggml.to_string_lossy().as_bytes())
        .map_err(|error| error.to_string())?;

    let initialized =
        unsafe { celia_core_initialize(ecapa_model_path.as_ptr(), whisper_model_path.as_ptr()) };

    if initialized {
        Ok(())
    } else {
        Err("C++ audio core không khởi tạo được model paths.".to_string())
    }
}

pub fn start_pipeline() -> Result<(), String> {
    let started = unsafe { celia_core_start_audio_pipeline() };
    if started {
        Ok(())
    } else {
        Err("C++ audio core không khởi động được audio pipeline.".to_string())
    }
}

pub fn stop_pipeline() -> Result<(), String> {
    let stopped = unsafe { celia_core_stop_audio_pipeline() };
    if stopped {
        Ok(())
    } else {
        Err("C++ audio core không dừng được audio pipeline.".to_string())
    }
}

#[cfg(feature = "desktop")]
struct DesktopInputMonitor {
    stop_tx: mpsc::Sender<()>,
    worker: Option<JoinHandle<()>>,
    level_state: Arc<InputLevelState>,
    sample_rate: u32,
    channels: u16,
    device_name: String,
}

#[cfg(feature = "desktop")]
struct InputLevelState {
    rms_bits: AtomicU32,
    peak_bits: AtomicU32,
    updated_at_ms: AtomicU64,
}

#[cfg(feature = "desktop")]
impl InputLevelState {
    fn new() -> Self {
        Self {
            rms_bits: AtomicU32::new(0.0f32.to_bits()),
            peak_bits: AtomicU32::new(0.0f32.to_bits()),
            updated_at_ms: AtomicU64::new(0),
        }
    }

    fn store(&self, rms: f32, peak: f32) {
        self.rms_bits.store(rms.to_bits(), Ordering::Relaxed);
        self.peak_bits.store(peak.to_bits(), Ordering::Relaxed);
        self.updated_at_ms
            .store(current_time_millis(), Ordering::Relaxed);
    }
}

#[cfg(feature = "desktop")]
static DESKTOP_INPUT_MONITOR: OnceLock<Mutex<Option<DesktopInputMonitor>>> = OnceLock::new();

#[cfg(feature = "desktop")]
pub fn start_input_monitor() -> Result<(), String> {
    let monitor_slot = DESKTOP_INPUT_MONITOR.get_or_init(|| Mutex::new(None));
    let mut monitor = monitor_slot
        .lock()
        .map_err(|_| "Không khóa được trạng thái micro.".to_string())?;

    if monitor.is_some() {
        return Ok(());
    }

    let level_state = Arc::new(InputLevelState::new());
    let worker_level_state = Arc::clone(&level_state);
    let (init_tx, init_rx) = mpsc::channel();
    let (stop_tx, stop_rx) = mpsc::channel();

    let worker = thread::spawn(move || {
        run_input_monitor_worker(worker_level_state, init_tx, stop_rx);
    });

    let init_result = init_rx
        .recv()
        .map_err(|_| "Thread micro dừng trước khi khởi tạo xong.".to_string())?;
    let (sample_rate, channels, device_name) = match init_result {
        Ok(config) => config,
        Err(error) => {
            let _ = worker.join();
            return Err(error);
        }
    };

    *monitor = Some(DesktopInputMonitor {
        stop_tx,
        worker: Some(worker),
        level_state,
        sample_rate,
        channels,
        device_name,
    });

    Ok(())
}

#[cfg(not(feature = "desktop"))]
pub fn start_input_monitor() -> Result<(), String> {
    Err("Input monitor chỉ được bật cho desktop build.".to_string())
}

#[cfg(feature = "desktop")]
pub fn stop_input_monitor() -> Result<(), String> {
    let monitor_slot = DESKTOP_INPUT_MONITOR.get_or_init(|| Mutex::new(None));
    let mut monitor = monitor_slot
        .lock()
        .map_err(|_| "Không khóa được trạng thái micro.".to_string())?;
    if let Some(mut active_monitor) = monitor.take() {
        let _ = active_monitor.stop_tx.send(());
        if let Some(worker) = active_monitor.worker.take() {
            let _ = worker.join();
        }
    }
    *monitor = None;
    Ok(())
}

#[cfg(not(feature = "desktop"))]
pub fn stop_input_monitor() -> Result<(), String> {
    Ok(())
}

#[cfg(feature = "desktop")]
pub fn input_level() -> Result<InputLevel, String> {
    let monitor_slot = DESKTOP_INPUT_MONITOR.get_or_init(|| Mutex::new(None));
    let monitor = monitor_slot
        .lock()
        .map_err(|_| "Không khóa được trạng thái micro.".to_string())?;

    let Some(monitor) = monitor.as_ref() else {
        return Ok(InputLevel {
            rms: 0.0,
            peak: 0.0,
            sample_rate: 0,
            channels: 0,
            device_name: "No active input".to_string(),
            status: "idle",
            updated_at_ms: 0,
        });
    };

    Ok(InputLevel {
        rms: f32::from_bits(monitor.level_state.rms_bits.load(Ordering::Relaxed)),
        peak: f32::from_bits(monitor.level_state.peak_bits.load(Ordering::Relaxed)),
        sample_rate: monitor.sample_rate,
        channels: monitor.channels,
        device_name: monitor.device_name.clone(),
        status: "recording",
        updated_at_ms: monitor.level_state.updated_at_ms.load(Ordering::Relaxed),
    })
}

#[cfg(not(feature = "desktop"))]
pub fn input_level() -> Result<InputLevel, String> {
    Ok(InputLevel {
        rms: 0.0,
        peak: 0.0,
        sample_rate: 0,
        channels: 0,
        device_name: "No active input".to_string(),
        status: "idle",
        updated_at_ms: 0,
    })
}

#[cfg(feature = "desktop")]
fn run_input_monitor_worker(
    level_state: Arc<InputLevelState>,
    init_tx: mpsc::Sender<Result<(u32, u16, String), String>>,
    stop_rx: mpsc::Receiver<()>,
) {
    let result = run_input_monitor_stream(level_state, init_tx.clone(), stop_rx);
    if let Err(error) = result {
        let _ = init_tx.send(Err(error));
    }
}

#[cfg(feature = "desktop")]
fn run_input_monitor_stream(
    level_state: Arc<InputLevelState>,
    init_tx: mpsc::Sender<Result<(u32, u16, String), String>>,
    stop_rx: mpsc::Receiver<()>,
) -> Result<(), String> {
    let host = cpal::default_host();
    let device = host
        .default_input_device()
        .ok_or_else(|| "Không tìm thấy micro mặc định trên Windows.".to_string())?;
    let device_name = device
        .name()
        .unwrap_or_else(|_| "Default input device".to_string());
    let supported_config = device
        .default_input_config()
        .map_err(|error| format!("Không đọc được cấu hình micro mặc định: {error}"))?;
    let sample_rate = supported_config.sample_rate().0;
    let channels = supported_config.channels();
    let stream_config: cpal::StreamConfig = supported_config.clone().into();

    let stream = match supported_config.sample_format() {
        cpal::SampleFormat::F32 => {
            build_input_stream_f32(&device, &stream_config, Arc::clone(&level_state))?
        }
        cpal::SampleFormat::I16 => {
            build_input_stream_i16(&device, &stream_config, Arc::clone(&level_state))?
        }
        cpal::SampleFormat::U16 => {
            build_input_stream_u16(&device, &stream_config, Arc::clone(&level_state))?
        }
        format => {
            return Err(format!(
                "Định dạng sample của micro chưa được hỗ trợ: {format:?}"
            ));
        }
    };

    stream
        .play()
        .map_err(|error| format!("Không chạy được stream micro: {error}"))?;
    init_tx
        .send(Ok((sample_rate, channels, device_name)))
        .map_err(|_| "Không gửi được trạng thái khởi tạo micro.".to_string())?;

    // Keep the CPAL stream on the worker thread; `cpal::Stream` is not Send/Sync.
    let _ = stop_rx.recv();
    drop(stream);
    Ok(())
}

#[cfg(feature = "desktop")]
fn build_input_stream_f32(
    device: &cpal::Device,
    config: &cpal::StreamConfig,
    level_state: Arc<InputLevelState>,
) -> Result<cpal::Stream, String> {
    let channels = usize::from(config.channels);
    device
        .build_input_stream(
            config,
            move |data: &[f32], _| update_level_from_f32(data, channels, &level_state),
            move |error| eprintln!("Micro input stream error: {error}"),
            None,
        )
        .map_err(|error| format!("Không tạo được stream micro f32: {error}"))
}

#[cfg(feature = "desktop")]
fn build_input_stream_i16(
    device: &cpal::Device,
    config: &cpal::StreamConfig,
    level_state: Arc<InputLevelState>,
) -> Result<cpal::Stream, String> {
    let channels = usize::from(config.channels);
    device
        .build_input_stream(
            config,
            move |data: &[i16], _| {
                update_level(
                    data.iter().map(|sample| *sample as f32 / i16::MAX as f32),
                    channels,
                    &level_state,
                )
            },
            move |error| eprintln!("Micro input stream error: {error}"),
            None,
        )
        .map_err(|error| format!("Không tạo được stream micro i16: {error}"))
}

#[cfg(feature = "desktop")]
fn build_input_stream_u16(
    device: &cpal::Device,
    config: &cpal::StreamConfig,
    level_state: Arc<InputLevelState>,
) -> Result<cpal::Stream, String> {
    let channels = usize::from(config.channels);
    device
        .build_input_stream(
            config,
            move |data: &[u16], _| {
                update_level(
                    data.iter()
                        .map(|sample| (*sample as f32 - 32768.0) / 32768.0),
                    channels,
                    &level_state,
                )
            },
            move |error| eprintln!("Micro input stream error: {error}"),
            None,
        )
        .map_err(|error| format!("Không tạo được stream micro u16: {error}"))
}

#[cfg(feature = "desktop")]
fn update_level_from_f32(data: &[f32], channels: usize, level_state: &InputLevelState) {
    update_level(data.iter().copied(), channels, level_state);
}

#[cfg(feature = "desktop")]
fn update_level(
    samples: impl Iterator<Item = f32>,
    channels: usize,
    level_state: &InputLevelState,
) {
    let mut sum = 0.0f32;
    let mut peak = 0.0f32;
    let mut count = 0usize;
    let channel_count = channels.max(1);

    for (index, sample) in samples.enumerate() {
        if index % channel_count != 0 {
            continue;
        }

        let value = sample.clamp(-1.0, 1.0);
        let abs = value.abs();
        peak = peak.max(abs);
        sum += value * value;
        count += 1;
    }

    if count == 0 {
        return;
    }

    let rms = (sum / count as f32).sqrt();
    level_state.store(rms, peak);
}

#[cfg(feature = "desktop")]
fn current_time_millis() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|duration| duration.as_millis() as u64)
        .unwrap_or_default()
}
