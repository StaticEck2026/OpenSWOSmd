#!/usr/bin/env bash
# TOMBSTONE — this script is retired and intentionally does nothing.
#
# It used to copy src/ and include/ from the SDL repo (SWOS_C_SDL) into the
# separate Amiga repo (SWOS_C), because the same C port lived in two trees.
# Those trees were MERGED on 2026-08-12. There is now ONE tree that builds
# both targets:
#
#     make                      -> SDL      (bin/SWOS*)
#     make -f Makefile.amiga    -> Amiga    (bin/SWOS2)
#
# Edit the file once. There is nothing to sync.
#
# It is kept (rather than deleted) because several docs still say "add the new
# file to sync_to_amiga_port.sh SRC_FILES" as a checklist step — those notes
# predate the merge. If you got here from one of them: the modern equivalent is
#   - shared code -> just add src/foo.c; both builds pick it up
#   - SDL-only    -> also add foo.c to SDL_ONLY_SRCS in Makefile.amiga
# See .claude/CLAUDE.md "one C source tree, two targets".

cat >&2 <<'MSG'
sync_to_amiga_port.sh is RETIRED — the SDL and Amiga trees were merged.

There is one tree now; nothing needs copying. Build with:
    make                     # SDL   -> bin/SWOS*
    make -f Makefile.amiga   # Amiga -> bin/SWOS2

Adding a new source file?
    shared code : add src/foo.c — both targets compile it
    SDL-only    : add src/foo.c AND list foo.c in SDL_ONLY_SRCS (Makefile.amiga)

See .claude/CLAUDE.md, section "one C source tree, two targets".
MSG
exit 1
