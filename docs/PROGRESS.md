# Progreso

## 2026-11-08 — Hito 0 (infraestructura), sesión 1

**Hecho**
- Investigación inicial en `docs/RESEARCH.md`: Ren'Py 8.5.3.26051504 → CPython 3.12.8; cómo compila
  renpy-build para Windows (llvm-mingw + cpython-mingw + ANGLE precompilado, no reutilizable para UWP);
  SDL2 2.0.20 tiene `VisualC-WinRT` y CMake `WindowsStore` (SDL3 ya no soporta WinRT); ANGLE recomendado:
  fork `SternXD/angle` (el que usa RetroArch UWP); openappx 0.7.0 para desplegar + API REST del Device Portal
  como respaldo; patrones de 2ship2harkinian-uwp y RetroArch para USB/mando/4K.
- Estructura del repo: `host/`, `deps/`, `python/`, `renpy/` (`VERSION`), `ci/`, `docs/`, `.github/workflows/`.
- `.gitignore` que bloquea scripts/archivos de Ren'Py, multimedia, fuentes, archivos comprimidos y certificados,
  y `ci/check-no-game-assets.sh` (se ejecuta en CI).
- Host mínimo C++/WinRT (`host/RenpyUWP`): CoreApplication + D3D11 que limpia a color verde azulado oscuro
  y escribe `LocalState\renpy-uwp.log`.
- Workflow `build-host.yml` (windows-2022): MSBuild → `.msix` → signtool (+ verificación) → artefacto
  `renpy-uwp-x64` con el paquete, VCLibs, el `.cer` y `package-info.json`.
- `ci/deploy.sh` (openappx, con respaldo curl/REST) y `ci/fetch-logs.sh` (REST con curl y pin de clave pública).
- `docs/SIGNING.md`: comandos de openssl probados localmente (OpenSSL 3.5.3) y carga con `gh secret set`.

- El usuario generó el certificado y cargó los secrets. Licencia del código propio: **MPL-2.0** (`LICENSE`,
  cabeceras Exhibit A; el código y los parches de terceros conservan su licencia).
- CI: 5 ejecuciones de `build-host`. Fallos corregidos: falta `winrt/Windows.ApplicationModel.Activation.h`
  (C3779); el empaquetado exige `mp:PhoneIdentity` (APPX1673). Run verde actual: 42520468937 (paquete 0.0.5.0).
- Despliegue real en la Xbox con `ci/deploy.sh` (openappx) y log descargado con `ci/fetch-logs.sh`, sin tocar la web.
  Problemas encontrados y corregidos (detalle en RESEARCH §10): `Name` del listado es el nombre visible; versiones
  duplicadas durante la actualización; lanzamiento inmediato tras reinstalar falla (se reintenta); log bloqueado
  para el portal (HTTP 423, ahora se abre en modo compartido); swapchain de 8×8 (ahora tamaño explícito).

- **Hito 0 cerrado (2026-11-08):** el usuario confirmó en la TV el splash y luego la pantalla verde azulado fija.
  Tag git `hito-0`.

**Siguiente paso**
- Hito 1: SDL2 2.0.20 WinRT + mando (A/B cambian el color), más dos pruebas pedidas por el usuario:
  acceso al USB desde C++ (unidades, listado de la raíz y lectura de `renpy-uwp-test.txt`) y salida HDMI
  (modos disponibles, cambio a 3840x2160 a 120 Hz, swapchain a la resolución real, fps medidos).

## 2026-11-08 — Hito 1 (SDL2 + mando + USB + HDMI), sesión 2

**Hecho**
- Hito 0 cerrado (tag `hito-0`). `CLAUDE.md`: requisito 3840x2160 a 120 Hz (TV Samsung S90D, HDMI 2.1) y criterio del Hito 5.
- Investigación en `docs/RESEARCH.md` §11–13; resultados en consola en §14.
- `deps/sdl2/build.ps1`: SDL2 2.0.20 (mismo tarball que renpy-build, SHA-256 verificado) con `VisualC-WinRT/SDL-UWP.vcxproj`,
  cacheado en CI con `actions/cache`.
- Host: SDL2 (ventana, eventos, mando: A/B color, X fps, Y prueba de USB, View modo por defecto, Menu reintenta 4K120),
  swapchain D3D11 propio de tamaño explícito, prueba de almacenamiento (Win32 vs FromApp vs WinRT) y prueba HDMI.
- Verificado en consola: tipo Game (5120 MB), mando detectado, `D:\` accesible, 4K a 119.88 Hz se activa con `HdrOption::None`.

**Falló / abierto**
- `EotfSdr` → E_INVALIDARG (resuelto con `None`).
- `Present` se bloquea ~2 s por frame tras el cambio de modo (y en un arranque sin cambio de modo). Sin explicar todavía.
- USB todavía no probado (el usuario debe prepararlo en NTFS con el ACL de ALL APPLICATION PACKAGES).

**Siguiente paso**
- Usuario: preparar el USB, abrir la app desde Dev Home y describir lo que ve en la TV (color, cambios con A/B, pantalla
  en negro tras el cambio a 4K120, efecto de View/Menu). Analizar el log con `ci/fetch-logs.sh`.

### 2026-11-08 — Hito 1, sesión 2 (continuación)
- USB preparado por el usuario (NTFS `RENPY`, ACL sternserv) con `\renpy-uwp-test.txt` y `\renpy\games\ksre\game\`.
  Layout registrado en `CLAUDE.md`. Prueba Y ampliada: lista y cuenta los juegos y mide la lectura sin caché.
- Observación del usuario: los botones cambian el color, pero en un momento la pantalla queda negra para siempre y View no responde.
  Causa encontrada: el swapchain de 3840x2160 reinicia la GPU (DEVICE_RESET) y el host no se recuperaba.
- Builds 0.0.12–0.0.14: swapchain con objeto de espera, recuperación de pérdida de dispositivo, matriz modo × swapchain.
  Resultados y conclusiones en RESEARCH §15: USB OK con APIs normales (25 MB/s sin caché); salida 4K120 OK; la app no supera 60 fps;
  swapchains > 1440p reinician la GPU.

**Siguiente paso (para cerrar el Hito 1)**
- Host estable por defecto: arrancar en salida 4K120 + swapchain 1440p (o en el modo que el usuario elija), liberar el swapchain
  antes del cambio de modo, registrar `QueryVideoMemoryInfo`, y dejar la matriz como diagnóstico opcional (botón) y no en el arranque.
- Prueba final del usuario (color, A/B, sin pantallas negras) y confirmación para cerrar el Hito 1.

### 2026-11-08 — Hito 1, build estable 0.0.16
- Decisión del usuario: por defecto salida 3840x2160 @ 119.88 Hz con swapchain 2560x1440. `CLAUDE.md` actualizado: el
  objetivo del Hito 5 pasa a ser 1440p escalado a 4K a 60 fps (límite de UWP), y queda anotado probar el juego en LocalState vs USB.
- Host: política por defecto con retroceso automático a 1080p60 si se pierde la GPU (o si los fps siguen bajos tras volver a
  medir), watchdog en el bucle principal, restauración del modo del sistema al suspender/salir (`CoreApplication::Suspending/
  Exiting`) y reaplicación al volver, `QueryVideoMemoryInfo` en el log, matriz de diagnóstico movida al botón RB.
- Medido en consola (0.0.16): política ACTIVA, **59.94 fps sostenidos 30 s, frame time 16.4–17.1 ms, 0 pausas**; VRAM 32 MB de
  un presupuesto de 3801 MB (la memoria no explica los reinicios con swapchains > 1440p).
- 0.0.15 retrocedió a 1080p60 por una pausa única de ~1 s tras el cambio de modo; corregido en 0.0.16 (repite la medida).

- Prueba del usuario (0.0.16): negro breve al cambiar de modo, luego colores; A/B responden al instante. Al salir la consola
  vuelve a 1080p60, que es su modo del sistema (todas las ejecuciones arrancan en 1920x1080 @ 59.94): el log muestra
  `lifecycle: suspending -> restoring the system display mode`. Política activa a 59.94 fps sin pausas.
- **Hito 1 cerrado (2026-11-08)** con la confirmación del usuario. Tag git `hito-1`.
- No verificado en esta prueba (el usuario salió directamente): volver a la app tras Home (reaplicación de 4K120) y View/Menu
  en la 0.0.16. Se comprobará al usar el host en el Hito 2.

**Siguiente paso**
- Hito 2 (decisivo): CPython 3.12.8 para UWP x64 embebido en el host. Investigar primero (RESEARCH): PCbuild con
  `WINAPI_FAMILY_APP`, parches de ms-iot/Greentwip, `CreateFileW` → `CreateFile2` (ya visto en el Hito 1), stdlib empaquetada.
  No empezar sin la confirmación del usuario.

## 2026-11-08 — Hito 2 (CPython embebido), sesión 3

**Hecho**
- Verificada la receta de Kodi (kodi-deps #33, fork `thexai/cpython@4c97e13`) y los PR de gh-152433 (RESEARCH §17–21).
  Decisión con visto bueno del usuario: base 3.12.11 del fork + serie de parches propia + biblioteca estática.
- `python/cpython-uwp/`: `CMakeLists.txt` derivado del del fork (PSF), 5 parches (CreateFile2 sin el truco "system",
  dynload, `st_size`, `_winapi` con `OSError(ENOSYS)`, módulos integrados), `build.ps1`, `make_stdlib_zip.py`; caché en CI.
- Host: `PythonHost.cpp` (hilo de 8 MB, `PyPreConfig` aislado con `utf8_mode`, `PyConfig` aislado, rutas relativas como Kodi),
  `python/smoke.py`. Auditoría `dumpbin /imports` en CI.
- 7 iteraciones de CI (PowerShell partía `-D...=10.0.22621.0`; macros del núcleo rompían expat; `pyconfig.h` para expat;
  `/sdl` y C4703; `in6addr_any` → `ws2_32.lib` aceptado; prueba de recursión mal planteada).
- **En la Xbox: `SMOKE DONE: 0 unexpected failure(s)`** (RESEARCH §23).
- `tools/dump-renpy-modules.sh` corregido (callbacks verificados en 8.5.3, protegidos, 4 momentos de volcado) y probado con el
  SDK oficial de Ren'Py 8.5.3 (`the_question`, guardar/cargar/salir automáticos).

- Volcado real de KS:RE (`docs/renpy-modules-ksre.json`, RESEARCH §24): a nuestro build solo le faltan los módulos excluidos
  (bz2, lzma, ssl, _hashlib, ctypes) y los exclusivos de POSIX. `ctypes` queda anotado para el Hito 4.
- Usuario: lo pendiente del Hito 1 funciona (volver desde Inicio reaplica 4K/120 tras un negro breve de resincronización
  HDMI; View y Menu OK).
- **Hito 2 cerrado (2026-11-08)** con la confirmación del usuario. Tag git `hito-2`.

**Siguiente paso**
- Hito 3 (dependencias C + pygame_sdl2): investigar primero en RESEARCH (recetas de renpy-build, versiones, ANGLE SternXD,
  FFmpeg LGPL, SDL2_image), incluir bz2/xz si son baratos. No empezar sin la confirmación del usuario.

## 2026-11-09 — Hito 3 (dependencias C + pygame_sdl2), sesión 4

**Hecho**
- Investigación en RESEARCH §25–29 (inventario de medios de KS:RE, versiones de renpy-build, ANGLE SternXD, Ren'Py estático,
  notas para el Hito 4).
- ANGLE (SternXD, `target_os="winuwp"`, solo D3D11) compila en CI con caché (`build-angle.yml`, 2 iteraciones). Aún no se empaqueta.
- Dependencias de medios en CI con caché: libpng, libjpeg-turbo, libwebp, freetype+harfbuzz, fribidi, SDL2_image, FFmpeg 4.3.1
  LGPL (solo matroska/ogg, vp9/opus/vorbis; configure confirma `HAVE_X86ASM 1` y `HAVE_AVX2_EXTERNAL 1`).
- Módulos de Ren'Py 8.5.3 estáticos (Cython 3.2.4): `_renpy`, `renpy.audio.renpysound`, `renpy.audio.filter`, `renpy.pygame.*`,
  con un parche de compatibilidad MSVC (`renpy/uwp/patches/0001-msvc-compat.diff`) y `vc_version.py` generado desde el tag.
- 14 iteraciones de build-host. Fallos corregidos: `tar` colgado ~2 h con `.tar.xz` (→ `tarfile` de Python); fribidi 1.0.7 usa
  `FRIBIDI_STATIC`; configure de FFmpeg ejecutaba pruebas UWP (→ `--enable-cross-compile`); `-lz` = `zlib.lib` con MSVC;
  `zconf.h` y `HAVE_UNISTD_H 0`; `setuplib` leía `.pyx` en cp1252 (→ `PYTHONUTF8=1`); VLA, `const int` y `lrint` en el C de
  Ren'Py; parche en CRLF (→ `.gitattributes`); nombres `lib*.lib` de libwebp. Cachés guardadas tras cada build.
- **En la Xbox (0.0.37.0):** pila de 8192 KB registrada; `renpy` 8.5.3.26051504 y `renpy.pygame` (WinRT, SDL 2.0.20) cargan;
  **PNG dibujado** (ventana `winrt` 1920x1080 sin GL); **benchmark VP9 1080p: 8 bits 774 fps, 10 bits 565 fps (202 fps con
  RGBA), threads=9 (frame)** con medios sintéticos.

**Falló**
- Reproducción vía `renpysound` (OGG y vídeo): el canal termina enseguida con duración 0, y en vídeo `python_read` lee un
  archivo cerrado (0 frames). FFmpeg decodifica bien abriendo por ruta (benchmark), así que el problema está en el camino
  RWops/ffmedia, aún sin diagnosticar.
- La ventana GL de pygame_sdl2 necesita ANGLE empaquetado (Hito 4); el test usa `renpy.pygame_AVOID_GL`.

- Usuario: vio las barras de colores (PNG) en la tele. Autoriza hasta 6 iteraciones más para `renpysound`.
- Diagnóstico (3 iteraciones extra): captura de `av_log`/`SDL_Log` a `LocalState\ffmpeg.log`, sonda RWops y variantes
  ruta/`open()`/`BytesIO`. Los tres fallos eran del test, no del port: `play()` tiene `end=0` por defecto (= duración 0 en
  `ffmedia.c`; Ren'Py pasa `end=-1`); el `open()` temporal se cerraba al liberarse; faltaba `advance_time()` por frame (el reloj
  de vídeo de `ffmedia`, que Ren'Py llama desde `display/core.py`).
- **En la Xbox (0.0.40.0): `MEDIA TEST DONE: 0 failure(s)`.** OGG Vorbis: duración 6,0 s, la posición avanza en tiempo real
  por las tres vías. VP9+Opus en MKV vía `renpysound`: 90 frames en 3 s (29,7 fps = tiempo real) en 8 bits y 89 en 10 bits, a
  1920x1080. Benchmark: 10 bits 1080p 558 fps (200 fps con RGBA).

- Decisión del usuario: FFmpeg queda enlazado estático, con cumplimiento de la LGPL-2.1 §6(a) documentado en
  THIRD_PARTY_NOTICES y RESEARCH §30 (FriBidi seguirá el mismo esquema).

- Usuario: probó en la tele y funciona (barras de colores, tonos y vídeo en movimiento).
- **Hito 3 cerrado (2026-11-09)** con la confirmación del usuario. Tag git `hito-3`.

**Siguiente paso**
- Hito 4 (Ren'Py arranca: "The Question" con mando). Primero la investigación en RESEARCH.

## 2026-11-09 — Hito 4 (Ren'Py arranca), sesión 5

**Hecho**
- Investigación en RESEARCH §31 (62 módulos, assimp, ANGLE/GLES, rutas del lanzador, usos de `ctypes`).
- Los 61 módulos de Ren'Py (todos menos Live2D; `tfd` aparte), parches `0001-msvc-compat` (más `alloca`) y
  `0002-uwp-sandbox` (presplash y locale sin `ctypes`).
- `deps/assimp` (5.4.3, glTF + STL) con su propia caché. `site-packages.zip` con los paquetes puros de renpy-build
  (ecdsa, rsa, six, requests...).
- Lanzador `python/renpy_uwp_main.py`: elige el juego en `<unidad>:\renpy\games\*\game\` (`renpy-uwp.ini`,
  `ci/set-game.sh`); partidas en `LocalState\saves`, logs en `LocalState\logs\<juego>`; renderer `gles2`. El host sale
  restaurando el modo HDMI cuando Ren'Py termina.
- ANGLE rehecho: los args de gn no se aplicaban nunca (`$args` es variable automática de PowerShell). Ahora va con
  `args.gn`, `is_clang=false`, `use_custom_libcxx=false` y verificación de dependencias con `dumpbin`. Se empaqueta
  `d3dcompiler_47.dll`.
- SDL 2.0.20: parche del formato de modo de pantalla en WinRT (enum DXGI en lugar de formato SDL).
- Swapchain de ANGLE en Xbox = modo HDMI: con 4K reinicia la GPU (`EGL_BAD_ALLOC`). El host deja el modo del sistema
  (`hdmi = system`) y baja a 1080p60 si supera 1440p. El 4K queda para el Hito 5.
- `sdk-fonts` en `renpy-base` (pantalla de Preferencias de The Question). Despliegue: reemisión del certificado del
  Device Portal con la misma clave detectada y aceptada automáticamente; `fetch-logs.sh` admite subcarpetas.
- **En la Xbox (0.0.47.0):** The Question con OpenGL ES 3.0 (ANGLE, D3D11) a 1080p; el usuario jugó con el mando, oyó el
  audio, guardó, cargó, probó los idiomas y salió. Log limpio (estado 0).
- Unas 9 iteraciones de CI (host y ANGLE).

**Avisos conocidos (inofensivos)**
- Ren'Py no puede escribir `.rpyc` de `renpy/common` en el paquete (compila en memoria en cada arranque, ~1,7 s) ni
  `game/cache` en el USB.

- **Hito 4 cerrado (2026-11-09)** con la confirmación del usuario. Tag git `hito-4`.

**Siguiente paso**
- Hito 5 (KS:RE): investigación hecha en RESEARCH §32. Arrancar KS:RE, vídeos, USB frente a LocalState, memoria y
  decidir el 1440p.

## 2026-11-09 — Hito 5 (KS:RE) y release IsroTaxo 0.1.56, sesión 6

**Hecho**
- Investigación de KS:RE 2.0.5 en RESEARCH §32. `pypresence` neutralizado desde el lanzador (stub en
  `sys.modules`). KS:RE arranca en la Xbox: menú, mando, OpenGL ES 3.0 a 1080p, script en 11 s desde USB.
- Memoria en el menú de KS:RE como tipo App: working set 774 MB, ~96 MB libres de ~1,25 GB. Hay que usarlo como
  tipo **Game** (pendiente de remedir).
- **IsroTaxo**: nombre, logo del autor (iconos, splash; `host/tools/make_branding.py`) y menú propio en Ren'Py
  (`host/RenpyUWP/menu`): continuar, cargar juego (detectados + explorador de carpetas con ayuda sobre la
  estructura), guía, acerca de, salir. Menú ↔ juego reiniciando la app (`CoreApplication::RequestRestartAsync`).
  Probado en la Xbox por el usuario.
- README nuevo (instalación, guía de permisos USB, guía de juegos, compatibilidad, licencias, capturas en la Xbox
  con The Question, cuyo arte es MIT). `ci/screenshot.sh` (Device Portal `/ext/screenshot`; no capta la guía de
  Xbox con una app en primer plano: se usaron fotos de la TV, sin EXIF).
- Versiones 0.1.x. **Pre-release v0.1.56** publicado con msix, VCLibs, zip de licencias y SHA256SUMS, atado al
  commit `09e0f28`.
- CI: GitHub dejó de arrancar `build-host` en `main` (sin jobs, «workflow file issue»); el mismo archivo funcionaba
  en otras ramas. Se resolvió renombrando el grupo de `concurrency`.

**Pendiente del Hito 5**
- Aceptación: Acto 1 completo de KS:RE, guardar, salir, volver, cargar.
- Vídeos dentro de KS:RE; USB frente a copia en LocalState (copia automatizada por Device Portal); memoria como
  tipo Game; decidir el 1440p (KS:RE es 1080p virtual).
