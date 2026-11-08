# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

"""Milestone 2 smoke test for the embedded interpreter (run by the host as smoke.main()).

Writes LocalState\\python.log (stdout/stderr) and LocalState\\faulthandler.log.
The module lists come from renpy-build tag renpy-8.5.3.26051504 (tasks/pythonlib.py PY3_MODULES,
source/Python-3.12.8-Setup.stdlib); the authoritative list will be the dump from a real KS:RE run.
"""

import sys

# Ren'Py's stdlib Python modules (PY3_MODULES without third-party packages), plus a few submodules.
RENPY_STDLIB = """
__future__ _collections_abc _compat_pickle _compression _markupbase _osx_support _py_abc _pydecimal
_pyio _sitebuiltins _strptime _threading_local _weakrefset abc argparse ast asyncio base64 bisect bz2
cProfile calendar cgi chunk cmd code codecs codeop collections colorsys compileall concurrent
configparser contextlib contextvars copy copyreg csv ctypes dataclasses datetime decimal difflib dis
doctest email encodings enum filecmp fileinput fnmatch fractions ftplib functools genericpath getopt
getpass gettext glob graphlib gzip hashlib heapq hmac html http imaplib imghdr importlib inspect io
ipaddress json keyword linecache locale logging lzma mimetypes modulefinder multiprocessing netrc ntpath
nturl2path numbers opcode operator optparse os pathlib pdb pickle pickletools pipes pkgutil platform
plistlib poplib posixpath pprint profile pstats pty py_compile pyclbr pydoc queue quopri random re
reprlib rlcompleter runpy sched secrets selectors shelve shlex shutil signal site socket socketserver
sre_compile sre_constants sre_parse ssl stat statistics string stringprep struct subprocess sunau
symtable tabnanny tarfile tempfile textwrap this threading timeit token tokenize trace traceback
tracemalloc tty types typing urllib uu uuid warnings wave weakref webbrowser xml zipapp zipfile zipimport
zoneinfo
urllib.request urllib.parse http.client email.utils json.decoder xml.etree.ElementTree xml.dom.minidom
concurrent.futures logging.handlers multiprocessing.pool importlib.resources encodings.idna
""".split()

# C modules Ren'Py builds in (Setup.stdlib + Windows-only ones).
RENPY_C_MODULES = """
_asyncio _bisect _bz2 _contextvars _csv _datetime _elementtree _heapq _json _lsprof _opcode _pickle
_queue _random _statistics _struct _typing _zoneinfo array binascii cmath math mmap select unicodedata
zlib _lzma pyexpat _md5 _sha1 _sha2 _sha3 _blake2 _codecs_cn _codecs_hk _codecs_iso2022 _codecs_jp
_codecs_kr _codecs_tw _multibytecodec _ctypes _socket _ssl _hashlib _winapi _overlapped msvcrt winreg
""".split()

# Not built in milestone 2 (need external libraries), or not available on Windows at all.
EXPECTED_MISSING = {
    "_bz2": "excluded in milestone 2 (needs bzip2)",
    "bz2": "excluded in milestone 2 (needs _bz2)",
    "_lzma": "excluded in milestone 2 (needs xz)",
    "lzma": "excluded in milestone 2 (needs _lzma)",
    "_ctypes": "excluded in milestone 2 (libffi, W^X)",
    "ctypes": "excluded in milestone 2 (needs _ctypes)",
    "_ssl": "excluded in milestone 2 (needs OpenSSL)",
    "ssl": "excluded in milestone 2 (needs _ssl)",
    "_hashlib": "excluded in milestone 2 (needs OpenSSL; hashlib falls back to _md5/_sha*/_blake2)",
    "winreg": "not registered on UWP by the fork (APP partition)",
    "pty": "POSIX only (also fails on desktop Windows)",
    "tty": "POSIX only (also fails on desktop Windows)",
}


def section(title):
    print()
    print("=" * 8, title, "=" * 8)


def check(name, fn):
    try:
        result = fn()
        print(f"OK    {name}: {result}")
        return True
    except BaseException as e:  # noqa: BLE001 - we want to log everything
        print(f"FAIL  {name}: {type(e).__name__}: {e}")
        return False


def import_all(names):
    import importlib
    ok, failed = [], []
    for name in names:
        try:
            importlib.import_module(name)
            ok.append(name)
        except BaseException as e:  # noqa: BLE001
            failed.append((name, f"{type(e).__name__}: {e}"))
    return ok, failed


def report_imports(title, names):
    section(title)
    ok, failed = import_all(names)
    unexpected = [(n, err) for n, err in failed if n not in EXPECTED_MISSING]
    expected = [(n, err) for n, err in failed if n in EXPECTED_MISSING]
    print(f"{len(ok)}/{len(names)} imported")
    for n, err in expected:
        print(f"EXPECTED-FAIL {n}: {err}  [{EXPECTED_MISSING[n]}]")
    for n, err in unexpected:
        print(f"FAIL {n}: {err}")
    return len(unexpected)


def main():
    import os
    localstate = sys._renpy_uwp_localstate
    log = open(os.path.join(localstate, "python.log"), "w", encoding="utf-8", buffering=1)
    sys.stdout = sys.stderr = log
    import faulthandler
    fh = open(os.path.join(localstate, "faulthandler.log"), "w", encoding="utf-8")
    faulthandler.enable(fh)

    failures = 0
    section("interpreter")
    print("sys.version:", sys.version)
    print("sys.platform:", sys.platform, "| implementation:", sys.implementation.name, sys.implementation.cache_tag)
    print("sys.executable:", repr(sys.executable), "| prefix:", repr(sys.prefix))
    print("sys.path:", sys.path)
    print("utf8_mode:", sys.flags.utf8_mode, "| isolated:", sys.flags.isolated,
          "| filesystem encoding:", sys.getfilesystemencoding(), "| recursion limit:", sys.getrecursionlimit())
    print("builtin_module_names:", " ".join(sorted(sys.builtin_module_names)))
    print("cwd:", os.getcwd())
    print("install dir:", sys._renpy_uwp_install, "| LocalState:", localstate)

    failures += report_imports("Ren'Py C modules", RENPY_C_MODULES)
    failures += report_imports("Ren'Py stdlib modules", RENPY_STDLIB)

    section("functional checks")
    import json, zlib
    data = ("Ren'Py UWP áéíóú ñ " * 100).encode("utf-8")
    failures += not check("zlib round trip", lambda: (len(zlib.compress(data)), zlib.decompress(zlib.compress(data)) == data))
    failures += not check("json round trip", lambda: json.loads(json.dumps({"a": [1, 2, "ñ"]})) == {"a": [1, 2, "ñ"]})
    import hashlib
    failures += not check("hashlib sha256", lambda: hashlib.sha256(b"renpy").hexdigest()[:16])
    import pickle
    failures += not check("pickle deep", lambda: len(pickle.loads(pickle.dumps(nested(400)))))
    # Ren'Py keeps the default recursion limit (1000; it never calls sys.setrecursionlimit).
    failures += not check("recursion 900 (default limit)", lambda: recurse(900))
    failures += not check("recursion 15000 (limit 20000, 8 MB thread)", lambda: deep_recursion(15000))
    failures += not check("C recursion: json/pickle depth 1400",
                          lambda: (len(json.dumps(nested(1400))), len(pickle.dumps(nested(1400)))))
    import unicodedata
    failures += not check("unicodedata", lambda: unicodedata.name("ñ"))

    section("files")
    failures += not check("listdir LocalState", lambda: os.listdir(localstate))
    test = os.path.join(localstate, "python-write-test.txt")

    def write_read():
        with open(test, "w", encoding="utf-8") as f:
            f.write("escrito desde Python áéíóú ñ\n")
        with open(test, encoding="utf-8") as f:
            text = f.read()
        size = os.path.getsize(test)
        os.remove(test)
        return text.strip(), size, os.path.exists(test)

    failures += not check("write/read/remove in LocalState", write_read)
    failures += not check("listdir install dir (relative '.')", lambda: sorted(os.listdir("."))[:20])
    failures += not check("open install file by relative path", lambda: os.path.getsize("python\\smoke.py"))
    failures += not check("open install file by absolute path",
                          lambda: open(os.path.join(sys._renpy_uwp_install, "python", "smoke.py"), "rb").read(16))
    failures += not check("listdir E:\\", lambda: os.listdir("E:\\"))
    game = "E:\\renpy\\games\\ksre\\game"
    failures += not check("listdir " + game, lambda: os.listdir(game))

    def read_game_file():
        files = [os.path.join(game, n) for n in os.listdir(game) if os.path.isfile(os.path.join(game, n))]
        smallest = min(files, key=os.path.getsize)
        with open(smallest, "rb") as f:
            content = f.read()
        largest = max(files, key=os.path.getsize)
        with open(largest, "rb") as f:
            head = f.read(64)
        return (f"{os.path.basename(smallest)}: stat size {os.path.getsize(smallest)} == read {len(content)}; "
                f"{os.path.basename(largest)}: {os.path.getsize(largest)} bytes, first 16 = {head[:16]!r}")

    failures += not check("read files from " + game, read_game_file)
    failures += not check("os.stat on E:\\renpy-uwp-test.txt", lambda: os.stat("E:\\renpy-uwp-test.txt").st_size)

    section("threads and time")
    import threading

    def run_threads():
        results = []
        lock = threading.Lock()

        def work(i):
            s = sum(range(100000 * (i + 1)))
            with lock:
                results.append(s)

        threads = [threading.Thread(target=work, args=(i,)) for i in range(8)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()
        return len(results)

    failures += not check("8 threads start/join", run_threads)
    import time, datetime
    failures += not check("time.tzname / timezone", lambda: (time.tzname, time.timezone, time.daylight))
    failures += not check("datetime.now()", lambda: datetime.datetime.now().isoformat())
    failures += not check("datetime.now().astimezone()", lambda: datetime.datetime.now().astimezone().isoformat())
    failures += not check("time.monotonic / perf_counter", lambda: (time.get_clock_info("monotonic"), time.perf_counter() > 0))
    failures += not check("os.cpu_count", lambda: os.cpu_count())
    import subprocess
    failures += not check("subprocess raises OSError (expected)", lambda: expect_oserror(lambda: subprocess.run(["cmd"])))
    import platform
    failures += not check("platform", lambda: (platform.system(), platform.release(), platform.machine()))
    import locale
    failures += not check("locale", lambda: (locale.getpreferredencoding(False), locale.getlocale()))

    section("summary")
    print(f"SMOKE DONE: {failures} unexpected failure(s)")
    log.flush()
    sys.stdout, sys.stderr = sys.__stdout__, sys.__stderr__
    faulthandler.disable()
    fh.close()
    log.close()
    return failures


def nested(depth):
    v = []
    for _ in range(depth):
        v = [v]
    return v


def recurse(n):
    return 0 if n == 0 else 1 + recurse(n - 1)


def deep_recursion(n):
    old = sys.getrecursionlimit()
    sys.setrecursionlimit(20000)
    try:
        return recurse(n)
    finally:
        sys.setrecursionlimit(old)


def expect_oserror(fn):
    try:
        fn()
    except OSError as e:
        return f"OSError errno={e.errno}: {e}"
    raise AssertionError("did not raise OSError")
