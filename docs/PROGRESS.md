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

**Falló / pendiente de verificar**
- Aún no se ha ejecutado el workflow (falta que el usuario cargue los secrets).
- No confirmado: formato del parámetro `path` del Device Portal para LocalState; si la Xbox exige confiar
  el `.cer` (`--install-cert`).

**Siguiente paso**
- Usuario: generar certificado y cargar `UWP_CERT_PFX_BASE64` / `UWP_CERT_PASSWORD`.
- Lanzar `gh workflow run build-host.yml`, iterar hasta verde, desplegar con `ci/deploy.sh` y bajar el log.
- Pendiente de decidir por el usuario: licencia del código propio del runtime (no hay `LICENSE` todavía).
