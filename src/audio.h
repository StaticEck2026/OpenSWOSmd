/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef AUDIO_H
#define AUDIO_H

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef short int16_t;
typedef int int32_t;

/* ---- Amiga hardware addresses ----
 * CUSTOM_BASE may already be redefined by swos_platform.h to route
 * pokes into the SDL host's stub struct. */
#ifndef CUSTOM_BASE
#define CUSTOM_BASE     0x00DFF000u
#endif
#define DMACON_OFF      0x96        /* DMA control register (write) */
#define AUD0VOL_OFF     0xA8        /* Audio channel 0 volume */
#define AUD1VOL_OFF     0xB8        /* Audio channel 1 volume */
#define AUD2VOL_OFF     0xC8        /* Audio channel 2 volume */
#define AUD3VOL_OFF     0xD8        /* Audio channel 3 volume */

#define AUD0LC_ADDR     (CUSTOM_BASE + 0x0A0u)
#define AUD1LC_ADDR     (CUSTOM_BASE + 0x0B0u)
#define AUD2LC_ADDR     (CUSTOM_BASE + 0x0C0u)
#define AUD3LC_ADDR     (CUSTOM_BASE + 0x0D0u)

/* ---- VariantDescriptor (32 bytes) ----
 * One entry in the bank's instrument table at bankSectionPtrs[0]; each
 * entry is `variantId << 5` bytes from the table base. Same shape as
 * the RJP1 sample-list record (see SFX.md / rjp1_player.h):
 *   +0..0x0B   3 LFO/sample data pointers
 *   +0x0C      signed 16-bit offset into bankSectionPtrs[1] (envelope data)
 *   +0x0E      volume scale
 *   +0x10      "initial play" sample offset (first-time, before loop kicks in)
 *   +0x12      initial length (DMA AUDxLEN word count for the first play)
 *   +0x14      loop offset (the steady-state DMA AUDxLC after first cycle)
 *   +0x16      loop length
 *   +0x18      pitch LFO loop start
 *   +0x1A      pitch LFO total length (word count; doubled when stored)
 *   +0x1C      volume LFO loop start
 *   +0x1E      volume LFO total length (word count; doubled when stored)
 *
 * The C field names follow the original SWOS audio code's `pvSample*`
 * convention even though it conflicts with the cleaner RJP1 naming —
 * tracked under "audio field rename" follow-up. */
#pragma pack(push, 2)
typedef struct {
    uint32_t sampleBasePtr;     /* +0x00 */
    uint32_t pitchLfoPtr;       /* +0x04 */
    uint32_t volLfoPtr;         /* +0x08 */
    int16_t  envDataOffset;     /* +0x0C — signed, into bankSectionPtrs[1] */
    uint16_t volScale;          /* +0x0E */
    uint16_t firstSampleOffset; /* +0x10 — initial-play offset (one-shot) */
    uint16_t firstSampleLength; /* +0x12 — initial-play length */
    uint16_t loopSampleOffset;  /* +0x14 — loop offset (steady-state) */
    uint16_t loopSampleLength;  /* +0x16 — loop length */
    uint16_t pitchLfoLoop;      /* +0x18 */
    uint16_t pitchLfoLen;       /* +0x1A — word count (caller doubles) */
    uint16_t volLfoLoop;        /* +0x1C */
    uint16_t volLfoLen;         /* +0x1E — word count (caller doubles) */
} VariantDescriptor;
#pragma pack(pop)

typedef char _variantdesc_size_check[(sizeof(VariantDescriptor) == 32) ? 1 : -1];

/* ---- AudioState structure (46 bytes, matches SWOS2.s layout) ----
 * Uses pragma pack(2) instead of __attribute__((packed)) so GCC
 * generates word-sized loads/stores instead of byte-by-byte access.
 * All fields are already at 2-byte-aligned offsets in the original
 * asm layout, so pack(2) produces the identical memory layout. */
#pragma pack(push, 2)
typedef struct {
    uint8_t *songPtr;              /* +0x00 */
    uint8_t *bankSectionPtrs[7];   /* +0x04..+0x1F */
    volatile uint16_t masterVolume;         /* +0x20 — volatile: shared with VBlank ISR */
    uint16_t modeFlags;            /* +0x22 */
    uint8_t  irqSubtick;           /* +0x24 */
    uint8_t  toggleFlag;           /* +0x25 */
    uint8_t  primaryChanEnable;    /* +0x26 */
    uint8_t  secondaryChanEnable;  /* +0x27 */
    volatile uint8_t  channelVolume[4];     /* +0x28..+0x2B — volatile: shared with VBlank ISR */
    uint8_t  channelChangeFlags;   /* +0x2C */
    uint8_t  currentChannelIdx;    /* +0x2D */
} AudioState;
#pragma pack(pop)

/* ---- AudioChannel structure (176 bytes / 0xB0, matches SWOS2.s layout) ----
 *
 * Each channel has a primary voice (PV, "beep") and a secondary voice (SV, "mod").
 * The PV and SV share the same hardware register base (hwRegBase).
 *
 * Uses pragma pack(2) instead of __attribute__((packed)) so GCC
 * generates word-sized loads/stores instead of byte-by-byte access.
 * All uint32_t fields are at 2-byte-aligned offsets (not always
 * 4-byte-aligned), so pack(2) preserves the identical memory layout
 * while allowing GCC to use move.w pairs instead of 4x move.b. */
#pragma pack(push, 2)
typedef struct {
    /* ---- Shared ---- */
    uint32_t hwRegBase;          /* 0x00: AUDxLC hardware address */

    /* ---- Primary voice (PV) fields: 0x04-0x5B ---- */
    uint32_t pvSampleBase;       /* 0x04: sample data base pointer */
    uint32_t pvPitchLfoPtr;      /* 0x08: pitch LFO waveform data */
    uint32_t pvVolLfoPtr;        /* 0x0C: volume LFO waveform data */
    uint32_t pvEnvDataPtr;       /* 0x10: envelope segment data */
    uint16_t pvSampleSlot;       /* 0x14: sample slot/offset ID */
    uint16_t pvSampleOffset;     /* 0x16: sample start offset (words) */
    uint16_t pvSampleLength;     /* 0x18: sample length (words) */
    uint16_t pvPitchLfoLoop;     /* 0x1A: pitch LFO loop start */
    uint32_t pvPitchLfoLen;      /* 0x1C: pitch LFO total length */
    uint32_t pvPitchLfoIdx;      /* 0x20: pitch LFO current index */
    uint16_t pvVolLfoLoop;       /* 0x24: volume LFO loop start */
    uint32_t pvVolLfoLen;        /* 0x26: volume LFO total length */
    uint32_t pvVolLfoIdx;        /* 0x2A: volume LFO current index */
    uint16_t pvDmaBit;           /* 0x2E: DMA enable bit mask */
    uint16_t pvVolScale;         /* 0x30: volume scale factor */
    uint16_t pvCurVol;           /* 0x32: current computed volume */
    uint16_t pvBasePeriod;       /* 0x34: base Amiga period */
    uint16_t pvCurPeriod;        /* 0x36: current period (with LFO) */
    uint8_t  pvStartFlag;        /* 0x38: DMA start pending */
    uint8_t  pvReloadFlag;       /* 0x39: AUDx reload pending */
    uint8_t  pvEnvState;         /* 0x3A: envelope state (0/2/4/0xFF) */
    int8_t   pvEnvDelta;         /* 0x3B: envelope delta per step */
    uint8_t  pvEnvCounter;       /* 0x3C: envelope step counter */
    uint8_t  pvEnvDivisor;       /* 0x3D: envelope rate divisor */
    uint8_t  pvEnvTarget;        /* 0x3E: envelope target value */
    uint8_t  pvEnvCurrent;       /* 0x3F: envelope current value */
    uint8_t  pvPitchslideCount;  /* 0x40: pitch slide step counter */
    uint8_t  _pad41;             /* 0x41 */
    uint32_t pvPitchslideRate;   /* 0x42: pitch slide rate (4 bytes) */
    uint32_t pvPitchslideAccum;  /* 0x46: pitch slide accumulator */
    uint32_t pvVariantDesc;      /* 0x4A: variant descriptor pointer */
    uint32_t pvNoteScript;       /* 0x4E: note track data pointer */
    uint32_t pvWaveData;         /* 0x52: wave/script data pointer */
    uint8_t  pvStepPeriod;       /* 0x56: script step period */
    uint8_t  pvScriptTimer;      /* 0x57: script inner timer */
    uint8_t  pvRepeatPeriod;     /* 0x58: script repeat period */
    uint8_t  pvPeriodCounter;    /* 0x59: script period counter */
    uint8_t  pvVariantId;        /* 0x5A: current variant ID */
    uint8_t  pvEnabled;          /* 0x5B: channel active flag */

    /* ---- Secondary voice (SV) fields: 0x5C-0xAF ---- */
    uint32_t svSampleBase;       /* 0x5C: sample data base pointer */
    uint32_t svPitchLfoPtr;      /* 0x60: pitch LFO waveform data */
    uint32_t svVolLfoPtr;        /* 0x64: volume LFO waveform data */
    uint32_t svEnvDataPtr;       /* 0x68: envelope segment data */
    uint16_t svSampleSlot;       /* 0x6C: sample slot/offset ID */
    uint16_t svSampleOffset;     /* 0x6E: sample start offset (words) */
    uint16_t svSampleLength;     /* 0x70: sample length (words) */
    uint16_t svPitchLfoLoop;     /* 0x72: pitch LFO loop start */
    uint32_t svPitchLfoLen;      /* 0x74: pitch LFO total length */
    uint32_t svPitchLfoIdx;      /* 0x78: pitch LFO current index */
    uint16_t svVolLfoLoop;       /* 0x7C: volume LFO loop start */
    uint32_t svVolLfoLen;        /* 0x7E: volume LFO total length */
    uint32_t svVolLfoIdx;        /* 0x82: volume LFO current index */
    uint16_t svDmaBit;           /* 0x86: DMA enable bit mask */
    uint16_t svVolScale;         /* 0x88: volume scale factor */
    uint16_t svCurVol;           /* 0x8A: current computed volume */
    uint16_t svBasePeriod;       /* 0x8C: base Amiga period */
    uint16_t svCurPeriod;        /* 0x8E: current period (with LFO) */
    uint8_t  svStartFlag;        /* 0x90: DMA start pending */
    uint8_t  svReloadFlag;       /* 0x91: AUDx reload pending */
    uint8_t  svEnvState;         /* 0x92: envelope state (0/2/4/0xFF) */
    int8_t   svEnvDelta;         /* 0x93: envelope delta per step */
    uint8_t  svEnvCounter;       /* 0x94: envelope step counter */
    uint8_t  svEnvDivisor;       /* 0x95: envelope rate divisor */
    uint8_t  svEnvTarget;        /* 0x96: envelope target value */
    uint8_t  svEnvCurrent;       /* 0x97: envelope current value */
    uint8_t  svPitchslideCount;  /* 0x98: pitch slide step counter */
    uint8_t  _pad99;             /* 0x99 */
    uint32_t svPitchslideRate;   /* 0x9A: pitch slide rate (4 bytes) */
    uint32_t svPitchslideAccum;  /* 0x9E: pitch slide accumulator */
    uint32_t svVariantDesc;      /* 0xA2: variant descriptor pointer */
    uint32_t svWaveData;         /* 0xA6: mod command stream pointer */
    uint8_t  svStepPeriod;       /* 0xAA: script step period */
    uint8_t  svScriptTimer;      /* 0xAB: script inner timer */
    uint8_t  svRepeatPeriod;     /* 0xAC: script repeat period */
    uint8_t  svPeriodCounter;    /* 0xAD: script period counter */
    uint8_t  svVariantId;        /* 0xAE: current variant ID */
    uint8_t  _padAF;             /* 0xAF */
} AudioChannel;
#pragma pack(pop)

#define AUDIO_CHANNEL_SIZE  0xB0

typedef char _audiochannel_size_check[(sizeof(AudioChannel) == AUDIO_CHANNEL_SIZE) ? 1 : -1];

/* ---- Previously ported functions ---- */
void _InitMenuMusic(void);
void _TryPlayMusic(void);
void _AudioBeepAllShort(void);
void _ClearAudioActiveFlag(void);
void _StopAudioAndClearFlags(void);
void _LoadSfxResources(void);
void _EnsureSfxLoadedAndStart(void);
void _TryRestartingMusic(void);
void _StopMusic(void);
void _CycleMenuMusic(void);

/* ---- Chunk 25: Audio system core ---- */
void _StopAllAudioChannels(void);
void _AudioSetChannelVolume(uint16_t channelOrAll, uint16_t volume);
void _AudioSetMode(uint16_t modeFlags);
void _InitAudioState(uint8_t *bankBase, uint8_t *songPtr);
void _AudioApplyPresetToChannels(uint16_t presetId);
void _AudioChannelNoteOnOff(uint16_t channel, uint16_t noteId);
void _AudioDisableDMAForChannel(AudioChannel *chan);
void _AudioChannelQueueNextWaveIfEnabled(uint16_t channel, uint16_t waveId);

#endif
