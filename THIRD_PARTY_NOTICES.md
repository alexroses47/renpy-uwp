# Third-party notices

This repository contains only the **runtime** (a UWP host for Ren'Py 8). It does not contain, and must never
contain, assets or scripts of any game. Games, including *Katawa Shoujo: Re-Engineered* (art/audio/text under
CC BY-NC-ND 4.0, code under MPL-2.0), are provided by the user from their own copy.

## Licensing of this project vs. third-party code

- **Original code of this runtime** (files carrying the MPL-2.0 Exhibit A header): Mozilla Public License 2.0,
  see `LICENSE`.
- **Third-party code keeps its original license.** This applies to the components listed below, to any copy of
  their sources fetched or vendored by the build, and to **our patches and modifications to third-party files**,
  which are distributed under the license of the file they modify. For example:
  Ren'Py → MIT; CPython → PSF-2.0; SDL2 / SDL2_image → zlib; FFmpeg → LGPL-2.1-or-later;
  FriBidi → LGPL-2.1-or-later; FreeType → FTL; HarfBuzz → MIT; ANGLE → BSD-3-Clause; zlib → zlib; libpng → libpng.
  The MPL-2.0 header is **not** added to third-party files or patches.
- Patches live under `deps/<component>/patches/` (and equivalents); each directory states the license that applies.

## Components currently redistributed in the package

| Component | Version | License | How it is used |
|---|---|---|---|
| CPython (`python312.lib` linked into `RenpyUWP.exe`, `python/python312.zip`) | 3.12.11 = `thexai/cpython@4c97e13` + `python/cpython-uwp/patches` | PSF-2.0 — `licenses/CPython-LICENSE.txt` (includes the licenses of vendored expat, libmpdec, HACL*) | Embedded interpreter |
| zlib | 1.3.1 (release tarball, SHA-256 `9a93b2b7…df23`, same as renpy-build) | zlib — `licenses/zlib-LICENSE.txt` | Compiled into `zlibstatic.lib` by `python/cpython-uwp` |
| CPython build file `python/cpython-uwp/CMakeLists.txt` | derived from `thexai/cpython@4c97e13` | PSF-2.0 | Adapted for a static library (see its header) |
| SDL2 (`SDL2.dll`, WinRT backend) | 2.0.20 (release tarball, SHA-256 `c56aba1d…dd06`, unmodified) | zlib — full text shipped in the artifact as `licenses/SDL2-LICENSE.txt` | Window, events, gamepad (`deps/sdl2/build.ps1`) |
| Ren'Py (`python/renpy.zip` + C/Cython modules in `renpy_static.lib`) | 8.5.3.26051504 (tag), plus `renpy/uwp/patches/0001-msvc-compat.diff` | MIT (bundled pygame_sdl2 code: zlib/LGPL headers as in Ren'Py's own notices) — `licenses/renpy-license.rst` | Engine; milestone 3 ships `_renpy`, `renpy.audio.*`, `renpy.pygame.*` |
| Ren'Py `sdk-fonts` (Source Han Sans Lite, Vazirmatn) in `renpy-base/sdk-fonts` | from the Ren'Py 8.5.3 tag | SIL Open Font License 1.1 (declared in the font files) | Fonts used by SDK sample games (The Question's language buttons) |
| SDL2_image | 2.6.2 (release tarball, unmodified) | zlib | Image loading for pygame_sdl2 (static) |
| libpng | 1.6.43 | libpng license | static |
| libjpeg-turbo | 1.5.3 | IJG + BSD-3-Clause | static |
| libwebp | 1.3.2 | BSD-3-Clause | static |
| FFmpeg (avformat, avcodec, avutil, swresample, swscale) | 4.3.1 + renpy-build's two patches; only matroska/ogg demuxers and vp9/opus/vorbis decoders | **LGPL-2.1-or-later**; built without `--enable-gpl`, `--enable-nonfree` or `--enable-version3` | linked statically into `RenpyUWP.exe` under LGPL-2.1 §6(a) (see below) |
| ANGLE (`libEGL.dll`, `libGLESv2.dll`) | `SternXD/angle` commit `45b0b1e0` (winuwp, D3D11 only, MSVC) | BSD-3-Clause — `licenses/ANGLE-LICENSE.txt` | OpenGL ES 3.0 on Direct3D 11 for Ren'Py's renderer |
| assimp | 5.4.3 (glTF + STL importers, static) | BSD-3-Clause — `licenses/assimp-LICENSE` | Required by `renpy.gl2.assimp` |
| FreeType, HarfBuzz, FriBidi | 2.13.3, 8.0.1, 1.0.7 (static) | FTL; MIT ("Old MIT"); LGPL-2.1-or-later (§6(a), as FFmpeg) | Ren'Py text rendering and bidi |
| Python packages in `python/site-packages.zip` | future 1.0.0, six 1.16.0, rsa 4.9, pyasn1 0.6.1, ecdsa 0.19.0, urllib3 2.7.0, charset-normalizer 3.3.2, certifi 2024.8.30, idna 3.8, requests 2.33.0, websockets 12.0, PySocks 1.7.1, pefile 2022.5.30 | MIT, Apache-2.0, BSD, MPL-2.0 (certifi) — `licenses/<package>-*` | Same set as renpy-build ships with Ren'Py |
| `d3dcompiler_47.dll` | Windows SDK 10.0.22621 `Redist\D3D\x64` | Microsoft Windows SDK redistributable terms | HLSL compiler used by ANGLE's D3D11 backend |
| Microsoft Visual C++ UWP runtime (`Microsoft.VCLibs.x64.14.00.appx`) | 14.0 (from the Windows SDK on the CI runner) | Microsoft Visual Studio redistributable terms | Framework dependency package installed next to the app |

**LGPL-2.1 compliance for FFmpeg (static linking, decided 2026-11-09).** FFmpeg is linked statically into
`RenpyUWP.exe`. Under LGPL-2.1 §6(a) this project makes available, together with every binary it distributes:
- the complete corresponding FFmpeg source: the upstream 4.3.1 tarball (URL and SHA-256 in `deps/media/version.ps1`),
  the two patches in `deps/media/ffmpeg/patches/` and the exact configure line in `deps/media/ffmpeg/build.sh`;
- the complete source of the application that links it (this repository, MPL-2.0) and the CI recipe
  (`.github/workflows/build-host.yml`), so anyone can rebuild and relink `RenpyUWP.exe` against a modified FFmpeg
  (set `MediaDir` to their own build);
- the LGPL text (`licenses/FFmpeg-COPYING.LGPLv2.1`) and this notice in the build artifact.
Nothing in this project's license forbids modifying the application or reverse engineering it to debug such
modifications. FriBidi (LGPL-2.1) is linked the same way and under the same terms.

The placeholder logos in `host/RenpyUWP/Assets/` are generated by `host/tools/make_placeholder_assets.py`
and are original to this project.

## Reference material (not copied, not redistributed)

| Project | License | Use |
|---|---|---|
| `xbmc/kodi-deps` (Kodi dependency build recipes) | **none declared** (no LICENSE file) | Read only as a reference for building CPython for UWP. No code is copied from it. |
| `xbmc/xbmc` (Kodi) | GPL-2.0-or-later | Read only (how Kodi initializes Python on UWP). No code is copied from it. |

## Build-time tools (not redistributed)

| Tool | License |
|---|---|
| openappx 0.7.0 (deploy from Linux) | see https://github.com/gianlucamazza/openappx |
| Windows SDK 10.0.22621.0 / MSVC v143 / C++/WinRT headers | Microsoft SDK license |

## Not included (part of renpy-build, not needed so far)

Versions follow `renpy-build` tag `renpy-8.5.3.26051504` unless noted in `docs/RESEARCH.md`.

| Component | renpy-build version | License | Notes |
|---|---|---|---|
| bzip2 | 1.0.8 | bzip2 license (BSD-style) | |
| xz / liblzma | 5.4.2 | 0BSD / public domain | |
| libffi | 3.4.5 | MIT | |
| OpenSSL | 3.3.2 | Apache-2.0 | |
| libavif / aom / libyuv | v0.11.1 / v3.5.0 / pinned commit | BSD-2 / BSD-2 + AOM patent / BSD-3 | only if needed |

The license texts of everything that is shipped are in the release's `licenses/` folder.
