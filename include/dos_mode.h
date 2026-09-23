/*
 * dos_mode.h — runtime flags for the optional DOS-version graphics/gameplay.
 *
 * The Amiga version is the definitive SWOS; these are PC-only extras. Set from
 * the [dos] section of SWOS.ini and the --dos-graphics / --dos-gameplay launch
 * options (see main.c / swos_settings.c). Game code reads these globals to pick
 * the DOS asset path; default 0 = the normal Amiga graphics/gameplay.
 *
 * gDosGraphics: load + render the DOS graphics from bin/grafs_dos.
 * gDosGameplay: use the DOS gameplay tuning instead of the Amiga one — 70 Hz
 *               game tick (DosGameplay_TargetFps) plus the DOS goalkeeper /
 *               ball-physics constants written by ApplyGameStyle (dos_gameplay.c).
 * gDosView:     render the match at the DOS release's 320x200 instead of the
 *               Amiga's 336x272 (game_loop.c MatchViewEnter). It is the SCREEN
 *               SHAPE, not the assets or the tuning, so HUD placement keys off
 *               this one: the two releases put the clock and the player-name
 *               indicator in measurably different places.
 */
#ifndef SWOS_DOS_MODE_H
#define SWOS_DOS_MODE_H

extern int _gDosGraphics;
extern int _gDosGameplay;
extern int _gDosView;      /* 1 = 320x200 DOS-shaped match view; 0 on Amiga */
extern int _gWidescreen;   /* 1 = wider 16:9 pitch view (SDL); 0 on Amiga */

/* Directory the DOS assets are loaded from, relative to the working dir (bin/).
 * Default "grafs_dos"; override via [dos] directory=... or --dos-dir=... to test
 * alternate sets (e.g. a SWOS-2020 graphics folder). No trailing slash. */
extern char _gDosGraphicsDir[256];

#endif /* SWOS_DOS_MODE_H */
