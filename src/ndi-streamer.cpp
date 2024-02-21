/*
 * ndi-streamer
 * ndi-streamer.cpp
 *
 * 02-19-2024
 * Matthew Todd Geiger
 */

// Standard library
#include <cstdlib>
#include <libavcodec/codec.h>
#include <libavcodec/codec_par.h>
#include <string>

// POSIX includes
#include <unistd.h>

// NDI SDK
#include <Processing.NDI.Lib.h>

// Local includes
#include "macro.hpp"

// FFMPEG Includes
extern "C" {
    #include <libavformat/avformat.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/time.h>
    #include <libswscale/swscale.h>
}

typedef struct CommandLineArguments {
    std::string videofile;

    CommandLineArguments() : videofile(std::string("")) {}
} COMMANDLINEARGUMENTS, *PCOMMANDLINEARGUMENTS;

// Process command line arguments
ERRORTYPE ParseCommandLineArguments(COMMANDLINEARGUMENTS &cmdlineargs, int argc, char **argv) {

    int opt = 0;
    while ((opt = getopt(argc, argv, "i:")) != -1) {
        switch (opt) {
            case 'i':
                cmdlineargs.videofile = std::string(optarg);
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
    
    // Initialize NDIlib
    if(!NDIlib_initialize())
        FATAL("Failed to initialize NDI library");

    // Create avformat context
    AVFormatContext *pFormatContext = avformat_alloc_context();
    if (pFormatContext == nullptr)
        FATAL("Failed to create avformat context");

    // Parse command line arguments
    if(ParseCommandLineArguments(cmdlineargs, argc, argv) == FAILED)
        FATAL("Failed to process command line input");

    // Open video file header
    if(avformat_open_input(&pFormatContext, cmdlineargs.videofile.c_str(), nullptr, nullptr) != 0)
        FATAL("Failed to open video file header");

    // Get stream information
    if(avformat_find_stream_info(pFormatContext, nullptr) < 0)
        FATAL("Failed to get stream information");

    // Get video codec
    const AVCodec *pCodec = nullptr;
    AVCodecParameters *pCodecParameters = nullptr;
    int videostreamindex = -1;

    for (int i = 0; i < pFormatContext->nb_streams; i++) {
        AVCodecParameters *pLocalCodecParameters = nullptr;
        pLocalCodecParameters = pFormatContext->streams[i]->codecpar;

        DEBUG("AVStream->time_base before open coded %d/%d", pFormatContext->streams[i]->time_base.num, pFormatContext->streams[i]->time_base.den);
        DEBUG("AVStream->r_frame_rate before open coded %d/%d", pFormatContext->streams[i]->r_frame_rate.num, pFormatContext->streams[i]->r_frame_rate.den);
        DEBUG("AVStream->start_time %" PRId64, pFormatContext->streams[i]->start_time);
        DEBUG("AVStream->duration %" PRId64, pFormatContext->streams[i]->duration);

        const AVCodec *pLocalCodec = avcodec_find_decoder(pLocalCodecParameters->codec_id);
        if(pLocalCodec == nullptr)
            FATAL("Unsupported codec");

        // Store the video stream index
        if (pLocalCodecParameters->codec_type == AVMEDIA_TYPE_VIDEO) {
            if(videostreamindex == -1) {
                videostreamindex = i;
                pCodec = pLocalCodec;
                pCodecParameters = pLocalCodecParameters;
            }
        }

        DEBUG("Video Codec: resolution %d x %d", pLocalCodecParameters->width, pLocalCodecParameters->height);
    }


    if (videostreamindex == -1)
        FATAL("File %s does not contain a video stream!", argv[1]);

    // Destroy avformat context
    avformat_close_input(&pFormatContext);

    // Destroy NDIlib instance
    NDIlib_destroy();

    return EXIT_SUCCESS;
}
