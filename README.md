<p align="center"><img src="host/branding/isrotaxo-logo.png" alt="IsroTaxo" width="560"></p>

# IsroTaxo — port experimental de Ren'Py 8 para Xbox Series X|S

**IsroTaxo** ejecuta juegos hechos con **Ren'Py 8** de forma **nativa** en una Xbox Series X|S en
**modo desarrollador** (Dev Mode). No es una versión web ni un emulador. Es el motor de Ren'Py 8.5
compilado para la consola como app UWP:

- Python 3.12 integrado y los módulos nativos de Ren'Py;
- SDL2 para la ventana, el mando y el audio;
- ANGLE para OpenGL ES sobre Direct3D 11;
- FFmpeg para el audio y el vídeo.

Primer juego probado de principio a fin: *Katawa Shoujo: Re-Engineered*.

> **Estado: experimental.** Funciona con los juegos probados, pero puede tener fallos con otros. Este
> repositorio **no incluye ni acepta juegos**: cada persona pone los suyos.

## Qué necesitas

- Una **Xbox Series X|S en modo desarrollador** (Dev Mode), con acceso al Device Portal desde tu PC.
- Un **USB formateado en NTFS** con los permisos para apps que explica la guía de abajo.
- La versión de **PC (Windows o Linux)** de un juego hecho con **Ren'Py 8**.

## Instalar IsroTaxo

1. Descarga de [Releases](../../releases) el archivo `IsroTaxo_<versión>_x64.msix` y la dependencia
   `Microsoft.VCLibs.x64.14.00.appx`.
2. En tu PC abre el **Device Portal** de la consola (`https://<IP-de-la-Xbox>:11443`), ve a
   **Home > My games & apps > Add**, elige el `.msix` y, en el paso de dependencias, añade el `.appx`
   de VCLibs.
3. En la consola, en **Dev Home**, selecciona **IsroTaxo**, pulsa **View details** y pon
   **App type: Game**. Así la consola le da más memoria, y KS:RE la necesita.
4. Abre IsroTaxo. Aparece su menú: **Cargar juego**, **Guía**, **Acerca de** y **Salir**.

## Guía 1: preparar el USB (permisos)

Las apps de la Xbox solo pueden leer un USB si tiene permisos para «todos los paquetes de aplicaciones».

**En Windows** (el método más fiable):

1. Formatea el USB en **NTFS**: clic derecho en la unidad > **Formatear** > Sistema de archivos **NTFS** >
   **Iniciar**.
2. Clic derecho en la unidad > **Propiedades** > pestaña **Seguridad** > **Opciones avanzadas** >
   **Agregar** > **Seleccionar una entidad de seguridad** > **Opciones avanzadas** > **Buscar ahora**.
   Elige **TODOS LOS PAQUETES DE APLICACIONES** (*ALL APPLICATION PACKAGES*) > **Aceptar**.
3. Marca **Control total** > **Aceptar**. Marca **Reemplazar todas las entradas de permisos de objetos
   secundarios** > **Aceptar**.
4. Si aparece un error sobre *System Volume Information*, ignóralo y continúa.
5. Al conectar el USB a la Xbox, elige usarlo **para medios** (no para «juegos y apps»).

Copia los juegos **después** de dar los permisos. Si los copias desde Windows, heredan los permisos de la carpeta.

**En Linux:** los archivos copiados con `ntfs-3g` pueden quedar **sin** ese permiso, aunque la carpeta lo
tenga. La guía de la comunidad sternserv explica cómo comprobarlo y corregirlo con `getfattr`/`setfattr`:
[Copying games from Linux](https://wiki.sternserv.xyz/docs/xbox-setup/copying-games-from-linux).
Si un juego está en el USB pero IsroTaxo no lo ve o no lo abre, casi siempre es esto.

## Guía 2: poner juegos

Copia la carpeta **`game`** de cada juego dentro de `renpy\games\<nombre-del-juego>\` en el USB:

```
E:\
└── renpy\
    └── games\
        ├── ksre\
        │   └── game\          ← la carpeta game del juego (archivos .rpa, .rpyc o .rpy)
        └── otro_juego\
            └── game\
```

- Basta con la carpeta `game`. **No** hacen falta la carpeta `renpy`, la carpeta `lib` ni los ejecutables
  del juego: IsroTaxo trae su propio Ren'Py 8.5.
- Los juegos de `E:\renpy\games\` aparecen solos en **Cargar juego**.
- Si el juego está en otra carpeta, entra en **Cargar juego > Explorar carpetas**. Navega con el mando hasta
  la carpeta del juego (la que contiene `game`, o la propia `game`) y pulsa **Cargar esta carpeta**.
  Si eliges una carpeta que no es un juego, IsroTaxo te muestra la estructura correcta.
- Las **partidas** se guardan en la consola (en el almacenamiento de la app), no en el USB.
- Al salir de un juego vuelves al menú de IsroTaxo.

## Compatibilidad

| Juego | ¿Funciona? |
|---|---|
| Ren'Py **8.x** (archivos `.rpa`, `.rpyc`, `.rpy` normales) | Sí, en general. Probados: *The Question* (ejemplo del SDK) y *Katawa Shoujo: Re-Engineered*. |
| Ren'Py **7.x o anterior** (Python 2) | No. |
| Juegos con **Live2D**, extensiones nativas propias (`.pyd`/`.dll`), `ctypes` o funciones de Steam | No, o sin esas partes (por ejemplo, sin logros de Steam ni Discord). |
| Juegos pensados solo para ratón | Arrancan, pero manejarlos con mando puede ser incómodo. |

Salida de vídeo actual: el modo del sistema de la consola (normalmente 1080p). El modo 4K está en desarrollo.

## Para desarrolladores

Todo se compila en GitHub Actions (`windows-2022`); el desarrollo se hace desde Linux con `gh`.

```bash
gh workflow run build-host.yml && gh run watch
export XBOX_DP_USER=... XBOX_DP_PASS=...   # credenciales del Device Portal (nunca en el repo)
ci/deploy.sh                    # instala el último build en la consola
ci/set-game.sh ksre             # opcional: abre ese juego directamente en el próximo arranque
ci/fetch-logs.sh                # baja LocalState\renpy-uwp.log (y --file logs/<juego>/log.txt)
```

| Ruta | Contenido |
|---|---|
| `host/` | App UWP (host C++/WinRT, menú de IsroTaxo en `host/RenpyUWP/menu`, lanzador Python) |
| `deps/` | Recetas de dependencias nativas para UWP (SDL2 + parches, medios, FFmpeg, assimp, ANGLE) |
| `python/` | CPython 3.12 para UWP |
| `renpy/` | Ren'Py fijado (`renpy/VERSION`), sus parches y el build estático de los módulos |
| `ci/` | Despliegue y logs desde Linux, comprobaciones de CI |
| `docs/` | `RESEARCH.md` (decisiones y hallazgos), `PROGRESS.md`, `SIGNING.md` |

El certificado de firma se prepara una vez (`docs/SIGNING.md`).

## Licencias

El código original de IsroTaxo (host, menú, lanzador, scripts y recetas) está bajo la
**Mozilla Public License 2.0** ([`LICENSE`](LICENSE)). El logo de IsroTaxo es obra del autor del proyecto.

Los componentes de terceros conservan sus licencias, y también nuestros parches a sus archivos: Ren'Py (MIT),
CPython (PSF), SDL2, SDL2_image y zlib (zlib), libpng, libjpeg-turbo, libwebp, FreeType, HarfBuzz, assimp y
ANGLE (BSD/MIT/FTL), FriBidi y FFmpeg (LGPL-2.1, enlazados según la §6(a)), fuentes SIL OFL. Los detalles
están en [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md), y cada release incluye la carpeta `licenses/`.

Los juegos no forman parte del proyecto y conservan sus licencias (por ejemplo, *Katawa Shoujo:
Re-Engineered*: código MPL-2.0 y recursos CC BY-NC-ND).

Ren'Py es obra de Tom Rothamel y colaboradores. Xbox es una marca de Microsoft. IsroTaxo no está afiliado a
ninguno de ellos.
