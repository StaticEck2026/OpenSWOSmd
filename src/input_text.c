/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/*
 * input_text.c — PrintFormatted + InputText family
 * Ported from SWOS2.s (Chunk 36)
 */

#include "input_text.h"
#include "menu_system.h"
#include "string_utils.h"

/* ---- External globals ---- */
extern volatile uint16_t panicReasonCode;
extern uint8_t inMenuKeyboardBuffer[100];
extern uint16_t currentKey;
extern volatile uint16_t shortFire;
extern uint16_t deadVar02;
extern uint16_t deadInputTextVar;
extern uint16_t allowExtraCharsFlag;
extern uint8_t inputAllowedControlKeys[];

/* ---- External functions ---- */
extern void _PanicHaltLoop(void);
extern void _MenuCheckControls(void);
extern void _CallDrawMenuItem(MenuEntry *entry);

/* ============================================================
 * _PrintFormatted
 * Format string printer: %0..%6 = decimal numbers, %a/%b/%c = strings.
 * Uses goto-based fall-through to match 68k digit emission behavior
 * (once we enter at a digit level, all lower digits are always emitted).
 * ============================================================ */
__attribute__((used, externally_visible))
void _PrintFormatted(const uint8_t *fmt, uint8_t *out,
                     uint16_t n0, uint16_t n1, uint16_t n2, uint16_t n3,
                     uint16_t n4, uint16_t n5, uint16_t n6,
                     const uint8_t *strA, const uint8_t *strB, const uint8_t *strC)
{
    for (;;) {
        /* Copy bytes until '%' or NUL */
        while (*fmt != '%') {
            uint8_t c = *fmt++;
            *out++ = c;
            if (c == 0) {
                return;
            }
        }

        /* Skip '%' and read the specifier character */
        fmt++;
        uint8_t spec = *fmt++;

        /* String substitution: %a, %b, %c */
        if (spec == 'a' || spec == 'b' || spec == 'c') {
            const uint8_t *src;
            if (spec == 'a')
                src = strA;
            else if (spec == 'b')
                src = strB;
            else
                src = strC;

            while (*src != 0) {
                *out++ = *src++;
            }
            continue;
        }

        /* Numeric substitution: %0..%6 */
        uint16_t val;
        if (spec == '0')      val = n0;
        else if (spec == '1') val = n1;
        else if (spec == '2') val = n2;
        else if (spec == '3') val = n3;
        else if (spec == '4') val = n4;
        else if (spec == '5') val = n5;
        else if (spec == '6') val = n6;
        else {
            panicReasonCode = 0x5916u;
            _PanicHaltLoop();
            return;
        }

        /* Convert number to decimal digits.
         * Asm uses fall-through: once we enter at a digit level,
         * all lower digits are always emitted (including zeros).
         * e.g. val=50 → "50", val=1050 → "1050", val=100 → "100" */
        if (val < 10)    goto emit_units;
        if (val < 100)   goto emit_tens;
        if (val < 1000)  goto emit_hundreds;
        if (val < 10000) goto emit_thousands;

        /* 10000+ : andi.l #$FFFF,d7; divu.w #10000 */
        *out++ = (uint8_t)(val / 10000) + '0';
        val = val % 10000;

    emit_thousands:
        *out++ = (uint8_t)(val / 1000) + '0';
        val = val % 1000;

    emit_hundreds:
        *out++ = (uint8_t)(val / 100) + '0';
        val = val % 100;

    emit_tens:
        *out++ = (uint8_t)(val / 10) + '0';
        val = val % 10;

    emit_units:
        *out++ = (uint8_t)val + '0';
    }
}

/* ============================================================
 * _InputTextFilterKey
 * Filters currentKey to allowed charset.
 * Returns: key value if allowed, 0 if not.
 * ============================================================ */
__attribute__((used, externally_visible))
uint16_t _InputTextFilterKey(void)
{
    uint16_t key = currentKey;

    /* A-Z allowed */
    if (key >= 'A' && key <= 'Z')
        return key;

    /* 0-9 allowed */
    if (key >= '0' && key <= '9')
        return key;

    /* When allowExtraCharsFlag is set, '.' and '/' are rejected
     * (intercepted before the table scan where they'd normally match) */
    if (allowExtraCharsFlag != 0) {
        if ((uint8_t)key == '.' || (uint8_t)key == '/')
            return 0;
    }

    /* Scan allowed control keys table (terminated by 0xFF) */
    {
        uint8_t *p = inputAllowedControlKeys;
        while (*p != 0xFF) {
            if (*p == (uint8_t)key)
                return key;
            p++;
        }
    }

    return 0;
}

/* ============================================================
 * _InsertCharacter
 * Inserts ch at pos within inMenuKeyboardBuffer, shifting bytes right.
 * Note: GetStringLength stub returns a0 = buffer + len (at NUL),
 * so the shift starts from the NUL and moves everything right.
 * ============================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _InsertCharacter(uint8_t *pos, uint8_t ch)
{
    uint16_t len = _GetStringLength(inMenuKeyboardBuffer);
    uint8_t *p = inMenuKeyboardBuffer + len; /* points to NUL (matching asm stub: adda.w d0,a0) */

    /* Shift bytes right from p down to pos (do-while matches asm: shift first, then check) */
    do {
        *(p + 1) = *p;
        p--;
    } while (p >= pos);

    /* Write ch at pos (p is now one below pos, so p+1 == pos) */
    *(p + 1) = ch;
}

/* ============================================================
 * _InputTextDeleteCharAtCursor
 * Deletes char at pos, shifting remaining chars left.
 * ============================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _InputTextDeleteCharAtCursor(uint8_t *pos)
{
    if (*pos == 0)
        return;

    uint8_t *p = pos;
    do {
        *p = *(p + 1);
        p++;
    } while (*(p - 1) != 0);
}

/* ============================================================
 * _InputText
 * Interactive text editor using the menu system.
 * maxLenPlus1: max input length + 1 (for NUL), must be <= 99
 * entryRaw: pointer to MenuEntry (passed as uint8_t* from asm)
 * Returns: 0 = committed (enter/fire), 1 = cancelled (escape)
 * ============================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
uint16_t _InputText(uint16_t maxLenPlus1, uint8_t *entryRaw)
{
    MenuEntry *entry = (MenuEntry *)entryRaw;

    deadVar02 = 1;

    if (maxLenPlus1 > 99) {
        panicReasonCode = 0x1197u;
        _PanicHaltLoop();
        return 0;
    }

    uint16_t maxLen = maxLenPlus1 - 1;

    /* Save original string pointer, copy to edit buffer */
    uint8_t *origStr = (uint8_t *)entry->fg;
    _StringCopy(origStr, inMenuKeyboardBuffer);
    entry->fg = (uint32_t)inMenuKeyboardBuffer;

    /* Cursor starts at end of string (at the NUL terminator).
     * Asm: GetStringLength stub sets a0 = buffer + len (at NUL). */
    uint16_t initLen = _GetStringLength(inMenuKeyboardBuffer);
    uint8_t *pos = inMenuKeyboardBuffer + initLen;

    deadInputTextVar = 0;

    for (;;) {
        _MenuCheckControls();

        if (shortFire != 0)
            goto commit;

        uint16_t key = _InputTextFilterKey();

        if (key == 0) {
            goto insert_cursor;
        }

        if (key == 13) { /* Enter — commit */
            goto commit;
        }

        if (key == 19) { /* Left shift — move cursor left */
            if (pos > inMenuKeyboardBuffer) {
                pos--;
            }
            continue;
        }

        if (key == 18) { /* Right shift — move cursor right */
            if (*pos != 0) {
                pos++;
            }
            continue;
        }

        if (key == 27) { /* Escape — cancel */
            entry->fg = (uint32_t)origStr;
            _CallDrawMenuItem(entry);
            deadVar02 = 0;
            return 1;
        }

        if (key == 127) { /* Backspace — delete char to the left */
            if (pos > inMenuKeyboardBuffer) {
                pos--;
                _InputTextDeleteCharAtCursor(pos);
            }
            goto insert_cursor;
        }

        if (key == 126) { /* 0x7E — delete at cursor (like Delete key) */
            _InputTextDeleteCharAtCursor(pos);
            goto insert_cursor;
        }

        /* Printable character (0x20..0x7F) — insert if not full */
        if (key >= 0x20 && key <= 0x7F) {
            uint16_t curLen = _GetStringLength(inMenuKeyboardBuffer);
            if (curLen != maxLen) {
                _InsertCharacter(pos, (uint8_t)key);
                pos++;
            }
        }

    insert_cursor:
        /* Show cursor: insert 0xFF at pos, draw, then remove it */
        _InsertCharacter(pos, 0xFF);
        _CallDrawMenuItem(entry);
        _InputTextDeleteCharAtCursor(pos);
    }

commit:
    _StringCopy(inMenuKeyboardBuffer, origStr);
    entry->fg = (uint32_t)origStr;
    _CallDrawMenuItem(entry);
    deadVar02 = 0;
    return 0;
}
