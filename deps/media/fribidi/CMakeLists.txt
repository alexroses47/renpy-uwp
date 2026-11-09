# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

# Minimal static build of GNU FriBidi 1.0.7 (LGPL-2.1-or-later) for UWP. Upstream only ships
# meson/autotools; the Unicode tables (*.tab.i) are pre-generated in the release tarball.
cmake_minimum_required(VERSION 3.15)
project(fribidi C)

set(FRIBIDI_SRC "" CACHE PATH "fribidi-1.0.7 source tree")
file(GLOB FRIBIDI_SOURCES ${FRIBIDI_SRC}/lib/*.c)
add_library(fribidi STATIC ${FRIBIDI_SOURCES})
target_include_directories(fribidi PRIVATE ${FRIBIDI_SRC}/lib ${CMAKE_CURRENT_SOURCE_DIR}/include
                                    PUBLIC  ${CMAKE_CURRENT_BINARY_DIR}/include)
# fribidi 1.0.7 spells the static switch FRIBIDI_STATIC (FRIBIDI_LIB_STATIC is from later versions).
target_compile_definitions(fribidi PRIVATE HAVE_CONFIG_H _CRT_SECURE_NO_WARNINGS PUBLIC FRIBIDI_STATIC)

# fribidi-config.h is generated from the upstream template with the values meson would use.
file(READ ${FRIBIDI_SRC}/lib/fribidi-config.h.in cfg)
string(REPLACE "@PACKAGE@" "fribidi" cfg "${cfg}")
string(REPLACE "@PACKAGE_NAME@" "GNU FriBidi" cfg "${cfg}")
string(REPLACE "@PACKAGE_BUGREPORT@" "https://github.com/fribidi/fribidi/issues/new" cfg "${cfg}")
string(REPLACE "@FRIBIDI_VERSION@" "1.0.7" cfg "${cfg}")
string(REPLACE "@FRIBIDI_MAJOR_VERSION@" "1" cfg "${cfg}")
string(REPLACE "@FRIBIDI_MINOR_VERSION@" "0" cfg "${cfg}")
string(REPLACE "@FRIBIDI_MICRO_VERSION@" "7" cfg "${cfg}")
string(REPLACE "@FRIBIDI_INTERFACE_VERSION@" "4" cfg "${cfg}")
string(REPLACE "@SIZEOF_INT@" "4" cfg "${cfg}")
# meson writes "#define FRIBIDI_BUILT_WITH_MSVC" here for MSVC builds.
string(REPLACE "@FRIBIDI_MSVC_BUILD_PLACEHOLDER@" "#define FRIBIDI_BUILT_WITH_MSVC" cfg "${cfg}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/include/fribidi-config.h "${cfg}")
file(COPY ${FRIBIDI_SRC}/lib/fribidi-unicode-version.h DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include)

install(TARGETS fribidi ARCHIVE DESTINATION lib)
file(GLOB FRIBIDI_HEADERS ${FRIBIDI_SRC}/lib/fribidi*.h)
install(FILES ${FRIBIDI_HEADERS}
              ${CMAKE_CURRENT_BINARY_DIR}/include/fribidi-config.h
              ${CMAKE_CURRENT_BINARY_DIR}/include/fribidi-unicode-version.h
        DESTINATION include/fribidi)
