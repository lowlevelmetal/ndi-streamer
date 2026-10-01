/**
 * @file sender.hpp
 * @brief An NDI source on the network.
 */

#pragma once

#include "frames.hpp"
#include "ndi/runtime.hpp"

#include <memory>
#include <string>

namespace ndistreamer::ndi {

struct SenderConfig {
    std::string name;   ///< Source name; NDI shows it as "HOST (name)".
    std::string groups; ///< Comma separated groups; empty for the default groups.
};

/**
 * @brief Publishes audio and video as an NDI source.
 *
 * Video is submitted asynchronously: NDI compresses and transmits a frame while the next one is being
 * prepared, and the sender keeps each frame alive until NDI has finished with it. Timing is the
 * caller's responsibility (NDI clocking is disabled) so frames go out at their media timestamps.
 *
 * SendVideo and SendAudio may be called from different threads.
 */
class Sender {
public:
    Sender(std::shared_ptr<Runtime> runtime, const SenderConfig &config);
    ~Sender();

    Sender(const Sender &) = delete;
    Sender &operator=(const Sender &) = delete;

    /**
     * @brief Queue a video frame for asynchronous transmission and release the previous one.
     */
    void SendVideo(VideoFrame frame, int64_t timecode);

    /**
     * @brief Wait until the last video frame has been processed and release it.
     */
    void FlushVideo();

    void SendAudio(const AudioFrame &frame, int64_t timecode);

    /**
     * @brief Number of receivers currently connected.
     */
    int Connections() const;

    /**
     * @brief The full network name of the source, e.g. "HOST (name)".
     */
    std::string SourceName() const;

private:
    std::shared_ptr<Runtime> m_runtime;
    const Runtime::Api &m_api;
    abi::SendInstance m_instance = nullptr;
    std::string m_name;
    VideoFrame m_in_flight;
};

} // namespace ndistreamer::ndi
