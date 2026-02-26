#include "swos_sections.h"
#include "swos_asm.h"

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;

#define SWOS_CHIPMEMSIZE 0x00200000u
#define SWOS_FASTMEMSIZE 0x00200000u
#define SWOS_CHIPBITS (1<<(SWOS_CHIPMEMSIZE/0x80000))-1
#define SWOS_FASTBITS ((1<<(SWOS_FASTMEMSIZE/0x80000))-1)<<24
#define SWOS_MAGIC_VALUE 0x12345678u
#define SWOS_FRONTBUFFER_SIZE_TOTAL_BYTES 48048u

#define COLOR00_ADDR 0x00DFF180u
#define DMACON_ADDR 0x00DFF096u
#define INTENA_ADDR 0x00DFF09Au
#define CIAA_PRA_ADDR 0x00BFE001u
#define CIAA_DDRA_ADDR 0x00BFE201u
#define CIAA_ICR_ADDR 0x00BFED01u
#define DMACON_BLT_OFF 1024u

#define LEVEL_2_INTERRUPT_AUTOVECTOR 0x68u
#define LEVEL_3_INTERRUPT_AUTOVECTOR 0x6Cu
#define LEVEL_4_INTERRUPT_AUTOVECTOR 0x70u

extern void SafeStart(void);
extern void LoadDosLibrary(void);
extern void DetachOSDisplay(void);
extern void InitHardwareAtFrameStart(void);
extern void SetupInterrupts(void);
extern void Int2KeyboardHandler(void);
extern void Int3TimerHandler(void);
extern void Int4Handler(void);

extern uint32_t S_VBR;
extern uint32_t loaderBankBitmap;
extern uint16_t chipRamBitmapEnd;
extern uint16_t pitchDatBuffer;
extern uint16_t currentMenu;
extern uint32_t magicValue;
extern uint32_t loaderMemTop;
extern uint32_t loaderParam;
extern uint16_t frontbufferBase;
extern uint16_t frontbufferBaseAlt;
extern uint16_t frontbufferMaskBuffer;

SWOS_CHIP_BSS volatile uint32_t g_swos_chip_bss_anchor;
SWOS_FAST_BSS volatile uint32_t g_swos_fast_bss_anchor;

static void set_vector(uint32_t vbr, uint32_t offset, void (*handler)(void))
{
    volatile uint32_t *vector = (volatile uint32_t *)(vbr + offset);
    *vector = (uint32_t)handler;
}

__attribute__((used, externally_visible)) void _start(void)
{
    SafeStart();
    LoadDosLibrary();
    DetachOSDisplay();
    InitHardwareAtFrameStart();

    uint32_t bank_bitmap_ptr = (uint32_t)&loaderBankBitmap;
    uint32_t bank_bitmap_end = (uint32_t)&chipRamBitmapEnd;
    do
    {
        *(volatile uint16_t *)bank_bitmap_ptr = 0x5555u;
        bank_bitmap_ptr += 2u;
    } while (bank_bitmap_ptr != bank_bitmap_end);

    uint32_t pitch_ptr = (uint32_t)&pitchDatBuffer;
    uint32_t menu_ptr = (uint32_t)&currentMenu;
    do
    {
        *(volatile uint16_t *)pitch_ptr = 0xBBBBu;
        pitch_ptr += 2u;
    } while (pitch_ptr != menu_ptr);

    magicValue = SWOS_MAGIC_VALUE;
    loaderMemTop = SWOS_CHIPMEMSIZE;
    *(volatile uint32_t *)&loaderBankBitmap = (SWOS_CHIPBITS | SWOS_FASTBITS);
    loaderParam = SWOS_FASTBITS;

    *(volatile uint16_t *)COLOR00_ADDR = 0xF00u;

    volatile uint16_t *front = &frontbufferBase;
    volatile uint16_t *alt = &frontbufferBaseAlt;
    volatile uint16_t *mask = &frontbufferMaskBuffer;

    for (uint32_t bytes = SWOS_FRONTBUFFER_SIZE_TOTAL_BYTES; bytes != 0u; bytes -= 2u)
    {
        *front++ = 0xF0F0u;
        *alt++ = 0xF0F0u;
        *mask++ = 0xF0F0u;
    }

    *(volatile uint16_t *)DMACON_ADDR = 0x8340u;
    *(volatile uint16_t *)DMACON_ADDR = DMACON_BLT_OFF;

    set_vector(S_VBR, LEVEL_2_INTERRUPT_AUTOVECTOR, Int2KeyboardHandler);
    set_vector(S_VBR, LEVEL_3_INTERRUPT_AUTOVECTOR, Int3TimerHandler);
    set_vector(S_VBR, LEVEL_4_INTERRUPT_AUTOVECTOR, Int4Handler);

    SetupInterrupts();

    *(volatile uint8_t *)CIAA_ICR_ADDR = 0x88u;
    *(volatile uint16_t *)INTENA_ADDR = 0xC008u;
    *(volatile uint8_t *)CIAA_ICR_ADDR = 0x82u;
    *(volatile uint8_t *)CIAA_DDRA_ADDR |= 0x02u;
    *(volatile uint8_t *)CIAA_PRA_ADDR |= 0x02u;
}
