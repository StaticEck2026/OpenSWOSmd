#!/usr/bin/env python3
"""Fetch recorded netplay matches from the matchmaking server, and list them.

The server's recorder (--record, docs/NETPLAY_SETUP.md section 12) keeps each match a
player streamed to it as <recordings>/<date>-<time>-<match id, first 8>.SRP -- the whole
match, playable with the highlights viewer and resimulable frame by frame
(bin/SWOS.x86_64 --srp-resim=<file>). A player streams it only with RECORD MATCHES on
(MULTIPLAYER SETTINGS, [netplay] record = server).

This mirrors the folder's nginx index, fetching only what is new or has grown (the
recorder rewrites a match's file as it goes), and prints one line a recording, beside
the match report for the same match when scripts/fetch_reports.py has fetched it.

  scripts/fetch_recordings.py                  # mirror, delete what we now hold from the server, list
  scripts/fetch_recordings.py --keep           # ... and leave it on the server
  scripts/fetch_recordings.py --match 4d33222c # only that match (a prefix of its id)
  scripts/fetch_recordings.py --no-fetch       # list what is already here
  scripts/fetch_recordings.py --url URL --out DIR

Like fetch_reports.py: every request carries SWOS_ADMIN_USER / SWOS_ADMIN_PASSWORD (the
recordings are behind the admin page's login), and what was fetched is deleted from the
server unless --keep -- a recording still growing (a match in progress) stays.

Default folder: build/recordings (gitignored).
"""
import argparse
import os
import re
import struct
import sys
import urllib.parse
import urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fetch_reports  # noqa: E402  -- the index parser and the report summary

REPO = fetch_reports.REPO
DEFAULT_URL = "https://online.openswos.net/recordings/"
DEFAULT_OUT = os.path.join(REPO, "build", "recordings")
DEFAULT_REPORTS = fetch_reports.DEFAULT_OUT
NAME = re.compile(r"^(\d{8})-(\d{6})-([0-9a-f]{8})\.SRP$", re.I)


def is_index(url):
    """The server publishes the folder as an nginx index, or it does not (yet)."""
    with urllib.request.urlopen(url, timeout=30) as r:
        head = r.read(4096).decode("utf-8", "replace")
    return "Index of" in head or "Directory listing for" in head   # nginx, or python -m http.server


def fetch(url, out_dir):
    """Returns (new, held): recordings that came down or grew, and every file now here at
    the size the server lists it, as 'name:size' -- what may be deleted there after the fetch."""
    new, held = 0, []
    for href, size in fetch_reports.listing(url):
        name = fetch_reports.safe_name(href)
        if not name or not NAME.match(name):
            continue
        path = os.path.join(out_dir, name)
        if os.path.exists(path) and (size is None or os.path.getsize(path) == size):
            if size is not None:
                held.append("%s:%d" % (name, size))
            continue
        with urllib.request.urlopen(urllib.parse.urljoin(url, href), timeout=120) as r:
            data = r.read()
        with open(path + ".part", "wb") as f:
            f.write(data)
        os.replace(path + ".part", path)
        new += 1
        if size is not None and len(data) == size:
            held.append("%s:%d" % (name, size))
    return new, held


def frames(path):
    """The frame count from the .SRP header (docs/REPLAY_FORMAT_V2.md), or None."""
    try:
        with open(path, "rb") as f:
            head = f.read(32)
        return struct.unpack(">I", head[16:20])[0] if len(head) >= 20 else None
    except OSError:
        return None


def report_for(reports_dir, prefix):
    """The summary line of the fetched match report whose id starts with `prefix`."""
    if not os.path.isdir(reports_dir):
        return None
    for match in sorted(os.listdir(reports_dir)):
        if match.lower().startswith(prefix.lower()):
            _, line = fetch_reports.summary(os.path.join(reports_dir, match))
            return line or "report %s (no rating.txt)" % match
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--url", default=DEFAULT_URL)
    ap.add_argument("--out", default=DEFAULT_OUT)
    ap.add_argument("--reports", default=DEFAULT_REPORTS, help="where fetch_reports.py keeps reports")
    ap.add_argument("--no-fetch", action="store_true")
    ap.add_argument("--match", help="only recordings whose match id starts with this")
    ap.add_argument("--keep", action="store_true", help="leave what was fetched on the server")
    ap.add_argument("--clear", action="store_true", help=argparse.SUPPRESS)   # the default now
    ap.add_argument("--admin-url", default=fetch_reports.DEFAULT_ADMIN)
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    if not args.no_fetch:
        url = args.url if args.url.endswith("/") else args.url + "/"
        fetch_reports.install_login("fetch_recordings", url)
        if not is_index(url):
            print("fetch_recordings: %s is not a file listing -- the server does not publish its "
                  "recordings there (yet)" % url, file=sys.stderr)
        else:
            n, held = fetch(url, args.out)
            print("fetch_recordings: %d new or grown recording(s) from %s -> %s"
                  % (n, url, os.path.relpath(args.out, REPO)), file=sys.stderr)
            if not args.keep and urllib.parse.urlsplit(url).hostname == fetch_reports.LOGIN_HOST:
                fetch_reports.clear_on_server(held, args.admin_url, "fetch_recordings")

    for name in sorted(os.listdir(args.out)):
        m = NAME.match(name)
        if not m:
            continue
        day, clock, prefix = m.groups()
        if args.match and not prefix.lower().startswith(args.match.lower()[:8]):
            continue
        path = os.path.join(args.out, name)
        n = frames(path)
        print("%s-%s-%s %s:%s  %s  %s frames, %d KB" % (
            day[:4], day[4:6], day[6:], clock[:2], clock[2:4], prefix,
            n if n is not None else "?", os.path.getsize(path) // 1024))
        rep = report_for(args.reports, prefix)
        print("    %s" % (rep if rep else "no report fetched for this match"))
        print("    watch:   bin/SWOS.x86_64 --view-replay=%s" % path)
        print("    re-play: bin/SWOS.x86_64 --srp-resim=%s   (frame by frame, names any frame that parts)" % path)


if __name__ == "__main__":
    main()
