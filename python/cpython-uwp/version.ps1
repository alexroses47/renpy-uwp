# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

# Pinned sources for the UWP CPython build (see docs/RESEARCH.md sections 17-21).
$PyForkUrl = 'https://github.com/thexai/cpython.git'
$PyForkCommit = '4c97e13f50ce1f925e5f95945c6d395b83f78a64'   # v3.12.11 + "Kodi patches"
$PyVersion = '3.12.11'
$PyVersionShort = '312'

$ZlibVersion = '1.3.1'
$ZlibUrl = "https://github.com/madler/zlib/releases/download/v$ZlibVersion/zlib-$ZlibVersion.tar.gz"
$ZlibSha256 = '9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23'   # same as renpy-build
