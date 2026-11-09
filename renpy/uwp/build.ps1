# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

# Ren'Py for UWP: Cython (3.2.4, static mode) -> renpy_static.lib with the selected C modules, and
# the pure-Python package as precompiled renpy.zip.
#   $Prefix/lib/renpy_static.lib, $Prefix/renpy.zip, $Prefix/renpy-build.json
param(
    [Parameter(Mandatory = $true)][string]$Prefix,
    [Parameter(Mandatory = $true)][string]$HostPython,
    [Parameter(Mandatory = $true)][string]$PythonPrefix,
    [Parameter(Mandatory = $true)][string]$SdlPrefix,
    [Parameter(Mandatory = $true)][string]$MediaPrefix,
    [string]$WorkDir = (Join-Path $env:RUNNER_TEMP 'renpy-uwp')
)

$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
$renpyVersion = (Get-Content (Join-Path $here '..\VERSION')).Trim()
# Milestone 3: pygame_sdl2 (renpy.pygame), audio/video (renpysound + filter) and _renpy.
$modules = @('_renpy', 'renpy.audio.renpysound', 'renpy.audio.filter', 'renpy.pygame.*')

function Invoke-Checked([string]$what, [scriptblock]$cmd) {
    & $cmd
    if ($LASTEXITCODE -ne 0) { throw "$what failed with exit code $LASTEXITCODE" }
}

New-Item -ItemType Directory -Force $WorkDir, $Prefix | Out-Null
$src = Join-Path $WorkDir 'renpy'
if (Test-Path $src) { Remove-Item -Recurse -Force $src }
Invoke-Checked 'clone renpy' { git clone -q --depth 1 --branch $renpyVersion https://github.com/renpy/renpy.git $src }

$venv = Join-Path $WorkDir 'venv'
Invoke-Checked 'venv' { & $HostPython -m venv $venv }
$py = Join-Path $venv 'Scripts\python.exe'
Invoke-Checked 'pip cython' { & $py -m pip install -q 'cython==3.2.4' setuptools }

Push-Location $src
try {
    # Ren'Py's setuplib opens .pyx files without an encoding; on Windows that is cp1252 and fails on
    # UTF-8 sources (textsupport.pyx). UTF-8 mode makes open() default to UTF-8, as on Linux.
    $env:PYTHONUTF8 = '1'
    $env:RENPY_STATIC = '1'
    $env:RENPY_REGENERATE_CYTHON = '1'
    $env:RENPY_CYTHON = (Join-Path $venv 'Scripts\cython.exe')
    $env:RENPY_CYTHON_SINGLETHREAD = '1'
    Invoke-Checked 'setup.py generate' { & $py setup.py generate | Out-Host }
} finally { Pop-Location }

$gen = Join-Path $WorkDir 'gen'
Invoke-Checked 'gen_static' { & $py -I (Join-Path $here 'gen_static.py') $src $gen @modules | Out-Host }

$build = Join-Path $WorkDir 'build'
Invoke-Checked 'cmake configure' {
    cmake -S $here -B $build -G 'Visual Studio 17 2022' -A x64 `
        '-DCMAKE_SYSTEM_NAME=WindowsStore' '-DCMAKE_SYSTEM_VERSION=10.0.22621.0' `
        "-DRENPY_ROOT=$src" "-DGEN_DIR=$gen" "-DPYTHON_PREFIX=$PythonPrefix" "-DSDL2_PREFIX=$SdlPrefix" `
        "-DMEDIA_PREFIX=$MediaPrefix" "-DCMAKE_INSTALL_PREFIX=$Prefix" | Out-Host
}
Invoke-Checked 'cmake build' { cmake --build $build --config Release --parallel -- /nologo /v:minimal | Out-Host }
Invoke-Checked 'cmake install' { cmake --install $build --config Release | Out-Host }

# Pure-Python Ren'Py package (renpy/**/*.py) as .pyc in a zip, like the stdlib.
Invoke-Checked 'renpy.zip' {
    & $py -I (Join-Path $here '..\..\python\cpython-uwp\make_stdlib_zip.py') $src (Join-Path $Prefix 'renpy.zip') --only renpy | Out-Host
}
New-Item -ItemType Directory -Force (Join-Path $Prefix 'licenses') | Out-Null
# Ren'Py has no LICENSE file at the root: the terms are in sphinx/source/license.rst (and MIT headers).
Copy-Item (Join-Path $src 'sphinx\source\license.rst') (Join-Path $Prefix 'licenses\RenPy-license.rst')
@{ renpy = $renpyVersion; cython = '3.2.4'; modules = $modules } | ConvertTo-Json |
    Set-Content -Encoding utf8 (Join-Path $Prefix 'renpy-build.json')
