#!/usr/bin/env bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
# Chooses which game the runtime starts: writes LocalState\renpy-uwp.ini on the Xbox.
#
# Usage:
#   ci/set-game.sh <game> [hdmi]  # game: folder under <drive>:\renpy\games\ (the_question, ksre)
#                                 # hdmi: system (default) or 4k (4K120 output + 1440p policy)
#   ci/set-game.sh --show     # print the current renpy-uwp.ini
#   ci/set-game.sh --clear    # empty ini: the runtime picks the first game alphabetically
#
# Requires XBOX_DP_USER / XBOX_DP_PASS (optional XBOX_DP_URL) and the app installed and launched once.
# shellcheck source=lib/devportal.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT/ci/lib/devportal.sh"

[[ $# -ge 1 && $# -le 2 ]] || { sed -n '5,12p' "$0"; exit 2; }

dp_require_creds
dp_load_pins
pfn="$(dp_package_full_name)"
[[ -n "$pfn" ]] || dp_die "package $DP_PACKAGE_NAME is not installed on the console"

if [[ "$1" == "--show" ]]; then
    dp_curl --fail -G "$DP_URL/api/filesystem/apps/file" \
        --data-urlencode "knownfolderid=LocalAppData" --data-urlencode "packagefullname=$pfn" \
        --data-urlencode 'path=\LocalState' --data-urlencode "filename=renpy-uwp.ini" ||
        dp_die "no renpy-uwp.ini yet"
    exit 0
fi

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
ini="$tmp/renpy-uwp.ini"
if [[ "$1" == "--clear" ]]; then
    printf '# renpy-uwp runtime settings\n' >"$ini"
else
    [[ "$1" =~ ^[A-Za-z0-9._-]+$ ]] || dp_die "invalid game folder name: $1"
    hdmi="${2:-system}"
    [[ "$hdmi" == system || "$hdmi" == 4k ]] || dp_die "hdmi must be 'system' or '4k'"
    printf '# renpy-uwp runtime settings\ngame = %s\nhdmi = %s\n' "$1" "$hdmi" >"$ini"
fi

dp_csrf
dp_curl --fail-with-body -b "$DP_JAR" -H "X-CSRF-Token: $DP_CSRF" -X POST \
    -F "file=@$ini;filename=renpy-uwp.ini" \
    "$DP_URL/api/filesystem/apps/file?knownfolderid=LocalAppData&packagefullname=$pfn&path=%5CLocalState" >/dev/null
dp_info "Uploaded LocalState\\renpy-uwp.ini:"
cat "$ini"
