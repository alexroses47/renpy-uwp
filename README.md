# renpy-uwp

Native port of the **Ren'Py 8** engine to **UWP** for **Xbox Series X|S in Developer Mode**
(embedded CPython + SDL2 + ANGLE + Ren'Py C modules). The runtime loads any Ren'Py 8 game from an external
folder (USB drive or LocalState). First target: *Katawa Shoujo: Re-Engineered*.

**No game files are included or accepted in this repository.** You provide your own copy of the game.

## Layout

| Path | Contents |
|---|---|
| `host/` | UWP app (C++/WinRT host, manifest, MSBuild project) |
| `deps/` | Recipes for native dependencies built for UWP |
| `python/` | Embedded CPython build for UWP |
| `renpy/` | Pinned Ren'Py version (`renpy/VERSION`), fetched in CI |
| `ci/` | Deploy / log scripts for Linux, CI helpers |
| `docs/` | `RESEARCH.md`, `PROGRESS.md`, `SIGNING.md` |
| `.github/workflows/` | Windows builds (GitHub Actions, `windows-2022`) |

## Quick start (from Linux)

```bash
gh workflow run build-host.yml && gh run watch
export XBOX_DP_USER=... XBOX_DP_PASS=...
ci/deploy.sh          # downloads the latest artifact and installs it on the console
ci/fetch-logs.sh      # downloads LocalState\renpy-uwp.log
```

See `docs/SIGNING.md` for the one-time signing certificate setup and `THIRD_PARTY_NOTICES.md` for licenses.

## License

The original code of this runtime (host, CI scripts, build recipes written for this project) is licensed under the
**Mozilla Public License 2.0** — see [`LICENSE`](LICENSE). Each such source file carries the MPL-2.0 Exhibit A header.

**Third-party code keeps its original license**, and so do our patches to third-party files: a patch to Ren'Py is
MIT, a patch to CPython is PSF-2.0, a patch to SDL2 is zlib, a patch to FFmpeg is LGPL-2.1-or-later, and so on.
MPL-2.0 does not apply to them. See [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

Games are not part of this project and keep their own licenses (e.g. *Katawa Shoujo: Re-Engineered*: CC BY-NC-ND 4.0
for assets, MPL-2.0 for code); no game files are distributed here.
