# ndistreamer
Stream the contents of an AV file over NDI

## Mission

Develop a lightweight tool for streaming NDI from a linux environment.

## Building

This application has only been built and tested using gentoo.
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

Release Build
```
mkdir build && cd build
cmake .. && make
```
Debug Build
```
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug .. && make
```

This will generate the executable in your build directory

## Usage

```
./ndistreamer
    -i /path/to/your/media.mp4
    -s "NDI Source Name"
    -t [software, cuda, vaapi]
```

## Running tests
These tests are minimal and incomplete. It is on my todo list to create a better testing system.
Build the version of the software you want, as shown above, then navigate into your build directory
```
cd build
```
Then run the automated tests
```
ctest
```

## Common Problems
### Can't find NDI header files or binaries
Make sure you have installed the NDI SDK in your /opt/ndi directory
### Can't see my NDI source on the network
Make sure you have the avahi service daemon running on your host machine
### VAAPI and/or CUDA is not working
Make sure you build FFmpeg with the proper vaapi and/or cuda support
