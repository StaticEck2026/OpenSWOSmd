#include "audio_irq.h"

/* ---- External data (SWOS2.s code section) ---- */
extern AudioState audioState;
extern AudioChannel audioChannel0;
extern AudioChannel audioChannel1;
extern AudioChannel audioChannel2;
extern AudioChannel audioChannel3;

/* ---- Amiga note period table (36 entries, from SWOS2.s line 9884) ---- */
static const uint16_t kAmigaNotePeriodTable[36] = {
    453,480,508,538,570,604,640,678,
    720,762,808,856,226,240,254,269,
    285,302,320,339,360,381,404,428,
    113,120,127,135,143,151,160,170,
    180,190,202,214
};

/* ---- Hardware access helpers ---- */

/* Get AUDx register base from channel struct */
static inline volatile uint8_t *audxBase(AudioChannel *chan)
{
    return (volatile uint8_t *)chan->hwRegBase;
}

static inline void writeAudxPtr(volatile uint8_t *audx, uint32_t ptr)
{
    *(volatile uint32_t *)audx = ptr;
}

static inline void writeAudxLen(volatile uint8_t *audx, uint16_t len)
{
    *(volatile uint16_t *)(audx + AUDX_LEN) = len;
}

static inline void writeAudxPer(volatile uint8_t *audx, uint16_t per)
{
    *(volatile uint16_t *)(audx + AUDX_PER) = per;
}

static inline void writeAudxVol(volatile uint8_t *audx, uint16_t vol)
{
    *(volatile uint16_t *)(audx + AUDX_VOL) = vol;
}

static inline void writeDMACON(uint16_t val)
{
    *(volatile uint16_t *)(CUSTOM_BASE + DMACON_OFF) = val;
}

/* =================================================================
 * PRIMARY VOICE (beep) — Envelope helpers
 * ================================================================= */

/* BeepVolumeEnvStop — clears envelope state */
static void beepVolumeEnvStop(AudioChannel *chan)
{
    chan->pvEnvState = 0;
}

/* BeepVolumeEnvLoadSegment — loads next envelope segment from data */
static void beepVolumeEnvLoadSegment(AudioChannel *chan)
{
    uint8_t *envData = (uint8_t *)chan->pvEnvDataPtr;
    uint8_t targetVal = envData[3];

    chan->pvEnvTarget = targetVal;
    chan->pvEnvDelta = (int8_t)(targetVal - envData[1]);
    chan->pvEnvDivisor = envData[4];
    chan->pvEnvCounter = envData[4];
    chan->pvEnvState = 2;
}

/* BeepVolumeEnvStep — processes one step of the beep volume envelope */
static void beepVolumeEnvStep(AudioChannel *chan)
{
    uint8_t envState;
    int16_t delta;
    uint8_t counter, divisor;
    int16_t stepVal;
    uint8_t newCurrent;

    envState = chan->pvEnvState;
    if (envState == 0)
        goto store_volume;

    delta = (int8_t)chan->pvEnvDelta;
    if (delta == 0)
        goto force_delta_zero;

    counter = chan->pvEnvCounter;
    if (counter == 0)
        goto force_delta_zero;

    stepVal = delta * (int16_t)(uint16_t)counter;

    divisor = chan->pvEnvDivisor;
    if (divisor == 0)
        goto force_delta_zero;

    stepVal = stepVal / (int16_t)(uint16_t)divisor;
    goto apply_step;

force_delta_zero:
    stepVal = 0;

apply_step:
    newCurrent = (uint8_t)((uint8_t)chan->pvEnvTarget - (uint8_t)stepVal);
    chan->pvEnvCurrent = newCurrent;

    chan->pvEnvCounter = (uint8_t)(chan->pvEnvCounter - 1);

    /* Check if counter wrapped to 0xFF */
    if (chan->pvEnvCounter == 0xFF)
    {
        envState = chan->pvEnvState;
        if (envState & 0x80)
        {
            /* Negative = stop */
            beepVolumeEnvStop(chan);
        }
        else
        {
            /* Dispatch on envState: 0=nop, 2=stop, 4=loadSegment */
            switch (envState)
            {
            case 0:
                break;
            case 2:
                beepVolumeEnvStop(chan);
                break;
            case 4:
                beepVolumeEnvLoadSegment(chan);
                break;
            default:
                break;
            }
        }
    }

store_volume:
    chan->pvCurVol = (uint16_t)chan->pvEnvCurrent;
}

/* BeepVolumeLfoStep — applies LFO modulation to beep volume */
static void beepVolumeLfoStep(AudioChannel *chan)
{
    uint32_t lfoPtr = chan->pvVolLfoPtr;
    uint8_t *lfoData;
    uint32_t idx;
    int8_t sample;
    int16_t mod;

    if (lfoPtr == 0)
        return;

    lfoData = (uint8_t *)lfoPtr;
    idx = chan->pvVolLfoIdx;
    sample = (int8_t)lfoData[idx];
    mod = ((int16_t)sample * (int16_t)chan->pvCurVol) >> 7;
    chan->pvCurVol = (uint16_t)((int16_t)chan->pvCurVol + mod);

    idx++;
    if (idx == chan->pvVolLfoLen)
    {
        idx = (uint32_t)chan->pvVolLfoLoop;
        idx += idx; /* double it */
    }
    chan->pvVolLfoIdx = idx;
}

/* ApplyBeepVolumeAndWriteAUDxVOL — computes final volume, writes HW */
static void applyBeepVolumeAndWriteAUDxVOL(AudioChannel *chan)
{
    volatile uint8_t *audx = audxBase(chan);
    int16_t vol;

    vol = (int16_t)chan->pvCurVol;
    /* muls.w volScale,d0 -> asr.w #6,d0 (truncate to 16-bit before shift) */
    vol = (int16_t)((int32_t)vol * (int32_t)(int16_t)chan->pvVolScale);
    vol >>= 6;
    /* muls.w masterVolume,d0 -> asr.w #6,d0 */
    vol = (int16_t)((int32_t)vol * (int32_t)(int16_t)audioState.masterVolume);
    vol >>= 6;
    chan->pvCurVol = (uint16_t)vol;

    /* Clamp: bls (unsigned <= 0x40), bgt (signed > 0x40), else 0 */
    if (vol < 0)
        vol = 0;
    else if (vol > 0x40)
        vol = 0x40;

    writeAudxVol(audx, (uint16_t)vol);
}

/* =================================================================
 * PRIMARY VOICE — Pitch & LFO
 * ================================================================= */

/* BeepLFOAndPitchSlide — pitch LFO + pitch slide for primary voice */
static void beepLFOAndPitchSlide(AudioChannel *chan)
{
    uint32_t lfoPtr;
    volatile uint8_t *audx;
    int16_t period;
    uint16_t finalPeriod;

    /* Pitch LFO */
    lfoPtr = chan->pvPitchLfoPtr;
    if (lfoPtr != 0)
    {
        uint8_t *lfoData = (uint8_t *)lfoPtr;
        uint32_t idx = chan->pvPitchLfoIdx;
        int8_t sample = (int8_t)lfoData[idx];
        int16_t mod;

        /* muls + asr #7 + neg */
        mod = (int16_t)((int16_t)sample * (int16_t)chan->pvBasePeriod);
        mod = (int16_t)(((int32_t)mod) >> 7);
        mod = -mod;
        /* bpl = if positive, skip halving; if negative, halve */
        if (mod < 0)
            mod >>= 1;

        period = (int16_t)chan->pvBasePeriod + mod;
        chan->pvCurPeriod = (uint16_t)period;

        idx++;
        if (idx == chan->pvPitchLfoLen)
        {
            idx = (uint32_t)chan->pvPitchLfoLoop;
            idx += idx;
        }
        chan->pvPitchLfoIdx = idx;
    }

    /* Pitch slide */
    audx = audxBase(chan);

    if (chan->pvPitchslideCount != 0)
    {
        uint32_t rate = chan->pvPitchslideRate;
        chan->pvPitchslideAccum += rate;
        chan->pvPitchslideCount--;
    }

    /* Final period = slide accumulator high word + current period */
    finalPeriod = (uint16_t)(chan->pvPitchslideAccum >> 16);
    finalPeriod += chan->pvCurPeriod;
    writeAudxPer(audx, finalPeriod);
}

/* =================================================================
 * PRIMARY VOICE — Note init, variant selection
 * ================================================================= */

/* InitVolumeEnvForNote — initializes beep envelope on note start */
static void initVolumeEnvForNote(AudioChannel *chan)
{
    uint8_t *envData = (uint8_t *)chan->pvEnvDataPtr;
    int8_t delta;

    chan->pvEnvTarget = 0;
    delta = (int8_t)(0 - chan->pvEnvCurrent);
    chan->pvEnvDelta = delta;
    chan->pvEnvDivisor = envData[5];
    chan->pvEnvCounter = envData[5];
    chan->pvEnvState = 0xFF; /* st = set true */
}

/* NoteOnInitChannel — sets up channel for note playback
 * Asm: AudioIRQNoteOnInitChannel (line 9480) */
static void noteOnInitChannel(AudioChannel *chan, uint16_t amigaPeriod)
{
    uint8_t *variantDesc = (uint8_t *)chan->pvVariantDesc;
    uint8_t *envData;
    uint8_t *sampleBase;
    volatile uint8_t *audx;

    chan->pvBasePeriod = amigaPeriod;
    chan->pvCurPeriod = amigaPeriod;
    chan->pvPitchslideAccum = 0;

    /* a3 = bankSectionPtrs[1] + word at variantDesc+0x0C */
    envData = audioState.bankSectionPtrs[1] + *(uint16_t *)(variantDesc + 0x0C);
    chan->pvEnvDataPtr = (uint32_t)envData;

    /* Init envelope: target = envData[1], delta = envData[1] - envData[0] */
    chan->pvEnvTarget = envData[1];
    chan->pvEnvDelta = (int8_t)(envData[1] - envData[0]);
    chan->pvEnvDivisor = envData[2];
    chan->pvEnvCounter = envData[2];
    chan->pvEnvState = 4;

    /* Disable DMA: write DMA bit without SET flag */
    writeDMACON(chan->pvDmaBit);

    /* Set up sample: a4 = sampleBase + word at variantDesc+0x10 */
    audx = audxBase(chan);
    sampleBase = (uint8_t *)chan->pvSampleBase;
    sampleBase += (uint32_t)*(uint16_t *)(variantDesc + 0x10);

    /* clr.w (a4) — clear first word of sample */
    *(uint16_t *)sampleBase = 0;

    writeAudxPtr(audx, (uint32_t)sampleBase);
    writeAudxLen(audx, *(uint16_t *)(variantDesc + 0x12));
    chan->pvStartFlag = 0xFF; /* st $38(a0) */
}

/* SelectVariant — selects instrument variant for primary voice */
static void selectVariant(AudioChannel *chan, uint8_t variantId)
{
    uint8_t *desc;

    if (variantId == chan->pvVariantId)
        return;

    chan->pvVariantId = variantId;

    /* desc = bankSectionPtrs[0] + variantId * 32 */
    desc = audioState.bankSectionPtrs[0] + ((uint16_t)variantId << 5);
    chan->pvVariantDesc = (uint32_t)desc;

    /* Copy descriptor fields to channel */
    chan->pvSampleSlot   = *(uint16_t *)(desc + 0x12);
    chan->pvSampleOffset = *(uint16_t *)(desc + 0x14);
    chan->pvSampleLength = *(uint16_t *)(desc + 0x16);
    chan->pvVolScale     = *(uint16_t *)(desc + 0x0E);
    chan->pvPitchLfoLoop = *(uint16_t *)(desc + 0x18);

    chan->pvPitchLfoIdx = 0;
    {
        uint32_t len = (uint32_t)*(uint16_t *)(desc + 0x1A);
        chan->pvPitchLfoLen = len + len;
    }

    chan->pvVolLfoLoop = *(uint16_t *)(desc + 0x1C);
    chan->pvVolLfoIdx = 0;
    {
        uint32_t len = (uint32_t)*(uint16_t *)(desc + 0x1E);
        chan->pvVolLfoLen = len + len;
    }

    /* Copy 3 pointers from descriptor */
    chan->pvSampleBase   = *(uint32_t *)(desc + 0x00);
    chan->pvPitchLfoPtr  = *(uint32_t *)(desc + 0x04);
    chan->pvVolLfoPtr    = *(uint32_t *)(desc + 0x08);
}

/* =================================================================
 * PRIMARY VOICE — DMA reload/start, Script interpreter
 * ================================================================= */

/* LoadAUDxPtrLenIfReloadNeeded — primary voice DMA reload */
static uint8_t loadAUDxPtrLenIfReloadNeeded(AudioChannel *chan)
{
    if (chan->pvReloadFlag & 1)
    {
        uint8_t *sampleBase;
        uint32_t offset;
        volatile uint8_t *audx;

        chan->pvReloadFlag &= ~1;

        sampleBase = (uint8_t *)chan->pvSampleBase;
        offset = (uint32_t)chan->pvSampleOffset;
        offset += offset; /* words to bytes */
        sampleBase += offset;

        audx = audxBase(chan);
        writeAudxPtr(audx, (uint32_t)sampleBase);
        writeAudxLen(audx, chan->pvSampleLength);
    }

    return audioState.irqSubtick;
}

/* EnableDMAIfStartFlag — primary voice DMA enable */
static void enableDMAIfStartFlag(AudioChannel *chan)
{
    /* bclr #0,$38(a0): test bit 0, clear it */
    if (!(chan->pvStartFlag & 1))
        return;

    chan->pvStartFlag &= ~1;
    chan->pvReloadFlag = 0xFF; /* st $39(a0) */
    writeDMACON(0x8200 | chan->pvDmaBit);
}

/* RunChannelScriptStep — primary voice script interpreter */
static void runChannelScriptStep(AudioChannel *chan, uint8_t loopCount)
{
    uint8_t d7 = loopCount;

    do {
        uint32_t scriptPtr = chan->pvWaveData;
        uint8_t *script;
        uint8_t cmdByte;

        if (scriptPtr != 0)
        {
            if (audioState.toggleFlag != 0)
                goto next_iter;

            chan->pvScriptTimer--;
            if (chan->pvScriptTimer != 0)
                goto next_iter;

            chan->pvPeriodCounter--;
            if (chan->pvPeriodCounter != 0)
                goto reload_timer;

            script = (uint8_t *)scriptPtr;
            goto fetch_cmd;
        }
        else
        {
            goto next_iter;
        }

fetch_cmd:
        cmdByte = *script++;
        if (cmdByte & 0x80)
        {
            /* Command byte — dispatch via jump table */
            uint8_t cmdIdx = (cmdByte & 0x7F);

            switch (cmdIdx)
            {
            case 0: /* cmd 0x80: play note from track */
            {
                uint8_t *trackPtr = (uint8_t *)chan->pvNoteScript;
                uint8_t noteRef;

                chan->pvRepeatPeriod = 1;

                for (;;)
                {
                    noteRef = *trackPtr++;
                    if (noteRef != 0)
                    {
                        /* Valid note reference */
                        uint8_t **noteTable;
                        uint8_t *noteData;

                        chan->pvNoteScript = (uint32_t)trackPtr;
                        noteTable = (uint8_t **)audioState.bankSectionPtrs[4];
                        noteData = noteTable[noteRef];
                        script = (uint8_t *)((uint32_t)noteData + (uint32_t)audioState.bankSectionPtrs[6]);
                        goto fetch_cmd;
                    }

                    /* noteRef == 0: peek at next byte */
                    noteRef = *trackPtr;
                    if (noteRef == 0)
                    {
                        /* End of track — disable channel */
                        script = (uint8_t *)0;
                        chan->pvEnabled = 0;
                        audioState.channelChangeFlags &= ~(1 << audioState.currentChannelIdx);
                        goto save_and_reload;
                    }
                    else if (noteRef & 0x80)
                    {
                        /* Indirect jump: next byte is index */
                        uint8_t jumpIdx = trackPtr[1];
                        uint8_t **jumpTable = (uint8_t **)audioState.bankSectionPtrs[3];
                        trackPtr = jumpTable[jumpIdx];
                        trackPtr = (uint8_t *)((uint32_t)trackPtr + (uint32_t)audioState.bankSectionPtrs[5]);
                    }
                    else
                    {
                        /* Relative backward jump */
                        trackPtr -= noteRef;
                    }
                }
                break;
            }
            case 1: /* cmd 0x82: init instrument/note params */
                initVolumeEnvForNote(chan);
                goto save_and_reload;

            case 2: /* cmd 0x84: set step period */
                chan->pvStepPeriod = *script++;
                goto fetch_cmd;

            case 3: /* cmd 0x86: set repeat period */
                chan->pvRepeatPeriod = *script++;
                goto fetch_cmd;

            case 4: /* cmd 0x88: set variant if nonzero */
            {
                uint8_t varId = *script++;
                if (varId != 0)
                    selectVariant(chan, varId);
                goto fetch_cmd;
            }
            case 5: /* cmd 0x8A: set volume scale */
            {
                uint8_t scaleVal = *script++;
                chan->pvVolScale = (uint16_t)scaleVal;
                goto fetch_cmd;
            }
            case 6: /* cmd 0x8C: set envelope params */
                chan->pvPitchslideCount = *script++;
                {
                    uint8_t *rate = (uint8_t *)&chan->pvPitchslideRate;
                    rate[0] = *script++;
                    rate[1] = *script++;
                    rate[2] = *script++;
                    rate[3] = *script++;
                }
                chan->pvPitchslideAccum = 0;
                goto fetch_cmd;

            case 7: /* cmd 0x8E: save and reload (skip note init) */
                goto save_and_reload;

            default:
                goto fetch_cmd;
            }
        }
        else
        {
            /* Note byte — look up period from table */
            uint16_t period = kAmigaNotePeriodTable[cmdByte];

            /* If secondary voice not active, init channel */
            if (chan->svWaveData == 0)
                noteOnInitChannel(chan, period);

save_and_reload:
            chan->pvWaveData = (uint32_t)script;
            chan->pvPeriodCounter = chan->pvRepeatPeriod;
        }

reload_timer:
        chan->pvScriptTimer = chan->pvStepPeriod;

next_iter:
        ;
    } while (d7-- > 0);
}

/* UpdateBeepVoiceParams — primary voice volume processing */
static void updateBeepVoiceParams(AudioChannel *chan)
{
    beepVolumeEnvStep(chan);
    beepVolumeLfoStep(chan);
    applyBeepVolumeAndWriteAUDxVOL(chan);
}

/* =================================================================
 * SECONDARY VOICE (mod) — Envelope helpers
 * ================================================================= */

/* VolumeEnvStop — clears secondary envelope state */
static void volumeEnvStop(AudioChannel *chan)
{
    chan->svEnvState = 0;
}

/* VolumeEnvLoadSegment — loads next secondary envelope segment */
static void volumeEnvLoadSegment(AudioChannel *chan)
{
    uint8_t *envData = (uint8_t *)chan->svEnvDataPtr;
    uint8_t targetVal = envData[3];

    chan->svEnvTarget = targetVal;
    chan->svEnvDelta = (int8_t)(targetVal - envData[1]);
    chan->svEnvDivisor = envData[4];
    chan->svEnvCounter = envData[4];
    chan->svEnvState = 2;
}

/* VolumeEnvStep — secondary voice volume envelope step */
static void volumeEnvStep(AudioChannel *chan)
{
    uint8_t envState;
    int16_t delta;
    uint8_t counter, divisor;
    int16_t stepVal;
    uint8_t newCurrent;

    envState = chan->svEnvState;
    if (envState == 0)
        goto store_volume;

    delta = (int8_t)chan->svEnvDelta;
    if (delta == 0)
        goto force_delta_zero;

    counter = chan->svEnvCounter;
    if (counter == 0)
        goto force_delta_zero;

    stepVal = delta * (int16_t)(uint16_t)counter;

    divisor = chan->svEnvDivisor;
    if (divisor == 0)
        goto force_delta_zero;

    stepVal = stepVal / (int16_t)(uint16_t)divisor;
    goto apply_step;

force_delta_zero:
    stepVal = 0;

apply_step:
    newCurrent = (uint8_t)((uint8_t)chan->svEnvTarget - (uint8_t)stepVal);
    chan->svEnvCurrent = newCurrent;

    chan->svEnvCounter = (uint8_t)(chan->svEnvCounter - 1);

    if (chan->svEnvCounter == 0xFF)
    {
        envState = chan->svEnvState;
        if (envState & 0x80)
        {
            volumeEnvStop(chan);
        }
        else
        {
            switch (envState)
            {
            case 0:
                break;
            case 2:
                volumeEnvStop(chan);
                break;
            case 4:
                volumeEnvLoadSegment(chan);
                break;
            default:
                break;
            }
        }
    }

store_volume:
    chan->svCurVol = (uint16_t)chan->svEnvCurrent;
}

/* VolumeLfoStep — secondary voice volume LFO */
static void volumeLfoStep(AudioChannel *chan)
{
    uint32_t lfoPtr = chan->svVolLfoPtr;
    uint8_t *lfoData;
    uint32_t idx;
    int8_t sample;
    int16_t mod;

    if (lfoPtr == 0)
        return;

    lfoData = (uint8_t *)lfoPtr;
    idx = chan->svVolLfoIdx;
    sample = (int8_t)lfoData[idx];
    mod = ((int16_t)sample * (int16_t)chan->svCurVol) >> 7;
    chan->svCurVol = (uint16_t)((int16_t)chan->svCurVol + mod);

    idx++;
    if (idx == chan->svVolLfoLen)
    {
        idx = (uint32_t)chan->svVolLfoLoop;
        idx += idx;
    }
    chan->svVolLfoIdx = idx;
}

/* ApplyVolumeAndWriteAUDxVOL — secondary voice final volume + HW write */
static void applyVolumeAndWriteAUDxVOL(AudioChannel *chan)
{
    volatile uint8_t *audx = audxBase(chan);
    int16_t vol;

    vol = (int16_t)chan->svCurVol;
    /* muls.w volScale,d0 -> asr.w #6,d0 */
    vol = (int16_t)((int32_t)vol * (int32_t)(int16_t)chan->svVolScale);
    vol >>= 6;
    /* muls.w masterVolume,d0 -> asr.w #6,d0 */
    vol = (int16_t)((int32_t)vol * (int32_t)(int16_t)audioState.masterVolume);
    vol >>= 6;
    chan->svCurVol = (uint16_t)vol;

    /* Clamp: bls (unsigned <= 0x40), bgt (signed > 0x40), else 0 */
    if (vol < 0)
        vol = 0;
    else if (vol > 0x40)
        vol = 0x40;

    writeAudxVol(audx, (uint16_t)vol);
}

/* ServiceVolume — secondary voice volume processing */
static void serviceVolume(AudioChannel *chan)
{
    volumeEnvStep(chan);
    volumeLfoStep(chan);
    applyVolumeAndWriteAUDxVOL(chan);
}

/* =================================================================
 * SECONDARY VOICE — Pitch LFO & slide, Note init, Instrument select
 * ================================================================= */

/* LFOAndPitchSlide — secondary voice pitch processing */
static void lfoAndPitchSlide(AudioChannel *chan)
{
    uint32_t lfoPtr;
    volatile uint8_t *audx;
    uint16_t finalPeriod;

    lfoPtr = chan->svPitchLfoPtr;
    if (lfoPtr != 0)
    {
        uint8_t *lfoData = (uint8_t *)lfoPtr;
        uint32_t idx = chan->svPitchLfoIdx;
        int8_t sample = (int8_t)lfoData[idx];
        int16_t mod;

        mod = (int16_t)((int16_t)sample * (int16_t)chan->svBasePeriod);
        mod = (int16_t)(((int32_t)mod) >> 7);
        mod = -mod;
        if (mod < 0)
            mod >>= 1;

        chan->svCurPeriod = (uint16_t)((int16_t)chan->svBasePeriod + mod);

        idx++;
        if (idx == chan->svPitchLfoLen)
        {
            idx = (uint32_t)chan->svPitchLfoLoop;
            idx += idx;
        }
        chan->svPitchLfoIdx = idx;
    }

    audx = audxBase(chan);

    if (chan->svPitchslideCount != 0)
    {
        uint32_t rate = chan->svPitchslideRate;
        chan->svPitchslideAccum += rate;
        chan->svPitchslideCount--;
    }

    finalPeriod = (uint16_t)(chan->svPitchslideAccum >> 16);
    finalPeriod += chan->svCurPeriod;
    writeAudxPer(audx, finalPeriod);
}

/* InitPitchEnv — secondary voice note init (analogous to NoteOnInitChannel) */
static void initPitchEnv(AudioChannel *chan, uint16_t amigaPeriod)
{
    uint8_t *variantDesc = (uint8_t *)chan->svVariantDesc;
    uint8_t *envData;
    uint8_t *sampleBase;
    uint32_t sampleOff;
    volatile uint8_t *audx;

    chan->svBasePeriod = amigaPeriod;
    chan->svCurPeriod = amigaPeriod;
    chan->svPitchslideAccum = 0;

    envData = audioState.bankSectionPtrs[1] + *(uint16_t *)(variantDesc + 0x0C);
    chan->svEnvDataPtr = (uint32_t)envData;

    chan->svEnvTarget = envData[1];
    {
        uint8_t d0 = envData[1];
        d0 -= envData[0];
        chan->svEnvDelta = (int8_t)d0;
    }
    chan->svEnvDivisor = envData[2];
    chan->svEnvCounter = envData[2];
    chan->svEnvState = 4;

    /* Disable DMA */
    writeDMACON(chan->svDmaBit);

    /* Set up sample */
    audx = audxBase(chan);
    sampleBase = (uint8_t *)chan->svSampleBase;
    sampleOff = *(uint16_t *)(variantDesc + 0x10);
    sampleBase += sampleOff;

    *(uint16_t *)sampleBase = 0;
    writeAudxPtr(audx, (uint32_t)sampleBase);
    writeAudxLen(audx, *(uint16_t *)(variantDesc + 0x12));
    chan->svStartFlag = 0xFF;
}

/* InitVolumeEnv — secondary voice envelope init */
static void initVolumeEnv(AudioChannel *chan)
{
    uint8_t *envData = (uint8_t *)chan->svEnvDataPtr;
    int8_t delta;

    chan->svEnvTarget = 0;
    delta = (int8_t)(0 - chan->svEnvCurrent);
    chan->svEnvDelta = delta;
    chan->svEnvDivisor = envData[5];
    chan->svEnvCounter = envData[5];
    chan->svEnvState = 0xFF;
}

/* SelectInstrument — selects instrument for secondary voice */
static void selectInstrument(AudioChannel *chan, uint8_t instrId)
{
    uint8_t *desc;

    if (instrId == chan->svVariantId)
        return;

    chan->svVariantId = instrId;

    desc = audioState.bankSectionPtrs[0] + ((uint16_t)instrId << 5);
    chan->svVariantDesc = (uint32_t)desc;

    chan->svSampleSlot     = *(uint16_t *)(desc + 0x12);
    chan->svSampleOffset   = *(uint16_t *)(desc + 0x14);
    chan->svSampleLength   = *(uint16_t *)(desc + 0x16);
    chan->svVolScale       = *(uint16_t *)(desc + 0x0E);
    chan->svPitchLfoLoop   = *(uint16_t *)(desc + 0x18);

    chan->svPitchLfoIdx = 0;
    {
        uint32_t len = (uint32_t)*(uint16_t *)(desc + 0x1A);
        chan->svPitchLfoLen = len + len;
    }

    chan->svVolLfoLoop = *(uint16_t *)(desc + 0x1C);
    chan->svVolLfoIdx = 0;
    {
        uint32_t len = (uint32_t)*(uint16_t *)(desc + 0x1E);
        chan->svVolLfoLen = len + len;
    }

    chan->svSampleBase    = *(uint32_t *)(desc + 0x00);
    chan->svPitchLfoPtr   = *(uint32_t *)(desc + 0x04);
    chan->svVolLfoPtr     = *(uint32_t *)(desc + 0x08);
}

/* =================================================================
 * SECONDARY VOICE — DMA reload/start, Mod command stream
 * ================================================================= */

/* LoadAUDxPtrLenIfReloadNeeded2 — secondary voice DMA reload */
static uint8_t loadAUDxPtrLenIfReloadNeeded2(AudioChannel *chan)
{
    if (chan->svReloadFlag & 1)
    {
        uint8_t *sampleBase;
        uint32_t offset;
        volatile uint8_t *audx;

        chan->svReloadFlag &= ~1;

        sampleBase = (uint8_t *)chan->svSampleBase;
        offset = (uint32_t)chan->svSampleOffset;
        offset += offset;
        sampleBase += offset;

        audx = audxBase(chan);
        writeAudxPtr(audx, (uint32_t)sampleBase);
        writeAudxLen(audx, chan->svSampleLength);
    }

    return audioState.irqSubtick;
}

/* EnableDMAIfStartFlag2 — secondary voice DMA enable */
static void enableDMAIfStartFlag2(AudioChannel *chan)
{
    /* bclr #0,$90(a0): test bit 0, clear it */
    if (!(chan->svStartFlag & 1))
        return;

    chan->svStartFlag &= ~1;
    chan->svReloadFlag = 0xFF; /* st $91(a0) */
    writeDMACON(0x8200 | chan->svDmaBit);
}

/* RunModCommandStream — secondary voice mod command interpreter */
static void runModCommandStream(AudioChannel *chan)
{
    uint32_t streamPtr;
    uint8_t *stream;
    uint8_t cmdByte;

    /* Check modeFlags bit 2 (secondary processing enabled) */
    if (!(audioState.modeFlags & 0x04))
        return;

    streamPtr = chan->svWaveData;
    if (streamPtr == 0)
        return;

    if (audioState.toggleFlag != 0)
        return;

    chan->svScriptTimer--;
    if (chan->svScriptTimer != 0)
        return;

    chan->svPeriodCounter--;
    if (chan->svPeriodCounter != 0)
        goto reload_timer;

    stream = (uint8_t *)streamPtr;
    goto fetch_cmd;

fetch_cmd:
    cmdByte = *stream++;
    if (cmdByte & 0x80)
    {
        uint8_t cmdIdx = (cmdByte & 0x7F);

        switch (cmdIdx)
        {
        case 0: /* cmd 0x80: end of mod stream — disable secondary voice */
            stream = (uint8_t *)0;
            {
                uint8_t bit = audioState.currentChannelIdx + 4;
                audioState.channelChangeFlags &= ~(1 << bit);
            }
            goto save_and_reload;

        case 1: /* cmd 0x82: init volume envelope */
            initVolumeEnv(chan);
            goto save_and_reload;

        case 2: /* cmd 0x84: set step period */
            chan->svStepPeriod = *stream++;
            goto fetch_cmd;

        case 3: /* cmd 0x86: set repeat period */
            chan->svRepeatPeriod = *stream++;
            goto fetch_cmd;

        case 4: /* cmd 0x88: select instrument if nonzero */
        {
            uint8_t instrId = *stream++;
            if (instrId != 0)
                selectInstrument(chan, instrId);
            goto fetch_cmd;
        }
        case 5: /* cmd 0x8A: set volume scale */
        {
            uint8_t scaleVal = *stream++;
            chan->svVolScale = (uint16_t)scaleVal;
            goto fetch_cmd;
        }
        case 6: /* cmd 0x8C: set pitch slide params */
            chan->svPitchslideCount = *stream++;
            {
                uint8_t *rate = (uint8_t *)&chan->svPitchslideRate;
                rate[0] = *stream++;
                rate[1] = *stream++;
                rate[2] = *stream++;
                rate[3] = *stream++;
            }
            chan->svPitchslideAccum = 0;
            goto fetch_cmd;

        case 7: /* cmd 0x8E: skip to save_and_reload */
            goto save_and_reload;

        default:
            goto fetch_cmd;
        }
    }
    else
    {
        /* Note byte — look up period, init pitch envelope */
        uint16_t period = kAmigaNotePeriodTable[cmdByte];
        initPitchEnv(chan, period);
    }

save_and_reload:
    chan->svWaveData = (uint32_t)stream;
    chan->svPeriodCounter = chan->svRepeatPeriod;

reload_timer:
    chan->svScriptTimer = chan->svStepPeriod;
}

/* =================================================================
 * TOP-LEVEL DISPATCH
 * ================================================================= */

/* ServiceOneChannel — processes one channel (both voices) */
static void serviceOneChannel(AudioChannel *chan)
{
    /* Check if secondary voice is active */
    if (chan->svWaveData != 0 && audioState.secondaryChanEnable != 0)
    {
        /* Secondary voice path */
        uint8_t loopCount = loadAUDxPtrLenIfReloadNeeded2(chan);
        enableDMAIfStartFlag2(chan);
        runChannelScriptStep(chan, loopCount);
        runModCommandStream(chan);
        serviceVolume(chan);
        lfoAndPitchSlide(chan);
        return;
    }

    /* Primary (beep) voice path */
    if (audioState.primaryChanEnable == 0)
        return;
    if (chan->pvEnabled == 0)
        return;

    {
        uint8_t loopCount = loadAUDxPtrLenIfReloadNeeded(chan);
        enableDMAIfStartFlag(chan);
        runChannelScriptStep(chan, loopCount);
        updateBeepVoiceParams(chan);
        beepLFOAndPitchSlide(chan);
    }
}

/* AudioIRQUpdateChannels — main IRQ handler for all 4 channels
 *
 * Called from the VBlank ISR via AudioIRQEntryThunk.
 * Returns channelChangeFlags in d0 (used by caller).
 */
__attribute__((used, externally_visible))
uint8_t _AudioIRQUpdateChannels(void)
{
    uint16_t counter;

    /* Read modeFlags bottom 2 bits as a speed counter */
    counter = audioState.modeFlags & 3;
    if (counter == 0)
    {
        /* Toggle flag when counter is 0 */
        audioState.toggleFlag ^= 1;
        counter = 1;
    }

    /* irqSubtick = counter - 1 (used as dbf loop count) */
    audioState.irqSubtick = (uint8_t)(counter - 1);

    /* Channel 0: move.b $28(a5),$21(a5) — per-channel vol to masterVolume low byte */
    audioState.masterVolume = (audioState.masterVolume & 0xFF00u) | (uint16_t)audioState.channelVolume[0];
    audioState.currentChannelIdx = 0;
    serviceOneChannel(&audioChannel0);

    /* Channel 1 */
    audioState.masterVolume = (audioState.masterVolume & 0xFF00u) | (uint16_t)audioState.channelVolume[1];
    audioState.currentChannelIdx = 1;
    serviceOneChannel(&audioChannel1);

    /* Channel 2 */
    audioState.masterVolume = (audioState.masterVolume & 0xFF00u) | (uint16_t)audioState.channelVolume[2];
    audioState.currentChannelIdx = 2;
    serviceOneChannel(&audioChannel2);

    /* Channel 3 */
    audioState.masterVolume = (audioState.masterVolume & 0xFF00u) | (uint16_t)audioState.channelVolume[3];
    audioState.currentChannelIdx = 3;
    serviceOneChannel(&audioChannel3);

    return audioState.channelChangeFlags;
}
