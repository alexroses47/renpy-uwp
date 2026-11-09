# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

# Generates synthetic test media for the milestone 3 test (no game content): a PNG, an Ogg Vorbis
# file and Matroska files with VP9 (8-bit and 10-bit, as KS:RE) + Opus. Uses an ffmpeg build on
# the CI runner only to create these files; that binary is not shipped.
param([Parameter(Mandatory = $true)][string]$OutDir)

$ErrorActionPreference = 'Stop'
function Invoke-Checked([string]$what, [scriptblock]$cmd) {
    & $cmd
    if ($LASTEXITCODE -ne 0) { throw "$what failed with exit code $LASTEXITCODE" }
}

if (-not (Get-Command ffmpeg -ErrorAction SilentlyContinue)) {
    Invoke-Checked 'choco ffmpeg' { choco install ffmpeg -y --no-progress | Out-Host }
    $env:PATH = "C:\ProgramData\chocolatey\bin;$env:PATH"
}
New-Item -ItemType Directory -Force $OutDir | Out-Null
$o = (Resolve-Path $OutDir).Path
$ff = @('-hide_banner', '-loglevel', 'error', '-y')

Invoke-Checked 'png' { ffmpeg @ff -f lavfi -i 'testsrc2=size=1280x720:rate=1' -frames:v 1 (Join-Path $o 'test.png') }
Invoke-Checked 'ogg' { ffmpeg @ff -f lavfi -i 'sine=frequency=440:sample_rate=44100:duration=6' -ac 2 -c:a libvorbis -q:a 4 (Join-Path $o 'test.ogg') }
foreach ($v in @(@{ n = 'test-vp9.mkv'; pix = 'yuv420p'; prof = '0' }, @{ n = 'test-vp9-10bit.mkv'; pix = 'yuv420p10le'; prof = '2' })) {
    Invoke-Checked $v.n {
        ffmpeg @ff -f lavfi -i 'testsrc2=size=1920x1080:rate=30' -f lavfi -i 'sine=frequency=660:sample_rate=48000' -t 10 `
            -c:v libvpx-vp9 -deadline realtime -cpu-used 8 -b:v 3M -pix_fmt $v.pix -profile:v $v.prof `
            -c:a libopus -b:a 96k -ac 2 (Join-Path $o $v.n)
    }
}
Get-ChildItem $o | Select-Object Name, Length | Format-Table -AutoSize | Out-String
foreach ($f in Get-ChildItem $o -Filter *.mkv) {
    ffprobe -v error -show_entries stream=codec_name,pix_fmt,width,height -of compact $f.FullName
}
