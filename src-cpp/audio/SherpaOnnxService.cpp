#include "audio/SherpaOnnxService.h"

#include "sherpa-onnx/c-api/cxx-api.h"

#include <algorithm>

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
    speech_seen_in_queue_ = false;
    running_ = true;
    status_ = "loading_sherpa_onnx";
    committed_transcript_.clear();
    partial_transcript_.clear();
    pending_samples_.clear();
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
    pending_samples_.clear();
    partial_transcript_.clear();
    speech_seen_in_queue_ = false;
    reset_requested_ = false;
    if (status_ != "error") {
        status_ = "idle";
    }
}

void SherpaOnnxService::reset_stream() {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_samples_.clear();
    speech_seen_in_queue_ = false;
    partial_transcript_.clear();
    reset_requested_ = true;
    if (running_ && status_ != "error") {
        status_ = "ready";
    }
    cv_.notify_one();
}

void SherpaOnnxService::push_audio(const float* samples, std::size_t sample_count, bool has_speech) {
    if (samples == nullptr || sample_count == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_ || stream_ == nullptr || recognizer_ == nullptr || status_ == "error") {
        return;
    }

    pending_samples_.insert(pending_samples_.end(), samples, samples + sample_count);
    speech_seen_in_queue_ = speech_seen_in_queue_ || has_speech;
    if (pending_samples_.size() > kMaxQueuedSamples) {
        pending_samples_.erase(
            pending_samples_.begin(),
            pending_samples_.begin() + static_cast<std::ptrdiff_t>(pending_samples_.size() - kMaxQueuedSamples));
    }

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

    auto stream = std::make_unique<sx::OnlineStream>(recognizer->CreateStream());
    if (stream->Get() == nullptr) {
        set_error("Khong tao duoc sherpa-onnx OnlineStream.");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        recognizer_ = std::move(recognizer);
        stream_ = std::move(stream);
        status_ = "ready";
    }

    for (;;) {
        std::vector<float> chunk;
        bool reset_stream = false;
        bool had_speech = false;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] {
                return stop_requested_ || reset_requested_ || pending_samples_.size() >= kWakeChunkSamples;
            });

            if (stop_requested_) {
                break;
            }

            reset_stream = reset_requested_;
            reset_requested_ = false;
            if (!pending_samples_.empty()) {
                chunk.swap(pending_samples_);
            }
            had_speech = speech_seen_in_queue_;
            speech_seen_in_queue_ = false;
        }

        if (reset_stream) {
            recognizer_->Reset(stream_.get());
            continue;
        }

        if (chunk.empty()) {
            continue;
        }

        stream_->AcceptWaveform(kSampleRate, chunk.data(), static_cast<int32_t>(chunk.size()));
        while (recognizer_->IsReady(stream_.get())) {
            recognizer_->Decode(stream_.get());
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (status_ != "error") {
                status_ = had_speech ? "streaming" : "listening";
            }
        }
        update_stream_result();
    }
}

void SherpaOnnxService::update_stream_result() {
    const auto result = recognizer_->GetResult(stream_.get());
    const auto text = trim_text(result.text);
    const bool endpoint = recognizer_->IsEndpoint(stream_.get());

    std::lock_guard<std::mutex> lock(mutex_);
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
        recognizer_->Reset(stream_.get());
        if (status_ != "error") {
            status_ = "ready";
        }
    }
}

void SherpaOnnxService::set_error(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    status_ = "error";
    committed_transcript_ = message;
    partial_transcript_.clear();
    running_ = false;
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
