#!/usr/bin/env bash
#
# upload_dist.sh — publish a finished drop to baker76.com over FTP.
#
#   scripts/upload_dist.sh                # upload the web bundle + the binaries
#   scripts/upload_dist.sh --web-only     # just dist/web/* -> /public_html/apps/swos
#   scripts/upload_dist.sh --bin-only     # just the archives -> /public_html/download/swos
#   scripts/upload_dist.sh --page-only    # just the downloads page (baker76.com/swos.html),
#                                         # regenerated from the drop already in dist/
#   scripts/upload_dist.sh --dry-run      # print what would go where, upload nothing
#   scripts/upload_dist.sh --no-tls       # plain FTP (password in cleartext)
#
# KNOWING WHEN IT IS FINISHED, WITHOUT WATCHING IT
#
# 112 MB over a home connection takes 20+ minutes, and for minutes at a time a
# single big file in flight looks exactly like a hang. So the run publishes its
# own state instead of making you guess from the log:
#
#   dist/.upload-status      one line, rewritten atomically:
#                              running  file=3/10 name=SWOS-full.zip ...
#                              done     rc=0 ok=10 failed=0 bytes=... elapsed=...
#                              failed   rc=1 ...
#   == DONE rc=N ...         the LAST line of the log, on every exit path
#
# Both come from the EXIT trap, so a crash, a Ctrl-C or a kill still lands in a
# terminal state rather than leaving "running" behind for ever. To wait for it
# from a script, block on the file and stop when it is no longer running:
#
#   until grep -qv '^running' dist/.upload-status 2>/dev/null; do sleep 10; done
#
# Do NOT poll by sleeping a guessed interval and tailing the log: that is how
# you end up with a pile of timers firing long after the upload finished.
#
# Run it AFTER scripts/build_dist.sh and scripts/build_macos.sh have both
# finished — it uploads what is in dist/ and never builds anything. It is a
# separate script rather than a step inside build_dist.sh on purpose: building a
# drop is something you do many times a day, publishing it to a public website
# is not, and the two should not share a trigger.
#
# CREDENTIALS COME FROM THE ENVIRONMENT, AND STAY OUT OF THE PROCESS TABLE
#
# FTP_USERNAME and FTP_PASSWORD (set in ~/.bashrc here). They are never passed
# as command-line arguments: `curl -u user:pass` is readable by any other user
# on the machine via `ps`, and lands in shell history. Instead they are written
# to a mode-0600 temp file that curl reads with --config, deleted on exit. For
# the same reason nothing in this script ever echoes $FTP_PASSWORD, including in
# --dry-run and including in error messages.
#
# TLS IS THE DEFAULT, AND THAT IS A REAL DECISION
#
# Plain FTP sends the password in cleartext over the wire. --ssl-reqd upgrades
# the connection with AUTH TLS (same port 21, still passive) and FAILS rather
# than falling back, so a server that quietly stops offering TLS cannot silently
# downgrade us. If baker76.com does not support FTPS at all, --no-tls disables
# it — but then the password is on the wire in the clear on every run.
#
# TLS ON SHARED HOSTING — WHY --insecure-tls EXISTS
#
# baker76.com is GoDaddy shared hosting, and its FTP service presents GoDaddy's
# OWN certificate (CN=*.prod.phx3.secureserver.net, issued by Starfield), not
# one for baker76.com. So the certificate is genuine and correctly issued, but
# the NAME does not match what we connected to, and curl rejects it with
# exit 60. Measured 2026-09-16, not assumed.
#
# There are three ways out, in descending order of how much they protect you:
#
#   1. Connect to the name the certificate actually covers — your cPanel server
#      hostname, e.g. a2plcpnl0123.prod.phx3.secureserver.net (cPanel shows it
#      under Server Information; it is NOT derivable from DNS — the reverse
#      lookup of the IP gives a *.host.secureserver.net name, a different parent
#      that the certificate does not cover). Then verification passes normally:
#           FTP_HOST=<that name> scripts/upload_dist.sh
#      Full encryption AND a verified server. This is the one to prefer.
#   2. --insecure-tls: still encrypted, so the password cannot be read by
#      someone watching the network, but the server is not authenticated, so it
#      is not proof against an active man-in-the-middle.
#   3. --no-tls: the password crosses the internet in cleartext. Avoid.
#
# UPLOADS ARE ATOMIC-ISH: UPLOAD TO .part, THEN RENAME
#
# SWOS.data is 22 MB and SWOS-full.zip is 35 MB. A visitor who fetches either
# one midway through a plain overwrite gets a truncated file that looks valid to
# the browser. So every file is uploaded as <name>.part and renamed onto the
# real name (RNFR/RNTO) only once the transfer completed — a rename is atomic on
# the server, so the published name is never a half-written file. A failed
# upload leaves the .part behind and the old file still serving.
#
# REMOTE PATHS ARE RELATIVE TO THE LOGIN DIRECTORY
#
# The paths below read as absolute (/public_html/apps/swos) because that is what
# every FTP client shows you — but FTP has no server-root concept: what a client
# calls "/" is wherever the login lands, which on cPanel-style hosting is the
# account home, and public_html sits inside it. curl follows the same rule, so
# the leading slash is stripped when the URL is built. If you ever genuinely
# need a path from the server root, give it a DOUBLE leading slash.

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 1

# ---- destination (override via the environment) -----------------------------
FTP_HOST="${FTP_HOST:-baker76.com}"
WEB_DIR="${FTP_WEB_DIR:-/public_html/apps/swos}"
BIN_DIR="${FTP_BIN_DIR:-/public_html/download/swos}"
# The downloads page players are pointed at: baker76.com/swos.html. Generated
# from packaging/web/swos.html just before the upload (below), so its build,
# date and sizes are the ones actually going out.
PAGE_DIR="${FTP_PAGE_DIR:-/public_html}"
PAGE_FILE="swos.html"
# ...and the changelog it embeds (packaging/web/changelog.html, edited by hand), which
# goes with it -- so `--page-only` publishes a new changelog entry without a drop.
CHANGELOG_SRC="packaging/web"
CHANGELOG_FILE="changelog.html"

# The WASM bundle. Built by `make WASM=1` into dist/web and deliberately kept
# out of every zip (the .data preload alone is ~21 MB) — the web build IS this
# upload.
WEB_FILES="SWOS.data SWOS.html SWOS.js SWOS.wasm"
WEB_SRC="dist/web"

# The tester-facing archives. dist/SWOS.nsp is NOT here: the Switch package is
# not something the website hands out.
BIN_FILES="SWOS-Amiga.zip SWOS-full.zip SWOS-macOS-app.zip SWOS-macOS.zip SWOS-Setup.exe SWOS.zip"
BIN_SRC="dist"

DO_WEB=1
DO_BIN=1
DO_PAGE=1     # the downloads page goes with the archives it lists
DRY_RUN=0
USE_TLS=1
INSECURE_TLS=0
FORCE=0
VERIFY_ONLY=0

usage() {
    awk 'NR >= 3 { if ($0 !~ /^#/) exit; sub(/^# ?/, ""); print }' "${BASH_SOURCE[0]}"
    cat <<EOF

Options:
      --web-only      upload only $WEB_SRC/* -> $WEB_DIR
      --bin-only      upload only the archives -> $BIN_DIR (and the page)
      --page-only     upload only the downloads page -> $PAGE_DIR/$PAGE_FILE and
                      the changelog it embeds ($CHANGELOG_FILE), describing the
                      archives in dist/ (all must be there)
      --dry-run       show what would be uploaded, transfer nothing
      --verify-only   upload nothing; just compare the sizes already on the
                      server against dist/ (re-runs the check after a fix)
      --insecure-tls  encrypt, but don't verify the server's certificate
                      (needed on shared hosting whose FTP cert is issued to the
                      PROVIDER, not to your domain — see TLS ON SHARED HOSTING)
      --no-tls        plain FTP instead of AUTH TLS (cleartext password)
      --force         upload even from a -dirty tree
  -h, --help          this text

Environment:
  FTP_USERNAME  login      (required)
  FTP_PASSWORD  password   (required)
  FTP_HOST      server     (default: $FTP_HOST)
  FTP_WEB_DIR   web dir    (default: $WEB_DIR)
  FTP_BIN_DIR   binary dir (default: $BIN_DIR)

Exit codes:
  0  everything uploaded      2  bad arguments
  1  something failed         3  nothing to do
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --web-only) DO_BIN=0; DO_PAGE=0; shift ;;
        --bin-only) DO_WEB=0; shift ;;
        --page-only) DO_WEB=0; DO_BIN=0; DO_PAGE=1; shift ;;
        --dry-run)  DRY_RUN=1; shift ;;
        --no-tls)   USE_TLS=0; shift ;;
        --insecure-tls) INSECURE_TLS=1; shift ;;
        --verify-only)  VERIFY_ONLY=1; shift ;;
        --force)    FORCE=1; shift ;;
        -h|--help)  usage; exit 0 ;;
        *) echo "upload_dist: unknown option '$1' (try --help)" >&2; exit 2 ;;
    esac
done

die() { echo "upload_dist: $*" >&2; exit 1; }
banner() { echo ""; echo "======================================================================"; echo "== $*"; echo "======================================================================"; }

[ "$DO_WEB" = 0 ] && [ "$DO_BIN" = 0 ] && [ "$DO_PAGE" = 0 ] && die "--web-only and --bin-only are mutually exclusive"

# ---- credentials ------------------------------------------------------------
[ -n "${FTP_USERNAME:-}" ] || die "FTP_USERNAME is not set (see --help)"
[ -n "${FTP_PASSWORD:-}" ] || die "FTP_PASSWORD is not set (see --help)"

# ---- don't publish a build you can't identify -------------------------------
# Same rule, and the same paths, as scripts/gen_build_info.sh: a -dirty stamp
# means the binaries are NOT the commit they claim, and a tester bug report
# against an unidentifiable build is the expensive kind. Publishing is the last
# place that should be allowed through.
if [ "$FORCE" = 0 ] && git rev-parse --git-dir >/dev/null 2>&1; then
    if ! git diff --quiet HEAD -- src include Makefile Makefile.amiga scripts 2>/dev/null; then
        echo "upload_dist: the tree is dirty — tracked source differs from HEAD, so the" >&2
        echo "             binaries in dist/ are stamped -dirty and cannot be traced to a" >&2
        echo "             commit. Commit first, rebuild, then upload (or --force)." >&2
        exit 1
    fi
fi
BUILD_COMMIT="$(git rev-parse --short HEAD 2>/dev/null || echo unknown)"

# ---- preflight: every file must exist BEFORE anything is uploaded -----------
# A half-published drop is worse than an unpublished one — the download page
# would offer a new Windows build beside yesterday's macOS build. So the whole
# set is checked up front and the run aborts before the first byte goes out.
MISSING=""
QUEUE=""      # "localpath|remotedir|basename" per line
queue_add() {
    local src="$1" dir="$2" name="$3"
    if [ -f "$src/$name" ]; then
        QUEUE="$QUEUE$src/$name|$dir|$name"$'\n'
    else
        MISSING="$MISSING  $src/$name"$'\n'
    fi
}

if [ "$DO_WEB" = 1 ]; then
    for f in $WEB_FILES; do queue_add "$WEB_SRC" "$WEB_DIR" "$f"; done
fi
if [ "$DO_BIN" = 1 ]; then
    for f in $BIN_FILES; do queue_add "$BIN_SRC" "$BIN_DIR" "$f"; done
fi
if [ "$DO_PAGE" = 1 ]; then
    # The page lists every archive, so each must be here to size it -- also when
    # only the page is going out, or it would advertise a "—" for one. (With the
    # archives going too, the queue above has already said which are missing.)
    if [ "$DO_BIN" = 0 ]; then
        for f in $BIN_FILES; do
            [ -f "$BIN_SRC/$f" ] || MISSING="$MISSING  $BIN_SRC/$f (the page lists it)"$'\n'
        done
    fi
    # Regenerate the page HERE: every archive is present, so the sizes and the
    # build on it are this drop's. (build_dist.sh also writes one, but the macOS
    # archives arrive after it.)
    if [ -z "$MISSING" ] && ! scripts/gen_downloads_page.sh; then
        echo "upload_dist: the downloads page could not be generated — not uploading" >&2
        exit 1
    fi
    queue_add "dist" "$PAGE_DIR" "$PAGE_FILE"
    queue_add "$CHANGELOG_SRC" "$PAGE_DIR" "$CHANGELOG_FILE"
fi

if [ -n "$MISSING" ]; then
    echo "upload_dist: these files are not in dist/ — build the drop first:" >&2
    printf '%s' "$MISSING" >&2
    echo "" >&2
    echo "  scripts/build_dist.sh        # everything this machine can build" >&2
    echo "  scripts/build_macos.sh       # the two macOS archives" >&2
    exit 1
fi
[ -n "$QUEUE" ] || { echo "upload_dist: nothing to upload"; exit 3; }

# ---- build the curl credential config ---------------------------------------
# Quoted form, so a password containing spaces or '#' survives; backslash and
# double-quote are escaped because curl unescapes them inside a quoted value.
# umask first: the file must never exist even briefly as world-readable.
CFG=""

# ---- progress state ---------------------------------------------------------
# Written atomically (temp + mv) so a reader never catches a half-written line,
# and finalised from the EXIT trap so EVERY exit path -- success, failure,
# Ctrl-C, SIGTERM -- ends in a terminal state. A status file that can be left
# saying "running" after the process is gone is worse than none: it teaches you
# to distrust it and go back to watching the log.
STATUS_FILE="${UPLOAD_STATUS_FILE:-dist/.upload-status}"
# Elapsed time comes from a recorded epoch, never from $SECONDS: SECONDS can be
# inherited from the environment, which silently shifts its base -- a test run
# here reported "elapsed=-1". A status file nobody trusts is no better than no
# status file.
START_EPOCH="$(date +%s)"
elapsed() { echo $(( $(date +%s) - START_EPOCH )); }
STATUS_LIVE=0            # 1 once the upload proper has started
OK_COUNT=0
FAIL_COUNT=0
TOTAL_BYTES=0
QUEUE_TOTAL=0

status() {
    [ -n "$STATUS_FILE" ] || return 0
    mkdir -p "$(dirname "$STATUS_FILE")" 2>/dev/null || return 0
    printf '%s\n' "$*" > "$STATUS_FILE.tmp" 2>/dev/null &&
        mv -f "$STATUS_FILE.tmp" "$STATUS_FILE" 2>/dev/null
    return 0
}

hms() {
    local t="${1:-0}"
    if [ "$t" -ge 60 ]; then printf '%dm%02ds' "$((t / 60))" "$((t % 60))"
    else printf '%ds' "$t"; fi
}

# The transfer runs as a background child so `wait` can be INTERRUPTED by a
# signal. With curl in the foreground bash defers the trap until the child
# returns, so a killed run would sit at "running" for up to --max-time (30
# minutes) with the process already gone -- exactly the state this file exists
# to make impossible. Measured, not assumed.
CURL_PID=""
on_signal() {
    trap - INT TERM
    [ -n "$CURL_PID" ] && kill -TERM "$CURL_PID" 2>/dev/null
    echo "" >&2
    echo "-- interrupted; a part-file is left on the server and the previous file is" >&2
    echo "   still the one being served." >&2
    exit 143            # 128 + SIGTERM, so the EXIT trap records a failure
}

cleanup() {
    local rc=$?
    [ -n "$CFG" ] && rm -f "$CFG"
    if [ "$STATUS_LIVE" = 1 ]; then
        if [ "$rc" = 0 ]; then
            status "done rc=0 ok=$OK_COUNT failed=$FAIL_COUNT bytes=$TOTAL_BYTES elapsed=$(elapsed)"
        else
            status "failed rc=$rc ok=$OK_COUNT failed=$FAIL_COUNT bytes=$TOTAL_BYTES elapsed=$(elapsed)"
        fi
        # The one line a watcher greps for. Last thing printed, always.
        echo ""
        echo "== DONE rc=$rc  ($OK_COUNT uploaded, $FAIL_COUNT failed, $(human "$TOTAL_BYTES") in $(hms "$(elapsed)"))"
    fi
    return $rc
}
trap cleanup EXIT
trap on_signal INT TERM

if [ "$DRY_RUN" = 0 ]; then
    OLD_UMASK="$(umask)"
    umask 077
    CFG="$(mktemp "${TMPDIR:-/tmp}/.swos-ftp.XXXXXXXX")" || die "cannot create temp file"
    umask "$OLD_UMASK"
    esc() { printf '%s' "$1" | sed 's/\\/\\\\/g; s/"/\\"/g'; }
    printf 'user = "%s:%s"\n' "$(esc "$FTP_USERNAME")" "$(esc "$FTP_PASSWORD")" > "$CFG" ||
        die "cannot write temp credential file"
fi

# ---- curl options -----------------------------------------------------------
# --ftp-pasv        passive, as the host requires
# --ftp-create-dirs make the remote directory if it isn't there yet
# --ssl-reqd        AUTH TLS, and fail rather than fall back to cleartext
# --retry           a dropped 22 MB transfer should not need a human
# --max-time        generous: 35 MB on a slow uplink is minutes, not seconds
#
# The retries exist for a transfer that DROPS midway, not for a server that
# isn't there — see the preflight below for why that distinction is load-bearing.
#
# TLS_OPTS is computed ONCE and used by all three curl call sites (preflight,
# upload, verify). It used to be spelled out at each of them, and that cost a
# real bug on 2026-09-16: --insecure was added to the upload and the preflight
# but not to the verify, so every verify connection failed certificate
# validation, returned nothing, and was reported as "server did not report
# SIZE". The upload was fine and the check silently wasn't running. One
# definition, three users — the drift cannot happen again.
TLS_OPTS=""
if [ "$USE_TLS" = 1 ]; then
    TLS_OPTS="--ssl-reqd"
    # --insecure only ever applies ON TOP of --ssl-reqd: it relaxes who we
    # trust, never whether the connection is encrypted.
    [ "$INSECURE_TLS" = 1 ] && TLS_OPTS="$TLS_OPTS --insecure"
fi
CURL_OPTS="--ftp-pasv --ftp-create-dirs --connect-timeout 20 --max-time 1800 --retry 3 --retry-delay 5 --fail --show-error $TLS_OPTS"
# A progress bar is useful at a terminal and noise in a log.
if [ -t 1 ]; then CURL_OPTS="$CURL_OPTS --progress-bar"; else CURL_OPTS="$CURL_OPTS --silent"; fi

# Strip the single leading slash: see "REMOTE PATHS" above. A double slash is
# left alone, which is how you ask for a real server-root path.
remote_url() {
    local dir="$1" name="$2"
    case "$dir" in
        //*) printf 'ftp://%s/%s/%s' "$FTP_HOST" "$dir" "$name" ;;
        /*)  printf 'ftp://%s/%s/%s' "$FTP_HOST" "${dir#/}" "$name" ;;
        *)   printf 'ftp://%s/%s/%s' "$FTP_HOST" "$dir" "$name" ;;
    esac
}

human() { numfmt --to=iec --suffix=B "$1" 2>/dev/null || echo "$1 bytes"; }

# ---- preflight: prove the connection ONCE, before sending 111 MB ------------
# One directory listing of the login dir validates DNS, reachability, TLS and
# the credentials in a single round trip, and does it in seconds.
#
# Without this the failure modes are miserable. An unreachable host does not
# refuse the connection, it BLACK-HOLES it: curl waits out --connect-timeout in
# full, and --retry then does the whole thing again. Measured here against a
# dead port: 30 s per attempt, ~2 min per file, which across ten files is twenty
# minutes of an apparently-hung script before the first error is printed. Wrong
# credentials are worse in a different way — without a preflight you find out
# after uploading a 22 MB .part.
#
# So: retries are for a transfer that drops midway, which is a transient fault
# worth re-trying. "The server is not there" and "that password is wrong" are
# not transient, and they are cheaper to discover up front.
preflight() {
    local out rc
    out="$(curl --config "$CFG" --ftp-pasv $TLS_OPTS \
                --connect-timeout 15 --max-time 45 --silent --show-error \
                --list-only "ftp://$FTP_HOST/" 2>&1)"
    rc=$?
    [ "$rc" = 0 ] && return 0

    echo "" >&2
    echo "upload_dist: cannot reach $FTP_HOST — nothing was uploaded." >&2
    case "$rc" in
        6)  echo "   DNS: '$FTP_HOST' does not resolve. Check FTP_HOST." >&2 ;;
        7)  echo "   The connection was refused. Is FTP enabled on this host?" >&2 ;;
        28) echo "   Timed out connecting. The host is not answering on port 21" >&2
            echo "   (a firewall that drops rather than refuses looks exactly like this)." >&2 ;;
        67) echo "   Login denied. Check FTP_USERNAME / FTP_PASSWORD." >&2 ;;
        60) echo "   The server's TLS certificate is not valid for '$FTP_HOST'." >&2
            echo "" >&2
            echo "   On shared hosting this is normal and does NOT mean anything is wrong:" >&2
            echo "   the FTP service presents the hosting PROVIDER's certificate rather than" >&2
            echo "   one issued for your domain. Three ways forward, best first:" >&2
            echo "" >&2
            echo "     1. Connect to the name the certificate covers — your cPanel server" >&2
            echo "        hostname (cPanel -> Server Information). Fully verified:" >&2
            echo "          FTP_HOST=<server>.prod.phx3.secureserver.net $0" >&2
            echo "     2. Encrypt without verifying the server (no passive eavesdropping," >&2
            echo "        but not proof against an active man-in-the-middle):" >&2
            echo "          $0 --insecure-tls" >&2
            echo "     3. $0 --no-tls   (password in CLEARTEXT — avoid)" >&2
            echo "" >&2
            echo "   The certificate actually presented was:" >&2
            echo "     $(timeout 15 openssl s_client -starttls ftp -connect "$FTP_HOST:21" \
                        </dev/null 2>/dev/null | openssl x509 -noout -subject 2>/dev/null \
                        || echo '(could not read it)')" >&2 ;;
        64|35)
            if [ "$USE_TLS" = 1 ]; then
                echo "   The server refused AUTH TLS. If baker76.com offers no FTPS:" >&2
                echo "     scripts/upload_dist.sh --no-tls" >&2
                echo "   — but that sends the password over the wire in cleartext." >&2
            else
                echo "   TLS error: $out" >&2
            fi ;;
        *)  echo "   curl exit $rc: $out" >&2 ;;
    esac
    return 1
}

# ---- upload -----------------------------------------------------------------
banner "upload to $FTP_HOST  (commit $BUILD_COMMIT)"
echo "-- user     : $FTP_USERNAME"
if [ "$USE_TLS" = 0 ]; then
    echo "-- transport: plain FTP, passive — CLEARTEXT PASSWORD"
elif [ "$INSECURE_TLS" = 1 ]; then
    echo "-- transport: FTP + AUTH TLS, passive — encrypted, CERTIFICATE NOT VERIFIED"
else
    echo "-- transport: FTP + AUTH TLS (explicit FTPS), passive — verified"
fi
[ "$DRY_RUN" = 1 ] && echo "-- DRY RUN  : nothing will be transferred"

if [ "$DRY_RUN" = 0 ] && [ "$VERIFY_ONLY" = 0 ]; then
    # Armed here, not at the first file: everything from this point can fail
    # (the connect, the credentials, a signal), and every one of those has to
    # reach the EXIT trap with the status live so it records a terminal state.
    STATUS_LIVE=1
    status "running phase=connect commit=$BUILD_COMMIT started=$(date -u +%FT%TZ)"
fi
if [ "$DRY_RUN" = 0 ]; then
    echo "-- connecting…"
    preflight || exit 1
    echo "-- connection OK, credentials accepted"
fi

FAILED=""
QUEUE_TOTAL="$(printf '%s' "$QUEUE" | grep -c . || true)"
FILE_NO=0
if [ "$STATUS_LIVE" = 1 ]; then
    status "running phase=upload file=0/$QUEUE_TOTAL commit=$BUILD_COMMIT elapsed=$(elapsed)"
fi

while IFS='|' read -r src dir name; do
    [ -n "$src" ] || continue
    [ "$VERIFY_ONLY" = 1 ] && break
    BYTES="$(stat -c %s "$src" 2>/dev/null || stat -f %z "$src" 2>/dev/null || echo 0)"
    URL="$(remote_url "$dir" "$name")"

    if [ "$DRY_RUN" = 1 ]; then
        printf '   %-22s %8s  ->  %s/%s\n' "$name" "$(human "$BYTES")" "$dir" "$name"
        OK_COUNT=$((OK_COUNT + 1))
        TOTAL_BYTES=$((TOTAL_BYTES + BYTES))
        continue
    fi

    FILE_NO=$((FILE_NO + 1))
    FILE_START="$(elapsed)"
    status "running file=$FILE_NO/$QUEUE_TOTAL name=$name bytes=$BYTES done=$OK_COUNT elapsed=$(elapsed)"
    echo ""
    echo "-- [$FILE_NO/$QUEUE_TOTAL] $name  ($(human "$BYTES"))  ->  $dir/"

    # Upload to <name>.part, then rename onto the real name. curl has already
    # CWD'd into the target directory by the time the post-quote commands run,
    # so these are bare basenames, not paths.
    curl --config "$CFG" $CURL_OPTS \
            -T "$src" "$URL.part" \
            -Q "-RNFR $name.part" \
            -Q "-RNTO $name" &
    CURL_PID=$!
    wait "$CURL_PID"
    RC=$?
    CURL_PID=""
    if [ "$RC" = 0 ]; then
        OK_COUNT=$((OK_COUNT + 1))
        TOTAL_BYTES=$((TOTAL_BYTES + BYTES))
        FILE_SECS=$(( $(elapsed) - FILE_START ))
        if [ "$FILE_SECS" -gt 0 ]; then
            echo "   ok  $(hms "$FILE_SECS")  ($(human "$((BYTES / FILE_SECS))")/s)"
        else
            echo "   ok  <1s"
        fi
        status "running file=$FILE_NO/$QUEUE_TOTAL done=$OK_COUNT bytes=$TOTAL_BYTES elapsed=$(elapsed)"
    else
        FAIL_COUNT=$((FAIL_COUNT + 1))
        FAILED="$FAILED  $name (curl exit $RC)"$'\n'
        echo "!! FAILED: $name (curl exit $RC)" >&2
        # 67 is "login denied", the one failure that will hit every remaining
        # file too, so stop rather than hammer the server with nine more.
        if [ "$RC" = 67 ]; then
            echo "!! login denied — check FTP_USERNAME / FTP_PASSWORD. Stopping." >&2
            break
        fi
        # A TLS refusal is equally systemic: say which flag turns it off.
        if [ "$USE_TLS" = 1 ] && { [ "$RC" = 64 ] || [ "$RC" = 35 ]; }; then
            echo "!! the server refused AUTH TLS. If baker76.com has no FTPS, re-run with" >&2
            echo "!!   scripts/upload_dist.sh --no-tls" >&2
            echo "!! (that sends the password in cleartext). Stopping." >&2
            break
        fi
    fi
done <<< "$QUEUE"

# ---- verify -----------------------------------------------------------------
# Ask the server how big each file it now has is, and compare. This catches the
# case the .part rename is designed around from the other side: a transfer that
# curl reported as fine but that landed short.
VERIFY_BAD=""
VERIFY_SKIPPED=0
if [ "$DRY_RUN" = 0 ] && [ "$FAIL_COUNT" = 0 ]; then
    banner "verify (remote SIZE vs local)"
    while IFS='|' read -r src dir name; do
        [ -n "$src" ] || continue
        LOCAL="$(stat -c %s "$src" 2>/dev/null || stat -f %z "$src" 2>/dev/null || echo 0)"
        URL="$(remote_url "$dir" "$name")"
        HEAD="$(curl --config "$CFG" --ftp-pasv $TLS_OPTS \
                     --connect-timeout 20 --max-time 60 --silent --show-error \
                     --head "$URL" 2>&1)"
        HEAD_RC=$?
        REMOTE="$(printf '%s' "$HEAD" | awk '/^Content-Length:/ { gsub(/\r/, ""); print $2 }' | tail -1)"

        # Three outcomes, and they must stay distinguishable. Collapsing the
        # first two into one benign "unverified" is what let a broken check pass
        # silently once already:
        #   - the connection FAILED       -> the check did not run. Not benign.
        #   - connected, no SIZE reported -> the server does not support it.
        #   - connected, size differs     -> a genuinely bad upload.
        if [ "$HEAD_RC" != 0 ]; then
            printf '   %-22s VERIFY CONNECTION FAILED (curl exit %s)\n' "$name" "$HEAD_RC"
            VERIFY_BAD="$VERIFY_BAD  $name (verify connection failed, curl exit $HEAD_RC)"$'\n'
        elif ! printf '%s' "$REMOTE" | grep -qE '^[0-9]+$'; then
            printf '   %-22s %12s  (server did not report SIZE — unverified)\n' "$name" "$LOCAL"
            VERIFY_SKIPPED=$((VERIFY_SKIPPED + 1))
        elif [ "$REMOTE" = "$LOCAL" ]; then
            printf '   %-22s %12s  OK\n' "$name" "$LOCAL"
        else
            printf '   %-22s local=%s remote=%s  MISMATCH\n' "$name" "$LOCAL" "$REMOTE"
            VERIFY_BAD="$VERIFY_BAD  $name (local $LOCAL, remote $REMOTE)"$'\n'
        fi
    done <<< "$QUEUE"
fi

# ---- summary ----------------------------------------------------------------
banner "SUMMARY"
echo "  host           : $FTP_HOST"
echo "  commit         : $BUILD_COMMIT"
[ "$DO_WEB" = 1 ] && echo "  web bundle     : $WEB_DIR"
[ "$DO_BIN" = 1 ] && echo "  archives       : $BIN_DIR"
if [ "$VERIFY_ONLY" = 1 ]; then
    echo "  uploaded       : nothing (--verify-only)"
else
    echo "  uploaded       : $OK_COUNT file(s), $(human "$TOTAL_BYTES")"
fi

if [ "$DRY_RUN" = 1 ]; then
    echo ""
    echo "-- dry run: nothing was transferred."
    exit 0
fi

if [ -n "$FAILED" ]; then
    echo "  FAILED         : $FAIL_COUNT"
    printf '%s' "$FAILED"
    echo ""
    echo "-- a failed upload leaves <name>.part on the server and the previous file"
    echo "   still in place, so the site is not serving a truncated download."
    exit 1
fi

if [ -n "$VERIFY_BAD" ]; then
    echo "  VERIFY         : size mismatch"
    printf '%s' "$VERIFY_BAD"
    exit 1
fi

if [ "$VERIFY_SKIPPED" -gt 0 ]; then
    echo "  verified       : sizes match ($VERIFY_SKIPPED unverified — server did not report SIZE)"
else
    echo "  verified       : all sizes match"
fi
echo ""
[ "$DO_WEB" = 1 ] && echo "-- web build live at https://$FTP_HOST/apps/swos/SWOS.html"
[ "$DO_BIN" = 1 ] && echo "-- downloads live at https://$FTP_HOST/download/swos/"
exit 0
