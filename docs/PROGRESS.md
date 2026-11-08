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
