# Investigación — Ren'Py 8 → UWP (Xbox Series S, Dev Mode)

Documento vivo. Cada sección dice qué se verificó, con qué fuente y en qué fecha.
"No confirmado" significa exactamente eso: no lo pude comprobar en una fuente primaria.

Fecha de esta revisión: **2026-11-08** (Hito 0).

---

## 1. Versión de Ren'Py y de Python

| Dato | Valor | Fuente |
|---|---|---|
| Última estable 8.5.x | **8.5.3.26051504** (publicada 2026-05-16) | `gh api repos/renpy/renpy/releases` (no hay release más reciente a 2026-11-08; 8.5.2.26010301, 8.5.1.25123106 y 8.5.0.25111603 son las anteriores) |
| Tag equivalente en renpy-build | `renpy-8.5.3.26051504` | `gh api repos/renpy/renpy-build/tags` |
| Python (Linux/mac/Android/iOS) | **CPython 3.12.8** | `renpy-build/tasks/python3.py`: `version = "3.12.8"` |
| Python (Windows) | **CPython 3.12.7** (fork `msys2-contrib/cpython-mingw`) | `renpy-build/tasks/python3.py`: `win_version = "3.12.7"` |
| Python (web) | 3.12.8 | `web_version = "3.12.8"` |
| Requisito del código de Ren'Py | `requires-python = ">=3.12.8,<3.13"` | `renpy/pyproject.toml` en el tag `8.5.3.26051504` |

**Decisión:** fijar **CPython 3.12.8** para UWP. Motivo: es la versión que exige `pyproject.toml`
de Ren'Py y la que usan todas las plataformas salvo Windows; en Windows usan 3.12.7 solo porque es la
última que tiene el fork MinGW. Nosotros compilaremos con MSVC desde CPython upstream (`PCbuild/`),
así que no dependemos de ese fork. Se reconfirmará al empezar el Hito 2.

Nota: en el tag 8.5.3 `pygame_sdl2` ya **no es un repo aparte** para Ren'Py: el código C de pygame vive
en `renpy/src/pygame/` (y los `.pyx` en el árbol de Ren'Py). El repo `renpy/pygame_sdl2` queda como
referencia histórica. A verificar en detalle en el Hito 3.

## 2. Cómo compila renpy-build para Windows

Fuente: clon de `renpy/renpy-build` en el tag `renpy-8.5.3.26051504` (commit `7bfab40c`).

- **No usa MSVC.** Compila Windows **desde Linux (Ubuntu 24.04)** con el toolchain cruzado
  **llvm-mingw 20241217 (UCRT)** de mstorsjo (`tasks/toolchain.py`).
- Python para Windows: `msys2-contrib/cpython-mingw` (3.12.7) + parches propios
  (`patches/Python-3.12.7/single-dllmain.diff`, `no-af-hyperv.diff`), configurado con autotools.
- **ANGLE no se compila**: usa binarios precompilados `prebuilt/x86_64/{libEGL.dll, libGLESv2.dll,
  d3dcompiler_47.dll}` (`tasks/angle.py`). Son binarios Win32 de escritorio, no válidos para UWP.
- Dependencias y versiones (`version = ...` en `tasks/*.py` y tarballs en `source/`):

| Dependencia | Versión | Licencia (a confirmar al compilarla) |
|---|---|---|
| zlib | 1.3.1 | zlib |
| bzip2 | 1.0.8 | bzip2 (BSD-like) |
| xz (liblzma) | 5.4.2 | 0BSD / dominio público |
| brotli | 1.1.0 | MIT |
| libffi | 3.4.5 | MIT |
| openssl | 3.3.2 | Apache-2.0 |
| libpng | 1.6.43 | libpng |
| libjpeg-turbo | 1.5.3 | IJG + BSD-3 |
| libwebp | 1.3.2 | BSD-3 |
| freetype | 2.13.3 (+ `freetypehb`, segunda build con harfbuzz) | FTL / GPLv2 (usaremos FTL) |
| harfbuzz | 8.0.1 | MIT ("Old MIT") |
| fribidi | 1.0.7 | LGPL-2.1+ |
| SDL2 | **2.0.20** (+ parches en `patches/SDL2-2.0.20/`) | zlib |
| SDL2_image | 2.6.2 | zlib |
| libavif | v0.11.1 | BSD-2 |
| aom | v3.5.0 | BSD-2 + patente AOM |
| libyuv | HEAD de chromium (sin fijar) | BSD-3 |
| assimp | v5.4.3 | BSD-3 |
| FFmpeg | **4.3.1** | LGPL-2.1+ (sin `--enable-gpl`) |

- FFmpeg en renpy-build: `--disable-everything` y solo habilita demuxers (ogg, matroska/webm, mov, mp3,
  wav, avi, mpegts, av1…), decoders (vorbis, opus, theora, vp8, vp9, mpeg1/2/4, flac, pcm, libaom_av1),
  `--enable-w32threads`, `--disable-d3d11va --disable-dxva2`. **No** pasa `--enable-gpl` ni `--enable-nonfree`
  → la configuración ya es LGPL. La replicaremos.
- Cython: se instala en el venv de renpy-build (versión no fijada en `tasks/cython.py`; usa la del `uv.lock`
  de Ren'Py — a fijar en el Hito 4).

**Consecuencia para nosotros:** no podemos reutilizar renpy-build tal cual (llvm-mingw desktop + binarios
Win32). Opciones evaluadas:
1. **MSVC en GitHub Actions** (`windows-2022`) con `WINAPI_FAMILY=WINAPI_FAMILY_APP`: es lo que usan
   RetroArch y 2ship UWP. **Elegida.**
2. llvm-mingw tiene soporte de destinos UWP (`*-w64-mingw32uwp`), y existe
   [`gianlucamazza/uwp-crossbuild`](https://github.com/gianlucamazza/uwp-crossbuild) (MIT, activo en 2026-10)
   para compilar C++/WinRT UWP desde Linux con clang-cl + lld-link + SDK bajo Wine. Descartada por ahora:
   el usuario ya tiene CI en Windows y mezclar CRTs (UCRT de MinGW vs. VCLibs) complica depurar. Queda como
   plan alternativo si MSVC bloquea algo.

## 3. SDL2 y UWP (WinRT)

Verificado con `gh api repos/libsdl-org/SDL/contents?ref=<tag>`:

| Versión SDL2 | `VisualC-WinRT/` | CMake `WindowsStore` |
|---|---|---|
| 2.0.20 (la de Ren'Py 8.5.3) | Sí | Sí (`SDL_BUILDING_WINRT=1` en `CMakeLists.txt`) |
| 2.30.9 | Sí | Sí |
| 2.32.10 (última 2.x) | Sí | Sí (+ define `WINAPI_FAMILY=WINAPI_FAMILY_APP`) |

- El backend WinRT de SDL2 está escrito en **C++/CX** (`src/core/winrt/*.cpp`, compila con `/ZW`).
  Funciona con MSVC v143; no con clang/mingw. Otra razón para MSVC.
- **SDL3 eliminó WinRT/UWP** (se sustituyó por GDK). Hay que quedarse en SDL2.
- Patrón probado en Xbox: 2ship2harkinian-uwp arranca con `SDL_WinRTRunApp(bootstrap, NULL)` desde
  `WinMain`, y SDL2 se encarga de CoreWindow y del mando (vía `Windows.Gaming.Input`; Ren'Py ya parchea
  `fix-windows_gaming_input.diff` en 2.0.20).
- **Decisión provisional (Hito 1):** compilar **SDL2 2.0.20 + los parches de Ren'Py que apliquen** con CMake
  `-DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0`. Si 2.0.20 da problemas en WinRT
  (bugs ya corregidos aguas arriba), subir a **2.32.10** y comprobar que pygame de Ren'Py no use APIs nuevas
  (la API de SDL2 es retrocompatible). `VisualC-WinRT` (proyecto VS) queda como respaldo.
- Para el host del Hito 0/1 usamos C++/WinRT (no C++/CX): conviven sin problema; solo SDL2 se compila con `/ZW`.

## 4. ANGLE para UWP (2026)

| Opción | Estado | Valoración |
|---|---|---|
| NuGet `ANGLE.WindowsStore` (Microsoft) | Última 2.1.13, publicada **2016-12-28** (2.1.14 está deslistada). | Muy antigua (GLES 2/3.0 parcial, bugs conocidos). Descartada salvo emergencia. |
| `microsoft/angle` (ramas `ms-master`, `winrt`, `uwp-2017`) | Sin cambios desde **2023-06-13** (último push); no archivado. | Igual de antigua en lo funcional. Descartada. |
| Google ANGLE upstream | Retiró el soporte WinRT (no se puede compilar para UWP sin parches). | Requiere parches. |
| **`SternXD/angle`** (fork de upstream, descripción "A conformant OpenGL ES implementation for UWP") | Último push **2025-03-10**. Es el que **usa RetroArch** para UWP: `pkg/msvc/msvc-uwp/ANGLE/README.md` dice que sus binarios x64/ARM64 salen de ese fork, basado en el commit upstream `6c2737be88`. | **Recomendada.** Moderna, probada en Xbox Series por RetroArch. |
| Mesa UWP (`SternXD/mesa-uwp`, rama `uwp-25.3.3`) | RetroArch también la incluye (GL de escritorio sobre D3D12 vía dozen/gallium). | Plan B si ANGLE falla con el renderer de Ren'Py. |

- Ren'Py en Windows ya usa ANGLE (GLES2 sobre D3D11) — su renderer `gl2` soporta GLES2. Así que no hace
  falta GL de escritorio.
- **Direct3D en Xbox Series X|S** (Chuck Walbourn, ["DirectX and UWP on Xbox Series X|S"](https://walbourn.github.io/directx-and-uwp-on-xbox-series-x-s/), 2022):
  **D3D11 FL 11.0 y D3D12 FL 11.0 tanto en modo App como Game**. El backend D3D11 de ANGLE es válido.
  Requiere Windows SDK **22621 o superior** (SHA1 ya no se acepta en Xbox) → usamos 10.0.22621.0
  (instalado en `windows-2022`, también 10.0.26100.0).
- **Decisión (Hito 3):** compilar `SternXD/angle` nosotros en CI (gn + ninja con `target_os="winuwp"`), fijando
  commit. Si la build de ANGLE es demasiado pesada para CI, usar temporalmente los binarios de RetroArch
  (`pkg/msvc/msvc-uwp/ANGLE/x64/`, BSD-3), citando la procedencia en `THIRD_PARTY_NOTICES.md`.
  No confirmado aún: las instrucciones exactas de build del fork (hay que leer su `doc/` en el Hito 3).

## 5. Despliegue desde Linux

### 5.1 openappx (opción principal)

- PyPI `openappx` **0.7.0** (repo `gianlucamazza/openappx`). El README afirma que la cadena
  pack → sign → deploy → launch está **verificada en una Xbox Series S dev kit (2026-08-08)**, y su código
  tiene notas observadas en Xbox OS 26100.8866 (la misma rama de SO que la consola del usuario).
- Probado localmente (`pip install 'openappx[sign]==0.7.0'` en un venv): CLI
  `openappx deploy --device URL --user NAME (--package F | --list | --uninstall PFN | --start PFN | --stop PFN | --install-cert CER) [--also-upload ...] [--app-id ID] [--pin-sha256 HEX | --insecure] [--csrf-bypass]`.
  La contraseña se pasa por la variable `OPENAPPX_DEVICE_PASSWORD` (no aparece en `ps`).
- `--pin-sha256` = SHA-256 del certificado DER del Device Portal:
  `openssl s_client -connect IP:11443 </dev/null 2>/dev/null | openssl x509 -outform DER | sha256sum`.
- CSRF: hace el handshake cookie `CSRF-Token` → cabecera `X-CSRF-Token`.
- `--start` necesita el `PackageFullName` y el `Application/@Id`; según el código, en Xbox el portal exige
  `appid` (AUMID `PackageFamilyName!AppId`) **y** `package`, ambos **en base64** (lo que la doc llama "hex64").
- Nosotros **firmamos en CI con signtool (MSBuild)**; openappx solo despliega. (openappx también sabe empaquetar
  y firmar, útil como respaldo.)

### 5.2 API REST del Device Portal (respaldo con `curl`)

Fuente: [Device Portal core API reference](https://learn.microsoft.com/en-us/windows/uwp/debug-test-perf/device-portal-api-core)
y [Device Portal overview — CSRF](https://learn.microsoft.com/en-us/windows/uwp/debug-test-perf/device-portal).

| Acción | Petición |
|---|---|
| Instalar (con dependencias) | `POST /api/app/packagemanager/package?package=<nombre .msix>`; cuerpo `multipart/form-data`, un campo `file` por archivo (paquete + dependencias) |
| Estado de la instalación | `GET /api/app/packagemanager/state` → 200 resultado / 204 en curso / 404 nada |
| Listar paquetes | `GET /api/app/packagemanager/packages` → `{"InstalledPackages":[{"Name","PackageFullName","PackageFamilyName",...}]}` |
| Desinstalar | `DELETE /api/app/packagemanager/package?package=<PackageFullName>` |
| Lanzar app | `POST /api/taskmanager/app?appid=<b64(AUMID)>&package=<b64(PFN)>` |
| Carpetas conocidas | `GET /api/filesystem/apps/knownfolders` |
| Listar archivos | `GET /api/filesystem/apps/files?knownfolderid=LocalAppData&packagefullname=<PFN>&path=<subdir>` |
| Descargar archivo | `GET /api/filesystem/apps/file?knownfolderid=LocalAppData&packagefullname=<PFN>&path=<subdir>&filename=<archivo>` |

- CSRF: todas las peticiones no-GET por HTTPS necesitan la cabecera `X-CSRF-Token` copiada de la cookie
  `CSRF-Token` (se obtiene con un GET previo). Alternativa documentada: usuario con prefijo `auto-`
  (no usar ese nombre en el navegador). Las descargas (GET) no lo necesitan.
- Con curl se puede fijar la clave pública aunque el certificado sea autofirmado:
  `curl -k --pinnedpubkey 'sha256//<base64>'` (curl verifica el pin incluso con `-k`).
- No confirmado: el formato exacto de `path` para LocalState (`\LocalState` vs `LocalState`). `ci/fetch-logs.sh`
  prueba ambos y, si falla, lista la raíz para diagnosticar.

### 5.3 Firma y certificado

- El `CN` del certificado debe ser igual a `Identity/@Publisher` del manifiesto → usamos **`CN=RenpyUWP`**.
- Certificado autofirmado con `basicConstraints=CA:FALSE`, `keyUsage=digitalSignature`, `extendedKeyUsage=codeSigning`.
  PFX exportado con `PBE-SHA1-3DES` + MAC SHA1 (formato que entiende cualquier signtool/CryptoAPI;
  el AES-256 por defecto de OpenSSL 3 puede fallar en versiones antiguas de Windows). Comandos probados
  localmente con OpenSSL 3.5.3.
- En Xbox Dev Mode la instalación por Device Portal no suele requerir confiar el certificado; si aparece un error
  de confianza (p. ej. `0x800B0109`), `openappx deploy --install-cert` lo instala (`ci/deploy.sh --install-cert`).
  No confirmado en la consola del usuario todavía.

## 6. Prior art: USB, mando y 4K

### 2ship2harkinian-uwp (`XboxEmulationHub/2ship2harkinian-uwp`, rama `uwp`, push 2025-12-14)
- Manifiesto (`vs2022-uwp/uwp/Package.appxmanifest`): capacidades `broadFileSystemAccess`, `expandedResources`,
  `runFullTrust` (rescap), `removableStorage`, `codeGeneration`, `internetClient(Server)`, `privateNetworkClientServer`.
- **USB/almacenamiento:** usa rutas Win32 directas (`std::filesystem`) a `D:/2ship/` o `E:/2ship/`, o LocalState,
  según un ajuste en `LocalSettings` (`vs2022-uwp/uwp/src/main.cpp`). Es decir: con `broadFileSystemAccess`
  en Dev Mode, las APIs Win32 normales llegan a la unidad USB.
- **Mando:** delega en SDL2 (`SDL_WinRTRunApp`).
- **Resolución:** su menú de arranque fija 1920×1080; el render principal crea el swapchain con
  `CreateSwapChainForCoreWindow` (`dx11glue.cpp`). No implementa 4K explícitamente.

### RetroArch UWP (`libretro/RetroArch`, `os/uwp/uwp_main.cpp`, `libretro-common/vfs/vfs_implementation_uwp.cpp`)
- **USB/almacenamiento:** `broadFileSystemAccess` + las APIs `*FromAppW` (`CreateFile2FromAppW`,
  `GetFileAttributesExFromAppW`, `CreateDirectoryFromAppW`, `DeleteFileFromAppW`…, de `fileapifromapp.h`).
  Si falta el permiso abre `ms-settings:privacy-broadfilesystemaccess`.
- **Mando:** driver propio `input/drivers/uwp_input.c` (Windows.Gaming.Input).
- **Resolución 4K:** si `AnalyticsInfo::VersionInfo().DeviceFamily() == L"Windows.Xbox"`, lee
  `HdmiDisplayInformation::GetForCurrentView().GetCurrentDisplayMode().ResolutionWidthInRawPixels()` /
  `ResolutionHeightInRawPixels()` (y `RefreshRate()`), y usa ese tamaño para la superficie EGL de ANGLE.
- Modelo de consola: `GetGamingDeviceModelInformation`.
- Walbourn (2022): **para Series S, usar 1440p (2560×1440) en vez de 4K**; la consola escala.

**Implicación para CPython (Hito 2, riesgo principal):** CPython usa `CreateFileW`, `FindFirstFileW`, etc. Hay que
verificar en la consola si funcionan sobre `E:\` con `broadFileSystemAccess` (2ship sugiere que sí) o si hay
que parchear `Modules/posixmodule.c`/`Python/fileutils.c` para usar las variantes `*FromAppW` como RetroArch.

## 7. App vs Game en Xbox

Fuentes: Walbourn 2017/2022 y la página archivada de Microsoft
[System resources for UWP apps and games on Xbox](https://learn.microsoft.com/en-us/windows/uwp/xbox-apps/system-resource-allocation).
- Memoria en primer plano: **App 1 GB, Game 5 GB** (cifras genéricas Xbox One/Series; no hay cifra específica
  para Series S → no confirmado). GPU: App ~45 % compartido; Game acceso completo.
- Cambiar a Game: Dev Home → seleccionar la app → View/Ver detalles → "App type" = **Game**.
  La capacidad `expandedResources` (que incluimos) también la marca como juego en Dev Mode; está obsoleta para la Store.

## 8. Entorno de CI

- `windows-2022` (runner-images, a 2026-11): Windows Server 2022 (10.0.20348), **Visual Studio Enterprise 2022
  17.14**, workload `Microsoft.VisualStudio.Workload.Universal`, Windows SDK **10.0.22621.0** y **10.0.26100.0**.
- VCLibs para UWP: `C:\Program Files (x86)\Microsoft SDKs\Windows Kits\10\ExtensionSDKs\Microsoft.VCLibs\14.0\Appx\Retail\x64\Microsoft.VCLibs.x64.14.00.appx`
  (MSBuild también la copia a `Dependencies\x64\` al empaquetar en modo sideload).
- C++/WinRT: usamos las cabeceras que trae el Windows SDK (`Include\<ver>\cppwinrt`) para no depender de NuGet en el Hito 0.

## 9. Decisiones del Hito 0

- Host mínimo C++/WinRT (CoreApplication + D3D11 que limpia la pantalla a un color fijo + log en LocalState),
  proyecto `.vcxproj` escrito a mano, MSBuild en CI, firmado con signtool vía MSBuild.
  (SDL2 entra en el Hito 1; este host solo prueba la cadena build → firma → deploy → log.)
- Identidad: `Name="RenpyUWP"`, `Publisher="CN=RenpyUWP"`, `Application Id="App"`.
- La versión del paquete la fija CI: `0.0.<run_number>.0`, para que cada despliegue sea una actualización.
- Ren'Py se descargará en CI por tag (`renpy/VERSION`) en vez de submódulo: evita clones pesados y deja el tag
  fijado en un único sitio.

## 10. Verificado en la consola del usuario (2026-11-08)

Xbox Series S, OS **10.0.26100.9647**, Device Portal en `https://192.168.100.102:11443` (certificado `CN=192.168.100.102`).

- **openappx 0.7.0 instala sin `--install-cert`**: la Xbox en Dev Mode acepta el paquete firmado con nuestro
  certificado autofirmado sin confiarlo antes. Reinstalar la misma versión y actualizar a una más nueva funcionan.
- `GET /api/app/packagemanager/packages` en Xbox: **`Name` es el nombre visible** ("Ren'Py UWP") y
  `PackageFamilyName` vale `RenpyUWP` (sin hash). Hay que identificar el paquete por el prefijo de
  `PackageFullName`. Durante una actualización **aparecen las dos versiones a la vez**: hay que tomar la más alta.
- Lanzar justo después de reinstalar puede fallar con HTTP 400 `0x80270300` "Failed to launch the application"
  (el paquete aún se está registrando). `ci/deploy.sh` espera y reintenta.
- Descarga de archivos: **`path=\LocalState`** funciona. Si la app tiene el archivo abierto sin compartir,
  el portal responde **HTTP 423** "File locked by another process" → el log se abre con `_wfsopen(..., _SH_DENYWR)`.
- Ventana UWP: 1920×1080 DIPs, `RawPixelsPerViewPixel` = 1.0. `CreateSwapChainForCoreWindow` con 0×0 dio un
  swapchain de **8×8**: hay que pasar el tamaño explícito.
- D3D11: nivel de funciones **11.0** (`0xb000`) en modo App, como indicaba Walbourn. El adaptador se llama
  `SraKmd_arden` y reporta 128 MB de memoria de vídeo dedicada.

---

# Hito 1 — SDL2 + mando + pruebas de USB y HDMI (investigación 2026-11-08)

## 11. SDL2 para UWP

- Versión: **SDL2 2.0.20**, tarball oficial `https://github.com/libsdl-org/SDL/releases/download/release-2.0.20/SDL2-2.0.20.tar.gz`,
  SHA-256 `c56aba1d7b5b0e7e999e4a7698c70b63a3394ff9704b5f6e1c57e0c16f04dd06` — **idéntico** al
  `source/SDL2-2.0.20.tar.gz` de renpy-build (mismo hash; `SDL_revision.h` = `b424665e`, tag `release-2.0.20`).
- Parches de Ren'Py (`patches/SDL2-2.0.20/series`): ninguno afecta al backend WinRT. `fix-windows_gaming_input.diff`
  es solo para cabeceras MinGW; `dinput-non-exclusive`, `missing-usb-name-crash` tocan DirectInput/RawInput/HIDAPI
  (escritorio); el resto es Android/iOS/mac/X11. **No se aplica ninguno** en UWP.
- Proyecto: **`VisualC-WinRT/SDL-UWP.vcxproj`** (DLL, C++/CX `/ZW`, usa `include/SDL_config_winrt.h`: joystick
  **WGI + XInput**, audio WASAPI, render D3D11). Se compila con overrides de MSBuild: `PlatformToolset=v143`,
  `WindowsTargetPlatformVersion=10.0.22621.0`, `TargetName=SDL2`. Descartado CMake `WindowsStore` en 2.0.20: genera
  su propio `SDL_config.h` y la detección de WGI/XInput depende de `check_include_file`, menos probado que el vcxproj.
- Arranque: `SDL_WinRTRunApp(main, NULL)` (como 2ship). SDL crea el `IFrameworkView` y ejecuta `main` en el hilo de
  UI de la CoreWindow (por eso las APIs `GetForCurrentView()` funcionan ahí, pero no se puede bloquear con `.get()`
  en operaciones async: hay que sondear `Status()` mientras se bombean eventos).
- Botón B: en Xbox es "atrás" del sistema. `SDL_HINT_WINRT_HANDLE_BACK_BUTTON=1` lo marca como manejado.
- En el Hito 1 SDL solo gestiona ventana, eventos y mando. Dibujamos con nuestro swapchain D3D11 sobre la CoreWindow
  de SDL (`SDL_GetWindowWMInfo` → `info.winrt.window`) para controlar su tamaño (4K). Mismo patrón que hará falta
  con ANGLE (superficie EGL con tamaño explícito).
- Licencia: zlib. Se redistribuye `SDL2.dll` en el paquete.

## 12. USB en Xbox Dev Mode

Fuentes: [sternserv — Xbox Developer Mode Setup](https://wiki.sternserv.xyz/docs/xbox-setup/xbox-developer-mode-setup),
[sternserv — RetroArch guide](https://wiki.sternserv.xyz/docs/emulation/retroarch-guide),
[sternserv — Copying games from Linux to NTFS](https://wiki.sternserv.xyz/docs/xbox-setup/copying-games-from-linux),
[Microsoft — File access permissions](https://learn.microsoft.com/en-us/windows/apps/develop/files/file-access-permissions).

- Comunidad: el USB **debe ser NTFS** con la entrada **ALL APPLICATION PACKAGES = Control total** en el ACL; en la
  consola se configura como **"Media"** (no "Games & apps", que lo formatearía). Aparece como **`E:\`**.
- Desde Linux: archivos copiados con ntfs-3g no heredan ese ACL; sternserv da el descriptor a escribir con
  `setfattr -n system.ntfs_acl`. Decodificado: owner/group Administrators, DACL con 2 ACE de acceso total
  (`0x001f01ff`): **Everyone (S-1-1-0)** y **ALL APPLICATION PACKAGES (S-1-15-2-1)**; flags de herencia `0x03`
  (OI|CI) en carpetas, `0x00` en archivos.
- exFAT/FAT32: ninguna fuente lo menciona para Dev Mode. exFAT no tiene ACL; **no confirmado** si la AppContainer
  puede acceder. Se prueba más adelante si hace falta (el plan original del proyecto decía exFAT: queda pendiente).
- Microsoft dice que `broadFileSystemAccess` "no está soportado en Xbox" (doc genérica, para la Store). En Dev Mode
  2ship y RetroArch lo declaran y acceden a `E:\` con Win32 (`std::filesystem` / `*FromAppW`). `removableStorage`
  da acceso vía `KnownFolders.RemovableDevices` solo para tipos de archivo declarados en `FileTypeAssociation`.
- Prueba del Hito 1 (registrada en el log): letras con `GetLogicalDrives`/`GetDriveTypeW`, volumen
  (`GetVolumeInformationW`), listado de la raíz con `FindFirstFileExFromAppW` **y** con `FindFirstFileExW` normal,
  lectura de `renpy-uwp-test.txt` con `CreateFile2FromAppW`, `CreateFile2`/`CreateFileW` y `_wfopen` (CRT), y
  `KnownFolders::RemovableDevices`. Así sabremos si CPython sin parches (usa `CreateFileW`/`FindFirstFileW`) llega al USB.
  Declaramos `FileTypeAssociation` para `.txt`, `.rpa`, `.rpy`, `.rpyc` para que `RemovableDevices` también vea esos archivos.

## 13. Salida HDMI: 4K y 120 Hz

API: [`Windows.Graphics.Display.Core.HdmiDisplayInformation`](https://learn.microsoft.com/en-us/uwp/api/windows.graphics.display.core.hdmidisplayinformation)
(desde 10.0.15063, UniversalApiContract v4): `GetForCurrentView()`, `GetSupportedDisplayModes()` →
`IVectorView<HdmiDisplayMode>`, `GetCurrentDisplayMode()`, `RequestSetCurrentDisplayModeAsync(mode[, HdmiDisplayHdrOption[, metadata]])`
→ `IAsyncOperation<bool>`, `SetDefaultDisplayModeAsync()`, evento `DisplayModesChanged`.
`HdmiDisplayMode`: `ResolutionWidthInRawPixels`, `ResolutionHeightInRawPixels`, `RefreshRate` (double, Hz),
`BitsPerPixel`, `ColorSpace` (`RgbLimited/RgbFull/BT2020/BT709`), `PixelEncoding` (`Rgb444/Ycc444/Ycc422/Ycc420`),
`StereoEnabled`, `IsSmpte2084Supported`, `Is2086MetadataSupported`, `IsSdrLuminanceSupported`,
`IsDolbyVisionLowLatencySupported`, `IsEqual()`. `HdmiDisplayHdrOption`: `None/EotfSdr/Eotf2084/DolbyVisionLowLatency`.
La doc no dice si hace falta una capacidad, ni si el modo se revierte al salir (se llama a `SetDefaultDisplayModeAsync`
al suspender por si acaso).

- Series S y 4K a 120 Hz: solo hay fuentes secundarias ([Windows Report](https://windowsreport.com/xbox-series-s-4k/),
  [How-To Geek](https://www.howtogeek.com/700984/how-to-enable-120hz-on-xbox-series-x-and-s/)) que dicen que la Series S
  puede **sacar** 4K a 120 Hz (escalado; los juegos renderizan a 1440p), con cable/puerto HDMI 2.1 y "Allow 120 Hz"
  activado en *TV & display options → Video modes*. La página oficial de soporte de Xbox no se pudo leer (se renderiza en
  cliente). **No confirmado** → se mide en la consola.
- Walbourn: en Series S el render recomendado es 1440p; la consola escala el swapchain a la salida.
- Plan de prueba en el host: registrar todos los modos; pedir en orden 3840×2160@~120, 2560×1440@~120,
  3840×2160@~60, 1920×1080@~120; registrar el resultado de cada petición y el modo final; crear el swapchain al tamaño
  del modo real (no al de la CoreWindow, que sigue en 1920×1080 DIPs) y medir fps (QPC) durante 5 s con `Present(1,0)`.
  Si un modo no aparece en la lista, el límite está en la negociación consola/TV/cable/ajustes; si aparece pero la
  petición devuelve `false`, el límite es de la API/política (p. ej. App vs Game). `MemoryManager::AppMemoryUsageLimit`
  se registra para saber si la app corre como App (~1 GB) o Game (~5 GB).
- TV Samsung S90D: el puerto HDMI usado debe tener **Input Signal Plus** activado para aceptar 4K a 120 Hz (ajuste de
  Samsung por puerto). Pendiente de confirmar por el usuario.

## 14. Resultados en la consola (Hito 1, 2026-11-08, builds 0.0.8–0.0.10)

- **Tipo Game**: `MemoryManager::AppMemoryUsageLimit` = **5120 MB** sin tocar Dev Home → `expandedResources` basta en Dev Mode.
- **Mando**: SDL 2.0.20 (WGI) detecta "Xbox One Game Controller" al arrancar.
- **Unidades** vistas por la app (sin USB): `C: D: G: J: K: L: M: N: O: Q: S: T: U: V: X: Y:`. Solo **`D:\`** es accesible
  (NTFS, 5 GB, carpetas `DevelopmentFiles`, `Drives`, `Temp`, `Titles`); el resto da error 5 (acceso denegado) o 3.
  En `D:\` funcionan **igual** `FindFirstFileExW` normal y `FindFirstFileExFromAppW`. `KnownFolders.RemovableDevices`: 0 carpetas.
- **`CreateFileW` no está declarado con `WINAPI_FAMILY_APP`** (error C3861): solo `CreateFile2`. Relevante para CPython (Hito 2).
- **HDMI** (TV Samsung S90D): la consola ofrece 16 modos, incluidos **3840×2160 @ 119.880 Hz** (RGB 8/10 bit e YCC420 8/10/12 bit),
  3840×2160 @ 59.94/50/23.976, 1920×1080 @ 119.88/59.94/50/23.976, 1280×720 @ 50. **No hay 2560×1440.** HDR no se anuncia en
  ningún modo (`hdr10=0`) → probablemente HDR desactivado en los ajustes de la consola (no confirmado).
- `RequestSetCurrentDisplayModeAsync(mode, HdmiDisplayHdrOption::EotfSdr)` → **E_INVALIDARG (0x80070057)** para cualquier modo.
  Con **`HdmiDisplayHdrOption::None`** (como hace Kodi, `xbmc/windowing/win10/WinSystemWin10.cpp`) → **`true`**, y
  `GetCurrentDisplayMode()` confirma **3840×2160 @ 119.880 Hz RGB 8 bit**. La CoreWindow sigue en 1920×1080 DIPs; el
  swapchain se crea a 3840×2160 sin error. No hace falta ninguna capacidad en el manifiesto.
- **Problema abierto — fps**: a 1080p60 el host da ~40 fps (`Present` medio 24.5 ms, con pausas de ~500 ms al inicio).
  Tras pasar a 4K120, **cada `Present` tarda ~2000 ms** (S_OK, no OCCLUDED) de forma persistente, aun recreando el swapchain
  (FLIP_DISCARD, latencia 1). En la build 0.0.8 pasó lo mismo **sin** cambio de modo (ventana inicialmente invisible).
  2 s parece un timeout de DXGI cuando el compositor no consume frames (sin vsync, salida no mostrada o app sin primer plano).
  Pendiente: observación del usuario en la TV y prueba lanzando desde Dev Home (no por Device Portal).
- Despliegue: actualizar con la versión anterior en ejecución deja la instalación "en curso" (HTTP 409 "Another deployment
  is running") hasta que la app se cierra → `ci/deploy.sh` ahora detiene la app antes de instalar.

## 15. USB y HDMI: conclusiones con el USB real (builds 0.0.11–0.0.14, 2026-11-08)

### USB (resuelto)
- El USB (NTFS, etiqueta `RENPY`, ACL de sternserv) aparece como **`E:\` tipo removable**, 29 GB.
- **Todo funciona igual con APIs Win32 normales y con `*FromAppW`**: listado (`FindFirstFileExW`), atributos, lectura con
  `CreateFile2`, con `CreateFile2FromAppW` y con el CRT (`_wfopen_s`). UTF-8 intacto (`áéíóú ñ`). `KnownFolders.RemovableDevices`
  también ve `E:\` y lee el `.txt` (solo los tipos declarados en `FileTypeAssociation`).
  → **Para CPython no hace falta parchear las rutas de archivos con `*FromApp`**; basta con que use `CreateFile2` (no existe
  `CreateFileW` en la partición de apps) y las APIs de búsqueda normales.
- KS:RE en `E:\renpy\games\ksre\game\`: 26 archivos, 4 carpetas, **1392 MB**, 6 `.rpa` (1.36 GB), 8 `.py`, 2 `.png`.
- Velocidad: el primer test (con caché) dio 3995 MB/s, que no es real. Sin caché (`FILE_FLAG_NO_BUFFERING`, 64 MB desde el
  offset 256 MB de `images.rpa`): **25.0 MB/s**. Es lento: suficiente para leer recursos bajo demanda, pero cargar 1.4 GB
  completo tardaría ~1 min. Probar otro puerto (USB 3) o un pendrive más rápido antes del Hito 5.

### HDMI / fps (limitaciones medidas)
Matriz modo de salida × tamaño de swapchain, `Present(1)` salvo indicación, 4 s por prueba tras 2 s de estabilización:

| Salida HDMI | Swapchain | Resultado |
|---|---|---|
| 3840×2160 @ 59.94 | 3840×2160 | **GPU reiniciada** (`DXGI_ERROR_DEVICE_RESET` 0x887A0007) — 2 de 2 veces |
| 3840×2160 @ 119.88 | 3840×2160 | **GPU reiniciada** — 2 de 2 |
| 3840×2160 @ 59.94 | 3200×1800 / 2880×1620 | **GPU reiniciada** |
| 3840×2160 @ 59.94 | 2560×1440 | 60.0 fps (build 0.0.13); GPU reiniciada en 0.0.14 (primer cambio desde 1080p60) |
| 3840×2160 @ 119.88 | 2560×1440 | **59.8–60.0 fps** (estable, 3 de 3) |
| 3840×2160 @ 119.88 | 1920×1080 | 60.0 fps |
| 3840×2160 @ 119.88 | 2560×1440, `Present(0)` | GPU reiniciada |
| 1920×1080 @ 119.88 | 1920×1080 | 60.0 fps; con `Present(0)` 59.8 fps |
| 1920×1080 @ 59.94 (por defecto) | 1920×1080 | ~45 fps en el arranque (con la prueba de USB en paralelo y pausas iniciales de ~500 ms) |

Conclusiones:
1. **La salida 3840×2160 a 119.88 Hz sí se consigue** desde UWP (`HdmiDisplayInformation` + `HdrOption::None`). No limitan
   la consola, el cable ni la TV: el modo aparece en la lista y la TV lo recibe.
2. **La app no pasa de 60 fps en ningún modo de 120 Hz**, ni a 1080p ni sin vsync (`Present(0)` también da 59.8). Así que el
   límite no es el vsync del swapchain sino **la composición de la CoreWindow UWP, que va a 60 Hz** (límite de plataforma/API para
   apps UWP en Dev Mode). No hay ajuste conocido que lo cambie; no confirmado en documentación (pendiente buscar si GDK/`expandedResources`
   o un `CoreWindow`/`SwapChainPanel` distinto lo permiten).
3. **Swapchains mayores de 2560×1440 reinician la GPU** (DEVICE_RESET) en Series S, a 60 y a 120 Hz. Coincide con la recomendación
   de Walbourn (1440p en Series S). Causa exacta no confirmada: candidatas, el presupuesto de memoria de vídeo (el adaptador anuncia
   **128 MB dedicados**; 2 búferes 4K BGRA = 66 MB) o un límite del plano de presentación. Siguiente diagnóstico:
   `IDXGIAdapter3::QueryVideoMemoryInfo` y liberar el swapchain **antes** del cambio de modo.
4. **Mejor modo alcanzable hoy: salida 3840×2160 @ 119.88 Hz con swapchain 2560×1440 a 60 fps** (la consola escala a 4K).
   Alternativa equivalente en fps: salida 4K60 con 1440p (aunque una vez reinició la GPU al cambiar desde 1080p).
5. La pantalla negra que vio el usuario era la GPU reiniciada por el swapchain 4K: el host 0.0.11 no se recuperaba de
   la pérdida del dispositivo y se quedaba bloqueado en `Present` (por eso View no respondía). Desde la 0.0.13 el host
   detecta `GetDeviceRemovedReason` y crea un dispositivo nuevo.
