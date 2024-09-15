/**
 * @file asyncndisource_test.cpp
 * @brief This file includes tests for working with NDI asynchronously.
 * @date 2024-09-11
 * @author Matthew Todd Geiger
 */

#include <gtest/gtest.h>

#include "asyncndisource.hpp"
#include "pixelencoder.hpp"

TEST(AsyncNdiSource, Create) {
	auto [source, err] = AV::Utils::AsyncNdiSource::Create("Test");
	EXPECT_EQ(err.code(), 0);
}

TEST(AsyncNdiSource, LoadVideoFrame) {
    // Create demuxer
    auto [demuxer, demuxer_err] = AV::Utils::Demuxer::Create("testcontent/rickroll.mp4");
    auto streams = demuxer->GetStreamPointers();

    // Create decoder
    auto codecpar = streams[0]->codecpar;
    auto [decoder, decoder_err] = AV::Utils::Decoder::Create(codecpar);

    // Create pixel encoder
    AV::Utils::PixelEncoderConfig encoder_config;
    encoder_config.src_width = streams[0]->codecpar->width;
    encoder_config.src_height = streams[0]->codecpar->height;
    encoder_config.src_pix_fmt = (AVPixelFormat)streams[0]->codecpar->format;
    encoder_config.dst_width = 1920;
    encoder_config.dst_height = 1080;
    encoder_config.dst_pix_fmt = AV_PIX_FMT_UYVY422;
    auto [encoder, encoder_err] = AV::Utils::PixelEncoder::Create(encoder_config);

    // Create NDI source
    auto [ndisource, ndisource_err] = AV::Utils::AsyncNdiSource::Create("Test NDI Source");

    // Decode frames and send to NDI
    for (int i = 0; i < 6; i++) {
        // Grab correct packet
        AVPacket *pkt = nullptr;
        do {
            auto [packet, packet_err] = demuxer->ReadFrame();
            if (packet->stream_index == 0) {
                pkt = packet;
            }

        } while (pkt == nullptr);

        // Fill Decoder
        auto fill_err = decoder->FillDecoder(pkt);
        EXPECT_EQ(fill_err.code(), 0);

        while (true) {
            auto [frame, frame_err] = decoder->Decode();
            if (frame_err.code() == (int)AV::Utils::AvError::DECODEREXHAUSTED) {
                break;
            }
            EXPECT_EQ(frame_err.code(), 0);
            auto [encoded_frame, encoded_frame_err] = encoder->Encode(frame);

            ndisource->Start();
            
            AV::Utils::AvException load_err;
            while(1) {
                load_err = ndisource->LoadVideoFrame(encoded_frame, encoder->GetPixelFormat(), streams[0]->time_base, decoder->GetFrameRate());
                if (load_err.code() != 0) {
                    if(load_err.code() == (int)AV::Utils::AvError::BUFFERFULL) {
                        continue;
                    }
                    
                    break;
                }
            }
            
            EXPECT_EQ(load_err.code(), 0);
        }
    }
}

int main(int argc, char **argv) {
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}