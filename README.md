# ndi-streamer

## Building

This application has only been built and tested using ubuntu.
Porting to other distros would be trivial

### Dependencies

Your basic set of requirements are as follows:

- C/C++ Compiler(GCC/G++ by default)
- cmake (With make preferred)
- ffmpeg dev libraries
- NDI SDK (https://ndi.video/download-ndi-sdk/)
    . My CMake script expects you to install this in /opt/ndi
- avahi client development libraries (NDI uses mdns)

You might need to manually acquire more dependencies depending
on your system configuration. Refer to the build system output.

### Invoking the build system

```
mkdir build && cd build
cmake .. && make
```

This will generate the executable in your build directory

## Usage

```
./ndi-streamer
    -i /path/to/your/media.mp4
    -s "NDI Source Name"
```

