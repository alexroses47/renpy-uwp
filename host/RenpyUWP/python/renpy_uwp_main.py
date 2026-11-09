# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

"""Ren'Py launcher for UWP: the equivalent of renpy.py with sandbox paths (docs/RESEARCH.md section 31.3).

Run by the host as renpy_uwp_main.main() on the SDL thread, after the window has been handed over.
  - Game: <drive>:\\renpy\\games\\<name>\\game\\ on removable/extra drives (D: to Z:). LocalState\\renpy-uwp.ini
    chooses one with "game = <name>" (ci/set-game.sh); otherwise the first one alphabetically.
  - renpy_base: <package>\\renpy-base (renpy.py + renpy\\common\\); the engine's .py code is in python\\renpy.zip.
  - Saves: LocalState\\saves (RENPY_PATH_TO_SAVES). Ren'Py logs (log.txt, traceback.txt): LocalState\\logs\\<name>.
  - Renderer: gles2 (ANGLE); SDL on WinRT has no desktop OpenGL.
"mode = media-test" in the ini runs the milestone 3 media test instead.

Returns 0 when Ren'Py ran (the host then restores the display mode and exits), non-zero otherwise
(the host stays in its diagnostic screen). Launcher log: LocalState\\renpy-launcher.log.
"""

import importlib.util
import os
import string
import sys
import time
import traceback

GAMES_DIR = os.path.join("renpy", "games")


def read_ini(path):
    values = {}
    try:
        with open(path, encoding="utf-8") as f:
            for line in f:
                line = line.split("#", 1)[0].strip()
                if "=" in line:
                    k, v = line.split("=", 1)
                    values[k.strip().lower()] = v.strip()
    except FileNotFoundError:
        pass
    return values


def find_games():
    """{name: basedir} for every <drive>:\\renpy\\games\\<name>\\game directory."""
    games = {}
    for letter in string.ascii_uppercase[3:]:
        root = f"{letter}:\\{GAMES_DIR}"
        try:
            names = sorted(os.listdir(root))
        except OSError:
            continue
        for name in names:
            basedir = os.path.join(root, name)
            if os.path.isdir(os.path.join(basedir, "game")) and name not in games:
                games[name] = basedir
    return games


def log(msg):
    print(time.strftime("%H:%M:%S"), msg, flush=True)


def main():
    localstate = sys._renpy_uwp_localstate
    install = sys._renpy_uwp_install
    out = open(os.path.join(localstate, "renpy-launcher.log"), "w", encoding="utf-8", buffering=1)
    sys.stdout = sys.stderr = out
    try:
        return run(localstate, install)
    except BaseException:
        traceback.print_exc()
        return 1
    finally:
        out.flush()


def run(localstate, install):
    ini = read_ini(os.path.join(localstate, "renpy-uwp.ini"))
    log(f"renpy-uwp launcher; ini: {ini}")

    if ini.get("mode") == "media-test":
        import media_test
        return media_test.main()

    games = find_games()
    log(f"games found: {games}")
    if not games:
        log("no game: expected <drive>:\\renpy\\games\\<name>\\game\\ on a USB drive")
        return 3
    name = ini.get("game") or sorted(games)[0]
    if name not in games:
        log(f"game {name!r} from renpy-uwp.ini not found")
        return 3
    basedir = games[name]

    renpy_base = os.path.join(install, "renpy-base")
    saves = os.path.join(localstate, "saves")
    logdir = os.path.join(localstate, "logs", name)
    os.makedirs(saves, exist_ok=True)
    os.makedirs(logdir, exist_ok=True)

    os.environ["RENPY_PATH_TO_SAVES"] = saves
    os.environ.setdefault("RENPY_RENDERER", ini.get("renderer", "gles2"))
    os.environ["RENPY_HIGHDPI"] = "1"  # skips the ctypes DPI query (display/core.py setup_dpi_scaling)
    os.environ["NO_COLOR"] = "1"       # skips the ctypes console-colour probe (error.py)

    # Pure-Python packages Ren'Py ships with its Python (ecdsa, rsa, six, requests...).
    site_zip = os.path.join(install, "python", "site-packages.zip")
    if site_zip not in sys.path:
        sys.path.append(site_zip)

    # Ren'Py's own launcher module: bootstrap() calls renpy.__main__.path_to_gamedir & co.
    spec = importlib.util.spec_from_file_location("renpy_launcher_main", os.path.join(renpy_base, "renpy.py"))
    launcher = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(launcher)
    launcher.path_to_logdir = lambda _basedir: logdir
    sys.modules["renpy_launcher_main"] = launcher

    sys.argv = [os.path.join(renpy_base, "renpy.py"), basedir]
    log(f"starting {name}: basedir={basedir} renpy_base={renpy_base} saves={saves} logs={logdir} "
        f"renderer={os.environ['RENPY_RENDERER']}")
    t0 = time.time()
    try:
        launcher.main()
        status = 0
    except SystemExit as e:
        status = e.code
    log(f"Ren'Py exited with status {status!r} after {time.time() - t0:.1f} s")
    return 0
