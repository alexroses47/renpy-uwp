#!/usr/bin/env bash
# Downloads LocalState\renpy-uwp.log of the installed RenpyUWP package from the Xbox
# through the Device Portal REST API (curl, pinned certificate).
#
# Usage: ci/fetch-logs.sh [--file NAME] [--list] [--quiet]
#   --file NAME  file inside LocalState to fetch (default renpy-uwp.log)
#   --list       list the files in LocalState instead of downloading
#   --quiet      do not print the log to stdout
# Output: logs/<name>-<timestamp>.log and logs/latest.log (logs/ is git-ignored).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=lib/devportal.sh
source "$ROOT/ci/lib/devportal.sh"

file="renpy-uwp.log" list=0 quiet=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --file) file="$2"; shift 2 ;;
        --list) list=1; shift ;;
        --quiet) quiet=1; shift ;;
        -h|--help) sed -n '2,10p' "$0"; exit 0 ;;
        *) dp_die "unknown option: $1" ;;
    esac
done

dp_require_creds
dp_load_pins

pfn="$(dp_package_full_name)"
[[ -n "$pfn" ]] || dp_die "package $DP_PACKAGE_NAME is not installed on the console"
dp_info "Package: $pfn"

# The documented 'path' parameter format is not explicit; try the known variants.
path_variants=('\LocalState' 'LocalState' '/LocalState')

list_dir() {
    dp_curl --fail -G "$DP_URL/api/filesystem/apps/files" \
        --data-urlencode "knownfolderid=LocalAppData" \
        --data-urlencode "packagefullname=$pfn" \
        --data-urlencode "path=$1"
}

if [[ $list -eq 1 ]]; then
    for p in "${path_variants[@]}"; do
        if out="$(list_dir "$p" 2>/dev/null)"; then
            python3 -I -c '
import json, sys
for it in json.load(sys.stdin).get("Items", []):
    print("%12s  %s" % (it.get("FileSize", 0), it.get("Name")))' <<<"$out"
            exit 0
        fi
    done
    dp_die "could not list LocalState"
fi

mkdir -p "$ROOT/logs"
stamp="$(date +%Y%m%d-%H%M%S)"
dest="$ROOT/logs/${file%.*}-$stamp.${file##*.}"
for p in "${path_variants[@]}"; do
    if dp_curl --fail -G -o "$dest" "$DP_URL/api/filesystem/apps/file" \
        --data-urlencode "knownfolderid=LocalAppData" \
        --data-urlencode "packagefullname=$pfn" \
        --data-urlencode "path=$p" \
        --data-urlencode "filename=$file" 2>/dev/null; then
        ln -sf "$(basename "$dest")" "$ROOT/logs/latest.log"
        dp_info "Saved $dest ($(wc -c <"$dest") bytes, path variant '$p')"
        [[ $quiet -eq 1 ]] || cat "$dest"
        exit 0
    fi
done
rm -f "$dest"

dp_info "Download failed. Contents of the package's LocalAppData root, for diagnosis:"
dp_curl -G "$DP_URL/api/filesystem/apps/files" \
    --data-urlencode "knownfolderid=LocalAppData" \
    --data-urlencode "packagefullname=$pfn" >&2 || true
echo >&2
dp_die "could not download LocalState\\$file (has the app been launched at least once?)"
