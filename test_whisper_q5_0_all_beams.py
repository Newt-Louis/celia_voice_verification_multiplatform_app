import os
import subprocess
import re
import json
import time
from datetime import datetime

def run_command(cmd):
    # Sử dụng /usr/bin/time -v để đo RAM trên Linux
    full_cmd = f"/usr/bin/time -v {cmd}"
    process = subprocess.Popen(full_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, shell=True)
    stdout, stderr = process.communicate()
    return stdout, stderr

def parse_benchmark_output(stdout, stderr):
    results = {"peak_ram_mb": 0, "transcribe_ms": 0, "transcript": ""}
    
    # Parse RAM từ output của /usr/bin/time -v
    ram_match = re.search(r"Maximum resident set size \(kbytes\): (\d+)", stderr)
    if ram_match:
        results["peak_ram_mb"] = int(ram_match.group(1)) / 1024
        
    # Parse thời gian xử lý từ output của whisper.cpp
    # whisper.cpp in thông tin thời gian ra stderr
    time_match = re.search(r"whisper_full_with_state:.*?total time\s*=\s*([\d.]+)\s*ms", stderr)
    if not time_match:
        # Thử regex khác nếu định dạng output thay đổi
        time_match = re.search(r"total time\s*=\s*([\d.]+)\s*ms", stderr)
        
    if time_match:
        results["transcribe_ms"] = float(time_match.group(1))
        
    # Parse nội dung transcript từ stdout
    transcript_lines = []
    for line in stdout.split('\n'):
        # Định dạng output: [00:00:00.000 --> 00:00:03.000]   Nội dung...
        if "-->" in line:
            # Loại bỏ mốc thời gian
            content = re.sub(r'\[.*?\]\s*', '', line).strip()
            if content:
                transcript_lines.append(content)
    
    results["transcript"] = " ".join(transcript_lines)
    return results

def main():
    project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../"))
    whisper_cli = os.path.join(project_root, "whisper.cpp/build/bin/whisper-cli")
    q5_0_model = os.path.join(project_root, "quantization/ggml-whispersmall-multilingual/q5_0/ggml-model-q5_0.bin")
    test_audio_m4a = os.path.join(project_root, "my_test_voice/pharse_2/my_voice_vie_1.m4a")
    test_audio_wav = os.path.join(project_root, "model_development/tests/tmp_test_q5_0.wav")
    
    # Keywords để mồi model (Initial Prompt)
    keywords = ["giọng nói", "xác thực", "danh tính", "chìa khóa", "bật", "mở", "tắt", "thiết bị", "thông minh", "người dùng"]
    initial_prompt = "Đây là đoạn hội thoại tiếng Việt về: " + ", ".join(keywords)

    print(f"--- BENCHMARK CHI TIẾT Q5_0 (BEAM-SIZE 1 -> 5) ---")
    
    # Convert audio sang WAV 16kHz Mono (yêu cầu của whisper.cpp)
    subprocess.run(["ffmpeg", "-y", "-i", test_audio_m4a, "-ar", "16000", "-ac", "1", "-c:a", "pcm_s16le", test_audio_wav], 
                   capture_output=True)
    
    if not os.path.exists(test_audio_wav):
        print("LỖI: Không thể tạo file WAV tạm.")
        return

    all_results = []
    model_size_mb = os.path.getsize(q5_0_model) / (1024 * 1024)

    for beam in range(1, 6):
        print(f"Đang chạy Beam-Size = {beam}...", end="", flush=True)
        
        # Lệnh chạy whisper-cli
        # -t 4: sử dụng 4 threads
        # -l vi: ngôn ngữ Tiếng Việt
        cmd = f'{whisper_cli} -m {q5_0_model} -f {test_audio_wav} -l vi -t 4 --prompt "{initial_prompt}" --beam-size {beam}'
        
        stdout, stderr = run_command(cmd)
        res = parse_benchmark_output(stdout, stderr)
        
        res["beam_size"] = beam
        res["model_name"] = "Q5_0 (5-bit)"
        res["file_size_mb"] = model_size_mb
        
        all_results.append(res)
        
        print(f" XONG")
        print(f"   RAM: {res['peak_ram_mb']:.2f} MB | Time: {res['transcribe_ms']:.2f} ms")
        print(f"   Transcript: \"{res['transcript']}\"")
        print("-" * 50)

    # Lưu báo cáo riêng cho Q5_0
    report_path = os.path.join(project_root, "benchmarks/q5_0_comprehensive_report.json")
    report_data = {
        "date": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "model": "Q5_0",
        "description": "Benchmark cho model Q5_0 với beam-size từ 1 đến 5",
        "data": all_results
    }
    
    with open(report_path, "w", encoding="utf-8") as f:
        json.dump(report_data, f, ensure_ascii=False, indent=2)
    
    print(f"\nBáo cáo chi tiết đã được lưu tại: {report_path}")
    
    # Dọn dẹp
    if os.path.exists(test_audio_wav):
        os.remove(test_audio_wav)

if __name__ == "__main__":
    main()
