# Opal in OpenSWOS

The OPL3 (AdLib / Sound Blaster FM chip) emulator of the PC menu music player
(`src/dos_music.c`, docs/DOS_SOUND.md "The music").

| | |
|---|---|
| origin | Opal, from Reality Adlib Tracker (https://www.3eality.com/), in its C form as carried by libADLMIDI |
| taken from | https://github.com/Wohlstand/libADLMIDI, tag `v1.6.2.1` (commit `99c5e03a5cf9b4fdf35b53efddb853c96320d50f`), `src/chips/opal/` |
| licence | public domain (`LICENSE.txt`: "All source code supplied with RAD is in the public domain") |
| files taken | `opal.c`, `opal.h`, `LICENSE.txt` — **unmodified** |
| sha256 | `opal.c` 1538eb96af39b3549f8bb487c3ff680da23cc0e19dd4ed7abca2eed46d97ccf0, `opal.h` 7878746c9c1a53f6bef23cce23af5732fdddbcafba084d246b23573ef449bda0 |

**Nothing else of libADLMIDI is in this tree, and nothing else may be added without
reading its licence first.** Its player (`adlmidi*.cpp`), its chip programming layer and
its bank database are GPL v3; its XMI converter and its other chip emulators are LGPL or
GPL. Only this directory (public domain) and `third_party/opl_models` (MIT) were taken.

Compiled once, with the model, by `src/dos_music_impl.c` — SDL builds only.

It is "not a complete emulator, just enough for Reality Adlib Tracker tunes" (its own
words): no timers, no percussion mode, no CSW. The player uses none of those. If the
sound ever needs to be closer to the chip, ymfm (BSD 3-clause, C++) is the permissive
alternative; it was not used because it would bring C++ into the builds that have none.
