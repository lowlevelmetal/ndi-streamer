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
#include "ndierror.hpp"
#include "macro.hpp"

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

    void NdiSource::m_Initialize() {
        NDIlib_send_create_t NDI_send_create_desc;
        NDI_send_create_desc.p_ndi_name = m_ndi_source_name.c_str();

        if(!(m_pNDI_send = NDIlib_send_create(&NDI_send_create_desc)))
            throw NdiErrorCode::SendCreate;

        m_initialized = false;
    }

} // namespace AV