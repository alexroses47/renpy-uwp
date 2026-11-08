#!/usr/bin/env bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

# Deploys the signed RenpyUWP package to the Xbox (Developer Mode) from Linux.
#
# Usage:
#   ci/deploy.sh                      # download the latest successful build-host artifact and install it
#   ci/deploy.sh --run <RUN_ID>       # use the artifact of a specific workflow run
#   ci/deploy.sh --dir <DIR>          # use an already downloaded artifact directory
#   Options: --install-cert  trust our .cer on the console first (only if install fails with a trust error)
#            --rest          skip openappx, use the raw Device Portal REST API with curl
#            --no-start      do not launch the app after installing
#
# Requires XBOX_DP_USER / XBOX_DP_PASS (optional XBOX_DP_URL). See ci/lib/devportal.sh.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=lib/devportal.sh
source "$ROOT/ci/lib/devportal.sh"

OPENAPPX_VERSION="0.7.0"
WORKFLOW="build-host.yml"
ARTIFACT="renpy-uwp-x64"
APP_ID="App"

run_id="" dir="" install_cert=0 use_rest=0 start=1
while [[ $# -gt 0 ]]; do
    case "$1" in
        --run) run_id="$2"; shift 2 ;;
        --dir) dir="$2"; shift 2 ;;
        --install-cert) install_cert=1; shift ;;
        --rest) use_rest=1; shift ;;
        --no-start) start=0; shift ;;
        -h|--help) sed -n '2,15p' "$0"; exit 0 ;;
        *) dp_die "unknown option: $1" ;;
    esac
done

dp_require_creds

# --- 1. Artifact -------------------------------------------------------------
if [[ -z "$dir" ]]; then
    command -v gh >/dev/null || dp_die "gh CLI is required to download the artifact (or pass --dir)"
    if [[ -z "$run_id" ]]; then
        run_id="$(gh run list --workflow "$WORKFLOW" --branch main --status success --limit 1 \
            --json databaseId --jq '.[0].databaseId')"
        [[ -n "$run_id" ]] || dp_die "no successful $WORKFLOW run found"
    fi
    dir="$ROOT/out/artifacts/$run_id"
    if [[ ! -d "$dir" ]]; then
        dp_info "Downloading artifact $ARTIFACT from run $run_id"
        gh run download "$run_id" -n "$ARTIFACT" -D "$dir"
    fi
fi
[[ -d "$dir" ]] || dp_die "artifact directory not found: $dir"

pkg="$(find "$dir" -maxdepth 1 \( -name '*.msix' -o -name '*.appx' \) | head -n1)"
[[ -n "$pkg" ]] || dp_die "no .msix/.appx in $dir"
mapfile -t deps < <(find "$dir/Dependencies/x64" -maxdepth 1 -name '*.appx' 2>/dev/null | sort)
cer="$(find "$dir" -maxdepth 1 -name '*.cer' | head -n1)"
dp_info "Package: $(basename "$pkg")"
[[ -f "$dir/package-info.json" ]] && cat "$dir/package-info.json" >&2 && echo >&2
for d in "${deps[@]}"; do dp_info "Dependency: $(basename "$d")"; done

# --- 2. Device Portal pins ---------------------------------------------------
dp_load_pins

# --- 3. openappx -------------------------------------------------------------
openappx_bin=""
if [[ $use_rest -eq 0 ]]; then
    if command -v openappx >/dev/null; then
        openappx_bin="$(command -v openappx)"
    else
        venv="${XDG_DATA_HOME:-$HOME/.local/share}/renpy-uwp/venv"
        if [[ ! -x "$venv/bin/openappx" ]]; then
            dp_info "Installing openappx $OPENAPPX_VERSION into $venv"
            python3 -m venv "$venv"
            "$venv/bin/pip" install -q "openappx[sign]==$OPENAPPX_VERSION"
        fi
        openappx_bin="$venv/bin/openappx"
    fi
fi

oa() {
    OPENAPPX_DEVICE_PASSWORD="$XBOX_DP_PASS" "$openappx_bin" deploy \
        --device "$DP_URL" --user "$XBOX_DP_USER" --pin-sha256 "$DP_CERT_PIN" "$@"
}

if [[ $install_cert -eq 1 ]]; then
    [[ -n "$cer" ]] || dp_die "no .cer in $dir"
    [[ -n "$openappx_bin" ]] || dp_die "--install-cert needs openappx (do not combine with --rest)"
    dp_info "Installing certificate $(basename "$cer") on the console"
    oa --install-cert "$cer"
fi

# --- 4. Install --------------------------------------------------------------
installed=0
if [[ -n "$openappx_bin" ]]; then
    dp_info "Installing with openappx"
    if oa --package "$pkg" ${deps[@]:+--also-upload "${deps[@]}"}; then
        installed=1
    else
        dp_info "openappx failed; falling back to the REST API with curl"
    fi
fi
if [[ $installed -eq 0 ]]; then
    dp_rest_install "$pkg" "${deps[@]}"
fi

# --- 5. Verify ---------------------------------------------------------------
pfn="$(dp_package_full_name)"
[[ -n "$pfn" ]] || dp_die "package $DP_PACKAGE_NAME is not listed on the console after install"
dp_info "Installed: $pfn"
if [[ -f "$dir/package-info.json" ]]; then
    want="$(python3 -I -c 'import json,sys; print(json.load(open(sys.argv[1], encoding="utf-8-sig"))["version"])' "$dir/package-info.json")"
    [[ "$pfn" == *"_${want}_"* ]] || dp_die "installed version does not match the artifact ($want)"
fi

# --- 6. Launch ---------------------------------------------------------------
if [[ $start -eq 1 ]]; then
    dp_info "Launching $APP_ID"
    if [[ -n "$openappx_bin" ]] && oa --start "$pfn" --app-id "$APP_ID"; then
        :
    else
        dp_rest_start "$pfn" "$APP_ID"
    fi
fi
dp_info "Done. Fetch the log with: ci/fetch-logs.sh"
