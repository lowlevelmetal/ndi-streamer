/**
 * @file sender.cpp
 * @brief An NDI source on the network.
 */

#include "ndi/sender.hpp"
#include "util/log.hpp"
#include "version.hpp"

#include <format>
#include <stdexcept>

namespace ndistreamer::ndi {

Sender::Sender(std::shared_ptr<Runtime> runtime, const SenderConfig &config)
    : m_runtime(std::move(runtime)), m_api(m_runtime->api()), m_name(config.name) {
    abi::SendCreate create{};
    create.p_ndi_name = config.name.c_str();
    create.p_groups = config.groups.empty() ? nullptr : config.groups.c_str();
    create.clock_video = false;
    create.clock_audio = false;

    m_instance = m_api.send_create(&create);
    if (!m_instance) {
        throw std::runtime_error(std::format("cannot create NDI source '{}'", config.name));
    }

    if (m_api.send_add_connection_metadata) {
        // Lets receivers identify what they are connected to.
        std::string product = std::format(R"(<ndi_product long_name="NDI Streamer" short_name="ndistreamer" )"
                                          R"(version="{}"/>)",
                                          NDISTREAMER_VERSION);
        abi::MetadataFrame metadata{};
        metadata.length = static_cast<int>(product.size() + 1);
        metadata.timecode = abi::kTimecodeSynthesize;
        metadata.p_data = product.data();
        m_api.send_add_connection_metadata(m_instance, &metadata);
    }
}

Sender::~Sender() {
    FlushVideo();
    m_api.send_destroy(m_instance);
}

void Sender::SendVideo(VideoFrame frame, int64_t timecode) {
    abi::VideoFrameV2 desc{};
    desc.xres = frame.width;
    desc.yres = frame.height;
    desc.FourCC = static_cast<uint32_t>(frame.fourcc);
    desc.frame_rate_N = frame.frame_rate_num;
    desc.frame_rate_D = frame.frame_rate_den;
    desc.picture_aspect_ratio = frame.aspect_ratio;
    desc.frame_format_type = frame.interlaced ? abi::FrameFormatInterleaved : abi::FrameFormatProgressive;
    desc.timecode = timecode;
    desc.p_data = frame.data;
    desc.line_stride_in_bytes = frame.stride;

    // Returns once NDI is done with the previous frame, which can then be released.
    m_api.send_video_async_v2(m_instance, &desc);
    m_in_flight = std::move(frame);
}

void Sender::FlushVideo() {
    if (!m_in_flight.storage) return;
    m_api.send_video_async_v2(m_instance, nullptr);
    m_in_flight = VideoFrame{};
}

void Sender::SendAudio(const AudioFrame &frame, int64_t timecode) {
    abi::AudioFrameV2 desc{};
    desc.sample_rate = frame.sample_rate;
    desc.no_channels = frame.channels;
    desc.no_samples = frame.samples_per_channel;
    desc.timecode = timecode;
    desc.p_data = const_cast<float *>(frame.samples.data()); // NDI only reads
    desc.channel_stride_in_bytes = frame.channel_stride * static_cast<int>(sizeof(float));

    m_api.send_audio_v2(m_instance, &desc);
}

int Sender::Connections() const {
    return m_api.send_get_no_connections(m_instance, 0);
}

std::string Sender::SourceName() const {
    if (m_api.send_get_source_name) {
        if (const abi::Source *source = m_api.send_get_source_name(m_instance); source && source->p_ndi_name) {
            return source->p_ndi_name;
        }
    }
    return m_name;
}

} // namespace ndistreamer::ndi
