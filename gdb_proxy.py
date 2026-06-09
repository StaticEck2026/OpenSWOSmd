#!/usr/bin/env python3
"""
gdb_proxy.py — drives gdb for either the SWOS Amiga build (m68k via
FS-UAE remote) or the SWOS Linux SDL build (native x86 process).

Two modes:

  amiga (default) — Spawns m68k-amiga-elf-gdb against the Amiga ELF and
      attaches to FS-UAE's remote-debugger stub. FS-UAE must be running
      with --remote_debugger=20 --remote_debugger_port=<N> first.

  sdl             — Spawns /usr/bin/gdb against the SDL Linux binary.
      Does not attach anywhere; use `cmd "run"` (or `cmd "start"`) to
      launch the SDL process under gdb control.

Subcommands:
  daemon [--mode amiga|sdl] [--elf PATH] [--bin PATH]
         [--host HOST] [--port PORT]
      Start the proxy.

  cmd "<gdb command>" [--timeout S]
      Send one gdb command, print response (output up to next "(gdb) "
      prompt). Pre-flushes any pending async output so asynchronous
      halts (watchpoint hits, signals) aren't lost between commands.

  poll [SECONDS]
      Return any buffered output without sending a command. Useful
      after a `continue` to wait for the target to halt and capture
      the halt info.

  raw "<text>"
      Send raw bytes (no newline appended).

  stop / status / ping
      Lifecycle helpers.

Defaults:
  Amiga gdb : /mnt/c/Users/headk/.vscode/extensions/bartmanabyss.amiga-debug-1.8.2/bin/linux/opt/bin/m68k-amiga-elf-gdb
  Amiga ELF : /mnt/c/Projects/AI/SWOS/SWOS_C/uae/dh0/data/SWOS2.elf
  Amiga port: 2345
  SDL gdb   : /usr/bin/gdb
  SDL bin   : /mnt/c/cygwin64/home/headk/SWOS_C_SDL/bin/SWOS
  Socket    : /tmp/swos_gdb.sock
"""

import argparse
import os
import re
import socket
import subprocess
import sys
import threading
import time

GDB_BIN_AMIGA = "/mnt/c/Users/headk/.vscode/extensions/bartmanabyss.amiga-debug-1.8.2/bin/linux/opt/bin/m68k-amiga-elf-gdb"
GDB_BIN_SDL   = "/usr/bin/gdb"
DEFAULT_ELF   = "/mnt/c/Projects/AI/SWOS/SWOS_C/uae/dh0/data/SWOS2.elf"
DEFAULT_BIN   = "/mnt/c/cygwin64/home/headk/SWOS_C_SDL/bin/SWOS"
DEFAULT_PORT  = 2345
SOCK_PATH     = "/tmp/swos_gdb.sock"


def detect_windows_host():
    """On WSL2 the Windows host is whatever /etc/resolv.conf has as nameserver."""
    try:
        with open("/etc/resolv.conf") as f:
            for line in f:
                if line.startswith("nameserver"):
                    return line.split()[1]
    except IOError:
        pass
    return "localhost"


class GdbDaemon:
    def __init__(self, mode, gdb_bin, target_path, host, port, cwd=None):
        env = dict(os.environ)
        self.gdb = subprocess.Popen(
            [gdb_bin, "-q", "--nx", target_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            bufsize=0,
            cwd=cwd,
            env=env,
        )
        self.buf_lock = threading.Lock()
        self.buf = bytearray()
        self.reader = threading.Thread(target=self._reader_loop, daemon=True)
        self.reader.start()
        self.mode = mode
        self.attach_ok = True
        self.attach_detail = "sdl" if mode == "sdl" else "pending"
        # Wait for initial gdb prompt.
        self._wait_for_prompt(timeout=5.0)
        # Drop the banner; subsequent `cmd` calls return clean responses.
        self._drain()
        if mode == "amiga":
            # Robustness: give the FS-UAE stub generous packet timeouts so a
            # slow handshake right after the SWOS2 entry trigger doesn't
            # half-fail into a degraded ("not being run") connection. These
            # are best-effort — ignore prompts that don't come.
            for opt in ("set tcp connect-timeout 20",
                        "set remotetimeout 20"):
                self._send_raw(opt + "\n")
                self._wait_for_prompt(timeout=3.0)
                self._drain()
            # Attach to FS-UAE's stub.
            self._send_raw(f"target remote {host}:{port}\n")
            self._wait_for_prompt(timeout=15.0)
            self._drain()
            # Self-verify so a degraded attach is VISIBLE (logged) rather than
            # silently accepted. The one-shot port means recovery is a full
            # FS-UAE restart — see amiga_gdb.sh, which reads this verdict.
            self.attach_ok, self.attach_detail = self._target_health()
            print(f"[daemon] attach health: "
                  f"{'OK' if self.attach_ok else 'DEGRADED'} "
                  f"({self.attach_detail})", file=sys.stderr)
        # In SDL mode we just leave gdb at its prompt; the caller issues
        # `run`/`start` to launch the inferior.

    def _target_health(self):
        """Probe whether the target is cleanly stopped (mode-generic — works
        for the Amiga FS-UAE remote and the sdl native inferior alike).
        Returns
        (ok, detail). Healthy == target reports a stop (SIGTRAP) AND a
        readable non-zero PC. Degraded == 'not being run' / no registers /
        a hung connection (commands time out)."""
        self._drain()  # clear any pending async/banner output
        self._send_raw("info program\n")
        got = self._wait_for_prompt(timeout=6.0)
        ip = self._drain().decode("utf-8", errors="replace")
        if not got:
            return False, "info program hung (dead/booting connection)"
        self._send_raw("print/x $pc\n")
        self._wait_for_prompt(timeout=5.0)
        pcout = self._drain().decode("utf-8", errors="replace")
        m = re.search(r"0x[0-9a-fA-F]+", pcout)
        pc = m.group(0) if m else None
        if "not being run" in ip or not ("stopped" in ip or "SIGTRAP" in ip):
            return False, "target not cleanly stopped"
        if pc is None or pc.lower() == "0x0":
            return False, "no readable PC"
        return True, f"stopped PC={pc}"

    # ---- I/O helpers --------------------------------------------------

    def _reader_loop(self):
        while True:
            chunk = self.gdb.stdout.read(4096)
            if not chunk:
                return
            with self.buf_lock:
                self.buf.extend(chunk)

    def _drain(self):
        with self.buf_lock:
            data = bytes(self.buf)
            self.buf.clear()
        return data

    def _wait_for_prompt(self, timeout):
        """Block until "(gdb) " appears in the buffer. Returns True on prompt,
        False on timeout."""
        end = time.time() + timeout
        while time.time() < end:
            with self.buf_lock:
                if b"(gdb) " in self.buf or b"(gdb)\n" in self.buf:
                    return True
            time.sleep(0.05)
        return False

    def _send_raw(self, payload):
        if isinstance(payload, str):
            payload = payload.encode()
        self.gdb.stdin.write(payload)
        self.gdb.stdin.flush()

    # ---- Public API ---------------------------------------------------

    def cmd(self, command, timeout=10.0):
        # Capture any async output (e.g., halt notifications) that arrived
        # since the last command, so we don't lose it.
        pre = self._drain().decode("utf-8", errors="replace")
        self._send_raw(command.rstrip("\n") + "\n")
        got = self._wait_for_prompt(timeout)
        post = self._drain().decode("utf-8", errors="replace")
        if not got:
            return f"[TIMEOUT after {timeout}s — partial output below]\n{pre}{post}"
        return pre + post

    def poll(self, timeout=0.5):
        """Wait up to `timeout` seconds of inactivity, then return whatever
        has been buffered. Extends the deadline by 300 ms whenever new bytes
        arrive — handy for catching halt notifications after `continue`."""
        end = time.time() + timeout
        last_size = -1
        while time.time() < end:
            with self.buf_lock:
                cur = len(self.buf)
            if cur != last_size:
                last_size = cur
                end = max(end, time.time() + 0.3)
            time.sleep(0.05)
        return self._drain().decode("utf-8", errors="replace")

    def wait_stop(self, timeout=300.0):
        """Block until the inferior STOPS — a breakpoint, watchpoint, temporary
        breakpoint, or signal shows up in gdb's output — then return everything
        captured. Purpose-built for "continue, then wait for the breakpoint to
        fire": returns the instant a stop signature appears, rather than waiting
        for inactivity (poll) or polling `info program` (which HANGS while an
        all-stop target is running — the reason `watch` never surfaced the hit).
        A SIGINT to the gdb process also stops the target ("received signal"),
        so this returns then too. On timeout returns whatever was captured plus
        a [WAITSTOP timeout] marker. Server-serial: blocks the daemon for its
        duration (intended — you're waiting on one breakpoint)."""
        stop_re = re.compile(
            r"Breakpoint \d+,|Temporary breakpoint \d+,|"
            r"received signal|Watchpoint \d+|hit Hardware watchpoint")
        end = time.time() + timeout
        captured = ""
        while time.time() < end:
            chunk = self._drain().decode("utf-8", errors="replace")
            if chunk:
                captured += chunk
                if stop_re.search(captured):
                    # Let the trailing frame line + "(gdb) " prompt arrive so a
                    # follow-up `cmd` doesn't race a half-written prompt.
                    time.sleep(0.25)
                    captured += self._drain().decode("utf-8", errors="replace")
                    return captured
                continue
            time.sleep(0.1)
        return captured + f"\n[WAITSTOP timeout after {timeout:.0f}s — target still running]"

    def raw(self, payload):
        self._send_raw(payload)
        return "[sent]"

    def stop(self):
        try:
            self._send_raw("quit\n")
        except Exception:
            pass
        try:
            self.gdb.wait(timeout=2.0)
        except Exception:
            self.gdb.kill()


def daemon_main(args):
    if os.path.exists(SOCK_PATH):
        try:
            os.remove(SOCK_PATH)
        except OSError:
            pass

    mode = args.mode
    if mode == "amiga":
        gdb_bin = GDB_BIN_AMIGA
        target_path = args.elf
        host = args.host or detect_windows_host()
        cwd = None
        print(f"[daemon] mode=amiga gdb={gdb_bin}", file=sys.stderr)
        print(f"[daemon] elf={target_path}", file=sys.stderr)
        print(f"[daemon] remote={host}:{args.port}", file=sys.stderr)
        d = GdbDaemon(mode, gdb_bin, target_path, host, args.port)
    elif mode == "sdl":
        gdb_bin = GDB_BIN_SDL
        target_path = args.bin
        # Run gdb from the binary's directory so the SDL inferior finds
        # its data/grafs/sound subdirs by relative path.
        cwd = os.path.dirname(os.path.abspath(target_path))
        print(f"[daemon] mode=sdl gdb={gdb_bin}", file=sys.stderr)
        print(f"[daemon] bin={target_path}", file=sys.stderr)
        print(f"[daemon] cwd={cwd}", file=sys.stderr)
        d = GdbDaemon(mode, gdb_bin, target_path, None, None, cwd=cwd)
    else:
        print(f"unknown mode: {mode}", file=sys.stderr)
        sys.exit(2)

    srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    srv.bind(SOCK_PATH)
    srv.listen(4)
    os.chmod(SOCK_PATH, 0o666)
    print(f"[daemon] listening on {SOCK_PATH}", file=sys.stderr)

    try:
        while True:
            client, _ = srv.accept()
            try:
                # Read request until we see "\n\n" sentinel.
                data = b""
                while True:
                    chunk = client.recv(65536)
                    if not chunk:
                        break
                    data += chunk
                    if b"\n\n" in data:
                        break
                req = data.split(b"\n\n", 1)[0].decode("utf-8", errors="replace")

                if req.startswith("CMD "):
                    rest = req[4:]
                    # Optional "TIMEOUT=N\t..." prefix.
                    timeout = 10.0
                    if rest.startswith("TIMEOUT="):
                        ts, rest = rest.split("\t", 1)
                        try:
                            timeout = float(ts[len("TIMEOUT="):])
                        except ValueError:
                            pass
                    out = d.cmd(rest, timeout=timeout)
                elif req.startswith("POLL "):
                    try:
                        t = float(req[5:].strip())
                    except ValueError:
                        t = 0.5
                    out = d.poll(timeout=t)
                elif req.startswith("WAITSTOP "):
                    try:
                        t = float(req[len("WAITSTOP "):].strip())
                    except ValueError:
                        t = 300.0
                    out = d.wait_stop(timeout=t)
                elif req.startswith("RAW "):
                    out = d.raw(req[4:])
                elif req == "SHUTDOWN":
                    client.sendall(b"stopping\n")
                    client.close()
                    break
                elif req == "PING":
                    out = "pong"
                elif req == "HEALTH":
                    # _target_health is mode-generic (plain `info program` +
                    # `print/x $pc`), so it returns a real stopped/running
                    # verdict for the sdl native target too, not just amiga.
                    ok, detail = d._target_health()
                    out = ("OK " if ok else "DEGRADED ") + detail
                else:
                    out = f"[unknown request] {req!r}"

                client.sendall(out.encode("utf-8", errors="replace"))
            except Exception as e:
                try:
                    client.sendall(f"[daemon error] {e}\n".encode())
                except Exception:
                    pass
            finally:
                client.close()
    finally:
        d.stop()
        if os.path.exists(SOCK_PATH):
            try:
                os.remove(SOCK_PATH)
            except OSError:
                pass


def send_request(req, timeout=30.0):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(timeout)
    s.connect(SOCK_PATH)
    s.sendall((req + "\n\n").encode("utf-8"))
    s.shutdown(socket.SHUT_WR)
    out = b""
    while True:
        try:
            chunk = s.recv(65536)
        except socket.timeout:
            break
        if not chunk:
            break
        out += chunk
    s.close()
    return out.decode("utf-8", errors="replace")


def watch_loop(args):
    """Generic stop-notifier. Polls the daemon's HEALTH verdict and writes one
    line to stdout on each transition INTO a cleanly-stopped state (a
    breakpoint, panic, signal, or the initial entry pause). Mode-agnostic: it
    relies only on HEALTH, which now reports stopped/running for both the amiga
    FS-UAE remote and the sdl native inferior. Pipe it into any notifier so a
    human never has to hand-poll or announce 'I loaded it'. Runs until killed
    (or until the first stop with --once). HEALTH is non-draining, so the gdb
    output buffer stays intact for a follow-up `cmd`/`poll`."""
    prev = None
    while True:
        if not os.path.exists(SOCK_PATH):
            verdict = "DOWN (socket missing)"
        else:
            try:
                verdict = send_request("HEALTH", timeout=args.timeout).strip()
            except Exception as e:
                verdict = f"DEGRADED (no response: {e})"
        cur = "stopped" if verdict.startswith("OK stopped") else "other"
        if cur == "stopped" and prev != "stopped":
            loc = ""
            if not args.no_frame:
                try:
                    fr = send_request("CMD TIMEOUT=5\tframe 0", timeout=10)
                    fm = re.search(r"#0\s+(?:0x[0-9a-fA-F]+ in )?(\w+)", fr)
                    lm = re.search(r"\bat .*?([\w.]+:\d+)", fr)
                    bits = [m.group(1) for m in (fm, lm) if m]
                    if bits:
                        loc = " | " + " ".join(bits)
                except Exception:
                    pass
            sys.stdout.write(f"{args.label}{verdict}{loc}\n")
            sys.stdout.flush()
            if args.once:
                return
        prev = cur
        time.sleep(args.interval)


def main():
    ap = argparse.ArgumentParser(
        description="Proxy for m68k-amiga-elf-gdb against FS-UAE remote debugger.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    # NOTE: the subcommand dest is "cmd_name" (NOT "mode") because the
    # `daemon` subcommand has its own `--mode amiga|sdl` option. Using
    # dest="mode" here collided with that, so argparse overwrote the
    # chosen subcommand ("daemon") with the --mode value ("amiga"), and
    # daemon_main() was never dispatched — the daemon exited instantly.
    sub = ap.add_subparsers(dest="cmd_name", required=True)

    d = sub.add_parser("daemon", help="Start the proxy (foreground).")
    d.add_argument("--mode", choices=("amiga", "sdl"), default="amiga",
                   help="amiga: m68k gdb + FS-UAE remote (default); "
                        "sdl: native gdb against the Linux x86 SWOS binary.")
    d.add_argument("--elf", default=DEFAULT_ELF, help="Amiga ELF (mode=amiga)")
    d.add_argument("--bin", default=DEFAULT_BIN, help="SDL binary (mode=sdl)")
    d.add_argument("--host", default=None,
                   help="FS-UAE host IP (auto-detect on WSL2)")
    d.add_argument("--port", type=int, default=DEFAULT_PORT)

    c = sub.add_parser("cmd", help="Send a gdb command, print response.")
    c.add_argument("command", help="Full gdb command string")
    c.add_argument("--timeout", type=float, default=10.0)

    p = sub.add_parser("poll", help="Return buffered output without sending a command.")
    p.add_argument("seconds", nargs="?", default="0.5")

    ws = sub.add_parser("waitstop",
                        help="Block until the target STOPS (breakpoint / watchpoint / "
                             "signal), then print the stop info. Use right after "
                             "`cmd continue` to be alerted the moment a breakpoint "
                             "fires — no polling, no info-program hang.")
    ws.add_argument("seconds", nargs="?", default="300")

    r = sub.add_parser("raw", help="Send raw bytes to gdb stdin (no newline added).")
    r.add_argument("data", help="Raw payload (use $'\\x03' from bash to send Ctrl-C)")

    sub.add_parser("stop", help="Shut the daemon down.")
    sub.add_parser("status", help="Check whether the daemon is reachable.")
    sub.add_parser("ping", help="Round-trip test.")
    sub.add_parser("health", help="Attach health verdict (OK/DEGRADED).")

    w = sub.add_parser("watch", help="Emit a line on each running->stopped "
                       "transition (breakpoint / panic / signal / entry pause). "
                       "Generic across amiga & sdl. Pipe into any notifier; "
                       "Ctrl-C or kill to stop.")
    w.add_argument("--interval", type=float, default=3.0,
                   help="Poll interval in seconds (default 3.0)")
    w.add_argument("--timeout", type=float, default=20.0,
                   help="Per-HEALTH-probe timeout in seconds (default 20.0)")
    w.add_argument("--label", default="STOP ",
                   help="Prefix for each emitted line (default 'STOP ')")
    w.add_argument("--once", action="store_true",
                   help="Exit after the first stop is reported")
    w.add_argument("--no-frame", action="store_true",
                   help="Skip the frame-0 location lookup on each stop")

    args = ap.parse_args()

    if args.cmd_name == "daemon":
        daemon_main(args)
    elif args.cmd_name == "cmd":
        req = f"CMD TIMEOUT={args.timeout}\t{args.command}"
        sys.stdout.write(send_request(req, timeout=args.timeout + 5))
        sys.stdout.flush()
    elif args.cmd_name == "poll":
        sys.stdout.write(send_request(f"POLL {args.seconds}",
                                      timeout=float(args.seconds) + 5))
        sys.stdout.flush()
    elif args.cmd_name == "waitstop":
        sys.stdout.write(send_request(f"WAITSTOP {args.seconds}",
                                      timeout=float(args.seconds) + 10))
        sys.stdout.flush()
    elif args.cmd_name == "raw":
        sys.stdout.write(send_request(f"RAW {args.data}"))
        sys.stdout.flush()
    elif args.cmd_name == "stop":
        if not os.path.exists(SOCK_PATH):
            print("not running")
            return
        try:
            print(send_request("SHUTDOWN", timeout=5))
        except Exception as e:
            print(f"[error] {e}")
    elif args.cmd_name == "status":
        if not os.path.exists(SOCK_PATH):
            print("not running (socket missing)")
            return
        try:
            print(send_request("PING", timeout=2))
        except Exception as e:
            print(f"socket present but no response: {e}")
    elif args.cmd_name == "ping":
        print(send_request("PING", timeout=2))
    elif args.cmd_name == "health":
        if not os.path.exists(SOCK_PATH):
            print("DOWN (socket missing)")
            return
        try:
            print(send_request("HEALTH", timeout=20))
        except Exception as e:
            print(f"DEGRADED (no response: {e})")
    elif args.cmd_name == "watch":
        watch_loop(args)


if __name__ == "__main__":
    main()
