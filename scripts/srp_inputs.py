#!/usr/bin/env python3
"""What each player's game READ from its controls, against what the match APPLIED.

A live match's .SRP keeps both (docs/REPLAY_FORMAT_V2.md section 4): INPT is the input
every frame was played with, READ is what each player's game read on each frame, how
many milliseconds after its read for the frame before, and which wait (time sync,
prediction balance) it was taking. An input read on frame F is applied on frame F + the
input delay -- if the delay held. This checks that it did, for every frame of both
players, shows how evenly each game's frames were paced, and says whether the balance
was on and how many frames each kind of wait slowed.

  scripts/srp_inputs.py <file.SRP> [...]      # one summary a file
  scripts/srp_inputs.py --frames <file.SRP>   # and every frame that does not fit
  scripts/srp_inputs.py --csv <file.SRP>      # frame, then per player: read, ms, waits, applied

Exit 1 when a file has a frame whose applied input is not the one read `delay` earlier.
"""
import struct
import sys
import zlib

UNKNOWN = 0xFF
STRIDE = 3          # a player's bytes a frame: controls, ms, wait flags
BALANCE, TIMESYNC = 1, 2


def chunks(path):
    b = open(path, 'rb').read()
    if b[:4] != b'SWRP':
        raise ValueError('not a .SRP')
    out = {}
    for i in range(struct.unpack('>H', b[20:22])[0]):
        fourcc, off, stored, raw = struct.unpack('>4sIII', b[32 + 16 * i:48 + 16 * i])
        d = b[off:off + stored]
        out[fourcc.decode()] = zlib.decompress(d, -15) if stored != raw else d
    return struct.unpack('>I', b[16:20])[0], out


def names(ch):
    p = ch.get('PLYR')
    out = ['player 1', 'player 2']
    if p and len(p) >= 2:
        at = 2 if p[0] == 0 else 0      # tolerate a version word
        try:
            for i in range(2):
                n = p[at]
                if n:
                    out[i] = p[at + 1:at + 1 + n].decode('latin-1')
                at += 1 + n
        except IndexError:
            pass
    return out


def check(path, show_frames, csv):
    frames, ch = chunks(path)
    read, inpt = ch.get('READ'), ch.get('INPT')
    print(path)
    if not read:
        print('  no READ chunk: recorded by a build before it, or with prediction data off')
        return 0
    if not inpt:
        print('  no INPT chunk: not a live match')
        return 0
    version, delay, head, n = struct.unpack('>HBBI', read[:8])
    n = min(n, frames, (len(read) - 8) // (2 * STRIDE), len(inpt) // 4)
    who = names(ch)

    def at(f, p):
        o = 8 + (f * 2 + p) * STRIDE
        return read[o], read[o + 1], read[o + 2]

    def unknown(t):
        return t == (UNKNOWN, UNKNOWN, UNKNOWN)
    bad_files = 0
    if csv:
        print('frame,' + ','.join('%s read,%s ms,%s waits,%s applied' % (w, w, w, w) for w in who))
        for f in range(n):
            row = [str(f)]
            for p in range(2):
                t = at(f, p)
                waits = '' if unknown(t) else '+'.join(
                    x for x, bit in (('balance', BALANCE), ('timesync', TIMESYNC)) if t[2] & bit)
                row += ['' if unknown(t) else '%d' % t[0], '' if unknown(t) else '%d' % t[1], waits,
                        '%d' % inpt[f * 4 + p * 2]]
            print(','.join(row))
        return 0
    on = [bool(head & 1), bool(head & 2)]
    print('  %d frames, input delay %d; prediction balance %s'
          % (n, delay, 'ON for both' if all(on) else 'off' if not any(on) else
             'off (only %s had it on, and it takes both)' % who[0 if on[0] else 1]))
    for p in range(2):
        known = wrong = balance = timesync = 0
        ms = []
        misses = []
        for f in range(n):
            t = at(f, p)
            if unknown(t):
                continue
            r, dt = t[0], t[1]
            known += 1
            balance += bool(t[2] & BALANCE)
            timesync += bool(t[2] & TIMESYNC)
            if f > 0:
                ms.append(dt)
            if f + delay < n and inpt[(f + delay) * 4 + p * 2] != r:
                wrong += 1
                misses.append((f, r, inpt[(f + delay) * 4 + p * 2]))
        if not known:
            print('  %-12s nothing: this half never reached the file' % who[p])
            continue
        ms.sort()
        if ms:
            pace = ('frame times: median %d ms, 95%% within %d ms, longest %s ms; %d over 30 ms'
                    % (ms[len(ms) // 2], ms[int(len(ms) * 0.95)],
                       '255+' if ms[-1] == 255 else ms[-1], sum(1 for x in ms if x > 30)))
        else:
            pace = 'no frame times'
        print('  %-12s %d frames read; applied %d frames later on %s; %s'
              % (who[p], known, delay,
                 'every one' if not wrong else 'all but %d' % wrong, pace))
        print('  %-12s slowed by a time sync wait on %d frames, by a balance wait on %d'
              % ('', timesync, balance))
        if wrong:
            bad_files = 1
            for f, r, a in (misses if show_frames else misses[:5]):
                print('      frame %d read 0x%02X, frame %d applied 0x%02X' % (f, r, f + delay, a))
            if not show_frames and len(misses) > 5:
                print('      ... %d more (--frames lists them)' % (len(misses) - 5))
    return bad_files


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    if not args:
        print(__doc__)
        return 2
    rc = 0
    for path in args:
        rc |= check(path, '--frames' in sys.argv, '--csv' in sys.argv)
    return rc


if __name__ == '__main__':
    sys.exit(main())
