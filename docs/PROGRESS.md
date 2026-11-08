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

**Pendiente**
- Confirmación visual del usuario en la TV (pantalla verde azulado) para cerrar el Hito 0.

**Siguiente paso**
- Tras la confirmación del usuario: Hito 1 (SDL2 2.0.20 WinRT + mando). No empezar sin su visto bueno.
