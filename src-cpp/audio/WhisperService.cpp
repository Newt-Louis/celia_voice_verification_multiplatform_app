#include "audio/WhisperService.h"

#include "whisper.h"

#include <algorithm>
#include <stdexcept>

namespace celia {
namespace {

std::string trim_text(std::string text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\n' || text.front() == '\r' || text.front() == '\t')) {
        text.erase(text.begin());
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\n' || text.back() == '\r' || text.back() == '\t')) {
        text.pop_back();
    }
    return text;
}

} // namespace

WhisperService::~WhisperService() {
    stop();
}

void WhisperService::start(const std::filesystem::path& model_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) {
        return;
    }

    if (model_path.empty() || !std::filesystem::exists(model_path)) {
        status_ = "error";
        transcript_ = "Không tìm thấy Whisper model: " + model_path.u8string();
        return;
    }

    stop_requested_ = false;
    running_ = true;
    status_ = "loading";
    transcript_.clear();
    pending_samples_.clear();
    worker_ = std::thread(&WhisperService::worker_loop, this, model_path);
}

void WhisperService::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_requested_ = true;
        cv_.notify_all();
    }

    if (worker_.joinable()) {
        worker_.join();
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (context_ != nullptr) {
        whisper_free(context_);
        context_ = nullptr;
    }
    running_ = false;
    if (status_ != "error") {
        status_ = "idle";
    }
    pending_samples_.clear();
}

void WhisperService::push_audio(const float* samples, std::size_t sample_count, bool has_speech) {
    if (samples == nullptr || sample_count == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_ || context_ == nullptr || status_ == "error") {
        return;
    }

    pending_samples_.insert(pending_samples_.end(), samples, samples + sample_count);
    speech_seen_in_queue_ = speech_seen_in_queue_ || has_speech;
    if (pending_samples_.size() > kMaxQueuedSamples) {
        pending_samples_.erase(pending_samples_.begin(), pending_samples_.begin() + static_cast<std::ptrdiff_t>(pending_samples_.size() - kMaxQueuedSamples));
    }

    if (pending_samples_.size() >= kMinChunkSamples && speech_seen_in_queue_) {
        cv_.notify_one();
    }
}

WhisperSnapshot WhisperService::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return WhisperSnapshot{status_, transcript_};
}

void WhisperService::worker_loop(std::filesystem::path model_path) {
    auto context_params = whisper_context_default_params();
    context_params.use_gpu = true;
    context_params.flash_attn = true;
    context_params.gpu_device = 0;
    auto* context = whisper_init_from_file_with_params(model_path.u8string().c_str(), context_params);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (context == nullptr) {
            status_ = "error";
            transcript_ = "Không load được Whisper model: " + model_path.u8string();
            running_ = false;
            return;
        }

        context_ = context;
        status_ = "ready";
    }

    for (;;) {
        std::vector<float> chunk;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] {
                return stop_requested_ || (pending_samples_.size() >= kMinChunkSamples && speech_seen_in_queue_);
            });

            if (stop_requested_) {
                break;
            }

            const auto chunk_size = (std::min)(pending_samples_.size(), kMaxChunkSamples);
            chunk.assign(pending_samples_.begin(), pending_samples_.begin() + static_cast<std::ptrdiff_t>(chunk_size));
            pending_samples_.erase(pending_samples_.begin(), pending_samples_.begin() + static_cast<std::ptrdiff_t>(chunk_size));
            speech_seen_in_queue_ = false;
            status_ = "transcribing " + std::to_string(chunk_size / kSampleRate) + "s";
        }

        static constexpr const char* kInitialPrompt =
            "Đây là đoạn hội thoại tiếng Việt về: giọng nói, xác thực, danh tính, chìa khóa, bật, mở, tắt, thiết bị, thông minh, người dùng";

        auto params = whisper_full_default_params(WHISPER_SAMPLING_BEAM_SEARCH);
        params.strategy = WHISPER_SAMPLING_BEAM_SEARCH;
        params.n_threads = inference_thread_count();
        params.translate = false;
        params.no_context = false;
        params.no_timestamps = false;
        params.single_segment = false;
        params.print_special = false;
        params.print_progress = false;
        params.print_realtime = false;
        params.print_timestamps = false;
        params.suppress_blank = true;
        params.suppress_nst = false;
        params.language = "vi";
        params.detect_language = false;
        params.initial_prompt = kInitialPrompt;
        params.beam_search.beam_size = 3;
        params.temperature = 0.0F;
        params.temperature_inc = 0.2F;
        params.entropy_thold = 2.40F;
        params.logprob_thold = -1.00F;
        params.no_speech_thold = 0.6F;

        const int result = whisper_full(context, params, chunk.data(), static_cast<int>(chunk.size()));
        if (result != 0) {
            std::lock_guard<std::mutex> lock(mutex_);
            status_ = "error";
            transcript_ += "\n[Whisper xử lý audio thất bại]";
            continue;
        }

        std::string text;
        const int segment_count = whisper_full_n_segments(context);
        for (int segment = 0; segment < segment_count; ++segment) {
            const char* segment_text = whisper_full_get_segment_text(context, segment);
            if (segment_text != nullptr) {
                text += segment_text;
            }
        }

        const auto cleaned_text = trim_text(text);
        if (cleaned_text.empty()) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (status_ != "error") {
                status_ = "ready_no_text";
            }
            continue;
        }

        append_transcript(cleaned_text);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (status_ != "error") {
                status_ = "ready";
            }
        }
    }
}

void WhisperService::append_transcript(const std::string& text) {
    if (text.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (!transcript_.empty()) {
        transcript_ += '\n';
    }
    transcript_ += text;

    if (transcript_.size() > kMaxTranscriptChars) {
        transcript_.erase(0, transcript_.size() - kMaxTranscriptChars);
    }
}

int WhisperService::inference_thread_count() {
    const auto hardware_threads = std::thread::hardware_concurrency();
    if (hardware_threads == 0) {
        return 2;
    }
    return static_cast<int>((std::min)(hardware_threads, 4U));
}

} // namespace celia
