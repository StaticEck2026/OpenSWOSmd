#!/usr/bin/env python3
"""Serve the recordings player locally, laid out as the server has it (docs/PLAYBACK.md).

    python3 scripts/serve_playback.py                 # http://127.0.0.1:8080/playback/
    python3 scripts/serve_playback.py --port 9000 --recordings some/folder

  /playback/     dist/playback -- scripts/build_playback.sh (make WASM=1 PLAYER=1)
  /recordings/   build/recordings by default -- what scripts/fetch_recordings.py
                 downloaded -- listed the way nginx's autoindex lists it, which is
                 what the page reads.

Stdlib only, so the Windows Python that scripts/serve_playback.bat finds will do.
A test server for one person: it binds 127.0.0.1 unless told otherwise.
"""
import argparse
import html
import http.server
import os
import sys
import time
import urllib.parse

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

TYPES = {
    ".html": "text/html; charset=utf-8",
    ".js": "text/javascript",
    ".wasm": "application/wasm",   # the browser streams-compiles only with this
    ".data": "application/octet-stream",
    ".SRP": "application/octet-stream",
}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--bind", default="127.0.0.1")
    ap.add_argument("--player", default=os.path.join(ROOT, "dist", "playback"))
    ap.add_argument("--recordings", default=os.path.join(ROOT, "build", "recordings"))
    args = ap.parse_args()
    # A relative folder is looked for where you are, then in the repository -- the .bat
    # is usually run from somewhere else, and `build\recordings-pred` means the repo's.
    for name in ("player", "recordings"):
        path = getattr(args, name)
        if not os.path.isabs(path) and not os.path.isdir(path) and os.path.isdir(os.path.join(ROOT, path)):
            setattr(args, name, os.path.join(ROOT, path))

    if not os.path.isfile(os.path.join(args.player, "SWOS.wasm")):
        sys.exit("serve_playback: no player in %s -- build it first:\n"
                 "  scripts/build_playback.sh        (WSL, needs emsdk)" % args.player)
    if not os.path.isdir(args.recordings):
        sys.exit("serve_playback: no recordings folder %s -- fetch some first:\n"
                 "  scripts/fetch_recordings.py" % args.recordings)
    mounts = {"/playback/": os.path.abspath(args.player),
              "/recordings/": os.path.abspath(args.recordings)}

    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self, fmt, *a):
            sys.stderr.write("[serve] %s\n" % (fmt % a))

        def do_GET(self):
            self.handle_get(send_body=True)

        def do_HEAD(self):
            self.handle_get(send_body=False)

        def handle_get(self, send_body):
            path = urllib.parse.unquote(urllib.parse.urlsplit(self.path).path)
            if path in ("/", "/playback"):
                return self.redirect("/playback/")
            if path == "/recordings":
                return self.redirect("/recordings/")
            for prefix, folder in mounts.items():
                if path.startswith(prefix):
                    rel = path[len(prefix):]
                    break
            else:
                return self.send_error(404)
            # One file name, no folders: nothing outside the two mounts is reachable.
            if "/" in rel or "\\" in rel or rel.startswith("."):
                return self.send_error(404)
            if rel == "":
                if prefix == "/recordings/":
                    return self.listing(folder, send_body)
                rel = "index.html"
            full = os.path.join(folder, rel)
            if not os.path.isfile(full):
                return self.send_error(404)
            ext = os.path.splitext(rel)[1]
            ctype = TYPES.get(ext, TYPES.get(ext.lower(), "application/octet-stream"))
            size = os.path.getsize(full)
            self.send_response(200)
            self.send_header("Content-Type", ctype)
            self.send_header("Content-Length", str(size))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            if send_body:
                with open(full, "rb") as f:
                    while True:
                        chunk = f.read(1 << 20)
                        if not chunk:
                            break
                        self.wfile.write(chunk)

        def listing(self, folder, send_body):
            # nginx autoindex's shape: <a href="NAME">NAME</a>  date time  size
            rows = []
            for name in sorted(os.listdir(folder)):
                full = os.path.join(folder, name)
                if not os.path.isfile(full) or not name.upper().endswith(".SRP"):
                    continue
                st = os.stat(full)
                when = time.strftime("%d-%b-%Y %H:%M", time.gmtime(st.st_mtime))
                href = urllib.parse.quote(name)
                rows.append('<a href="%s">%s</a>  %s  %d' % (href, html.escape(name), when, st.st_size))
            body = ("<html><head><title>Index of /recordings/</title></head><body>"
                    "<h1>Index of /recordings/</h1><hr><pre>\n" + "\n".join(rows) +
                    "\n</pre><hr></body></html>\n").encode()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            if send_body:
                self.wfile.write(body)

        def redirect(self, where):
            self.send_response(302)
            self.send_header("Location", where)
            self.send_header("Content-Length", "0")
            self.end_headers()

    n = sum(1 for f in os.listdir(args.recordings) if f.upper().endswith(".SRP"))
    class Server(http.server.ThreadingHTTPServer):
        # On Windows SO_REUSEADDR lets a second server bind a port the first still holds,
        # and the browser then talks to whichever Windows picks -- an old one serving
        # another folder. Take the port exclusively there, so a busy port is an error.
        allow_reuse_address = os.name != "nt"

        def server_bind(self):
            import socket
            if os.name == "nt" and hasattr(socket, "SO_EXCLUSIVEADDRUSE"):
                self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
            super().server_bind()

    try:
        srv = Server((args.bind, args.port), Handler)
    except OSError as e:
        sys.exit("serve_playback: cannot listen on %s:%d (%s) -- is another one still running? "
                 "Close it, or pick another port with --port" % (args.bind, args.port, e.strerror or e))
    print("serve_playback: http://%s:%d/playback/  -- %d recording(s) from %s  (Ctrl+C to stop)"
          % ("127.0.0.1" if args.bind in ("0.0.0.0", "") else args.bind, args.port, n, args.recordings))
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
