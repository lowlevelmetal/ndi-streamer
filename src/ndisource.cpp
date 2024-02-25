/*
 * ndi-streamer
 * ndisource.cpp
 *
 * 09-21-2024
 * Matthew Todd Geiger
 */

// local includes
#include "ndisource.hpp"
#include "Processing.NDI.Send.h"
#include "Processing.NDI.structs.h"
#include "ndierror.hpp"
#include "macro.hpp"
#include "decoder.hpp"

namespace AV {
    NdiSource::NdiSource(std::string &ndi_source_name) : m_ndi_source_name(ndi_source_name) {
        try {
            m_Initialize();
        } catch(NdiErrorCode &err) {
            ERROR("%s", NdiErrorStr(err).c_str());
        }
    }

    NdiSource::~NdiSource() {
        if(m_initialized)
            NDIlib_send_destroy(m_pNDI_send);
    }

    NdiErrorCode NdiSource::SendPacket(int pixel_format, int width, int height, int fr_num, int fr_den, int stride, uint8_t *video_data) {
        NDIlib_video_frame_v2_t video_frame;

        // Verify supported formats
        switch(pixel_format) {
            case AV_PIX_FMT_UYVY422:
                video_frame.FourCC = NDIlib_FourCC_type_UYVY;
                break;
            default:
                return NdiErrorCode::UnsupportedPixFormat;
        }

        // Create NDI packet
        video_frame.frame_format_type = NDIlib_frame_format_type_progressive;
        video_frame.p_data = video_data;
        video_frame.line_stride_in_bytes = stride;
        video_frame.xres = width;
        video_frame.yres = height;
        video_frame.frame_rate_N = fr_num;
        video_frame.frame_rate_D = fr_den;

        DEBUG("\n\tWidth --> %d\n"
                "\tHeight --> %d\n"
                "\tStride --> %d\n"
                "\tfr_num --> %d\n"
                "\tfr_den --> %d\n", width, height, stride, fr_num, fr_den);

        // Send out packet on network
        NDIlib_send_send_video_v2(m_pNDI_send, &video_frame);

        return NdiErrorCode::NoError;
    }

    void NdiSource::m_Initialize() {
        NDIlib_send_create_t NDI_send_create_desc;
        NDI_send_create_desc.p_ndi_name = m_ndi_source_name.c_str();

        if(!(m_pNDI_send = NDIlib_send_create(&NDI_send_create_desc)))
            throw NdiErrorCode::SendCreate;

        m_initialized = false;
    }

} // namespace AV