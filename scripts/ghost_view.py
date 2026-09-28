#!/usr/bin/env python3
"""Draw the ghost windows a match report carries: what a player saw against what happened.

Every online match keeps windows around each G-mark and the worst detected ghosts
(src/netplay_ghost.c) -- every frame as finally played and, where it differed, as that
player's screen drew it -- and the match report zips both players' as ghost_player1.csv
and ghost_player2.csv (docs/NETPLAY_REPORT.md). This draws each window around the ball:

    filled dot    where a player really was (the match as played)
    hollow ring   where this player's screen drew him, when that was somewhere else,
                  joined to the dot by a line
    red / blue    team 1 / team 2;  white: the ball

as a contact sheet (window<N>.png) and an animation (window<N>.gif), for each player.

  scripts/ghost_view.py build/reports/<match id>/player1.zip
  scripts/ghost_view.py ghost_player2.csv --out /tmp/ghosts
  scripts/ghost_view.py <zip> --step 2 --radius 70

Default output: build/ghost/<zip or csv name>/.
"""
import argparse
import csv
import io
import os
import re
import sys
import zipfile

from PIL import Image, ImageDraw

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
TEAM_COLOURS = [(230, 60, 50), (70, 120, 240)]
GRASS = (34, 110, 40)
SCALE = 3


def read_csv(text):
    """{window: {"why": str, "frames": {frame: {"final": row, "drawn": row}}}}, fps, player."""
    fps, player = 50, "?"
    m = re.search(r"player (\d+), (\d+) frames a second", text)
    if m:
        player, fps = m.group(1), int(m.group(2))
    lines = [l for l in text.splitlines() if l and not l.startswith("#")]
    windows = {}
    for row in csv.DictReader(io.StringIO("\n".join(lines))):
        w = windows.setdefault(int(row["window"]), {"why": row["why"], "frames": {}})
        w["frames"].setdefault(int(row["frame"]), {})[row["version"]] = row
    return windows, fps, player


def positions(row):
    pts = []
    for t in (1, 2):
        for p in range(1, 12):
            pts.append((t - 1, int(row["t%dp%d_x" % (t, p)]), int(row["t%dp%d_y" % (t, p)])))
    return pts


def frame_image(final, drawn, radius, label):
    size = 2 * radius * SCALE
    img = Image.new("RGB", (size, size + 16), GRASS)
    d = ImageDraw.Draw(img)
    cx, cy = int(final["ball_x"]), int(final["ball_y"])

    def to_px(x, y):
        return ((x - cx + radius) * SCALE, (y - cy + radius) * SCALE + 16)

    fin = positions(final)
    drw = positions(drawn) if drawn else fin
    for (team, x, y), (_, dx, dy) in zip(fin, drw):
        px, py = to_px(x, y)
        if (dx, dy) != (x, y):
            qx, qy = to_px(dx, dy)
            d.line([px, py, qx, qy], fill=TEAM_COLOURS[team], width=1)
            d.ellipse([qx - 7, qy - 7, qx + 7, qy + 7], outline=TEAM_COLOURS[team], width=2)
        d.ellipse([px - 5, py - 5, px + 5, py + 5], fill=TEAM_COLOURS[team])
    bx, by = to_px(int(final["ball_x"]), int(final["ball_y"]) - int(final["ball_z"]))
    d.ellipse([bx - 3, by - 3, bx + 3, by + 3], fill=(255, 255, 255))
    if drawn and (drawn["ball_x"], drawn["ball_y"], drawn["ball_z"]) != (final["ball_x"], final["ball_y"], final["ball_z"]):
        qx, qy = to_px(int(drawn["ball_x"]), int(drawn["ball_y"]) - int(drawn["ball_z"]))
        d.ellipse([qx - 4, qy - 4, qx + 4, qy + 4], outline=(255, 255, 255), width=1)
    d.rectangle([0, 0, size, 15], fill=(0, 0, 0))
    d.text((3, 2), label, fill=(255, 255, 0) if drawn else (220, 220, 220))
    return img


def offset(final, drawn):
    if not drawn:
        return 0
    return max(max(abs(a[1] - b[1]), abs(a[2] - b[2])) for a, b in zip(positions(final), positions(drawn)))


def render(text, out_dir, step, radius, cols):
    windows, fps, player = read_csv(text)
    os.makedirs(out_dir, exist_ok=True)
    for n, w in sorted(windows.items()):
        frames = sorted(w["frames"])
        if not frames:
            continue
        tiles, anim = [], []
        worst = max((offset(w["frames"][f]["final"], w["frames"][f].get("drawn")) for f in frames), default=0)
        wrong = sum(1 for f in frames if "drawn" in w["frames"][f])
        for i, f in enumerate(frames):
            v = w["frames"][f]
            if "final" not in v:
                continue
            s = f // fps
            label = "%d:%02d f%d%s" % (s // 60, s % 60, f, "  DRAWN %dpx off" % offset(v["final"], v.get("drawn"))
                                       if "drawn" in v else "")
            img = frame_image(v["final"], v.get("drawn"), radius, label)
            anim.append(img)
            if i % step == 0 or "drawn" in v and i % max(1, step // 2) == 0:
                tiles.append(img.resize((img.width // 2, img.height // 2)))
        tiles = tiles[:cols * 8]
        rows = (len(tiles) + cols - 1) // cols
        tw, th = tiles[0].size
        sheet = Image.new("RGB", (cols * tw, rows * th + 20), (0, 0, 0))
        ImageDraw.Draw(sheet).text(
            (4, 4), "player %s, window %d (%s): frames %d-%d, %d drawn differently, worst %d px"
            % (player, n, w["why"], frames[0], frames[-1], wrong, worst), fill=(255, 255, 255))
        for i, t in enumerate(tiles):
            sheet.paste(t, ((i % cols) * tw, 20 + (i // cols) * th))
        png = os.path.join(out_dir, "window%d.png" % n)
        sheet.save(png)
        gif = os.path.join(out_dir, "window%d.gif" % n)
        anim[0].save(gif, save_all=True, append_images=anim[1:], duration=int(1000 / fps) * 2, loop=0)
        print("ghost_view: player %s window %d (%s): %d frames, %d drawn differently, worst %d px -> %s, %s"
              % (player, n, w["why"], len(frames), wrong, worst, os.path.relpath(png, REPO),
                 os.path.basename(gif)))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("source", help="a match report zip, or a ghost_player<N>.csv")
    ap.add_argument("--out", help="output folder (default build/ghost/<source name>)")
    ap.add_argument("--step", type=int, default=6, help="frames between contact-sheet tiles")
    ap.add_argument("--radius", type=int, default=60, help="pitch pixels shown around the ball")
    ap.add_argument("--cols", type=int, default=8)
    args = ap.parse_args()

    base = os.path.splitext(os.path.basename(args.source))[0]
    parent = os.path.basename(os.path.dirname(os.path.abspath(args.source)))
    out = args.out or os.path.join(REPO, "build", "ghost", parent + "-" + base)
    sources = []
    if args.source.lower().endswith(".zip"):
        with zipfile.ZipFile(args.source) as z:
            for name in z.namelist():
                m = re.fullmatch(r"ghost_player([12])\.csv", os.path.basename(name))
                if m:
                    sources.append(("player" + m.group(1), z.read(name).decode("latin-1")))
    else:
        sources.append(("", open(args.source, encoding="latin-1").read()))
    if not sources:
        sys.exit("ghost_view: %s has no ghost windows (no G-mark and nothing detected in that match)"
                 % args.source)
    for sub, text in sources:
        render(text, os.path.join(out, sub) if sub else out, args.step, args.radius, args.cols)


if __name__ == "__main__":
    main()
