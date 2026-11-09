# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

# Builds assimp 5.4.3 (BSD-3-Clause) as a static library for UWP x64, with the options of renpy-build
# (tasks/assimp.py): no export, no tests, glTF + STL importers only, external zlib. Ren'Py imports
# renpy.gl2.assimp unconditionally (renpy/__init__.py), so the module and this library are required.
# renpy-build's patches/assimp.diff only concerns Android/MinGW and is not needed with MSVC.
# Output: $Prefix/lib/assimp.lib, $Prefix/include/assimp/, $Prefix/licenses/assimp-LICENSE
param(
    [Parameter(Mandatory = $true)][string]$Prefix,
    [Parameter(Mandatory = $true)][string]$MediaPrefix,   # z.lib and zlib.h
    [string]$WorkDir = (Join-Path $env:RUNNER_TEMP 'assimp')
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'version.ps1')
function Invoke-Checked([string]$what, [scriptblock]$cmd) {
    & $cmd
    if ($LASTEXITCODE -ne 0) { throw "$what failed with exit code $LASTEXITCODE" }
}

New-Item -ItemType Directory -Force $WorkDir, $Prefix | Out-Null
$Prefix = (Resolve-Path $Prefix).Path
$tar = Join-Path $WorkDir "assimp-$AssimpVersion.tar.gz"
if (-not (Test-Path $tar)) {
    Invoke-Checked 'download' { curl.exe -fsSL --retry 4 --retry-delay 5 --retry-all-errors --connect-timeout 30 --max-time 900 -A 'renpy-uwp-ci' -o $tar $AssimpUrl }
}
$h = (Get-FileHash -Algorithm SHA256 $tar).Hash.ToLowerInvariant()
if ($h -ne $AssimpSha256) { throw "SHA-256 mismatch for assimp: $h" }
$src = Join-Path $WorkDir "assimp-$AssimpVersion"
if (Test-Path $src) { Remove-Item -Recurse -Force $src }
Invoke-Checked 'extract' { python -I -c "import sys, tarfile; tarfile.open(sys.argv[1]).extractall(sys.argv[2], filter='data')" $tar $WorkDir }

$b = Join-Path $WorkDir 'build'
if (Test-Path $b) { Remove-Item -Recurse -Force $b }
$cflags = '/DWIN32 /D_WINDOWS /W3 /sdl- /D_CRT_SECURE_NO_WARNINGS'
Invoke-Checked 'configure' {
    cmake -S $src -B $b -G 'Visual Studio 17 2022' -A x64 `
        '-DCMAKE_SYSTEM_NAME=WindowsStore' '-DCMAKE_SYSTEM_VERSION=10.0.22621.0' `
        "-DCMAKE_INSTALL_PREFIX=$Prefix" '-DCMAKE_POLICY_VERSION_MINIMUM=3.5' `
        "-DCMAKE_C_FLAGS=$cflags" "-DCMAKE_CXX_FLAGS=$cflags /EHsc /GR" `
        '-DCMAKE_CXX_STANDARD=17' '-DBUILD_SHARED_LIBS=OFF' '-DASSIMP_NO_EXPORT=ON' `
        '-DASSIMP_BUILD_TESTS=OFF' '-DASSIMP_BUILD_ASSIMP_TOOLS=OFF' '-DASSIMP_BUILD_SAMPLES=OFF' `
        '-DASSIMP_WARNINGS_AS_ERRORS=OFF' '-DASSIMP_INSTALL_PDB=OFF' `
        '-DASSIMP_BUILD_ZLIB=OFF' "-DZLIB_INCLUDE_DIR=$(Join-Path $MediaPrefix 'include')" `
        "-DZLIB_LIBRARY=$(Join-Path $MediaPrefix 'lib\z.lib')" `
        '-DASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT=OFF' '-DASSIMP_BUILD_GLTF_IMPORTER=ON' '-DASSIMP_BUILD_STL_IMPORTER=ON' | Out-Host
}
Invoke-Checked 'build' { cmake --build $b --config Release --parallel -- /nologo /v:minimal | Out-Host }
Invoke-Checked 'install' { cmake --install $b --config Release | Out-Host }

# MSVC names it assimp-vc143-mt.lib; give the host a stable name.
$lib = Get-ChildItem (Join-Path $Prefix 'lib') -Filter 'assimp*.lib' | Select-Object -First 1
if (-not $lib) { throw 'assimp library not found after install' }
Copy-Item $lib.FullName (Join-Path $Prefix 'lib\assimp.lib')
New-Item -ItemType Directory -Force (Join-Path $Prefix 'licenses') | Out-Null
Copy-Item (Join-Path $src 'LICENSE') (Join-Path $Prefix 'licenses\assimp-LICENSE')
Get-ChildItem (Join-Path $Prefix 'lib') -File | Select-Object Name, Length | Format-Table -AutoSize | Out-String
