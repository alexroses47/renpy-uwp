# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

"""Writes renpy/vc_version.py for a release tag, as renpy.versions.generate_vc_version() would.

Ren'Py derives it from git history, which a shallow clone does not have. Without the file Ren'Py
reports the "main" branch version (8.6.0.0+unofficial) instead of the tag.
Usage: write_vc_version.py RENPY_ROOT TAG        (TAG like 8.5.3.26051504)
"""

import os
import re
import sys

root, tag = sys.argv[1], sys.argv[2]
base, _, suffix = tag.rpartition(".")
src = open(os.path.join(root, "renpy", "versions.py"), encoding="utf-8").read()
m = None
for m in re.finditer(r'Version\("(\w+)", 3, "([\d.]+)", "([^"]+)"\)', src):
    if m.group(2) == base:
        break
else:
    sys.exit(f"no Version(...) entry for {base} in renpy/versions.py")
values = {"branch": m.group(1), "nightly": False, "official": False, "version": tag, "version_name": m.group(3)}
with open(os.path.join(root, "renpy", "vc_version.py"), "w", encoding="utf-8") as f:
    for k, v in sorted(values.items()):
        f.write(f"{k} = {v!r}\n")
print("vc_version:", values)
