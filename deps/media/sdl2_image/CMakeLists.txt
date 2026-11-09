# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

# Static build of SDL2_image 2.6.2 (zlib license) for UWP, against our SDL2 (vcxproj build, no
# SDL2Config.cmake) and our libpng/libjpeg/libwebp. Formats: the ones renpy-build leaves enabled,
# without AVIF (not used by KS:RE; see docs/RESEARCH.md section 26).
cmake_minimum_required(VERSION 3.15)
project(SDL2_image C)

set(SDL2_IMAGE_SRC "" CACHE PATH "SDL2_image-2.6.2 source tree")
set(SDL2_DIR_PREFIX "" CACHE PATH "SDL2 install prefix (include/SDL2, lib/SDL2.lib)")
set(DEPS_PREFIX "" CACHE PATH "prefix with libpng, libjpeg, libwebp")

file(GLOB IMG_SOURCES ${SDL2_IMAGE_SRC}/IMG*.c)
list(FILTER IMG_SOURCES EXCLUDE REGEX "IMG_(WIC|xxx)\\.c$")
add_library(SDL2_image STATIC ${IMG_SOURCES})
target_include_directories(SDL2_image PRIVATE
  ${SDL2_IMAGE_SRC} ${SDL2_IMAGE_SRC}/include ${SDL2_DIR_PREFIX}/include/SDL2 ${DEPS_PREFIX}/include)
target_compile_definitions(SDL2_image PRIVATE
  LOAD_BMP LOAD_GIF LOAD_JPG LOAD_PNG LOAD_PNM LOAD_SVG LOAD_TGA LOAD_XPM LOAD_XV LOAD_WEBP
  _CRT_SECURE_NO_WARNINGS)
target_compile_options(SDL2_image PRIVATE /sdl- /we4013)

install(TARGETS SDL2_image ARCHIVE DESTINATION lib)
if(EXISTS ${SDL2_IMAGE_SRC}/include/SDL_image.h)
  install(FILES ${SDL2_IMAGE_SRC}/include/SDL_image.h DESTINATION include/SDL2)
else()
  install(FILES ${SDL2_IMAGE_SRC}/SDL_image.h DESTINATION include/SDL2)
endif()
