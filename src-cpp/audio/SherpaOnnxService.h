#pragma once

#include <condition_variable>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "sherpa-onnx/c-api/cxx-api.h"

namespace celia {

struct SherpaOnnxModelPaths {
    std::filesystem::path encoder;
    std::filesystem::path decoder;
    std::filesystem::path joiner;
    std::filesystem::path tokens;
};

struct TranscriptionSnapshot {
    std::string status = "idle";
    std::string transcript;
};

class SherpaOnnxService {
public:
    SherpaOnnxService() = default;
    ~SherpaOnnxService();

    SherpaOnnxService(const SherpaOnnxService&) = delete;
    SherpaOnnxService& operator=(const SherpaOnnxService&) = delete;

    void start(const SherpaOnnxModelPaths& model_paths);
    void stop();
    void reset_stream();
    void push_audio(const float* samples, std::size_t sample_count, bool has_speech);
    TranscriptionSnapshot snapshot() const;

private:
    void worker_loop(SherpaOnnxModelPaths model_paths);
    void update_stream_result();
    void set_error(const std::string& message);
    std::string current_transcript_locked() const;
    static bool has_all_model_files(const SherpaOnnxModelPaths& model_paths);
    static std::string model_paths_error(const SherpaOnnxModelPaths& model_paths);

    static constexpr int kSampleRate = 16000;
    static constexpr std::size_t kMaxQueuedSamples = kSampleRate * 6;
    static constexpr std::size_t kWakeChunkSamples = kSampleRate / 10;
    static constexpr std::size_t kMaxTranscriptChars = 12000;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::thread worker_;
    std::unique_ptr<sherpa_onnx::cxx::OnlineRecognizer> recognizer_;
    std::unique_ptr<sherpa_onnx::cxx::OnlineStream> stream_;
    bool stop_requested_ = false;
    bool running_ = false;
    bool reset_requested_ = false;
    bool speech_seen_in_queue_ = false;
    std::vector<float> pending_samples_;
    std::string status_ = "idle";
    std::string committed_transcript_;
    std::string partial_transcript_;
};

} // namespace celia
