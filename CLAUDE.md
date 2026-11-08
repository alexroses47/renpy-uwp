# Proyecto: Ren'Py 8 → UWP (Xbox Dev Mode) para correr Katawa Shoujo: Re-Engineered

Responde siempre en español. Código, comentarios y mensajes de commit en inglés.

## Objetivo

Portar el motor **Ren'Py 8** a una app **UWP** que corra en una **Xbox Series S en Dev Mode**, y usarla para jugar
**Katawa Shoujo: Re-Engineered (KS:RE)**. El port debe ser genérico: un "runtime" de Ren'Py para UWP que carga
cualquier juego Ren'Py 8 desde una carpeta externa. KS:RE es el primer juego objetivo.

No es un port vía web/WebView: es nativo (CPython embebido + SDL2 + ANGLE + módulos C de Ren'Py).
Si el Hito 2 resulta inviable, detente y propón el plan B (contenedor UWP con WebView y la exportación web de Ren'Py).

## Entorno del usuario (respétalo)

- Máquina de desarrollo: **openSUSE Tumbleweed (Linux)**. **No hay Windows** disponible.
- Toda compilación para Windows/UWP se hace en **GitHub Actions** (`runs-on: windows-2022`, fijado; no `windows-latest`).
- El usuario controla CI desde la terminal con la CLI `gh` (`gh workflow run`, `gh run watch`, `gh run view --log-failed`,
  `gh run download`).
- TV: **Samsung S90D** conectada por **HDMI 2.1**.
- Consola: Xbox Series S, Dev Mode, devkit type "Universal Windows App Devkit", OS 10.0.26100.x, sandbox XDKS.1.
- Device Portal: `https://192.168.100.102:11443`. Credenciales del Device Portal en variables de entorno
  (`XBOX_DP_USER`, `XBOX_DP_PASS`), **nunca** en el repo ni en logs.
- Despliegue desde Linux: probar **openappx** (`pip install openappx[sign]`, comando `openappx deploy`). Si falla,
  alternativa: API REST del Windows Device Portal con `curl`, y último recurso la web del Device Portal (manual).
- KS:RE está instalado sobre la copia de Steam (app 3068300) en `~/.local/share/Steam`. Los datos del juego se
  copian a un USB para la Xbox.
- **USB de juegos (layout estándar):** NTFS, etiqueta `RENPY`, ACL de sternserv (Everyone + ALL APPLICATION PACKAGES,
  Control total) en todos los archivos y carpetas; en la consola aparece como `E:\` (tipo removable). Estructura:
  ```
  E:\renpy-uwp-test.txt                  ← archivo de prueba del host
  E:\renpy\games\<juego>\game\          ← carpeta game/ de cada juego Ren'Py (p. ej. E:\renpy\games\ksre\game\)
  ```
  El runtime busca juegos en `<unidad>:\renpy\games\*\game\`. exFAT no se usa (sin ACL; no probado).

## Reglas no negociables

1. **Nunca** commitear recursos de juegos (imágenes, audio, video, `.rpa`, scripts de KS). Están bajo
   CC BY-NC-ND. El repo solo contiene el runtime; el usuario aporta el juego. Añade reglas en `.gitignore`.
2. Respetar licencias: Ren'Py (MIT, con dependencias de otras licencias), KS:RE (MPL-2.0 para código).
   Compilar FFmpeg **solo con componentes LGPL** (sin `--enable-gpl` ni `--enable-nonfree`). Mantener un
   `THIRD_PARTY_NOTICES.md` actualizado.
3. Secretos solo en GitHub Secrets: `UWP_CERT_PFX_BASE64`, `UWP_CERT_PASSWORD`. El `CN` del certificado debe
   coincidir exactamente con `Publisher` en `Package.appxmanifest`.
4. Un hito a la vez. No avances al siguiente sin cumplir los criterios de aceptación del actual y sin
   confirmación del usuario tras probar en la Xbox.
5. Antes de escribir código de un hito, investiga y deja las decisiones en `docs/RESEARCH.md`
   (fuentes, versiones exactas, alternativas descartadas y por qué).
6. Lleva `docs/PROGRESS.md`: fecha, qué se hizo, qué falló, siguiente paso. Actualízalo al final de cada sesión.
7. Commits pequeños y descriptivos. Cada cambio a CI debe poder probarse con un `gh workflow run`.
8. Pide al usuario solo lo que solo él puede hacer (instalar/probar en la Xbox, describir lo que ve,
   crear secrets, generar el certificado). Dale instrucciones exactas, paso a paso, con comandos copiables.

## Arquitectura objetivo

```
App UWP (host C++/WinRT, CoreApplication)
 ├─ Logging desde la primera línea → LocalState\renpy-uwp.log (también stdout/stderr de Python)
 ├─ SDL2 (backend WinRT/UWP)        → ventana, mando (GameController), audio
 ├─ CPython 3.x embebido (build UWP) → misma versión de Python que usa Ren'Py 8.5.x
 │   ├─ stdlib empaquetada en el appx (zip o carpeta), PYTHONHOME dentro del paquete
 │   └─ Ren'Py 8.5.x (.py) + pygame_sdl2 + módulos C/Cython de Ren'Py compilados para UWP
 ├─ Dependencias C: zlib, libpng, libjpeg-turbo, libwebp, freetype, harfbuzz, fribidi,
 │   SDL2_image, FFmpeg (LGPL: avformat/avcodec/swresample/swscale)
 ├─ ANGLE (OpenGL ES → Direct3D 11) para el renderer GL de Ren'Py
 └─ Datos:
     ├─ Juego: carpeta en el USB (E:\renpy\games\<juego>\game\) o LocalState como alternativa
     └─ Partidas y persistentes: LocalState (config.savedir)
```

Requisito de salida de vídeo: **el objetivo final es 3840x2160 (4K) a 120 Hz** en la Samsung S90D (HDMI 2.1).
El host debe pedir ese modo con `HdmiDisplayInformation` y crear el swapchain a la resolución real. Si la consola,
la API o la configuración lo impiden, documentar qué lo limita exactamente y usar el mejor modo alcanzable.

Restricciones UWP que hay que asumir y verificar:
- Sandbox AppContainer: acceso a disco limitado al paquete (solo lectura), LocalState/LocalCache/TempState
  y almacenamiento extraíble según capacidades del manifiesto. Sin `subprocess`, sin escribir junto al ejecutable.
- La ventana UWP reporta 1080p aunque la TV esté en 4K. Para 1440p/4K hay que crear el swapchain al tamaño
  real (patrón usado por RetroArch UWP: leer `HdmiDisplayInformation`). SDL2 en UWP no lo hace solo.
- Ejecutar como tipo **Game** (no App) en Dev Home para tener más memoria y GPU.
- Nivel de Direct3D limitado en UWP sobre Xbox: verificar la tabla de Chuck Walbourn
  ("DirectX and UWP on Xbox Series X|S") antes de elegir backend de ANGLE.

## Referencias a estudiar (prior art)

- `Greentwip/cpython`: CPython **3.4** portado a UWP/Xbox One con Pygame y un juego completo corriendo
  (carpeta `UWP/`). Guía para los parches de CPython en UWP (rutas, zona horaria, os.*).
- `ms-iot/python` (archivado): CPython modificado para apps UWP en Windows 10 IoT Core (`Tools/pyuwp`).
- CPython upstream: `PC/` y `PCbuild/` (cómo se compila en Windows; `python_uwp.cpp` es para el paquete
  de la Store, no es una build UWP sandbox, no confundir).
- `renpy/renpy-build`: cómo Ren'Py compila Python y todas sus dependencias por plataforma. La meta es
  añadir/emular un destino "uwp" o replicar sus pasos con MSVC para UWP.
- `renpy/pygame_sdl2`.
- SDL2: soporte WinRT/UWP (proyectos `VisualC-WinRT` o CMake con `CMAKE_SYSTEM_NAME=WindowsStore`; verificar
  cuál funciona con la versión de SDL2 que use Ren'Py).
- ANGLE para UWP: Google retiró el soporte WinRT; evaluar el fork de Microsoft (`microsoft/angle`, rama
  para Windows Store / NuGet `ANGLE.WindowsStore`) y cómo lo usó Servo para UWP.
- Ports UWP de código abierto para Xbox Dev Mode como patrones de almacenamiento USB, mando, 4K y
  empaquetado: `XboxEmulationHub/2ship2harkinian-uwp`, RetroArch (código UWP en `uwp/`), DuckStation-UWP
  (`DevModeUWP/DuckStation-UWP`). Guía de la comunidad: wiki.sternserv.xyz.
- Plantilla `justin-credible/xbox-uwp-sdl2-starter` (C# + SDL2): útil como referencia de manifiesto y SDL2 en Xbox,
  aunque nuestro host será C++.
- openappx (PyPI): empaquetar/firmar/desplegar desde Linux; `openappx deploy --device https://IP:11443 ...`.

## Hitos y criterios de aceptación

### Hito 0 — Infraestructura
- Repo con estructura: `host/` (app UWP), `deps/` (scripts/recetas de dependencias), `python/`, `renpy/`
  (submódulo o fetch en CI de la versión fijada), `ci/`, `docs/`, `.github/workflows/`.
- Instrucciones para el usuario: generar certificado de firma en Linux con `openssl` (EKU codeSigning, CN = Publisher),
  convertir a base64, cargar secrets con `gh secret set`.
- Workflow que compila y publica un artefacto `.msix` firmado + dependencias (VCLibs).
- Script `ci/deploy.sh` que despliega a la Xbox desde Linux (openappx o REST) y `ci/fetch-logs.sh` que descarga
  `LocalState\renpy-uwp.log` vía API REST del Device Portal.
- **Aceptación:** `gh workflow run` → artefacto → `ci/deploy.sh` instala en la Xbox sin tocar la web.

### Hito 1 — Host UWP + SDL2
- App C++/WinRT mínima que inicializa SDL2 (UWP), abre ventana a pantalla completa, limpia a un color,
  lee el mando (cambia el color con A/B) y escribe un log en LocalState desde la primera línea.
- **Aceptación:** en la Xbox se ve el color, responde al mando y el log se descarga con `ci/fetch-logs.sh`.

### Hito 2 — CPython embebido (DECISIVO)
- Fijar la versión exacta de Python que usa Ren'Py 8.5.x (confirmar en `renpy-build`).
- Compilar CPython para UWP (x64) + stdlib; embeberlo en el host; ejecutar un script que escriba en el log
  `sys.version`, `sys.path`, prueba de `zlib`, `json`, `os.listdir` sobre LocalState y sobre el USB.
- **Aceptación:** el log en la Xbox muestra todo lo anterior sin errores.
- **Si tras un esfuerzo razonable no es viable:** detente, documenta el bloqueo en `docs/RESEARCH.md`
  y propón el plan B (WebView + exportación web de Ren'Py 8).

### Hito 3 — Dependencias + pygame_sdl2
- Compilar para UWP: zlib, libpng, libjpeg-turbo, libwebp, freetype, harfbuzz, fribidi, SDL2_image,
  FFmpeg (LGPL) y ANGLE; luego pygame_sdl2 como módulos de extensión del Python embebido.
- Cachear dependencias en CI (`actions/cache`) con clave por versión/receta: los builds completos son lentos.
- **Aceptación:** script Python en la Xbox que, vía pygame_sdl2, dibuja una imagen PNG y reproduce un OGG.

### Hito 4 — Ren'Py arranca
- Compilar los módulos C/Cython de Ren'Py 8.5.x; bootstrap equivalente a `renpy.py` con rutas UWP
  (base del juego configurable, savedir en LocalState). Parchear lo que la sandbox bloquee, documentando cada parche.
- **Aceptación:** el juego de ejemplo de Ren'Py ("The Question") corre en la Xbox con mando.

### Hito 5 — KS:RE jugable
- Cargar KS:RE desde el USB; partidas en LocalState; videos (FFmpeg); fuentes; mando; tipo Game.
- Resolución real: objetivo **3840x2160 a 120 Hz** (swapchain al tamaño HDMI, modo pedido con `HdmiDisplayInformation`);
  si no es alcanzable, el mejor modo posible con la limitación documentada. Medir memoria con el Device Portal (Performance).
- **Aceptación:** se puede jugar el Acto 1 completo, guardar, salir, volver y cargar, con la salida en 3840x2160 a 120 Hz
  (o en el mejor modo alcanzable documentado en `docs/RESEARCH.md`) y los fps reales registrados en el log.

## Flujo de trabajo en cada sesión

1. Lee `docs/PROGRESS.md` y `docs/RESEARCH.md`.
2. Investiga lo necesario (web, repos de referencia) antes de cambiar código del hito actual.
3. Cambios pequeños → commit → `gh workflow run` → `gh run watch` → si falla, `gh run view --log-failed`,
   corrige y repite. Resume al usuario solo lo relevante.
4. Cuando haya un build listo para probar: despliega (o da el comando exacto), y pide al usuario la prueba concreta
   ("abre X desde Dev Home y dime qué ves"). Luego baja y analiza el log.
5. Actualiza `docs/PROGRESS.md` al terminar.
