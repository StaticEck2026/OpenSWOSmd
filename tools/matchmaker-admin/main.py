#!/usr/bin/env python3
"""matchmaker-admin: a tiny, deliberately powerless web trigger for a handful of
matchmaker maintenance actions (git pull, rebuild, systemd service control).

This process runs as an UNPRIVILEGED user and binds 127.0.0.1 only - nginx
reverse-proxies it from outside with auth_basic (see README.md), so it's never
reachable without a password and never reachable except through TLS.

It cannot run git, cmake, or systemctl itself. All it can do is drop a named,
empty request file into REQUESTS_DIR - the action name IS the filename, taken
from a fixed allowlist below, never shell-interpolated. A ROOT cron job
(admin-cron-worker.sh, run once a minute) is the only thing that actually
executes anything; this process never runs as root and never will.

Stdlib only, on purpose - one file, nothing to pip install on the server.
"""
import json
import os
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs

LISTEN_ADDR = ("127.0.0.1", 8899)
ADMIN_DIR = "/srv/swos-matchmaker/admin"
REQUESTS_DIR = os.path.join(ADMIN_DIR, "requests")
PROCESSED_DIR = os.path.join(ADMIN_DIR, "processed")
LATEST_LOG = os.path.join(PROCESSED_DIR, "latest.log")

# action name -> button label. admin-cron-worker.sh has its own copy of this
# same list; both must agree, or a queued file just sits there unprocessed
# (the worker only recognizes what's in its own case statement).
ACTIONS = [
    ("pull", "git pull"),
    ("build", "Rebuild (--rebuild --build-only)"),
    ("install", "Service: install"),
    ("restart", "Service: restart"),
    ("status", "Service: status"),
    ("remove", "Service: remove"),
]
ACTION_NAMES = {name for name, _ in ACTIONS}
CONFIRM_ACTIONS = {"remove", "restart"}


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
document.querySelectorAll('button[data-action]').forEach(function(btn){{
  btn.addEventListener('click', function(){{
    fetch('action', {{method:'POST', headers:{{'Content-Type':'application/x-www-form-urlencoded'}}, body:'action='+encodeURIComponent(btn.dataset.action)}})
      .then(function(){{ poll(); }})
      .catch(function(e){{ document.getElementById('log').textContent = 'error: '+e; }});
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
        # Filename alone carries the request - contents are empty and never
        # read. Nanosecond timestamp keeps multiple queued requests ordered
        # and gives the worker a human-readable history.
        fname = f"{time.time_ns()}-{action}.req"
        os.makedirs(REQUESTS_DIR, exist_ok=True)
        with open(os.path.join(REQUESTS_DIR, fname), "wb"):
            pass
        body = f"queued: {action}\n".encode("utf-8")
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
