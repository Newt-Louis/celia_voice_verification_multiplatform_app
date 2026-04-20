#include "audio/WhisperService.h"

#include "whisper.h"

#include <algorithm>
#if defined(_WIN32)
#include <malloc.h>
#endif
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
#if defined(_WIN32)
    _heapmin();
#endif
}

void WhisperService::reset_stream() {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_samples_.clear();
    speech_seen_in_queue_ = false;
    abort_current_ = true;
    if (running_ && context_ != nullptr && status_ != "error") {
        status_ = "ready";
    }
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
    auto* context = whisper_init_from_file_with_params_no_state(model_path.u8string().c_str(), context_params);

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

            abort_current_ = false;
            const auto chunk_size = (std::min)(pending_samples_.size(), kMaxChunkSamples);
            chunk.assign(pending_samples_.begin(), pending_samples_.begin() + static_cast<std::ptrdiff_t>(chunk_size));
            const auto consume_size = chunk_size > kOverlapSamples ? chunk_size - kOverlapSamples : chunk_size;
            pending_samples_.erase(pending_samples_.begin(), pending_samples_.begin() + static_cast<std::ptrdiff_t>(consume_size));
            speech_seen_in_queue_ = false;
            status_ = "transcribing " + std::to_string(chunk_size / kSampleRate) + "s";
        }

        auto* state = whisper_init_state(context);
        if (state == nullptr) {
            std::lock_guard<std::mutex> lock(mutex_);
            status_ = "error";
            transcript_ += "\n[Whisper không tạo được runtime state]";
            continue;
        }

        const auto transcript_size_before = snapshot().transcript.size();

        auto params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
        params.strategy = WHISPER_SAMPLING_GREEDY;
        params.n_threads = inference_thread_count();
        params.translate = false;
        params.no_context = true;
        params.no_timestamps = false;
        params.single_segment = false;
        params.print_special = false;
        params.print_progress = false;
        params.print_realtime = false;
        params.print_timestamps = false;
        params.token_timestamps = true;
        params.max_len = 32;
        params.split_on_word = true;
        params.max_tokens = 6;
        params.suppress_blank = true;
        params.suppress_nst = false;
        params.language = "en";
        params.detect_language = false;
        params.initial_prompt = nullptr;
        params.greedy.best_of = 1;
        params.temperature = 0.0F;
        params.temperature_inc = 0.2F;
        params.entropy_thold = 2.40F;
        params.logprob_thold = -1.00F;
        params.no_speech_thold = 0.6F;
        params.new_segment_callback = &WhisperService::on_new_segment;
        params.new_segment_callback_user_data = this;
        params.abort_callback = &WhisperService::should_abort;
        params.abort_callback_user_data = this;

        const int result = whisper_full_with_state(context, state, params, chunk.data(), static_cast<int>(chunk.size()));
        if (result != 0) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (abort_current_) {
                status_ = "ready";
            } else {
                status_ = "error";
                transcript_ += "\n[Whisper xử lý audio thất bại]";
            }
            whisper_free_state(state);
#if defined(_WIN32)
            _heapmin();
#endif
            continue;
        }

        if (snapshot().transcript.size() == transcript_size_before) {
            append_segments_from_state(state, whisper_full_n_segments_from_state(state));
        }

        if (snapshot().transcript.size() == transcript_size_before) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (status_ != "error") {
                status_ = "ready_no_text";
            }
        } else {
            std::lock_guard<std::mutex> lock(mutex_);
            if (status_ != "error") {
                status_ = "ready";
            }
        }

        whisper_free_state(state);
#if defined(_WIN32)
        _heapmin();
#endif
    }
}

void WhisperService::append_transcript(const std::string& text) {
    if (text.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    const auto recent_window = transcript_.size() > 400 ? transcript_.substr(transcript_.size() - 400) : transcript_;
    if (recent_window.find(text) != std::string::npos) {
        return;
    }

    if (!transcript_.empty()) {
        transcript_ += ' ';
    }
    transcript_ += text;

    if (transcript_.size() > kMaxTranscriptChars) {
        transcript_.erase(0, transcript_.size() - kMaxTranscriptChars);
    }
}

void WhisperService::append_segments_from_state(whisper_state* state, int new_segment_count) {
    if (state == nullptr || new_segment_count <= 0) {
        return;
    }

    const int segment_count = whisper_full_n_segments_from_state(state);
    const int first_segment = (std::max)(0, segment_count - new_segment_count);
    for (int segment = first_segment; segment < segment_count; ++segment) {
        const char* segment_text = whisper_full_get_segment_text_from_state(state, segment);
        if (segment_text != nullptr) {
            append_transcript(trim_text(segment_text));
        }
    }
}

void WhisperService::on_new_segment(whisper_context* context, whisper_state* state, int new_segment_count, void* user_data) {
    (void)context;
    auto* service = static_cast<WhisperService*>(user_data);
    if (service != nullptr) {
        service->append_segments_from_state(state, new_segment_count);
    }
}

bool WhisperService::should_abort(void* user_data) {
    auto* service = static_cast<WhisperService*>(user_data);
    if (service == nullptr) {
        return false;
    }

    std::lock_guard<std::mutex> lock(service->mutex_);
    return service->stop_requested_ || service->abort_current_;
}

int WhisperService::inference_thread_count() {
    const auto hardware_threads = std::thread::hardware_concurrency();
    if (hardware_threads == 0) {
        return 2;
    }
    return static_cast<int>((std::min)(hardware_threads, 4U));
}

} // namespace celia
