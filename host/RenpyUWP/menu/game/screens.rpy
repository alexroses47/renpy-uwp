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
            textbutton ("Continuar: " + _esc(_game_title(last))) style "isro_button" action Function(_start_game, last) default_focus True
        textbutton "Cargar juego" style "isro_button" action Return("load") default_focus (last is None)
        textbutton "Guía" style "isro_button" action Return("guide")
        textbutton "Acerca de" style "isro_button" action Return("about")
        textbutton "Salir" style "isro_button" action Return("quit")
    text ("Port experimental de Ren'Py 8 para Xbox Series X|S  ·  versión " + _host.app_version()) style "isro_small" xalign 0.5 yalign 0.97
    key "game_menu" action NullAction()


screen load_game():
    default games = _host.find_games()
    default last = _last_game()
    use isro_page("Cargar juego"):
        vbox:
            spacing 12
            if games:
                text "Juegos encontrados en \\renpy\\games\\ de tus unidades:" style "isro_text"
                for name, basedir, where in games[:8]:
                    textbutton ("{}   ({})".format(_esc(name), where)) style "isro_button" action Return(basedir) default_focus (basedir == last)
                if len(games) > 8:
                    text ("Y {} más: usa «Explorar carpetas».".format(len(games) - 8)) style "isro_small"
            else:
                text "No se encontró ningún juego en \\renpy\\games\\ de tus unidades." style "isro_text"
                text "Copia cada juego a E:\\renpy\\games\\<nombre>\\game\\ (ver Guía) o búscalo con «Explorar carpetas»." style "isro_small"
            null height 16
            textbutton "Explorar carpetas…" style "isro_button" action Return("browse") default_focus (not games)
            textbutton "Volver" style "isro_button" action Return("back")


screen browse(path, page):
    default entries = (_host.drives() if path is None else _subdirs(path))
    $ items = entries or []
    $ pages = max(1, (len(items) + BROWSE_PAGE - 1) // BROWSE_PAGE)
    $ shown = items[page * BROWSE_PAGE:(page + 1) * BROWSE_PAGE]
    use isro_page("Explorar carpetas", back=("back", None)):
        vbox:
            spacing 10
            if path is None:
                text "Elige una unidad:" style "isro_text"
                if not items:
                    text "No hay unidades disponibles. Conecta el USB y configúralo como «medios»." style "isro_small"
            else:
                text _esc(path) style "isro_text" color "#ff8a3c"
                text "Entra en la carpeta del juego (la que contiene la carpeta game) y pulsa «Cargar esta carpeta»." style "isro_small"
                textbutton "Cargar esta carpeta" style "isro_button" action Return(("select", path)) default_focus True
                textbutton ".. (subir)" style "isro_button" action Return(("up", None))
                if entries is None:
                    text "No se puede leer esta carpeta. Revisa los permisos del USB (Guía)." style "isro_small"
            for e in shown:
                if path is None:
                    textbutton _esc(_root_label(e)) style "isro_button" action Return(("open", e))
                else:
                    $ full = os.path.join(path, e)
                    textbutton ((_esc(e) + "   · juego Ren'Py") if _host.game_basedir(full) else _esc(e)) style "isro_button" action Return(("open", full))
            if pages > 1:
                hbox:
                    spacing 20
                    textbutton "< Anterior" style "isro_nav" action Return(("page", page - 1)) sensitive (page > 0)
                    text ("Página {}/{}".format(page + 1, pages)) style "isro_small" yalign 0.5
                    textbutton "Siguiente >" style "isro_nav" action Return(("page", page + 1)) sensitive (page + 1 < pages)
            textbutton "Volver al menú" style "isro_button" action Return(("back", None))


screen invalid_folder(path):
    use isro_page("Esa carpeta no es un juego Ren'Py", back="ok"):
        vbox:
            spacing 18
            text _esc(path) style "isro_text" color "#ff8a3c"
            text "La carpeta que cargues tiene que contener una carpeta llamada game con los archivos del juego (.rpa, .rpyc o .rpy). Puedes elegir la carpeta del juego o la propia carpeta game." style "isro_text"
            frame:
                style "isro_box"
                vbox:
                    spacing 4
                    text "E:\\renpy\\games\\mi_juego\\" style "isro_text"
                    text "        game\\" style "isro_text"
                    text "                archivos .rpa, .rpyc o .rpy" style "isro_text"
                    text "                (y carpetas como images\\, audio\\...)" style "isro_small"
            text "Si la estructura es correcta y aun así no funciona, revisa los permisos del USB (Guía > Preparar el USB)." style "isro_small"
            textbutton "Entendido" style "isro_button" action Return("ok") default_focus True


screen guide(page):
    use isro_page("Guía ({}/3)".format(page + 1)):
        vbox:
            spacing 16
            if page == 0:
                text "Preparar el USB" style "isro_title" size 40
                text "1. Formatea el USB en NTFS." style "isro_text"
                text "2. Da permiso a las apps. En Windows: clic derecho en la unidad > Propiedades > Seguridad > Opciones avanzadas > Agregar > Seleccionar una entidad de seguridad > escribe TODOS LOS PAQUETES DE APLICACIONES (ALL APPLICATION PACKAGES) > Aceptar. Marca Control total, acepta, marca «Reemplazar todas las entradas de permisos de objetos secundarios» y aplica. Ignora el aviso de System Volume Information." style "isro_text"
                text "3. Si copias los juegos desde Linux, revisa la guía del repositorio: los archivos pueden quedar sin ese permiso." style "isro_text"
                text "4. Al conectar el USB a la Xbox, elige usarlo para medios (no para juegos y apps)." style "isro_text"
            elif page == 1:
                text "Poner juegos" style "isro_title" size 40
                text "1. Usa la versión de PC (Windows o Linux) de un juego hecho con Ren'Py 8." style "isro_text"
                text "2. Copia su carpeta game así: E:\\renpy\\games\\<nombre del juego>\\game\\" style "isro_text"
                text "3. Basta con la carpeta game (con sus archivos .rpa, .rpyc o .rpy). No hacen falta la carpeta renpy ni los ejecutables: IsroTaxo trae su propio Ren'Py." style "isro_text"
                text "4. Los juegos de E:\\renpy\\games\\ aparecen solos en «Cargar juego». Si el juego está en otra carpeta, usa «Explorar carpetas»." style "isro_text"
            else:
                text "Jugar" style "isro_title" size 40
                text "• Funciona con juegos de Ren'Py 8. No funcionan los de Ren'Py 7 o anteriores, ni los que usan Live2D, extensiones nativas o funciones de Steam." style "isro_text"
                text "• Mando: A para avanzar y elegir, B para el menú del juego, como en la versión de PC con mando." style "isro_text"
                text "• Las partidas se guardan en la consola (no en el USB)." style "isro_text"
                text "• Al salir de un juego vuelves a este menú." style "isro_text"
            null height 10
            hbox:
                spacing 20
                textbutton "< Anterior" style "isro_nav" action Return(page - 1) sensitive (page > 0)
                textbutton "Siguiente >" style "isro_nav" action Return(page + 1) sensitive (page < 2) default_focus True
                textbutton "Volver" style "isro_nav" action Return("back")


screen about():
    use isro_page("Acerca de"):
        vbox:
            spacing 14
            text "IsroTaxo — port experimental de Ren'Py 8 para Xbox Series X|S" style "isro_text" size 38 color "#ff8a3c"
            text ("Versión " + _host.app_version() + "  ·  Ren'Py " + renpy.version_only + "  ·  Python " + sys.version.split()[0]) style "isro_text"
            text "Ejecuta juegos de Ren'Py 8 de forma nativa en una Xbox en modo desarrollador (Dev Mode). No incluye juegos: cada usuario pone los suyos." style "isro_text"
            text "Código fuente y guía: github.com/alexroses47/renpy-uwp (licencia MPL-2.0)." style "isro_text"
            text "Componentes: Ren'Py (MIT), CPython (PSF), SDL2, SDL2_image y zlib (zlib), libpng, libjpeg-turbo, libwebp, FreeType, HarfBuzz, assimp y ANGLE (licencias BSD/MIT/FTL), FriBidi y FFmpeg (LGPL-2.1), fuentes SIL OFL. Los textos de las licencias van en la carpeta licenses de la release y en THIRD_PARTY_NOTICES.md." style "isro_small"
            text "Ren'Py es obra de Tom Rothamel y colaboradores. Xbox es una marca de Microsoft. Este proyecto no está afiliado a ninguno de ellos." style "isro_small"
            null height 10
            textbutton "Volver" style "isro_button" action Return("back") default_focus True
