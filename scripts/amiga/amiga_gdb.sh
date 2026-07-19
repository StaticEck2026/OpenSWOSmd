#!/usr/bin/env bash
# SWOS -> AmigaScope shim.
#
# The profiling/debug harness now lives in the AmigaScope submodule
# (scripts/amiga/amigascope). This shim supplies the SWOS-specific config
# (hard drives, kickstart, :SWOS2 entry trigger) and forwards to it, so the
# familiar `scripts/amiga/amiga_gdb.sh up|down|restart|status` keeps working.
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export AMIGASCOPE_ENV="${AMIGASCOPE_ENV:-$HERE/amigascope.env}"
exec "$HERE/amigascope/amiga_gdb.sh" "$@"
