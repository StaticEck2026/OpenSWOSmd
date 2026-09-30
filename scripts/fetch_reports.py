#!/usr/bin/env python3
"""Fetch netplay match reports from the matchmaking server, and list them.

The server keeps each online match's report as <reports>/<match id>/player1.zip and
player2.zip (docs/NETPLAY_REPORT.md): both players' SWOS.log and rating.txt, one zip
from each side. The public server's folder is published as an nginx index; this
mirrors it, fetching only what is new or has changed size, and prints one line a match:
when, who, the teams, the result each saw, their ratings and comments.

  scripts/fetch_reports.py                       # mirror + list, newest last
  scripts/fetch_reports.py --grep crom1          # only matches whose report mentions it
  scripts/fetch_reports.py --no-fetch            # list what is already here
  scripts/fetch_reports.py --url URL --out DIR   # another server, another folder
  scripts/fetch_reports.py --clear               # then delete from the server what we now hold

--clear asks the server's admin gateway (tools/matchmaker-admin, POST /cc/clear) to delete
exactly the files this run holds, at the size it holds them -- a report that arrived or
changed meanwhile stays. It needs the admin page's login in SWOS_ADMIN_USER and
SWOS_ADMIN_PASSWORD; --admin-url for another server.

Default folder: build/reports (gitignored). Each match is build/reports/<match id>/,
with the two zips and, beside them, their contents unpacked as player<N>/.
"""
import argparse
import base64
import json
import os
import re
import sys
import urllib.parse
import urllib.request
import zipfile

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
DEFAULT_URL = "https://us.swos.eu/reports/"
DEFAULT_OUT = os.path.join(REPO, "build", "reports")
DEFAULT_ADMIN = "https://us.swos.eu/cc/clear"


def listing(url):
    with urllib.request.urlopen(url, timeout=30) as r:
        text = r.read().decode("utf-8", "replace")
    # The size follows the </a> on the same line; pair each href with its line.
    out = []
    for line in text.splitlines():
        m = re.search(r'<a href="([^"]+)">[^<]*</a>\s*(.*)$', line)
        if not m:
            continue
        href, rest = m.group(1), m.group(2)
        if href.startswith("..") or href.startswith("/") or "://" in href:
            continue
        size = rest.split()[-1] if rest.split() else "-"
        out.append((href, int(size) if size.isdigit() else None))
    return out


def safe_name(name):
    """A folder or file name from the index, or None if it is not a plain one."""
    name = urllib.parse.unquote(name).rstrip("/")
    return name if re.fullmatch(r"[A-Za-z0-9._-]+", name) and name not in (".", "..") else None


def fetch(url, out_dir):
    """Mirror the index. Returns (new, held): how many zips came down, and every file now
    here at the size the server lists it, as 'match id/player<N>.zip:size'."""
    new, held = 0, []
    for href, _ in listing(url):
        if not href.endswith("/"):
            continue
        match = safe_name(href)
        if not match:
            continue
        mdir = os.path.join(out_dir, match)
        for fhref, size in listing(urllib.parse.urljoin(url, href)):
            name = safe_name(fhref)
            if not name or not name.endswith(".zip"):
                continue
            path = os.path.join(mdir, name)
            if os.path.exists(path) and (size is None or os.path.getsize(path) == size):
                if size is not None:
                    held.append("%s/%s:%d" % (match, name, size))
                continue
            os.makedirs(mdir, exist_ok=True)
            with urllib.request.urlopen(urllib.parse.urljoin(url, href + fhref), timeout=60) as r:
                data = r.read()
            with open(path + ".part", "wb") as f:
                f.write(data)
            os.replace(path + ".part", path)
            unpack(path)
            new += 1
            if size is not None and len(data) == size:
                held.append("%s/%s:%d" % (match, name, size))
    return new, held


def clear_on_server(entries, admin_url, tag):
    """Ask the admin gateway to delete these 'path:size' entries. The login comes from the
    environment and is never printed."""
    user, password = os.environ.get("SWOS_ADMIN_USER"), os.environ.get("SWOS_ADMIN_PASSWORD")
    if not entries:
        print("%s: nothing held, nothing to clear" % tag, file=sys.stderr)
        return
    if not user or not password:
        print("%s: --clear needs SWOS_ADMIN_USER and SWOS_ADMIN_PASSWORD (the admin page's login)"
              % tag, file=sys.stderr)
        return
    body = urllib.parse.urlencode({"entries": ",".join(entries)}).encode()
    req = urllib.request.Request(admin_url, data=body, method="POST")
    req.add_header("Content-Type", "application/x-www-form-urlencoded")
    req.add_header("Authorization", "Basic " + base64.b64encode(
        ("%s:%s" % (user, password)).encode()).decode())
    try:
        with urllib.request.urlopen(req, timeout=60) as r:
            got = json.loads(r.read().decode())
    except Exception as e:                      # the message never carries the login
        print("%s: clearing on the server failed: %s" % (tag, e), file=sys.stderr)
        return
    print("%s: cleared %d file(s) on the server; %d kept (changed or already gone), %d refused"
          % (tag, len(got.get("removed", [])), len(got.get("kept", [])), len(got.get("refused", []))),
          file=sys.stderr)


def unpack(zpath):
    """player1.zip -> player1/, member names checked: a zip is data from the internet."""
    dest = zpath[:-4]
    os.makedirs(dest, exist_ok=True)
    with zipfile.ZipFile(zpath) as z:
        for info in z.infolist():
            name = os.path.basename(info.filename)
            if not name or not re.fullmatch(r"[A-Za-z0-9._-]+", name):
                continue
            with open(os.path.join(dest, name), "wb") as f:
                f.write(z.read(info))


def field(text, key):
    m = re.search(r"^%s\s+(.*)$" % re.escape(key), text, re.M)
    return m.group(1).strip() if m else ""


def summary(mdir):
    """One line for a match, from whichever rating.txt is there (both say the same)."""
    texts = []
    for p in ("player1", "player2"):
        f = os.path.join(mdir, p, "rating.txt")
        if os.path.exists(f):
            texts.append(open(f, encoding="latin-1").read())
    if not texts:
        return None, ""
    t = max(texts, key=len)
    players = re.findall(r"^player (\d)\s+(.*?)(?: \(host\))?\r?$", t, re.M)
    blocks = re.split(r"^player \d\s+.*$", t, flags=re.M)[1:]
    sides = []
    for (num, name), block in zip(players, blocks):
        rating = field(block, "rating") or "sent nothing"
        comment = field(block, "comment")
        saw = field(block, "saw")
        side = "%s: %s" % (name, rating)
        if comment and comment != "-":
            side += ' "%s"' % comment
        if saw and saw != "-":
            side += " [%s]" % saw
        sides.append(side)
    when = field(t, "kick-off").split(" (")[0]
    line = "%s  %s  %s  %s  |  %s" % (when, os.path.basename(mdir), field(t, "teams"),
                                     field(t, "played"), "  |  ".join(sides))
    return when, line


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--url", default=DEFAULT_URL)
    ap.add_argument("--out", default=DEFAULT_OUT)
    ap.add_argument("--no-fetch", action="store_true")
    ap.add_argument("--grep", help="only matches whose report text contains this (any case)")
    ap.add_argument("--clear", action="store_true", help="then delete from the server what is now held here")
    ap.add_argument("--admin-url", default=DEFAULT_ADMIN)
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    if not args.no_fetch:
        url = args.url if args.url.endswith("/") else args.url + "/"
        n, held = fetch(url, args.out)
        print("fetch_reports: %d new zip(s) from %s -> %s" % (n, url, os.path.relpath(args.out, REPO)),
              file=sys.stderr)
        if args.clear:
            clear_on_server(held, args.admin_url, "fetch_reports")

    rows = []
    for match in sorted(os.listdir(args.out)):
        mdir = os.path.join(args.out, match)
        if not os.path.isdir(mdir):
            continue
        if args.grep:
            hay = ""
            for root, _, files in os.walk(mdir):
                for f in files:
                    if f.endswith((".txt", ".log")):
                        hay += open(os.path.join(root, f), encoding="latin-1").read()
            if args.grep.lower() not in hay.lower():
                continue
        when, line = summary(mdir)
        if line:
            rows.append((when, line))
    for _, line in sorted(rows):
        print(line)


if __name__ == "__main__":
    main()
