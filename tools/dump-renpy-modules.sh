#!/usr/bin/env bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

# Records which Python modules a Ren'Py game really loads on desktop Linux, to use as the
# source of truth for the modules the UWP build must provide.
#
# It temporarily drops a small .rpy into the game's game/ folder, lets you play, and removes it
# (and its .rpyc) afterwards. Nothing of the game is copied anywhere.
#
# Usage: tools/dump-renpy-modules.sh [GAME_DIR] [OUTPUT_JSON]
#   GAME_DIR     folder that contains game/ (default: autodetect KS:RE under ~/.local/share/Steam)
#   OUTPUT_JSON  default: docs/renpy-modules-ksre.json
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
game_dir="${1:-}"
out="${2:-$ROOT/docs/renpy-modules-ksre.json}"

if [[ -z "$game_dir" ]]; then
    common="$HOME/.local/share/Steam/steamapps/common"
    game_dir="$(find "$common" -maxdepth 2 -type d -name game -path '*atawa*' -printf '%h\n' 2>/dev/null | head -n1 || true)"
    [[ -n "$game_dir" ]] || { echo "error: KS:RE not found under $common; pass GAME_DIR" >&2; exit 1; }
fi
[[ -d "$game_dir/game" ]] || { echo "error: $game_dir/game does not exist" >&2; exit 1; }

hook="$game_dir/game/zz_renpy_uwp_module_dump.rpy"
raw="${out%.json}.raw.json"
cleanup() { rm -f "$hook" "${hook}c" "${hook%.rpy}.rpyc"; }
trap cleanup EXIT

cat >"$hook" <<'RPY'
# Temporary file written by renpy-uwp tools/dump-renpy-modules.sh. Safe to delete.
init -1999 python:
    import sys as _uwp_sys, json as _uwp_json, os as _uwp_os, importlib.machinery as _uwp_mach

    _uwp_seen = {}

    def _uwp_kind(name, mod):
        if name in _uwp_sys.builtin_module_names:
            return "builtin"
        f = getattr(mod, "__file__", None) or ""
        if any(f.endswith(s) for s in _uwp_mach.EXTENSION_SUFFIXES):
            return "extension"
        if f:
            return "python"
        return "other"

    def _uwp_dump(reason):
        for k, m in list(_uwp_sys.modules.items()):
            if k not in _uwp_seen:
                _uwp_seen[k] = {"kind": _uwp_kind(k, m), "file": getattr(m, "__file__", None), "first_seen": reason}
        data = {
            "python": _uwp_sys.version,
            "renpy": renpy.version(),
            "builtin_module_names": sorted(_uwp_sys.builtin_module_names),
            "modules": dict(sorted(_uwp_seen.items())),
        }
        path = _uwp_os.environ.get("RENPY_UWP_DUMP", _uwp_os.path.expanduser("~/renpy-uwp-modules.raw.json"))
        with open(path, "w") as f:
            _uwp_json.dump(data, f, indent=1)

    config.start_callbacks.append(lambda: _uwp_dump("start"))
    config.after_load_callbacks.append(lambda: _uwp_dump("after_load"))
    config.quit_callbacks.append(lambda: _uwp_dump("quit"))
    config.periodic_callbacks.append(lambda: (_uwp_dump("periodic") if renpy.get_game_runtime() % 10 < 0.05 else None))
RPY

launcher="$(find "$game_dir" -maxdepth 1 -name '*.sh' | head -n1 || true)"
echo "Hook installed: $hook"
echo
echo "Now:"
echo "  1. Start the game (Steam, or: RENPY_UWP_DUMP='$raw' '${launcher:-<game>.sh}')"
echo "     If you start it from Steam, the dump goes to ~/renpy-uwp-modules.raw.json"
echo "  2. Open the main menu, start a new game for a few lines, open Preferences,"
echo "     save, then load that save (and play a video if you can)."
echo "  3. Quit the game from its menu."
read -r -p "Press Enter here after quitting the game... " _

src="$raw"
[[ -s "$src" ]] || src="$HOME/renpy-uwp-modules.raw.json"
[[ -s "$src" ]] || { echo "error: no dump found ($raw or ~/renpy-uwp-modules.raw.json)" >&2; exit 1; }
python3 -I - "$src" "$out" <<'PY'
import json, sys
data = json.load(open(sys.argv[1]))
mods = data["modules"]
# Keep only the module names and kinds (no game file paths in the repo).
summary = {
    "python": data["python"],
    "renpy": data["renpy"],
    "builtin_module_names": data["builtin_module_names"],
    "modules": {k: v["kind"] for k, v in mods.items()},
}
json.dump(summary, open(sys.argv[2], "w"), indent=1, sort_keys=True)
kinds = {}
for v in summary["modules"].values():
    kinds[v] = kinds.get(v, 0) + 1
print(f"{len(mods)} modules seen: {kinds}")
print("extension modules:", ", ".join(k for k, v in summary["modules"].items() if v == "extension"))
PY
echo "Written: $out"
