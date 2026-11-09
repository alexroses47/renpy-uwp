# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

# Static library with Ren'Py's C/Cython modules for UWP (renpy_static.lib) + the inittab that
# registers them as built-in modules. Ren'Py's own sources are MIT-licensed.
cmake_minimum_required(VERSION 3.15)
project(renpy_static C CXX)

set(RENPY_ROOT "" CACHE PATH "Ren'Py source tree (after setup.py generate)")
set(GEN_DIR "" CACHE PATH "output of gen_static.py (modules.cmake, renpy_inittab.c)")
set(PYTHON_PREFIX "" CACHE PATH "CPython UWP prefix (include/)")
set(SDL2_PREFIX "" CACHE PATH "SDL2 prefix (include/SDL2)")
set(MEDIA_PREFIX "" CACHE PATH "media deps prefix (png, jpeg, freetype, harfbuzz, fribidi, ffmpeg)")
set(ASSIMP_PREFIX "" CACHE PATH "assimp prefix (include/assimp)")

include(${GEN_DIR}/modules.cmake)
add_library(renpy_static STATIC ${RENPY_STATIC_SOURCES} ${GEN_DIR}/renpy_inittab.c)
target_include_directories(renpy_static PRIVATE
  ${RENPY_ROOT}/src
  ${RENPY_ROOT}/src/pygame
  ${RENPY_ROOT}/tmp/gen3-static
  ${PYTHON_PREFIX}/include
  ${SDL2_PREFIX}/include
  ${SDL2_PREFIX}/include/SDL2
  ${MEDIA_PREFIX}/include
  ${MEDIA_PREFIX}/include/SDL2
  ${MEDIA_PREFIX}/include/freetype2
  ${MEDIA_PREFIX}/include/harfbuzz
  ${MEDIA_PREFIX}/include/fribidi
  ${ASSIMP_PREFIX}/include
)
target_compile_definitions(renpy_static PRIVATE
  Py_NO_ENABLE_SHARED FRIBIDI_STATIC _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
target_compile_options(renpy_static PRIVATE /sdl- /bigobj $<$<COMPILE_LANGUAGE:C>:/we4013> $<$<COMPILE_LANGUAGE:CXX>:/EHsc>)
install(TARGETS renpy_static ARCHIVE DESTINATION lib)
