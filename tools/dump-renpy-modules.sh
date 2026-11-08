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
cleanup() {
    rm -f "$hook" "${hook}c" "${hook%.rpy}.rpyc"
    # Ren'Py (developer mode) keeps copies of compiled .rpy files in ~/.renpy/backups/<game>/.
    rm -f "$HOME"/.renpy/backups/*/zz_renpy_uwp_module_dump.*.rpy 2>/dev/null || true
}
trap cleanup EXIT

cat >"$hook" <<'RPY'
# Temporary file written by renpy-uwp tools/dump-renpy-modules.sh. Safe to delete.
# Runs at init 999 (after renpy/common, which defines config.after_load_callbacks) inside
# "python hide", so nothing is added to the store or to save files. Every callback registration
# is guarded: an unknown config variable can never stop the game. Verified against Ren'Py
# 8.5.3.26051504 (renpy/config.py, renpy/common/00start.rpy).
init 999 python hide:
    import sys, os, json, atexit, types
    import importlib.machinery as mach

    state = sys.modules.get("_renpy_uwp_dump")
    if state is None:
        state = types.ModuleType("_renpy_uwp_dump")
        state.seen = {}
        state.events = []
        state.registered = []
        state.failed = []
        state.first_interaction = False
        sys.modules["_renpy_uwp_dump"] = state

    path = os.environ.get("RENPY_UWP_DUMP") or os.path.expanduser("~/renpy-uwp-modules.raw.json")

    def kind(name, mod):
        if name in sys.builtin_module_names:
            return "builtin"
        f = getattr(mod, "__file__", None) or ""
        if f.endswith(tuple(mach.EXTENSION_SUFFIXES)):
            return "extension"
        return "python" if f else "other"

    def dump(reason):
        try:
            for k, m in list(sys.modules.items()):
                if k not in state.seen:
                    state.seen[k] = {"kind": kind(k, m), "first_seen": reason}
            state.events.append(reason)
            data = {
                "python": sys.version,
                "renpy": renpy.version(),
                "events": state.events,
                "registered": state.registered,
                "registration_failures": state.failed,
                "builtin_module_names": sorted(sys.builtin_module_names),
                "modules": dict(sorted(state.seen.items())),
            }
            tmp = path + ".tmp"
            with open(tmp, "w") as f:
                json.dump(data, f, indent=1)
            os.replace(tmp, path)
        except Exception:
            pass

    def register(name, fn):
        try:
            getattr(renpy.config, name).append(fn)
            state.registered.append(name)
        except Exception as e:
            state.failed.append("%s: %s" % (name, e))

    def first_interaction():
        if not state.first_interaction:
            state.first_interaction = True
            dump("first_interaction")

    register("start_interact_callbacks", first_interaction)
    register("after_load_callbacks", lambda: dump("after_load"))
    register("quit_callbacks", lambda: dump("quit"))
    register("at_exit_callbacks", lambda: dump("at_exit"))
    try:
        atexit.register(dump, "atexit")
        state.registered.append("atexit")
    except Exception as e:
        state.failed.append("atexit: %s" % e)
    dump("end_of_init")
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
    "events": data.get("events", []),
    "registered": data.get("registered", []),
    "registration_failures": data.get("registration_failures", []),
    "builtin_module_names": data["builtin_module_names"],
    "modules": {k: v["kind"] for k, v in mods.items()},
}
json.dump(summary, open(sys.argv[2], "w"), indent=1, sort_keys=True)
kinds = {}
for v in summary["modules"].values():
    kinds[v] = kinds.get(v, 0) + 1
print(f"{len(mods)} modules seen: {kinds}")
print("dump events:", ", ".join(summary["events"]))
if summary["registration_failures"]:
    print("callbacks not registered:", summary["registration_failures"])
print("extension modules:", ", ".join(k for k, v in summary["modules"].items() if v == "extension"))
PY
echo "Written: $out"
