# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

init -30 python:
    import os
    import sys
    import renpy_uwp_main as _host

    BROWSE_PAGE = 7
    HIDDEN_DIRS = {"system volume information", "$recycle.bin", "wpsettings.dat"}

    def _esc(s):
        """Text from the file system, shown literally (no [interpolation] or {text tags})."""
        return str(s).replace("[", "[[").replace("{", "{{")

    def _subdirs(path):
        try:
            names = os.listdir(path)
        except OSError:
            return None
        out = []
        for n in sorted(names, key=str.lower):
            if n.lower() in HIDDEN_DIRS or n.startswith("."):
                continue
            if os.path.isdir(os.path.join(path, n)):
                out.append(n)
        return out

    def _parent(path):
        if path is None:
            return None
        p = os.path.dirname(os.path.normpath(path))
        # Leaving a drive root (E:\) or LocalState\games goes back to the list of roots.
        if os.path.normpath(path) in [os.path.normpath(r) for r in _host.drives()] or p == os.path.normpath(path):
            return None
        return p

    def _root_label(root):
        if root.endswith(":\\"):
            return T("drive") + root[:2]
        return T("localstate")

    def _last_game():
        b = _host.read_ini().get("last_game")
        if b and _host.game_basedir(b) == b:
            return b
        return None

    def _game_title(basedir):
        return os.path.basename(os.path.normpath(basedir))

    def _start_game(basedir):
        _host.request_game(basedir)
        renpy.quit(save=False)

    def _quit():
        renpy.quit(save=False)

label main_menu:
    # No Ren'Py main menu: go straight to the IsroTaxo hub.
    return

label start:
    jump hub

label hub:
    call screen hub
    if _return == "load":
        call browse_games
    elif _return == "guide":
        call guide
    elif _return == "about":
        call screen about
    elif _return == "lang":
        $ isro_toggle_language()
    elif _return == "quit":
        $ _quit()
    jump hub

label browse_games:
    call screen load_game
    if _return == "back":
        return
    if _return == "browse":
        call browse_folders
        return
    $ _start_game(_return)
    return

label browse_folders:
    $ browse_path = None
    $ browse_page = 0
    while True:
        call screen browse(browse_path, browse_page)
        $ act, arg = _return
        if act == "back":
            return
        elif act == "open":
            $ browse_path, browse_page = arg, 0
        elif act == "up":
            $ browse_path, browse_page = _parent(browse_path), 0
        elif act == "page":
            $ browse_page = arg
        elif act == "select":
            $ chosen = _host.game_basedir(arg)
            if chosen:
                $ _start_game(chosen)
            else:
                call screen invalid_folder(arg)

label guide:
    $ guide_page = 0
    while True:
        call screen guide(guide_page)
        if _return == "back":
            return
        $ guide_page = _return
