/**
 * @file cudadecoder.cpp
 * @brief This file includes the CudaDecoder class.
 * @version 1.0
 * @date 2024-09-19
 * @author Matthew Todd Geiger
 */

#include "cudadecoder.hpp"
#include "macro.hpp"

#include <chrono>

namespace AV::Utils {

/**
 * @brief Get the frame rate of the decoder
 *
 * @return CodecFrameRate
 */
CodecFrameRate CudaDecoder::GetFrameRate() {
    FUNCTION_CALL_DEBUG();

    return {m_codec->framerate.num, m_codec->framerate.den};
}

/**
 * @brief Fill the decoder with a packet
 *
 * @param packet The packet to fill the decoder with
 * @return AvException
 */
AvException CudaDecoder::FillCudaDecoder(AVPacket *packet) {
    FUNCTION_CALL_DEBUG();

    int ret = avcodec_send_packet(m_codec, packet);
    if (ret < 0) {
        PRINT_FFMPEG_ERR(ret);
        return AvException(AvError::SENDPACKET);
    }

    DEBUG("CudaDecoder Filled");

    return AvException(AvError::NOERROR);
}

/**
 * @brief Decode a packet
 *
 * @return CudaDecoderOutput
 */
CudaDecoderOutput CudaDecoder::Decode() {
    FUNCTION_CALL_DEBUG();
#ifdef _DEBUG
    // Profile function
    auto time_start = std::chrono::high_resolution_clock::now();
#endif

    av_frame_unref(m_last_frame);

    AVFrame *tmp_frame = av_frame_alloc();

    // Recieve frame from decoder
    int ret = avcodec_receive_frame(m_codec, tmp_frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        DEBUG("CudaDecoder exhausted");
        return {nullptr, AvException(AvError::DECODEREXHAUSTED)};
    } else if (ret < 0) {
        PRINT_FFMPEG_ERR(ret);
        return {nullptr, AvException(AvError::RECIEVEFRAME)};
    }

    // Print out tmp frame info
    DEBUG("Temp Frame: %dx%d, format: %s, pts: %ld", tmp_frame->width, tmp_frame->height, av_get_pix_fmt_name((AVPixelFormat)tmp_frame->format), tmp_frame->pts);

    // Load from off gpu if needed
    ret = av_hwframe_transfer_data(m_last_frame, tmp_frame, 0);
    if (ret < 0) {
        PRINT_FFMPEG_ERR(ret);
        return {nullptr, AvException(AvError::HWFRAME_TRANSFER)};
    }

    m_last_frame->pts = tmp_frame->pts;

    av_frame_free(&tmp_frame);

#ifdef _DEBUG
    // Profile function
    auto time_end = std::chrono::high_resolution_clock::now();
    DEBUG("Decode time (seconds): %f", std::chrono::duration<double>(time_end - time_start).count());
#endif

    // Print frame info
    DEBUG("Frame: %dx%d, format: %s", m_last_frame->width, m_last_frame->height, av_get_pix_fmt_name((AVPixelFormat)m_last_frame->format));
    DEBUG("Time base: %d/%d", m_last_frame->time_base.num, m_last_frame->time_base.den);

    // Print some pixel data from Y and UV
    uint8_t *y_data = m_last_frame->data[0];
    uint8_t *uv_data = m_last_frame->data[1];
    DEBUG("First Y bytes: %02x %02x %02x %02x", y_data[0], y_data[1], y_data[2], y_data[3]);
    DEBUG("First UV bytes: %02x %02x %02x %02x", uv_data[0], uv_data[1], uv_data[2], uv_data[3]);

    return {m_last_frame, AvException(AvError::NOERROR)};
}

/**
 * @brief Create a CudaDecoder object
 *
 * @param codecpar codec parameters
 * @return CudaDecoderResult
 */
CudaDecoderResult CudaDecoder::Create(AVCodecParameters *codecpar) {
    FUNCTION_CALL_DEBUG();

    AvException error;

    // Create a new decoder object, return nullopt if error
    try {
        return {std::unique_ptr<CudaDecoder>(new CudaDecoder(codecpar)), error};
    } catch (AvException e) {
        error = e;
        DEBUG("CudaDecoder error: %s", error.what());
    }

    return {nullptr, error};
}

/**
 * @brief Construct a new CudaDecoder:: CudaDecoder object
 *
 * @param codec_id codec ID
 */
CudaDecoder::CudaDecoder(AVCodecParameters *codecpar) : m_codecpar(codecpar) {
    FUNCTION_CALL_DEBUG();

    AvError err = m_Initialize();
    if (err != AvError::NOERROR) {
        throw err;
    }
}


/**
 * @brief Destroy the CudaDecoder:: CudaDecoder object
 */
CudaDecoder::~CudaDecoder() {
    FUNCTION_CALL_DEBUG();

    // Free the frame
    if (m_last_frame) {
        av_frame_free(&m_last_frame);
        DEBUG("av_frame_free called");
    }

    // Close and free the decoder context
    if (m_codec) {
        avcodec_close(m_codec);
        avcodec_free_context(&m_codec);
        DEBUG("avcodec_free_context called");
    }

    if(m_hw_device_ctx) {
        av_buffer_unref(&m_hw_device_ctx);
    }
}
/**
 * @brief Initialize the decoder
 *
 * @return AvError
 */
AvError CudaDecoder::m_Initialize() {
    FUNCTION_CALL_DEBUG();

    // Create hardware device
    int ret = av_hwdevice_ctx_create(&m_hw_device_ctx, AV_HWDEVICE_TYPE_CUDA, nullptr, nullptr, 0);
    if (ret < 0) {
        DEBUG("av_hwdevice_ctx_create failed");
        PRINT_FFMPEG_ERR(ret);
        return AvError::HWDEVICECTXALLOC;
    }

    // Find the appropriate decoder
    const AVCodec *codec = avcodec_find_decoder(m_codecpar->codec_id);
    if (!codec) {
        DEBUG("avcodec_find_decoder failed");
        return AvError::FINDDECODER;
    }

    // Allocate the codec context
    m_codec = avcodec_alloc_context3(codec);
    if (!m_codec) {
        DEBUG("avcodec_alloc_context3 failed");
        return AvError::DECODERALLOC;
    }

    // Copy the codec parameters
    ret = avcodec_parameters_to_context(m_codec, m_codecpar);
    if (ret < 0) {
        DEBUG("avcodec_parameters_to_context failed");
        PRINT_FFMPEG_ERR(ret);
        return AvError::DECPARAMS;
    }

    // Attach hardware device to decoder
    m_codec->hw_device_ctx = av_buffer_ref(m_hw_device_ctx);

    // Open the decoder context
    ret = avcodec_open2(m_codec, codec, nullptr);
    if (ret < 0) {
        DEBUG("avcodec_open2 failed");
        PRINT_FFMPEG_ERR(ret);
        return AvError::DECPARAMS;
    }

    // Allocate frame
    m_last_frame = av_frame_alloc();
    if (!m_last_frame) {
        DEBUG("av_frame_alloc failed");
        return AvError::FRAMEALLOC;
    }

    return AvError::NOERROR;
}

} // namespace AV::Utils