# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

# Builds SDL2 (WinRT/UWP backend) for x64 with MSVC and installs it into $Prefix:
#   $Prefix/include/SDL2/*.h, $Prefix/lib/SDL2.lib, $Prefix/bin/SDL2.dll, $Prefix/licenses/SDL2-LICENSE.txt
# SDL2 itself is zlib-licensed; no patches are applied (see docs/RESEARCH.md section 11).

param(
    [Parameter(Mandatory = $true)][string]$Prefix,
    [string]$Configuration = 'Release',
    [string]$WorkDir = (Join-Path $env:RUNNER_TEMP 'sdl2-build')
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'version.ps1')

New-Item -ItemType Directory -Force $WorkDir | Out-Null
$tarball = Join-Path $WorkDir "SDL2-$SdlVersion.tar.gz"
if (-not (Test-Path $tarball)) {
    Write-Host "Downloading $SdlUrl"
    Invoke-WebRequest -Uri $SdlUrl -OutFile $tarball
}
$hash = (Get-FileHash -Algorithm SHA256 $tarball).Hash.ToLowerInvariant()
if ($hash -ne $SdlSha256) { throw "SHA-256 mismatch for $tarball`: $hash (expected $SdlSha256)" }

$src = Join-Path $WorkDir "SDL2-$SdlVersion"
if (Test-Path $src) { Remove-Item -Recurse -Force $src }
tar -xzf $tarball -C $WorkDir
if ($LASTEXITCODE -ne 0) { throw "tar failed" }

$proj = Join-Path $src 'VisualC-WinRT\SDL-UWP.vcxproj'
$out = (Join-Path $WorkDir "out\$Configuration").Replace('\', '/') + '/'
$int = (Join-Path $WorkDir "obj\$Configuration").Replace('\', '/') + '/'
msbuild $proj /m /nologo /v:minimal `
    /p:Configuration=$Configuration /p:Platform=x64 `
    /p:PlatformToolset=v143 `
    /p:WindowsTargetPlatformVersion=10.0.22621.0 `
    /p:WindowsTargetPlatformMinVersion=10.0.19041.0 `
    /p:TargetName=SDL2 `
    /p:OutDir=$out /p:IntDir=$int
if ($LASTEXITCODE -ne 0) { throw "msbuild SDL-UWP failed with exit code $LASTEXITCODE" }

$inc = New-Item -ItemType Directory -Force (Join-Path $Prefix 'include\SDL2')
$lib = New-Item -ItemType Directory -Force (Join-Path $Prefix 'lib')
$bin = New-Item -ItemType Directory -Force (Join-Path $Prefix 'bin')
$lic = New-Item -ItemType Directory -Force (Join-Path $Prefix 'licenses')
Copy-Item (Join-Path $src 'include\*.h') $inc
Copy-Item (Join-Path $out 'SDL2.lib') $lib
Copy-Item (Join-Path $out 'SDL2.dll') $bin
if (Test-Path (Join-Path $out 'SDL2.pdb')) { Copy-Item (Join-Path $out 'SDL2.pdb') $bin }
Copy-Item (Join-Path $src 'LICENSE.txt') (Join-Path $lic 'SDL2-LICENSE.txt')
@{ name = 'SDL2'; version = $SdlVersion; sha256 = $SdlSha256; configuration = $Configuration } |
    ConvertTo-Json | Set-Content -Encoding utf8 (Join-Path $Prefix 'sdl2-build.json')
Get-ChildItem -Recurse $Prefix -File | Where-Object { $_.Directory.Name -ne 'SDL2' } |
    Select-Object FullName, Length | Format-Table -AutoSize | Out-String -Width 300
