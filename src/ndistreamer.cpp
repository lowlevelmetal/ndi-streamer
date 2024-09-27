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
#include "macro.hpp"
#include "app.hpp"
#include "vaapiapp.hpp"
#include "cudaapp.hpp"

typedef struct CommandLineArguments {
    std::string videofile;
    std::string ndisource;
    std::string hwtype;

    CommandLineArguments() : videofile(""), ndisource("NDI Source"), hwtype("software") {}
} COMMANDLINEARGUMENTS, *PCOMMANDLINEARGUMENTS;

void Usage(const char *const argv0) {
    printf("\n%s\n"
           "\t-i /path/to/media.mp4\n"
           "\t-s \"NDI Source Name\"\n"
           "\t-t [software, cuda, vaapi]\n\n",
           argv0);
}

// Process command line arguments
ERRORTYPE ParseCommandLineArguments(COMMANDLINEARGUMENTS &cmdlineargs, int argc, char **argv) {

    int opt = 0;
    while ((opt = getopt(argc, argv, "i:s:t:")) != -1) {
        switch (opt) {
        case 'i':
            cmdlineargs.videofile = optarg;
            break;
        case 's':
            cmdlineargs.ndisource = optarg;
            break;
        case 't':
            cmdlineargs.hwtype = optarg;
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

    if(cmdlineargs.hwtype != "software" && cmdlineargs.hwtype != "cuda" && cmdlineargs.hwtype != "vaapi") {
        ERROR("Invalid HW type");
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

    PRINT("NDI Streamer");
    PRINT("NDI Source: %s", cmdlineargs.ndisource.c_str());
    PRINT("Video File: %s", cmdlineargs.videofile.c_str());
    PRINT("HW Type: %s", cmdlineargs.hwtype.c_str());

    // Create the application
    if(cmdlineargs.hwtype == "software") {
        auto [app, err] = App::Create(cmdlineargs.ndisource, cmdlineargs.videofile);
        if (err.code()) {
            FATAL("Error creating application: %s", err.what());
        }

        // Run the application
        err = app->Run();
        if (err.code()) {
            FATAL("Error running application: %s", err.what());
        }
    } else if(cmdlineargs.hwtype == "vaapi") {
        auto [app, err] = VAAPIApp::Create(cmdlineargs.ndisource, cmdlineargs.videofile);
        if (err.code()) {
            FATAL("Error creating application: %s", err.what());
        }

        // Run the application
        err = app->Run();
        if (err.code()) {
            FATAL("Error running application: %s", err.what());
        }
    } else if(cmdlineargs.hwtype == "cuda") {
        auto [app, err] = CudaApp::Create(cmdlineargs.ndisource, cmdlineargs.videofile);
        if (err.code()) {
            FATAL("Error creating application: %s", err.what());
        }

        // Run the application
        err = app->Run();
        if (err.code()) {
            FATAL("Error running application: %s", err.what());
        }
    }


    fflush(stdout);
    fflush(stderr);

    return EXIT_SUCCESS;
}
