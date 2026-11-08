# python/cpython-uwp

CPython 3.12.11 for UWP x64 (Xbox Developer Mode), built as a **static library with the extension
modules built in**, plus the stdlib as precompiled `.pyc` files in `python312.zip`.

| File | Origin / license |
|---|---|
| `CMakeLists.txt` | **Derived from** `CMakeLists.txt` of [thexai/cpython](https://github.com/thexai/cpython) at `4c97e13` ("Kodi patches", CPython 3.12.11). PSF License Version 2, like the rest of that CPython fork. Generated once from the original by a transformation script and then edited; the header lists the changes. |
| `patches/*.patch` | Our patch series applied on top of `4c97e13`. They modify CPython files and are distributed under the **PSF License Version 2**. Several follow upstream PRs of python/cpython gh-152433 (#153021, #152797). |
| `build.ps1`, `version.ps1`, `make_stdlib_zip.py` | renpy-uwp, MPL-2.0. |

Nothing is taken from `xbmc/kodi-deps` (no license). See `docs/RESEARCH.md` sections 17–21.

Build (CI does this; see `.github/workflows/build-host.yml`):

```powershell
./python/cpython-uwp/build.ps1 -Prefix out/deps/cpython -HostPython (Get-Command python3.12).Source
```

Output: `lib/python312.lib`, `lib/zlibstatic.lib`, `include/`, `python312.zip`, `licenses/`.
Not built in milestone 2: `_ssl _hashlib _bz2 _lzma _sqlite3 _ctypes _uuid` (external libraries / W^X).
