# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

# Builds CPython 3.12.11 for UWP x64 as a static library and the precompiled stdlib zip.
#   $Prefix/lib/python312.lib, $Prefix/lib/zlibstatic.lib, $Prefix/include/..., $Prefix/python312.zip
# Requires: MSVC v143 (VS 2022), CMake, git, and a host Python 3.12 ($HostPython).

param(
    [Parameter(Mandatory = $true)][string]$Prefix,
    [Parameter(Mandatory = $true)][string]$HostPython,
    [string]$Configuration = 'Release',
    [string]$WorkDir = (Join-Path $env:RUNNER_TEMP 'cpython-uwp')
)

$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
. (Join-Path $here 'version.ps1')

function Invoke-Checked([string]$what, [scriptblock]$cmd) {
    & $cmd
    if ($LASTEXITCODE -ne 0) { throw "$what failed with exit code $LASTEXITCODE" }
}

$hostVer = & $HostPython -c "import sys; print('%d.%d' % sys.version_info[:2])"
if ($hostVer -ne '3.12') { throw "HostPython must be 3.12 (got $hostVer): .pyc and frozen modules depend on it" }

New-Item -ItemType Directory -Force $WorkDir | Out-Null
$src = Join-Path $WorkDir 'src'
$build = Join-Path $WorkDir 'build'
foreach ($d in @($src, $build)) { if (Test-Path $d) { Remove-Item -Recurse -Force $d } }

# --- sources: fork at the pinned commit + our patch series + our CMakeLists.txt ------------
Invoke-Checked 'git init' { git init -q $src }
Push-Location $src
try {
    Invoke-Checked 'git fetch' { git fetch -q --depth 1 $PyForkUrl $PyForkCommit }
    Invoke-Checked 'git checkout' { git checkout -q FETCH_HEAD }
    $patches = Get-ChildItem (Join-Path $here 'patches') -Filter '*.patch' | Sort-Object Name
    foreach ($p in $patches) {
        Write-Host "Applying $($p.Name)"
        Invoke-Checked "git apply $($p.Name)" { git apply --whitespace=nowarn $p.FullName }
    }
} finally { Pop-Location }
Copy-Item (Join-Path $here 'CMakeLists.txt') (Join-Path $src 'CMakeLists.txt') -Force

# --- zlib -------------------------------------------------------------------------------------
$zlibTar = Join-Path $WorkDir "zlib-$ZlibVersion.tar.gz"
if (-not (Test-Path $zlibTar)) { Invoke-WebRequest -Uri $ZlibUrl -OutFile $zlibTar }
$h = (Get-FileHash -Algorithm SHA256 $zlibTar).Hash.ToLowerInvariant()
if ($h -ne $ZlibSha256) { throw "zlib SHA-256 mismatch: $h" }
$zlibSrc = Join-Path $WorkDir "zlib-$ZlibVersion"
if (Test-Path $zlibSrc) { Remove-Item -Recurse -Force $zlibSrc }
Invoke-Checked 'tar zlib' { tar -xzf $zlibTar -C $WorkDir }

# --- configure + build (Visual Studio generator => AppContainer => WINAPI_FAMILY_APP) ---------
$hostPy = (Get-Command $HostPython).Source
Invoke-Checked 'cmake configure' {
    cmake -S $src -B $build -G 'Visual Studio 17 2022' -A x64 `
        '-DCMAKE_SYSTEM_NAME=WindowsStore' '-DCMAKE_SYSTEM_VERSION=10.0.22621.0' `
        "-DPYTHON_FOR_BUILD=$hostPy" "-DZLIB_SOURCE_DIR=$zlibSrc" "-DCMAKE_INSTALL_PREFIX=$Prefix"
}
Invoke-Checked 'cmake build' { cmake --build $build --config $Configuration --parallel -- /nologo /v:minimal }
Invoke-Checked 'cmake install' { cmake --install $build --config $Configuration }

# --- precompiled stdlib zip -----------------------------------------------------------------
Invoke-Checked 'stdlib zip' {
    & $HostPython -I (Join-Path $here 'make_stdlib_zip.py') (Join-Path $src 'Lib') (Join-Path $Prefix "python$PyVersionShort.zip")
}

New-Item -ItemType Directory -Force (Join-Path $Prefix 'licenses') | Out-Null
Copy-Item (Join-Path $src 'LICENSE') (Join-Path $Prefix 'licenses\CPython-LICENSE.txt')
Copy-Item (Join-Path $zlibSrc 'LICENSE') (Join-Path $Prefix 'licenses\zlib-LICENSE.txt')
@{
    name = 'cpython-uwp'; version = $PyVersion; fork = $PyForkUrl; commit = $PyForkCommit
    patches = @($patches | ForEach-Object Name); zlib = $ZlibVersion; configuration = $Configuration
} | ConvertTo-Json | Set-Content -Encoding utf8 (Join-Path $Prefix 'cpython-uwp-build.json')
Get-ChildItem -Recurse -File $Prefix -Include *.lib, *.zip, *.json | Select-Object FullName, Length |
    Format-Table -AutoSize | Out-String -Width 300
