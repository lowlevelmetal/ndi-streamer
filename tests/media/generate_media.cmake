# Generates the short clips used by the test suite with the ffmpeg CLI. Only FFmpeg's built-in
# encoders are used, so any FFmpeg build works.
#
# Usage: cmake -DFFMPEG=/usr/bin/ffmpeg -DOUTPUT_DIR=<dir> -P generate_media.cmake

if(NOT FFMPEG OR NOT OUTPUT_DIR)
    message(FATAL_ERROR "FFMPEG and OUTPUT_DIR must be set")
endif()

file(MAKE_DIRECTORY "${OUTPUT_DIR}")

function(run_ffmpeg name required)
    set(output "${OUTPUT_DIR}/${name}")
    if(EXISTS "${output}")
        return()
    endif()
    execute_process(
        COMMAND "${FFMPEG}" -hide_banner -loglevel error -y ${ARGN} "${output}.tmp"
        RESULT_VARIABLE result
        ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        file(REMOVE "${output}.tmp")
        if(required)
            message(FATAL_ERROR "generating ${name} failed: ${error}")
        endif()
        message(WARNING "skipping ${name} (needs an optional encoder): ${error}")
        return()
    endif()
    file(RENAME "${output}.tmp" "${output}")
endfunction()

function(generate name)
    run_ffmpeg(${name} TRUE ${ARGN})
endfunction()

# For clips that need encoders not every FFmpeg build has; tests using them are skipped.
function(generate_optional name)
    run_ffmpeg(${name} FALSE ${ARGN})
endfunction()

# 2 s of 25 fps video with B-frames, and 48 kHz stereo AAC.
generate(av.mp4
    -f lavfi -i testsrc2=size=640x360:rate=25:duration=2
    -f lavfi -i sine=frequency=440:sample_rate=48000:duration=2
    -ac 2 -c:v mpeg4 -bf 2 -q:v 5 -c:a aac -f mp4)

# NTSC frame rate, no audio.
generate(video_only.mkv
    -f lavfi -i testsrc2=size=320x240:rate=30000/1001:duration=1.5
    -c:v mpeg4 -q:v 5 -f matroska)

# 5.1 AC-3, no video.
generate(audio_51.mka
    -f lavfi -i sine=frequency=1000:sample_rate=48000:duration=1.5
    -af pan=5.1|FL=c0|FR=c0|FC=c0|LFE=c0|BL=c0|BR=c0 -c:a ac3 -f matroska)

# 16-bit mono PCM at 44.1 kHz: exercises sample format conversion.
generate(audio_s16_mono.wav
    -f lavfi -i sine=frequency=1000:sample_rate=44100:duration=1.5
    -c:a pcm_s16le -f wav)

# Interlaced (top field first) MPEG-2.
generate(interlaced.ts
    -f lavfi -i testsrc2=size=720x576:rate=25:duration=1
    -vf setfield=tff -flags +ilme+ildct -field_order tt -c:v mpeg2video -q:v 5 -f mpegts)

# Audio starting 0.5 s after the video.
generate(late_audio.mkv
    -f lavfi -i testsrc2=size=320x240:rate=25:duration=2
    -itsoffset 0.5 -f lavfi -i sine=frequency=440:sample_rate=48000:duration=1.5
    -c:v mpeg4 -q:v 5 -c:a aac -f matroska)

# RGB with alpha, decodable straight to BGRA.
generate(bgra.mkv
    -f lavfi -i testsrc2=size=160x120:rate=10:duration=1
    -pix_fmt bgra -c:v ffv1 -f matroska)

# Raw MPEG-4 elementary stream: no container timestamps.
generate(raw.m4v
    -f lavfi -i testsrc2=size=320x240:rate=25:duration=1
    -c:v mpeg4 -q:v 5 -f m4v)

# A video frame, another 0.25 s later, then a 5.25 s gap: a legitimate pause, not a discontinuity.
generate(slideshow.mkv
    -f lavfi -i testsrc2=size=160x120:rate=4:duration=0.75
    -vf "setpts='if(eq(N,2),5.5,N*0.25)/TB'" -fps_mode passthrough -c:v mpeg4 -q:v 5 -f matroska)

# 60 fps with frequent keyframes: enough packets to fill the video queue quickly.
generate(av_60fps.mp4
    -f lavfi -i testsrc2=size=320x180:rate=60:duration=5
    -f lavfi -i sine=frequency=440:sample_rate=48000:duration=5
    -c:v mpeg4 -g 30 -q:v 5 -c:a aac -f mp4)

# H.264 whose only IDR before the 10 s mark is cut off, like joining a live stream mid-GOP.
generate_optional(gop_source.ts
    -f lavfi -i testsrc2=size=320x240:rate=25:duration=12
    -c:v libx264 -preset ultrafast -g 250 -bf 0 -f mpegts)
if(EXISTS "${OUTPUT_DIR}/gop_source.ts")
    generate(midgop.ts -i "${OUTPUT_DIR}/gop_source.ts" -ss 1 -c copy -copyinkf -f mpegts)
endif()

# MP3 carries encoder delay and padding that the decoder trims.
generate_optional(loop.mp3
    -f lavfi -i sine=frequency=440:sample_rate=44100:duration=1
    -c:a libmp3lame -b:a 128k -f mp3)
