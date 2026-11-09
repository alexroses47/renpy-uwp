# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

# Builds the media dependencies of Ren'Py for UWP x64 (static, MSVC) into $Prefix:
# libpng, libjpeg-turbo, libwebp, freetype (+harfbuzz), harfbuzz, fribidi, SDL2_image, FFmpeg (LGPL).
# Needs: MSVC environment (cl/link in PATH), CMake, NASM in PATH, MSYS2 at C:\msys64 (make, diffutils).
# zlib comes from the CPython build ($ZlibPrefix: include/zlib, lib/zlibstatic.lib).

param(
    [Parameter(Mandatory = $true)][string]$Prefix,
    [Parameter(Mandatory = $true)][string]$ZlibPrefix,
    [Parameter(Mandatory = $true)][string]$SdlPrefix,
    [string]$WorkDir = (Join-Path $env:RUNNER_TEMP 'media-deps')
)

$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
. (Join-Path $here 'version.ps1')

function Invoke-Checked([string]$what, [scriptblock]$cmd) {
    & $cmd
    if ($LASTEXITCODE -ne 0) { throw "$what failed with exit code $LASTEXITCODE" }
}

$tars = New-Item -ItemType Directory -Force (Join-Path $WorkDir 'tars')
$srcRoot = New-Item -ItemType Directory -Force (Join-Path $WorkDir 'src')
$buildRoot = New-Item -ItemType Directory -Force (Join-Path $WorkDir 'build')
New-Item -ItemType Directory -Force (Join-Path $Prefix 'licenses') | Out-Null
$Prefix = (Resolve-Path $Prefix).Path

function Get-Source([string]$key) {
    $d = $MediaDeps[$key]
    $tar = Join-Path $tars $d.file
    if (-not (Test-Path $tar)) {
        Write-Host "Downloading $($d.url)"
        Invoke-WebRequest -Uri $d.url -OutFile $tar -UserAgent 'renpy-uwp-ci'
    }
    $h = (Get-FileHash -Algorithm SHA256 $tar).Hash.ToLowerInvariant()
    if ($h -ne $d.sha) { throw "SHA-256 mismatch for $($d.file): $h" }
    $dir = Join-Path $srcRoot ($d.file -replace '\.tar\.(gz|xz|bz2)$', '')
    if (Test-Path $dir) { Remove-Item -Recurse -Force $dir }
    Invoke-Checked "extract $($d.file)" { tar -xf $tar -C $srcRoot }
    return $dir
}

function Copy-License([string]$src, [string]$name, [string[]]$candidates) {
    foreach ($c in $candidates) {
        $f = Join-Path $src $c
        if (Test-Path $f) { Copy-Item $f (Join-Path $Prefix "licenses\$name-$((Split-Path $c -Leaf))"); }
    }
}

$zinc = Join-Path $ZlibPrefix 'include\zlib'
$zlib = Join-Path $ZlibPrefix 'lib\zlibstatic.lib'
$cflags = '/DWIN32 /D_WINDOWS /W3 /sdl- /D_CRT_SECURE_NO_WARNINGS'
$common = @(
    '-G', 'Visual Studio 17 2022', '-A', 'x64',
    '-DCMAKE_SYSTEM_NAME=WindowsStore', '-DCMAKE_SYSTEM_VERSION=10.0.22621.0',
    "-DCMAKE_INSTALL_PREFIX=$Prefix", "-DCMAKE_PREFIX_PATH=$Prefix",
    '-DBUILD_SHARED_LIBS=OFF', '-DCMAKE_POLICY_VERSION_MINIMUM=3.5',
    "-DCMAKE_C_FLAGS=$cflags", "-DCMAKE_CXX_FLAGS=$cflags /EHsc /GR"
)

function Build-CMake([string]$name, [string]$src, [string[]]$extra, [string]$target = '') {
    $b = Join-Path $buildRoot $name
    if (Test-Path $b) { Remove-Item -Recurse -Force $b }
    Write-Host "=== $name"
    Invoke-Checked "$name configure" { cmake -S $src -B $b @common @extra }
    if ($target) {
        Invoke-Checked "$name build" { cmake --build $b --config Release --parallel --target $target -- /nologo /v:minimal }
    } else {
        Invoke-Checked "$name build" { cmake --build $b --config Release --parallel -- /nologo /v:minimal }
        Invoke-Checked "$name install" { cmake --install $b --config Release }
    }
    return $b
}

# --- zlib for consumers that expect z.lib / zlib.h in the prefix (FFmpeg -lz, FindZLIB) -------
New-Item -ItemType Directory -Force (Join-Path $Prefix 'lib'), (Join-Path $Prefix 'include') | Out-Null
Copy-Item $zlib (Join-Path $Prefix 'lib\z.lib')
Copy-Item (Join-Path $zinc '*.h') (Join-Path $Prefix 'include')
$zlibArgs = @("-DZLIB_INCLUDE_DIR=$(Join-Path $Prefix 'include')", "-DZLIB_LIBRARY=$(Join-Path $Prefix 'lib\z.lib')")

# --- libpng -----------------------------------------------------------------------------------
$s = Get-Source 'libpng'
Build-CMake 'libpng' $s (@('-DPNG_SHARED=OFF', '-DPNG_STATIC=ON', '-DPNG_TESTS=OFF', '-DPNG_EXECUTABLES=OFF',
    '-DPNG_TOOLS=OFF', "-DCMAKE_C_FLAGS=$cflags /DPNG_NO_CONSOLE_IO") + $zlibArgs) | Out-Null
Copy-License $s 'libpng' @('LICENSE')
$pngLib = Get-ChildItem (Join-Path $Prefix 'lib') -Filter 'libpng16*.lib' | Select-Object -First 1
Copy-Item $pngLib.FullName (Join-Path $Prefix 'lib\png.lib')

# --- libjpeg-turbo 1.5.3 (old CMake: build only the static library, install by hand) ------------
$s = Get-Source 'libjpeg'
$b = Build-CMake 'libjpeg' $s @('-DENABLE_SHARED=OFF', '-DENABLE_STATIC=ON', '-DWITH_SIMD=ON', '-DWITH_TURBOJPEG=OFF',
    '-DWITH_JAVA=OFF', '-DWITH_CRT_DLL=ON') 'jpeg-static'
$jlib = Get-ChildItem -Recurse $b -Filter 'jpeg-static.lib' | Select-Object -First 1
Copy-Item $jlib.FullName (Join-Path $Prefix 'lib\jpeg.lib')
foreach ($h in 'jpeglib.h', 'jmorecfg.h', 'jerror.h') { Copy-Item (Join-Path $s $h) (Join-Path $Prefix 'include') }
Copy-Item (Get-ChildItem -Recurse $b -Filter 'jconfig.h' | Select-Object -First 1).FullName (Join-Path $Prefix 'include')
Copy-License $s 'libjpeg-turbo' @('LICENSE.md', 'README.ijg')

# --- libwebp (not used by KS:RE; generic runtime) --------------------------------------------
$s = Get-Source 'libwebp'
Build-CMake 'libwebp' $s @('-DWEBP_BUILD_ANIM_UTILS=OFF', '-DWEBP_BUILD_CWEBP=OFF', '-DWEBP_BUILD_DWEBP=OFF',
    '-DWEBP_BUILD_GIF2WEBP=OFF', '-DWEBP_BUILD_IMG2WEBP=OFF', '-DWEBP_BUILD_VWEBP=OFF', '-DWEBP_BUILD_WEBPINFO=OFF',
    '-DWEBP_BUILD_WEBPMUX=OFF', '-DWEBP_BUILD_EXTRAS=OFF') | Out-Null
Copy-License $s 'libwebp' @('COPYING', 'PATENTS')

# --- freetype (without harfbuzz), harfbuzz (with freetype), freetype again (with harfbuzz) ----
$ftSrc = Get-Source 'freetype'
$pngArgs = @("-DPNG_LIBRARY=$(Join-Path $Prefix 'lib\png.lib')", "-DPNG_PNG_INCLUDE_DIR=$(Join-Path $Prefix 'include')")
$ftCommon = @('-DFT_DISABLE_BROTLI=ON', '-DFT_DISABLE_BZIP2=ON', '-DFT_REQUIRE_ZLIB=ON', '-DFT_REQUIRE_PNG=ON') + $zlibArgs + $pngArgs
Build-CMake 'freetype' $ftSrc (@('-DFT_DISABLE_HARFBUZZ=ON') + $ftCommon) | Out-Null

$s = Get-Source 'harfbuzz'
Build-CMake 'harfbuzz' $s @('-DHB_HAVE_FREETYPE=ON', '-DHB_BUILD_SUBSET=OFF', '-DHB_BUILD_UTILS=OFF',
    "-DFREETYPE_LIBRARY=$(Join-Path $Prefix 'lib\freetype.lib')",
    "-DFREETYPE_INCLUDE_DIRS=$(Join-Path $Prefix 'include\freetype2')",
    "-DFREETYPE_INCLUDE_DIR_freetype2=$(Join-Path $Prefix 'include\freetype2')",
    "-DFREETYPE_INCLUDE_DIR_ft2build=$(Join-Path $Prefix 'include\freetype2')") | Out-Null
Copy-License $s 'harfbuzz' @('COPYING')

Build-CMake 'freetypehb' $ftSrc (@('-DFT_REQUIRE_HARFBUZZ=ON',
    "-DHarfBuzz_INCLUDE_DIR=$(Join-Path $Prefix 'include\harfbuzz')",
    "-DHarfBuzz_LIBRARY=$(Join-Path $Prefix 'lib\harfbuzz.lib')") + $ftCommon) | Out-Null
Copy-License $ftSrc 'freetype' @('LICENSE.TXT', 'docs\FTL.TXT')

# --- fribidi (our CMake) ------------------------------------------------------------------------
$s = Get-Source 'fribidi'
Build-CMake 'fribidi' (Join-Path $here 'fribidi') @("-DFRIBIDI_SRC=$s") | Out-Null
Copy-License $s 'fribidi' @('COPYING')

# --- SDL2_image (our CMake) ---------------------------------------------------------------------
$s = Get-Source 'sdl2_image'
Build-CMake 'sdl2_image' (Join-Path $here 'sdl2_image') @("-DSDL2_IMAGE_SRC=$s", "-DSDL2_DIR_PREFIX=$SdlPrefix",
    "-DDEPS_PREFIX=$Prefix") | Out-Null
Copy-License $s 'SDL2_image' @('LICENSE.txt')

# --- FFmpeg (MSYS2 bash, MSVC toolchain) --------------------------------------------------------
$s = Get-Source 'ffmpeg'
$bash = 'C:\msys64\usr\bin\bash.exe'
$env:MSYS2_PATH_TYPE = 'inherit'
$env:CHERE_INVOKING = '1'
Invoke-Checked 'msys2 packages' { & $bash -lc 'pacman -S --noconfirm --needed make diffutils patch' }
# MSYS2's /usr/bin/link would shadow MSVC's link.exe.
Invoke-Checked 'hide msys link' { & $bash -lc 'test ! -e /usr/bin/link.exe || mv -f /usr/bin/link.exe /usr/bin/link.exe.msys' }
$sh = (Join-Path $here 'ffmpeg\build.sh')
Invoke-Checked 'ffmpeg' { & $bash -lc "'$($sh -replace '\\','/')' '$($s -replace '\\','/')' '$($Prefix -replace '\\','/')' '$($zinc -replace '\\','/')' '$((Split-Path $zlib) -replace '\\','/')'" }
Copy-License $s 'FFmpeg' @('COPYING.LGPLv2.1', 'LICENSE.md')

@{ deps = $MediaDeps } | ConvertTo-Json -Depth 4 | Set-Content -Encoding utf8 (Join-Path $Prefix 'media-deps-build.json')
Get-ChildItem (Join-Path $Prefix 'lib') -File | Select-Object Name, Length | Format-Table -AutoSize | Out-String -Width 200
