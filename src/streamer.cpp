/**
 * @file streamer.cpp
 * @brief The streaming pipeline: input -> decode -> process -> paced NDI output.
 */

#include "streamer.hpp"
#include "util/log.hpp"

#include <algorithm>

#include <pthread.h>

namespace ndistreamer {

namespace {

using Clock = PresentationClock::Clock;
using namespace std::chrono_literals;

// Queue depths. Packet queues absorb uneven interleaving in the input; frame queues let decoding run
// slightly ahead of presentation to ride out decoder jitter.
constexpr size_t kVideoPacketQueue = 120;
constexpr size_t kAudioPacketQueue = 256;
constexpr size_t kVideoFrameQueue = 8;
constexpr size_t kAudioFrameQueue = 64;

// The master stream re-anchors the clock when it falls this far behind schedule, or (for live inputs,
// where timestamps can jump) when it runs this far ahead.
constexpr auto kResyncLate = 1s;
constexpr auto kResyncEarly = 5s;

// Audio is the master clock while it is actually being sent; video takes over when it goes quiet.
constexpr auto kAudioMasterIdle = 1s;

// A late video frame is dropped when a newer one is already waiting, but the picture never freezes
// for longer than kMaxFreeze.
constexpr auto kMinDropThreshold = 40ms;
constexpr auto kMaxFreeze = 200ms;

// When decoded video has been late for this long, the decoder skips ahead to keyframes. That sheds
// decoding work and keeps the input flowing, so audio is never starved.
constexpr auto kVideoBehindGrace = 1s;
constexpr auto kMaxDecodeFreeze = 1s;

// Waiting is done in short steps so a re-anchored clock takes effect promptly.
constexpr auto kMaxSleepStep = 50ms;

double Seconds(Clock::duration d) {
    return std::chrono::duration<double>(d).count();
}

int64_t Ticks(Clock::time_point t) {
    return t.time_since_epoch().count();
}

} // namespace

Streamer::Streamer(const Options &options, std::stop_source stop)
    : m_options(options), m_stop(std::move(stop)), m_video_packets(kVideoPacketQueue),
      m_audio_packets(kAudioPacketQueue), m_video_frames(kVideoFrameQueue), m_audio_frames(kAudioFrameQueue) {
    // Load NDI first: it is quick and fails fast, while opening a network input may take a while.
    m_runtime = ndi::Runtime::Load(options.ndi_library);
    log::Info("NDI runtime: {} ({})", m_runtime->Version(), m_runtime->Path());

    m_input = std::make_unique<av::MediaInput>(options.input, m_stop.get_token());
    m_input->LogInfo();

    if (AVStream *stream = m_input->VideoStream()) {
        m_video_decoder = std::make_unique<av::Decoder>(stream, options.decoder);

        av::VideoProcessorConfig config;
        config.filter = options.video_filter;
        config.pixel_format = options.pixel_format;
        config.time_base = stream->time_base;
        config.frame_rate = m_input->VideoFrameRate();
        config.hw_device = m_video_decoder->HardwareDevice();
        m_video_processor = std::make_unique<av::VideoProcessor>(std::move(config));

        std::string hw = m_video_decoder->HardwareName();
        log::Info("video decoder: {} ({})", m_video_decoder->CodecName(), hw.empty() ? "software" : hw);
    }

    if (AVStream *stream = m_input->AudioStream()) {
        m_audio_decoder = std::make_unique<av::Decoder>(stream, av::DecoderConfig{});

        av::AudioProcessorConfig config;
        config.sample_rate = options.audio_rate;
        config.channels = options.audio_channels;
        config.time_base = stream->time_base;
        m_audio_processor = std::make_unique<av::AudioProcessor>(config);

        log::Info("audio decoder: {}", m_audio_decoder->CodecName());
    }

    m_sender = std::make_unique<ndi::Sender>(m_runtime, options.sender);

    int streams = (m_video_decoder ? 1 : 0) + (m_audio_decoder ? 1 : 0);
    m_clock = std::make_unique<PresentationClock>(streams);
    m_live = m_input->IsLive();
}

Streamer::~Streamer() {
    m_stop.request_stop();
    for (auto &thread : m_threads) {
        if (thread.joinable()) thread.join();
    }
}

void Streamer::Start() {
    if (m_video_decoder) {
        Spawn("nds-video-dec", &Streamer::VideoDecodeLoop);
        Spawn("nds-video-out", &Streamer::VideoPresentLoop);
    }
    if (m_audio_decoder) {
        m_audio_presenting = true;
        Spawn("nds-audio-dec", &Streamer::AudioDecodeLoop);
        Spawn("nds-audio-out", &Streamer::AudioPresentLoop);
    }
    Spawn("nds-demux", &Streamer::DemuxLoop);
}

void Streamer::Spawn(const char *name, void (Streamer::*body)(std::stop_token)) {
    {
        std::lock_guard lock(m_state_mutex);
        m_running++;
    }

    m_threads.emplace_back([this, name, body] {
        pthread_setname_np(pthread_self(), name);
        try {
            (this->*body)(m_stop.get_token());
        } catch (...) {
            Fail(std::current_exception());
        }

        std::lock_guard lock(m_state_mutex);
        m_running--;
        m_state_cv.notify_all();
    });
}

void Streamer::Fail(std::exception_ptr error) {
    {
        std::lock_guard lock(m_state_mutex);
        if (!m_error) m_error = error;
    }
    m_stop.request_stop();
}

bool Streamer::WaitFor(std::chrono::milliseconds timeout) {
    std::unique_lock lock(m_state_mutex);
    return m_state_cv.wait_for(lock, timeout, [this] { return m_running == 0; });
}

void Streamer::Wait() {
    for (auto &thread : m_threads) {
        if (thread.joinable()) thread.join();
    }

    std::lock_guard lock(m_state_mutex);
    if (m_error) std::rethrow_exception(m_error);
}

void Streamer::DemuxLoop(std::stop_token stop) {
    const int video_index = m_input->VideoStream() ? m_input->VideoStream()->index : -1;

    while (!stop.stop_requested()) {
        av::PacketPtr packet = m_input->Read();
        if (!packet) {
            if (stop.stop_requested() || !m_options.loop || !m_input->Restart()) break;

            m_loops++;
            log::Debug("looping input (pass {})", m_loops.load() + 1);
            // Decoders drain the previous pass before the restarted bitstream arrives.
            if (m_video_decoder && !m_video_packets.Push({nullptr}, stop)) break;
            if (m_audio_decoder && !m_audio_packets.Push({nullptr}, stop)) break;
            continue;
        }

        auto &queue = packet->stream_index == video_index ? m_video_packets : m_audio_packets;
        if (!queue.Push({std::move(packet)}, stop)) break;
    }

    m_video_packets.Close();
    m_audio_packets.Close();
    log::Debug("input finished");
}

template <typename OnFrame, typename ShouldSkip>
void Streamer::Decode(av::Decoder &decoder, BoundedQueue<PacketItem> &packets, std::stop_token stop,
                      OnFrame &&on_frame, ShouldSkip &&should_skip) {
    auto drain = [&] {
        while (av::FramePtr frame = decoder.Receive()) {
            on_frame(std::move(frame));
            if (stop.stop_requested()) return false;
        }
        return true;
    };

    bool skipping = false;
    while (auto item = packets.Pop(stop)) {
        if (!item->packet) {
            // The input looped: flush out the end of the previous pass, then reset for the new one.
            decoder.Send(nullptr);
            if (!drain()) return;
            decoder.Reset();
            continue;
        }

        // Discarding packets up to the next keyframe sheds work when decoding can't keep up.
        if (!skipping && should_skip()) skipping = true;
        if (skipping) {
            if (!(item->packet->flags & AV_PKT_FLAG_KEY)) {
                m_video_skipped++;
                continue;
            }
            skipping = false;
        }

        // A full decoder must hand out frames before it takes more input.
        int attempts = 0;
        while (!decoder.Send(item->packet.get())) {
            if (!drain()) return;
            if (++attempts > 8) {
                log::Warn("{} decoder is not accepting input; skipping a packet", decoder.CodecName());
                break;
            }
        }
        if (!drain()) return;
    }

    if (stop.stop_requested()) return;
    decoder.Send(nullptr);
    drain();
}

void Streamer::VideoDecodeLoop(std::stop_token stop) {
    const av::VideoProcessor::Sink sink = [&](VideoFrame &&frame) { return m_video_frames.Push(std::move(frame), stop); };
    const AVRational time_base = m_input->VideoStream()->time_base;
    const AVRational frame_rate = m_input->VideoFrameRate();
    const auto drop_threshold = std::max<Clock::duration>(
        std::chrono::microseconds(2 * av_rescale(1000000, frame_rate.den, frame_rate.num)), kMinDropThreshold);

    // Under overload, shed work in two steps. While packets are backing up, decoded frames that are
    // already late skip filtering and conversion (at least one frame is still shown every
    // kMaxDecodeFreeze). If decoding itself stays behind for kVideoBehindGrace, packets are skipped up
    // to the next keyframe. Without a backlog the source itself is late, and dropping would not help.
    Clock::time_point last_emitted{};
    Clock::time_point behind_since{};
    Clock::time_point last_warning{};

    auto on_frame = [&](av::FramePtr frame) {
        int64_t ts = frame->best_effort_timestamp != AV_NOPTS_VALUE ? frame->best_effort_timestamp : frame->pts;
        if (m_options.pacing && ts != AV_NOPTS_VALUE && m_clock->Started()) {
            auto now = Clock::now();
            bool behind = now - m_clock->DueTime(av::ToMicros(ts, time_base)) > drop_threshold &&
                          m_video_packets.Size() > 0;
            if (!behind) {
                behind_since = {};
            } else if (behind_since == Clock::time_point{}) {
                behind_since = now;
            }
            if (behind && now - last_emitted < kMaxDecodeFreeze) {
                m_video_dropped++;
                return;
            }
        }
        m_video_processor->Process(std::move(frame), sink);
        last_emitted = Clock::now();
    };

    auto should_skip = [&] {
        if (behind_since == Clock::time_point{} || Clock::now() - behind_since < kVideoBehindGrace) return false;
        if (Clock::now() - last_warning > 10s) {
            log::Warn("video decoding cannot keep up in real time; skipping ahead to keyframes");
            last_warning = Clock::now();
        }
        return true;
    };

    Decode(*m_video_decoder, m_video_packets, stop, on_frame, should_skip);
    if (!stop.stop_requested()) m_video_processor->Flush(sink);

    m_video_frames.Close();
}

void Streamer::AudioDecodeLoop(std::stop_token stop) {
    const av::AudioProcessor::Sink sink = [&](AudioFrame &&frame) { return m_audio_frames.Push(std::move(frame), stop); };

    Decode(
        *m_audio_decoder, m_audio_packets, stop, [&](av::FramePtr frame) { m_audio_processor->Process(frame.get(), sink); },
        [] { return false; });
    if (!stop.stop_requested()) m_audio_processor->Flush(sink);

    m_audio_frames.Close();
}

bool Streamer::AudioIsMaster() const {
    if (!m_audio_presenting.load()) return false;
    int64_t last = m_audio_last_sent.load();
    return last != 0 && Clock::now() - Clock::time_point(Clock::duration(last)) < kAudioMasterIdle;
}

bool Streamer::WaitUntilDue(int64_t pts_us, bool is_audio, std::stop_token stop) {
    std::mutex mutex;
    std::condition_variable_any cv;

    while (true) {
        auto now = Clock::now();
        auto due = m_clock->DueTime(pts_us);

        // Only the master moves the clock; a slave waits for (or drops behind) the master's schedule.
        bool master = is_audio || !AudioIsMaster();
        bool far_behind = now - due > kResyncLate;
        bool far_ahead = m_live && due - now > kResyncEarly;
        if (master && (far_behind || far_ahead)) {
            m_resyncs++;
            log::Warn("{} is {:.2f} s {} schedule; resynchronizing", is_audio ? "audio" : "video",
                      Seconds(far_behind ? now - due : due - now), far_behind ? "behind" : "ahead of");
            m_clock->Resync(pts_us, now);
            return true;
        }
        if (now >= due) return true;

        // Sleep in short steps so a re-anchored clock or a change of master takes effect promptly.
        std::unique_lock lock(mutex);
        cv.wait_until(lock, stop, std::min(due, now + kMaxSleepStep), [] { return false; });
        if (stop.stop_requested()) return false;
    }
}

void Streamer::VideoPresentLoop(std::stop_token stop) {
    bool started = false;
    Clock::time_point last_sent{};

    while (auto frame = m_video_frames.Pop(stop)) {
        if (!started) {
            if (!m_clock->WaitForStart(frame->pts_us, stop)) break;
            started = true;
        }

        if (m_options.pacing) {
            const auto now = Clock::now();
            const auto lateness = now - m_clock->DueTime(frame->pts_us);
            const auto drop_threshold = std::max<Clock::duration>(std::chrono::microseconds(2 * frame->duration_us),
                                                                  kMinDropThreshold);
            const bool late = lateness > drop_threshold;

            // Dropping only helps when a newer frame is ready; otherwise this is the freshest picture.
            bool resync_pending = !AudioIsMaster() && lateness > kResyncLate;
            if (late && !resync_pending && m_video_frames.Size() > 0 && now - last_sent < kMaxFreeze) {
                m_video_dropped++;
                log::Trace("dropped video frame {:.3f} s late", Seconds(lateness));
                continue;
            }
            if (!WaitUntilDue(frame->pts_us, false, stop)) break;
        }

        int64_t timecode = m_clock->Timecode(frame->pts_us);
        m_sender->SendVideo(std::move(*frame), timecode);
        last_sent = Clock::now();
        m_video_sent++;
    }

    if (!started) m_clock->StreamFinished();
    m_sender->FlushVideo();
}

void Streamer::AudioPresentLoop(std::stop_token stop) {
    bool started = false;

    while (auto frame = m_audio_frames.Pop(stop)) {
        if (!started) {
            if (!m_clock->WaitForStart(frame->pts_us, stop)) break;
            started = true;
        }

        if (m_options.pacing && !WaitUntilDue(frame->pts_us, true, stop)) break;

        m_sender->SendAudio(*frame, m_clock->Timecode(frame->pts_us));
        m_audio_last_sent = Ticks(Clock::now());
        m_audio_sent++;
    }

    if (!started) m_clock->StreamFinished();
    m_audio_presenting = false;
}

StreamerStats Streamer::Stats() const {
    StreamerStats stats;
    stats.video_sent = m_video_sent.load();
    stats.video_dropped = m_video_dropped.load();
    stats.audio_sent = m_audio_sent.load();
    stats.video_skipped = m_video_skipped.load();
    stats.loops = m_loops.load();
    stats.resyncs = m_resyncs.load();
    stats.video_queued = m_video_frames.Size();
    stats.audio_queued = m_audio_frames.Size();
    stats.connections = m_sender->Connections();
    return stats;
}

void Streamer::LogStats() const {
    StreamerStats stats = Stats();
    log::Info("stats: video {} sent, {} dropped, {} packets skipped | audio {} blocks | queued v{}/{} a{}/{} | "
              "loops {} | resyncs {} | receivers {}",
              stats.video_sent, stats.video_dropped, stats.video_skipped, stats.audio_sent, stats.video_queued,
              kVideoFrameQueue, stats.audio_queued, kAudioFrameQueue, stats.loops, stats.resyncs, stats.connections);
}

} // namespace ndistreamer
