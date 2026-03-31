#pragma once

#include "common.h"
#include "podradio/url_classifier.h"

#if PODRADIO_HAS_MPV
    // libmpv is available - use direct API
#else
    // No libmpv at compile time - use external mpv process
    #ifndef _MSC_VER
        #warning "Compiling without libmpv - will use external mpv process (configure mpv_path in config.ini)"
    #endif
#endif

namespace podradio {

// =========================================================
// MPV Controller - Media playback controller using libmpv
// Manages audio/video playback, volume, speed, seeking, playlists.
// =========================================================
class MPVController {
public:
    // End-of-file callback type
    // reason: 0=EOF (normal end), 1=stop, 2=quit, 3=error, 4=redirect, 5=interrupted
    using EndFileCallback = std::function<void(int reason)>;

    // Playback state snapshot
    struct State {
        bool paused = true;
        bool has_media = false;
        bool core_idle = true;
        int volume = MAX_VOLUME;
        double speed = DEFAULT_SPEED;
        double time_pos = 0.0;         // Current playback position (seconds)
        double media_duration = 0.0;   // Media total duration (seconds)
        std::string title;
        std::string current_url;
        std::string audio_codec;
        std::string video_codec;
        bool has_video = false;        // Whether video track is present (runtime detection)
        int playlist_pos = -1;         // Current playlist position (0-based)
        int playlist_count = 0;        // Playlist total count
    };

    ~MPVController();

    // Initialize mpv context and start event loop thread
    bool initialize();

    // Play audio (video output disabled)
    void play_audio(const std::string& url);

    // Play video (video output enabled in GUI mode)
    void play_video(const std::string& url);

    // Smart play - auto-detect and use best playback method
    void play(const std::string& url, bool force_video = false);

    // Autoplay for playlist auto-advance (keep-open=no for continuous playback)
    void play_for_autoplay(const std::string& url, bool is_video = false);

    // Hybrid play - local file then online URL
    void play_hybrid(const std::string& local_file, const std::string& online_url);

    // Seamless play - local cache then online stream with gapless transition
    void play_seamless(const std::string& local_file, const std::string& online_url,
                       bool is_video = false);

    // Smart play - auto-detect local cache and choose best method
    void play_smart(const std::string& url, const std::string& local_file, bool is_video = false);

    // Play a list of URLs
    void play_list(const std::vector<std::string>& urls, bool is_video = false);

    // Play list starting from a specific index
    void play_list_from(const std::vector<std::string>& urls, int start_idx, bool is_video = false);

    // Pause playback
    void pause();

    // Toggle pause state
    void toggle_pause();

    // Stop playback
    void stop();

    // Get current volume
    int get_volume() const;

    // Set volume (0-100)
    void set_volume(int vol);

    // Increase volume by VOLUME_STEP
    void volume_up();

    // Decrease volume by VOLUME_STEP
    void volume_down();

    // Get current playback speed
    double get_speed() const;

    // Set playback speed
    void set_speed(double s);

    // Increase speed by SPEED_STEP
    void speed_up();

    // Decrease speed by SPEED_STEP
    void speed_down();

    // Reset speed to DEFAULT_SPEED
    void reset_speed();

    // Adjust speed (true=faster, false=slower)
    void adjust_speed(bool faster);

    // Seek to absolute position (seconds)
    void seek_absolute(double seconds);

    // Seek relative to current position (seconds)
    void seek_relative(double seconds);

    // Get current playback position (seconds)
    double get_position() const;

    // Get media duration (seconds)
    double get_duration() const;

    // Get remaining time (seconds)
    double get_time_remaining() const;

    // Check if currently playing
    bool is_playing() const;

    // Check if currently paused
    bool is_paused() const;

    // Check if stopped/idle
    bool is_stopped() const;

    // Get media title
    std::string get_media_title() const;

    // Get current playing URL
    std::string get_current_url() const;

    // Set loop mode for single file (R mode)
    void set_loop_file(bool loop);

    // Set loop mode for playlist (C mode)
    void set_loop_playlist(bool loop);

    // Get full state snapshot
    State get_state();

    // Check if yt-dlp is available
    bool is_ytdl_available() const;

    // Get raw mpv handle (advanced use)
#if PODRADIO_HAS_MPV
    mpv_handle* get_handle();
#else
    void* get_handle();  // Returns nullptr in external mode
#endif

    // Set end-of-file callback
    void set_end_file_callback(EndFileCallback callback);

private:
#if PODRADIO_HAS_MPV
    mpv_handle* ctx_ = nullptr;
#endif
    std::thread mpv_thread_;
    std::atomic<bool> running_{false};
    std::mutex mtx_;
    State state_;
    bool ytdl_available_ = false;
    std::string ytdl_path_;
    EndFileCallback end_file_callback_;

    // External mpv process mode (used when libmpv not available at compile time)
    bool use_external_mpv_ = false;
    std::string external_mpv_path_;
#ifdef _WIN32
    HANDLE external_mpv_process_ = INVALID_HANDLE_VALUE;
    HANDLE external_mpv_stdin_ = INVALID_HANDLE_VALUE;
    HANDLE external_mpv_stdout_ = INVALID_HANDLE_VALUE;
#else
    pid_t external_mpv_pid_ = -1;
    int external_mpv_stdin_fd_ = -1;
    int external_mpv_stdout_fd_ = -1;
#endif

    // Event loop thread function
    void event_loop();

    // Update internal state from mpv properties
    void update_state();

    // Launch external mpv process (used in external mode)
    void launch_process(const std::string& cmd);
};

} // namespace podradio
