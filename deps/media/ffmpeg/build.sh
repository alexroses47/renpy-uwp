#!/usr/bin/env bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

# Builds FFmpeg 4.3.1 (LGPL only) for UWP x64 with MSVC, from an MSYS2 bash that inherits the
# MSVC environment (cl.exe/link.exe in PATH). Codecs/containers: exactly what KS:RE uses
# (docs/ksre-media-inventory.json, docs/RESEARCH.md section 26).
# Usage: build.sh SRC_DIR PREFIX ZLIB_INCLUDE_DIR ZLIB_LIB_DIR   (Windows or MSYS paths)
set -euo pipefail

src="$(cygpath -u "$1")"
prefix="$(cygpath -m "$2")"
zinc="$(cygpath -m "$3")"
zlib="$(cygpath -m "$4")"
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cd "$src"
for p in "$here"/patches/*.diff; do
    echo "Applying $(basename "$p")"
    patch -p1 -N < "$p"
done

which cl link nasm
# --enable-cross-compile: configure must not run its test programs, which are linked as UWP
# (-APPCONTAINER WindowsApp.lib) and cannot start on the build machine (same as FFmpegInterop).
# NOTE: deliberately no --enable-gpl / --enable-nonfree / --enable-version3 (LGPL-2.1-or-later).
./configure \
    --prefix="$prefix" \
    --toolchain=msvc --target-os=win32 --arch=x86_64 \
    --enable-cross-compile \
    --enable-static --disable-shared \
    --disable-programs --disable-doc --disable-debug \
    --disable-network --disable-avdevice --disable-avfilter --disable-postproc \
    --disable-everything \
    --enable-avcodec --enable-avformat --enable-avutil --enable-swresample --enable-swscale \
    --enable-protocol=file \
    --enable-demuxer=matroska,ogg \
    --enable-decoder=vp9,opus,vorbis \
    --enable-parser=vp9,opus,vorbis \
    --enable-zlib \
    --enable-w32threads --disable-pthreads \
    --enable-x86asm --x86asmexe=nasm \
    --disable-d3d11va --disable-dxva2 --disable-mediafoundation \
    --disable-iconv --disable-bzlib --disable-lzma --disable-sdl2 --disable-schannel \
    --extra-cflags="-MD -DWINAPI_FAMILY=WINAPI_FAMILY_APP -D_WIN32_WINNT=0x0A00 -I$zinc" \
    --extra-ldflags="-APPCONTAINER WindowsApp.lib -LIBPATH:$zlib" \
    || { tail -n 60 ffbuild/config.log; exit 1; }

grep -E "^(HAVE_UWP|HAVE_WINRT|CONFIG_GPL|CONFIG_VERSION3|CONFIG_NONFREE)" ffbuild/config.mak || true
# The x86-64 SIMD (NASM) code must be on: VP9 decoding is several times slower in plain C.
grep -E "^#define (HAVE_X86ASM|HAVE_SSSE3_EXTERNAL|HAVE_AVX2_EXTERNAL|HAVE_AVX512_EXTERNAL|ARCH_X86_64) " config.h
grep -q "^#define HAVE_X86ASM 1" config.h || { echo "FATAL: FFmpeg configured without x86asm"; exit 1; }
grep -q "^#define HAVE_AVX2_EXTERNAL 1" config.h || { echo "FATAL: FFmpeg configured without AVX2 asm"; exit 1; }
make -j"$(nproc)"
make install

# Static libraries come out as lib*.a (MSVC COFF format); also provide *.lib names for MSBuild.
for l in avcodec avformat avutil swresample swscale; do
    cp "$prefix/lib/lib$l.a" "$prefix/lib/$l.lib"
done
