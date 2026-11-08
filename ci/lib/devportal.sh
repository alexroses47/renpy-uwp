# shellcheck shell=bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

# Shared helpers for talking to the Xbox Windows Device Portal (WDP) REST API with curl.
#
# Environment:
#   XBOX_DP_USER, XBOX_DP_PASS   Device Portal credentials (required, never logged)
#   XBOX_DP_URL                  default https://192.168.100.102:11443
#   XBOX_DP_PUBKEY_PIN           optional: base64 SHA-256 of the portal's public key (curl pin)
#   XBOX_DP_CERT_PIN             optional: hex SHA-256 of the portal's DER certificate (openappx pin)
#
# Pins are trusted on first use and cached in ~/.config/renpy-uwp/.

DP_URL="${XBOX_DP_URL:-https://192.168.100.102:11443}"
DP_URL="${DP_URL%/}"
DP_CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/renpy-uwp"
DP_PACKAGE_NAME="${DP_PACKAGE_NAME:-RenpyUWP}"

dp_die() { echo "error: $*" >&2; exit 1; }
dp_info() { echo "==> $*" >&2; }

dp_require_creds() {
    [[ -n "${XBOX_DP_USER:-}" ]] || dp_die "XBOX_DP_USER is not set"
    [[ -n "${XBOX_DP_PASS:-}" ]] || dp_die "XBOX_DP_PASS is not set"
    command -v curl >/dev/null || dp_die "curl is required"
    command -v openssl >/dev/null || dp_die "openssl is required"
    command -v python3 >/dev/null || dp_die "python3 is required"
}

dp_hostport() {
    local hp="${DP_URL#https://}"
    echo "${hp%%/*}"
}

# Loads (or creates, trust-on-first-use) both pins into DP_PUBKEY_PIN and DP_CERT_PIN.
dp_load_pins() {
    local pub_file="$DP_CONFIG_DIR/xbox-dp.pubkey-sha256" cert_file="$DP_CONFIG_DIR/xbox-dp.cert-sha256"
    DP_PUBKEY_PIN="${XBOX_DP_PUBKEY_PIN:-}"
    DP_CERT_PIN="${XBOX_DP_CERT_PIN:-}"
    [[ -z "$DP_PUBKEY_PIN" && -s "$pub_file" ]] && DP_PUBKEY_PIN="$(<"$pub_file")"
    [[ -z "$DP_CERT_PIN" && -s "$cert_file" ]] && DP_CERT_PIN="$(<"$cert_file")"
    if [[ -n "$DP_PUBKEY_PIN" && -n "$DP_CERT_PIN" ]]; then
        return 0
    fi

    local hp pem
    hp="$(dp_hostport)"
    dp_info "No cached Device Portal pin; fetching certificate from $hp (trust on first use)"
    pem="$(openssl s_client -connect "$hp" </dev/null 2>/dev/null | openssl x509 2>/dev/null)" ||
        dp_die "cannot fetch the Device Portal certificate from $hp (is the console on and Remote Access enabled?)"
    [[ -n "$pem" ]] || dp_die "empty certificate from $hp"
    DP_PUBKEY_PIN="$(openssl x509 -pubkey -noout <<<"$pem" | openssl pkey -pubin -outform der |
        openssl dgst -sha256 -binary | base64)"
    DP_CERT_PIN="$(openssl x509 -outform DER <<<"$pem" | sha256sum | cut -d' ' -f1)"
    mkdir -p "$DP_CONFIG_DIR"
    chmod 700 "$DP_CONFIG_DIR"
    printf '%s\n' "$DP_PUBKEY_PIN" >"$pub_file"
    printf '%s\n' "$DP_CERT_PIN" >"$cert_file"
    openssl x509 -noout -subject -enddate <<<"$pem" >&2
    dp_info "Pinned: pubkey sha256//$DP_PUBKEY_PIN ; cert sha256 $DP_CERT_PIN"
    dp_info "Saved in $DP_CONFIG_DIR (delete those files if the console certificate changes)"
}

# curl wrapper: pinned public key, credentials via a config on stdin (not visible in ps).
dp_curl() {
    local s="${XBOX_DP_USER}:${XBOX_DP_PASS}"
    s="${s//\\/\\\\}"
    s="${s//\"/\\\"}"
    printf 'user = "%s"\n' "$s" |
        curl -sS -k --pinnedpubkey "sha256//$DP_PUBKEY_PIN" --connect-timeout 10 -K - "$@"
}

# Prints the JSON list of installed packages.
dp_list_packages() {
    dp_curl --fail "$DP_URL/api/app/packagemanager/packages"
}

# Prints the PackageFullName of the installed package named $1 (default RenpyUWP), or nothing.
dp_package_full_name() {
    local name="${1:-$DP_PACKAGE_NAME}"
    dp_list_packages | python3 -I -c '
import json, sys
name = sys.argv[1]
pkgs = json.load(sys.stdin).get("InstalledPackages", [])
# On Xbox, "Name" holds the display name, so match on the identity prefix of the full name.
match = [p for p in pkgs if p.get("PackageFullName", "").split("_")[0] == name]
if match:
    print(match[0]["PackageFullName"])
' "$name"
}

# Obtains a CSRF cookie jar for non-GET requests. Sets DP_JAR and DP_CSRF.
dp_csrf() {
    DP_JAR="$(mktemp)"
    trap 'rm -f "$DP_JAR"' EXIT
    dp_curl --fail -c "$DP_JAR" -o /dev/null "$DP_URL/api/os/info"
    DP_CSRF="$(awk '$6 == "CSRF-Token" {print $7}' "$DP_JAR")"
    [[ -n "$DP_CSRF" ]] || dp_die "Device Portal did not return a CSRF-Token cookie"
}

# Installs a package (+ dependencies) with the raw REST API. Args: package [deps...]
dp_rest_install() {
    local pkg="$1"
    shift
    local args=(-F "file=@$pkg")
    local d
    for d in "$@"; do args+=(-F "file=@$d"); done
    dp_csrf
    dp_info "Uploading $(basename "$pkg") (+$# dependencies) via REST"
    dp_curl --fail-with-body -b "$DP_JAR" -H "X-CSRF-Token: $DP_CSRF" -X POST "${args[@]}" \
        "$DP_URL/api/app/packagemanager/package?package=$(basename "$pkg")"
    echo
    local code body i
    for i in $(seq 1 120); do
        body="$(dp_curl -w '\n%{http_code}' "$DP_URL/api/app/packagemanager/state")"
        code="${body##*$'\n'}"
        body="${body%$'\n'*}"
        if [[ "$code" == "204" ]]; then
            sleep 2
            continue
        fi
        dp_info "Install state (HTTP $code): $body"
        return 0
    done
    dp_die "timed out waiting for installation"
}

# Launches an installed app with the REST API. Args: PackageFullName AppId
dp_rest_start() {
    local pfn="$1" app_id="$2" pfam aumid
    # Name_Version_Arch_ResourceId_PublisherId -> Name_PublisherId
    pfam="$(python3 -I -c 'import sys; p=sys.argv[1].split("_"); print(p[0] + "_" + p[-1])' "$pfn")"
    aumid="$pfam!$app_id"
    dp_csrf
    dp_curl --fail-with-body -b "$DP_JAR" -H "X-CSRF-Token: $DP_CSRF" -X POST -H 'Content-Length: 0' \
        "$DP_URL/api/taskmanager/app?appid=$(dp_b64url "$aumid")&package=$(dp_b64url "$pfn")"
    echo
}

# base64 (what the WDP docs call "hex64"), then percent-encoded for a query string.
dp_b64url() {
    python3 -I -c 'import base64, sys, urllib.parse; print(urllib.parse.quote(base64.b64encode(sys.argv[1].encode()).decode(), safe=""))' "$1"
}
