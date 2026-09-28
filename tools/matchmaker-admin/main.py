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

# Runs immediately, right here, as this same unprivileged user - no root, no
# queue, no cron delay.
LOCAL_ACTIONS = {
    "pull": ["git", "pull", "--ff-only"],
    "build": ["scripts/run_matchmaker.sh", "--rebuild", "--build-only"],
    "status": ["scripts/swos-matchmaker-service.sh", "status"],
}
# Needs real root - queued for admin-cron-worker.sh instead. That script has
# its own copy of this same set; both must agree, or a queued file just sits
# there unprocessed (the worker only recognizes what's in its own case
# statement).
QUEUED_ACTIONS = {"install", "restart", "remove"}

ACTIONS = [
    ("pull", "git pull"),
    ("build", "Rebuild (--rebuild --build-only)"),
    ("status", "Service: status"),
    ("install", "Service: install"),
    ("restart", "Service: restart"),
    ("remove", "Service: remove"),
]
ACTION_NAMES = {name for name, _ in ACTIONS}
CONFIRM_ACTIONS = {"remove", "restart"}
assert ACTION_NAMES == set(LOCAL_ACTIONS) | QUEUED_ACTIONS

# Guards against two local actions (e.g. two clicks on "build") racing in the
# same build/ directory - admin-cron-worker.sh has its own, separate flock for
# the same reason on the queued side.
local_run_lock = threading.Lock()


def render_index():
    buttons = []
    for name, label in ACTIONS:
        confirm = f" onclick=\"return confirm('{label}?')\"" if name in CONFIRM_ACTIONS else ""
        buttons.append(f'<button data-action="{name}"{confirm}>{label}</button>')
    return f"""<!doctype html>
<html><head><meta charset="utf-8"><title>swos-matchmaker admin</title>
<style>
body{{background:#111;color:#ddd;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;margin:0;padding:24px 32px 48px;}}
h1{{font-size:18px;margin:0 0 16px;}}
button{{background:#222;color:#ddd;border:1px solid #444;border-radius:4px;padding:8px 14px;margin:0 8px 8px 0;cursor:pointer;font-size:13px;}}
button:hover{{background:#2a2a2a;}}
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
  var label = btn.textContent;
  btn.addEventListener('click', function(){{
    // Immediate, unmissable feedback that the click registered - disabling
    // every button (not just this one) also stops a second click starting a
    // different action while this one's still only queued/starting.
    allButtons.forEach(function(b){{ b.disabled = true; }});
    btn.textContent = label+' (sent...)';
    document.getElementById('log').textContent = 'starting: '+btn.dataset.action+' ... please wait';
    fetch('action', {{method:'POST', headers:{{'Content-Type':'application/x-www-form-urlencoded'}}, body:'action='+encodeURIComponent(btn.dataset.action)}})
      .then(function(r){{ return r.text(); }})
      // The real result replaces this on the next poll() tick (already running
      // every 2s below) once the action actually finishes - this is just so a
      // click shows SOMETHING immediately instead of the still-stale old log.
      .then(function(t){{ document.getElementById('log').textContent = t; }})
      .catch(function(e){{ document.getElementById('log').textContent = 'error: '+e; }})
      .finally(function(){{ allButtons.forEach(function(b){{ b.disabled = false; }}); btn.textContent = label; }});
  }});
}});
function poll(){{
  fetch('status').then(function(r){{return r.json();}}).then(function(s){{
    document.getElementById('pending').textContent = s.pending>0 ? ('pending: '+s.pending+' ('+s.pendingNames+')') : '';
    document.getElementById('log').textContent = s.log;
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


def run_local_action(action):
    ts = time.strftime("%Y-%m-%d %H:%M:%S")
    header = f"=== {ts}  action={action} ===\n"
    try:
        result = subprocess.run(
            LOCAL_ACTIONS[action], cwd=REPO_DIR,
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
        if self.path != "/action":
            self.send_error(404)
            return
        length = int(self.headers.get("Content-Length", 0))
        raw = self.rfile.read(length).decode("utf-8", "replace")
        action = parse_qs(raw).get("action", [""])[0]
        if action not in ACTION_NAMES:
            self.send_error(400, "unknown action")
            return

        if action in LOCAL_ACTIONS:
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
