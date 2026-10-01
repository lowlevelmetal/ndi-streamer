#!/usr/bin/env bash
# Packages a portable ndistreamer build for release.
#
# Usage: packaging/package.sh BINARY DEPS_PREFIX OUTPUT_DIR
#
#   BINARY       ndistreamer built with -DNDISTREAMER_STATIC=ON
#   DEPS_PREFIX  the prefix given to packaging/build-static-deps.sh
#   OUTPUT_DIR   where the archives are written
#
# Fails if the binary depends on anything besides glibc. Produces:
#   ndistreamer-VERSION-linux-ARCH.tar.gz          the binary, docs and all license texts
#   ndistreamer-VERSION-third-party-sources.tar    the exact sources of the bundled libraries
#   SHA256SUMS

set -euo pipefail

if [ $# -ne 3 ]; then
    echo "usage: $0 BINARY DEPS_PREFIX OUTPUT_DIR" >&2
    exit 2
fi

binary=$(realpath "$1")
deps=$(realpath "$2")
output=$(realpath -m "$3")
root=$(cd "$(dirname "$0")/.." && pwd)
share=$deps/share/ndistreamer-deps

version=$("$binary" --version | head -n 1 | awk '{print $2}')
arch=$(uname -m)
name=ndistreamer-$version-linux-$arch

# Only glibc may remain dynamic.
allowed='^(libc\.so\.6|libm\.so\.6|libdl\.so\.2|libpthread\.so\.0|librt\.so\.1|ld-linux.*\.so\.[0-9]+)$'
needed=$(readelf -d "$binary" | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p')
unexpected=$(grep -Ev "$allowed" <<<"$needed" || true)
if [ -n "$unexpected" ]; then
    echo "error: $binary depends on non-glibc libraries:" >&2
    echo "$unexpected" >&2
    exit 1
fi
glibc=$(objdump -T "$binary" | grep -oE 'GLIBC_[0-9]+(\.[0-9]+)+' | sed 's/GLIBC_//' | sort -V | tail -n 1)
echo "==> $binary needs only glibc $glibc or newer"

license_of() {
    case $1 in
    ffmpeg) echo "LGPL-3.0-or-later" ;;
    dav1d) echo "BSD-2-Clause" ;;
    mbedtls) echo "Apache-2.0" ;;
    zlib) echo "Zlib" ;;
    *) echo "MIT" ;;
    esac
}

staging=$(mktemp -d)
trap 'rm -rf "$staging"' EXIT
dir=$staging/$name
mkdir -p "$dir/licenses" "$output"

cp "$binary" "$dir/ndistreamer"
strip "$dir/ndistreamer"
cp "$root/README.md" "$root/CHANGELOG.md" "$dir/"
cp "$root/LICENSE" "$dir/LICENSE"
cp "$share"/licenses/* "$dir/licenses/"

{
    cat <<EOF
# Third-party software

This build of ndistreamer $version statically links the libraries below. ndistreamer itself is
MIT licensed (see LICENSE). The full license texts are in the \`licenses\` directory.

The binary needs glibc $glibc or newer. It does not include the NDI runtime, which is loaded from
the system at start-up and is licensed separately by Vizrt NDI AB. NDI® is a registered trademark of
Vizrt NDI AB.

| Component | Version | License | Source |
|---|---|---|---|
EOF
    while IFS=$'\t' read -r component ver url _; do
        echo "| $component | $ver | $(license_of "$component") | $url |"
    done <"$share/manifest.tsv"
    cat <<EOF

The C++ runtime (libstdc++, libgcc, libatomic) is linked under the GCC Runtime Library Exception.

## FFmpeg

FFmpeg is licensed under the GNU Lesser General Public License version 3 or later. You may modify
FFmpeg and relink ndistreamer against your version: the complete source of ndistreamer is at
https://github.com/lowlevelmetal/ndi-streamer (tag v$version), and
\`packaging/build-static-deps.sh\` rebuilds every library listed above. The exact source archives
used for this build are published next to it as \`ndistreamer-$version-third-party-sources.tar\`.

FFmpeg was configured with:

\`\`\`
$(cat "$share/ffmpeg-configuration.txt")
\`\`\`
EOF
} >"$dir/THIRD_PARTY_NOTICES.md"

tar -C "$staging" -czf "$output/$name.tar.gz" --owner=0 --group=0 "$name"

sources=()
while IFS=$'\t' read -r _ _ url _; do
    sources+=("$(basename "$url")")
done <"$share/manifest.tsv"
tar -C "${NDISTREAMER_DOWNLOAD_DIR:-$deps/src}" -cf "$output/ndistreamer-$version-third-party-sources.tar" \
    --owner=0 --group=0 "${sources[@]}"

(cd "$output" && sha256sum "$name.tar.gz" "ndistreamer-$version-third-party-sources.tar" >SHA256SUMS)
echo "==> wrote $output/$name.tar.gz"
ls -la "$output"
