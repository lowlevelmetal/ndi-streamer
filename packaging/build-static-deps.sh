#!/usr/bin/env bash
# Builds FFmpeg and the libraries it uses as static libraries, so ndistreamer can be linked into a
# portable binary that depends only on glibc (configure it with -DNDISTREAMER_STATIC=ON).
#
# Usage: packaging/build-static-deps.sh PREFIX
#
# Needs a C compiler, make, cmake, meson, ninja, nasm, pkg-config, curl and tar. Sources are
# downloaded into $NDISTREAMER_DOWNLOAD_DIR (default: PREFIX/src) and verified against SHA-256 sums.
# License texts and a manifest of what was built go to PREFIX/share/ndistreamer-deps.
#
# Included: FFmpeg's built-in codecs, formats and protocols; AV1 software decoding (dav1d); TLS for
# https and friends (Mbed TLS); VAAPI and NVDEC hardware decoding; V4L2 capture.
# Not included: X11 screen capture and ALSA/PulseAudio input, which need the system's libraries
# (build against your distribution's FFmpeg for those).

set -euo pipefail

if [ $# -ne 1 ]; then
    echo "usage: $0 PREFIX" >&2
    exit 2
fi

prefix=$(realpath -m "$1")
downloads=${NDISTREAMER_DOWNLOAD_DIR:-$prefix/src}
build=$prefix/build
share=$prefix/share/ndistreamer-deps
jobs=$(nproc)

# name version sha256 url license-file(s, relative to the source tree)
packages=(
    "zlib 1.3.2 d7a0654783a4da529d1bb793b7ad9c3318020af77667bcae35f95d0e42a792f3 https://zlib.net/zlib-1.3.2.tar.xz"
    "nv-codec-headers 11.1.5.3 2974b91062197e0527dffa3aadd8fe3bfa6681ae45f5ff9181bc0ca6479abd59 https://github.com/FFmpeg/nv-codec-headers/releases/download/n11.1.5.3/nv-codec-headers-11.1.5.3.tar.gz"
    "libdrm 2.4.134 ac5e74d157830eb8bee44c6a6bf3ad49774ef0dd2a72bdad74a8f20308b52a95 https://dri.freedesktop.org/libdrm/libdrm-2.4.134.tar.xz"
    "libva 2.24.1 eec6050b52876f229bd35e9df17cd31a06785e18e6f7990c445b584628483d67 https://github.com/intel/libva/releases/download/2.24.1/libva-2.24.1.tar.bz2"
    "dav1d 1.5.4 686616b7c69eb88d44459391ab25cac13b6647a3b288835c5784e71c1514a5c5 https://downloads.videolan.org/pub/videolan/dav1d/1.5.4/dav1d-1.5.4.tar.xz"
    "mbedtls 3.6.7 a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6 https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.7/mbedtls-3.6.7.tar.bz2"
    "ffmpeg 9.0.2 8c3850283eb25fa026482078a04051e0be17347b09ef81a0849bec15a96e002e https://ffmpeg.org/releases/ffmpeg-9.0.2.tar.xz"
)

# Where the statically linked libva looks for VA drivers (the system's); LIBVA_DRIVERS_PATH overrides.
va_driver_path=/usr/lib/x86_64-linux-gnu/dri:/usr/lib64/dri:/usr/lib/dri:/usr/local/lib/dri:/usr/lib/aarch64-linux-gnu/dri

mkdir -p "$prefix" "$downloads" "$build" "$share/licenses"
: > "$share/manifest.tsv"

# Only our own packages are visible to pkg-config, so nothing is picked up from the system by accident.
export PKG_CONFIG_LIBDIR=$prefix/lib/pkgconfig
export PKG_CONFIG_PATH=
export CFLAGS="-O2 -fPIC"
export CXXFLAGS="-O2 -fPIC"

log() {
    echo "==> $*"
}

# fetch NAME: download (if needed), verify and unpack a package; prints its source directory.
fetch() {
    local entry name version sha url file dir
    for entry in "${packages[@]}"; do
        read -r name version sha url <<<"$entry"
        [ "$name" = "$1" ] && break
    done
    file=$downloads/$(basename "$url")
    if [ ! -f "$file" ] || ! echo "$sha  $file" | sha256sum -c --status; then
        log "downloading $name $version" >&2
        curl -fsSL --retry 3 -o "$file.part" "$url"
        mv "$file.part" "$file"
    fi
    echo "$sha  $file" | sha256sum -c --quiet >&2

    dir=$build/$name-$version
    rm -rf "$dir"
    mkdir -p "$dir"
    tar -xf "$file" -C "$dir" --strip-components=1
    printf '%s\t%s\t%s\t%s\n' "$name" "$version" "$url" "$sha" >> "$share/manifest.tsv"
    echo "$dir"
}

meson_static() {
    local src=$1
    shift
    meson setup "$src/_build" "$src" --prefix="$prefix" --libdir=lib --buildtype=release \
        -Ddefault_library=static "$@"
    meson install -C "$src/_build"
}

build_zlib() {
    local src
    src=$(fetch zlib)
    (cd "$src" && ./configure --prefix="$prefix" --static && make -j"$jobs" && make install)
    cp "$src/LICENSE" "$share/licenses/zlib.txt"
}

build_nv_codec_headers() {
    local src
    src=$(fetch nv-codec-headers)
    make -C "$src" PREFIX="$prefix" install
    # MIT notice from the headers themselves; there is no separate license file.
    sed -n '/\/\*/,/\*\//p' "$src/include/ffnvcodec/dynlink_loader.h" | head -n 30 > "$share/licenses/nv-codec-headers.txt"
}

build_libdrm() {
    local src
    src=$(fetch libdrm)
    meson_static "$src" -Dintel=disabled -Dradeon=disabled -Damdgpu=disabled -Dnouveau=disabled \
        -Dvmwgfx=disabled -Domap=disabled -Dexynos=disabled -Dfreedreno=disabled -Dtegra=disabled \
        -Dvc4=disabled -Detnaviv=disabled -Dcairo-tests=disabled -Dman-pages=disabled -Dvalgrind=disabled \
        -Dtests=false -Dinstall-test-programs=false -Dudev=false
    # MIT notice from the sources; there is no separate license file.
    sed -n '/Copyright 1999/,/\*\//p' "$src/xf86drm.c" > "$share/licenses/libdrm.txt"
}

build_libva() {
    local src
    src=$(fetch libva)
    # libva only knows how to build shared libraries.
    sed -i 's/shared_library(/library(/' "$src/va/meson.build"
    meson_static "$src" -Dwith_x11=no -Dwith_glx=no -Dwith_wayland=no -Denable_docs=false \
        -Ddriverdir="$va_driver_path"
    cp "$src/COPYING" "$share/licenses/libva.txt"
}

build_dav1d() {
    local src
    src=$(fetch dav1d)
    meson_static "$src" -Denable_tools=false -Denable_tests=false -Denable_examples=false
    cp "$src/COPYING" "$share/licenses/dav1d.txt"
}

build_mbedtls() {
    local src
    src=$(fetch mbedtls)
    cmake -S "$src" -B "$src/_build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" \
        -DCMAKE_INSTALL_LIBDIR=lib -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DENABLE_PROGRAMS=OFF \
        -DENABLE_TESTING=OFF -DUSE_SHARED_MBEDTLS_LIBRARY=OFF -DUSE_STATIC_MBEDTLS_LIBRARY=ON
    cmake --build "$src/_build"
    cmake --install "$src/_build"
    cp "$src/LICENSE" "$share/licenses/mbedtls.txt"
}

build_ffmpeg() {
    local src
    src=$(fetch ffmpeg)
    local flags=(
        --prefix="$prefix"
        --pkg-config-flags=--static
        --extra-cflags="-I$prefix/include"
        --extra-ldflags="-L$prefix/lib"
        --enable-static --disable-shared --enable-pic
        --disable-autodetect --disable-doc --disable-debug --disable-ffplay --disable-ffprobe
        --enable-version3
        --enable-zlib --enable-libdav1d --enable-mbedtls
        --enable-vaapi --enable-ffnvcodec --enable-nvdec
    )
    (cd "$src" && ./configure "${flags[@]}" && make -j"$jobs" && make install)
    cp "$src/COPYING.LGPLv3" "$share/licenses/ffmpeg-LGPLv3.txt"
    cp "$src/LICENSE.md" "$share/licenses/ffmpeg-LICENSE.md"
    printf '%s\n' "${flags[@]:4}" > "$share/ffmpeg-configuration.txt"
}

build_zlib
build_nv_codec_headers
build_libdrm
build_libva
build_dav1d
build_mbedtls
build_ffmpeg

# Keep the downloads, drop the build trees.
[ "${NDISTREAMER_KEEP_BUILD:-0}" = 1 ] || rm -rf "$build"
log "done: configure ndistreamer with -DNDISTREAMER_STATIC=ON -DCMAKE_PREFIX_PATH=$prefix"
