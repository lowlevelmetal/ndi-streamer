#!/usr/bin/env bash
# End-to-end test against the real NDI runtime: ndistreamer sends a looping clip and ndi_receiver
# receives it over the network stack. A private NDI configuration and a local discovery server are
# used, so neither avahi nor changes to ~/.ndi are needed.
#
# Usage: run_e2e.sh NDISTREAMER NDI_RECEIVER NDI_LIB_DIR DISCOVERY_SERVER MEDIA WORK_DIR

set -euo pipefail

streamer=$1
receiver=$2
ndi_lib=$3
discovery=$4
media=$5
work=$6

mkdir -p "$work/config"
export NDI_CONFIG_DIR="$work/config"
echo '{ "ndi": { "networks": { "discovery": "127.0.0.1" } } }' > "$NDI_CONFIG_DIR/ndi-config.v1.json"

pids=()
cleanup() {
    for pid in "${pids[@]}"; do kill "$pid" 2>/dev/null || true; done
    wait 2>/dev/null || true
}
trap cleanup EXIT

fail() {
    echo "FAIL: $*"
    echo "--- ndistreamer log"; cat "$work/streamer.log" || true
    echo "--- receiver log"; cat "$work/receiver.log" || true
    exit 1
}

# An already running discovery server on this machine works just as well.
"$discovery" > "$work/discovery.log" 2>&1 &
pids+=($!)
sleep 1

name="ndistreamer-e2e-$$"
"$streamer" -i "$media" --loop -s "$name" --ndi-lib "$ndi_lib" > "$work/streamer.log" 2>&1 &
streamer_pid=$!
pids+=("$streamer_pid")

"$receiver" "$name" 3 > "$work/received.txt" 2> "$work/receiver.log" || fail "receiver could not connect"

kill -INT "$streamer_pid"
wait "$streamer_pid" || fail "ndistreamer did not exit cleanly after SIGINT"
grep -q "stopped" "$work/streamer.log" || fail "ndistreamer did not report a clean stop"

video=$(grep -c '^video' "$work/received.txt" || true)
audio=$(grep -c '^audio' "$work/received.txt" || true)
echo "received $video video frames and $audio audio blocks in 3 s"

# 25 fps for 3 s is 75 frames; connecting takes a moment.
[ "$video" -ge 60 ] && [ "$video" -le 80 ] || fail "expected about 75 video frames, got $video"
# 48 kHz in 1024-sample blocks for 3 s is about 140 blocks.
[ "$audio" -ge 110 ] || fail "expected about 140 audio blocks, got $audio"

awk '$1 == "video" && !($3 == 640 && $4 == 360 && $5 == "UYVY" && $6 == 25 && $7 == 1 && $9 == 1) { bad++ }
     END { exit bad > 0 }' "$work/received.txt" || fail "unexpected video format"

awk '$1 == "audio" && !($3 == 48000 && $4 == 2) { bad++ }
     $1 == "audio" && $7 > 0.03 { loud++ }
     END { exit bad > 0 || loud == 0 }' "$work/received.txt" || fail "unexpected or silent audio"

# Timecodes advance exactly one frame at a time, except for at most a short wait at a loop point.
awk '$1 == "video" { if (prev && ($8 - prev < 400000 || $8 - prev > 650000)) bad++;
                     if (prev && $8 - prev != 400000) odd++; prev = $8 }
     END { exit bad > 0 || odd > 2 }' "$work/received.txt" || fail "video timecodes are not continuous"

echo "PASS"
