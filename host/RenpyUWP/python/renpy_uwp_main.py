# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

"""IsroTaxo launcher: the equivalent of Ren'Py's renpy.py with sandbox paths (docs/RESEARCH.md 31.3, 33).

Run by the host as renpy_uwp_main.main() on the SDL thread, after the window has been handed over.
Flow (one Ren'Py run per process; switching between the menu and a game restarts the app):
  - no pending launch: the IsroTaxo menu (a small Ren'Py game in <package>\\menu) lets the player pick a
    game; it stores "launch = <basedir>" in LocalState\\renpy-uwp.ini and asks the host to restart.
  - "launch" present: it is consumed and that game runs; when it quits, the host restarts into the menu.
  - menu "Salir": the app exits.
Paths: renpy_base = <package>\\renpy-base (renpy.py, renpy\\common, sdk-fonts); the engine's .py code is in
python\\renpy.zip. Saves in LocalState\\saves (RENPY_PATH_TO_SAVES); Ren'Py logs in LocalState\\logs\\<game>.
Games are found in <drive>:\\renpy\\games\\<name>\\game (D: to Z:) and LocalState\\games\\<name>\\game, or any
folder chosen in the menu. "mode = media-test" in the ini runs the milestone 3 media test.

Returns 0 when Ren'Py ran (the host then restarts or exits, restoring the display mode), non-zero on a
launcher error (the host shows its diagnostic screen). Launcher log: LocalState\\renpy-launcher.log.
"""

import importlib.util
import os
import re
import string
import sys
import time
import traceback

GAMES_DIR = os.path.join("renpy", "games")
GAME_FILE_EXTS = (".rpy", ".rpyc", ".rpa", ".rpymc", ".rpym")
INI_NAME = "renpy-uwp.ini"
MENU_NAME = "isrotaxo-menu"


# --- settings (LocalState\\renpy-uwp.ini) --------------------------------------------------------------

def localstate():
    return sys._renpy_uwp_localstate


def ini_path():
    return os.path.join(localstate(), INI_NAME)


def read_ini(path=None):
    values = {}
    try:
        with open(path or ini_path(), encoding="utf-8") as f:
            for line in f:
                line = line.split("#", 1)[0].strip()
                if "=" in line:
                    k, v = line.split("=", 1)
                    values[k.strip().lower()] = v.strip()
    except FileNotFoundError:
        pass
    return values


def write_ini(values, path=None):
    path = path or ini_path()
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        f.write("# IsroTaxo settings (written by the app and by ci/set-game.sh)\n")
        for k, v in values.items():
            f.write(f"{k} = {v}\n")
    os.replace(tmp, path)


# --- games ------------------------------------------------------------------------------------------------

def has_game_files(directory):
    try:
        return any(n.lower().endswith(GAME_FILE_EXTS) for n in os.listdir(directory))
    except OSError:
        return False


def game_basedir(path):
    """The Ren'Py base directory for a folder the player chose, or None.

    Accepts the base directory itself (it contains game\\ with .rpy/.rpyc/.rpa files) or its game\\ folder."""
    p = os.path.normpath(path)
    if os.path.basename(p).lower() == "game" and has_game_files(p):
        return os.path.dirname(p)
    g = os.path.join(p, "game")
    if os.path.isdir(g) and has_game_files(g):
        return p
    return None


def drives():
    """Roots the player can browse: removable/extra drives that exist, and LocalState\\games."""
    roots = [f"{letter}:\\" for letter in string.ascii_uppercase[3:] if os.path.isdir(f"{letter}:\\")]
    internal = os.path.join(localstate(), "games")
    if os.path.isdir(internal):
        roots.append(internal)
    return roots


def find_games():
    """[(name, basedir, where)] for <drive>:\\renpy\\games\\<name> and LocalState\\games\\<name>."""
    found = []
    roots = [(f"{letter}:\\{GAMES_DIR}", f"{letter}:") for letter in string.ascii_uppercase[3:]]
    roots.append((os.path.join(localstate(), "games"), "LocalState"))
    for root, where in roots:
        try:
            names = sorted(os.listdir(root), key=str.lower)
        except OSError:
            continue
        for name in names:
            basedir = os.path.join(root, name)
            if game_basedir(basedir) == basedir:
                found.append((name, basedir, where))
    return found


def resolve_game(value):
    """A launch value is a base directory path, or a game folder name (ci/set-game.sh)."""
    if re.match(r"^[A-Za-z]:\\", value) or value.startswith("\\\\"):
        return game_basedir(value)
    for name, basedir, _where in find_games():
        if name.lower() == value.lower():
            return basedir
    return None


def request_game(basedir):
    """Called by the menu: remember the game, launch it after an app restart."""
    values = read_ini()
    values["launch"] = basedir
    values["last_game"] = basedir
    write_ini(values)
    request_restart()


def request_restart():
    import _renpy_uwp
    _renpy_uwp.request_restart()


def app_version():
    """Package version from AppxManifest.xml (e.g. 0.0.50.0)."""
    try:
        with open(os.path.join(sys._renpy_uwp_install, "AppxManifest.xml"), encoding="utf-8") as f:
            m = re.search(r"<Identity[^>]*\sVersion=\"([^\"]+)\"", f.read())
            return m.group(1) if m else "?"
    except OSError:
        return "?"


# --- platform stubs ---------------------------------------------------------------------------------------

def install_platform_stubs():
    """Modules that cannot work on Xbox, replaced before the game runs (no change to the game files).

    pypresence (Discord Rich Presence; KS:RE ships it in game/python-packages and starts a thread that
    calls Presence.connect()): Discord is not available on the console and its IPC named pipes are
    outside the sandbox, so connect() raises DiscordNotFound immediately, as on a PC without Discord.
    The public names KS:RE and pypresence users import are provided (docs/RESEARCH.md section 32.2).
    """
    import types

    if "pypresence" in sys.modules:
        return
    m = types.ModuleType("pypresence", "renpy-uwp stub: Discord Rich Presence is not available on Xbox")

    class PyPresenceException(Exception):
        pass

    names = {"PyPresenceException": PyPresenceException}
    for n in ("DiscordNotFound", "InvalidPipe", "InvalidArgument", "ServerError", "DiscordError",
              "ArgumentError", "EventNotFound", "PipeClosed", "ResponseTimeout", "ConnectionTimeout"):
        names[n] = type(n, (PyPresenceException,), {"__module__": "pypresence"})
    names["InvalidID"] = type("InvalidID", (names["DiscordError"],), {"__module__": "pypresence"})

    class Presence:
        def __init__(self, client_id=None, *args, **kwargs):
            self.client_id = client_id

        def connect(self, *args, **kwargs):
            raise names["DiscordNotFound"]("Discord is not available on Xbox (renpy-uwp)")

        def update(self, *args, **kwargs):
            raise names["PipeClosed"]("not connected")

        def clear(self, *args, **kwargs):
            raise names["PipeClosed"]("not connected")

        def close(self, *args, **kwargs):
            pass

    names.update(Presence=Presence, AioPresence=Presence, Client=Presence, AioClient=Presence, BaseClient=Presence)
    for k, v in names.items():
        v.__module__ = "pypresence"
        setattr(m, k, v)
    m.exceptions = m
    sys.modules["pypresence"] = m
    sys.modules["pypresence.exceptions"] = m


_logdir = None


def path_to_logdir(_basedir):
    """Replaces renpy.py's path_to_logdir (which returns the game's basedir, on the USB drive).
    A module-level function: Ren'Py pickles the globals of modules named renpy* (renpy/__init__.py backup)."""
    return _logdir


def log(msg):
    print(time.strftime("%H:%M:%S"), msg, flush=True)


def main():
    out = open(os.path.join(localstate(), "renpy-launcher.log"), "w", encoding="utf-8", buffering=1)
    sys.stdout = sys.stderr = out
    try:
        return run(localstate(), sys._renpy_uwp_install)
    except BaseException:
        traceback.print_exc()
        return 1
    finally:
        out.flush()


def run(localstate_dir, install):
    ini = read_ini()
    log(f"IsroTaxo launcher {app_version()}; ini: {ini}")

    if ini.get("mode") == "media-test":
        import media_test
        return media_test.main()

    launch = ini.pop("launch", None)
    if launch is not None:
        write_ini(ini)  # one-shot: the next start shows the menu again
        basedir = resolve_game(launch)
        if basedir:
            run_renpy(localstate_dir, install, basedir, os.path.basename(basedir), ini)
            log("game finished: restarting into the menu")
            request_restart()
            return 0
        log(f"launch target {launch!r} is not a Ren'Py game folder; showing the menu")

    run_renpy(localstate_dir, install, os.path.join(install, "menu"), MENU_NAME, ini)
    return 0


def run_renpy(localstate_dir, install, basedir, name, ini):
    renpy_base = os.path.join(install, "renpy-base")
    saves = os.path.join(localstate_dir, "saves")
    logdir = os.path.join(localstate_dir, "logs", name)
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

    install_platform_stubs()

    # Ren'Py's own launcher module: bootstrap() calls renpy.__main__.path_to_gamedir & co.
    # Its module name must not start with "renpy": Ren'Py backs up (pickles) the globals of those modules.
    global _logdir
    _logdir = logdir
    spec = importlib.util.spec_from_file_location("uwp_renpy_launcher", os.path.join(renpy_base, "renpy.py"))
    launcher = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(launcher)
    launcher.path_to_logdir = path_to_logdir
    sys.modules["uwp_renpy_launcher"] = launcher

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
    return status
