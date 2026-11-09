# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

# IsroTaxo menu texts in English and Spanish. The language follows the console's preferred language
# (sys._renpy_uwp_language, set by the host) unless the player picked one ("lang" in renpy-uwp.ini).

init -20 python:
    import sys

    ISRO_STRINGS = {
        "tagline": ("Experimental Ren'Py 8 port for Xbox Series X|S", "Port experimental de Ren'Py 8 para Xbox Series X|S"),
        "version": ("version", "versión"),
        "continue": ("Continue: ", "Continuar: "),
        "load_game": ("Load game", "Cargar juego"),
        "guide": ("Guide", "Guía"),
        "about": ("About", "Acerca de"),
        "quit": ("Quit", "Salir"),
        "language": ("Idioma: Español", "Language: English"),
        "back": ("Back", "Volver"),
        "back_menu": ("Back to the menu", "Volver al menú"),
        "prev": ("< Previous", "< Anterior"),
        "next": ("Next >", "Siguiente >"),
        "page": ("Page {}/{}", "Página {}/{}"),
        "games_found": ("Games found in \\renpy\\games\\ on your drives:", "Juegos encontrados en \\renpy\\games\\ de tus unidades:"),
        "more_games": ("And {} more: use «Browse folders».", "Y {} más: usa «Explorar carpetas»."),
        "no_games": ("No game found in \\renpy\\games\\ on your drives.", "No se encontró ningún juego en \\renpy\\games\\ de tus unidades."),
        "no_games_hint": ("Copy each game to E:\\renpy\\games\\<name>\\game\\ (see Guide) or find it with «Browse folders».",
                          "Copia cada juego a E:\\renpy\\games\\<nombre>\\game\\ (ver Guía) o búscalo con «Explorar carpetas»."),
        "browse": ("Browse folders…", "Explorar carpetas…"),
        "browse_title": ("Browse folders", "Explorar carpetas"),
        "pick_drive": ("Choose a drive:", "Elige una unidad:"),
        "no_drives": ("No drives available. Connect the USB drive and set it up for media.", "No hay unidades disponibles. Conecta el USB y configúralo como «medios»."),
        "browse_hint": ("Open the game's folder (the one that contains the game folder) and press «Load this folder».",
                        "Entra en la carpeta del juego (la que contiene la carpeta game) y pulsa «Cargar esta carpeta»."),
        "load_this": ("Load this folder", "Cargar esta carpeta"),
        "up": (".. (up)", ".. (subir)"),
        "cannot_read": ("This folder cannot be read. Check the USB permissions (Guide).", "No se puede leer esta carpeta. Revisa los permisos del USB (Guía)."),
        "is_game": ("   · Ren'Py game", "   · juego Ren'Py"),
        "drive": ("Drive ", "Unidad "),
        "localstate": ("Games copied to the console (LocalState)", "Juegos copiados a la consola (LocalState)"),
        "invalid_title": ("That folder is not a Ren'Py game", "Esa carpeta no es un juego Ren'Py"),
        "invalid_body": ("The folder you load must contain a folder named game with the game files (.rpa, .rpyc or .rpy). You can pick the game's folder or the game folder itself.",
                         "La carpeta que cargues tiene que contener una carpeta llamada game con los archivos del juego (.rpa, .rpyc o .rpy). Puedes elegir la carpeta del juego o la propia carpeta game."),
        "invalid_files": ("                .rpa, .rpyc or .rpy files", "                archivos .rpa, .rpyc o .rpy"),
        "invalid_dirs": ("                (and folders such as images\\, audio\\...)", "                (y carpetas como images\\, audio\\...)"),
        "invalid_perm": ("If the layout is right and it still does not work, check the USB permissions (Guide > Prepare the USB drive).",
                         "Si la estructura es correcta y aun así no funciona, revisa los permisos del USB (Guía > Preparar el USB)."),
        "ok": ("Got it", "Entendido"),
        "guide_title": ("Guide ({}/3)", "Guía ({}/3)"),
        "g0_title": ("Prepare the USB drive", "Preparar el USB"),
        "g0_1": ("1. Format the USB drive as NTFS.", "1. Formatea el USB en NTFS."),
        "g0_2": ("2. Give apps permission. On Windows: right-click the drive > Properties > Security > Advanced > Add > Select a principal > type ALL APPLICATION PACKAGES > OK. Tick Full control, OK, tick «Replace all child object permission entries» and apply. Ignore the System Volume Information warning.",
                 "2. Da permiso a las apps. En Windows: clic derecho en la unidad > Propiedades > Seguridad > Opciones avanzadas > Agregar > Seleccionar una entidad de seguridad > escribe TODOS LOS PAQUETES DE APLICACIONES (ALL APPLICATION PACKAGES) > Aceptar. Marca Control total, acepta, marca «Reemplazar todas las entradas de permisos de objetos secundarios» y aplica. Ignora el aviso de System Volume Information."),
        "g0_3": ("3. If you copy games from Linux, see the guide in the repository: the files may lack that permission.",
                 "3. Si copias los juegos desde Linux, revisa la guía del repositorio: los archivos pueden quedar sin ese permiso."),
        "g0_4": ("4. When you connect the USB drive to the Xbox, choose to use it for media (not for games and apps).",
                 "4. Al conectar el USB a la Xbox, elige usarlo para medios (no para juegos y apps)."),
        "g1_title": ("Add games", "Poner juegos"),
        "g1_1": ("1. Use the PC (Windows or Linux) version of a game made with Ren'Py 8.", "1. Usa la versión de PC (Windows o Linux) de un juego hecho con Ren'Py 8."),
        "g1_2": ("2. Copy its game folder like this: E:\\renpy\\games\\<game name>\\game\\", "2. Copia su carpeta game así: E:\\renpy\\games\\<nombre del juego>\\game\\"),
        "g1_3": ("3. The game folder (with its .rpa, .rpyc or .rpy files) is enough. You do not need the renpy folder or the executables: IsroTaxo has its own Ren'Py.",
                 "3. Basta con la carpeta game (con sus archivos .rpa, .rpyc o .rpy). No hacen falta la carpeta renpy ni los ejecutables: IsroTaxo trae su propio Ren'Py."),
        "g1_4": ("4. Games in E:\\renpy\\games\\ show up in «Load game» by themselves. If a game is somewhere else, use «Browse folders».",
                 "4. Los juegos de E:\\renpy\\games\\ aparecen solos en «Cargar juego». Si el juego está en otra carpeta, usa «Explorar carpetas»."),
        "g2_title": ("Play", "Jugar"),
        "g2_1": ("• Works with Ren'Py 8 games. Ren'Py 7 or older games, and games that use Live2D, native extensions or Steam features, do not work.",
                 "• Funciona con juegos de Ren'Py 8. No funcionan los de Ren'Py 7 o anteriores, ni los que usan Live2D, extensiones nativas o funciones de Steam."),
        "g2_2": ("• Controller: A to advance and choose, B for the game menu, as in the PC version with a controller.",
                 "• Mando: A para avanzar y elegir, B para el menú del juego, como en la versión de PC con mando."),
        "g2_3": ("• Saves are stored on the console (not on the USB drive).", "• Las partidas se guardan en la consola (no en el USB)."),
        "g2_4": ("• Quitting a game brings you back to this menu.", "• Al salir de un juego vuelves a este menú."),
        "about_title": ("IsroTaxo — experimental Ren'Py 8 port for Xbox Series X|S", "IsroTaxo — port experimental de Ren'Py 8 para Xbox Series X|S"),
        "about_version": ("Version ", "Versión "),
        "about_1": ("Runs Ren'Py 8 games natively on an Xbox in Developer Mode (Dev Mode). No games are included: everyone brings their own.",
                    "Ejecuta juegos de Ren'Py 8 de forma nativa en una Xbox en modo desarrollador (Dev Mode). No incluye juegos: cada usuario pone los suyos."),
        "about_2": ("Source code and guide: github.com/alexroses47/renpy-uwp (MPL-2.0 license).", "Código fuente y guía: github.com/alexroses47/renpy-uwp (licencia MPL-2.0)."),
        "about_3": ("Components: Ren'Py (MIT), CPython (PSF), SDL2, SDL2_image and zlib (zlib), libpng, libjpeg-turbo, libwebp, FreeType, HarfBuzz, assimp and ANGLE (BSD/MIT/FTL licenses), FriBidi and FFmpeg (LGPL-2.1), SIL OFL fonts. The license texts are in the release's licenses folder and in THIRD_PARTY_NOTICES.md.",
                    "Componentes: Ren'Py (MIT), CPython (PSF), SDL2, SDL2_image y zlib (zlib), libpng, libjpeg-turbo, libwebp, FreeType, HarfBuzz, assimp y ANGLE (licencias BSD/MIT/FTL), FriBidi y FFmpeg (LGPL-2.1), fuentes SIL OFL. Los textos de las licencias van en la carpeta licenses de la release y en THIRD_PARTY_NOTICES.md."),
        "about_4": ("Ren'Py is the work of Tom Rothamel and contributors. Xbox is a trademark of Microsoft. This project is not affiliated with any of them.",
                    "Ren'Py es obra de Tom Rothamel y colaboradores. Xbox es una marca de Microsoft. Este proyecto no está afiliado a ninguno de ellos."),
    }

    def isro_language():
        chosen = _host.read_ini().get("lang", "")
        if chosen in ("en", "es"):
            return chosen
        system = getattr(sys, "_renpy_uwp_language", "") or ""
        return "es" if system.lower().startswith("es") else "en"

    isro_lang = isro_language()

    def T(key):
        en, es = ISRO_STRINGS[key]
        return es if isro_lang == "es" else en

    def isro_toggle_language():
        global isro_lang
        isro_lang = "en" if isro_lang == "es" else "es"
        values = _host.read_ini()
        values["lang"] = isro_lang
        _host.write_ini(values)
        renpy.restart_interaction()
