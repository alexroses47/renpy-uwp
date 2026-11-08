# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

"""Build pythonXY.zip: the stdlib as precompiled .pyc files (no sources), for zipimport.

Usage: python3.12 -I make_stdlib_zip.py <Lib dir> <output zip>
Must run with the same minor version as the target interpreter (3.12): .pyc magic numbers differ.
"""
import importlib.util
import py_compile
import sys
import tempfile
import zipfile
from pathlib import Path

# Not needed by Ren'Py or not usable in a UWP sandbox.
EXCLUDE_DIRS = {
    "test", "tests", "idlelib", "tkinter", "turtledemo", "lib2to3", "ensurepip", "venv",
    "__pycache__", "site-packages", "msilib", "curses", "sqlite3",
}
EXCLUDE_FILES = {"turtle.py", "antigravity.py"}


def main() -> int:
    lib, out = Path(sys.argv[1]), Path(sys.argv[2])
    if sys.version_info[:2] != (3, 12):
        print("error: must run with Python 3.12", file=sys.stderr)
        return 2
    count = 0
    with tempfile.TemporaryDirectory() as tmp, zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as zf:
        tmpdir = Path(tmp)
        for path in sorted(lib.rglob("*.py")):
            rel = path.relative_to(lib)
            if any(part in EXCLUDE_DIRS for part in rel.parts[:-1]) or rel.name in EXCLUDE_FILES:
                continue
            cfile = tmpdir / "x.pyc"
            py_compile.compile(str(path), cfile=str(cfile), dfile=str(rel).replace("\\", "/"),
                               doraise=True, optimize=0,
                               invalidation_mode=py_compile.PycInvalidationMode.UNCHECKED_HASH)
            arc = rel.with_suffix(".pyc").as_posix()
            zf.write(cfile, arc)
            count += 1
    print(f"{out}: {count} modules, magic {importlib.util.MAGIC_NUMBER.hex()}, {out.stat().st_size // 1024} KB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
