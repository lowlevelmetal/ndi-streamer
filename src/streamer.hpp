/**
 * @file streamer.hpp
 * @brief The streaming pipeline: input -> decode -> process -> paced NDI output.
 */

#pragma once

#include "av/audio_processor.hpp"
#include "av/decoder.hpp"
#include "av/input.hpp"
#include "av/video_processor.hpp"
#include "frames.hpp"
#include "ndi/sender.hpp"
#include "options.hpp"
#include "util/clock.hpp"
#include "util/queue.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>
#include <vector>

namespace ndistreamer {

struct StreamerStats {
    uint64_t video_sent = 0;
    uint64_t video_dropped = 0;
    uint64_t video_skipped = 0; ///< Packets not decoded because video could not keep up.
    uint64_t audio_sent = 0;
    uint64_t loops = 0;
    uint64_t resyncs = 0;
    size_t video_queued = 0;
    size_t audio_queued = 0;
    int connections = 0;
};

/**
 * @brief Owns every pipeline stage and the threads that run them.
 *
 *   demux --> video packets --> video decode+process --> video frames --> video presenter --+
 *         \-> audio packets --> audio decode+process --> audio frames --> audio presenter --+--> NDI
 *
 * Stages are connected by bounded queues, so a slow consumer applies back-pressure upstream.
 * Presenters release frames at their media timestamps using a shared PresentationClock. Audio is the
 * master clock while it is being sent. Late video frames are dropped when newer ones are ready. If video
 * cannot keep up, late frames skip filtering and conversion, and if decoding itself is too slow the
 * decoder skips ahead to keyframes, so audio is never starved.
 */
class Streamer {
public:
    /**
     * Opens the input, the decoders and the NDI source. Throws on failure.
     * @param stop requesting a stop ends streaming (and interrupts blocking input I/O)
     */
    Streamer(const Options &options, std::stop_source stop);
    ~Streamer();

    Streamer(const Streamer &) = delete;
    Streamer &operator=(const Streamer &) = delete;

    void Start();

    /**
     * @brief Wait up to @p timeout for every stage to finish.
     * @return true once streaming has ended (input exhausted, stopped, or failed).
     */
    bool WaitFor(std::chrono::milliseconds timeout);

    /**
     * @brief Join all threads; rethrows the first error raised by any stage.
     */
    void Wait();

    StreamerStats Stats() const;
    void LogStats() const;

    std::string SourceName() const { return m_sender->SourceName(); }

private:
    struct PacketItem {
        av::PacketPtr packet; ///< nullptr marks a discontinuity (the input looped)
    };

    void Spawn(const char *name, void (Streamer::*body)(std::stop_token));
    void Fail(std::exception_ptr error);

    void DemuxLoop(std::stop_token stop);
    void VideoDecodeLoop(std::stop_token stop);
    void AudioDecodeLoop(std::stop_token stop);
    void VideoPresentLoop(std::stop_token stop);
    void AudioPresentLoop(std::stop_token stop);

    template <typename OnFrame, typename ShouldSkip>
    void Decode(av::Decoder &decoder, BoundedQueue<PacketItem> &packets, std::stop_token stop, OnFrame &&on_frame,
                ShouldSkip &&should_skip);

    /**
     * @brief Sleep until @p pts_us is due, following clock re-anchoring while waiting.
     * The master stream re-anchors the clock when it is far off schedule.
     * @return false if stopped while waiting
     */
    bool WaitUntilDue(int64_t pts_us, bool is_audio, std::stop_token stop);

    bool AudioIsMaster() const;

    Options m_options;
    std::stop_source m_stop;

    std::shared_ptr<ndi::Runtime> m_runtime;
    std::unique_ptr<av::MediaInput> m_input;
    std::unique_ptr<av::Decoder> m_video_decoder;
    std::unique_ptr<av::Decoder> m_audio_decoder;
    std::unique_ptr<av::VideoProcessor> m_video_processor;
    std::unique_ptr<av::AudioProcessor> m_audio_processor;
    std::unique_ptr<ndi::Sender> m_sender;
    std::unique_ptr<PresentationClock> m_clock;

    BoundedQueue<PacketItem> m_video_packets;
    BoundedQueue<PacketItem> m_audio_packets;
    BoundedQueue<VideoFrame> m_video_frames;
    BoundedQueue<AudioFrame> m_audio_frames;

    std::vector<std::thread> m_threads;
    std::mutex m_state_mutex;
    std::condition_variable m_state_cv;
    int m_running = 0;
    std::exception_ptr m_error;

    bool m_live = false;
    std::atomic<bool> m_audio_presenting{false};
    std::atomic<int64_t> m_audio_last_sent{0};      ///< steady_clock ticks; 0 before the first block
    std::atomic<uint64_t> m_video_sent{0};
    std::atomic<uint64_t> m_video_dropped{0};
    std::atomic<uint64_t> m_video_skipped{0};
    std::atomic<uint64_t> m_audio_sent{0};
    std::atomic<uint64_t> m_loops{0};
    std::atomic<uint64_t> m_resyncs{0};
};

} // namespace ndistreamer
