#include "audio/SherpaOnnxService.h"

#include "sherpa-onnx/c-api/cxx-api.h"

#include <algorithm>
#include <utility>

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

std::string to_utf8_path(const std::filesystem::path& path) {
    return path.u8string();
}

} // namespace

SherpaOnnxService::~SherpaOnnxService() {
    stop();
}

void SherpaOnnxService::start(const SherpaOnnxModelPaths& model_paths) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) {
        return;
    }

    if (!has_all_model_files(model_paths)) {
        status_ = "error";
        committed_transcript_ = model_paths_error(model_paths);
        partial_transcript_.clear();
        return;
    }

    stop_requested_ = false;
    reset_requested_ = false;
    session_requested_ = false;
    running_ = true;
    status_ = "loading_sherpa_onnx";
    clear_transcript_locked(true);
    clear_pending_audio_locked(true);
    worker_ = std::thread(&SherpaOnnxService::worker_loop, this, model_paths);
}

void SherpaOnnxService::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_requested_ = true;
        cv_.notify_all();
    }

    if (worker_.joinable()) {
        worker_.join();
    }

    std::lock_guard<std::mutex> lock(mutex_);
    stream_.reset();
    recognizer_.reset();
    running_ = false;
    session_requested_ = false;
    reset_requested_ = false;
    clear_pending_audio_locked(true);
    clear_transcript_locked(true);
    if (status_ != "error") {
        status_ = "idle";
    }
}

void SherpaOnnxService::start_session() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_ || status_ == "error") {
        return;
    }

    session_requested_ = true;
    reset_requested_ = false;
    clear_pending_audio_locked(false);
    clear_transcript_locked(true);
    if (status_ != "loading_sherpa_onnx") {
        status_ = "ready";
    }
    cv_.notify_one();
}

void SherpaOnnxService::stop_session() {
    std::lock_guard<std::mutex> lock(mutex_);
    session_requested_ = false;
    reset_requested_ = false;
    clear_pending_audio_locked(true);
    clear_transcript_locked(true);
    if (status_ != "error") {
        status_ = recognizer_ != nullptr ? "ready" : "loading_sherpa_onnx";
    }
    cv_.notify_one();
}

void SherpaOnnxService::reset_stream() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_ || status_ == "error") {
        return;
    }

    clear_pending_audio_locked(true);
    clear_transcript_locked(false);
    reset_requested_ = true;
    if (session_requested_ && status_ != "loading_sherpa_onnx") {
        status_ = "ready";
    }
    cv_.notify_one();
}

void SherpaOnnxService::push_audio(const float* samples, std::size_t sample_count, bool has_speech) {
    if (samples == nullptr || sample_count == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_ || status_ == "error" || !session_requested_) {
        return;
    }

    if (pending_samples_.empty()) {
        pending_samples_.reserve(kMaxQueuedSamples);
    }

    pending_samples_.insert(pending_samples_.end(), samples, samples + sample_count);
    pending_spans_.push_back(PendingSpan{sample_count, has_speech});
    discard_oldest_samples_locked((std::max)(std::size_t{0}, unread_sample_count_locked() > kMaxQueuedSamples
        ? unread_sample_count_locked() - kMaxQueuedSamples
        : std::size_t{0}));

    if (pending_samples_.size() >= kWakeChunkSamples) {
        cv_.notify_one();
    }
}

TranscriptionSnapshot SherpaOnnxService::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return TranscriptionSnapshot{status_, current_transcript_locked()};
}

void SherpaOnnxService::worker_loop(SherpaOnnxModelPaths model_paths) {
    namespace sx = sherpa_onnx::cxx;

    sx::OnlineRecognizerConfig config;
    config.model_config.transducer.encoder = to_utf8_path(model_paths.encoder);
    config.model_config.transducer.decoder = to_utf8_path(model_paths.decoder);
    config.model_config.transducer.joiner = to_utf8_path(model_paths.joiner);
    config.model_config.tokens = to_utf8_path(model_paths.tokens);
    config.model_config.num_threads = 1;
    config.model_config.provider = "cpu";
    config.feat_config.sample_rate = kSampleRate;
    config.feat_config.feature_dim = 80;
    config.decoding_method = "greedy_search";
    config.max_active_paths = 1;
    config.enable_endpoint = true;
    config.rule1_min_trailing_silence = 2.0F;
    config.rule2_min_trailing_silence = 0.8F;
    config.rule3_min_utterance_length = 12.0F;

    auto recognizer = std::make_unique<sx::OnlineRecognizer>(sx::OnlineRecognizer::Create(config));
    if (recognizer->Get() == nullptr) {
        set_error("Khong load duoc sherpa-onnx OnlineRecognizer. Kiem tra duong dan model va DLL.");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        recognizer_ = std::move(recognizer);
        status_ = "ready";
        pending_samples_.reserve(kMaxQueuedSamples);
    }

    std::vector<float> chunk;
    chunk.reserve(kWakeChunkSamples * 2);

    for (;;) {
        std::unique_ptr<sx::OnlineStream> replacement_stream;
        bool reset_stream = false;
        bool stop_session = false;
        bool had_speech = false;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return wait_for_work_locked(); });

            if (stop_requested_) {
                break;
            }

            if (!session_requested_) {
                stop_session = stream_ != nullptr || unread_sample_count_locked() > 0 || reset_requested_;
                reset_requested_ = false;
                clear_pending_audio_locked(true);
                clear_transcript_locked(true);
                if (status_ != "error") {
                    status_ = "ready";
                }
                if (!stop_session) {
                    continue;
                }
            } else if (stream_ == nullptr || reset_requested_) {
                reset_stream = true;
                reset_requested_ = false;
                clear_pending_audio_locked(false);
                partial_transcript_.clear();
            } else if (!pop_pending_audio_locked(chunk, had_speech)) {
                continue;
            }
        }

        if (stop_session) {
            std::lock_guard<std::mutex> lock(mutex_);
            stream_.reset();
            continue;
        }

        if (reset_stream) {
            replacement_stream = std::make_unique<sx::OnlineStream>(recognizer_->CreateStream());
            if (replacement_stream->Get() == nullptr) {
                set_error("Khong tao duoc sherpa-onnx OnlineStream.");
                return;
            }

            std::lock_guard<std::mutex> lock(mutex_);
            if (!session_requested_) {
                continue;
            }
            stream_ = std::move(replacement_stream);
            if (status_ != "error") {
                status_ = "ready";
            }
            continue;
        }

        auto* active_stream = stream_.get();
        active_stream->AcceptWaveform(kSampleRate, chunk.data(), static_cast<int32_t>(chunk.size()));
        while (recognizer_->IsReady(active_stream)) {
            recognizer_->Decode(active_stream);
        }
        const auto result = recognizer_->GetResult(active_stream);
        const bool endpoint = recognizer_->IsEndpoint(active_stream);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!session_requested_ || stream_ == nullptr || status_ == "error") {
                continue;
            }

            const auto text = trim_text(result.text);
            partial_transcript_ = text;
            if (endpoint) {
                if (!text.empty()) {
                    if (!committed_transcript_.empty()) {
                        committed_transcript_ += ' ';
                    }
                    committed_transcript_ += text;
                    if (committed_transcript_.size() > kMaxTranscriptChars) {
                        committed_transcript_.erase(0, committed_transcript_.size() - kMaxTranscriptChars);
                    }
                }
                partial_transcript_.clear();
                reset_requested_ = true;
                status_ = "ready";
            } else {
                status_ = had_speech ? "streaming" : "listening";
            }
        }
    }
}

bool SherpaOnnxService::wait_for_work_locked() const {
    return stop_requested_ ||
        (!session_requested_ && (stream_ != nullptr || unread_sample_count_locked() > 0 || reset_requested_)) ||
        (session_requested_ && (reset_requested_ || stream_ == nullptr || unread_sample_count_locked() >= kWakeChunkSamples));
}

std::size_t SherpaOnnxService::unread_sample_count_locked() const {
    return pending_samples_.size() >= pending_read_offset_ ? pending_samples_.size() - pending_read_offset_ : 0;
}

void SherpaOnnxService::clear_pending_audio_locked(bool release_memory) {
    pending_read_offset_ = 0;
    pending_spans_.clear();
    if (release_memory) {
        std::vector<float>().swap(pending_samples_);
    } else {
        pending_samples_.clear();
    }
}

void SherpaOnnxService::compact_pending_audio_locked(bool release_memory) {
    if (pending_read_offset_ == 0) {
        if (release_memory && pending_samples_.empty()) {
            std::vector<float>().swap(pending_samples_);
        }
        return;
    }

    if (pending_read_offset_ >= pending_samples_.size()) {
        clear_pending_audio_locked(release_memory);
        return;
    }

    pending_samples_.erase(
        pending_samples_.begin(),
        pending_samples_.begin() + static_cast<std::ptrdiff_t>(pending_read_offset_));
    pending_read_offset_ = 0;

    if (release_memory && pending_samples_.empty()) {
        std::vector<float>().swap(pending_samples_);
    }
}

void SherpaOnnxService::discard_oldest_samples_locked(std::size_t sample_count) {
    if (sample_count == 0) {
        return;
    }

    auto remaining = sample_count;
    while (remaining > 0 && !pending_spans_.empty()) {
        auto& span = pending_spans_.front();
        const auto consume = (std::min)(remaining, span.sample_count);
        span.sample_count -= consume;
        pending_read_offset_ += consume;
        remaining -= consume;
        if (span.sample_count == 0) {
            pending_spans_.pop_front();
        }
    }

    if (pending_read_offset_ >= kCompactThresholdSamples || unread_sample_count_locked() == 0) {
        compact_pending_audio_locked(false);
    }
}

bool SherpaOnnxService::pop_pending_audio_locked(std::vector<float>& chunk, bool& had_speech) {
    const auto unread_samples = unread_sample_count_locked();
    if (unread_samples < kWakeChunkSamples) {
        return false;
    }

    const auto sample_count = (std::min)(kWakeChunkSamples, unread_samples);
    chunk.assign(
        pending_samples_.begin() + static_cast<std::ptrdiff_t>(pending_read_offset_),
        pending_samples_.begin() + static_cast<std::ptrdiff_t>(pending_read_offset_ + sample_count));

    had_speech = false;
    auto remaining = sample_count;
    while (remaining > 0 && !pending_spans_.empty()) {
        auto& span = pending_spans_.front();
        had_speech = had_speech || span.has_speech;
        const auto consume = (std::min)(remaining, span.sample_count);
        span.sample_count -= consume;
        pending_read_offset_ += consume;
        remaining -= consume;
        if (span.sample_count == 0) {
            pending_spans_.pop_front();
        }
    }

    if (pending_read_offset_ >= kCompactThresholdSamples || unread_sample_count_locked() == 0) {
        compact_pending_audio_locked(false);
    }

    return true;
}

void SherpaOnnxService::clear_transcript_locked(bool release_memory) {
    if (release_memory) {
        std::string().swap(committed_transcript_);
        std::string().swap(partial_transcript_);
        return;
    }

    committed_transcript_.clear();
    partial_transcript_.clear();
}

void SherpaOnnxService::set_error(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    status_ = "error";
    committed_transcript_ = message;
    partial_transcript_.clear();
    running_ = false;
    session_requested_ = false;
    stream_.reset();
    clear_pending_audio_locked(true);
}

std::string SherpaOnnxService::current_transcript_locked() const {
    if (partial_transcript_.empty()) {
        return committed_transcript_;
    }
    if (committed_transcript_.empty()) {
        return partial_transcript_;
    }
    return committed_transcript_ + " " + partial_transcript_;
}

bool SherpaOnnxService::has_all_model_files(const SherpaOnnxModelPaths& model_paths) {
    return std::filesystem::exists(model_paths.encoder) &&
        std::filesystem::exists(model_paths.decoder) &&
        std::filesystem::exists(model_paths.joiner) &&
        std::filesystem::exists(model_paths.tokens);
}

std::string SherpaOnnxService::model_paths_error(const SherpaOnnxModelPaths& model_paths) {
    std::string message = "Khong tim thay du bo model sherpa-onnx:";
    if (!std::filesystem::exists(model_paths.encoder)) {
        message += "\nencoder: " + model_paths.encoder.u8string();
    }
    if (!std::filesystem::exists(model_paths.decoder)) {
        message += "\ndecoder: " + model_paths.decoder.u8string();
    }
    if (!std::filesystem::exists(model_paths.joiner)) {
        message += "\njoiner: " + model_paths.joiner.u8string();
    }
    if (!std::filesystem::exists(model_paths.tokens)) {
        message += "\ntokens: " + model_paths.tokens.u8string();
    }
    return message;
}

} // namespace celia
