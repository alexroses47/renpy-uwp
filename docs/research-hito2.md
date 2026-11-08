# Investigación externa — Hito 2: CPython 3.12 embebido en UWP (Xbox Series S, Dev Mode)

> Origen: investigación hecha fuera del repo (Claude, búsqueda web) el 2026-11-08.
> Marcas: **[C]** confirmado por fuente citada · **[I]** inferencia a verificar.

## Resumen

- **CPython 3.12 en UWP sobre Xbox Series S ya existe.** Kodi distribuye CPython **3.12.11** compilado para UWP x64
  (fork `thexai/cpython` + receta en `xbmc/kodi-deps`), probado por su desarrollador en una **Xbox Series S en Dev Mode**
  (Debug y Release). 3.12.11 cumple `>=3.12.8,<3.13` de Ren'Py 8.5. Recomendación: partir de ese fork/receta, no portar
  desde cero.
  - [C] xbmc/xbmc PR #26946: "Tested UWP x64 on Xbox Series S (Debug and Release)"; pycryptodome probado en Series S (dev mode).
  - [C] xbmc/kodi-deps PR #33: paquetes `python-3.12.11-win10-x64-v143-20250706.7z`, runner Windows Server 2022.
- **Upstream CPython (2026)** aceptó en `main` una serie de parches UWP/Xbox (issue **gh-152433**, autor thexai):
  #152601 (socketmodule), #152689 (ceval.c: omitir SetThreadStackGuarantee), #152792 y #154700 (_ctypes),
  #152797 (dynload_win.c: rutas relativas en UWP), #152962 (_winapi), #153036 (overlapped.c), #154918 (winreg import
  condicional). **No hay backport a 3.12** (rama solo de seguridad): aplicar a mano lo que falte en el fork.
- Ports viejos solo como referencia: Kodi 3.8 / Paxxi (issue xbmc/xbmc #18846: LoadPackagedLibrary, Py_WINVER, fijar
  home para pasar `_PyUnicode_InitEncodings`), kodi-deps #20 (evitar Shlwapi.lib, restricciones de CreateFile2),
  Greentwip/cpython 3.4, ms-iot/python. PPSSPP documenta W^X en UWP (RW→RX con VirtualProtectFromApp).

## APIs problemáticas (función → dónde → alternativa → acción)

| API | Dónde en CPython 3.12 | Alternativa UWP | Acción |
|---|---|---|---|
| LoadLibraryExW | Python/dynload_win.c | LoadPackagedLibrary (rutas relativas del paquete) | [C] parche Kodi/#152797. Con módulos built-in se puede desactivar dynload. |
| SetThreadStackGuarantee | Python/ceval.c | no existe | [C] omitir (#152689). Compensar con hilo de stack grande. |
| VirtualAlloc RWX | Modules/_ctypes/malloc_closure.c | VirtualAllocFromApp + VirtualProtectFromApp RW→RX + capability codeGeneration | [C] **Excluir _ctypes en Hito 2.** |
| CreateFileW | posixmodule.c, fileutils.c, _io/winconsoleio.c, mmapmodule.c, winreg.c | CreateFile2 | [C] restricciones: share mode ≠ 0, solo FILE_ATTRIBUTE_NORMAL, parámetros extendidos limpios. [I] shim por header forzado (/FI). |
| CreateProcessW y afines | Modules/_winapi.c, posixmodule (system, startfile, kill, spawn) | no hay procesos | [C] #152962. [I] **mantener _winapi compilado con stubs** que lancen OSError (shutil/subprocess/tempfile lo importan). |
| Registro (RegOpenKeyExW…) | PC/winreg.c, Modules/getpath.c+getpath.py, platform.py, mimetypes.py | — | [C] #154918. [I] excluir winreg; fijar home y module_search_paths_set=1 para que getpath no toque el registro; stubs si no enlaza. |
| Consola (GetConsoleMode, ReadConsoleW…) | _io/winconsoleio.c, posixmodule (get_terminal_size), signalmodule.c, msvcrtmodule.c | — | [I] excluir msvcrt; stubs en winconsoleio; install_signal_handlers=0. |
| GetVersionExW / GetFileVersionInfoW | Python/sysmodule.c | — | [I] stub con valor fijo. |
| GetUserNameW | posixmodule (os.getlogin) | — | [I] stub OSError. |
| BCryptGenRandom | Python/bootstrap_hash.c | permitido | [C] ya se usa desde 3.11; nada que hacer. |
| Winsock | socketmodule.c, selectmodule.c | disponible | [C] #152601. Opcional. |
| IOCP / overlapped | Modules/overlapped.c | parcial | [C] #153036. [I] excluir _overlapped/asyncio. |
| CreateFileMappingW/MapViewOfFile | mmapmodule.c | *FromApp | [I] excluir mmap. |
| Shlwapi | rutas | PathCch* | [C] Kodi evitó enlazar Shlwapi.lib. |

**Excluir en Hito 2:** msvcrt, winreg, _overlapped, mmap, _ctypes, _ssl, _hashlib, _sqlite3, _tkinter, _multiprocessing,
winsound, _lzma, _bz2, pyexpat (salvo necesidad). **Mantener _winapi con stubs.**

## Build

- Primero: leer el bloque `python` de `xbmc/kodi-deps` (CMakeLists.txt, workflow de Actions) y el árbol de
  `thexai/cpython` en el commit fijado por kodi-deps #33 (3.12.11). Copiar flags/defines/módulos excluidos/parches.
  (Que el fork use su propio build CMake viene de resúmenes de PR, no de lectura directa: **verificar**.)
- Si se toca PCbuild a mano [I]: `AppContainerApplication=true`, `ApplicationType=Windows Store`,
  `ApplicationTypeRevision=10.0`, `WindowsTargetPlatformVersion=10.0.22621.0`; `_WIN32_WINNT=0x0A00`; subir
  Py_WINVER/Py_NTDDI en PC/pyconfig.h.
- Si es CMake: `CMAKE_SYSTEM_NAME=WindowsStore`, `CMAKE_SYSTEM_VERSION=10.0`, y **definir explícitamente**
  `WINAPI_FAMILY=WINAPI_FAMILY_APP` y `_WIN32_WINNT=0x0A00` (CMake no lo hace solo; harfbuzz #1576, docs vcpkg UWP).
- Hacer fallar APIs no permitidas: `/we4013` (función no declarada = error en C); enlazar **solo WindowsApp.lib**
  (+ `/NODEFAULTLIB` de kernel32/user32/advapi32/shell32/ole32/version/shlwapi; limpiar AdditionalDependencies);
  auditoría `dumpbin /imports` contra la lista de APIs UWP.
- Inicialización [I] (Kodi confirma que fijar home/paths fue imprescindible):
  `PyPreConfig_InitIsolatedConfig` + `utf8_mode=1`; `PyConfig_InitIsolatedConfig`; `home` = InstalledLocation;
  `module_search_paths_set=1` con `python312.zip` (+ `Lib`); `write_bytecode=0` (o `pycache_prefix` en LocalState);
  `install_signal_handlers=0`; `parse_argv=0`; `site_import=0`; `safe_path=1`.
- Stdlib: `python312.zip` con .pyc precompilados generado en CI con un Python **3.12** del host (`PC/layout --zip-lib
  --precompile` o `compileall -b` + zip). Extensiones no van en el zip.

## Módulos built-in vs .pyd

- Hito 2 (rápido): estrategia Kodi, python312.dll + .pyd en el paquete vía LoadPackagedLibrary.
- Hito 3 en adelante (recomendado): **estático** — stdlib C vía `PC/config.c` (`_PyImport_Inittab`), y módulos
  Cython de Ren'Py/pygame_sdl2 con `PyImport_AppendInittab("renpy.display.render", PyInit_render)` antes de inicializar.
- [C] renpy-build #217 (oct 2026): los binarios mingw de renpy-build no sirven para MSVC/UWP → recompilar todo con MSVC.
- Lista de módulos (validar): núcleo de 3.12; [C, renpyweb] binascii, _struct, math, _pickle, _random, _datetime, zlib,
  _md5, array, _sha1, _sha256, _sha512; [I] probables _json, _bisect, _heapq, unicodedata, _contextvars, _blake2, _sha3,
  _statistics, _queue, select, _socket (opcional), _winapi (stubs).
- **Validación obligatoria:** en un PC, correr KS:RE con Ren'Py 8.5.3 oficial y volcar `sorted(sys.modules)` y
  `sys.builtin_module_names` tras menú + una partida. Esa lista manda.

## Dependencias C

zlib: sí (RPA, guardados). OpenSSL: omitir (hashlib cae a _md5/_sha*/_blake2). libffi/_ctypes: omitir (W^X;
`windll.kernel32` falla en UWP). sqlite3, bz2, xz, expat, mpdecimal: omitir.

## Riesgos

- 3.12 no tiene JIT; solo ctypes necesita memoria ejecutable → sin _ctypes no hace falta codeGeneration.
- Sin SetThreadStackGuarantee: **correr Python en un hilo propio con stack grande (p. ej. 8 MB)**; Ren'Py es sensible
  (parseo recursivo, pickle profundo).
- Hilos: probar `threading` en el humo (Ren'Py usa hilos en escritorio).
- stdout/stderr pueden ser None: redirigir a `LocalState\python.log`; `faulthandler.enable()` a otro archivo.
- Ren'Py usa `os.environ["APPDATA"]` en renpy.py para guardados en Windows → fijar APPDATA (u override de savedir) a
  LocalState antes de arrancar Ren'Py.
- `utf8_mode=1`; comprobar zona horaria (`time.tzname`, `datetime.now()`).
- InstalledLocation es solo lectura; shim de CreateFile2 ingenuo puede romper open()/os.stat.
- Sin verificar: si Ren'Py 8.5 usa ctypes/windll al arrancar en Windows (p. ej. DPI) → proteger o stub.

## Plan propuesto

1. Estudiar receta Kodi (kodi-deps + thexai/cpython @ commit de #33) y documentar en RESEARCH.md.
2. Versión: 3.12.11 del fork (o rebase de sus parches sobre v3.12.8 para alinear con renpy-build).
3. Job CI `cpython-uwp` (windows-2022): Release x64, WINAPI_FAMILY_APP, /we4013, solo WindowsApp.lib, módulos excluidos;
   artefactos python312.dll/.lib, include/, python312.zip; verificación de imports con dumpbin.
4. Backport manual de parches de gh-152433 que falten.
5. Host: hilo de 8 MB → Py_PreInitialize(utf8) → Py_InitializeFromConfig.
6. `smoke.py`: redirigir logs; sys.version, sys.path, builtin_module_names, utf8_mode; imports en bucle con OK/error;
   leer `E:\renpy\games\ksre\game\…` (64 bytes de un archivo); escribir y releer en LocalState; lanzar y unir un Thread;
   time.tzname.
7. Desplegar con ci/deploy.sh y bajar logs con ci/fetch-logs.sh.
8. Iterar sobre fallos de import con stubs.

Estimación [I]: con fork Kodi, dificultad media (1–2 semanas hobby); desde cero, alta (3–6 semanas).
Bloqueos probables: acoplamiento del fork al build de Kodi; getpath tocando registro; _io/posixmodule sin enlazar;
desbordamiento de stack; shutil/tempfile por _winapi.

## Plan B

- B1: 3.13+ (el fork de Kodi lo soporta) — exige parchear Ren'Py y recompilar Cython; no recomendado.
- B2: exportación web de Ren'Py 8 (sin hilos, documentado): probar KS:RE web en el Edge de la Xbox; o contenedor UWP
  con WebView.

## Fuentes principales

- https://github.com/xbmc/xbmc/pull/26946 · https://github.com/xbmc/kodi-deps/pull/33 · https://github.com/thexai/kodi-deps
- https://github.com/xbmc/xbmc/pull/30362 · https://github.com/xbmc/kodi-deps/pull/82 · https://github.com/xbmc/kodi-deps/pull/81
- https://github.com/xbmc/xbmc/issues/18846 · https://github.com/xbmc/kodi-deps/pull/20
- CPython: #152601 #152689 #152797 #154700 (gh-152433) — https://github.com/python/cpython/pull/152797
- https://github.com/python/cpython/blob/main/PC/python_uwp.cpp
- https://learn.microsoft.com/en-us/uwp/win32-and-com/win32-apis · https://learn.microsoft.com/en-us/uwp/win32-and-com/win32-and-com-for-uwp-apps
- https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualprotectfromapp
- https://github.com/harfbuzz/harfbuzz/issues/1576 · https://github.com/MicrosoftDocs/vcpkg-docs/blob/main/vcpkg/users/platforms/uwp.md
- https://github.com/renpy/renpyweb/blob/master/Python3-Modules-Setup.local · https://github.com/renpy/renpy-build/pull/217
- https://github.com/renpy/renpy-build/pull/219 · https://github.com/renpy/renpy/blob/master/renpy.py · https://www.renpy.org/doc/html/web.html
- https://blog.henrikrydgard.com/porting-to-uwp/ · https://github.com/Greentwip/cpython · https://github.com/ms-iot/python
