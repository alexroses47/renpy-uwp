# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

"""Builds site-packages.zip: the pure-Python packages Ren'Py ships next to the stdlib.

Same set and versions as renpy-build renpy-8.5.3.26051504 (tasks/python3.py, pip task), minus
setuptools (build tool) and chardet (LGPL; requests uses charset-normalizer). certifi is unpinned
there; it is pinned here to the version in renpy-build's requirements.txt. Only pure wheels
(py3-none-any) are accepted, and pefile (sdist only) is pure Python. Ren'Py imports ecdsa/rsa/six at
startup (renpy/savetoken.py); the rest is used by updates, web requests and projects.

Usage: python make_site_zip.py OUT_ZIP LICENSES_DIR    (run with the same Python 3.12 as the target)
"""

import os
import shutil
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

WHEELS = [
    "future==1.0.0", "six==1.16.0", "rsa==4.9", "pyasn1==0.6.1", "ecdsa==0.19.0", "urllib3==2.7.0",
    "charset-normalizer==3.3.2", "certifi==2024.8.30", "idna==3.8", "requests==2.33.0",
    "websockets==12.0", "pysocks==1.7.1",
]
SDISTS = ["pefile==2022.5.30"]


def pip(*args):
    subprocess.run([sys.executable, "-m", "pip", "install", "--quiet", "--no-deps", "--no-compile",
                    "--disable-pip-version-check", *args], check=True)


def main():
    out, licenses = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
    licenses.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        site = Path(tmp) / "site"
        pip("--target", str(site), "--only-binary=:all:", "--platform", "any", "--python-version",
            f"{sys.version_info.major}.{sys.version_info.minor}", "--implementation", "py", *WHEELS)
        pip("--target", str(site), "--no-binary=:all:", *SDISTS)
        shutil.rmtree(site / "bin", ignore_errors=True)  # console scripts (futurize, pasteurize, ...)

        native = [p for p in site.rglob("*") if p.suffix in (".pyd", ".so", ".dll")]
        if native:
            sys.exit(f"native code in site-packages (must be pure Python): {native}")

        for info in sorted(site.glob("*.dist-info")):
            for f in info.rglob("*"):
                if f.is_file() and (f.name.upper().startswith(("LICEN", "COPYING", "NOTICE"))):
                    shutil.copy(f, licenses / f"{info.name.split('-')[0]}-{f.name}")

        count = 0
        with zipfile.PyZipFile(out, "w", compression=zipfile.ZIP_DEFLATED, optimize=0) as z:
            for item in sorted(site.iterdir()):
                if item.name.endswith(".dist-info") or item.name == "__pycache__":
                    continue
                if item.is_dir() and (item / "__init__.py").exists():
                    z.writepy(str(item))  # package -> .pyc (recursive)
                    # data files the packages read at run time (e.g. certifi/cacert.pem)
                    for f in item.rglob("*"):
                        if f.is_file() and f.suffix not in (".py", ".pyc") and "__pycache__" not in f.parts:
                            z.write(f, f.relative_to(site).as_posix())
                elif item.suffix == ".py":
                    z.writepy(str(item))
                elif item.is_dir():
                    # namespace-style dir without __init__ (e.g. future's 'past' has one; keep any others raw)
                    for f in item.rglob("*"):
                        if f.is_file() and "__pycache__" not in f.parts:
                            z.write(f, f.relative_to(site).as_posix())
                else:
                    continue
                count += 1
        names = zipfile.ZipFile(out).namelist()
        tops = sorted({n.split("/")[0] for n in names})
        print(f"{out}: {len(names)} files, {count} top-level items: {tops}")


if __name__ == "__main__":
    main()
