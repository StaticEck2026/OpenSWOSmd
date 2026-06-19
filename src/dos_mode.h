/*
 * dos_mode.h — runtime flags for the optional DOS-version graphics/gameplay.
 *
 * The Amiga version is the definitive SWOS; these are PC-only extras. Set from
 * the [dos] section of SWOS.ini and the --dos-graphics / --dos-gameplay launch
 * options (see main.c / swos_settings.c). Game code reads these globals to pick
 * the DOS asset path; default 0 = the normal Amiga graphics/gameplay.
 *
 * gDosGraphics: load + render the DOS graphics from bin/grafs_dos.
 * gDosGameplay: DOS gameplay tweaks (framerate / minor changes). Reserved — not
 *               wired up yet; kept so the option exists.
 */
#ifndef SWOS_DOS_MODE_H
#define SWOS_DOS_MODE_H

extern int gDosGraphics;
extern int gDosGameplay;

#endif /* SWOS_DOS_MODE_H */
