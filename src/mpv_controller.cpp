#include "podradio/mpv_controller.h"
#include "podradio/utils.h"
#include "podradio/logger.h"
#include "podradio/event_log.h"

namespace podradio
{

MPVController::~MPVController() {
    running_ = false;
    if (mpv_thread_.joinable()) mpv_thread_.join();
    if (ctx_) mpv_terminate_destroy(ctx_);
}

bool MPVController::initialize() {
    // Only switch LC_NUMERIC, protect LC_CTYPE for wide character environment
    // mpv_create() detects locale and prints warnings, but only LC_NUMERIC needs to be "C"
    setlocale(LC_NUMERIC, "C");
    ctx_ = mpv_create();
    setlocale(LC_NUMERIC, "");  // Restore

    if (!ctx_) {
        LOG("[MPV] Failed to create context");
        return false;
    }

    ytdl_path_ = Utils::execute_command("which yt-dlp");
    while (!ytdl_path_.empty() && (ytdl_path_.back() == '\n' || ytdl_path_.back() == '\r')) ytdl_path_.pop_back();
    ytdl_available_ = !ytdl_path_.empty();
    LOG(fmt::format("[MPV] yt-dlp: {}", ytdl_available_ ? ytdl_path_ : "not found"));

    // Default: no window (suitable for audio playback)
    mpv_set_option_string(ctx_, "force-window", "no");
    mpv_set_option_string(ctx_, "vo", "null");
    mpv_set_option_string(ctx_, "keep-open", "yes");
    mpv_set_option_string(ctx_, "idle", "once");

    // Suppress mpv terminal output to prevent screen corruption
    // msg-level: set log level to error, only show severe errors
    // quiet: disable most terminal output
    mpv_set_option_string(ctx_, "msg-level", "all=error");
    mpv_set_option_string(ctx_, "quiet", "yes");

    mpv_set_option_string(ctx_, "ytdl", "yes");
    if (ytdl_available_) {
        mpv_set_option_string(ctx_, "ytdl-path", ytdl_path_.c_str());
    }

    mpv_set_option_string(ctx_, "ytdl-format", "bestvideo+bestaudio/best");
    LOG("[MPV] ytdl-format: bestvideo+bestaudio/best");

    if (mpv_initialize(ctx_) < 0) return false;

    mpv_observe_property(ctx_, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(ctx_, 0, "volume", MPV_FORMAT_INT64);
    mpv_observe_property(ctx_, 0, "media-title", MPV_FORMAT_STRING);
    mpv_observe_property(ctx_, 0, "path", MPV_FORMAT_STRING);
    mpv_observe_property(ctx_, 0, "core-idle", MPV_FORMAT_FLAG);
    mpv_observe_property(ctx_, 0, "audio-codec", MPV_FORMAT_STRING);
    mpv_observe_property(ctx_, 0, "video-codec", MPV_FORMAT_STRING);

    running_ = true;
    mpv_thread_ = std::thread(&MPVController::event_loop, this);
    return true;
}

void MPVController::play_audio(const std::string& url) {
    if (url.empty()) {
        LOG("[MPV] Error: Empty URL");
        return;
    }
    LOG(fmt::format("[MPV] Playing audio: {}", url));

    if (!ctx_) {
        LOG("[MPV] Error: ctx_ is null");
        return;
    }

    // Single file mode: keep-open=yes for pause and progress control
    mpv_set_option_string(ctx_, "keep-open", "yes");
    LOG("[MPV] Single file mode: keep-open=yes");

    mpv_set_option_string(ctx_, "force-window", "no");
    mpv_set_option_string(ctx_, "vo", "null");

    const char* cmd[] = {"loadfile", url.c_str(), "replace", nullptr};
    int result = mpv_command(ctx_, cmd);
    LOG(fmt::format("[MPV] loadfile result: {}", result));

    // Ensure playback starts (not paused)
    int pause_val = 0;
    mpv_set_property(ctx_, "pause", MPV_FORMAT_FLAG, &pause_val);
    LOG("[MPV] Ensured playing (pause=no)");
}

void MPVController::play_video(const std::string& url) {
    if (url.empty()) {
        LOG("[MPV] Error: Empty URL");
        return;
    }
    LOG(fmt::format("[MPV] Playing video: {}", url));

    if (!ctx_) {
        LOG("[MPV] Error: ctx_ is null");
        return;
    }

    // Single file mode: keep-open=yes for pause and progress control
    mpv_set_option_string(ctx_, "keep-open", "yes");
    LOG("[MPV] Single file mode: keep-open=yes");

    if (Utils::has_gui()) {
        mpv_set_option_string(ctx_, "vo", "gpu");
        mpv_set_option_string(ctx_, "force-window", "yes");
        LOG("[MPV] Video mode: opening window");
    } else {
        mpv_set_option_string(ctx_, "vo", "null");
        mpv_set_option_string(ctx_, "force-window", "no");
        LOG("[MPV] TTY mode: no video window");
    }

    const char* cmd[] = {"loadfile", url.c_str(), "replace", nullptr};
    int result = mpv_command(ctx_, cmd);
    LOG(fmt::format("[MPV] loadfile result: {}", result));

    // Ensure playback starts (not paused)
    int pause_val = 0;
    mpv_set_property(ctx_, "pause", MPV_FORMAT_FLAG, &pause_val);
    LOG("[MPV] Ensured playing (pause=no)");
}

void MPVController::play(const std::string& url, bool force_video) {
    URLType type = URLClassifier::classify(url);

    // Improved video detection logic
    // 1. Forced video mode
    // 2. Explicit video file URL (.mp4, .webm, etc)
    // 3. YouTube video URL (watch?v=)
    bool should_play_video = force_video ||
                             type == URLType::VIDEO_FILE ||
                             type == URLType::YOUTUBE_VIDEO;

    // YouTube channel/RSS opens window in GUI environment
    // because YouTube content is usually video
    if (Utils::has_gui() && URLClassifier::is_youtube(type)) {
        should_play_video = true;
    }

    if (should_play_video && Utils::has_gui()) {
        play_video(url);
    } else {
        // Audio default: background playback, no window
        play_audio(url);
    }
}

void MPVController::play_for_autoplay(const std::string& url, bool is_video) {
    if (url.empty()) {
        LOG("[MPV] play_for_autoplay: Empty URL");
        return;
    }

    LOG(fmt::format("[MPV] Autoplay: {}", url));

    // Key setting: keep-open=no allows END_FILE event to trigger after playback ends
    // This enables autoplay chain to continue working
    mpv_set_option_string(ctx_, "keep-open", "no");
    LOG("[MPV] Autoplay mode: keep-open=no for continuous playback");

    if (is_video && Utils::has_gui()) {
        mpv_set_option_string(ctx_, "vo", "gpu");
        mpv_set_option_string(ctx_, "force-window", "yes");
    } else {
        mpv_set_option_string(ctx_, "force-window", "no");
        mpv_set_option_string(ctx_, "vo", "null");
    }

    const char* cmd[] = {"loadfile", url.c_str(), "replace", nullptr};
    int result = mpv_command(ctx_, cmd);
    LOG(fmt::format("[MPV] Autoplay loadfile result: {}", result));

    // Ensure playback starts
    int pause_val = 0;
    mpv_set_property(ctx_, "pause", MPV_FORMAT_FLAG, &pause_val);
    LOG("[MPV] Autoplay: Ensured playing (pause=no)");
}

void MPVController::play_hybrid(const std::string& local_file, const std::string& online_url) {
    LOG(fmt::format("[MPV] Hybrid: {} + {}", local_file, online_url));

    std::string tmp_playlist = fmt::format("/tmp/podradio_hybrid_{}.m3u", getpid());
    std::ofstream f(tmp_playlist);
    if (f.is_open()) {
        f << local_file << "\n";
        f << online_url << "\n";
        f.close();

        if (Utils::has_gui()) {
            mpv_set_option_string(ctx_, "vo", "gpu");
            mpv_set_option_string(ctx_, "force-window", "yes");
        }

        const char* cmd[] = {"loadlist", tmp_playlist.c_str(), "replace", nullptr};
        mpv_command(ctx_, cmd);
    } else {
        play_video(online_url);
    }
}

void MPVController::play_seamless(const std::string& local_file, const std::string& online_url, bool is_video) {
    LOG(fmt::format("[MPV] Seamless: local={}, online={}", local_file, online_url));
    EVENT_LOG(fmt::format("Seamless Play: {} -> online", fs::path(local_file).filename().string()));

    // Enable gapless mode for seamless transition
    mpv_set_option_string(ctx_, "gapless-audio", "yes");
    EVENT_LOG("PRE-FETCHING: gapless-audio enabled");

    // Pre-load next playlist item
    mpv_set_option_string(ctx_, "prefetch-playlist", "yes");
    EVENT_LOG("PRE-FETCHING: prefetch-playlist enabled");

    if (is_video && Utils::has_gui()) {
        mpv_set_option_string(ctx_, "vo", "gpu");
        mpv_set_option_string(ctx_, "force-window", "yes");
    } else {
        mpv_set_option_string(ctx_, "force-window", "no");
        mpv_set_option_string(ctx_, "vo", "null");
    }

    // Create playlist: local cache -> online stream
    std::string tmp_playlist = fmt::format("/tmp/podradio_seamless_{}.m3u", getpid());
    std::ofstream f(tmp_playlist);
    if (f.is_open()) {
        f << local_file << "\n";
        f << online_url << "\n";
        f.close();

        LOG(fmt::format("[MPV] Seamless playlist created: {}", tmp_playlist));
        EVENT_LOG("Seamless: Playlist [local -> online]");

        const char* cmd[] = {"loadlist", tmp_playlist.c_str(), "replace", nullptr};
        int result = mpv_command(ctx_, cmd);
        LOG(fmt::format("[MPV] Seamless loadlist result: {}", result));

        if (result >= 0) {
            EVENT_LOG("PRE-FETCHING: Online stream queued");
        }
    } else {
        // Fallback to direct online stream
        LOG("[MPV] Seamless: Failed to create playlist, falling back to online");
        EVENT_LOG("Seamless: Fallback to online");
        play(online_url, is_video);
    }
}

void MPVController::play_smart(const std::string& url, const std::string& local_file, bool is_video) {
    // If local complete file exists, play it directly
    if (!local_file.empty() && fs::exists(local_file)) {
        if (Utils::is_partial_download(local_file)) {
            // Partial download: seamless switch to online stream
            LOG(fmt::format("[MPV] Smart: Partial cache, seamless play: {}", local_file));
            EVENT_LOG(fmt::format("Cache Play: Partial -> Seamless ({})", fs::path(local_file).filename().string()));
            play_seamless(local_file, url, is_video);
        } else {
            // Complete download: play local file directly
            LOG(fmt::format("[MPV] Smart: Full cache, local play: {}", local_file));
            EVENT_LOG(fmt::format("Cache Play: Local ({})", fs::path(local_file).filename().string()));
            play("file://" + local_file, is_video);
        }
    } else {
        // No local cache: play online stream directly
        LOG(fmt::format("[MPV] Smart: No cache, online play: {}", url));
        EVENT_LOG("Play: Online (no cache)");
        play(url, is_video);
    }
}

void MPVController::play_list(const std::vector<std::string>& urls, bool is_video) {
    if (urls.empty()) return;

    // Playlist mode: keep-open=no allows auto-play next item
    mpv_set_option_string(ctx_, "keep-open", "no");
    LOG("[MPV] Playlist mode: keep-open=no for auto-play next");

    if (is_video && Utils::has_gui()) {
        mpv_set_option_string(ctx_, "vo", "gpu");
        mpv_set_option_string(ctx_, "force-window", "yes");
    } else {
        mpv_set_option_string(ctx_, "force-window", "no");
        mpv_set_option_string(ctx_, "vo", "null");
    }

    std::string tmp = fmt::format("/tmp/podradio_{}.m3u", getpid());
    std::ofstream f(tmp);
    if (f.is_open()) {
        for (const auto& url : urls) f << url << "\n";
        f.close();
        const char* cmd[] = {"loadlist", tmp.c_str(), "replace", nullptr};
        mpv_command(ctx_, cmd);

        // Ensure playback starts (not paused)
        int pause_val = 0;
        mpv_set_property(ctx_, "pause", MPV_FORMAT_FLAG, &pause_val);
        LOG("[MPV] play_list: Ensured playing (pause=no)");
    } else play(urls[0], is_video);
}

void MPVController::play_list_from(const std::vector<std::string>& urls, int start_idx, bool is_video) {
    if (urls.empty()) return;
    if (start_idx < 0) start_idx = 0;
    if (start_idx >= (int)urls.size()) start_idx = urls.size() - 1;

    // Playlist mode settings
    mpv_set_option_string(ctx_, "keep-open", "no");
    LOG(fmt::format("[MPV] Playlist mode: keep-open=no, start from idx {}", start_idx));

    if (is_video && Utils::has_gui()) {
        mpv_set_option_string(ctx_, "vo", "gpu");
        mpv_set_option_string(ctx_, "force-window", "yes");
    } else {
        mpv_set_option_string(ctx_, "force-window", "no");
        mpv_set_option_string(ctx_, "vo", "null");
    }

    std::string tmp = fmt::format("/tmp/podradio_{}.m3u", getpid());
    std::ofstream f(tmp);
    if (f.is_open()) {
        for (const auto& url : urls) f << url << "\n";
        f.close();

        // Load playlist
        const char* cmd[] = {"loadlist", tmp.c_str(), "replace", nullptr};
        mpv_command(ctx_, cmd);

        // Set playback position to specified start index
        int64_t pos = start_idx;
        mpv_set_property(ctx_, "playlist-pos", MPV_FORMAT_INT64, &pos);
        LOG(fmt::format("[MPV] play_list_from: Set playlist-pos to {}", start_idx));

        // Ensure playback starts (not paused)
        int pause_val = 0;
        mpv_set_property(ctx_, "pause", MPV_FORMAT_FLAG, &pause_val);
        LOG("[MPV] play_list_from: Ensured playing (pause=no)");
    } else play(urls[start_idx], is_video);
}

void MPVController::toggle_pause() {
    int p = 0;
    mpv_get_property(ctx_, "pause", MPV_FORMAT_FLAG, &p);
    p = !p;
    mpv_set_property(ctx_, "pause", MPV_FORMAT_FLAG, &p);
}

void MPVController::pause() {
    int p = 1;
    mpv_set_property(ctx_, "pause", MPV_FORMAT_FLAG, &p);
}

void MPVController::stop() {
    const char* cmd[] = {"stop", nullptr};
    mpv_command(ctx_, cmd);
}

void MPVController::set_volume(int vol) {
    if (vol < 0) vol = 0;
    if (vol > MAX_VOLUME) vol = MAX_VOLUME;
    int64_t v = vol;
    mpv_set_property(ctx_, "volume", MPV_FORMAT_INT64, &v);
    state_.volume = vol;
}

int MPVController::get_volume() const {
    return state_.volume;
}

void MPVController::volume_up() {
    State s = const_cast<MPVController*>(this)->get_state();
    set_volume(s.volume + VOLUME_STEP);
}

void MPVController::volume_down() {
    State s = const_cast<MPVController*>(this)->get_state();
    set_volume(s.volume - VOLUME_STEP);
}

double MPVController::get_speed() const {
    return state_.speed;
}

void MPVController::set_speed(double s) {
    if (s < MIN_SPEED) s = MIN_SPEED;
    if (s > MAX_SPEED) s = MAX_SPEED;
    mpv_set_property(ctx_, "speed", MPV_FORMAT_DOUBLE, &s);
    state_.speed = s;
}

void MPVController::speed_up() {
    adjust_speed(true);
}

void MPVController::speed_down() {
    adjust_speed(false);
}

void MPVController::reset_speed() {
    double s = DEFAULT_SPEED;
    mpv_set_property(ctx_, "speed", MPV_FORMAT_DOUBLE, &s);
}

void MPVController::adjust_speed(bool faster) {
    double s = DEFAULT_SPEED;
    mpv_get_property(ctx_, "speed", MPV_FORMAT_DOUBLE, &s);
    s = faster ? s * (1.0 + SPEED_STEP) : s / (1.0 + SPEED_STEP);
    if (s < MIN_SPEED) s = MIN_SPEED;
    if (s > MAX_SPEED) s = MAX_SPEED;
    mpv_set_property(ctx_, "speed", MPV_FORMAT_DOUBLE, &s);
}

void MPVController::seek_absolute(double seconds) {
    std::string cmd = fmt::format("seek {} absolute", seconds);
    const char* argv[] = {"seek", fmt::format("{}", seconds).c_str(), "absolute", nullptr};
    // Use command API for seeking
    mpv_command(ctx_, argv);
}

void MPVController::seek_relative(double seconds) {
    const char* argv[] = {"seek", fmt::format("{}", seconds).c_str(), nullptr};
    mpv_command(ctx_, argv);
}

double MPVController::get_position() const {
    return state_.time_pos;
}

double MPVController::get_duration() const {
    return state_.media_duration;
}

double MPVController::get_time_remaining() const {
    double remaining = state_.media_duration - state_.time_pos;
    return remaining > 0 ? remaining : 0;
}

bool MPVController::is_playing() const {
    return state_.has_media && !state_.paused;
}

bool MPVController::is_paused() const {
    return state_.paused;
}

bool MPVController::is_stopped() const {
    return !state_.has_media;
}

std::string MPVController::get_media_title() const {
    return state_.title;
}

std::string MPVController::get_current_url() const {
    return state_.current_url;
}

void MPVController::set_loop_file(bool loop) {
    const char* val = loop ? "inf" : "no";
    mpv_set_option_string(ctx_, "loop-file", val);
    LOG(fmt::format("[MPV] loop-file set to: {}", val));
}

void MPVController::set_loop_playlist(bool loop) {
    const char* val = loop ? "inf" : "no";
    mpv_set_option_string(ctx_, "loop-playlist", val);
    LOG(fmt::format("[MPV] loop-playlist set to: {}", val));
}

MPVController::State MPVController::get_state() {
    std::lock_guard<std::mutex> lock(mtx_);
    return state_;
}

bool MPVController::is_ytdl_available() const {
    return ytdl_available_;
}

mpv_handle* MPVController::get_handle() {
    return ctx_;
}

void MPVController::set_end_file_callback(EndFileCallback callback) {
    end_file_callback_ = callback;
}

void MPVController::event_loop() {
    while (running_) {
        mpv_event* event = mpv_wait_event(ctx_, 0.05);
        if (event->event_id == MPV_EVENT_SHUTDOWN) break;

        if (event->event_id == MPV_EVENT_FILE_LOADED) {
            LOG("[MPV] File loaded");
            EVENT_LOG("MPV: File loaded");
        } else if (event->event_id == MPV_EVENT_END_FILE) {
            mpv_event_end_file* ef = (mpv_event_end_file*)event->data;
            int reason = static_cast<int>(ef->reason);
            LOG(fmt::format("[MPV] End file, reason: {}", reason));
            // MPV end-file reason details:
            // 0 = EOF (normal end, possibly gapless switch to next track)
            // 1 = stop (stopped)
            // 2 = quit (exited)
            // 3 = error (real error)
            // 4 = redirect
            // 5 = interrupted by new file load (normal, e.g. quick switch)
            if (reason == 0) {
                EVENT_LOG("MPV: Track ended (gapless next?)");
            } else if (reason == 3) {
                EVENT_LOG("MPV: Playback error!");
            } else if (reason == 5) {
                EVENT_LOG("MPV: Interrupted by new file");
            }

            // Call end-of-file callback
            // reason=0 means normal end, triggers autoplay next logic
            if (end_file_callback_) {
                end_file_callback_(reason);
            }
        } else if (event->event_id == MPV_EVENT_SEEK) {
            LOG("[MPV] Seek event");
        } else if (event->event_id == MPV_EVENT_PLAYBACK_RESTART) {
            LOG("[MPV] Playback restart");
            EVENT_LOG("MPV: Playback restart");
        }

        update_state();
    }
}

void MPVController::update_state() {
    if (!ctx_) return;

    int p = 0;
    mpv_get_property(ctx_, "pause", MPV_FORMAT_FLAG, &p);

    char* path = nullptr;
    int has_path = mpv_get_property(ctx_, "path", MPV_FORMAT_STRING, &path);

    int64_t v = 100;
    mpv_get_property(ctx_, "volume", MPV_FORMAT_INT64, &v);

    double sp = 1.0;
    mpv_get_property(ctx_, "speed", MPV_FORMAT_DOUBLE, &sp);

    char* t = nullptr;
    mpv_get_property(ctx_, "media-title", MPV_FORMAT_STRING, &t);

    int idle = 1;
    mpv_get_property(ctx_, "core-idle", MPV_FORMAT_FLAG, &idle);

    char* codec = nullptr;
    mpv_get_property(ctx_, "audio-codec", MPV_FORMAT_STRING, &codec);

    char* vcodec = nullptr;
    mpv_get_property(ctx_, "video-codec", MPV_FORMAT_STRING, &vcodec);

    // Get playback position and duration
    double time_pos = 0.0;
    mpv_get_property(ctx_, "time-pos", MPV_FORMAT_DOUBLE, &time_pos);

    double duration = 0.0;
    mpv_get_property(ctx_, "duration", MPV_FORMAT_DOUBLE, &duration);

    // Get playlist position and count
    int64_t pl_pos = -1;
    mpv_get_property(ctx_, "playlist-pos", MPV_FORMAT_INT64, &pl_pos);

    int64_t pl_count = 0;
    mpv_get_property(ctx_, "playlist-count", MPV_FORMAT_INT64, &pl_count);

    {
        std::lock_guard<std::mutex> lock(mtx_);
        state_.paused = p;
        // Key fix: use has_path >= 0 && path to check
        state_.has_media = (has_path >= 0 && path);
        state_.volume = (int)v;
        state_.speed = sp;
        state_.time_pos = time_pos;
        state_.media_duration = duration;
        state_.playlist_pos = (int)pl_pos;
        state_.playlist_count = (int)pl_count;
        if (t) state_.title = t;
        if (path) state_.current_url = path;
        state_.core_idle = idle;
        if (codec) state_.audio_codec = codec;
        if (vcodec) state_.video_codec = vcodec;
        // Runtime detection: if video-codec property exists and non-empty, video stream present
        state_.has_video = (vcodec != nullptr && strlen(vcodec) > 0);
    }

    if (path) mpv_free(path);
    if (t) mpv_free(t);
    if (codec) mpv_free(codec);
    if (vcodec) mpv_free(vcodec);
}

} // namespace podradio
