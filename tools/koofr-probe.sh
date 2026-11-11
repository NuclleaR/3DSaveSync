#!/usr/bin/env bash
# Checks how Koofr's WebDAV answers the calls 3DSaveSync relies on
# (MKCOL, PUT, PROPFIND, MOVE with Overwrite: F, GET, DELETE).
# Prints one line per call with the HTTP status code and saves one real
# PROPFIND response as a test fixture. Credentials are kept in a temporary
# netrc file that is removed on exit; nothing else is written.
#
# Usage: bash tools/koofr-probe.sh
set -euo pipefail

BASE="https://app.koofr.net/dav/Koofr"
FIXTURE="$(dirname "$0")/../tests/fixtures/koofr-propfind.xml"
PROPFIND_BODY='<?xml version="1.0" encoding="utf-8"?><d:propfind xmlns:d="DAV:"><d:prop><d:resourcetype/><d:getcontentlength/></d:prop></d:propfind>'

# Display path and its percent-encoded form (space = %20, é = %C3%A9).
P="/3DSaveSync-probe"
P_SRC="$P/src"
P_SUB="$P/src/sub dir";                 E_SUB="$P/src/sub%20dir"
P_FILE="$P/src/sub dir/file é.txt";     E_FILE="$P/src/sub%20dir/file%20%C3%A9.txt"
P_DST="$P/dst"
P_MOVED="$P/dst/moved"
P_MOVED_FILE="$P/dst/moved/file é.txt"; E_MOVED_FILE="$P/dst/moved/file%20%C3%A9.txt"
P_TAKEN="$P/dst/taken"
P_NOPE="$P/nope"

IFS= read -rp "Koofr email: " user
IFS= read -rsp "Koofr app password: " pass
echo

# netrc value in double quotes, so spaces and special characters survive
# (curl >= 7.84 understands quoted netrc strings with \" and \\ escapes).
netrc_quote() {
    local s="${1//\\/\\\\}"
    s="${s//\"/\\\"}"
    printf '"%s"' "$s"
}

netrc="$(mktemp)"
out="$(mktemp)"
trap 'rm -f "$netrc" "$out"' EXIT
chmod 600 "$netrc"
printf 'machine app.koofr.net login %s password %s\n' "$(netrc_quote "$user")" "$(netrc_quote "$pass")" > "$netrc"
unset pass

# call METHOD DISPLAY_PATH ENCODED_PATH [extra curl args...]
call() {
    local method="$1" shown="$2" encoded="$3"
    shift 3
    local code
    code="$(curl --netrc-file "$netrc" -s -o "$out" -w '%{http_code}' -X "$method" "$@" "$BASE$encoded")" || code="curl-error"
    printf '%-9s %s -> %s\n' "$method" "$shown" "$code"
}

propfind() {
    call PROPFIND "$1" "$2" -H "Depth: $3" -H 'Content-Type: application/xml' --data-binary "$PROPFIND_BODY"
}

existing="$(curl --netrc-file "$netrc" -s -o /dev/null -w '%{http_code}' -X PROPFIND -H 'Depth: 0' "$BASE$P")" || existing="curl-error"
if [ "$existing" = "207" ]; then
    echo "$P already exists in Koofr. Remove it (or rename it) and run again." >&2
    exit 1
fi
if [ "$existing" = "401" ] || [ "$existing" = "403" ]; then
    echo "Koofr rejected the login ($existing). Check the email and app password." >&2
    exit 1
fi

call MKCOL "$P" "$P"
call MKCOL "$P (again, already exists)" "$P"
call MKCOL "$P_SRC" "$P_SRC"
call MKCOL "$P_SUB" "$E_SUB"
call PUT "$P_FILE" "$E_FILE" -H 'Content-Type: application/octet-stream' --data-binary 'hello'

propfind "$P_SUB (Depth 1)" "$E_SUB" 1
mkdir -p "$(dirname "$FIXTURE")"
cp "$out" "$FIXTURE"
echo "          PROPFIND response saved to tests/fixtures/koofr-propfind.xml"

call MKCOL "$P_DST" "$P_DST"
call MOVE "$P_SUB -> $P_MOVED (Overwrite: F)" "$E_SUB" -H "Destination: $BASE$P_MOVED" -H 'Overwrite: F'
call MKCOL "$P_TAKEN" "$P_TAKEN"
call MOVE "$P_MOVED -> $P_TAKEN (destination exists, Overwrite: F)" "$P_MOVED" -H "Destination: $BASE$P_TAKEN" -H 'Overwrite: F'

call GET "$P_MOVED_FILE" "$E_MOVED_FILE"
printf '          body: %s\n' "$(cat "$out")"

propfind "$P_NOPE (missing, Depth 1)" "$P_NOPE" 1

call DELETE "$P" "$P"
call DELETE "$P (again, already gone)" "$P"
