use std::path::{Path, PathBuf};
use tauri::{AppHandle, Manager};

#[derive(Clone)]
pub struct ModelPaths {
    pub ecapa_onnx: PathBuf,
    pub whisper_ggml: PathBuf,
}

pub fn resolve_model_paths(app: &AppHandle) -> Result<ModelPaths, String> {
    let candidates = collect_model_roots(app);

    for root in candidates {
        let paths = ModelPaths {
            ecapa_onnx: root.join("models/v2_int8_fast/ecapa_int8_dynamic.onnx"),
            whisper_ggml: root.join("models/q5_0/ggml-model-q5_0.bin"),
        };

        if paths.ecapa_onnx.exists() && paths.whisper_ggml.exists() {
            return Ok(paths);
        }
    }

    Err("Không tìm thấy model ECAPA ONNX hoặc Whisper GGML.".to_string())
}

fn collect_model_roots(app: &AppHandle) -> Vec<PathBuf> {
    let mut roots = Vec::new();

    if let Ok(resource_dir) = app.path().resource_dir() {
        push_model_root_candidates(&mut roots, resource_dir);
    }

    if let Ok(current_dir) = std::env::current_dir() {
        push_model_root_candidates(&mut roots, current_dir.clone());
        if let Some(parent) = current_dir.parent() {
            push_model_root_candidates(&mut roots, parent.to_path_buf());
        }
    }

    normalize_roots(roots)
}

fn push_model_root_candidates(roots: &mut Vec<PathBuf>, root: PathBuf) {
    roots.push(root.join("_up_"));
    roots.push(root);
}

fn normalize_roots(roots: Vec<PathBuf>) -> Vec<PathBuf> {
    let mut normalized = Vec::new();
    for root in roots {
        if !contains_path(&normalized, &root) {
            normalized.push(root);
        }
    }
    normalized
}

fn contains_path(paths: &[PathBuf], path: &Path) -> bool {
    paths.iter().any(|existing| existing == path)
}
