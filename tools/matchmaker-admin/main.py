#!/usr/bin/env python3
"""matchmaker-admin: a small web trigger for a handful of matchmaker
maintenance actions (git pull, rebuild, systemd service control).

This process runs as an UNPRIVILEGED user and binds 127.0.0.1 only - nginx
reverse-proxies it from outside with auth_basic (see README.md), so it's never
reachable without a password and never reachable except through TLS.

Two tiers, by whether the action actually needs root:
  - pull/build/status need no elevated privilege at all (git, a plain build,
    and reading a unit's status are all fine as this same unprivileged user) -
    these run IMMEDIATELY, in a background thread, no delay.
  - install/remove/restart genuinely need root (systemctl enable/disable/
    start/stop --now, and writing the unit file into /etc/systemd) - this
    process must never have that, so for these it only ever drops a named,
    empty request file into REQUESTS_DIR (the action name IS the filename,
    from the fixed allowlist below, never shell-interpolated). A ROOT cron
    job (admin-cron-worker.sh, run once a minute) is the only thing that ever
    executes those three; this process never runs as root and never will.

Stdlib only, on purpose - one file, nothing to pip install on the server.
"""
import json
import os
import re
import subprocess
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs

LISTEN_ADDR = ("127.0.0.1", 8899)
REPO_DIR = "/home/redhair/git/OpenSWOS"
ADMIN_DIR = "/srv/swos-matchmaker/admin"
REQUESTS_DIR = os.path.join(ADMIN_DIR, "requests")
PROCESSED_DIR = os.path.join(ADMIN_DIR, "processed")
LATEST_LOG = os.path.join(PROCESSED_DIR, "latest.log")
# What the matchmaker keeps for us (--reports, --recordings): cleared from here once
# fetched (scripts/fetch_reports.py / fetch_recordings.py --clear, or the buttons).
REPORTS_DIR = "/srv/swos-matchmaker/reports"
RECORDINGS_DIR = "/srv/swos-matchmaker/recordings"
# The only names /clear will touch -- a report is <match id>/player<N>.zip, a recording
# <date>-<time>-<match id prefix>.SRP. Nothing else, and nothing outside those two dirs.
REPORT_NAME = re.compile(r"^([0-9a-f]{16})/(player[12]\.zip)$")
RECORDING_NAME = re.compile(r"^(\d{8}-\d{6}-[0-9a-f]{8}\.SRP)$")

# Runs immediately, right here, as this same unprivileged user - no root, no
# queue, no cron delay.
LOCAL_ACTIONS = {
    "pull": ["git", "pull", "--ff-only"],
    "build": ["scripts/run_matchmaker.sh", "--rebuild", "--build-only", "--with-playback"],
    "status": ["scripts/swos-matchmaker-service.sh", "status"],
    # The recordings player alone: built and copied into /srv/.../playback, which this
    # account owns -- no root, and no restart, so no match in progress is touched.
    "player": ["scripts/update_playback.sh"],
}
# Needs real root - queued for admin-cron-worker.sh instead. That script has
# its own copy of this same set; both must agree, or a queued file just sits
# there unprocessed (the worker only recognizes what's in its own case
# statement).
QUEUED_ACTIONS = {"install", "restart", "remove"}
# Run here too, as Python rather than a command: emptying the two folders the
# matchmaker writes for us. Same user, no root.
CLEAR_ACTIONS = {"clear-reports", "clear-recordings"}

ACTIONS = [
    ("pull", "git pull"),
    ("build", "Rebuild (server + recordings player)"),
    ("player", "Update recordings player"),
    ("status", "Service: status"),
    ("install", "Service: install"),
    ("restart", "Service: restart"),
    ("remove", "Service: remove"),
    ("clear-reports", "Clear all reports"),
    ("clear-recordings", "Clear all recordings"),
]
ACTION_NAMES = {name for name, _ in ACTIONS}
CONFIRM_ACTIONS = {"remove", "restart", "install", "clear-reports", "clear-recordings"}
assert ACTION_NAMES == set(LOCAL_ACTIONS) | QUEUED_ACTIONS | CLEAR_ACTIONS

# Guards against two local actions (e.g. two clicks on "build") racing in the
# same build/ directory - admin-cron-worker.sh has its own, separate flock for
# the same reason on the queued side.
local_run_lock = threading.Lock()


def render_index():
    buttons = []
    for name, label in ACTIONS:
        confirm_attr = f' data-confirm="{label}?"' if name in CONFIRM_ACTIONS else ""
        buttons.append(f'<button data-action="{name}"{confirm_attr}>{label}</button>')
    return f"""<!doctype html>
<html><head><meta charset="utf-8"><title>swos-matchmaker admin</title>
<style>
body{{background:#111;color:#ddd;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;margin:0;padding:24px 32px 48px;}}
h1{{font-size:18px;margin:0 0 16px;}}
button{{background:#222;color:#ddd;border:1px solid #444;border-radius:4px;padding:8px 14px;margin:0 8px 8px 0;cursor:pointer;font-size:13px;}}
button:hover{{background:#2a2a2a;}}
button:disabled{{opacity:.45;cursor:default;}}
#pending{{color:#8ecbff;font-size:12px;margin:8px 0;}}
pre{{background:#181818;border:1px solid #333;border-radius:4px;padding:12px;font-size:12px;line-height:1.5;white-space:pre-wrap;word-break:break-all;max-height:70vh;overflow:auto;}}
</style></head>
<body>
<h1>swos-matchmaker admin</h1>
<div>{"".join(buttons)}</div>
<div id="pending"></div>
<pre id="log">(loading...)</pre>
<script>
var allButtons = document.querySelectorAll('button[data-action]');
allButtons.forEach(function(btn){{
  btn.addEventListener('click', function(){{
    // Was a separate inline onclick="return confirm(...)" before - which did
    // NOT stop this addEventListener handler from firing regardless of
    // Cancel/OK (confirmed live: two "remove" requests got queued from two
    // cancelled dialogs). The confirm check has to live in THIS handler,
    // the one that actually sends the request, not a second, independent one.
    if(btn.dataset.confirm && !confirm(btn.dataset.confirm)) return;
    // Immediate, unmissable feedback that the click registered - disabling
    // every button (not just this one, greyed out via CSS, no label/size
    // change) also stops a second click starting a different action while
    // this one's still only queued/starting. Every bit of actual status text
    // goes into the log pane, nowhere else.
    allButtons.forEach(function(b){{ b.disabled = true; }});
    document.getElementById('log').textContent = 'starting: '+btn.dataset.action+' ... please wait';
    fetch('action', {{method:'POST', headers:{{'Content-Type':'application/x-www-form-urlencoded'}}, body:'action='+encodeURIComponent(btn.dataset.action)}})
      .then(function(r){{ return r.text(); }})
      // The real result replaces this on the next poll() tick (already running
      // every 2s below) once the action actually finishes - this is just so a
      // click shows SOMETHING immediately instead of the still-stale old log.
      .then(function(t){{ document.getElementById('log').textContent = t; }})
      .catch(function(e){{ document.getElementById('log').textContent = 'error: '+e; }})
      .finally(function(){{ allButtons.forEach(function(b){{ b.disabled = false; }}); }});
  }});
}});
// Only touch the pane when its text changes: rewriting it every poll (2 s) wiped any
// selection in progress, which is what made the output nearly impossible to copy.
function setLog(text){{
  var logEl = document.getElementById('log');
  if (logEl.textContent !== text) logEl.textContent = text;
}}
function poll(){{
  fetch('status').then(function(r){{return r.json();}}).then(function(s){{
    document.getElementById('pending').textContent = s.pending>0 ? ('pending: '+s.pending+' ('+s.pendingNames+')') : '';
    var logEl = document.getElementById('log');
    if(s.running || s.pending>0){{
      // Something is still in flight (local action running, or a queued one
      // not yet picked up by the cron) - s.log is still the PREVIOUS action's
      // result at this point, so leave whatever's already shown (our own
      // "starting:.../started:..." message from the click, or this) alone
      // rather than flashing that stale old entry back up.
      if(logEl.textContent==='(loading...)') logEl.textContent = '(an action is currently running, please wait...)';
    }} else {{
      setLog(s.log);
    }}
  }}).catch(function(e){{ document.getElementById('log').textContent = 'poll error: '+e; }});
}}
poll();
setInterval(poll, 2000);
</script>
</body></html>"""


def write_log(action, text):
    os.makedirs(PROCESSED_DIR, exist_ok=True)
    fname = os.path.join(PROCESSED_DIR, f"{time.time_ns()}-{action}.log")
    with open(fname, "w", encoding="utf-8") as f:
        f.write(text)
    with open(LATEST_LOG, "w", encoding="utf-8") as f:
        f.write(text)


def delete_file(path, size=None):
    """Remove one file, only if it is still that size (None: any). True if removed."""
    try:
        if size is not None and os.path.getsize(path) != size:
            return False
        os.remove(path)
        return True
    except OSError:
        return False


def prune_empty_report_dirs():
    for d in os.listdir(REPORTS_DIR) if os.path.isdir(REPORTS_DIR) else []:
        full = os.path.join(REPORTS_DIR, d)
        if re.fullmatch(r"[0-9a-f]{16}", d) and os.path.isdir(full):
            try:
                os.rmdir(full)          # only succeeds when empty
            except OSError:
                pass


def clear_entries(entries):
    """entries: 'path:size' strings from a fetch script. Deletes each listed file that
    still has exactly that size -- anything uploaded, or grown, since the fetch stays."""
    removed, kept, refused = [], [], []
    for entry in entries:
        name, _, size = entry.strip().rpartition(":")
        if not name or not size.isdigit():
            refused.append(entry)
            continue
        m = REPORT_NAME.match(name)
        if m:
            path = os.path.join(REPORTS_DIR, m.group(1), m.group(2))
        elif RECORDING_NAME.match(name):
            path = os.path.join(RECORDINGS_DIR, name)
        else:
            refused.append(entry)
            continue
        (removed if delete_file(path, int(size)) else kept).append(name)
    prune_empty_report_dirs()
    if any(RECORDING_NAME.match(n) for n in removed):
        refresh_recordings_index()      # the cleared recordings' cards go with them
    return removed, kept, refused


def refresh_recordings_index():
    """The playback page's list (scripts/recordings_index.py): cards whose .SRP was
    cleared go, and index.json is written again."""
    try:
        subprocess.run(["python3", "scripts/recordings_index.py", "--recordings", RECORDINGS_DIR,
                        "--ledger", "/srv/swos-matchmaker/matches.jsonl"],
                       cwd=REPO_DIR, capture_output=True, timeout=120)
    except Exception:
        pass


def clear_all(which):
    """The buttons: every report, or every recording, whatever its size."""
    removed = 0
    if which == "clear-reports":
        for d in os.listdir(REPORTS_DIR) if os.path.isdir(REPORTS_DIR) else []:
            for z in ("player1.zip", "player2.zip"):
                removed += delete_file(os.path.join(REPORTS_DIR, d, z)) if re.fullmatch(r"[0-9a-f]{16}", d) else 0
        prune_empty_report_dirs()
    else:
        for f in os.listdir(RECORDINGS_DIR) if os.path.isdir(RECORDINGS_DIR) else []:
            if RECORDING_NAME.match(f):
                removed += delete_file(os.path.join(RECORDINGS_DIR, f))
        refresh_recordings_index()      # their cards go with them
    return removed


def run_local_action(action):
    ts = time.strftime("%Y-%m-%d %H:%M:%S")
    if action in CLEAR_ACTIONS:
        n = clear_all(action)
        write_log(action, f"=== {ts}  action={action} ===\nremoved {n} file(s)\n--- OK ---\n")
        return
    header = f"=== {ts}  action={action} ===\n"
    # LC_ALL=C forces git (and anything else that gettext-translates its own
    # output) to print its untranslated, English messages regardless of the
    # server's own locale - confirmed needed: git pull printed "Bereits
    # aktuell." here otherwise.
    env = dict(os.environ, LC_ALL="C", LANGUAGE="C")
    try:
        result = subprocess.run(
            LOCAL_ACTIONS[action], cwd=REPO_DIR, env=env,
            capture_output=True, text=True, timeout=1800,
        )
        out = result.stdout + result.stderr
        footer = "--- OK ---\n" if result.returncode == 0 else f"--- FAILED (exit {result.returncode}) ---\n"
    except Exception as e:
        out = str(e) + "\n"
        footer = "--- FAILED (exception) ---\n"
    write_log(action, header + out + footer)


def start_local_action(action):
    if not local_run_lock.acquire(blocking=False):
        return "busy: another local action is already running, try again shortly\n"

    def worker():
        try:
            run_local_action(action)
        finally:
            local_run_lock.release()

    threading.Thread(target=worker, daemon=True).start()
    return f"started: {action}\n"


class Handler(BaseHTTPRequestHandler):
    server_version = "matchmaker-admin/1.0"

    def log_message(self, fmt, *args):
        pass  # systemd journal already timestamps stdout/stderr; skip the default access log

    def do_GET(self):
        if self.path == "/":
            body = render_index().encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        elif self.path == "/status":
            self.handle_status()
        else:
            self.send_error(404)

    def do_POST(self):
        if self.path == "/clear":
            self.handle_clear()
            return
        if self.path != "/action":
            self.send_error(404)
            return
        length = int(self.headers.get("Content-Length", 0))
        raw = self.rfile.read(length).decode("utf-8", "replace")
        action = parse_qs(raw).get("action", [""])[0]
        if action not in ACTION_NAMES:
            self.send_error(400, "unknown action")
            return

        if action in LOCAL_ACTIONS or action in CLEAR_ACTIONS:
            msg = start_local_action(action)
        else:
            # Filename alone carries the request - contents are empty and
            # never read. Nanosecond timestamp keeps multiple queued requests
            # ordered and gives the worker a human-readable history.
            fname = f"{time.time_ns()}-{action}.req"
            os.makedirs(REQUESTS_DIR, exist_ok=True)
            with open(os.path.join(REQUESTS_DIR, fname), "wb"):
                pass
            msg = f"queued: {action} (root cron picks this up within a minute)\n"

        body = msg.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def handle_clear(self):
        """POST /clear, body 'entries=<path:size>,<path:size>,...' (form-encoded): what a
        fetch script has downloaded. Answers JSON: removed, kept (gone or changed), refused."""
        length = int(self.headers.get("Content-Length", 0))
        if length > (1 << 20):
            self.send_error(413)
            return
        raw = self.rfile.read(length).decode("utf-8", "replace")
        entries = [e for e in parse_qs(raw).get("entries", [""])[0].split(",") if e]
        removed, kept, refused = clear_entries(entries)
        ts = time.strftime("%Y-%m-%d %H:%M:%S")
        write_log("clear", f"=== {ts}  action=clear (from a fetch script) ===\n"
                           f"removed {len(removed)}, kept {len(kept)}, refused {len(refused)}\n--- OK ---\n")
        body = json.dumps({"removed": removed, "kept": kept, "refused": refused}).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def handle_status(self):
        pending = sorted(
            f for f in os.listdir(REQUESTS_DIR) if f.endswith(".req")
        ) if os.path.isdir(REQUESTS_DIR) else []
        try:
            with open(LATEST_LOG, "r", encoding="utf-8", errors="replace") as f:
                log_text = f.read()
        except FileNotFoundError:
            log_text = "(no runs yet)"
        body = json.dumps({
            "pending": len(pending),
            "pendingNames": ",".join(pending),
            "log": log_text,
            # True exactly while a local action's write_log() hasn't run yet
            # (release() only happens after it does - see start_local_action) -
            # lets the page tell "still running" apart from "already finished,
            # this really is the current log", instead of showing the PREVIOUS
            # action's now-stale log while a new one is still in flight.
            "running": local_run_lock.locked(),
        }).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


def main():
    os.makedirs(REQUESTS_DIR, exist_ok=True)
    os.makedirs(PROCESSED_DIR, exist_ok=True)
    server = ThreadingHTTPServer(LISTEN_ADDR, Handler)
    print(f"matchmaker-admin listening on {LISTEN_ADDR[0]}:{LISTEN_ADDR[1]} "
          f"(localhost only - put nginx + auth_basic in front)")
    server.serve_forever()


if __name__ == "__main__":
    main()
