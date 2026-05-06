/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "audio.h"
#include "endian_helpers.h"
#include "display.h"

#ifdef SWOS_SDL_BUILD
/* RJP1 SDL backend (defined in audio_sdl.c). */
extern int  AudioSDL_StartSong(const uint8_t *song_data, int song_size,
                               const uint8_t *ins_data,  int ins_size,
                               int subsong);
extern int  AudioSDL_LoadSfxBank(const uint8_t *song_data, int song_size,
                                 const uint8_t *ins_data,  int ins_size);
extern void AudioSDL_StopSong(void);
extern void AudioSDL_SetMasterVolume(int vol_0_64);

/* Buffer capacities — must match the array sizes in src/swos_globals.c.
 * The RJP1 loader only uses these to bounds-check the .sng layout. */
#define MENU_SONG_DATA_BYTES  (8  * 1024)
#define MENU_INS_BANK_BYTES   (64 * 1024)
/* SFX bank: sfx.sng is small (~2.3 KB) but sfx.in1 is sample-heavy.
 * sfxSongData is sized 8 KB; poolplyrDatBuffer is 0x14000 = 80 KB
 * (see swos_amiga_data_stubs.c for the size rationale — sfx.in1 is
 * 71.5 KB and the original 0xCBE8 / 52 KB allocation overflowed into
 * imageCmdWorkBuf, clobbering chars sprites). */
#define SFX_SONG_DATA_BYTES   (8  * 1024)
#define SFX_INS_BANK_BYTES    (0x14000)
#endif

/* ---- External globals (all live in SWOS2.s) ---- */
extern uint16_t audioBankLoaded;
extern volatile uint16_t audioActive;
extern uint8_t menuSongData[];
extern uint8_t menuInsBank[];
extern uint8_t aSoundMenuSng[];
extern uint8_t aSoundMenuIns[];
extern uint8_t sfxSongData[];
extern uint8_t poolplyrDatBuffer[];
extern uint8_t sfxInsBank2[];
extern uint8_t aSoundSfxSng[];
extern uint8_t aSoundSfxIn1[];
extern uint8_t aSoundSfxIn2[];
extern volatile uint16_t currentGameTick;
extern void _WaitForVSync(void);   /* game_utils.c — pumps audio + events on SDL */

/* Audio system data (PC-relative in SWOS2.s code section) */
extern AudioState audioState;
extern AudioChannel audioChannel0;
extern AudioChannel audioChannel1;
extern AudioChannel audioChannel2;
extern AudioChannel audioChannel3;

/* ---- External C functions ---- */
extern int16_t _PlaySong(uint8_t *name, uint8_t *dest);  /* asset_loader.c */

/* ---- Pointer to each channel by index ---- */
static AudioChannel *audioChannels[4];

static void ensureChannelPtrs(void)
{
    audioChannels[0] = &audioChannel0;
    audioChannels[1] = &audioChannel1;
    audioChannels[2] = &audioChannel2;
    audioChannels[3] = &audioChannel3;
}

/* ================================================================
 * Access (inline equivalent of Resload.s:Access)
 *
 * Relocates a pointer by adding bankSectionPtrs[5] and masking
 * to 24-bit Amiga address space.
 * ================================================================ */
static uint8_t *accessRelocate(uint8_t *ptr)
{
    return (uint8_t *)(((uint32_t)ptr + (uint32_t)audioState.bankSectionPtrs[5]) & 0x00FFFFFFu);
}

/* ================================================================
 * StopAllAudioChannels  (SWOS2.s line 9175)
 *
 * Disables all 4 audio DMA channels and clears master volume
 * and mode flags in audioState.
 * ================================================================ */
__attribute__((used, externally_visible))
void _StopAllAudioChannels(void)
{
    audioState.masterVolume = 0;
    audioState.modeFlags = 0;

    /* Disable DMA for all 4 audio channels (bits 0-3 of DMACON) */
    custom->dmacon = 0x000F;

    /* Zero all 4 channel volumes — use absolute addresses (not in struct) */
    *(volatile uint16_t *)(CUSTOM_BASE + AUD0VOL_OFF) = 0;
    *(volatile uint16_t *)(CUSTOM_BASE + AUD1VOL_OFF) = 0;
    *(volatile uint16_t *)(CUSTOM_BASE + AUD2VOL_OFF) = 0;
    *(volatile uint16_t *)(CUSTOM_BASE + AUD3VOL_OFF) = 0;
}

/* ================================================================
 * AudioSetChannelVolume  (SWOS2.s line 9192)
 *
 * Sets the volume for a specific channel (0-3) or all channels
 * (if channelOrAll has bit 7 set, e.g. 0xFF/0xFFFF).
 * Volume 0x40 is kept as-is; other values are masked to 0x3F.
 * ================================================================ */
__attribute__((used, externally_visible))
void _AudioSetChannelVolume(uint16_t channelOrAll, uint16_t volume)
{
    uint8_t vol;

    /* Clamp: if exactly 0x40, keep it; otherwise mask to 0x3F */
    if (volume != 0x40)
        volume &= 0x3F;
    vol = (uint8_t)volume;

    /* tst.b d0 / bmi — tests bit 7 of low byte */
    if (channelOrAll & 0x80)
    {
        /* Apply to all channels */
        audioState.channelVolume[0] = vol;
        audioState.channelVolume[1] = vol;
        audioState.channelVolume[2] = vol;
        audioState.channelVolume[3] = vol;
#ifdef SWOS_SDL_BUILD
        AudioSDL_SetMasterVolume(vol);
#endif
    }
    else
    {
        audioState.channelVolume[channelOrAll & 3] = vol;
#ifdef SWOS_SDL_BUILD
        /* Without this the per-channel volume only updated the
         * Amiga-side state byte and never reached the SDL audio
         * engine — so e.g. the chant fade-out path (which sets
         * channel-2 volume to 0 over 128 frames) silently did
         * nothing and SFX_CROWD_CHANT_2 looped forever at full
         * volume because its wave had repeat_len > 0. */
        extern void AudioSDL_SetChannelVolume(int channel, int vol_0_64);
        AudioSDL_SetChannelVolume(channelOrAll & 3, vol);
#endif
    }
}

/* ================================================================
 * AudioSetMode  (SWOS2.s line 9215)
 *
 * Sets audio mode flags (bits 0-3). Bit 3 enables primary channel
 * processing; bit 2 enables secondary channel processing.
 * ================================================================ */
__attribute__((used, externally_visible))
void _AudioSetMode(uint16_t mode)
{
    mode &= 0x0F;
    audioState.modeFlags = mode;
    audioState.toggleFlag = 0;
    audioState.primaryChanEnable = 0xFF;
    audioState.secondaryChanEnable = 0xFF;

    if (!(mode & 0x08))
        audioState.primaryChanEnable = 0;
    if (!(mode & 0x04))
        audioState.secondaryChanEnable = 0;
}

/* ================================================================
 * ClearAudioChannelStruct  (SWOS2.s line 9164)
 *
 * Zeros out an entire audio channel structure (0xB0 = 176 bytes).
 * Uses word-sized writes (0x58 words = 88 words = 176 bytes).
 * ================================================================ */
__attribute__((optimize("no-tree-loop-distribute-patterns")))
static void clearAudioChannelStruct(AudioChannel *chan)
{
    uint16_t *p = (uint16_t *)chan;
    int16_t i;

    for (i = 0x57; i >= 0; i--)
        *p++ = 0;
}

/* Also exported for potential asm callers */
__attribute__((used, externally_visible))
void _ClearAudioChannelStruct(AudioChannel *chan)
{
    clearAudioChannelStruct(chan);
}

/* ================================================================
 * InitAudioState  (SWOS2.s line 9093)
 *
 * Initializes the audio system state from a loaded audio bank.
 *   bankBase: pointer to loaded instrument/sample bank
 *   songPtr:  pointer to loaded song data
 *
 * Parses bank header to extract 7 section pointers, sets default
 * volumes/modes, relocates bank descriptor pointers, and
 * initializes all 4 audio channel structures with hardware
 * register base addresses and DMA bits.
 * ================================================================ */
__attribute__((used, externally_visible))
void _InitAudioState(uint8_t *bankBase, uint8_t *songPtr)
{
#ifdef SWOS_SDL_BUILD
    /* The real init parses the bank header byte-for-byte, but the whole
     * bank layout is 68k big-endian. Until we add proper byte-swap
     * support across audio.c / audio_irq.c, just zero the state and
     * bail so the loader-intro music path doesn't crash. Menu music
     * stays silent on SDL for now. */
    (void)bankBase;
    (void)songPtr;
    /* Clear audio state so later ISR tick doesn't see garbage. */
    {
        uint8_t *p = (uint8_t *)&audioState;
        for (unsigned i = 0; i < sizeof(audioState); ++i) p[i] = 0;
    }
    audioState.masterVolume = 0;
    audioState.modeFlags    = 0;

    /* Even though we don't parse the bank, the IRQ ticks will run and
     * may dispatch to channels later (e.g. after _AudioSetMode flips
     * the enable bytes). hwRegBase must point at valid storage or the
     * AUDx writes inside loadAUDxPtrLenIfReloadNeeded* SIGSEGV. The
     * AUDxLC_ADDR macros resolve to (custom + 0xA0/B0/C0/D0) which is
     * already-allocated s_custom_storage on the SDL host. */
    ensureChannelPtrs();

    clearAudioChannelStruct(&audioChannel0);
    audioChannel0.hwRegBase = AUD0LC_ADDR;
    audioChannel0.pvDmaBit = 1;
    audioChannel0.svDmaBit = 1;

    clearAudioChannelStruct(&audioChannel1);
    audioChannel1.hwRegBase = AUD1LC_ADDR;
    audioChannel1.pvDmaBit = 2;
    audioChannel1.svDmaBit = 2;

    clearAudioChannelStruct(&audioChannel2);
    audioChannel2.hwRegBase = AUD2LC_ADDR;
    audioChannel2.pvDmaBit = 4;
    audioChannel2.svDmaBit = 4;

    clearAudioChannelStruct(&audioChannel3);
    audioChannel3.hwRegBase = AUD3LC_ADDR;
    audioChannel3.pvDmaBit = 8;
    audioChannel3.svDmaBit = 8;
    return;
#else
    uint8_t *src;
    int16_t i;
    uint8_t *descriptors;
    uint8_t *relocBase;
    uint16_t descriptorSize;

    /* songPtr + 4 -> audioState.songPtr */
    audioState.songPtr = songPtr + 4;

    /* Parse bank header: skip 8-byte header, then 7 sections
     * each prefixed by a 4-byte size field. The bank format is
     * native big-endian from the Amiga disk, so on little-endian
     * hosts we must assemble the size byte-by-byte. */
    src = bankBase + 8;
    for (i = 0; i < 7; i++)
    {
        /* Bank section-size word is big-endian; BE_READ_U32 is a no-op
         * on Amiga BE host, byte-swaps on x86 LE. */
        uint32_t sectionSize = BE_READ_U32(src);
        src += 4;
        audioState.bankSectionPtrs[i] = src;
        src += sectionSize;
    }

    /* Default master volume and mode */
    audioState.masterVolume = 0x40;
    audioState.channelVolume[0] = 0x40;
    audioState.channelVolume[1] = 0x40;
    audioState.channelVolume[2] = 0x40;
    audioState.channelVolume[3] = 0x40;
    audioState.modeFlags = 0x000D;
    audioState.irqSubtick = 0;
    audioState.toggleFlag = 0;
    audioState.primaryChanEnable = 0xFF;
    audioState.secondaryChanEnable = 0xFF;

    /* Relocate bank descriptors:
     * descriptors = bankSectionPtrs[0]
     * relocBase = songPtr (after +4 offset)
     * descriptorSize = word before descriptors (at descriptors[-4])
     * Each descriptor is 0x20 bytes with 3 pointers to relocate. */
    descriptors = audioState.bankSectionPtrs[0];
    relocBase = audioState.songPtr;
    /* asm: move.l -4(a2),d1 loads 32-bit, but subi.w #$20,d1 / bne
     * tests only the low 16 bits. Read low word of the 32-bit field. */
    descriptorSize = (uint16_t)(*(uint32_t *)(descriptors - 4));

    while (descriptorSize != 0)
    {
        uint32_t val;

        /* Relocate ptr at +0x00 (always nonzero) */
        val = *(uint32_t *)(descriptors + 0);
        val += (uint32_t)relocBase;
        *(uint32_t *)(descriptors + 0) = val;

        /* Relocate ptr at +0x04 (skip if zero) */
        val = *(uint32_t *)(descriptors + 4);
        if (val != 0)
            val += (uint32_t)relocBase;
        *(uint32_t *)(descriptors + 4) = val;

        /* Relocate ptr at +0x08 (skip if zero) */
        val = *(uint32_t *)(descriptors + 8);
        if (val != 0)
            val += (uint32_t)relocBase;
        *(uint32_t *)(descriptors + 8) = val;

        /* Double the word at +0x10 */
        *(uint16_t *)(descriptors + 0x10) *= 2;

        descriptors += 0x20;
        descriptorSize -= 0x20;
    }

    /* Initialize all 4 audio channels */
    ensureChannelPtrs();

    clearAudioChannelStruct(&audioChannel0);
    audioChannel0.hwRegBase = AUD0LC_ADDR;
    audioChannel0.pvDmaBit = 1;
    audioChannel0.svDmaBit = 1;

    clearAudioChannelStruct(&audioChannel1);
    audioChannel1.hwRegBase = AUD1LC_ADDR;
    audioChannel1.pvDmaBit = 2;
    audioChannel1.svDmaBit = 2;

    clearAudioChannelStruct(&audioChannel2);
    audioChannel2.hwRegBase = AUD2LC_ADDR;
    audioChannel2.pvDmaBit = 4;
    audioChannel2.svDmaBit = 4;

    clearAudioChannelStruct(&audioChannel3);
    audioChannel3.hwRegBase = AUD3LC_ADDR;
    audioChannel3.pvDmaBit = 8;
    audioChannel3.svDmaBit = 8;
#endif /* !SWOS_SDL_BUILD */
}

/* ================================================================
 * AudioDisableDMAForChannel  (SWOS2.s line 9426)
 *
 * Disables DMA for the specified audio channel by writing its
 * DMA bit to DMACON (without SET bit, so it clears).
 * ================================================================ */
__attribute__((used, externally_visible))
void _AudioDisableDMAForChannel(AudioChannel *chan)
{
    custom->dmacon = chan->pvDmaBit;
}

/* ================================================================
 * AudioChannelNoteOnOff  (SWOS2.s line 9270)
 *
 * Starts or stops a note on a specific audio channel.
 * If noteId is 0, stops the channel (disables DMA, clears enabled flag).
 * Otherwise, looks up note data and wave data from the audio bank
 * and sets up the channel for playback.
 *
 * Params: channel 0-3, noteId (0 = stop, nonzero = start note)
 * ================================================================ */
__attribute__((used, externally_visible))
void _AudioChannelNoteOnOff(uint16_t channel, uint16_t noteId)
{
    AudioChannel *chan;
    uint8_t *noteTable;
    uint32_t noteOffset;
    uint8_t *noteData;
    uint8_t waveIdx;
    uint32_t waveOffset;
    uint8_t *waveTable;
    uint8_t *waveData;

    ensureChannelPtrs();

    /* Set channel change bit */
    audioState.channelChangeFlags |= (uint8_t)(1 << (channel & 3));

    /* Get channel pointer */
    chan = audioChannels[channel & 3];

    if (noteId == 0)
    {
        /* Stop channel */
        chan->pvEnabled = 0;
        _AudioDisableDMAForChannel(chan);
        return;
    }

    /* Look up note data from bank section 3 */
    noteTable = audioState.bankSectionPtrs[3];  /* offset 0x10 from state */
    noteOffset = noteId * 4;
    noteData = *(uint8_t **)(noteTable + noteOffset);

    /* Access: relocate pointer (add bankSectionPtrs[5], mask to 24-bit) */
    noteData = accessRelocate(noteData);

    /* First byte of note data is the wave index */
    waveIdx = *noteData++;
    chan->pvNoteScript = (uint32_t)noteData;

    /* Look up wave data from bank section 4, relocate with section 6 */
    waveTable = audioState.bankSectionPtrs[4];  /* offset 0x14 from state */
    waveOffset = (uint32_t)waveIdx * 4;
    waveData = *(uint8_t **)(waveTable + waveOffset);
    waveData = (uint8_t *)((uint32_t)waveData + (uint32_t)audioState.bankSectionPtrs[6]);

    chan->pvWaveData = (uint32_t)waveData;
    chan->pvStepPeriod = 6;
    chan->pvScriptTimer = 1;
    chan->pvPeriodCounter = 1;
    chan->pvEnabled = 0xFF;
}

/* ================================================================
 * AudioApplyPresetToChannels  (SWOS2.s line 9236)
 *
 * Applies a preset instrument/note configuration to all 4 channels.
 * Looks up preset data from audioPresetChannelCmdTable (bankSectionPtrs[2]).
 * Each preset is 4 bytes (one per channel). Zero = skip, nonzero = note ID.
 * ================================================================ */
__attribute__((used, externally_visible))
void _AudioApplyPresetToChannels(uint16_t presetId)
{
#ifdef SWOS_SDL_BUILD
    /* Walk the preset table just like the Amiga byte-poking engine:
     * one 4-byte preset record = one noteId per channel; each non-zero
     * noteId fires a sample on its channel after a song_data[3]+[5]
     * indirection. AudioSDL_TriggerSfxPreset does the whole walk. */
    extern void AudioSDL_TriggerSfxPreset(int preset_id);
    AudioSDL_TriggerSfxPreset((int)(presetId & 0x3F));   /* 6-bit preset id, see Amiga &= 0x3F */
    return;
#else
    uint8_t *presetTable;
    uint8_t *preset;
    uint16_t noteId;

    presetId &= 0x3F;

    /* bankSectionPtrs[2] is the preset table pointer */
    presetTable = audioState.bankSectionPtrs[2];
    preset = presetTable + (uint32_t)presetId * 4;

    /* Channel 0 */
    noteId = preset[0];
    if (noteId != 0)
        _AudioChannelNoteOnOff(0, noteId);

    /* Channel 1 */
    noteId = preset[1];
    if (noteId != 0)
        _AudioChannelNoteOnOff(1, noteId);

    /* Channel 2 */
    noteId = preset[2];
    if (noteId != 0)
        _AudioChannelNoteOnOff(2, noteId);

    /* Channel 3 */
    noteId = preset[3];
    if (noteId != 0)
        _AudioChannelNoteOnOff(3, noteId);
#endif /* !SWOS_SDL_BUILD */
}

/* ================================================================
 * AudioChannelQueueNextWaveIfEnabled  (SWOS2.s line 9308)
 *
 * Queues a next wave for playback on a channel, but only if
 * secondary channel processing (modeFlags bit 2) is enabled.
 *
 * Params: channel 0-3, waveId (index into bank section 4)
 * ================================================================ */
__attribute__((used, externally_visible))
void _AudioChannelQueueNextWaveIfEnabled(uint16_t channel, uint16_t waveId)
{
#ifdef SWOS_SDL_BUILD
    /* Mirror Amiga: bankSectionPtrs[4]+[6] indirection on waveId.
     * Without this, kicks come out as whistles and vice-versa.
     * AudioSDL_TriggerSfxByWaveId does the lookup + trigger. */
    extern void AudioSDL_TriggerSfxByWaveId(int channel, int wave_id);
    AudioSDL_TriggerSfxByWaveId((int)(channel & 3), (int)waveId);
    return;
#else
    AudioChannel *chan;
    uint8_t *waveTable;
    uint32_t waveOffset;
    uint8_t *waveData;

    /* Check modeFlags bit 2 (audioFlags bit 2 in asm) */
    if (!(audioState.modeFlags & 0x04))
        return;

    ensureChannelPtrs();

    /* Set channel change bit (upper nibble: bit d0+4) */
    audioState.channelChangeFlags |= (uint8_t)(1 << ((channel & 3) + 4));

    chan = audioChannels[channel & 3];

    /* Look up wave data */
    waveTable = audioState.bankSectionPtrs[4];
    waveOffset = (uint32_t)waveId * 4;
    waveData = *(uint8_t **)(waveTable + waveOffset);
    waveData = (uint8_t *)((uint32_t)waveData + (uint32_t)audioState.bankSectionPtrs[6]);

    chan->svWaveData = (uint32_t)waveData;
    chan->svStepPeriod = 6;
    chan->svScriptTimer = 1;
    chan->svPeriodCounter = 1;
#endif /* !SWOS_SDL_BUILD */
}

/* ================================================================
 * AudioBeepAllShort  (SWOS2.s line 10978)
 *
 * Fades audio volume from 0x40 down to 0, waiting 2 game ticks
 * per step. Then sets final volume to 0.
 * ================================================================ */
__attribute__((used, externally_visible))
void _AudioBeepAllShort(void)
{
    int16_t vol;

    for (vol = 0x40; vol >= 0; vol--)
    {
        int16_t i;

        _AudioSetChannelVolume(0xFFFF, (uint16_t)vol);

        /* 2 vblanks per step. _WaitForVSync (vs. raw spin on currentGameTick)
         * keeps AudioSDL_Tick fed during the fade so the queue doesn't drain. */
        for (i = 1; i >= 0; i--) {
            _WaitForVSync();
        }
    }

    _AudioSetChannelVolume(0xFFFF, 0);
}

/* ================================================================
 * InitMenuMusic  (SWOS2.s line 11167)
 * ================================================================ */
__attribute__((used, externally_visible))
void _InitMenuMusic(void)
{
retry:
    audioBankLoaded = 0;
    audioActive = 0;
    if (_PlaySong(aSoundMenuSng, menuSongData) != 0)
        goto retry;
    if (_PlaySong(aSoundMenuIns, menuInsBank) != 0)
        goto retry;
}

/* ================================================================
 * TryPlayMusic  (SWOS2.s line 11189)
 * ================================================================ */
__attribute__((used, externally_visible))
void _TryPlayMusic(void)
{
    uint16_t vol;
    uint16_t tick;

    /* Don't kick off music if the user just hit the X button. Without
     * this gate, pressing X mid-match causes the menu unwind to call
     * _CycleMenuMusic → _TryPlayMusic, starting the menu song just
     * before the process exits — audible as a brief musical blip. */
    extern uint16_t g_quitToDos;
    if (g_quitToDos != 0)
        return;

    if (audioActive != 0)
        return;

#ifdef SWOS_SDL_BUILD
    /* On SDL the RJP1 player produces samples directly into the SDL
     * audio callback. Load (once) and start subsong 1, then fade in via
     * the master-volume ramp. */
    if (audioBankLoaded == 0) {
        if (AudioSDL_StartSong(menuSongData, MENU_SONG_DATA_BYTES,
                               menuInsBank,  MENU_INS_BANK_BYTES, 1) != 0) {
            /* Load failed — keep audio silent but mark "loaded" so we
             * don't hammer the loader every vblank. */
        }
        audioBankLoaded = 1;
    }

    _AudioSetChannelVolume(0xFFFF, 0);
    audioActive = 1;

    for (vol = 0; vol <= 0x22; vol += 2) {
        _AudioSetChannelVolume(0xFFFF, vol);
        _WaitForVSync();
    }
    _AudioSetChannelVolume(0xFFFF, 0x23);
    return;
#else
    if (audioBankLoaded == 0)
    {
        _StopAllAudioChannels();
        _InitAudioState(menuSongData, menuInsBank);
        audioBankLoaded = 1;
    }

    _AudioSetChannelVolume(0xFFFF, 0);
    _AudioApplyPresetToChannels(0);
    audioActive = 1;

    for (vol = 0; vol <= 0x22; vol += 2)
    {
        _AudioSetChannelVolume(0xFFFF, vol);
        _WaitForVSync();
    }

    _AudioSetChannelVolume(0xFFFF, 0x23);
#endif
}

/* ================================================================
 * ClearAudioActiveFlag  (SWOS2.s line 10916)
 * ================================================================ */
__attribute__((used, externally_visible))
void _ClearAudioActiveFlag(void)
{
    audioActive = 0;
}

/* ================================================================
 * StopAudioAndClearFlags  (SWOS2.s line 10922)
 * ================================================================ */
__attribute__((used, externally_visible))
void _StopAudioAndClearFlags(void)
{
    _StopAllAudioChannels();
    audioActive = 0;
    audioBankLoaded = 0;
}

/* ================================================================
 * LoadSfxResources  (SWOS2.s line 10880)
 * ================================================================ */
__attribute__((used, externally_visible))
void _LoadSfxResources(void)
{
retry:
    _StopAllAudioChannels();
    audioBankLoaded = 0;
    if (_PlaySong(aSoundSfxSng, sfxSongData) != 0)
        goto retry;
    if (_PlaySong(aSoundSfxIn1, poolplyrDatBuffer) != 0)
        goto retry;
    if (_PlaySong(aSoundSfxIn2, sfxInsBank2) != 0)
        goto retry;

#ifdef SWOS_SDL_BUILD
    /* Hand the loaded bank to the RJP1 player so AudioSDL_TriggerSfx
     * (called from _AudioApplyPresetToChannels / _AudioChannelQueueNextWaveIfEnabled)
     * has samples to fire. The Amiga byte-poking engine in audio_irq.c
     * is stubbed on SDL — this is what makes in-match SFX audible. */
    AudioSDL_LoadSfxBank(sfxSongData, SFX_SONG_DATA_BYTES,
                         poolplyrDatBuffer, SFX_INS_BANK_BYTES);
#endif
}

/* ================================================================
 * EnsureSfxLoadedAndStart  (SWOS2.s line 10899)
 * ================================================================ */
__attribute__((used, externally_visible))
void _EnsureSfxLoadedAndStart(void)
{
    if (audioBankLoaded == 0)
    {
        _StopAllAudioChannels();
        _InitAudioState(sfxSongData, poolplyrDatBuffer);
        audioBankLoaded = 1;
    }
    _AudioSetChannelVolume(0xFFFF, 0x40);
    audioActive = 1;
}

/* ================================================================
 * StopMusic  (SWOS2.s line 10947)
 * ================================================================ */
__attribute__((used, externally_visible))
void _StopMusic(void)
{
    int16_t vol;

    if (audioActive == 0)
        return;

    for (vol = 0x23; vol >= 0; vol--)
    {
        int16_t i;

        _AudioSetChannelVolume(0xFFFF, (uint16_t)vol);

        for (i = 1; i >= 0; i--) {
            _WaitForVSync();
        }
    }

    _AudioSetChannelVolume(0xFFFF, 0);
    audioActive = 0;
#ifdef SWOS_SDL_BUILD
    AudioSDL_StopSong();
    audioBankLoaded = 0;   /* next _TryPlayMusic will re-load the song */
#endif
}

/* ================================================================
 * CycleMenuMusic  (SWOS2.s line 69209)
 * ================================================================ */
__attribute__((used, externally_visible))
void _CycleMenuMusic(void)
{
    extern uint16_t menuMusic;
    if (menuMusic == 0)
        _StopMusic();
    else
        _TryPlayMusic();
}

/* ================================================================
 * TryRestartingMusic  (SWOS2.s line 10935)
 * ================================================================ */
__attribute__((used, externally_visible))
void _TryRestartingMusic(void)
{
    if (audioActive == 0)
        _TryPlayMusic();
    else
        _StopMusic();
}
