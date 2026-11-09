#!/usr/bin/env bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
# Saves a screenshot of what the console shows right now (including the Xbox guide and system UI),
# via the Xbox Device Portal (/ext/screenshot).
#
# Usage: ci/screenshot.sh [OUT.png]      (default: screenshots/xbox-<timestamp>.png)
# Check every capture for personal data (account e-mail, gamertag) before publishing it.
# shellcheck source=lib/devportal.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT/ci/lib/devportal.sh"

out="${1:-$ROOT/screenshots/xbox-$(date +%Y%m%d-%H%M%S).png}"
mkdir -p "$(dirname "$out")"
dp_require_creds
dp_load_pins
dp_curl --fail -o "$out" "$DP_URL/ext/screenshot?download=false"
file -b "$out" | grep -q '^PNG image' || dp_die "the console did not return a PNG"
dp_info "Saved $out ($(file -b "$out" | cut -d, -f2 | xargs))"
