#pragma once

#include <condition_variable>
#include <cstddef>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct whisper_context;
struct whisper_state;

namespace celia {

struct WhisperSnapshot {
    std::string status = "idle";
    std::string transcript;
};

class WhisperService {
public:
    WhisperService() = default;
    ~WhisperService();

    WhisperService(const WhisperService&) = delete;
    WhisperService& operator=(const WhisperService&) = delete;

    void start(const std::filesystem::path& model_path);
    void stop();
    void reset_stream();
    void push_audio(const float* samples, std::size_t sample_count, bool has_speech);
    WhisperSnapshot snapshot() const;

private:
    void worker_loop(std::filesystem::path model_path);
    void append_transcript(const std::string& text);
    void append_segments_from_state(whisper_state* state, int new_segment_count);
    static int inference_thread_count();
    static void on_new_segment(whisper_context* context, whisper_state* state, int new_segment_count, void* user_data);
    static bool should_abort(void* user_data);

    static constexpr int kSampleRate = 16000;
    static constexpr std::size_t kMinChunkSamples = kSampleRate;
    static constexpr std::size_t kMaxChunkSamples = kSampleRate * 3;
    static constexpr std::size_t kOverlapSamples = kSampleRate / 2;
    static constexpr std::size_t kMaxQueuedSamples = kSampleRate * 10;
    static constexpr std::size_t kMaxTranscriptChars = 12000;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::thread worker_;
    whisper_context* context_ = nullptr;
    bool stop_requested_ = false;
    bool running_ = false;
    bool abort_current_ = false;
    bool speech_seen_in_queue_ = false;
    std::vector<float> pending_samples_;
    std::string status_ = "idle";
    std::string transcript_;
};

} // namespace celia
