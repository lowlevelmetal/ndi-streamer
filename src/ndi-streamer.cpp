/*
 * ndi-streamer
 * ndi-streamer.cpp
 *
 * 02-19-2024
 * Matthew Todd Geiger
 */

// Standard library
#include <cstdlib>
#include <string>

// POSIX includes
#include <unistd.h>

// Local includes
#include "averror.hpp"
#include "macro.hpp"
#include "decoder.hpp"
#include "ndierror.hpp"
#include "ndisource.hpp"

typedef struct CommandLineArguments {
    std::string videofile;
    std::string ndisource;

    CommandLineArguments() : videofile(std::string("")), ndisource(std::string("NDI Source")) {}
} COMMANDLINEARGUMENTS, *PCOMMANDLINEARGUMENTS;

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

int main(int argc, char** argv) {
    COMMANDLINEARGUMENTS cmdlineargs;

    // Parse command line arguments
    if(ParseCommandLineArguments(cmdlineargs, argc, argv) == FAILED)
        FATAL("Failed to process command line input");

    AV::NdiSource ndisrc(cmdlineargs.ndisource);
    AV::Decoder decoder(cmdlineargs.videofile);

    int total_frames = 0;
    AV::AvErrorCode ret;
    while((ret = decoder.ReadFrame()) == AV::AvErrorCode::NoError) {
        if(decoder.IsCurrentFrameVideo()) {
            DEBUG("Processing Video Frame --> %d", total_frames);

            int decoder_count = 0;
            while((ret = decoder.DecodeVideoPacket()) == AV::AvErrorCode::NoError) {
                DEBUG("Packet Decoded --> %d", decoder_count);

                int format = decoder.GetFrameFormat();
                switch (format) {
                    case AV_PIX_FMT_YUV420P:
                        DEBUG("Sending YUV420P packet");
                        break;
                    default:
                        FATAL("Unsupported format --> 0x%04x", format);
                }

                int resx, resy;
                int fr_num, fr_den;

                decoder.GetPacketDimensions(&resx, &resy);
                decoder.GetPacketFrameRate(&fr_num, &fr_den);

                auto ndiret = ndisrc.SendPacket(format,
                    resx, resy,
                    fr_num, fr_den,
                    decoder.GetPacketStride(),
                    decoder.GetPacketData());

                if(ndiret != AV::NdiErrorCode::NoError)
                    FATAL("%s", AV::NdiErrorStr(ndiret).c_str());

                decoder_count++;
            }

            if(ret != AV::AvErrorCode::PacketsClaimed)
                FATAL("%s", AV::AvErrorStr(ret).c_str());

            total_frames++;
        }
    }

    if(ret != AV::AvErrorCode::PacketsClaimed)
        FATAL("%s", AV::AvErrorStr(ret).c_str());


    return EXIT_SUCCESS;
}
