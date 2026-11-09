#!/usr/bin/env bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

# Fails if any tracked file looks like game content (Ren'Py scripts/archives, media, fonts)
# outside the allow-list. Runs in CI and can be used as a local pre-commit check.
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

# Our own files that use game-like extensions: app tiles, the IsroTaxo menu (a tiny Ren'Py game written
# for this project), the project logo (the author's artwork) and README screenshots (our menu; Ren'Py's
# MIT-licensed sample game The Question).
allow_re='^(host/RenpyUWP/Assets/[A-Za-z0-9_.-]+\.png|host/RenpyUWP/menu/game/[a-z_]+\.rpy|host/RenpyUWP/menu/game/images/(logo|flower)\.png|host/branding/isrotaxo-logo\.png|docs/screenshots/(menu\.png|xbox-[a-z-]+\.jpg))$'
block_re='\.(rpa|rpi|rpy|rpyc|rpym|rpymc|rpyb|rpu|png|jpe?g|webp|avif|gif|bmp|tga|dds|ogg|opus|oga|mp3|wav|flac|m4a|webm|mkv|ogv|mp4|m4v|avi|mpe?g|ttf|otf|ttc|woff2?|moc3|zip|7z|rar|pfx|p12|key|pem)$'

bad=$(git ls-files | grep -Ei "$block_re" | grep -Ev "$allow_re" || true)
big=$(git ls-files -z | xargs -0 -r du -k 2>/dev/null | awk '$1 > 2048 {print $2}' || true)

status=0
if [[ -n "$bad" ]]; then
    echo "ERROR: tracked files that look like game assets or secrets:" >&2
    echo "$bad" >&2
    status=1
fi
if [[ -n "$big" ]]; then
    echo "ERROR: tracked files larger than 2 MiB (game data?):" >&2
    echo "$big" >&2
    status=1
fi
[[ $status -eq 0 ]] && echo "OK: no game assets or secrets tracked."
exit $status
