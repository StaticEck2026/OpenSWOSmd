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
        # Wait for initial gdb prompt.
        self._wait_for_prompt(timeout=5.0)
        # Drop the banner; subsequent `cmd` calls return clean responses.
        self._drain()
        if mode == "amiga":
            # Attach to FS-UAE's stub.
            self._send_raw(f"target remote {host}:{port}\n")
            self._wait_for_prompt(timeout=10.0)
            self._drain()
        # In SDL mode we just leave gdb at its prompt; the caller issues
        # `run`/`start` to launch the inferior.

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
                elif req.startswith("RAW "):
                    out = d.raw(req[4:])
                elif req == "SHUTDOWN":
                    client.sendall(b"stopping\n")
                    client.close()
                    break
                elif req == "PING":
                    out = "pong"
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


def main():
    ap = argparse.ArgumentParser(
        description="Proxy for m68k-amiga-elf-gdb against FS-UAE remote debugger.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    sub = ap.add_subparsers(dest="mode", required=True)

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

    r = sub.add_parser("raw", help="Send raw bytes to gdb stdin (no newline added).")
    r.add_argument("data", help="Raw payload (use $'\\x03' from bash to send Ctrl-C)")

    sub.add_parser("stop", help="Shut the daemon down.")
    sub.add_parser("status", help="Check whether the daemon is reachable.")
    sub.add_parser("ping", help="Round-trip test.")

    args = ap.parse_args()

    if args.mode == "daemon":
        daemon_main(args)
    elif args.mode == "cmd":
        req = f"CMD TIMEOUT={args.timeout}\t{args.command}"
        sys.stdout.write(send_request(req, timeout=args.timeout + 5))
        sys.stdout.flush()
    elif args.mode == "poll":
        sys.stdout.write(send_request(f"POLL {args.seconds}",
                                      timeout=float(args.seconds) + 5))
        sys.stdout.flush()
    elif args.mode == "raw":
        sys.stdout.write(send_request(f"RAW {args.data}"))
        sys.stdout.flush()
    elif args.mode == "stop":
        if not os.path.exists(SOCK_PATH):
            print("not running")
            return
        try:
            print(send_request("SHUTDOWN", timeout=5))
        except Exception as e:
            print(f"[error] {e}")
    elif args.mode == "status":
        if not os.path.exists(SOCK_PATH):
            print("not running (socket missing)")
            return
        try:
            print(send_request("PING", timeout=2))
        except Exception as e:
            print(f"socket present but no response: {e}")
    elif args.mode == "ping":
        print(send_request("PING", timeout=2))


if __name__ == "__main__":
    main()
