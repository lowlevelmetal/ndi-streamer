/*
 * ndi-streamer
 * ndi-streamer.cpp
 *
 * 02-19-2024
 * Matthew Todd Geiger
 */

// Standard library
#include <cstdlib>
#include <iostream>
#include <string>

// POSIX includes
#include <unistd.h>

// Local includes
#include "averror.hpp"
#include "decoder.hpp"
#include "macro.hpp"
#include "ndierror.hpp"
#include "ndisource.hpp"

typedef struct CommandLineArguments {
    std::string videofile;
    std::string ndisource;

    CommandLineArguments() : videofile(std::string("")), ndisource(std::string("NDI Source")) {}
} COMMANDLINEARGUMENTS, *PCOMMANDLINEARGUMENTS;

void Usage(const char *const argv0) {
    printf("\n%s\n"
           "\t-i /path/to/media.mp4\n"
           "\t-s \"NDI Source Name\"\n\n",
           argv0);
}

// Process command line arguments
ERRORTYPE ParseCommandLineArguments(COMMANDLINEARGUMENTS &cmdlineargs, int argc, char **argv) {

    int opt = 0;
    while ((opt = getopt(argc, argv, "i:s:")) != -1) {
        switch (opt) {
        case 'i':
            cmdlineargs.videofile = std::string(optarg);
            break;
        case 's':
            cmdlineargs.ndisource = std::string(optarg);
            break;
        default:
            return FAILED;
        }
    }

    DEBUG("Video file --> %s", cmdlineargs.videofile.c_str());

    if (cmdlineargs.videofile == "") {
        ERROR("videofile required");
        return FAILED;
    }

    return SUCCESSFUL;
}

int main(int argc, char **argv) {
    COMMANDLINEARGUMENTS cmdlineargs;

    // Parse command line arguments
    if (!ParseCommandLineArguments(cmdlineargs, argc, argv)) {
        Usage(argv[0]);
        return EXIT_FAILURE;
    }

    // Create NDI server
    AV::NdiSource ndisrc(cmdlineargs.ndisource);

    // Create libav decoder
    AV::Decoder decoder(cmdlineargs.videofile);

#ifdef _DEBUG
    std::cout << "Press any key to continue..." << std::endl;
    std::cin.get();
#endif

    int total_audio_frames = 0;
    int total_video_frames = 0;
    AV::AvErrorCode ret;
    while ((ret = decoder.ReadFrame()) == AV::AvErrorCode::NoError) { // Read a frame
        if (decoder.IsCurrentFrameVideo()) {                          // If the frame is a video
            DEBUG("Processing Video Frame --> %d", total_video_frames);

            int decoder_count = 0;
            while ((ret = decoder.DecodeVideoPacket()) == AV::AvErrorCode::NoError) { // Start decoding packets in frames
                DEBUG("Packet Decoded --> %d", decoder_count);

                // Convert pixel format to UVYV
                //
                // YUV420p is very commonly used in mpeg4 files
                // which is not explicitly supported by NDI
                uint8_t *converted_buffer = decoder.ConvertToUVYV();
                if (!converted_buffer) {
                    FATAL("Failed to create UVYV buffer");
                }

                int resx, resy;
                int fr_num, fr_den;

                decoder.GetPacketDimensions(&resx, &resy);
                decoder.GetPacketFrameRate(&fr_num, &fr_den);

                // Send an NDI video packet out on the network
                auto ndiret = ndisrc.SendVideoPacket(AV_PIX_FMT_UYVY422,
                                                     resx, resy,
                                                     fr_num, fr_den,
                                                     decoder.GetUVYVPacketStride(),
                                                     converted_buffer);

                if (ndiret != AV::NdiErrorCode::NoError)
                    FATAL("%s", AV::NdiErrorStr(ndiret).c_str());

                // We need to manually free the av_buffer that was
                // created by ConvertToUVYV
                av_free(converted_buffer);

                decoder_count++;
            }

            if (ret != AV::AvErrorCode::PacketsClaimed)
                FATAL("%s", AV::AvErrorStr(ret).c_str());

            total_video_frames++;
        } else if (decoder.IsCurrentFrameAudio()) { // If the frame is audio
            DEBUG("Processing Audio Frame --> %d", total_audio_frames);

            int decoder_count = 0;
            while ((ret = decoder.DecodeAudioPacket()) == AV::AvErrorCode::NoError) { // Start decoding packets in frames
                DEBUG("Audio Packet Decoded --> %d", decoder_count);
                decoder_count++;

                uint8_t *audio_data = decoder.GetPacketData();
                int sample_rate = decoder.GetSampleRate();
                int channels = decoder.GetAudioChannels();
                int samples = decoder.GetSampleCount();
                int stride = decoder.GetBytesPerSample();

                ndisrc.SendAudioPacket(audio_data, sample_rate, channels, samples, stride, decoder.GetAudioSampleFormat());

                if (ret != AV::AvErrorCode::NoError)
                    FATAL("%s", AV::AvErrorStr(ret).c_str());
            }

            if (ret != AV::AvErrorCode::PacketsClaimed)
                FATAL("%s", AV::AvErrorStr(ret).c_str());

            total_audio_frames++;
        }
    }

    if (ret != AV::AvErrorCode::PacketsClaimed)
        FATAL("%s", AV::AvErrorStr(ret).c_str());

    return EXIT_SUCCESS;
}
