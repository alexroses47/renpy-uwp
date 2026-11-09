# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

# IsroTaxo menu screens. Gamepad: D-pad to move, A to choose, B to go back.

define isro_bg = "#070e1f"

style isro_text:
    size 32
    color "#e8eaf6"
    line_spacing 6

style isro_small is isro_text:
    size 26
    color "#9aa3c7"

style isro_title is isro_text:
    size 54
    color "#ff8a3c"
    bold True

style isro_box:
    background Solid("#10172e")
    padding (30, 20, 30, 20)

style isro_button:
    xsize 1100
    padding (40, 10, 40, 10)
    background Solid("#151d38")
    hover_background Solid("#ff4f8b")
    insensitive_background Solid("#0c1226")

style isro_button_text:
    size 34
    color "#ffffff"
    hover_color "#070e1f"
    insensitive_color "#4b5375"

style isro_nav is isro_button:
    xsize 360

style isro_nav_text is isro_button_text


# Common page layout: flower, title, content (transcluded), B = back.
screen isro_page(title, back="back"):
    modal True
    add Solid(isro_bg)
    add "images/flower.png" xpos 70 ypos 40 zoom 0.42
    text title style "isro_title" xpos 260 ypos 80
    fixed:
        xpos 260 ypos 190 xsize 1600 ysize 860
        transclude
    key "game_menu" action Return(back)


screen hub():
    modal True
    default last = _last_game()
    add Solid(isro_bg)
    add "images/logo.png" xalign 0.5 ypos 30 zoom 0.72
    vbox:
        xalign 0.5 ypos 520
        spacing 16
        if last:
            textbutton (T("continue") + _esc(_game_title(last))) style "isro_button" action Function(_start_game, last) default_focus True
        textbutton T("load_game") style "isro_button" action Return("load") default_focus (last is None)
        textbutton T("guide") style "isro_button" action Return("guide")
        textbutton T("about") style "isro_button" action Return("about")
        textbutton T("language") style "isro_button" action Return("lang")
        textbutton T("quit") style "isro_button" action Return("quit")
    text (T("tagline") + "  ·  " + T("version") + " " + _host.app_version()) style "isro_small" xalign 0.5 yalign 0.97
    key "game_menu" action NullAction()


screen load_game():
    default games = _host.find_games()
    default last = _last_game()
    use isro_page(T("load_game")):
        vbox:
            spacing 12
            if games:
                text T("games_found") style "isro_text"
                for name, basedir, where in games[:8]:
                    textbutton ("{}   ({})".format(_esc(name), where)) style "isro_button" action Return(basedir) default_focus (basedir == last)
                if len(games) > 8:
                    text (T("more_games").format(len(games) - 8)) style "isro_small"
            else:
                text T("no_games") style "isro_text"
                text T("no_games_hint") style "isro_small"
            null height 16
            textbutton T("browse") style "isro_button" action Return("browse") default_focus (not games)
            textbutton T("back") style "isro_button" action Return("back")


screen browse(path, page):
    default entries = (_host.drives() if path is None else _subdirs(path))
    $ items = entries or []
    $ pages = max(1, (len(items) + BROWSE_PAGE - 1) // BROWSE_PAGE)
    $ shown = items[page * BROWSE_PAGE:(page + 1) * BROWSE_PAGE]
    use isro_page(T("browse_title"), back=("back", None)):
        vbox:
            spacing 10
            if path is None:
                text T("pick_drive") style "isro_text"
                if not items:
                    text T("no_drives") style "isro_small"
            else:
                text _esc(path) style "isro_text" color "#ff8a3c"
                text T("browse_hint") style "isro_small"
                textbutton T("load_this") style "isro_button" action Return(("select", path)) default_focus True
                textbutton T("up") style "isro_button" action Return(("up", None))
                if entries is None:
                    text T("cannot_read") style "isro_small"
            for e in shown:
                if path is None:
                    textbutton _esc(_root_label(e)) style "isro_button" action Return(("open", e))
                else:
                    $ full = os.path.join(path, e)
                    textbutton ((_esc(e) + T("is_game")) if _host.game_basedir(full) else _esc(e)) style "isro_button" action Return(("open", full))
            if pages > 1:
                hbox:
                    spacing 20
                    textbutton T("prev") style "isro_nav" action Return(("page", page - 1)) sensitive (page > 0)
                    text (T("page").format(page + 1, pages)) style "isro_small" yalign 0.5
                    textbutton T("next") style "isro_nav" action Return(("page", page + 1)) sensitive (page + 1 < pages)
            textbutton T("back_menu") style "isro_button" action Return(("back", None))


screen invalid_folder(path):
    use isro_page(T("invalid_title"), back="ok"):
        vbox:
            spacing 18
            text _esc(path) style "isro_text" color "#ff8a3c"
            text T("invalid_body") style "isro_text"
            frame:
                style "isro_box"
                vbox:
                    spacing 4
                    text "E:\\renpy\\games\\<game>\\" style "isro_text"
                    text "        game\\" style "isro_text"
                    text T("invalid_files") style "isro_text"
                    text T("invalid_dirs") style "isro_small"
            text T("invalid_perm") style "isro_small"
            textbutton T("ok") style "isro_button" action Return("ok") default_focus True


screen guide(page):
    use isro_page(T("guide_title").format(page + 1)):
        vbox:
            spacing 16
            if page == 0:
                text T("g0_title") style "isro_title" size 40
                text T("g0_1") style "isro_text"
                text T("g0_2") style "isro_text"
                text T("g0_3") style "isro_text"
                text T("g0_4") style "isro_text"
            elif page == 1:
                text T("g1_title") style "isro_title" size 40
                text T("g1_1") style "isro_text"
                text T("g1_2") style "isro_text"
                text T("g1_3") style "isro_text"
                text T("g1_4") style "isro_text"
            else:
                text T("g2_title") style "isro_title" size 40
                text T("g2_1") style "isro_text"
                text T("g2_2") style "isro_text"
                text T("g2_3") style "isro_text"
                text T("g2_4") style "isro_text"
            null height 10
            hbox:
                spacing 20
                textbutton T("prev") style "isro_nav" action Return(page - 1) sensitive (page > 0)
                textbutton T("next") style "isro_nav" action Return(page + 1) sensitive (page < 2) default_focus True
                textbutton T("back") style "isro_nav" action Return("back")


screen about():
    use isro_page(T("about")):
        vbox:
            spacing 14
            text T("about_title") style "isro_text" size 38 color "#ff8a3c"
            text (T("about_version") + _host.app_version() + "  ·  Ren'Py " + renpy.version_only + "  ·  Python " + sys.version.split()[0]) style "isro_text"
            text T("about_1") style "isro_text"
            text T("about_2") style "isro_text"
            text T("about_3") style "isro_small"
            text T("about_4") style "isro_small"
            null height 10
            textbutton T("back") style "isro_button" action Return("back") default_focus True
