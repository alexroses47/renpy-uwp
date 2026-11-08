# This file is derived from CMakeLists.txt of https://github.com/thexai/cpython at commit
# 4c97e13f50ce1f925e5f95945c6d395b83f78a64 ("Kodi patches", Python 3.12.11), which is part of
# that CPython fork and distributed under the PSF License Version 2 (see LICENSE in the CPython
# source tree). renpy-uwp changes (see python/cpython-uwp/README.md):
#   - builds a STATIC library (python312.lib) with the extension modules built in, instead of
#     python312.dll + python3.dll + .pyd files;
#   - no external dependency packages: zlib is compiled from source (ZLIB_SOURCE_DIR), expat and
#     libmpdec are the copies vendored in the CPython tree; _ssl/_hashlib/_bz2/_lzma/_sqlite3/
#     _ctypes/_uuid are not built (milestone 2);
#   - the frozen/deepfreeze generators run with PYTHON_FOR_BUILD (a host Python 3.12) instead of
#     a NuGet download;
#   - /ZW removed (plain C), /we4013 added (undeclared functions are errors), nothing linked here:
#     the final executable links WindowsApp.lib (audit with dumpbin /imports).

cmake_minimum_required(VERSION 3.15)

project(python VERSION 3.12.11 LANGUAGES C CXX)

set(PYTHON_FOR_BUILD "" CACHE FILEPATH "Host Python 3.12 used to generate frozen modules")
set(ZLIB_SOURCE_DIR "" CACHE PATH "zlib source tree (zlib 1.3.1)")
if(NOT PYTHON_FOR_BUILD OR NOT ZLIB_SOURCE_DIR)
  message(FATAL_ERROR "PYTHON_FOR_BUILD and ZLIB_SOURCE_DIR must be set")
endif()

## zlib (static, compiled from source)
add_library(zlibstatic STATIC
  ${ZLIB_SOURCE_DIR}/adler32.c ${ZLIB_SOURCE_DIR}/compress.c ${ZLIB_SOURCE_DIR}/crc32.c
  ${ZLIB_SOURCE_DIR}/deflate.c ${ZLIB_SOURCE_DIR}/gzclose.c ${ZLIB_SOURCE_DIR}/gzlib.c
  ${ZLIB_SOURCE_DIR}/gzread.c ${ZLIB_SOURCE_DIR}/gzwrite.c ${ZLIB_SOURCE_DIR}/infback.c
  ${ZLIB_SOURCE_DIR}/inffast.c ${ZLIB_SOURCE_DIR}/inflate.c ${ZLIB_SOURCE_DIR}/inftrees.c
  ${ZLIB_SOURCE_DIR}/trees.c ${ZLIB_SOURCE_DIR}/uncompr.c ${ZLIB_SOURCE_DIR}/zutil.c
)
target_include_directories(zlibstatic PUBLIC ${ZLIB_SOURCE_DIR})
target_compile_definitions(zlibstatic PRIVATE _CRT_SECURE_NO_DEPRECATE _CRT_NONSTDC_NO_DEPRECATE)
target_compile_options(zlibstatic PRIVATE /we4013)


add_custom_target(gen.frozen
    COMMAND ${PYTHON_FOR_BUILD} ${CMAKE_CURRENT_SOURCE_DIR}/Programs/_freeze_module.py importlib._bootstrap ${CMAKE_CURRENT_SOURCE_DIR}/Lib/importlib/_bootstrap.py ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/importlib._bootstrap.h
    COMMAND ${PYTHON_FOR_BUILD} ${CMAKE_CURRENT_SOURCE_DIR}/Programs/_freeze_module.py importlib._bootstrap_external ${CMAKE_CURRENT_SOURCE_DIR}/Lib/importlib/_bootstrap_external.py ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/importlib._bootstrap_external.h
    COMMAND ${PYTHON_FOR_BUILD} ${CMAKE_CURRENT_SOURCE_DIR}/Programs/_freeze_module.py zipimport ${CMAKE_CURRENT_SOURCE_DIR}/Lib/zipimport.py ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/zipimport.h
    COMMAND ${PYTHON_FOR_BUILD} ${CMAKE_CURRENT_SOURCE_DIR}/Programs/_freeze_module.py abc ${CMAKE_CURRENT_SOURCE_DIR}/Lib/abc.py ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/abc.h
    COMMAND ${PYTHON_FOR_BUILD} ${CMAKE_CURRENT_SOURCE_DIR}/Programs/_freeze_module.py codecs ${CMAKE_CURRENT_SOURCE_DIR}/Lib/codecs.py ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/codecs.h
    COMMAND ${PYTHON_FOR_BUILD} ${CMAKE_CURRENT_SOURCE_DIR}/Programs/_freeze_module.py io ${CMAKE_CURRENT_SOURCE_DIR}/Lib/io.py ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/io.h
    COMMAND ${PYTHON_FOR_BUILD} ${CMAKE_CURRENT_SOURCE_DIR}/Programs/_freeze_module.py _collections_abc ${CMAKE_CURRENT_SOURCE_DIR}/Lib/_collections_abc.py ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/_collections_abc.h
    COMMAND ${PYTHON_FOR_BUILD} ${CMAKE_CURRENT_SOURCE_DIR}/Programs/_freeze_module.py _sitebuiltins ${CMAKE_CURRENT_SOURCE_DIR}/Lib/_sitebuiltins.py ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/_sitebuiltins.h
    COMMAND ${PYTHON_FOR_BUILD} ${CMAKE_CURRENT_SOURCE_DIR}/Programs/_freeze_module.py genericpath ${CMAKE_CURRENT_SOURCE_DIR}/Lib/genericpath.py ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/genericpath.h
    COMMAND ${PYTHON_FOR_BUILD} ${CMAKE_CURRENT_SOURCE_DIR}/Programs/_freeze_module.py ntpath ${CMAKE_CURRENT_SOURCE_DIR}/Lib/ntpath.py ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/ntpath.h
    COMMAND ${PYTHON_FOR_BUILD} ${CMAKE_CURRENT_SOURCE_DIR}/Programs/_freeze_module.py posixpath ${CMAKE_CURRENT_SOURCE_DIR}/Lib/posixpath.py ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/posixpath.h
    COMMAND ${PYTHON_FOR_BUILD} ${CMAKE_CURRENT_SOURCE_DIR}/Programs/_freeze_module.py os ${CMAKE_CURRENT_SOURCE_DIR}/Lib/os.py ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/os.h
    COMMAND ${PYTHON_FOR_BUILD} ${CMAKE_CURRENT_SOURCE_DIR}/Programs/_freeze_module.py site ${CMAKE_CURRENT_SOURCE_DIR}/Lib/site.py ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/site.h
    COMMAND ${PYTHON_FOR_BUILD} ${CMAKE_CURRENT_SOURCE_DIR}/Programs/_freeze_module.py stat ${CMAKE_CURRENT_SOURCE_DIR}/Lib/stat.py ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/stat.h
    COMMAND ${PYTHON_FOR_BUILD} ${CMAKE_CURRENT_SOURCE_DIR}/Programs/_freeze_module.py importlib.util ${CMAKE_CURRENT_SOURCE_DIR}/Lib/importlib/util.py ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/importlib.util.h
    COMMAND ${PYTHON_FOR_BUILD} ${CMAKE_CURRENT_SOURCE_DIR}/Programs/_freeze_module.py importlib.machinery ${CMAKE_CURRENT_SOURCE_DIR}/Lib/importlib/machinery.py ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/importlib.machinery.h
    COMMAND ${PYTHON_FOR_BUILD} ${CMAKE_CURRENT_SOURCE_DIR}/Programs/_freeze_module.py runpy ${CMAKE_CURRENT_SOURCE_DIR}/Lib/runpy.py ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/runpy.h
    COMMAND ${PYTHON_FOR_BUILD} ${CMAKE_CURRENT_SOURCE_DIR}/Programs/_freeze_module.py __hello__ ${CMAKE_CURRENT_SOURCE_DIR}/Lib/__hello__.py ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/__hello__.h
    COMMAND ${PYTHON_FOR_BUILD} ${CMAKE_CURRENT_SOURCE_DIR}/Programs/_freeze_module.py __phello__ ${CMAKE_CURRENT_SOURCE_DIR}/Lib/__phello__/__init__.py ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/__phello__.h
    COMMAND ${PYTHON_FOR_BUILD} ${CMAKE_CURRENT_SOURCE_DIR}/Programs/_freeze_module.py __phello__.ham ${CMAKE_CURRENT_SOURCE_DIR}/Lib/__phello__/ham/__init__.py ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/__phello__.ham.h
    COMMAND ${PYTHON_FOR_BUILD} ${CMAKE_CURRENT_SOURCE_DIR}/Programs/_freeze_module.py __phello__.ham.eggs ${CMAKE_CURRENT_SOURCE_DIR}/Lib/__phello__/ham/eggs.py ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/__phello__.ham.eggs.h
    COMMAND ${PYTHON_FOR_BUILD} ${CMAKE_CURRENT_SOURCE_DIR}/Programs/_freeze_module.py __phello__.spam ${CMAKE_CURRENT_SOURCE_DIR}/Lib/__phello__/spam.py ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/__phello__.spam.h
    COMMAND ${PYTHON_FOR_BUILD} ${CMAKE_CURRENT_SOURCE_DIR}/Programs/_freeze_module.py frozen_only ${CMAKE_CURRENT_SOURCE_DIR}/Tools/freeze/flag.py ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/frozen_only.h
    COMMAND ${PYTHON_FOR_BUILD} ${CMAKE_CURRENT_SOURCE_DIR}/Programs/_freeze_module.py getpath ${CMAKE_CURRENT_SOURCE_DIR}/Modules/getpath.py ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/getpath.h
  BYPRODUCTS
    ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/importlib._bootstrap.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/importlib._bootstrap_external.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/zipimport.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/abc.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/codecs.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/io.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/_collections_abc.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/_sitebuiltins.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/genericpath.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/ntpath.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/posixpath.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/os.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/site.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/stat.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/importlib.util.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/importlib.machinery.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/runpy.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/__hello__.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/__phello__.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/__phello__.ham.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/__phello__.ham.eggs.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/__phello__.spam.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/frozen_only.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/getpath.h
)

add_custom_target(gen.deepfreeze
  COMMAND ${PYTHON_FOR_BUILD} ${CMAKE_CURRENT_SOURCE_DIR}/Tools/build/deepfreeze.py
            ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/importlib._bootstrap.h:importlib._bootstrap
            ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/importlib._bootstrap_external.h:importlib._bootstrap_external
            ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/zipimport.h:zipimport
            ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/abc.h:abc
            ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/codecs.h:codecs
            ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/io.h:io
            ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/_collections_abc.h:_collections_abc
            ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/_sitebuiltins.h:_sitebuiltins
            ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/genericpath.h:genericpath
            ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/ntpath.h:ntpath
            ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/posixpath.h:posixpath
            ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/os.h:os
            ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/site.h:site
            ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/stat.h:stat
            ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/importlib.util.h:importlib.util
            ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/importlib.machinery.h:importlib.machinery
            ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/runpy.h:runpy
            ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/__hello__.h:__hello__
            ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/__phello__.h:__phello__
            ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/__phello__.ham.h:__phello__.ham
            ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/__phello__.ham.eggs.h:__phello__.ham.eggs
            ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/__phello__.spam.h:__phello__.spam
            ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/frozen_only.h:frozen_only
            ${CMAKE_CURRENT_SOURCE_DIR}/Python/frozen_modules/getpath.h:getpath
            -o ${CMAKE_CURRENT_SOURCE_DIR}/Python/deepfreeze/deepfreeze.c
  BYPRODUCTS
    ${CMAKE_CURRENT_SOURCE_DIR}/Python/deepfreeze/deepfreeze.c
)

#ReleaseLevelNumber  -   10 for alpha, 11 for beta, 12 for RC (gamma), and 15 for final
set(RELEASELEVELNUMBER 15)
set(RELEASESERIAL 0)

#MicroVersionNumber*1000 + ReleaseLevelNumber*10 + ReleaseSerial
math(EXPR FIELD3 "${PROJECT_VERSION_PATCH}*1000 + ${RELEASELEVELNUMBER}*10 + ${RELEASESERIAL}")
set(MS_DLL_ID "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")

configure_file(
  cmake/pythonnt_rc.h.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/pythonnt_rc.h
)

set(CMAKE_C_FLAGS_DEBUG "/Zi /Ob0 /Od")
set(CMAKE_CXX_FLAGS_DEBUG "/Zi /Ob0 /Od")

set(python_headers
    Include/abstract.h
    Include/boolobject.h
    Include/bytearrayobject.h
    Include/bytesobject.h
    Include/ceval.h
    Include/codecs.h
    Include/compile.h
    Include/complexobject.h
    Include/datetime.h
    Include/descrobject.h
    Include/dictobject.h
    Include/dynamic_annotations.h
    Include/enumobject.h
    Include/errcode.h
    Include/fileobject.h
    Include/fileutils.h
    Include/floatobject.h
    Include/frameobject.h
    Include/import.h
    Include/interpreteridobject.h
    Include/intrcheck.h
    Include/iterobject.h
    Include/listobject.h
    Include/longobject.h
    Include/marshal.h
    Include/memoryobject.h
    Include/methodobject.h
    Include/modsupport.h
    Include/moduleobject.h
    Include/object.h
    Include/objimpl.h
    Include/opcode.h
    Include/osdefs.h
    Include/osmodule.h
    Include/patchlevel.h
    Include/py_curses.h
    Include/pybuffer.h
    Include/pycapsule.h
    Include/pyerrors.h
    Include/pyexpat.h
    Include/pyframe.h
    Include/pyhash.h
    Include/pylifecycle.h
    Include/pymath.h
    Include/pymacro.h
    Include/pymem.h
    Include/pyport.h
    Include/pystate.h
    Include/pystats.h
    Include/pystrcmp.h
    Include/pystrtod.h
    Include/Python.h
    Include/pythonrun.h
    Include/pythread.h
    Include/pytypedefs.h
    Include/rangeobject.h
    Include/setobject.h
    Include/sliceobject.h
    Include/structmember.h
    Include/structseq.h
    Include/sysmodule.h
    Include/traceback.h
    Include/tracemalloc.h
    Include/tupleobject.h
    Include/unicodeobject.h
    Include/weakrefobject.h
    Include/cpython/abstract.h
    Include/cpython/bytearrayobject.h
    Include/cpython/bytesobject.h
    Include/cpython/cellobject.h
    Include/cpython/ceval.h
    Include/cpython/classobject.h
    Include/cpython/code.h
    Include/cpython/compile.h
    Include/cpython/complexobject.h
    Include/cpython/context.h
    Include/cpython/descrobject.h
    Include/cpython/dictobject.h
    Include/cpython/fileobject.h
    Include/cpython/fileutils.h
    Include/cpython/floatobject.h
    Include/cpython/frameobject.h
    Include/cpython/funcobject.h
    Include/cpython/genobject.h
    Include/cpython/import.h
    Include/cpython/initconfig.h
    Include/cpython/interpreteridobject.h
    Include/cpython/listobject.h
    Include/cpython/longintrepr.h
    Include/cpython/longobject.h
    Include/cpython/memoryobject.h
    Include/cpython/methodobject.h
    Include/cpython/modsupport.h
    Include/cpython/object.h
    Include/cpython/objimpl.h
    Include/cpython/odictobject.h
    Include/cpython/picklebufobject.h
    Include/cpython/pyctype.h
    Include/cpython/pydebug.h
    Include/cpython/pyerrors.h
    Include/cpython/pyfpe.h
    Include/cpython/pyframe.h
    Include/cpython/pylifecycle.h
    Include/cpython/pymem.h
    Include/cpython/pystate.h
    Include/cpython/pythonrun.h
    Include/cpython/pythread.h
    Include/cpython/pytime.h
    Include/cpython/setobject.h
    Include/cpython/sysmodule.h
    Include/cpython/traceback.h
    Include/cpython/tupleobject.h
    Include/cpython/unicodeobject.h
    Include/cpython/warnings.h
    Include/cpython/weakrefobject.h
    Include/internal/pycore_abstract.h
    Include/internal/pycore_asdl.h
    Include/internal/pycore_ast.h
    Include/internal/pycore_ast_state.h
    Include/internal/pycore_atexit.h
    Include/internal/pycore_atomic.h
    Include/internal/pycore_atomic_funcs.h
    Include/internal/pycore_bitutils.h
    Include/internal/pycore_bytes_methods.h
    Include/internal/pycore_bytesobject.h
    Include/internal/pycore_call.h
    Include/internal/pycore_ceval.h
    Include/internal/pycore_ceval_state.h
    Include/internal/pycore_code.h
    Include/internal/pycore_compile.h
    Include/internal/pycore_condvar.h
    Include/internal/pycore_context.h
    Include/internal/pycore_descrobject.h
    Include/internal/pycore_dict.h
    Include/internal/pycore_dict_state.h
    Include/internal/pycore_dtoa.h
    Include/internal/pycore_exceptions.h
    Include/internal/pycore_faulthandler.h
    Include/internal/pycore_fileutils.h
    Include/internal/pycore_fileutils_windows.h
    Include/internal/pycore_floatobject.h
    Include/internal/pycore_format.h
    Include/internal/pycore_frame.h
    Include/internal/pycore_function.h
    Include/internal/pycore_gc.h
    Include/internal/pycore_genobject.h
    Include/internal/pycore_getopt.h
    Include/internal/pycore_gil.h
    Include/internal/pycore_global_objects.h
    Include/internal/pycore_global_objects_fini_generated.h
    Include/internal/pycore_hamt.h
    Include/internal/pycore_hashtable.h
    Include/internal/pycore_import.h
    Include/internal/pycore_initconfig.h
    Include/internal/pycore_interp.h
    Include/internal/pycore_intrinsics.h
    Include/internal/pycore_list.h
    Include/internal/pycore_long.h
    Include/internal/pycore_moduleobject.h
    Include/internal/pycore_namespace.h
    Include/internal/pycore_object.h
    Include/internal/pycore_object_state.h
    Include/internal/pycore_obmalloc.h
    Include/internal/pycore_obmalloc_init.h
    Include/internal/pycore_pathconfig.h
    Include/internal/pycore_pyarena.h
    Include/internal/pycore_pyerrors.h
    Include/internal/pycore_pyhash.h
    Include/internal/pycore_pylifecycle.h
    Include/internal/pycore_pymem.h
    Include/internal/pycore_pymem_init.h
    Include/internal/pycore_pystate.h
    Include/internal/pycore_pythread.h
    Include/internal/pycore_range.h
    Include/internal/pycore_runtime.h
    Include/internal/pycore_runtime_init.h
    Include/internal/pycore_runtime_init_generated.h
    Include/internal/pycore_signal.h
    Include/internal/pycore_sliceobject.h
    Include/internal/pycore_strhex.h
    Include/internal/pycore_structseq.h
    Include/internal/pycore_sysmodule.h
    Include/internal/pycore_symtable.h
    Include/internal/pycore_time.h
    Include/internal/pycore_token.h
    Include/internal/pycore_traceback.h
    Include/internal/pycore_tracemalloc.h
    Include/internal/pycore_tuple.h
    Include/internal/pycore_typeobject.h
    Include/internal/pycore_typevarobject.h
    Include/internal/pycore_ucnhash.h
    Include/internal/pycore_unionobject.h
    Include/internal/pycore_unicodeobject.h
    Include/internal/pycore_unicodeobject_generated.h
    Include/internal/pycore_warnings.h
    Modules/_math.h
    Modules/rotatingtree.h
    Modules/_io/_iomodule.h
    Modules/_hacl/internal/Hacl_Hash_MD5.h
    Modules/_hacl/internal/Hacl_Hash_SHA1.h
    Modules/_hacl/internal/Hacl_Hash_SHA2.h
    Modules/_hacl/internal/Hacl_Hash_SHA3.h
    Modules/_sre/sre_constants.h
    Modules/_sre/sre_lib.h
    Modules/_sre/sre.h
    Modules/cjkcodecs/alg_jisx0201.h
    Modules/cjkcodecs/cjkcodecs.h
    Modules/cjkcodecs/emu_jisx0213_2000.h
    Modules/cjkcodecs/mappings_cn.h
    Modules/cjkcodecs/mappings_hk.h
    Modules/cjkcodecs/mappings_jisx0213_pair.h
    Modules/cjkcodecs/mappings_jp.h
    Modules/cjkcodecs/mappings_kr.h
    Modules/cjkcodecs/mappings_tw.h
    Modules/cjkcodecs/multibytecodec.h
    Objects/stringlib/count.h
    Objects/stringlib/fastsearch.h
    Objects/stringlib/find.h
    Objects/stringlib/partition.h
    Objects/stringlib/replace.h
    Objects/stringlib/split.h
    Objects/unicodetype_db.h
    Parser/pegen.h
    Parser/string_parser.h
    Parser/tokenizer.h
    PC/errmap.h
    PC/pyconfig.h
    Python/condvar.h
    Python/importdl.h
    Python/stdlib_module_names.h
    Python/thread_nt.h
)

set(DECIMAL_SOURCES
  Modules/_decimal/_decimal.c
  Modules/_decimal/libmpdec/basearith.c
  Modules/_decimal/libmpdec/constants.c
  Modules/_decimal/libmpdec/context.c
  Modules/_decimal/libmpdec/convolute.c
  Modules/_decimal/libmpdec/crt.c
  Modules/_decimal/libmpdec/difradix2.c
  Modules/_decimal/libmpdec/fnt.c
  Modules/_decimal/libmpdec/fourstep.c
  Modules/_decimal/libmpdec/io.c
  Modules/_decimal/libmpdec/mpalloc.c
  Modules/_decimal/libmpdec/mpdecimal.c
  Modules/_decimal/libmpdec/numbertheory.c
  Modules/_decimal/libmpdec/sixstep.c
  Modules/_decimal/libmpdec/transpose.c
)
# libmpdec has an io.h that must not shadow the CRT one: include path per source only.
# CONFIG_64 + ANSI: portable C, no MASM helpers (same choice as the fork for x64).
set_source_files_properties(${DECIMAL_SOURCES} PROPERTIES
  INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/Modules/_decimal/libmpdec
  COMPILE_DEFINITIONS "CONFIG_64;ANSI;_CRT_SECURE_NO_WARNINGS")
set_source_files_properties(
  Modules/pyexpat.c Modules/_elementtree.c
  Modules/expat/xmlparse.c Modules/expat/xmlrole.c Modules/expat/xmltok.c
  PROPERTIES
  INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/Modules/expat
  COMPILE_DEFINITIONS "XML_STATIC;PYEXPAT_EXPORTS;_CRT_SECURE_NO_WARNINGS")

add_library(python STATIC
  ${python_headers}
  Modules/_abc.c
  Modules/_bisectmodule.c
  Modules/_blake2/blake2module.c
  Modules/_blake2/blake2b_impl.c
  Modules/_blake2/blake2s_impl.c
  Modules/_codecsmodule.c
  Modules/_collectionsmodule.c
  Modules/_contextvarsmodule.c
  Modules/_csv.c
  Modules/_functoolsmodule.c
  Modules/_hacl/Hacl_Hash_MD5.c
  Modules/_hacl/Hacl_Hash_SHA1.c
  Modules/_hacl/Hacl_Hash_SHA2.c
  Modules/_hacl/Hacl_Hash_SHA3.c
  Modules/_heapqmodule.c
  Modules/_json.c
  Modules/_localemodule.c
  Modules/_lsprof.c
  Modules/_pickle.c
  Modules/_randommodule.c
  Modules/_sre/sre.c
  Modules/_stat.c
  Modules/_struct.c
  Modules/_weakref.c
  Modules/arraymodule.c
  Modules/atexitmodule.c
  Modules/audioop.c
  Modules/binascii.c
  Modules/cmathmodule.c
  Modules/_datetimemodule.c
  Modules/errnomodule.c
  Modules/faulthandler.c
  Modules/gcmodule.c
  Modules/getbuildinfo.c
  Modules/itertoolsmodule.c
  Modules/main.c
  Modules/mathmodule.c
  Modules/md5module.c
  Modules/mmapmodule.c
  Modules/_opcode.c
  Modules/_operator.c
  Modules/posixmodule.c
  Modules/rotatingtree.c
  Modules/sha1module.c
  Modules/sha2module.c
  Modules/sha3module.c
  Modules/signalmodule.c
  Modules/_statisticsmodule.c
  Modules/symtablemodule.c
  Modules/_threadmodule.c
  Modules/_tracemalloc.c
  Modules/_typingmodule.c
  Modules/timemodule.c
  Modules/xxsubtype.c
  Modules/_xxsubinterpretersmodule.c
  Modules/_xxinterpchannelsmodule.c
  Modules/_io/fileio.c
  Modules/_io/bytesio.c
  Modules/_io/stringio.c
  Modules/_io/bufferedio.c
  Modules/_io/iobase.c
  Modules/_io/textio.c
  Modules/_io/winconsoleio.c
  Modules/_io/_iomodule.c
  Modules/cjkcodecs/_codecs_cn.c
  Modules/cjkcodecs/_codecs_hk.c
  Modules/cjkcodecs/_codecs_iso2022.c
  Modules/cjkcodecs/_codecs_jp.c
  Modules/cjkcodecs/_codecs_kr.c
  Modules/cjkcodecs/_codecs_tw.c
  Modules/cjkcodecs/multibytecodec.c
  Modules/_winapi.c
  Objects/abstract.c
  Objects/boolobject.c
  Objects/bytearrayobject.c
  Objects/bytes_methods.c
  Objects/bytesobject.c
  Objects/call.c
  Objects/capsule.c
  Objects/cellobject.c
  Objects/classobject.c
  Objects/codeobject.c
  Objects/complexobject.c
  Objects/descrobject.c
  Objects/dictobject.c
  Objects/enumobject.c
  Objects/exceptions.c
  Objects/fileobject.c
  Objects/floatobject.c
  Objects/frameobject.c
  Objects/funcobject.c
  Objects/genericaliasobject.c
  Objects/genobject.c
  Objects/interpreteridobject.c
  Objects/iterobject.c
  Objects/listobject.c
  Objects/longobject.c
  Objects/memoryobject.c
  Objects/methodobject.c
  Objects/moduleobject.c
  Objects/namespaceobject.c
  Objects/object.c
  Objects/obmalloc.c
  Objects/odictobject.c
  Objects/picklebufobject.c
  Objects/rangeobject.c
  Objects/setobject.c
  Objects/sliceobject.c
  Objects/structseq.c
  Objects/tupleobject.c
  Objects/typeobject.c
  Objects/typevarobject.c
  Objects/unicodectype.c
  Objects/unicodeobject.c
  Objects/unionobject.c
  Objects/weakrefobject.c
  Parser/myreadline.c
  Parser/tokenizer.c
  Parser/token.c
  Parser/pegen.c
  Parser/pegen_errors.c
  Parser/action_helpers.c
  Parser/parser.c
  Parser/string_parser.c
  Parser/peg_api.c
  PC/invalid_parameter_handler.c
  PC/winreg.c
  PC/config.c
  PC/msvcrtmodule.c
  Python/pyhash.c
  Python/_warnings.c
  Python/asdl.c
  Python/assemble.c
  Python/ast.c
  Python/ast_opt.c
  Python/ast_unparse.c
  Python/bltinmodule.c
  Python/bootstrap_hash.c
  Python/ceval.c
  Python/codecs.c
  Python/compile.c
  Python/context.c
  Python/dynamic_annotations.c
  Python/dynload_win.c
  Python/errors.c
  Python/fileutils.c
  Python/flowgraph.c
  Python/formatter_unicode.c
  Python/frame.c
  Python/frozen.c
  Python/future.c
  Python/getargs.c
  Python/getcompiler.c
  Python/getcopyright.c
  Python/getopt.c
  Python/getplatform.c
  Python/getversion.c
  Python/ceval_gil.c
  Python/hamt.c
  Python/hashtable.c
  Python/import.c
  Python/importdl.c
  Python/initconfig.c
  Python/intrinsics.c
  Python/instrumentation.c
  Python/legacy_tracing.c
  Python/marshal.c
  Python/modsupport.c
  Python/mysnprintf.c
  Python/mystrtoul.c
  Python/pathconfig.c
  Python/perf_trampoline.c
  Python/preconfig.c
  Python/pyarena.c
  Python/pyctype.c
  Python/pyfpe.c
  Python/pylifecycle.c
  Python/pymath.c
  Python/pytime.c
  Python/pystate.c
  Python/pystrcmp.c
  Python/pystrhex.c
  Python/pystrtod.c
  Python/dtoa.c
  Python/Python-ast.c
  Python/Python-tokenize.c
  Python/pythonrun.c
  Python/specialize.c
  Python/suggestions.c
  Python/structmember.c
  Python/symtable.c
  Python/sysmodule.c
  Python/thread.c
  Python/traceback.c
  Python/tracemalloc.c
  Python/deepfreeze/deepfreeze.c
  Modules/zlibmodule.c
  PC/dl_nt.c
  Modules/getpath.c
  # ---- renpy-uwp: extension modules built into the library (registered in PC/config.c) ----
  Modules/_asynciomodule.c
  Modules/_queuemodule.c
  Modules/selectmodule.c
  Modules/unicodedata.c
  Modules/socketmodule.c
  Modules/overlapped.c
  Modules/_multiprocessing/multiprocessing.c
  Modules/_multiprocessing/semaphore.c
  Modules/_zoneinfo.c
  Modules/pyexpat.c
  Modules/_elementtree.c
  Modules/expat/xmlparse.c
  Modules/expat/xmlrole.c
  Modules/expat/xmltok.c
  ${DECIMAL_SOURCES}
)

target_compile_definitions(python
  PUBLIC
  Py_NO_ENABLE_SHARED
  PRIVATE
  Py_BUILD_CORE
  Py_BUILD_CORE_BUILTIN
  _Py_HAVE_ZLIB
  MS_DLL_ID="${MS_DLL_ID}"
  "PY3_DLLNAME=L\"python3\""
  VPATH="${CMAKE_CURRENT_SOURCE_DIR}"
  PREFIX=NULL
  EXEC_PREFIX=NULL
  PLATLIBDIR="DLLs"
  VERSION="${MS_DLL_ID}"
  _WIN32_WINNT=0x0A00
  WINVER=0x0A00
  NTDDI_VERSION=0x0A000007
  $<$<CONFIG:Debug>:DEBUG>
  $<$<CONFIG:RelWithDebInfo>:NDEBUG>
)

target_include_directories(python PRIVATE
    Include/
    Include/cpython
    Include/internal
    Modules/_hacl/include
    PC/
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_options(python PRIVATE
  /MP
  /sdl-
  /we4013
)

set(PY_SHARED_LIB_NAME ${PROJECT_NAME}${PROJECT_VERSION_MAJOR}${PROJECT_VERSION_MINOR})
set_target_properties(python PROPERTIES
  OUTPUT_NAME ${PY_SHARED_LIB_NAME}
  COMPILE_PDB_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}
  COMPILE_PDB_NAME ${PY_SHARED_LIB_NAME}
)

target_link_libraries(python PUBLIC zlibstatic)

add_dependencies(gen.deepfreeze gen.frozen)
add_dependencies(python gen.deepfreeze)


install(TARGETS python zlibstatic
  ARCHIVE DESTINATION lib
)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Include/
  DESTINATION include
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/PC/pyconfig.h
  DESTINATION include
)
install(FILES ${ZLIB_SOURCE_DIR}/zlib.h ${ZLIB_SOURCE_DIR}/zconf.h
  DESTINATION include/zlib
)
