# OPL models in OpenSWOS

The Miles Sound System (AIL) AdLib driver's arithmetic, for the PC menu music player
(`src/dos_music.c`): a note to the chip's frequency number and block
(`oplModel_ailFreq`), and velocity and channel volume to the operators' levels
(`oplModel_ailVolume`).

| | |
|---|---|
| taken from | https://github.com/Wohlstand/libADLMIDI, tag `v1.6.2.1` (commit `99c5e03a5cf9b4fdf35b53efddb853c96320d50f`), `src/models/` |
| licence | MIT (`LICENSE.txt`) |
| files taken | `model_ail.c`, `opl_models.h`, `LICENSE.txt` — **unmodified** |
| sha256 | `model_ail.c` 61a1e7879096afa9cb127187f8eeffae8528532c7c638ebd56984d94ef7508b0, `opl_models.h` ea296dd3a7b3018082c87318db5672250c950db0faf91dc90808a9b5923943df |

`opl_models.h` declares the other models of that library too (DMX, Apogee, HMI, ...);
their sources were not taken and nothing calls them.

See `third_party/opal/OPENSWOS.md` for what must NOT be taken from libADLMIDI.
