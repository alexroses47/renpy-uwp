# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

# IsroTaxo menu: a small Ren'Py game shipped in the package (<package>\menu). It lists and browses
# games, shows the guide and the about page, and asks the launcher (python/renpy_uwp_main.py) to
# start the chosen game after an app restart.

define config.name = "IsroTaxo"
define config.version = "0.1"
define config.save_directory = "isrotaxo-menu"
define config.window_title = "IsroTaxo"

define config.screen_width = 1920
define config.screen_height = 1080
define config.default_fullscreen = True

# Nothing to save or roll back in a menu.
define config.has_autosave = False
define config.has_quicksave = False
define config.autosave_on_quit = False
define config.rollback_enabled = False
define config.window = "hide"

# The B button / Escape open no game menu; each screen handles "game_menu" as "back".
define _game_menu_screen = None
define config.quit_action = Function(renpy.quit, save=False)
