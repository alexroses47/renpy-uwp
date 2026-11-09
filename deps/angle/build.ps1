# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

# Builds ANGLE (OpenGL ES -> Direct3D 11) for UWP x64 from the SternXD/angle fork.
#   $Prefix/bin/libEGL.dll, libGLESv2.dll ; $Prefix/lib/*.dll.lib ; $Prefix/include/{EGL,GLES2,GLES3,KHR}
# ANGLE is BSD-3-Clause. See docs/RESEARCH.md section 27.

param(
    [Parameter(Mandatory = $true)][string]$Prefix,
    [string]$WorkDir = (Join-Path $env:RUNNER_TEMP 'angle')
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'version.ps1')

function Invoke-Checked([string]$what, [scriptblock]$cmd) {
    & $cmd
    if ($LASTEXITCODE -ne 0) { throw "$what failed with exit code $LASTEXITCODE" }
}

New-Item -ItemType Directory -Force $WorkDir | Out-Null
$depot = Join-Path $WorkDir 'depot_tools'
$src = Join-Path $WorkDir 'angle'

# depot_tools (gclient, gn, ninja). Use the local Visual Studio, not Google's packaged toolchain.
if (-not (Test-Path $depot)) {
    Invoke-Checked 'clone depot_tools' { git clone -q --depth 1 https://chromium.googlesource.com/chromium/tools/depot_tools.git $depot }
}
$env:PATH = "$depot;$env:PATH"
$env:DEPOT_TOOLS_WIN_TOOLCHAIN = '0'
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
$env:vs2022_install = $vs
Write-Host "Visual Studio: $vs"
# First run bootstraps depot_tools (its own Python 3 via CIPD, python3_bin_reldir.txt); only then
# freeze it so later steps do not self-update.
Invoke-Checked 'depot_tools bootstrap' { cmd /c "update_depot_tools.bat" }
Invoke-Checked 'gclient bootstrap' { cmd /c "gclient --version" }
$env:DEPOT_TOOLS_UPDATE = '0'

# ANGLE at the pinned fork commit.
if (-not (Test-Path (Join-Path $src '.git'))) {
    Invoke-Checked 'git init' { git init -q $src }
}
Push-Location $src
try {
    Invoke-Checked 'fetch angle' { git fetch -q --depth 1 $AngleUrl $AngleCommit }
    Invoke-Checked 'checkout angle' { git checkout -q FETCH_HEAD }
    Invoke-Checked 'bootstrap' { python scripts/bootstrap.py }
    Invoke-Checked 'gclient sync' { cmd /c "gclient sync --no-history --shallow -j 8" }

    $args = @(
        'target_os="winuwp"', 'target_cpu="x64"', 'is_debug=false', 'is_component_build=false',
        'symbol_level=0', 'dcheck_always_on=false',
        'angle_enable_d3d11=true', 'angle_enable_d3d9=false', 'angle_enable_gl=false',
        'angle_enable_vulkan=false', 'angle_enable_null=false', 'angle_enable_swiftshader=false',
        'angle_enable_wgpu=false', 'angle_build_tests=false', 'angle_has_frame_capture=false'
    ) -join ' '
    Write-Host "gn args: $args"
    Invoke-Checked 'gn gen' { cmd /c "gn gen out\uwp --args=""$args""" }
    Invoke-Checked 'ninja' { cmd /c "autoninja -C out\uwp libEGL libGLESv2" }
} finally { Pop-Location }

$out = Join-Path $src 'out\uwp'
foreach ($d in 'bin', 'lib', 'include', 'licenses') { New-Item -ItemType Directory -Force (Join-Path $Prefix $d) | Out-Null }
foreach ($n in 'libEGL', 'libGLESv2') {
    Copy-Item (Join-Path $out "$n.dll") (Join-Path $Prefix 'bin')
    Copy-Item (Join-Path $out "$n.dll.lib") (Join-Path $Prefix 'lib')
}
foreach ($inc in 'EGL', 'GLES2', 'GLES3', 'KHR') {
    Copy-Item -Recurse (Join-Path $src "include\$inc") (Join-Path $Prefix 'include')
}
Copy-Item (Join-Path $src 'LICENSE') (Join-Path $Prefix 'licenses\ANGLE-LICENSE.txt')
@{ name = 'ANGLE'; fork = $AngleUrl; commit = $AngleCommit; gn_args = $args } |
    ConvertTo-Json | Set-Content -Encoding utf8 (Join-Path $Prefix 'angle-build.json')
Get-ChildItem -Recurse -File (Join-Path $Prefix 'bin') | ForEach-Object {
    Write-Host ("{0}  {1}  sha256={2}" -f $_.Name, $_.Length, (Get-FileHash -Algorithm SHA256 $_.FullName).Hash)
}
