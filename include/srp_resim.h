/*
 * srp_resim.h -- play a recorded match again, exactly (--srp-resim=<file.SRP>).
 * src/srp_resim.c says what it proves.
 */
#ifndef SRP_RESIM_H
#define SRP_RESIM_H

#include <stdint.h>

/* The command line: the recording to play again. 0, or 1 without a path. */
int32_t SrpResim_Arm(const char *path);
int32_t SrpResim_Armed(void);

/* Read the recording and say how it was played: the gameplay style, the fire-hold
 * lockout and the view (1 DOS, 0 Amiga, -1 a recording from before it said), for main.c
 * to publish before anything reads them. 0, or 1 if it is not a resumable .SRP (and why
 * is printed). */
int32_t SrpResim_Rules(int *dosGameplay, int *fireHoldLockout, int *dosView);

/* Play it again and check every frame; exits the process (0 as recorded, 2 parted, 1
 * refused). From SWOS.c, where the replay rig and the live test start. */
void    SrpResim_Run(void);

#endif /* SRP_RESIM_H */
