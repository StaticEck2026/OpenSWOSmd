#ifndef AUDIO_IRQ_H
#define AUDIO_IRQ_H

#include "audio.h"

/* AUDx register block offsets */
#define AUDX_LC    0   /* uint32_t: sample pointer */
#define AUDX_LEN   4   /* uint16_t: length in words */
#define AUDX_PER   6   /* uint16_t: period */
#define AUDX_VOL   8   /* uint16_t: volume */

/* ---- Exported function ---- */
uint8_t _AudioIRQUpdateChannels(void);

#endif
