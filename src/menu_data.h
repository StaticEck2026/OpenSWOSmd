/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#pragma once

#include "game_types.h"

/* 30 */
typedef enum MenuEntryColors
{
  PINK_TO_BROWN_0 = 0x0,
  PINK_TO_BROWN_1 = 0x1,
  PINK_TO_BROWN_2 = 0x2,
  BLUE_TO_BLACK_3 = 0x3,
  ORANGE_TO_BROWN_4 = 0x4,
  ORANGE_TO_BROWN_5 = 0x5,
  ORANGE_TO_BROWN_6 = 0x6,
  PINK_TO_BROWN_7 = 0x7,
  BLUE_TO_BLACK_8 = 0x8,
  PINK_TO_BROWN_9 = 0x9,
  RED_10 = 0xA,
  BLUE_TO_PURPLE_11 = 0xB,
  ORANGE_TO_BROWN_12 = 0xC,
  BRIGHT_BLUE_TO_BLUE_13 = 0xD,
  YELLOW_GREEN_BLACK_14 = 0xE,
  YELLOW_GREEN_BLACK_15 = 0xF,
} MenuEntryColors;


typedef enum MenuCtrlMask
{
  MENU_CTRL_FIRE = 0x1,
  MENU_CTRL_LEFT_2 = 0x2,
  MENU_CTRL_RIGHT_2 = 0x4,
  MENU_CTRL_UP_2 = 0x8,
  MENU_CTRL_DOWN_2 = 0x10,
  MENU_CTRL_SHORT_FIRE = 0x20,
  MENU_CTRL_LEFT = 0x40,
  MENU_CTRL_RIGHT = 0x80,
  MENU_CTRL_UP = 0x100,
  MENU_CTRL_DOWN = 0x200,
  MENU_CTRL_HI_MASK = 0xFC00,
} MenuCtrlMask;


/* 31 */
typedef enum StringFlags
{
  STR_COLOR_MASK = 0xF,
  STR_FLAGS_MASK = 0xF0,
  STR_ALIGN_RIGHT = 0x4000,
  STR_ALIGN_LEFT = 0x8000,
} StringFlags;


/* 34 */
typedef enum MenuBackgroundColors
{
  COLOR_ORANGE = 6u,
  COLOR_DARK_BLUE = 8u,
  COLOR_PINK_GRAYISH = 9u,
  COLOR_RED = 10u,
  COLOR_PURPLISH = 11u,
  COLOR_LIGHT_BLUE = 13u,
  COLOR_GREEN = 14u,
  COLOR_YELLOW = 15u,
} MenuBackgroundColors;


/* 133 */
enum {
  MENU_CMD_END = 0x0,
  MENU_CMD_INVISIBLE = 0x1,
  MENU_CMD_NAV = 0x2,
  MENU_CMD_BG_FUNC = 0x3,
  MENU_CMD_COLORS = 0x4,
  MENU_CMD_BG_SPRITE = 0x5,
  MENU_CMD_FG_FUNC = 0x7,
  MENU_CMD_TEXT_PTR = 0x8,
  MENU_CMD_SPRITE_FG = 0x9,
  MENU_CMD_STRING_TABLE = 0xA,
  MENU_CMD_MULTILINE = 0xB,
  MENU_CMD_NUMBER = 0xC,
  MENU_CMD_ONSELECT_FIRE = 0xD,
  MENU_CMD_INPUT_FUNC = 0xE,
  MENU_CMD_BEFORE_DRAW = 0xF,
  MENU_CMD_AFTER_DRAW = 0x10,
  MENU_CMD_UP_SKIP = 0x11,
  MENU_CMD_DOWN_SKIP = 0x12,
  MENU_CMD_LEFT_SKIP = 0x13,
  MENU_CMD_RIGHT_SKIP = 0x14,
  MENU_CMD_SPRITE_COPY = 0x15,
  MENU_CMD_TEXT_NO_PTR = 0x16,
};
typedef uint16_t MenuCommand;


/* 134 */
enum {
  MENU_MARK_END_ENTRY = 0x0,
  MENU_MARK_END_MENU = 0xFC19,
  MENU_MARK_START_MENU = 0xFC1A,
  MENU_MARK_START_TEMPLATE = 0xFC1B,
  MENU_MARK_INIT_ENTRY = 0xFC1C,
  MENU_MARK_WIDE_COORDS = 0xFFFE,
  MENU_MARK_BYTE_COORDS = 0xFFFF,
};
typedef uint16_t MenuMark;

typedef uint16_t MenuColorFlags;
typedef uint16_t MenuStringFlags;

/* 135 */
SWOS_PACK_PUSH1
typedef struct MenuCmd_Colors
{
  uint16_t type;
  MenuColorFlags flags;
} MenuCmd_Colors;
SWOS_PACK_POP

/* 136 */
SWOS_PACK_PUSH1
typedef struct MenuCmd_TextPtr
{
  uint16_t type;
  MenuStringFlags strFlags;
  char *textPtr;
} MenuCmd_TextPtr;
SWOS_PACK_POP

/* 137 */
SWOS_PACK_PUSH1
typedef struct MenuCmd_TextNoPtr
{
  uint16_t type;
  MenuStringFlags strFlags;
} MenuCmd_TextNoPtr;
SWOS_PACK_POP

/* 138 */
SWOS_PACK_PUSH1
typedef struct MenuCmd_OnSelect
{
  uint16_t type;
  void *funcPtr;
} MenuCmd_OnSelect;
SWOS_PACK_POP

/* 139 */
SWOS_PACK_PUSH1
typedef struct MenuCmd_Nav
{
  uint16_t type;
  int8_t left;
  int8_t right;
  int8_t up;
  int8_t down;
} MenuCmd_Nav;
SWOS_PACK_POP

/* 140 */
SWOS_PACK_PUSH1
typedef struct MenuCmd_Skip2
{
  uint16_t type;
  int8_t arg0;
  int8_t arg1;
} MenuCmd_Skip2;
SWOS_PACK_POP

/* 141 */
SWOS_PACK_PUSH1
typedef struct MenuCoords16
{
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
} MenuCoords16;
SWOS_PACK_POP

/* 142 */
SWOS_PACK_PUSH1
typedef struct MenuCoords8
{
  int8_t x;
  int8_t y;
  int8_t w;
  int8_t h;
} MenuCoords8;
SWOS_PACK_POP

/* 143 */
SWOS_PACK_PUSH1
typedef struct MenuHeader
{
  void *onInitPtr;
  void *onReturnPtr;
  void *onDrawPtr;
  uint32_t defaultEntry;
} MenuHeader;
SWOS_PACK_POP

/* 145 */
SWOS_PACK_PUSH1
typedef struct MenuEntry_Byte_ColorsOnly
{
  MenuCoords8 rc;
  MenuCmd_Colors colors;
  uint16_t end;
} MenuEntry_Byte_ColorsOnly;
SWOS_PACK_POP

/* 146 */
SWOS_PACK_PUSH1
typedef struct MenuEntry_Byte_TextPtrOnly
{
  MenuCoords8 rc;
  MenuCmd_TextPtr text;
  uint16_t end;
} MenuEntry_Byte_TextPtrOnly;
SWOS_PACK_POP

/* 148 */
SWOS_PACK_PUSH1
typedef struct MenuEntry_Byte_NavOnly
{
  MenuCoords8 rc;
  MenuCmd_Nav nav;
  uint16_t end;
} MenuEntry_Byte_NavOnly;
SWOS_PACK_POP

/* 149 */
SWOS_PACK_PUSH1
typedef struct MenuEntry_Byte_Nav_Skip
{
  MenuCoords8 rc;
  MenuCmd_Nav nav;
  MenuCmd_Skip2 skip;
  uint16_t end;
} MenuEntry_Byte_Nav_Skip;
SWOS_PACK_POP

/* 151 */
SWOS_PACK_PUSH1
typedef struct MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect
{
  MenuCoords8 rc;
  MenuCmd_Nav nav;
  MenuCmd_Colors colors;
  MenuCmd_TextPtr text;
  MenuCmd_OnSelect onSelect;
  uint16_t end;
} MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect;
SWOS_PACK_POP

/* 155 */
SWOS_PACK_PUSH1
typedef struct MenuMark_StartMenu
{
  uint16_t mark;
  int16_t menuX;
  int16_t menuY;
} MenuMark_StartMenu;
SWOS_PACK_POP

/* 157 */
SWOS_PACK_PUSH1
typedef struct MenuCmd_Invisible
{
  uint16_t type;
} MenuCmd_Invisible;
SWOS_PACK_POP

/* 158 */
SWOS_PACK_PUSH1
typedef struct MenuCmd_Number
{
  uint16_t type;
  MenuStringFlags strFlags;
  int16_t value;
} MenuCmd_Number;
SWOS_PACK_POP

/* 162 */
SWOS_PACK_PUSH1
typedef struct MenuEntry_Byte_Colors_TextPtr
{
  MenuCoords8 rc;
  MenuCmd_Colors colors;
  MenuCmd_TextPtr text;
  uint16_t end;
} MenuEntry_Byte_Colors_TextPtr;
SWOS_PACK_POP

/* 167 */
SWOS_PACK_PUSH1
typedef struct MenuCmd_StringTable
{
  uint16_t type;
  MenuStringFlags strFlags;
  void *tablePtr;
} MenuCmd_StringTable;
SWOS_PACK_POP

/* 168 */
SWOS_PACK_PUSH1
typedef struct MenuEntry_Byte_Nav_Colors_StringTable_OnSelect
{
  MenuCoords8 rc;
  MenuCmd_Nav nav;
  MenuCmd_Colors colors;
  MenuCmd_StringTable strTab;
  MenuCmd_OnSelect onSelect;
  uint16_t end;
} MenuEntry_Byte_Nav_Colors_StringTable_OnSelect;
SWOS_PACK_POP

/* 170 */
SWOS_PACK_PUSH1
typedef struct MenuCmd_SpriteFg
{
  uint16_t type;
  int16_t spriteId;
} MenuCmd_SpriteFg;
SWOS_PACK_POP

/* 171 */
SWOS_PACK_PUSH1
typedef struct MenuCmd_InputFunc
{
  uint16_t type;
  uint16_t ctrlMask;
  void *funcPtr;
} MenuCmd_InputFunc;
SWOS_PACK_POP

/* 172 */
SWOS_PACK_PUSH1
typedef struct MenuCmd_BeforeDraw
{
  uint16_t type;
  void *funcPtr;
} MenuCmd_BeforeDraw;
SWOS_PACK_POP

/* 176 */
SWOS_PACK_PUSH1
typedef struct MenuEntry_Byte_Colors_Number
{
  MenuCoords8 rc;
  MenuCmd_Colors colors;
  MenuCmd_Number number;
  uint16_t end;
} MenuEntry_Byte_Colors_Number;
SWOS_PACK_POP

/* 177 */
SWOS_PACK_PUSH1
typedef struct MenuEntry_Byte_NumberOnly
{
  MenuCoords8 rc;
  MenuCmd_Number number;
  uint16_t end;
} MenuEntry_Byte_NumberOnly;
SWOS_PACK_POP

/* 179 */
SWOS_PACK_PUSH1
typedef struct MenuEntry_Byte_Colors_TextPtr_BeforeDraw
{
  MenuCoords8 rc;
  MenuCmd_Colors colors;
  MenuCmd_TextPtr text;
  MenuCmd_BeforeDraw beforeDraw;
  uint16_t end;
} MenuEntry_Byte_Colors_TextPtr_BeforeDraw;
SWOS_PACK_POP

/* 180 */
SWOS_PACK_PUSH1
typedef struct MenuEntry_Byte_Invisible_Colors_TextPtr
{
  MenuCoords8 rc;
  MenuCmd_Invisible invisible;
  MenuCmd_Colors colors;
  MenuCmd_TextPtr text;
  uint16_t end;
} MenuEntry_Byte_Invisible_Colors_TextPtr;
SWOS_PACK_POP

/* 186 */
SWOS_PACK_PUSH1
typedef struct Menu_DesignDIYCup_OptionRow
{
  MenuEntry_Byte_Colors_TextPtr descText;
  MenuEntry_Byte_Colors_Number descNum;
  MenuEntry_Byte_Nav_Colors_StringTable_OnSelect legs;
  MenuEntry_Byte_Nav_Colors_StringTable_OnSelect extraTime;
  MenuEntry_Byte_Nav_Colors_StringTable_OnSelect penalties;
} Menu_DesignDIYCup_OptionRow;
SWOS_PACK_POP

/* 188 */
SWOS_PACK_PUSH1
typedef struct MenuEntry_Byte_TextNoPtrOnly
{
  MenuCoords8 rc;
  MenuCmd_TextNoPtr text;
  uint16_t end;
} MenuEntry_Byte_TextNoPtrOnly;
SWOS_PACK_POP

/* 189 */
SWOS_PACK_PUSH1
typedef struct Menu_DIYCup_StatsBlock
{
  MenuMark_StartMenu menuStart;
  MenuEntry_Byte_TextPtrOnly entry0;
  MenuEntry_Byte_TextNoPtrOnly entries1_4[4];
  MenuEntry_Byte_NumberOnly entry5;
  MenuEntry_Byte_NumberOnly entry6;
  MenuEntry_Byte_TextNoPtrOnly entry7;
} Menu_DIYCup_StatsBlock;
SWOS_PACK_POP

/* 194 */
SWOS_PACK_PUSH1
typedef struct MenuEntry_Byte_SpriteFgOnly
{
  MenuCoords8 rc;
  MenuCmd_SpriteFg sprite;
  uint16_t end;
} MenuEntry_Byte_SpriteFgOnly;
SWOS_PACK_POP

/* 197 */
SWOS_PACK_PUSH1
typedef struct MenuEntry_Wide_NavOnly
{
  MenuCoords16 rc;
  MenuCmd_Nav nav;
  uint16_t end;
} MenuEntry_Wide_NavOnly;
SWOS_PACK_POP

/* 199 */
SWOS_PACK_PUSH1
typedef struct MenuEntry_Wide_TextNoPtrOnly
{
  MenuCoords16 rc;
  MenuCmd_TextNoPtr text;
  uint16_t end;
} MenuEntry_Wide_TextNoPtrOnly;
SWOS_PACK_POP

/* 205 */
SWOS_PACK_PUSH1
typedef struct MenuEntry_Byte_Invisible_NavOnly
{
  MenuCoords8 rc;
  MenuCmd_Invisible invisible;
  MenuCmd_Nav nav;
  uint16_t end;
} MenuEntry_Byte_Invisible_NavOnly;
SWOS_PACK_POP

/* 210 */
SWOS_PACK_PUSH1
typedef struct MenuEntry_Wide_NumberOnly
{
  MenuCoords16 rc;
  MenuCmd_Number number;
  uint16_t end;
} MenuEntry_Wide_NumberOnly;
SWOS_PACK_POP

/* 212 */
SWOS_PACK_PUSH1
typedef struct MenuEntry_Wide_MultilineText
{
  MenuCoords16 rc;
  MenuCmd_TextPtr text;
  uint16_t end;
} MenuEntry_Wide_MultilineText;
SWOS_PACK_POP

/* 214 */
SWOS_PACK_PUSH1
typedef struct MenuEntry_Byte_Nav_TextPtr
{
  MenuCoords8 rc;
  MenuCmd_Nav nav;
  MenuCmd_TextPtr text;
  uint16_t end;
} MenuEntry_Byte_Nav_TextPtr;
SWOS_PACK_POP

/* 239 */
SWOS_PACK_PUSH1
typedef struct MenuEntry_Byte_Nav
{
  MenuCoords8 rc;
  MenuCmd_Nav nav;
  uint16_t end;
} MenuEntry_Byte_Nav;
SWOS_PACK_POP

/* 240 */
SWOS_PACK_PUSH1
typedef struct MenuEntry_Wide_CoordsOnly
{
  MenuCoords16 rc;
  uint16_t end;
} MenuEntry_Wide_CoordsOnly;
SWOS_PACK_POP

/* 241 */
SWOS_PACK_PUSH1
typedef struct MenuEntry_Byte_CoordsOnly
{
  MenuCoords8 rc;
  uint16_t end;
} MenuEntry_Byte_CoordsOnly;
SWOS_PACK_POP

/* 242 */
SWOS_PACK_PUSH1
typedef struct MenuEntry_Wide_TextPtrOnly
{
  MenuCoords16 rc;
  MenuCmd_TextPtr text;
  uint16_t end;
} MenuEntry_Wide_TextPtrOnly;
SWOS_PACK_POP

/* 243 */
SWOS_PACK_PUSH1
typedef struct MenuEntry_Wide_ColorsOnly
{
  MenuCoords16 rc;
  MenuCmd_Colors colors;
  uint16_t end;
} MenuEntry_Wide_ColorsOnly;
SWOS_PACK_POP

/* 244 */
SWOS_PACK_PUSH1
typedef struct MenuCmd_AfterDraw
{
  uint16_t type;
  void *funcPtr;
} MenuCmd_AfterDraw;
SWOS_PACK_POP

/* 245 */
SWOS_PACK_PUSH1
typedef struct MenuCmd_SpriteCopy
{
  uint16_t type;
  void *sprPtr;
} MenuCmd_SpriteCopy;
SWOS_PACK_POP

/* 246 */
SWOS_PACK_PUSH1
typedef struct MenuCmd_FgFunc
{
  uint16_t type;
  void *funcPtr;
} MenuCmd_FgFunc;
SWOS_PACK_POP

/* 247 */
SWOS_PACK_PUSH1
typedef struct MenuCmd_BgFunc
{
  uint16_t type;
  void *funcPtr;
} MenuCmd_BgFunc;
SWOS_PACK_POP

/* 248 */
SWOS_PACK_PUSH1
typedef struct MenuEntry_Wide_SpriteCopyOnly
{
  MenuCoords16 rc;
  MenuCmd_SpriteCopy spriteCopy;
  uint16_t end;
} MenuEntry_Wide_SpriteCopyOnly;
SWOS_PACK_POP

/* 156 - Menu 1: chooseTeamsFriendlyMenu (0x3CA = 970 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_ChooseTeamsFriendly
{
  MenuHeader hdr;
  MenuCoords16 entry0_rc;
  MenuCmd_Colors entry0_colors;
  MenuCmd_TextNoPtr entry0_text;
  MenuCommand entry0_end;
  MenuMark markByteCoords;
  MenuEntry_Byte_ColorsOnly entries1_3[3];
  MenuEntry_Byte_TextPtrOnly entries4_6[3];
  MenuMark_StartMenu menu1Start;
  MenuMark template1_markStart;
  MenuCoords8 template1_rc;
  MenuCmd_Colors template1_colors;
  MenuCmd_TextPtr template1_text;
  MenuCmd_OnSelect template1_onSelect;
  MenuCommand template1_end;
  MenuEntry_Byte_Nav_Skip entry7;
  MenuEntry_Byte_Nav_Skip entries8_22[15];
  MenuEntry_Byte_NavOnly entries23_38[16];
  MenuEntry_Byte_Nav_Skip entry39;
  MenuEntry_Byte_Nav_Skip entries40_54[15];
  MenuMark markInitEntry;
  MenuMark_StartMenu menu2Start;
  MenuCoords8 entry55_rc;
  MenuCmd_Nav entry55_nav;
  MenuCmd_Skip2 entry55_skip[2];
  MenuCmd_Colors entry55_colors;
  MenuCmd_TextPtr entry55_text;
  MenuCmd_OnSelect entry55_onSelect;
  MenuCommand entry55_end;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry56;
  MenuCoords8 entry57_rc;
  MenuCmd_Nav entry57_nav;
  MenuCmd_Skip2 entry57_skip[2];
  MenuCmd_Colors entry57_colors;
  MenuCmd_TextNoPtr entry57_text;
  MenuCmd_OnSelect entry57_onSelect;
  MenuCommand entry57_end;
  MenuCoords8 entry58_rc;
  MenuCmd_Colors entry58_colors;
  MenuCmd_TextNoPtr entry58_text;
  MenuCommand entry58_end;
  MenuMark markEndMenu;
} Menu_ChooseTeamsFriendly;
SWOS_PACK_POP

/* 161 - Menu 2: chooseTeamsFriendlyMenu2 (0x4E4 = 1252 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_ChooseTeamsFriendly2
{
  MenuHeader hdr;
  MenuCoords16 entry0_rc;
  MenuCmd_Colors entry0_colors;
  MenuCmd_TextNoPtr entry0_text;
  MenuCommand entry0_end;
  MenuMark markByteCoords;
  MenuCoords8 entry1_rc;
  MenuCmd_Invisible entry1_invisible;
  MenuCmd_Colors entry1_colors;
  MenuCmd_Number entry1_number;
  MenuCommand entry1_end;
  MenuEntry_Byte_ColorsOnly entries2_4[3];
  MenuEntry_Byte_TextPtrOnly entries5_7[3];
  MenuMark_StartMenu menu1Start;
  MenuMark template1_markStart;
  MenuCoords8 template1_rc;
  MenuCmd_Colors template1_colors;
  MenuCmd_TextPtr template1_text;
  MenuCmd_OnSelect template1_onSelect;
  MenuCommand template1_end;
  MenuEntry_Byte_Nav_Skip entry8;
  MenuEntry_Byte_Nav_Skip entries9_29[21];
  MenuEntry_Byte_NavOnly entries30_51[22];
  MenuEntry_Byte_Nav_Skip entry52;
  MenuEntry_Byte_Nav_Skip entries53_73[21];
  MenuMark markInitEntry;
  MenuMark_StartMenu menu2Start;
  MenuCoords8 entry74_rc;
  MenuCmd_Nav entry74_nav;
  MenuCmd_Skip2 entry74_skip[2];
  MenuCmd_Colors entry74_colors;
  MenuCmd_TextPtr entry74_text;
  MenuCmd_OnSelect entry74_onSelect;
  MenuCommand entry74_end;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry75;
  MenuCoords8 entry76_rc;
  MenuCmd_Nav entry76_nav;
  MenuCmd_Skip2 entry76_skip[2];
  MenuCmd_Colors entry76_colors;
  MenuCmd_TextNoPtr entry76_text;
  MenuCmd_OnSelect entry76_onSelect;
  MenuCommand entry76_end;
  MenuCoords8 entry77_rc;
  MenuCmd_Colors entry77_colors;
  MenuCmd_TextNoPtr entry77_text;
  MenuCommand entry77_end;
  MenuMark markEndMenu;
} Menu_ChooseTeamsFriendly2;
SWOS_PACK_POP

/* 169 - Menu 3: designDiyLeagueMenu (0x25E = 606 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_DesignDIYLeague
{
  MenuHeader hdr;
  MenuMark markByteCoords0;
  MenuEntry_Byte_Colors_TextPtr entry0;
  MenuMark markWideCoords;
  MenuCoords16 entry1_rc;
  MenuCmd_Invisible entry1_invisible;
  MenuCmd_Colors entry1_colors;
  MenuCmd_TextPtr entry1_text;
  MenuCommand entry1_end;
  MenuMark_StartMenu menu1Start;
  MenuMark markByteCoords1;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry2;
  MenuEntry_Byte_Colors_TextPtr entries3_7[5];
  MenuEntry_Byte_Nav_Colors_StringTable_OnSelect entries8_10[3];
  MenuEntry_Byte_Colors_TextPtr entry11;
  MenuEntry_Byte_Nav_Colors_StringTable_OnSelect entry12;
  MenuCoords8 entry13_rc;
  MenuCmd_Nav entry13_nav;
  MenuCmd_Colors entry13_colors;
  MenuCmd_Number entry13_number;
  MenuCmd_OnSelect entry13_onSelect;
  MenuCommand entry13_end;
  MenuCoords8 entry14_rc;
  MenuCmd_Nav entry14_nav;
  MenuCmd_Colors entry14_colors;
  MenuCmd_TextNoPtr entry14_text;
  MenuCmd_OnSelect entry14_onSelect;
  MenuCommand entry14_end;
  MenuCoords8 entry15_rc;
  MenuCmd_Nav entry15_nav;
  MenuCmd_Colors entry15_colors;
  MenuCmd_Number entry15_number;
  MenuCmd_OnSelect entry15_onSelect;
  MenuCommand entry15_end;
  MenuCoords8 entry16_rc;
  MenuCmd_Nav entry16_nav;
  MenuCmd_Colors entry16_colors;
  MenuCmd_TextNoPtr entry16_text;
  MenuCmd_OnSelect entry16_onSelect;
  MenuCommand entry16_end;
  MenuCoords8 entry17_rc;
  MenuCmd_Nav entry17_nav;
  MenuCmd_Colors entry17_colors;
  MenuCmd_TextNoPtr entry17_text;
  MenuCmd_OnSelect entry17_onSelect;
  MenuCommand entry17_end;
  MenuCoords8 entry18_rc;
  MenuCmd_Invisible entry18_invisible;
  MenuCmd_Colors entry18_colors;
  MenuCmd_TextNoPtr entry18_text;
  MenuCommand entry18_end;
  MenuCoords8 entry19_rc;
  MenuCmd_Invisible entry19_invisible;
  MenuCmd_Colors entry19_colors;
  MenuCmd_TextNoPtr entry19_text;
  MenuCommand entry19_end;
  MenuMark_StartMenu menu2Start;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entries20_22[3];
  MenuCoords8 entry23_rc;
  MenuCmd_Colors entry23_colors;
  MenuCmd_TextNoPtr entry23_text;
  MenuCommand entry23_end;
  MenuMark markEndMenu;
} Menu_DesignDIYLeague;
SWOS_PACK_POP

/* 197 - Stage block for DesignDiyTournament with TextNoPtr status line (300 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_DesignDiyTournament_StageBlockA
{
  MenuMark_StartMenu startMenu;
  MenuEntry_Byte_NumberOnly stageNum;
  MenuEntry_Byte_SpriteFgOnly icon;
  MenuCoords8 qualifiers_rc;
  MenuCmd_Nav qualifiers_nav;
  MenuCmd_Colors qualifiers_colors;
  MenuCmd_Number qualifiers_number;
  MenuCmd_InputFunc qualifiers_input;
  MenuCommand qualifiers_end;
  MenuCoords8 fromValue_rc;
  MenuCmd_Nav fromValue_nav;
  MenuCmd_Colors fromValue_colors;
  MenuCmd_TextNoPtr fromValue_text;
  MenuCmd_OnSelect fromValue_onSelect;
  MenuCommand fromValue_end;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect star;
  MenuMark markWideCoords;
  MenuCoords16 statusLine_rc;
  MenuCmd_Colors statusLine_colors;
  MenuCmd_TextNoPtr statusLine_text;
  MenuCommand statusLine_end;
  MenuMark markByteCoords;
  MenuEntry_Byte_Nav_Colors_StringTable_OnSelect opts[4];
  MenuCoords8 spacer_rc;
  MenuCmd_Nav spacer_nav;
  MenuCmd_Invisible spacer_invisible;
  MenuCommand spacer_end;
  MenuEntry_Byte_Nav_Colors_StringTable_OnSelect optLast;
} Menu_DesignDiyTournament_StageBlockA;
SWOS_PACK_POP

/* 197b - Stage block for DesignDiyTournament with TextPtr status line (304 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_DesignDiyTournament_StageBlockB
{
  MenuMark_StartMenu startMenu;
  MenuEntry_Byte_NumberOnly stageNum;
  MenuEntry_Byte_SpriteFgOnly icon;
  MenuCoords8 qualifiers_rc;
  MenuCmd_Nav qualifiers_nav;
  MenuCmd_Colors qualifiers_colors;
  MenuCmd_Number qualifiers_number;
  MenuCmd_InputFunc qualifiers_input;
  MenuCommand qualifiers_end;
  MenuCoords8 fromValue_rc;
  MenuCmd_Nav fromValue_nav;
  MenuCmd_Colors fromValue_colors;
  MenuCmd_TextNoPtr fromValue_text;
  MenuCmd_OnSelect fromValue_onSelect;
  MenuCommand fromValue_end;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect star;
  MenuMark markWideCoords;
  MenuCoords16 statusLine_rc;
  MenuCmd_Colors statusLine_colors;
  MenuCmd_TextPtr statusLine_text;
  MenuCommand statusLine_end;
  MenuMark markByteCoords;
  MenuEntry_Byte_Nav_Colors_StringTable_OnSelect opts[4];
  MenuCoords8 spacer_rc;
  MenuCmd_Nav spacer_nav;
  MenuCmd_Invisible spacer_invisible;
  MenuCommand spacer_end;
  MenuEntry_Byte_Nav_Colors_StringTable_OnSelect optLast;
} Menu_DesignDiyTournament_StageBlockB;
SWOS_PACK_POP

/* 198 - Menu 8: designDiyTournamentMenu (0x920 = 2336 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_DesignDiyTournament
{
  MenuHeader hdr;
  MenuMark markByteCoords0;
  MenuEntry_Byte_Colors_TextPtr entry0;
  MenuMark markWideCoords0;
  MenuCoords16 entry1_rc;
  MenuCmd_Invisible entry1_invisible;
  MenuCmd_Colors entry1_colors;
  MenuCmd_TextPtr entry1_text;
  MenuCommand entry1_end;
  MenuCoords16 entry2_rc;
  MenuCmd_Nav entry2_nav;
  MenuCmd_Colors entry2_colors;
  MenuCmd_TextPtr entry2_text;
  MenuCmd_OnSelect entry2_onSelect;
  MenuCommand entry2_end;
  MenuMark markByteCoords1;
  MenuEntry_Byte_Nav_Colors_StringTable_OnSelect entries3_5[3];
  MenuEntry_Byte_Colors_TextPtr entry6;
  MenuEntry_Byte_Nav_Colors_StringTable_OnSelect entry7;
  MenuEntry_Byte_Colors_TextPtr entry8;
  MenuEntry_Byte_Nav_Colors_StringTable_OnSelect entry9;
  MenuEntry_Byte_Colors_TextPtr entry10;
  MenuCoords8 entry11_rc;
  MenuCmd_Nav entry11_nav;
  MenuCmd_Colors entry11_colors;
  MenuCmd_TextNoPtr entry11_text;
  MenuCmd_OnSelect entry11_onSelect;
  MenuCommand entry11_end;
  MenuEntry_Byte_Colors_TextPtr entry12;
  MenuCoords8 entry13_rc;
  MenuCmd_Nav entry13_nav;
  MenuCmd_Colors entry13_colors;
  MenuCmd_TextNoPtr entry13_text;
  MenuCmd_OnSelect entry13_onSelect;
  MenuCommand entry13_end;
  MenuMark_StartMenu startMenuHeaders;
  MenuEntry_Byte_TextPtrOnly entries14_16[3];
  Menu_DesignDiyTournament_StageBlockA stage1;
  Menu_DesignDiyTournament_StageBlockB stage2;
  Menu_DesignDiyTournament_StageBlockA stages3_6[4];
  MenuMark_StartMenu startMenuFooter;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entries89_91[3];
  MenuCoords8 entry92_rc;
  MenuCmd_Colors entry92_colors;
  MenuCmd_TextNoPtr entry92_text;
  MenuCommand entry92_end;
  MenuMark markEndMenu;
} Menu_DesignDiyTournament;
SWOS_PACK_POP

/* 183 - Menu 4: DIYLeagueTournamentMenu (0x3D8 = 984 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_DIYLeagueTournament
{
  MenuHeader hdr;
  MenuCoords16 entry0_rc;
  MenuCmd_Colors entry0_colors;
  MenuCmd_TextNoPtr entry0_text;
  MenuCommand entry0_end;
  MenuMark markByteCoords0;
  MenuCoords8 entry1_rc;
  MenuCmd_Nav entry1_nav;
  MenuCmd_Skip2 entry1_skip;
  MenuCmd_Colors entry1_colors;
  MenuCmd_TextPtr entry1_text;
  MenuCmd_OnSelect entry1_onSelect;
  MenuCommand entry1_end;
  MenuCoords8 entry2_rc;
  MenuCmd_Nav entry2_nav;
  MenuCmd_Invisible entry2_invisible;
  MenuCmd_Colors entry2_colors;
  MenuCmd_TextPtr entry2_text;
  MenuCommand entry2_end;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry3;
  MenuCoords8 entry4_rc;
  MenuCmd_Nav entry4_nav;
  MenuCmd_Invisible entry4_invisible;
  MenuCmd_Colors entry4_colors;
  MenuCmd_TextPtr entry4_text;
  MenuCmd_OnSelect entry4_onSelect;
  MenuCommand entry4_end;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entries5_6[2];
  MenuCoords8 entry7_rc;
  MenuCmd_Nav entry7_nav;
  MenuCmd_Invisible entry7_invisible;
  MenuCmd_Colors entry7_colors;
  MenuCmd_TextPtr entry7_text;
  MenuCmd_OnSelect entry7_onSelect;
  MenuCommand entry7_end;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entries8_10[3];
  MenuEntry_Byte_Colors_TextPtr entries11_13[3];
  MenuEntry_Byte_Colors_Number entries14_15[2];
  MenuEntry_Byte_Colors_TextPtr_BeforeDraw entries16_23[8];
  MenuEntry_Byte_Invisible_Colors_TextPtr entries24_29[6];
  MenuMark markWideCoords0;
  MenuCoords16 entry30_rc;
  MenuCmd_Invisible entry30_invisible;
  MenuCmd_Colors entry30_colors;
  MenuCmd_TextPtr entry30_text;
  MenuCommand entry30_end;
  MenuMark markByteCoords1;
  MenuEntry_Byte_NumberOnly entry31;
  MenuEntry_Byte_Colors_TextPtr entry32;
  MenuEntry_Byte_Colors_Number entries33_38[6];
  MenuMark markWideCoords1;
  MenuCoords16 entry39_rc;
  MenuCmd_Colors entry39_colors;
  MenuCmd_Number entry39_number;
  MenuCommand entry39_end;
  MenuMark markByteCoords2;
  MenuCoords8 entry40_rc;
  MenuCmd_Nav entry40_nav;
  MenuCmd_Colors entry40_colors;
  MenuCmd_SpriteFg entry40_sprite;
  MenuCmd_OnSelect entry40_onSelect;
  MenuCommand entry40_end;
  MenuCoords8 entry41_rc;
  MenuCmd_Nav entry41_nav;
  MenuCmd_Colors entry41_colors;
  MenuCmd_SpriteFg entry41_sprite;
  MenuCmd_OnSelect entry41_onSelect;
  MenuCommand entry41_end;
  MenuCoords8 entry42_rc;
  MenuCmd_InputFunc entry42_input;
  MenuCommand entry42_end;
  MenuMark markEndMenu;
} Menu_DIYLeagueTournament;
SWOS_PACK_POP

/* 187 - Menu 5: designDiyCupMenu (0x524 = 1316 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_DesignDIYCup
{
  MenuHeader hdr;
  MenuMark markByteCoords0;
  MenuEntry_Byte_Colors_TextPtr entry0;
  MenuMark markWideCoords0;
  MenuCoords16 entry1_rc;
  MenuCmd_Invisible entry1_invisible;
  MenuCmd_Colors entry1_colors;
  MenuCmd_TextPtr entry1_text;
  MenuCommand entry1_end;
  MenuMark_StartMenu startMenuA;
  MenuCoords16 entry2_rc;
  MenuCmd_Nav entry2_nav;
  MenuCmd_Colors entry2_colors;
  MenuCmd_TextPtr entry2_text;
  MenuCmd_OnSelect entry2_onSelect;
  MenuCommand entry2_end;
  MenuMark markByteCoords1;
  MenuEntry_Byte_Nav_Colors_StringTable_OnSelect entries3_5[3];
  MenuEntry_Byte_Colors_TextPtr entry6;
  MenuEntry_Byte_Nav_Colors_StringTable_OnSelect entry7;
  MenuEntry_Byte_Colors_TextPtr entry8;
  MenuEntry_Byte_Nav_Colors_StringTable_OnSelect entry9;
  MenuEntry_Byte_Colors_TextPtr entry10;
  MenuCoords8 entry11_rc;
  MenuCmd_Nav entry11_nav;
  MenuCmd_Colors entry11_colors;
  MenuCmd_Number entry11_number;
  MenuCmd_OnSelect entry11_onSelect;
  MenuCommand entry11_end;
  MenuEntry_Byte_Colors_TextPtr entry12;
  MenuCoords8 entry13_rc;
  MenuCmd_Nav entry13_nav;
  MenuCmd_Colors entry13_colors;
  MenuCmd_TextNoPtr entry13_text;
  MenuCmd_OnSelect entry13_onSelect;
  MenuCommand entry13_end;
  MenuEntry_Byte_Colors_TextPtr entry14;
  MenuMark markWideCoords1;
  MenuCoords16 entry15_rc;
  MenuCmd_Nav entry15_nav;
  MenuCmd_Colors entry15_colors;
  MenuCmd_TextNoPtr entry15_text;
  MenuCmd_OnSelect entry15_onSelect;
  MenuCommand entry15_end;
  MenuMark markByteCoords2;
  MenuEntry_Byte_TextPtrOnly entries16_17[2];
  Menu_DesignDIYCup_OptionRow rows[6];
  MenuMark_StartMenu startMenuB;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entries48_50[3];
  MenuCoords8 entry51_rc;
  MenuCmd_Colors entry51_colors;
  MenuCmd_TextNoPtr entry51_text;
  MenuCommand entry51_end;
  MenuMark markEndMenu;
} Menu_DesignDIYCup;
SWOS_PACK_POP

/* 190 - Menu 6: DIYCupMenu (0x422 = 1058 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_DIYCup
{
  MenuHeader hdr;
  MenuCoords16 entry0_rc;
  MenuCmd_Colors entry0_colors;
  MenuCmd_TextNoPtr entry0_text;
  MenuCommand entry0_end;
  MenuMark markByteCoords0;
  MenuCoords8 entry1_rc;
  MenuCmd_Nav entry1_nav;
  MenuCmd_Skip2 entry1_skip;
  MenuCmd_Colors entry1_colors;
  MenuCmd_TextPtr entry1_text;
  MenuCmd_OnSelect entry1_onSelect;
  MenuCommand entry1_end;
  MenuCoords8 entry2_rc;
  MenuCmd_Nav entry2_nav;
  MenuCmd_Invisible entry2_invisible;
  MenuCmd_Colors entry2_colors;
  MenuCmd_TextPtr entry2_text;
  MenuCommand entry2_end;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry3;
  MenuCoords8 entry4_rc;
  MenuCmd_Nav entry4_nav;
  MenuCmd_Invisible entry4_invisible;
  MenuCmd_Colors entry4_colors;
  MenuCmd_TextPtr entry4_text;
  MenuCmd_OnSelect entry4_onSelect;
  MenuCommand entry4_end;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entries5_6[2];
  MenuCoords8 entry7_rc;
  MenuCmd_Nav entry7_nav;
  MenuCmd_Invisible entry7_invisible;
  MenuCmd_Colors entry7_colors;
  MenuCmd_TextPtr entry7_text;
  MenuCmd_OnSelect entry7_onSelect;
  MenuCommand entry7_end;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entries8_10[3];
  MenuEntry_Byte_Colors_TextPtr entries11_13[3];
  MenuEntry_Byte_Colors_Number entries14_15[2];
  MenuCoords8 entry16_rc;
  MenuCmd_Nav entry16_nav;
  MenuCmd_Colors entry16_colors;
  MenuCmd_SpriteFg entry16_sprite;
  MenuCmd_OnSelect entry16_onSelect;
  MenuCommand entry16_end;
  MenuCoords8 entry17_rc;
  MenuCmd_Nav entry17_nav;
  MenuCmd_Colors entry17_colors;
  MenuCmd_SpriteFg entry17_sprite;
  MenuCmd_OnSelect entry17_onSelect;
  MenuCommand entry17_end;
  Menu_DIYCup_StatsBlock blocks18_65[6];
  MenuCoords8 entry66_rc;
  MenuCmd_InputFunc entry66_input;
  MenuCommand entry66_end;
  MenuMark markEndMenu;
} Menu_DIYCup;
SWOS_PACK_POP

/* 193 - Menu 7: optionsMenu (0x182 = 386 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_Options
{
  MenuHeader hdr;
  MenuMark markByteCoords;
  MenuEntry_Byte_Colors_TextPtr entry0;
  MenuMark_StartMenu startMenu0;
  MenuEntry_Byte_Colors_TextPtr labelGameLength;
  MenuEntry_Byte_Nav_Colors_StringTable_OnSelect gameLength;
  MenuEntry_Byte_Colors_TextPtr labelPitchType;
  MenuEntry_Byte_Nav_Colors_StringTable_OnSelect pitchType;
  MenuEntry_Byte_Colors_TextPtr labelAutoReplays;
  MenuEntry_Byte_Nav_Colors_StringTable_OnSelect autoReplays;
  MenuEntry_Byte_Colors_TextPtr labelAllEqual;
  MenuEntry_Byte_Nav_Colors_StringTable_OnSelect allEqual;
  MenuEntry_Byte_Colors_TextPtr labelAutoSaveHl;
  MenuEntry_Byte_Nav_Colors_StringTable_OnSelect autoSaveHl;
  MenuEntry_Byte_Colors_TextPtr labelMenuMusic;
  MenuEntry_Byte_Nav_Colors_StringTable_OnSelect menuMusic;
  MenuMark_StartMenu startMenuExit;
  MenuCoords8 exit_rc;
  MenuCmd_Nav exit_nav;
  MenuCmd_Colors exit_colors;
  MenuCmd_TextPtr exit_text;
  MenuCmd_InputFunc exit_input;
  MenuCommand exit_end;
  MenuCoords8 versionText_rc;
  MenuCmd_Invisible versionText_invisible;
  MenuCmd_TextPtr versionText_text;
  MenuCommand versionText_end;
  MenuMark markEndMenu;
} Menu_Options;
SWOS_PACK_POP

/* 199 - Menu 9: friendlyGameMenu (0x12E = 302 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_FriendlyGame
{
  MenuHeader hdr;
  MenuMark markByteCoords;
  MenuEntry_Byte_Colors_TextPtr entry0;
  MenuMark_StartMenu menu1Start;
  MenuEntry_Byte_Colors_TextPtr entry1;
  MenuEntry_Byte_Colors_TextPtr entry2;
  MenuEntry_Byte_Nav_Colors_StringTable_OnSelect entry3;
  MenuEntry_Byte_Nav_Colors_StringTable_OnSelect entry4;
  MenuCoords8 entry5_rc;
  MenuCmd_Nav entry5_nav;
  MenuCmd_Colors entry5_colors;
  MenuCmd_TextNoPtr entry5_text;
  MenuCmd_OnSelect entry5_onSelect;
  MenuCommand entry5_end;
  MenuCoords8 entry6_rc;
  MenuCmd_Nav entry6_nav;
  MenuCmd_Colors entry6_colors;
  MenuCmd_TextNoPtr entry6_text;
  MenuCmd_OnSelect entry6_onSelect;
  MenuCommand entry6_end;
  MenuMark_StartMenu menu2Start;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry7;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry8;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry9;
  MenuCoords8 entry10_rc;
  MenuCmd_Colors entry10_colors;
  MenuCmd_TextNoPtr entry10_text;
  MenuCommand entry10_end;
  MenuMark markEndMenu;
} Menu_FriendlyGame;
SWOS_PACK_POP

/* 200 - Menu 14: competitionStatsMenu (0x9E = 158 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_CompetitionStats
{
  MenuHeader hdr;
  MenuMark markByteCoords;
  MenuEntry_Byte_Colors_TextPtr entry0;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry1;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry2;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry3;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry4;
  MenuMark markEndMenu;
} Menu_CompetitionStats;
SWOS_PACK_POP

/* 201 - Menu 15: careerOverMenu (0xE4 = 228 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_CareerOver
{
  MenuHeader hdr;
  MenuMark markByteCoords;
  MenuCoords8 entry0_rc;
  MenuCmd_Colors entry0_colors;
  MenuCmd_TextNoPtr entry0_text;
  MenuCommand entry0_end;
  MenuCoords8 entry1_rc;
  MenuCmd_Colors entry1_colors;
  MenuCmd_TextNoPtr entry1_text;
  MenuCommand entry1_end;
  MenuCoords8 entry2_rc;
  MenuCmd_Colors entry2_colors;
  MenuCmd_TextNoPtr entry2_text;
  MenuCommand entry2_end;
  MenuCoords8 entry3_rc;
  MenuCmd_Invisible entry3_invisible;
  MenuCmd_TextPtr entry3_text;
  MenuCommand entry3_end;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry4;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry5;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry6;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry7;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry8;
  MenuMark markEndMenu;
} Menu_CareerOver;
SWOS_PACK_POP

/* 201 - Menu 15: careerMenu (0x14C = 332 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_Career
{
  MenuHeader hdr;
  MenuMark markByteCoords;
  MenuEntry_Byte_Colors_TextPtr entry0;
  MenuCoords8 entry1_rc;
  MenuCmd_Nav entry1_nav;
  MenuCmd_Invisible entry1_invisible;
  MenuCmd_Colors entry1_colors;
  MenuCmd_TextPtr entry1_text;
  MenuCmd_OnSelect entry1_onSelect;
  MenuCommand entry1_end;
  MenuEntry_Byte_Colors_TextPtr entry2;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry3;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry4;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry5;
  MenuEntry_Byte_Colors_TextPtr entry6;
  MenuCoords8 entry7_rc;
  MenuCmd_Nav entry7_nav;
  MenuCmd_Colors entry7_colors;
  MenuCmd_TextNoPtr entry7_text;
  MenuCmd_InputFunc entry7_input;
  MenuCommand entry7_end;
  MenuEntry_Byte_Colors_TextPtr entry8;
  MenuEntry_Byte_Nav_Colors_StringTable_OnSelect entry9;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry10;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry11;
  MenuMark markEndMenu;
} Menu_Career;
SWOS_PACK_POP

/* 202 - Menu 16: viewCompetitionsMenu (0xF8 = 248 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_ViewCompetitions
{
  MenuHeader hdr;
  MenuMark markByteCoords;
  MenuEntry_Byte_Colors_TextPtr entry0;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry1;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry2;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry3;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry4;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry5;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry6;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry7;
  MenuMark markEndMenu;
} Menu_ViewCompetitions;
SWOS_PACK_POP

/* 203 - Menu 23: continueMenu (0x42 = 66 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_Continue
{
  MenuHeader hdr;
  MenuCoords16 entry0_coords;
  MenuCmd_Colors entry0_colors;
  MenuCmd_TextPtr entry0_text;
  MenuCommand entry0_end;
  MenuMark markByteCoords;
  MenuCoords8 entry1_rc;
  MenuCmd_Colors entry1_colors;
  MenuCmd_TextPtr entry1_text;
  MenuCmd_OnSelect entry1_onSelect;
  MenuCommand entry1_end;
  MenuMark markEndMenu;
} Menu_Continue;
SWOS_PACK_POP

/* 204 - Menu 24: continueAbortMenu (0x74 = 116 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_ContinueAbort
{
  MenuHeader hdr;
  MenuMark markByteCoords;
  MenuEntry_Byte_Colors_TextPtr entry0;
  MenuCoords8 entry1_rc;
  MenuCmd_Colors entry1_colors;
  MenuCmd_TextPtr entry1_multilineText;
  MenuCommand entry1_end;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry2;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry3;
  MenuMark markEndMenu;
} Menu_ContinueAbort;
SWOS_PACK_POP

/* 205 - Menu 25: SWOS_MainMenu (0x1B6 = 438 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_SWOSMainMenu
{
  MenuHeader hdr;
  MenuCoords16 entry0_coords;
  MenuCmd_SpriteFg entry0_sprite;
  MenuCommand entry0_end;
  MenuMark_StartMenu menu1Start;
  MenuMark markByteCoords1;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry1;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry2;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry3;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry4;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry5;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry6;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry7;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry8;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry9;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry10;
  MenuMark_StartMenu menu2Start;
  MenuCoords8 entry11_rc;
  MenuCmd_Nav entry11_nav;
  MenuCmd_Colors entry11_colors;
  MenuCmd_TextNoPtr entry11_text;
  MenuCmd_OnSelect entry11_onSelect;
  MenuCommand entry11_end;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry12;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry13;
  MenuMark_StartMenu menu3Start;
  MenuMark markEndMenu;
} Menu_SWOSMainMenu;
SWOS_PACK_POP

/* 206 - Menu 21: chooseTeamsMenu (0xBE = 190 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_ChooseTeams
{
  MenuHeader hdr;
  MenuMark markByteCoords;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry0;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry1;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry2;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry3;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry4;
  MenuMark markWideCoords;
  MenuCoords16 entry5_rc;
  MenuCmd_Colors entry5_colors;
  MenuCmd_TextNoPtr entry5_text;
  MenuCommand entry5_end;
  MenuMark markEndMenu;
} Menu_ChooseTeams;
SWOS_PACK_POP

/* 207 - Menu 26: offersMenu (0xF4 = 244 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_Offers
{
  MenuHeader hdr;
  MenuMark markByteCoords;
  MenuEntry_Byte_Colors_TextPtr entry0;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry1;
  MenuMark_StartMenu menu1Start;
  MenuMark template1_markStart;
  MenuCoords8 template1_rc;
  MenuCmd_Colors template1_colors;
  MenuCmd_OnSelect template1_onSelect;
  MenuCmd_BeforeDraw template1_afterDraw;
  MenuCommand template1_end;
  MenuMark markWideCoords;
  MenuEntry_Wide_NavOnly entries2_6[5];
  MenuMark markInitEntry;
  MenuMark_StartMenu menu2Start;
  MenuCoords16 entry7_rc;
  MenuCmd_Colors entry7_colors;
  MenuCommand entry7_end;
  MenuEntry_Wide_TextNoPtrOnly entries8_9[2];
  MenuMark markByteCoords2;
  MenuEntry_Byte_NumberOnly entry10;
  MenuMark markEndMenu;
} Menu_Offers;
SWOS_PACK_POP

/* 208 - Menu 33: buyPlayerMenu (0xE4 = 228 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_BuyPlayer
{
  MenuHeader hdr;
  MenuMark markByteCoords;
  MenuEntry_Byte_Colors_TextPtr entry0;
  MenuEntry_Byte_TextNoPtrOnly entry1;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry2;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry3;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry4;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry5;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry6;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry7;
  MenuMark markEndMenu;
} Menu_BuyPlayer;
SWOS_PACK_POP

/* 209 - Menu 36: nationalTeamAddPlayerMenu (0x80 = 128 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_NationalTeamAddPlayer
{
  MenuHeader hdr;
  MenuMark markByteCoords;
  MenuEntry_Byte_Colors_TextPtr entry0;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry1;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry2;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry3;
  MenuMark markEndMenu;
} Menu_NationalTeamAddPlayer;
SWOS_PACK_POP

/* 210 - Menu 40: chairmanMemosMenu (0x92 = 146 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_ChairmanMemos
{
  MenuHeader hdr;
  MenuMark markByteCoords;
  MenuEntry_Byte_Colors_TextPtr entry0;
  MenuMark markWideCoords;
  MenuEntry_Wide_TextNoPtrOnly entries1_3[3];
  MenuCoords16 entry4_rc;
  MenuCmd_TextPtr entry4_text;
  MenuCommand entry4_end;
  MenuCoords16 entry5_rc;
  MenuCmd_TextPtr entry5_text;
  MenuCommand entry5_end;
  MenuMark markByteCoords2;
  MenuCoords8 entry6_rc;
  MenuCmd_Invisible entry6_invisible;
  MenuCmd_Colors entry6_colors;
  MenuCmd_TextPtr entry6_text;
  MenuCmd_OnSelect entry6_onSelect;
  MenuCommand entry6_end;
  MenuMark markEndMenu;
} Menu_ChairmanMemos;
SWOS_PACK_POP

/* 211 - Menu 46: highlightsMenu (0x9E = 158 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_Highlights
{
  MenuHeader hdr;
  MenuMark markByteCoords;
  MenuEntry_Byte_Colors_TextPtr entry0;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry1;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry2;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry3;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry4;
  MenuMark markEndMenu;
} Menu_Highlights;
SWOS_PACK_POP

/* 212 - Menu 42: chooseTacticsMenu (0xF8 = 248 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_ChooseTactics
{
  MenuHeader hdr;
  MenuMark markByteCoords;
  MenuEntry_Byte_Colors_TextPtr entry0;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry1;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry2;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry3;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry4;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry5;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry6;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry7;
  MenuMark markEndMenu;
} Menu_ChooseTactics;
SWOS_PACK_POP

/* 213 - Menu 41: clubBusinessMenu (0x156 = 342 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_ClubBusiness
{
  MenuHeader hdr;
  MenuMark markByteCoords;
  MenuEntry_Byte_Colors_TextPtr entry0;
  MenuCoords8 entry1_rc;
  MenuCmd_Colors entry1_colors;
  MenuCmd_TextPtr entry1_text;
  MenuCmd_OnSelect entry1_onSelect;
  MenuCommand entry1_end;
  MenuMark_StartMenu menu1Start;
  MenuMark markWideCoords;
  MenuCoords16 entry2_rc;
  MenuCmd_TextPtr entry2_text;
  MenuCommand entry2_end;
  MenuMark markByteCoords2;
  MenuCoords8 entry3_rc;
  MenuCmd_Invisible entry3_invisible;
  MenuCmd_TextPtr entry3_text;
  MenuCommand entry3_end;
  MenuEntry_Byte_TextPtrOnly entry4;
  MenuEntry_Byte_TextPtrOnly entry5;
  MenuCoords8 entry6_rc;
  MenuCmd_Invisible entry6_invisible;
  MenuCmd_TextPtr entry6_text;
  MenuCommand entry6_end;
  MenuEntry_Byte_TextPtrOnly entry7;
  MenuEntry_Byte_TextPtrOnly entry8;
  MenuEntry_Byte_TextPtrOnly entry9;
  MenuEntry_Byte_TextPtrOnly entry10;
  MenuEntry_Byte_TextPtrOnly entry11;
  MenuMark_StartMenu menu2Start;
  MenuMark markWideCoords2;
  MenuEntry_Wide_TextNoPtrOnly entry12;
  MenuMark markByteCoords3;
  MenuCoords8 entry13_rc;
  MenuCmd_Invisible entry13_invisible;
  MenuCmd_TextNoPtr entry13_text;
  MenuCommand entry13_end;
  MenuEntry_Byte_TextNoPtrOnly entry14;
  MenuEntry_Byte_TextNoPtrOnly entry15;
  MenuCoords8 entry16_rc;
  MenuCmd_Invisible entry16_invisible;
  MenuCmd_TextNoPtr entry16_text;
  MenuCommand entry16_end;
  MenuEntry_Byte_TextNoPtrOnly entry17;
  MenuEntry_Byte_TextNoPtrOnly entry18;
  MenuEntry_Byte_TextNoPtrOnly entry19;
  MenuEntry_Byte_TextNoPtrOnly entry20;
  MenuCoords8 entry21_rc;
  MenuCmd_Colors entry21_colors;
  MenuCmd_TextNoPtr entry21_text;
  MenuCommand entry21_end;
  MenuMark markEndMenu;
} Menu_ClubBusiness;
SWOS_PACK_POP

/* 214 - Menu 39: gotJobOfferMenu (0x140 = 320 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_GotJobOffer
{
  MenuHeader hdr;
  MenuMark markByteCoords;
  MenuEntry_Byte_Colors_TextPtr entry0;
  MenuMark markWideCoords;
  MenuEntry_Wide_TextNoPtrOnly entries1_7[7];
  MenuMark markByteCoords2;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry8;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry9;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry10;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry11;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry12;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry13;
  MenuMark markEndMenu;
} Menu_GotJobOffer;
SWOS_PACK_POP

/* 215 - Menu 38: jobOffersMenu (0x180 = 384 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_JobOffers
{
  MenuHeader hdr;
  MenuMark markByteCoords;
  MenuEntry_Byte_Colors_TextPtr entry0;
  MenuMark_StartMenu menu1Start;
  MenuMark template1_markStart;
  MenuCoords8 template1_rc;
  MenuCmd_Colors template1_colors;
  MenuCmd_OnSelect template1_onSelect;
  MenuCmd_BeforeDraw template1_afterDraw;
  MenuCommand template1_end;
  MenuMark markWideCoords;
  MenuEntry_Wide_NavOnly entries1_5[5];
  MenuMark markInitEntry;
  MenuMark_StartMenu menu2Start;
  MenuMark markByteCoords2;
  MenuEntry_Byte_TextPtrOnly entry6;
  MenuMark_StartMenu menu3Start;
  MenuMark template2_markStart;
  MenuCoords8 template2_rc;
  MenuCmd_Colors template2_colors;
  MenuCmd_BeforeDraw template2_afterDraw;
  MenuCmd_OnSelect template2_onSelect;
  MenuCommand template2_end;
  MenuMark markWideCoords2;
  MenuEntry_Wide_NavOnly entries7_11[5];
  MenuMark markInitEntry2;
  MenuMark_StartMenu menu4Start;
  MenuMark markByteCoords3;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry12;
  MenuEntry_Byte_NumberOnly entry13;
  MenuMark markWideCoords3;
  MenuEntry_Wide_TextNoPtrOnly entry14;
  MenuMark markByteCoords4;
  MenuEntry_Byte_NumberOnly entry15;
  MenuMark markWideCoords4;
  MenuEntry_Wide_TextNoPtrOnly entry16;
  MenuMark markEndMenu;
} Menu_JobOffers;
SWOS_PACK_POP

/* 216 - Menu 27: playerTransferMenu (0x1CE = 462 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_PlayerTransfer
{
  MenuHeader hdr;
  MenuMark markByteCoords;
  MenuEntry_Byte_Colors_TextPtr entry0;
  MenuMark markWideCoords;
  MenuEntry_Wide_TextNoPtrOnly entries1_3[3];
  MenuCoords16 entry4_rc;
  MenuCmd_Invisible entry4_invisible;
  MenuCmd_TextNoPtr entry4_text;
  MenuCommand entry4_end;
  MenuCoords16 entry5_rc;
  MenuCmd_Invisible entry5_invisible;
  MenuCmd_TextNoPtr entry5_text;
  MenuCommand entry5_end;
  MenuMark markByteCoords2;
  MenuEntry_Byte_TextPtrOnly entries6_8[3];
  MenuEntry_Byte_TextNoPtrOnly entry9;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entries10_11[2];
  MenuCoords8 entry12_rc;
  MenuCmd_Invisible entry12_invisible;
  MenuCmd_Colors entry12_colors;
  MenuCmd_TextPtr entry12_text;
  MenuCmd_OnSelect entry12_onSelect;
  MenuCommand entry12_end;
  MenuMark template13_markStart;
  MenuCoords8 template13_rc;
  MenuCmd_Colors template13_colors;
  MenuCmd_AfterDraw template13_afterDraw;
  MenuCommand template13_end;
  MenuMark markWideCoords2;
  MenuEntry_Wide_CoordsOnly entries13_16[4];
  MenuMark markInitEntry;
  MenuEntry_Wide_ColorsOnly entry17;
  MenuMark markByteCoords3;
  MenuEntry_Byte_NumberOnly entry18;
  MenuEntry_Byte_SpriteFgOnly entry19;
  MenuEntry_Byte_TextPtrOnly entry20;
  MenuEntry_Byte_TextNoPtrOnly entry21;
  MenuEntry_Byte_TextPtrOnly entry22;
  MenuEntry_Byte_TextNoPtrOnly entry23;
  MenuEntry_Byte_SpriteFgOnly entries24_26[3];
  MenuEntry_Byte_TextNoPtrOnly entry27;
  MenuMark markWideCoords3;
  MenuEntry_Wide_TextPtrOnly entry28;
  MenuMark markEndMenu;
} Menu_PlayerTransfer;
SWOS_PACK_POP

/* 217 - Menu 34: transferMarketMenu (0x1FC = 508 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_TransferMarket
{
  MenuHeader hdr;
  MenuMark markByteCoords;
  MenuEntry_Byte_Colors_TextPtr entry0;
  MenuEntry_Byte_TextNoPtrOnly entry1;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry2;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry3;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry4;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry5;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry6;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry7;
  MenuEntry_Byte_TextPtrOnly entry8;
  MenuMark_StartMenu menu1Start;
  MenuMark template1_markStart;
  MenuCoords8 template1_rc;
  MenuCmd_Colors template1_colors;
  MenuCmd_BeforeDraw template1_afterDraw;
  MenuCmd_OnSelect template1_onSelect;
  MenuCommand template1_end;
  MenuMark markWideCoords;
  MenuEntry_Wide_NavOnly entries9_13[5];
  MenuMark markInitEntry;
  MenuMark_StartMenu menu2Start;
  MenuCoords16 entry14_rc;
  MenuCmd_Colors entry14_colors;
  MenuCommand entry14_end;
  MenuMark markByteCoords2;
  MenuEntry_Byte_NumberOnly entry15;
  MenuEntry_Byte_SpriteFgOnly entry16;
  MenuEntry_Byte_TextPtrOnly entry17;
  MenuEntry_Byte_TextNoPtrOnly entry18;
  MenuEntry_Byte_TextPtrOnly entry19;
  MenuEntry_Byte_TextNoPtrOnly entry20;
  MenuEntry_Byte_SpriteFgOnly entries21_23[3];
  MenuEntry_Byte_TextNoPtrOnly entry24;
  MenuMark markWideCoords2;
  MenuCoords16 entry25_rc;
  MenuCmd_TextPtr entry25_text;
  MenuCommand entry25_end;
  MenuMark markEndMenu;
} Menu_TransferMarket;
SWOS_PACK_POP

/* 218 - Menu 18: managementRecordMenu (0x150 = 336 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_ManagementRecord
{
  MenuHeader hdr;
  MenuCoords16 entry0_coords;
  MenuCmd_Colors entry0_colors;
  MenuCmd_TextNoPtr entry0_text;
  MenuCmd_BeforeDraw entry0_beforeDraw;
  MenuCommand entry0_end;
  MenuMark markByteCoords;
  MenuCoords8 entry1_rc;
  MenuCmd_Nav entry1_nav;
  MenuCmd_Colors entry1_colors;
  MenuCmd_SpriteFg entry1_sprite;
  MenuCmd_OnSelect entry1_onSelect;
  MenuCommand entry1_end;
  MenuCoords8 entry2_rc;
  MenuCmd_Nav entry2_nav;
  MenuCmd_Colors entry2_colors;
  MenuCmd_SpriteFg entry2_sprite;
  MenuCmd_OnSelect entry2_onSelect;
  MenuCommand entry2_end;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry3;
  MenuEntry_Byte_TextNoPtrOnly entries4_8[5];
  MenuMark markWideCoords;
  MenuCoords16 entry9_rc;
  MenuCmd_TextPtr entry9_text;
  MenuCommand entry9_end;
  MenuMark markByteCoords2;
  MenuEntry_Byte_TextPtrOnly entry10;
  MenuEntry_Byte_TextNoPtrOnly entry11;
  MenuMark markWideCoords2;
  MenuCoords16 entry12_rc;
  MenuCmd_TextPtr entry12_text;
  MenuCommand entry12_end;
  MenuEntry_Wide_TextNoPtrOnly entries13_14[2];
  MenuMark markByteCoords3;
  MenuEntry_Byte_TextPtrOnly entry15;
  MenuEntry_Byte_TextNoPtrOnly entry16;
  MenuEntry_Byte_SpriteFgOnly entries17_20[4];
  MenuMark markEndMenu;
} Menu_ManagementRecord;
SWOS_PACK_POP

/* 219 - Menu 30: viewSquadMenu (0x276 = 630 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_ViewSquad
{
  MenuHeader hdr;
  MenuMark markByteCoords;
  MenuEntry_Byte_Colors_TextPtr entry0;
  MenuEntry_Byte_Colors_TextPtr entry1;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry2;
  MenuCoords8 entry3_rc;
  MenuCmd_Nav entry3_nav;
  MenuCmd_Invisible entry3_invisible;
  MenuCmd_Colors entry3_colors;
  MenuCmd_TextPtr entry3_text;
  MenuCmd_OnSelect entry3_onSelect;
  MenuCommand entry3_end;
  MenuMark_StartMenu menu1Start;
  MenuMark template1_markStart;
  MenuCoords8 template1_rc;
  MenuCmd_Colors template1_colors;
  MenuCmd_BeforeDraw template1_afterDraw;
  MenuCmd_OnSelect template1_onSelect;
  MenuCommand template1_end;
  MenuMark markWideCoords;
  MenuEntry_Wide_NavOnly entries4_19[16];
  MenuCoords16 entry20_rc;
  MenuCmd_Nav entry20_nav;
  MenuCmd_Skip2 entry20_skip;
  MenuCmd_Invisible entry20_invisible;
  MenuCommand entry20_end;
  MenuMark markInitEntry;
  MenuMark markByteCoords2;
  MenuCoords8 entry21_rc;
  MenuCmd_Invisible entry21_invisible;
  MenuCmd_Nav entry21_nav;
  MenuCmd_Colors entry21_colors;
  MenuCmd_OnSelect entry21_onSelect;
  MenuCmd_BeforeDraw entry21_afterDraw;
  MenuCommand entry21_end;
  MenuEntry_Byte_Invisible_NavOnly entry22;
  MenuMark_StartMenu menu2Start;
  MenuMark markWideCoords2;
  MenuCoords16 entry23_rc;
  MenuCmd_Colors entry23_colors;
  MenuCommand entry23_end;
  MenuMark markByteCoords3;
  MenuEntry_Byte_NumberOnly entry24;
  MenuEntry_Byte_SpriteFgOnly entry25;
  MenuEntry_Byte_TextPtrOnly entry26;
  MenuEntry_Byte_TextNoPtrOnly entry27;
  MenuEntry_Byte_TextPtrOnly entry28;
  MenuEntry_Byte_TextNoPtrOnly entry29;
  MenuEntry_Byte_SpriteFgOnly entries30_32[3];
  MenuEntry_Byte_TextNoPtrOnly entry33;
  MenuMark markWideCoords3;
  MenuCoords16 entry34_rc;
  MenuCmd_TextPtr entry34_text;
  MenuCommand entry34_end;
  MenuMark markEndMenu;
} Menu_ViewSquad;
SWOS_PACK_POP

/* 220 - Menu 31: buyOtherPlayersMenu (0x210 = 528 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_BuyOtherPlayers
{
  MenuHeader hdr;
  MenuMark markByteCoords;
  MenuEntry_Byte_Colors_TextPtr entry0;
  MenuEntry_Byte_Colors_TextPtr entry1;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry2;
  MenuMark_StartMenu menu1Start;
  MenuMark template1_markStart;
  MenuCoords8 template1_rc;
  MenuCmd_Colors template1_colors;
  MenuCmd_BeforeDraw template1_afterDraw;
  MenuCmd_OnSelect template1_onSelect;
  MenuCommand template1_end;
  MenuMark markWideCoords;
  MenuEntry_Wide_NavOnly entries3_18[16];
  MenuMark markInitEntry;
  MenuMark_StartMenu menu2Start;
  MenuCoords16 entry19_rc;
  MenuCmd_Colors entry19_colors;
  MenuCommand entry19_end;
  MenuMark markByteCoords2;
  MenuEntry_Byte_NumberOnly entry20;
  MenuEntry_Byte_SpriteFgOnly entry21;
  MenuEntry_Byte_TextPtrOnly entry22;
  MenuEntry_Byte_TextNoPtrOnly entry23;
  MenuEntry_Byte_TextPtrOnly entry24;
  MenuEntry_Byte_TextNoPtrOnly entry25;
  MenuEntry_Byte_SpriteFgOnly entries26_28[3];
  MenuEntry_Byte_TextNoPtrOnly entry29;
  MenuMark markWideCoords2;
  MenuCoords16 entry30_rc;
  MenuCmd_TextPtr entry30_text;
  MenuCommand entry30_end;
  MenuMark markEndMenu;
} Menu_BuyOtherPlayers;
SWOS_PACK_POP

/* 221 - Menu 32: showPlayerMarketMenu (0x256 = 598 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_ShowPlayerMarket
{
  MenuHeader hdr;
  MenuMark markByteCoords;
  MenuEntry_Byte_Colors_TextPtr entry0;
  MenuEntry_Byte_TextNoPtrOnly entry1;
  MenuEntry_Byte_TextPtrOnly entry2;
  MenuEntry_Byte_Colors_TextPtr entry3;
  MenuEntry_Byte_Nav_Colors_StringTable_OnSelect entry4;
  MenuEntry_Byte_Colors_TextPtr entry5;
  MenuCoords8 entry6_rc;
  MenuCmd_Nav entry6_nav;
  MenuCmd_Skip2 entry6_skip;
  MenuCmd_Colors entry6_colors;
  MenuCmd_StringTable entry6_strTab;
  MenuCmd_OnSelect entry6_onSelect;
  MenuCommand entry6_end;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry7;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry8;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry9;
  MenuMark_StartMenu menu1Start;
  MenuMark template1_markStart;
  MenuCoords8 template1_rc;
  MenuCmd_Colors template1_colors;
  MenuCmd_BeforeDraw template1_afterDraw;
  MenuCmd_OnSelect template1_onSelect;
  MenuCommand template1_end;
  MenuMark markWideCoords;
  MenuEntry_Wide_NavOnly entries10_19[10];
  MenuMark markInitEntry;
  MenuMark_StartMenu menu2Start;
  MenuCoords16 entry20_rc;
  MenuCmd_Colors entry20_colors;
  MenuCommand entry20_end;
  MenuMark markByteCoords2;
  MenuEntry_Byte_NumberOnly entry21;
  MenuEntry_Byte_SpriteFgOnly entry22;
  MenuEntry_Byte_TextPtrOnly entry23;
  MenuEntry_Byte_TextNoPtrOnly entry24;
  MenuEntry_Byte_TextPtrOnly entry25;
  MenuEntry_Byte_TextNoPtrOnly entry26;
  MenuEntry_Byte_SpriteFgOnly entries27_29[3];
  MenuEntry_Byte_TextNoPtrOnly entry30;
  MenuMark markWideCoords2;
  MenuCoords16 entry31_rc;
  MenuCmd_TextPtr entry31_text;
  MenuCommand entry31_end;
  MenuMark markEndMenu;
} Menu_ShowPlayerMarket;
SWOS_PACK_POP

/* 222 - Menu 12: topGoalScorersMenu (0x1A6 = 422 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_TopGoalScorers
{
  MenuHeader hdr;
  MenuMark markByteCoords;
  MenuEntry_Byte_Colors_TextPtr entry0;
  MenuCoords8 entry1_rc;
  MenuCmd_TextPtr entry1_multilineText;
  MenuCommand entry1_end;
  MenuMark markWideCoords;
  MenuEntry_Wide_MultilineText entries2_4[3];
  MenuMark markByteCoords2;
  MenuCoords8 entry5_rc;
  MenuCmd_Colors entry5_colors;
  MenuCmd_TextPtr entry5_text;
  MenuCmd_OnSelect entry5_onSelect;
  MenuCommand entry5_end;
  MenuMark_StartMenu menu1Start;
  MenuMark template1_markStart;
  MenuCoords8 template1_rc;
  MenuCmd_BeforeDraw template1_beforeDraw;
  MenuCommand template1_end;
  MenuEntry_Byte_NumberOnly entries6_20[15];
  MenuMark markInitEntry;
  MenuEntry_Byte_TextPtrOnly entry21;
  MenuEntry_Byte_TextNoPtrOnly entry22;
  MenuEntry_Byte_NumberOnly entry23;
  MenuMark markWideCoords2;
  MenuEntry_Wide_NumberOnly entries24_26[3];
  MenuMark markEndMenu;
} Menu_TopGoalScorers;
SWOS_PACK_POP

/* 223 - Menu 28: thirdPartyOfferMenu (0x220 = 544 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_ThirdPartyOffer
{
  MenuHeader hdr;
  MenuMark markByteCoords;
  MenuEntry_Byte_Colors_TextPtr entry0;
  MenuMark markWideCoords;
  MenuEntry_Wide_TextNoPtrOnly entries1_2[2];
  MenuCoords16 entry3_rc;
  MenuCmd_Invisible entry3_invisible;
  MenuCmd_TextNoPtr entry3_text;
  MenuCommand entry3_end;
  MenuEntry_Wide_TextNoPtrOnly entry4;
  MenuMark markByteCoords2;
  MenuEntry_Byte_TextPtrOnly entries5_7[3];
  MenuEntry_Byte_TextNoPtrOnly entry8;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entries9_10[2];
  MenuCoords8 entry11_rc;
  MenuCmd_Nav entry11_nav;
  MenuCmd_Skip2 entry11_rightSkip;
  MenuCmd_Skip2 entry11_leftSkip;
  MenuCmd_Colors entry11_colors;
  MenuCmd_TextPtr entry11_text;
  MenuCmd_OnSelect entry11_onSelect;
  MenuCommand entry11_end;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entries12_13[2];
  MenuCoords8 entry14_rc;
  MenuCmd_Invisible entry14_invisible;
  MenuCmd_Colors entry14_colors;
  MenuCmd_TextPtr entry14_text;
  MenuCmd_OnSelect entry14_onSelect;
  MenuCommand entry14_end;
  MenuMark template15_markStart;
  MenuCoords8 template15_rc;
  MenuCmd_Colors template15_colors;
  MenuCmd_AfterDraw template15_afterDraw;
  MenuCommand template15_end;
  MenuMark markWideCoords2;
  MenuEntry_Wide_CoordsOnly entries15_18[4];
  MenuMark markInitEntry;
  MenuEntry_Wide_ColorsOnly entry19;
  MenuMark markByteCoords3;
  MenuEntry_Byte_NumberOnly entry20;
  MenuEntry_Byte_SpriteFgOnly entry21;
  MenuEntry_Byte_TextPtrOnly entry22;
  MenuEntry_Byte_TextNoPtrOnly entry23;
  MenuEntry_Byte_TextPtrOnly entry24;
  MenuEntry_Byte_TextNoPtrOnly entry25;
  MenuEntry_Byte_SpriteFgOnly entries26_28[3];
  MenuEntry_Byte_TextNoPtrOnly entry29;
  MenuMark markWideCoords3;
  MenuEntry_Wide_TextPtrOnly entry30;
  MenuMark markEndMenu;
} Menu_ThirdPartyOffer;
SWOS_PACK_POP

/* 224 - Menu 29: playerOffersMenu (0x20E = 526 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_PlayerOffers
{
  MenuHeader hdr;
  MenuMark markByteCoords;
  MenuEntry_Byte_Colors_TextPtr entry0;
  MenuEntry_Byte_TextNoPtrOnly entry1;
  MenuMark markWideCoords;
  MenuEntry_Wide_TextPtrOnly entry2;
  MenuEntry_Wide_TextNoPtrOnly entries3_4[2];
  MenuMark markByteCoords2;
  MenuEntry_Byte_TextPtrOnly entries5_7[3];
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry8;
  MenuCoords8 entry9_rc;
  MenuCmd_Nav entry9_nav;
  MenuCmd_Colors entry9_colors;
  MenuCmd_TextNoPtr entry9_text;
  MenuCmd_InputFunc entry9_inputFunc;
  MenuCommand entry9_end;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entries10_13[4];
  MenuMark template14_markStart;
  MenuCoords8 template14_rc;
  MenuCmd_Colors template14_colors;
  MenuCmd_AfterDraw template14_afterDraw;
  MenuCommand template14_end;
  MenuMark markWideCoords2;
  MenuEntry_Wide_CoordsOnly entries14_17[4];
  MenuMark markInitEntry;
  MenuEntry_Wide_ColorsOnly entry18;
  MenuMark markByteCoords3;
  MenuEntry_Byte_NumberOnly entry19;
  MenuEntry_Byte_SpriteFgOnly entry20;
  MenuEntry_Byte_TextPtrOnly entry21;
  MenuEntry_Byte_TextNoPtrOnly entry22;
  MenuEntry_Byte_TextPtrOnly entry23;
  MenuEntry_Byte_TextNoPtrOnly entry24;
  MenuEntry_Byte_SpriteFgOnly entries25_27[3];
  MenuEntry_Byte_TextNoPtrOnly entry28;
  MenuMark markWideCoords3;
  MenuEntry_Wide_TextPtrOnly entry29;
  MenuMark markEndMenu;
} Menu_PlayerOffers;
SWOS_PACK_POP

/* 225 - Menu 44: importTacticsMenu (0x1EA = 490 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_ImportTactics
{
  MenuHeader hdr;
  MenuMark markByteCoords;
  MenuEntry_Byte_Colors_TextPtr entry0;
  MenuMark_StartMenu menu1Start;
  MenuMark template1_markStart;
  MenuCoords8 template1_rc;
  MenuCmd_Colors template1_colors;
  MenuCmd_OnSelect template1_onSelect;
  MenuCommand template1_end;
  MenuEntry_Byte_Nav_TextPtr entries1_18[18];
  MenuMark markInitEntry;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry19;
  MenuMark_StartMenu menu2Start;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry20;
  MenuMark markEndMenu;
} Menu_ImportTactics;
SWOS_PACK_POP

/* 226 - Menu 10: editTeamsMenu (0x676 = 1654 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_EditTeams
{
  MenuHeader hdr;
  MenuMark markByteCoords;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry0;
  MenuMark markWideCoords;
  MenuCoords16 entry1_rc;
  MenuCmd_Invisible entry1_invisible;
  MenuCmd_Colors entry1_colors;
  MenuCmd_Number entry1_number;
  MenuCommand entry1_end;
  MenuMark markByteCoords2;
  MenuEntry_Byte_TextPtrOnly entry2;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry3;
  MenuMark_StartMenu menu1Start;
  MenuEntry_Byte_TextPtrOnly entry4;
  MenuCoords8 entry5_rc;
  MenuCmd_Colors entry5_colors;
  MenuCmd_SpriteCopy entry5_spriteCopy;
  MenuCmd_Nav entry5_nav;
  MenuCmd_InputFunc entry5_inputFunc;
  MenuCommand entry5_end;
  MenuCoords8 entry6_rc;
  MenuCmd_Colors entry6_colors;
  MenuCmd_SpriteCopy entry6_spriteCopy;
  MenuCmd_Nav entry6_nav;
  MenuCmd_InputFunc entry6_inputFunc;
  MenuCommand entry6_end;
  MenuCoords8 entry7_rc;
  MenuCmd_Colors entry7_colors;
  MenuCmd_SpriteCopy entry7_spriteCopy;
  MenuCmd_Nav entry7_nav;
  MenuCmd_InputFunc entry7_inputFunc;
  MenuCommand entry7_end;
  MenuMark_StartMenu menu1Start2;
  MenuEntry_Byte_TextPtrOnly entry8;
  MenuCoords8 entry9_rc;
  MenuCmd_Colors entry9_colors;
  MenuCmd_SpriteCopy entry9_spriteCopy;
  MenuCmd_Nav entry9_nav;
  MenuCmd_InputFunc entry9_inputFunc;
  MenuCommand entry9_end;
  MenuCoords8 entry10_rc;
  MenuCmd_Colors entry10_colors;
  MenuCmd_SpriteCopy entry10_spriteCopy;
  MenuCmd_Nav entry10_nav;
  MenuCmd_InputFunc entry10_inputFunc;
  MenuCommand entry10_end;
  MenuCoords8 entry11_rc;
  MenuCmd_Colors entry11_colors;
  MenuCmd_SpriteCopy entry11_spriteCopy;
  MenuCmd_Nav entry11_nav;
  MenuCmd_InputFunc entry11_inputFunc;
  MenuCommand entry11_end;
  MenuMark_StartMenu menu1Start3;
  MenuCoords8 entry12_rc;
  MenuCmd_Nav entry12_nav;
  MenuCmd_Colors entry12_colors;
  MenuCmd_SpriteFg entry12_sprite;
  MenuCmd_OnSelect entry12_onSelect;
  MenuCommand entry12_end;
  MenuCoords8 entry13_rc;
  MenuCmd_Nav entry13_nav;
  MenuCmd_Colors entry13_colors;
  MenuCmd_SpriteFg entry13_sprite;
  MenuCmd_OnSelect entry13_onSelect;
  MenuCommand entry13_end;
  MenuCoords8 entry14_rc;
  MenuCmd_Nav entry14_nav;
  MenuCmd_Colors entry14_colors;
  MenuCmd_SpriteFg entry14_sprite;
  MenuCmd_OnSelect entry14_onSelect;
  MenuCommand entry14_end;
  MenuCoords8 entry15_rc;
  MenuCmd_Nav entry15_nav;
  MenuCmd_Colors entry15_colors;
  MenuCmd_SpriteFg entry15_sprite;
  MenuCmd_OnSelect entry15_onSelect;
  MenuCommand entry15_end;
  MenuMark_StartMenu menu1Start4;
  MenuCoords8 entry16_rc;
  MenuCmd_Nav entry16_nav;
  MenuCmd_Colors entry16_colors;
  MenuCmd_SpriteFg entry16_sprite;
  MenuCmd_OnSelect entry16_onSelect;
  MenuCommand entry16_end;
  MenuCoords8 entry17_rc;
  MenuCmd_Nav entry17_nav;
  MenuCmd_Colors entry17_colors;
  MenuCmd_SpriteFg entry17_sprite;
  MenuCmd_OnSelect entry17_onSelect;
  MenuCommand entry17_end;
  MenuCoords8 entry18_rc;
  MenuCmd_Nav entry18_nav;
  MenuCmd_Colors entry18_colors;
  MenuCmd_SpriteFg entry18_sprite;
  MenuCmd_OnSelect entry18_onSelect;
  MenuCommand entry18_end;
  MenuCoords8 entry19_rc;
  MenuCmd_Nav entry19_nav;
  MenuCmd_Colors entry19_colors;
  MenuCmd_SpriteFg entry19_sprite;
  MenuCmd_OnSelect entry19_onSelect;
  MenuCommand entry19_end;
  MenuMark_StartMenu menu1Start5;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry20;
  MenuMark markWideCoords2;
  MenuCoords16 entry21_rc;
  MenuCmd_Nav entry21_nav;
  MenuCmd_Colors entry21_colors;
  MenuCmd_TextPtr entry21_text;
  MenuCmd_OnSelect entry21_onSelect;
  MenuCommand entry21_end;
  MenuMark markByteCoords3;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry22;
  MenuMark template23_markStart;
  MenuCoords8 template23_rc;
  MenuCmd_BeforeDraw template23_beforeDraw;
  MenuCommand template23_end;
  MenuMark_StartMenu menu1Start6;
  MenuCoords8 entry23_rc;
  MenuCmd_Nav entry23_nav;
  MenuCmd_Colors entry23_colors;
  MenuCmd_SpriteFg entry23_sprite;
  MenuCmd_OnSelect entry23_onSelect;
  MenuCommand entry23_end;
  MenuCoords8 entry24_rc;
  MenuCmd_Nav entry24_nav;
  MenuCmd_Colors entry24_colors;
  MenuCmd_SpriteFg entry24_sprite;
  MenuCmd_OnSelect entry24_onSelect;
  MenuCommand entry24_end;
  MenuCoords8 entry25_rc;
  MenuCmd_Nav entry25_nav;
  MenuCmd_Colors entry25_colors;
  MenuCmd_SpriteFg entry25_sprite;
  MenuCmd_OnSelect entry25_onSelect;
  MenuCommand entry25_end;
  MenuCoords8 entry26_rc;
  MenuCmd_Nav entry26_nav;
  MenuCmd_Colors entry26_colors;
  MenuCmd_SpriteFg entry26_sprite;
  MenuCmd_OnSelect entry26_onSelect;
  MenuCommand entry26_end;
  MenuCoords8 entry27_rc;
  MenuCmd_Nav entry27_nav;
  MenuCmd_Colors entry27_colors;
  MenuCmd_SpriteFg entry27_sprite;
  MenuCmd_OnSelect entry27_onSelect;
  MenuCommand entry27_end;
  MenuCoords8 entry28_rc;
  MenuCmd_Nav entry28_nav;
  MenuCmd_Colors entry28_colors;
  MenuCmd_SpriteFg entry28_sprite;
  MenuCmd_OnSelect entry28_onSelect;
  MenuCommand entry28_end;
  MenuCoords8 entry29_rc;
  MenuCmd_Nav entry29_nav;
  MenuCmd_Colors entry29_colors;
  MenuCmd_SpriteFg entry29_sprite;
  MenuCmd_OnSelect entry29_onSelect;
  MenuCommand entry29_end;
  MenuCoords8 entry30_rc;
  MenuCmd_Nav entry30_nav;
  MenuCmd_Colors entry30_colors;
  MenuCmd_SpriteFg entry30_sprite;
  MenuCmd_OnSelect entry30_onSelect;
  MenuCommand entry30_end;
  MenuCoords8 entry31_rc;
  MenuCmd_Nav entry31_nav;
  MenuCmd_Colors entry31_colors;
  MenuCmd_SpriteFg entry31_sprite;
  MenuCmd_OnSelect entry31_onSelect;
  MenuCommand entry31_end;
  MenuCoords8 entry32_rc;
  MenuCmd_Nav entry32_nav;
  MenuCmd_Colors entry32_colors;
  MenuCmd_SpriteFg entry32_sprite;
  MenuCmd_OnSelect entry32_onSelect;
  MenuCommand entry32_end;
  MenuCoords8 entry33_rc;
  MenuCmd_Nav entry33_nav;
  MenuCmd_Colors entry33_colors;
  MenuCmd_SpriteFg entry33_sprite;
  MenuCmd_OnSelect entry33_onSelect;
  MenuCommand entry33_end;
  MenuCoords8 entry34_rc;
  MenuCmd_Nav entry34_nav;
  MenuCmd_Colors entry34_colors;
  MenuCmd_SpriteFg entry34_sprite;
  MenuCmd_OnSelect entry34_onSelect;
  MenuCommand entry34_end;
  MenuCoords8 entry35_rc;
  MenuCmd_Nav entry35_nav;
  MenuCmd_Colors entry35_colors;
  MenuCmd_SpriteFg entry35_sprite;
  MenuCmd_OnSelect entry35_onSelect;
  MenuCommand entry35_end;
  MenuCoords8 entry36_rc;
  MenuCmd_Nav entry36_nav;
  MenuCmd_Colors entry36_colors;
  MenuCmd_SpriteFg entry36_sprite;
  MenuCmd_OnSelect entry36_onSelect;
  MenuCommand entry36_end;
  MenuCoords8 entry37_rc;
  MenuCmd_Nav entry37_nav;
  MenuCmd_Colors entry37_colors;
  MenuCmd_SpriteFg entry37_sprite;
  MenuCmd_OnSelect entry37_onSelect;
  MenuCommand entry37_end;
  MenuCoords8 entry38_rc;
  MenuCmd_Nav entry38_nav;
  MenuCmd_Colors entry38_colors;
  MenuCmd_SpriteFg entry38_sprite;
  MenuCmd_OnSelect entry38_onSelect;
  MenuCommand entry38_end;
  MenuMark markInitEntry;
  MenuMark_StartMenu menu1Start7;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entries39_54[16];
  MenuEntry_Byte_NumberOnly entry55;
  MenuEntry_Byte_ColorsOnly entry56;
  MenuEntry_Byte_TextPtrOnly entry57;
  MenuEntry_Byte_SpriteFgOnly entries58_60[3];
  MenuMark markEndMenu;
} Menu_EditTeams;
SWOS_PACK_POP

/* 227 - Menu 11: competitionTopGoalScorersMenu (0x2EE = 750 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_CompetitionTopGoalScorers
{
  MenuHeader hdr;
  MenuCoords16 entry0_rc;
  MenuCmd_Colors entry0_colors;
  MenuCmd_TextPtr entry0_text;
  MenuCommand entry0_end;
  MenuMark markByteCoords;
  MenuEntry_Byte_Colors_TextPtr entry1;
  MenuCoords8 entry2_rc;
  MenuCmd_Colors entry2_colors;
  MenuCmd_TextPtr entry2_text;
  MenuCmd_OnSelect entry2_onSelect;
  MenuCommand entry2_end;
  MenuMark_StartMenu menu1Start;
  MenuEntry_Byte_NumberOnly entry3;
  MenuEntry_Byte_TextNoPtrOnly entries4_5[2];
  MenuEntry_Byte_NumberOnly entries6_7[2];
  MenuEntry_Byte_TextNoPtrOnly entries8_9[2];
  MenuEntry_Byte_NumberOnly entries10_11[2];
  MenuEntry_Byte_TextNoPtrOnly entries12_13[2];
  MenuEntry_Byte_NumberOnly entries14_15[2];
  MenuEntry_Byte_TextNoPtrOnly entries16_17[2];
  MenuEntry_Byte_NumberOnly entries18_19[2];
  MenuEntry_Byte_TextNoPtrOnly entries20_21[2];
  MenuEntry_Byte_NumberOnly entries22_23[2];
  MenuEntry_Byte_TextNoPtrOnly entries24_25[2];
  MenuEntry_Byte_NumberOnly entries26_27[2];
  MenuEntry_Byte_TextNoPtrOnly entries28_29[2];
  MenuEntry_Byte_NumberOnly entries30_31[2];
  MenuEntry_Byte_TextNoPtrOnly entries32_33[2];
  MenuEntry_Byte_NumberOnly entries34_35[2];
  MenuEntry_Byte_TextNoPtrOnly entries36_37[2];
  MenuEntry_Byte_NumberOnly entries38_39[2];
  MenuEntry_Byte_TextNoPtrOnly entries40_41[2];
  MenuEntry_Byte_NumberOnly entries42_43[2];
  MenuEntry_Byte_TextNoPtrOnly entries44_45[2];
  MenuEntry_Byte_NumberOnly entries46_47[2];
  MenuEntry_Byte_TextNoPtrOnly entries48_49[2];
  MenuEntry_Byte_NumberOnly entries50_51[2];
  MenuEntry_Byte_TextNoPtrOnly entries52_53[2];
  MenuEntry_Byte_NumberOnly entries54_55[2];
  MenuEntry_Byte_TextNoPtrOnly entries56_57[2];
  MenuEntry_Byte_NumberOnly entries58_59[2];
  MenuEntry_Byte_TextNoPtrOnly entries60_61[2];
  MenuEntry_Byte_NumberOnly entry62;
  MenuMark markEndMenu;
} Menu_CompetitionTopGoalScorers;
SWOS_PACK_POP

/* 228 - Menu 13: squadMenu (0x3EC = 1004 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_Squad
{
  MenuHeader hdr;
  MenuMark markByteCoords;
  MenuCoords8 entry0_rc;
  MenuCmd_Invisible entry0_invisible;
  MenuCmd_Colors entry0_colors;
  MenuCmd_Number entry0_number;
  MenuCommand entry0_end;
  MenuCoords8 entry1_rc;
  MenuCmd_Colors entry1_colors;
  MenuCmd_SpriteFg entry1_sprite;
  MenuCommand entry1_end;
  MenuEntry_Byte_Colors_TextPtr entry2;
  MenuEntry_Byte_TextNoPtrOnly entry3;
  MenuEntry_Byte_TextPtrOnly entries4_5[2];
  MenuCoords8 entry6_rc;
  MenuCmd_Nav entry6_nav;
  MenuCmd_Skip2 entry6_downSkip;
  MenuCmd_Colors entry6_colors;
  MenuCmd_TextPtr entry6_text;
  MenuCmd_OnSelect entry6_onSelect;
  MenuCommand entry6_end;
  MenuCoords8 entry7_rc;
  MenuCmd_Nav entry7_nav;
  MenuCmd_Invisible entry7_invisible;
  MenuCommand entry7_end;
  MenuCoords8 entry8_rc;
  MenuCmd_Nav entry8_nav;
  MenuCmd_Skip2 entry8_leftSkip;
  MenuCmd_Skip2 entry8_rightSkip;
  MenuCmd_Colors entry8_colors;
  MenuCmd_TextPtr entry8_text;
  MenuCmd_OnSelect entry8_onSelect;
  MenuCommand entry8_end;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry9;
  MenuCoords8 entry10_rc;
  MenuCmd_Invisible entry10_invisible;
  MenuCmd_Colors entry10_colors;
  MenuCmd_TextPtr entry10_text;
  MenuCmd_OnSelect entry10_onSelect;
  MenuCommand entry10_end;
  MenuCoords8 entry11_rc;
  MenuCmd_OnSelect entry11_onSelect;
  MenuCommand entry11_end;
  MenuEntry_Byte_TextNoPtrOnly entry12;
  MenuEntry_Byte_TextPtrOnly entries13_15[3];
  MenuEntry_Byte_TextNoPtrOnly entry16;
  MenuEntry_Byte_TextPtrOnly entries17_19[3];
  MenuCoords8 entry20_rc;
  MenuCmd_Invisible entry20_invisible;
  MenuCmd_TextPtr entry20_text;
  MenuCommand entry20_end;
  MenuMark markWideCoords;
  MenuEntry_Wide_TextPtrOnly entries21_22[2];
  MenuEntry_Wide_ColorsOnly entry23;
  MenuEntry_Wide_TextPtrOnly entry24;
  MenuEntry_Wide_NumberOnly entry25;
  MenuEntry_Wide_TextPtrOnly entry26;
  MenuEntry_Wide_NumberOnly entry27;
  MenuMark template28_markStart;
  uint16_t template28_coordsMark;
  MenuCoords8 template28_rc;
  MenuCmd_Colors template28_colors;
  MenuCmd_AfterDraw template28_afterDraw;
  MenuCmd_OnSelect template28_onSelect;
  MenuCommand template28_end;
  MenuEntry_Byte_NavOnly entries28_43[16];
  MenuCoords8 entry44_rc;
  MenuCmd_Nav entry44_nav;
  MenuCmd_Skip2 entry44_upSkip;
  MenuCmd_Invisible entry44_invisible;
  MenuCommand entry44_end;
  MenuMark markInitEntry;
  MenuCoords8 entry45_rc;
  MenuCmd_Invisible entry45_invisible;
  MenuCmd_Nav entry45_nav;
  MenuCmd_Colors entry45_colors;
  MenuCmd_OnSelect entry45_onSelect;
  MenuCmd_AfterDraw entry45_afterDraw;
  MenuCommand entry45_end;
  MenuCoords8 entry46_rc;
  MenuCmd_Colors entry46_colors;
  MenuCmd_SpriteFg entry46_sprite;
  MenuCommand entry46_end;
  MenuEntry_Byte_NumberOnly entry47;
  MenuEntry_Byte_Colors_TextPtr entry48;
  MenuEntry_Byte_TextPtrOnly entry49;
  MenuEntry_Byte_TextNoPtrOnly entry50;
  MenuEntry_Byte_SpriteFgOnly entries51_53[3];
  MenuEntry_Byte_TextNoPtrOnly entry54;
  MenuEntry_Byte_NumberOnly entry55;
  MenuCoords8 entry56_rc;
  MenuCmd_FgFunc entry56_fgFunc;
  MenuCommand entry56_end;
  MenuMark markWideCoords2;
  MenuEntry_Wide_TextPtrOnly entry57;
  MenuMark markByteCoords2;
  MenuEntry_Byte_TextNoPtrOnly entry58;
  MenuEntry_Byte_NumberOnly entries59_61[3];
  MenuMark markWideCoords3;
  MenuEntry_Wide_NumberOnly entries62_63[2];
  MenuMark markEndMenu;
} Menu_Squad;
SWOS_PACK_POP

/* 229 - Menu 17: contestMenu (0x32C = 812 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_Contest
{
  MenuHeader hdr;
  MenuMark markByteCoords;
  MenuCoords8 entry0_rc;
  MenuCmd_Colors entry0_colors;
  MenuCmd_TextNoPtr entry0_text;
  MenuCommand entry0_end;
  MenuCoords8 entry1_rc;
  MenuCmd_Colors entry1_colors;
  MenuCmd_TextNoPtr entry1_text;
  MenuCommand entry1_end;
  MenuCoords8 entry2_rc;
  MenuCmd_Colors entry2_colors;
  MenuCmd_TextNoPtr entry2_text;
  MenuCommand entry2_end;
  MenuMark markWideCoords;
  MenuCoords16 entry3_rc;
  MenuCmd_Colors entry3_colors;
  MenuCmd_TextPtr entry3_text;
  MenuCommand entry3_end;
  MenuMark markByteCoords2;
  MenuCoords8 entry4_rc;
  MenuCmd_Nav entry4_nav;
  MenuCmd_Skip2 entry4_upSkip;
  MenuCmd_Colors entry4_colors;
  MenuCmd_TextPtr entry4_text;
  MenuCmd_OnSelect entry4_onSelect;
  MenuCommand entry4_end;
  MenuCoords8 entry5_rc;
  MenuCmd_Nav entry5_nav;
  MenuCmd_Skip2 entry5_upSkip;
  MenuCmd_Skip2 entry5_rightSkip;
  MenuCmd_Colors entry5_colors;
  MenuCmd_TextPtr entry5_text;
  MenuCmd_OnSelect entry5_onSelect;
  MenuCommand entry5_end;
  MenuCoords8 entry6_rc;
  MenuCmd_Nav entry6_nav;
  MenuCmd_Skip2 entry6_rightSkip;
  MenuCmd_Skip2 entry6_upSkip;
  MenuCmd_Colors entry6_colors;
  MenuCmd_TextPtr entry6_text;
  MenuCmd_OnSelect entry6_onSelect;
  MenuCommand entry6_end;
  MenuCoords8 entry7_rc;
  MenuCmd_Nav entry7_nav;
  MenuCmd_Skip2 entry7_upSkip;
  MenuCmd_Skip2 entry7_downSkip;
  MenuCmd_Colors entry7_colors;
  MenuCmd_TextPtr entry7_text;
  MenuCmd_OnSelect entry7_onSelect;
  MenuCommand entry7_end;
  MenuCoords8 entry8_rc;
  MenuCmd_Nav entry8_nav;
  MenuCmd_Skip2 entry8_leftSkip;
  MenuCmd_Colors entry8_colors;
  MenuCmd_TextPtr entry8_text;
  MenuCmd_OnSelect entry8_onSelect;
  MenuCommand entry8_end;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry9;
  MenuCoords8 entry10_rc;
  MenuCmd_Nav entry10_nav;
  MenuCmd_Skip2 entry10_downSkip;
  MenuCmd_Colors entry10_colors;
  MenuCmd_TextPtr entry10_text;
  MenuCmd_OnSelect entry10_onSelect;
  MenuCommand entry10_end;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry11;
  MenuCoords8 entry12_rc;
  MenuCmd_Nav entry12_nav;
  MenuCmd_Skip2 entry12_rightSkip;
  MenuCmd_Colors entry12_colors;
  MenuCmd_TextPtr entry12_text;
  MenuCmd_OnSelect entry12_onSelect;
  MenuCommand entry12_end;
  MenuCoords8 entry13_rc;
  MenuCmd_Invisible entry13_invisible;
  MenuCmd_Nav entry13_nav;
  MenuCmd_Colors entry13_colors;
  MenuCmd_TextPtr entry13_text;
  MenuCmd_OnSelect entry13_onSelect;
  MenuCommand entry13_end;
  MenuCoords8 entry14_rc;
  MenuCmd_Invisible entry14_invisible;
  MenuCmd_Nav entry14_nav;
  MenuCmd_Colors entry14_colors;
  MenuCmd_TextPtr entry14_text;
  MenuCmd_OnSelect entry14_onSelect;
  MenuCommand entry14_end;
  MenuCoords8 entry15_rc;
  MenuCmd_Invisible entry15_invisible;
  MenuCmd_Nav entry15_nav;
  MenuCmd_Colors entry15_colors;
  MenuCmd_TextPtr entry15_text;
  MenuCmd_OnSelect entry15_onSelect;
  MenuCommand entry15_end;
  MenuCoords8 entry16_rc;
  MenuCmd_Nav entry16_nav;
  MenuCmd_Colors entry16_colors;
  MenuCmd_SpriteFg entry16_sprite;
  MenuCmd_OnSelect entry16_onSelect;
  MenuCommand entry16_end;
  MenuCoords8 entry17_rc;
  MenuCmd_Nav entry17_nav;
  MenuCmd_Colors entry17_colors;
  MenuCmd_SpriteFg entry17_sprite;
  MenuCmd_OnSelect entry17_onSelect;
  MenuCommand entry17_end;
  MenuMark markWideCoords2;
  MenuEntry_Wide_ColorsOnly entry18;
  MenuMark_StartMenu menu1Start;
  MenuMark template19_markStart;
  uint16_t template19_coordsMark;
  MenuCoords8 template19_rc;
  MenuCmd_BeforeDraw template19_beforeDraw;
  MenuCommand template19_end;
  MenuEntry_Byte_CoordsOnly entries19_32[14];
  MenuMark markInitEntry;
  MenuMark markWideCoords3;
  MenuEntry_Wide_ColorsOnly entry33;
  MenuMark markByteCoords3;
  MenuEntry_Byte_TextPtrOnly entry34;
  MenuEntry_Byte_TextNoPtrOnly entry35;
  MenuEntry_Byte_TextPtrOnly entry36;
  MenuEntry_Byte_ColorsOnly entry37;
  MenuEntry_Byte_TextPtrOnly entry38;
  MenuEntry_Byte_NumberOnly entry39;
  MenuEntry_Byte_TextPtrOnly entry40;
  MenuEntry_Byte_NumberOnly entry41;
  MenuEntry_Byte_TextPtrOnly entry42;
  MenuEntry_Byte_TextNoPtrOnly entry43;
  MenuMark markEndMenu;
} Menu_Contest;
SWOS_PACK_POP

/* 230 - Menu 19: selectTeamsForTacticsMenu (0x434 = 1076 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_SelectTeamsForTactics
{
  MenuHeader hdr;
  MenuMark markByteCoords;
  MenuEntry_Byte_Colors_TextPtr entry0;
  MenuMark_StartMenu menu1Start;
  MenuMark template1_markStart;
  MenuCoords8 template1_rc;
  MenuCmd_Colors template1_colors;
  MenuCmd_TextPtr template1_text;
  MenuCmd_OnSelect template1_onSelect;
  MenuCommand template1_end;
  MenuEntry_Byte_Nav_Skip entries1_22[22];
  MenuEntry_Byte_NavOnly entries23_44[22];
  MenuEntry_Byte_Nav_Skip entries45_66[22];
  MenuMark markInitEntry;
  MenuMark_StartMenu menu2Start;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry67;
  MenuMark markEndMenu;
} Menu_SelectTeamsForTactics;
SWOS_PACK_POP

/* 231 - Menu 20: selectTeamsMenu (0x5A4 = 1444 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_SelectTeams
{
  MenuHeader hdr;
  MenuCoords16 entry0_rc;
  MenuCmd_Colors entry0_colors;
  MenuCmd_TextNoPtr entry0_text;
  MenuCommand entry0_end;
  MenuMark template1_markStart;
  uint16_t template1_coordsMark;
  MenuCoords8 template1_rc;
  MenuCmd_Colors template1_colors;
  MenuCmd_TextPtr template1_text;
  MenuCmd_Invisible template1_invisible;
  MenuCmd_OnSelect template1_onSelect;
  MenuCommand template1_end;
  MenuMark_StartMenu menu1Start;
  MenuEntry_Byte_NavOnly entries1_10[10];
  MenuMark_StartMenu menu1Start2;
  MenuEntry_Byte_NavOnly entries11_20[10];
  MenuMark template21_markStart;
  MenuCoords8 template21_rc;
  MenuCmd_Colors template21_colors;
  MenuCmd_TextPtr template21_text;
  MenuCmd_OnSelect template21_onSelect;
  MenuCommand template21_end;
  MenuCoords8 entry21_rc;
  MenuCmd_Nav entry21_nav;
  MenuCmd_Skip2 entry21_leftSkip;
  MenuCmd_Skip2 entry21_rightSkip;
  MenuCommand entry21_end;
  MenuCoords8 entry22_rc;
  MenuCmd_Nav entry22_nav;
  MenuCmd_Skip2 entry22_upSkip;
  MenuCommand entry22_end;
  MenuCoords8 entry23_rc;
  MenuCmd_Nav entry23_nav;
  MenuCmd_Skip2 entry23_upSkip;
  MenuCommand entry23_end;
  MenuCoords8 entry24_rc;
  MenuCmd_Nav entry24_nav;
  MenuCmd_Skip2 entry24_upSkip;
  MenuCommand entry24_end;
  MenuCoords8 entry25_rc;
  MenuCmd_Nav entry25_nav;
  MenuCmd_Skip2 entry25_upSkip;
  MenuCommand entry25_end;
  MenuCoords8 entry26_rc;
  MenuCmd_Nav entry26_nav;
  MenuCmd_Skip2 entry26_upSkip;
  MenuCommand entry26_end;
  MenuCoords8 entry27_rc;
  MenuCmd_Nav entry27_nav;
  MenuCmd_Skip2 entry27_upSkip;
  MenuCommand entry27_end;
  MenuCoords8 entry28_rc;
  MenuCmd_Nav entry28_nav;
  MenuCmd_Skip2 entry28_upSkip;
  MenuCommand entry28_end;
  MenuCoords8 entry29_rc;
  MenuCmd_Nav entry29_nav;
  MenuCmd_Skip2 entry29_upSkip;
  MenuCommand entry29_end;
  MenuCoords8 entry30_rc;
  MenuCmd_Nav entry30_nav;
  MenuCmd_Skip2 entry30_upSkip;
  MenuCommand entry30_end;
  MenuCoords8 entry31_rc;
  MenuCmd_Nav entry31_nav;
  MenuCmd_Skip2 entry31_upSkip;
  MenuCommand entry31_end;
  MenuCoords8 entry32_rc;
  MenuCmd_Nav entry32_nav;
  MenuCmd_Skip2 entry32_upSkip;
  MenuCommand entry32_end;
  MenuCoords8 entry33_rc;
  MenuCmd_Nav entry33_nav;
  MenuCmd_Skip2 entry33_upSkip;
  MenuCommand entry33_end;
  MenuCoords8 entry34_rc;
  MenuCmd_Nav entry34_nav;
  MenuCmd_Skip2 entry34_upSkip;
  MenuCommand entry34_end;
  MenuCoords8 entry35_rc;
  MenuCmd_Nav entry35_nav;
  MenuCmd_Skip2 entry35_upSkip;
  MenuCommand entry35_end;
  MenuCoords8 entry36_rc;
  MenuCmd_Nav entry36_nav;
  MenuCmd_Skip2 entry36_upSkip;
  MenuCommand entry36_end;
  MenuCoords8 entry37_rc;
  MenuCmd_Nav entry37_nav;
  MenuCmd_Skip2 entry37_upSkip;
  MenuCommand entry37_end;
  MenuCoords8 entry38_rc;
  MenuCmd_Nav entry38_nav;
  MenuCmd_Skip2 entry38_upSkip;
  MenuCommand entry38_end;
  MenuCoords8 entry39_rc;
  MenuCmd_Nav entry39_nav;
  MenuCmd_Skip2 entry39_upSkip;
  MenuCommand entry39_end;
  MenuCoords8 entry40_rc;
  MenuCmd_Nav entry40_nav;
  MenuCmd_Skip2 entry40_upSkip;
  MenuCommand entry40_end;
  MenuCoords8 entry41_rc;
  MenuCmd_Nav entry41_nav;
  MenuCmd_Skip2 entry41_upSkip;
  MenuCommand entry41_end;
  MenuCoords8 entry42_rc;
  MenuCmd_Nav entry42_nav;
  MenuCmd_Skip2 entry42_upSkip;
  MenuCommand entry42_end;
  MenuEntry_Byte_NavOnly entries43_64[22];
  MenuCoords8 entry65_rc;
  MenuCmd_Nav entry65_nav;
  MenuCmd_Skip2 entry65_leftSkip;
  MenuCmd_Skip2 entry65_rightSkip;
  MenuCommand entry65_end;
  MenuCoords8 entry66_rc;
  MenuCmd_Nav entry66_nav;
  MenuCmd_Skip2 entry66_downSkip;
  MenuCommand entry66_end;
  MenuCoords8 entry67_rc;
  MenuCmd_Nav entry67_nav;
  MenuCmd_Skip2 entry67_downSkip;
  MenuCommand entry67_end;
  MenuCoords8 entry68_rc;
  MenuCmd_Nav entry68_nav;
  MenuCmd_Skip2 entry68_downSkip;
  MenuCommand entry68_end;
  MenuCoords8 entry69_rc;
  MenuCmd_Nav entry69_nav;
  MenuCmd_Skip2 entry69_downSkip;
  MenuCommand entry69_end;
  MenuCoords8 entry70_rc;
  MenuCmd_Nav entry70_nav;
  MenuCmd_Skip2 entry70_downSkip;
  MenuCommand entry70_end;
  MenuCoords8 entry71_rc;
  MenuCmd_Nav entry71_nav;
  MenuCmd_Skip2 entry71_downSkip;
  MenuCommand entry71_end;
  MenuCoords8 entry72_rc;
  MenuCmd_Nav entry72_nav;
  MenuCmd_Skip2 entry72_downSkip;
  MenuCommand entry72_end;
  MenuCoords8 entry73_rc;
  MenuCmd_Nav entry73_nav;
  MenuCmd_Skip2 entry73_downSkip;
  MenuCommand entry73_end;
  MenuCoords8 entry74_rc;
  MenuCmd_Nav entry74_nav;
  MenuCmd_Skip2 entry74_downSkip;
  MenuCommand entry74_end;
  MenuCoords8 entry75_rc;
  MenuCmd_Nav entry75_nav;
  MenuCmd_Skip2 entry75_downSkip;
  MenuCommand entry75_end;
  MenuCoords8 entry76_rc;
  MenuCmd_Nav entry76_nav;
  MenuCmd_Skip2 entry76_downSkip;
  MenuCommand entry76_end;
  MenuCoords8 entry77_rc;
  MenuCmd_Nav entry77_nav;
  MenuCmd_Skip2 entry77_downSkip;
  MenuCommand entry77_end;
  MenuCoords8 entry78_rc;
  MenuCmd_Nav entry78_nav;
  MenuCmd_Skip2 entry78_downSkip;
  MenuCommand entry78_end;
  MenuCoords8 entry79_rc;
  MenuCmd_Nav entry79_nav;
  MenuCmd_Skip2 entry79_downSkip;
  MenuCommand entry79_end;
  MenuCoords8 entry80_rc;
  MenuCmd_Nav entry80_nav;
  MenuCmd_Skip2 entry80_downSkip;
  MenuCommand entry80_end;
  MenuCoords8 entry81_rc;
  MenuCmd_Nav entry81_nav;
  MenuCmd_Skip2 entry81_downSkip;
  MenuCommand entry81_end;
  MenuCoords8 entry82_rc;
  MenuCmd_Nav entry82_nav;
  MenuCmd_Skip2 entry82_downSkip;
  MenuCommand entry82_end;
  MenuCoords8 entry83_rc;
  MenuCmd_Nav entry83_nav;
  MenuCmd_Skip2 entry83_downSkip;
  MenuCommand entry83_end;
  MenuCoords8 entry84_rc;
  MenuCmd_Nav entry84_nav;
  MenuCmd_Skip2 entry84_downSkip;
  MenuCommand entry84_end;
  MenuCoords8 entry85_rc;
  MenuCmd_Nav entry85_nav;
  MenuCmd_Skip2 entry85_downSkip;
  MenuCommand entry85_end;
  MenuCoords8 entry86_rc;
  MenuCmd_Nav entry86_nav;
  MenuCmd_Skip2 entry86_downSkip;
  MenuCommand entry86_end;
  MenuMark markInitEntry;
  MenuMark_StartMenu menu1Start3;
  MenuCoords8 entry87_rc;
  MenuCmd_Nav entry87_nav;
  MenuCmd_Skip2 entry87_upSkip;
  MenuCmd_Skip2 entry87_rightSkip;
  MenuCmd_Colors entry87_colors;
  MenuCmd_TextPtr entry87_text;
  MenuCmd_OnSelect entry87_onSelect;
  MenuCommand entry87_end;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry88;
  MenuCoords8 entry89_rc;
  MenuCmd_Nav entry89_nav;
  MenuCmd_Skip2 entry89_downSkip;
  MenuCmd_Skip2 entry89_rightSkip;
  MenuCmd_Colors entry89_colors;
  MenuCmd_TextNoPtr entry89_text;
  MenuCmd_OnSelect entry89_onSelect;
  MenuCommand entry89_end;
  MenuCoords8 entry90_rc;
  MenuCmd_Colors entry90_colors;
  MenuCmd_TextNoPtr entry90_text;
  MenuCommand entry90_end;
  MenuMark markEndMenu;
} Menu_SelectTeams;
SWOS_PACK_POP

/* 232 - Menu 22: selectFilesMenu (0x35E = 862 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_SelectFiles
{
  MenuHeader hdr;
  MenuCoords16 entry0_rc;
  MenuCmd_Colors entry0_colors;
  MenuCmd_TextPtr entry0_text;
  MenuCommand entry0_end;
  MenuMark_StartMenu menu1Start;
  MenuMark template1_markStart;
  uint16_t template1_coordsMark;
  MenuCoords8 template1_rc;
  MenuCmd_Colors template1_colors;
  MenuCmd_TextNoPtr template1_text;
  MenuCmd_OnSelect template1_onSelect;
  MenuCmd_BeforeDraw template1_beforeDraw;
  MenuCommand template1_end;
  MenuEntry_Byte_NavOnly entry1;
  MenuCoords8 entry2_rc;
  MenuCmd_Nav entry2_nav;
  MenuCmd_Skip2 entry2_upSkip;
  MenuCommand entry2_end;
  MenuCoords8 entry3_rc;
  MenuCmd_Nav entry3_nav;
  MenuCmd_Skip2 entry3_upSkip;
  MenuCommand entry3_end;
  MenuCoords8 entry4_rc;
  MenuCmd_Nav entry4_nav;
  MenuCmd_Skip2 entry4_upSkip;
  MenuCommand entry4_end;
  MenuCoords8 entry5_rc;
  MenuCmd_Nav entry5_nav;
  MenuCmd_Skip2 entry5_upSkip;
  MenuCommand entry5_end;
  MenuCoords8 entry6_rc;
  MenuCmd_Nav entry6_nav;
  MenuCmd_Skip2 entry6_upSkip;
  MenuCommand entry6_end;
  MenuCoords8 entry7_rc;
  MenuCmd_Nav entry7_nav;
  MenuCmd_Skip2 entry7_upSkip;
  MenuCommand entry7_end;
  MenuCoords8 entry8_rc;
  MenuCmd_Nav entry8_nav;
  MenuCmd_Skip2 entry8_upSkip;
  MenuCommand entry8_end;
  MenuCoords8 entry9_rc;
  MenuCmd_Nav entry9_nav;
  MenuCmd_Skip2 entry9_upSkip;
  MenuCommand entry9_end;
  MenuCoords8 entry10_rc;
  MenuCmd_Nav entry10_nav;
  MenuCmd_Skip2 entry10_upSkip;
  MenuCommand entry10_end;
  MenuCoords8 entry11_rc;
  MenuCmd_Nav entry11_nav;
  MenuCmd_Skip2 entry11_upSkip;
  MenuCommand entry11_end;
  MenuCoords8 entry12_rc;
  MenuCmd_Nav entry12_nav;
  MenuCmd_Skip2 entry12_upSkip;
  MenuCommand entry12_end;
  MenuCoords8 entry13_rc;
  MenuCmd_Nav entry13_nav;
  MenuCmd_Skip2 entry13_upSkip;
  MenuCommand entry13_end;
  MenuCoords8 entry14_rc;
  MenuCmd_Nav entry14_nav;
  MenuCmd_Skip2 entry14_upSkip;
  MenuCommand entry14_end;
  MenuCoords8 entry15_rc;
  MenuCmd_Nav entry15_nav;
  MenuCmd_Skip2 entry15_upSkip;
  MenuCommand entry15_end;
  MenuEntry_Byte_NavOnly entries16_31[16];
  MenuCoords8 entry32_rc;
  MenuCmd_Nav entry32_nav;
  MenuCmd_Skip2 entry32_downSkip;
  MenuCommand entry32_end;
  MenuCoords8 entry33_rc;
  MenuCmd_Nav entry33_nav;
  MenuCmd_Skip2 entry33_downSkip;
  MenuCommand entry33_end;
  MenuCoords8 entry34_rc;
  MenuCmd_Nav entry34_nav;
  MenuCmd_Skip2 entry34_downSkip;
  MenuCommand entry34_end;
  MenuCoords8 entry35_rc;
  MenuCmd_Nav entry35_nav;
  MenuCmd_Skip2 entry35_downSkip;
  MenuCommand entry35_end;
  MenuCoords8 entry36_rc;
  MenuCmd_Nav entry36_nav;
  MenuCmd_Skip2 entry36_downSkip;
  MenuCommand entry36_end;
  MenuCoords8 entry37_rc;
  MenuCmd_Nav entry37_nav;
  MenuCmd_Skip2 entry37_downSkip;
  MenuCommand entry37_end;
  MenuCoords8 entry38_rc;
  MenuCmd_Nav entry38_nav;
  MenuCmd_Skip2 entry38_downSkip;
  MenuCommand entry38_end;
  MenuCoords8 entry39_rc;
  MenuCmd_Nav entry39_nav;
  MenuCmd_Skip2 entry39_downSkip;
  MenuCommand entry39_end;
  MenuCoords8 entry40_rc;
  MenuCmd_Nav entry40_nav;
  MenuCmd_Skip2 entry40_downSkip;
  MenuCommand entry40_end;
  MenuCoords8 entry41_rc;
  MenuCmd_Nav entry41_nav;
  MenuCmd_Skip2 entry41_downSkip;
  MenuCommand entry41_end;
  MenuCoords8 entry42_rc;
  MenuCmd_Nav entry42_nav;
  MenuCmd_Skip2 entry42_downSkip;
  MenuCommand entry42_end;
  MenuCoords8 entry43_rc;
  MenuCmd_Nav entry43_nav;
  MenuCmd_Skip2 entry43_downSkip;
  MenuCommand entry43_end;
  MenuCoords8 entry44_rc;
  MenuCmd_Nav entry44_nav;
  MenuCmd_Skip2 entry44_downSkip;
  MenuCommand entry44_end;
  MenuCoords8 entry45_rc;
  MenuCmd_Nav entry45_nav;
  MenuCmd_Skip2 entry45_downSkip;
  MenuCommand entry45_end;
  MenuMark markInitEntry;
  MenuMark_StartMenu menu1Start2;
  MenuCoords8 entry46_rc;
  MenuCmd_Invisible entry46_invisible;
  MenuCmd_Colors entry46_colors;
  MenuCmd_TextPtr entry46_text;
  MenuCommand entry46_end;
  MenuCoords8 entry47_rc;
  MenuCmd_Invisible entry47_invisible;
  MenuCmd_TextPtr entry47_text;
  MenuCommand entry47_end;
  MenuCoords8 entry48_rc;
  MenuCmd_Invisible entry48_invisible;
  MenuCmd_Nav entry48_nav;
  MenuCmd_Colors entry48_colors;
  MenuCmd_TextNoPtr entry48_text;
  MenuCmd_OnSelect entry48_onSelect;
  MenuCommand entry48_end;
  MenuCoords8 entry49_rc;
  MenuCmd_Invisible entry49_invisible;
  MenuCmd_Nav entry49_nav;
  MenuCmd_Colors entry49_colors;
  MenuCmd_TextPtr entry49_text;
  MenuCmd_OnSelect entry49_onSelect;
  MenuCommand entry49_end;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry50;
  MenuMark markEndMenu;
} Menu_SelectFiles;
SWOS_PACK_POP

/* 233 - Menu 35: selectNationalTeamMenu (0x2C8 = 712 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_SelectNationalTeam
{
  MenuHeader hdr;
  MenuCoords16 entry0_rc;
  MenuCmd_Colors entry0_colors;
  MenuCmd_TextNoPtr entry0_text;
  MenuCommand entry0_end;
  MenuMark markByteCoords;
  MenuCoords8 entry1_rc;
  MenuCmd_Colors entry1_colors;
  MenuCmd_TextNoPtr entry1_text;
  MenuCommand entry1_end;
  MenuEntry_Byte_TextPtrOnly entry2;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entries3_4[2];
  MenuCoords8 entry5_rc;
  MenuCmd_Nav entry5_nav;
  MenuCmd_Skip2 entry5_rightSkip;
  MenuCmd_Colors entry5_colors;
  MenuCmd_TextPtr entry5_text;
  MenuCmd_OnSelect entry5_onSelect;
  MenuCommand entry5_end;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry6;
  MenuMark_StartMenu menu1Start;
  MenuMark template7_markStart;
  MenuCoords8 template7_rc;
  MenuCmd_Colors template7_colors;
  MenuCmd_AfterDraw template7_afterDraw;
  MenuCmd_OnSelect template7_onSelect;
  MenuCommand template7_end;
  MenuMark markWideCoords;
  MenuEntry_Wide_CoordsOnly entry7;
  MenuEntry_Wide_NavOnly entries8_23[16];
  MenuCoords16 entry24_rc;
  MenuCmd_Nav entry24_nav;
  MenuCmd_Skip2 entry24_leftSkip;
  MenuCmd_Invisible entry24_invisible;
  MenuCommand entry24_end;
  MenuMark markInitEntry;
  MenuMark markByteCoords2;
  MenuCoords8 entry25_rc;
  MenuCmd_Invisible entry25_invisible;
  MenuCmd_Nav entry25_nav;
  MenuCmd_Colors entry25_colors;
  MenuCmd_OnSelect entry25_onSelect;
  MenuCmd_AfterDraw entry25_afterDraw;
  MenuCommand entry25_end;
  MenuEntry_Byte_Invisible_NavOnly entry26;
  MenuMark_StartMenu menu1Start2;
  MenuMark markWideCoords2;
  MenuEntry_Wide_ColorsOnly entry27;
  MenuMark markByteCoords3;
  MenuEntry_Byte_NumberOnly entry28;
  MenuEntry_Byte_SpriteFgOnly entry29;
  MenuEntry_Byte_TextPtrOnly entry30;
  MenuEntry_Byte_TextNoPtrOnly entry31;
  MenuEntry_Byte_TextPtrOnly entry32;
  MenuEntry_Byte_TextNoPtrOnly entry33;
  MenuEntry_Byte_SpriteFgOnly entries34_36[3];
  MenuEntry_Byte_TextNoPtrOnly entry37;
  MenuMark markWideCoords3;
  MenuEntry_Wide_TextPtrOnly entry38;
  MenuMark markEndMenu;
} Menu_SelectNationalTeam;
SWOS_PACK_POP

/* 234 - Menu 37: playMatchMenu (0x4BC = 1212 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_PlayMatch
{
  MenuHeader hdr;
  MenuMark_StartMenu menu1Start;
  MenuMark markByteCoords;
  MenuCoords8 entry0_rc;
  MenuCmd_Colors entry0_colors;
  MenuCmd_SpriteFg entry0_sprite;
  MenuCommand entry0_end;
  MenuEntry_Byte_Colors_TextPtr entry1;
  MenuEntry_Byte_TextNoPtrOnly entry2;
  MenuEntry_Byte_TextPtrOnly entries3_4[2];
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry5;
  MenuCoords8 entry6_rc;
  MenuCmd_Nav entry6_nav;
  MenuCmd_Skip2 entry6_upSkip;
  MenuCmd_Skip2 entry6_rightSkip;
  MenuCmd_Colors entry6_colors;
  MenuCmd_TextPtr entry6_multiline;
  MenuCmd_OnSelect entry6_onSelect;
  MenuCommand entry6_end;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry7;
  MenuCoords8 entry8_rc;
  MenuCmd_Nav entry8_nav;
  MenuCmd_Skip2 entry8_rightSkip;
  MenuCmd_Colors entry8_colors;
  MenuCmd_TextPtr entry8_text;
  MenuCmd_OnSelect entry8_onSelect;
  MenuCommand entry8_end;
  MenuCoords8 entry9_rc;
  MenuCmd_Nav entry9_nav;
  MenuCmd_Skip2 entry9_upSkip;
  MenuCmd_Skip2 entry9_rightSkip;
  MenuCmd_Colors entry9_colors;
  MenuCmd_TextPtr entry9_text;
  MenuCmd_OnSelect entry9_onSelect;
  MenuCommand entry9_end;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entry10;
  MenuCoords8 entry11_rc;
  MenuCmd_OnSelect entry11_onSelect;
  MenuCommand entry11_end;
  MenuEntry_Byte_ColorsOnly entry12;
  MenuMark template13_markStart;
  MenuCoords8 template13_rc;
  MenuCmd_Colors template13_colors;
  MenuCmd_AfterDraw template13_afterDraw;
  MenuCmd_OnSelect template13_onSelect;
  MenuCommand template13_end;
  MenuEntry_Byte_NavOnly entries13_23[11];
  MenuCoords8 entry24_rc;
  MenuCmd_Nav entry24_nav;
  MenuCmd_Skip2 entry24_upSkip;
  MenuCommand entry24_end;
  MenuCoords8 entry25_rc;
  MenuCmd_Nav entry25_nav;
  MenuCmd_Skip2 entry25_upSkip;
  MenuCommand entry25_end;
  MenuCoords8 entry26_rc;
  MenuCmd_Nav entry26_nav;
  MenuCmd_Skip2 entry26_upSkip;
  MenuCommand entry26_end;
  MenuCoords8 entry27_rc;
  MenuCmd_Nav entry27_nav;
  MenuCmd_Skip2 entry27_upSkip;
  MenuCommand entry27_end;
  MenuCoords8 entry28_rc;
  MenuCmd_Nav entry28_nav;
  MenuCmd_Skip2 entry28_upSkip;
  MenuCommand entry28_end;
  MenuCoords8 entry29_rc;
  MenuCmd_Nav entry29_nav;
  MenuCmd_Skip2 entry29_upSkip;
  MenuCmd_Invisible entry29_invisible;
  MenuCommand entry29_end;
  MenuMark markInitEntry;
  MenuCoords8 entry30_rc;
  MenuCmd_Invisible entry30_invisible;
  MenuCmd_Nav entry30_nav;
  MenuCmd_Colors entry30_colors;
  MenuCmd_OnSelect entry30_onSelect;
  MenuCmd_AfterDraw entry30_afterDraw;
  MenuCommand entry30_end;
  MenuCoords8 entry31_rc;
  MenuCmd_Colors entry31_colors;
  MenuCmd_SpriteFg entry31_sprite;
  MenuCommand entry31_end;
  MenuEntry_Byte_NumberOnly entry32;
  MenuEntry_Byte_Colors_TextPtr entry33;
  MenuEntry_Byte_TextPtrOnly entry34;
  MenuEntry_Byte_TextNoPtrOnly entry35;
  MenuEntry_Byte_TextPtrOnly entry36;
  MenuCoords8 entry37_rc;
  MenuCmd_Colors entry37_colors;
  MenuCmd_SpriteFg entry37_sprite;
  MenuCommand entry37_end;
  MenuMark_StartMenu menu1Start2;
  MenuMark template38_markStart;
  MenuCoords8 template38_rc;
  MenuCmd_Colors template38_colors;
  MenuCmd_OnSelect template38_onSelect;
  MenuCommand template38_end;
  MenuCoords8 entry38_rc;
  MenuCmd_Nav entry38_nav;
  MenuCmd_TextPtr entry38_text;
  MenuCommand entry38_end;
  MenuCoords8 entry39_rc;
  MenuCmd_Nav entry39_nav;
  MenuCmd_TextPtr entry39_text;
  MenuCommand entry39_end;
  MenuCoords8 entry40_rc;
  MenuCmd_Nav entry40_nav;
  MenuCmd_TextPtr entry40_text;
  MenuCommand entry40_end;
  MenuCoords8 entry41_rc;
  MenuCmd_Nav entry41_nav;
  MenuCmd_TextPtr entry41_text;
  MenuCommand entry41_end;
  MenuCoords8 entry42_rc;
  MenuCmd_Nav entry42_nav;
  MenuCmd_TextPtr entry42_text;
  MenuCommand entry42_end;
  MenuCoords8 entry43_rc;
  MenuCmd_Nav entry43_nav;
  MenuCmd_TextPtr entry43_text;
  MenuCommand entry43_end;
  MenuCoords8 entry44_rc;
  MenuCmd_Nav entry44_nav;
  MenuCmd_TextPtr entry44_text;
  MenuCommand entry44_end;
  MenuCoords8 entry45_rc;
  MenuCmd_Nav entry45_nav;
  MenuCmd_TextPtr entry45_text;
  MenuCommand entry45_end;
  MenuCoords8 entry46_rc;
  MenuCmd_Nav entry46_nav;
  MenuCmd_TextPtr entry46_text;
  MenuCommand entry46_end;
  MenuCoords8 entry47_rc;
  MenuCmd_Nav entry47_nav;
  MenuCmd_TextPtr entry47_text;
  MenuCommand entry47_end;
  MenuCoords8 entry48_rc;
  MenuCmd_Nav entry48_nav;
  MenuCmd_TextPtr entry48_text;
  MenuCommand entry48_end;
  MenuCoords8 entry49_rc;
  MenuCmd_Nav entry49_nav;
  MenuCmd_TextPtr entry49_text;
  MenuCommand entry49_end;
  MenuCoords8 entry50_rc;
  MenuCmd_Nav entry50_nav;
  MenuCmd_TextPtr entry50_text;
  MenuCommand entry50_end;
  MenuCoords8 entry51_rc;
  MenuCmd_Nav entry51_nav;
  MenuCmd_TextPtr entry51_text;
  MenuCommand entry51_end;
  MenuCoords8 entry52_rc;
  MenuCmd_Nav entry52_nav;
  MenuCmd_TextPtr entry52_text;
  MenuCommand entry52_end;
  MenuCoords8 entry53_rc;
  MenuCmd_Nav entry53_nav;
  MenuCmd_TextPtr entry53_text;
  MenuCommand entry53_end;
  MenuCoords8 entry54_rc;
  MenuCmd_Nav entry54_nav;
  MenuCmd_TextPtr entry54_text;
  MenuCommand entry54_end;
  MenuCoords8 entry55_rc;
  MenuCmd_Nav entry55_nav;
  MenuCmd_TextPtr entry55_text;
  MenuCommand entry55_end;
  MenuMark markInitEntry2;
  MenuMark_StartMenu menu1Start3;
  MenuEntry_Byte_SpriteFgOnly entry56;
  MenuEntry_Byte_TextNoPtrOnly entries57_67[11];
  MenuMark markEndMenu;
} Menu_PlayMatch;
SWOS_PACK_POP

/* 235 - Menu 43: editTacticsMenu (0x31C = 796 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_EditTactics
{
  MenuHeader hdr;
  MenuMark markByteCoords;
  MenuCoords8 entry0_rc;
  MenuCmd_Colors entry0_colors;
  MenuCmd_SpriteFg entry0_sprite;
  MenuCommand entry0_end;
  MenuCoords8 entry1_rc;
  MenuCmd_Colors entry1_colors;
  MenuCmd_TextNoPtr entry1_text;
  MenuCommand entry1_end;
  MenuCoords8 entry2_rc;
  MenuCmd_InputFunc entry2_inputFunc;
  MenuCmd_AfterDraw entry2_afterDraw;
  MenuCommand entry2_end;
  MenuCoords8 entry3_rc;
  MenuCmd_InputFunc entry3_inputFunc;
  MenuCmd_AfterDraw entry3_afterDraw;
  MenuCommand entry3_end;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entries4_5[2];
  MenuCoords8 entry6_rc;
  MenuCmd_Nav entry6_nav;
  MenuCmd_Colors entry6_colors;
  MenuCmd_StringTable entry6_strTab;
  MenuCmd_OnSelect entry6_onSelect;
  MenuCommand entry6_end;
  MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect entries7_9[3];
  MenuMark_StartMenu menu1Start;
  MenuEntry_Byte_ColorsOnly entry10;
  MenuMark template11_markStart;
  MenuCoords8 template11_rc;
  MenuCmd_Colors template11_colors;
  MenuCmd_AfterDraw template11_afterDraw;
  MenuCmd_OnSelect template11_onSelect;
  MenuCommand template11_end;
  MenuEntry_Byte_NavOnly entries11_25[15];
  MenuCoords8 entry26_rc;
  MenuCmd_Nav entry26_nav;
  MenuCmd_Skip2 entry26_upSkip;
  MenuCommand entry26_end;
  MenuCoords8 entry27_rc;
  MenuCmd_Nav entry27_nav;
  MenuCmd_Invisible entry27_invisible;
  MenuCommand entry27_end;
  MenuCoords8 entry28_rc;
  MenuCmd_Invisible entry28_invisible;
  MenuCmd_Nav entry28_nav;
  MenuCmd_Colors entry28_colors;
  MenuCmd_OnSelect entry28_onSelect;
  MenuCmd_AfterDraw entry28_afterDraw;
  MenuCommand entry28_end;
  MenuMark markInitEntry;
  MenuMark template29_markStart;
  MenuCoords8 template29_rc;
  MenuCmd_Colors template29_colors;
  MenuCmd_AfterDraw template29_afterDraw;
  MenuCmd_OnSelect template29_onSelect;
  MenuCommand template29_end;
  MenuEntry_Byte_NavOnly entries29_38[10];
  MenuMark markInitEntry2;
  MenuMark_StartMenu menu1Start2;
  MenuMark_StartMenu menu1Start3;
  MenuCoords8 entry39_rc;
  MenuCmd_Colors entry39_colors;
  MenuCmd_SpriteFg entry39_sprite;
  MenuCommand entry39_end;
  MenuEntry_Byte_NumberOnly entry40;
  MenuEntry_Byte_Colors_TextPtr entry41;
  MenuEntry_Byte_TextPtrOnly entry42;
  MenuEntry_Byte_TextNoPtrOnly entry43;
  MenuEntry_Byte_TextPtrOnly entry44;
  MenuMark_StartMenu menu1Start4;
  MenuMark markEndMenu;
} Menu_EditTactics;
SWOS_PACK_POP

/* 236 - Menu 45: stadiumMenu (0x366 = 870 bytes) */
SWOS_PACK_PUSH1
typedef struct Menu_Stadium
{
  MenuHeader hdr;
  MenuMark_StartMenu menu1Start;
  MenuMark markByteCoords;
  MenuEntry_Byte_TextPtrOnly entries0_1[2];
  MenuCoords8 entry2_rc;
  MenuCmd_Colors entry2_colors;
  MenuCmd_TextNoPtr entry2_text;
  MenuCommand entry2_end;
  MenuEntry_Byte_TextPtrOnly entry3;
  MenuCoords8 entry4_rc;
  MenuCmd_Colors entry4_colors;
  MenuCmd_TextNoPtr entry4_text;
  MenuCommand entry4_end;
  MenuMark_StartMenu menu1Start2;
  MenuMark template5_markStart;
  MenuCoords8 template5_rc;
  MenuCmd_BeforeDraw template5_beforeDraw;
  MenuCommand template5_end;
  MenuCoords8 entry5_rc;
  MenuCmd_SpriteCopy entry5_spriteCopy;
  MenuCommand entry5_end;
  MenuCoords8 entry6_rc;
  MenuCmd_SpriteCopy entry6_spriteCopy;
  MenuCommand entry6_end;
  MenuCoords8 entry7_rc;
  MenuCmd_SpriteCopy entry7_spriteCopy;
  MenuCommand entry7_end;
  MenuCoords8 entry8_rc;
  MenuCmd_SpriteCopy entry8_spriteCopy;
  MenuCommand entry8_end;
  MenuCoords8 entry9_rc;
  MenuCmd_SpriteCopy entry9_spriteCopy;
  MenuCommand entry9_end;
  MenuCoords8 entry10_rc;
  MenuCmd_SpriteCopy entry10_spriteCopy;
  MenuCommand entry10_end;
  MenuCoords8 entry11_rc;
  MenuCmd_SpriteCopy entry11_spriteCopy;
  MenuCommand entry11_end;
  MenuCoords8 entry12_rc;
  MenuCmd_SpriteCopy entry12_spriteCopy;
  MenuCommand entry12_end;
  MenuCoords8 entry13_rc;
  MenuCmd_SpriteCopy entry13_spriteCopy;
  MenuCommand entry13_end;
  MenuCoords8 entry14_rc;
  MenuCmd_SpriteCopy entry14_spriteCopy;
  MenuCommand entry14_end;
  MenuCoords8 entry15_rc;
  MenuCmd_SpriteCopy entry15_spriteCopy;
  MenuCommand entry15_end;
  MenuMark markInitEntry;
  MenuMark_StartMenu menu1Start3;
  MenuEntry_Byte_TextPtrOnly entries16_31[16];
  MenuMark_StartMenu menu1Start4;
  MenuMark template32_markStart;
  MenuCoords8 template32_rc;
  MenuCmd_BeforeDraw template32_beforeDraw;
  MenuCommand template32_end;
  MenuCoords8 entry32_rc;
  MenuCmd_SpriteCopy entry32_spriteCopy;
  MenuCommand entry32_end;
  MenuCoords8 entry33_rc;
  MenuCmd_SpriteCopy entry33_spriteCopy;
  MenuCommand entry33_end;
  MenuCoords8 entry34_rc;
  MenuCmd_SpriteCopy entry34_spriteCopy;
  MenuCommand entry34_end;
  MenuCoords8 entry35_rc;
  MenuCmd_SpriteCopy entry35_spriteCopy;
  MenuCommand entry35_end;
  MenuCoords8 entry36_rc;
  MenuCmd_SpriteCopy entry36_spriteCopy;
  MenuCommand entry36_end;
  MenuCoords8 entry37_rc;
  MenuCmd_SpriteCopy entry37_spriteCopy;
  MenuCommand entry37_end;
  MenuCoords8 entry38_rc;
  MenuCmd_SpriteCopy entry38_spriteCopy;
  MenuCommand entry38_end;
  MenuCoords8 entry39_rc;
  MenuCmd_SpriteCopy entry39_spriteCopy;
  MenuCommand entry39_end;
  MenuCoords8 entry40_rc;
  MenuCmd_SpriteCopy entry40_spriteCopy;
  MenuCommand entry40_end;
  MenuCoords8 entry41_rc;
  MenuCmd_SpriteCopy entry41_spriteCopy;
  MenuCommand entry41_end;
  MenuCoords8 entry42_rc;
  MenuCmd_SpriteCopy entry42_spriteCopy;
  MenuCommand entry42_end;
  MenuMark markInitEntry2;
  MenuMark_StartMenu menu1Start5;
  MenuEntry_Byte_TextPtrOnly entries43_58[16];
  MenuEntry_Byte_CoordsOnly entry59;
  MenuMark markEndMenu;
} Menu_Stadium;
SWOS_PACK_POP

extern Menu_ChooseTeamsFriendly chooseTeamsFriendlyMenu;
extern Menu_ChooseTeamsFriendly2 chooseTeamsFriendlyMenu2;
extern Menu_DesignDIYLeague designDiyLeagueMenu;
extern Menu_DIYLeagueTournament diyLeagueTournamentMenu;
extern Menu_DesignDIYCup designDiyCupMenu;
extern Menu_DIYCup diyCupMenu;
extern Menu_Options optionsMenu;
extern Menu_DesignDiyTournament designDiyTournamentMenu;
extern Menu_FriendlyGame friendlyGameMenu;
extern Menu_EditTeams editTeamsMenu;
extern Menu_CompetitionTopGoalScorers competitionTopGoalScorersMenu;
extern Menu_TopGoalScorers topGoalScorersMenu;
extern Menu_Squad squadMenu;
extern Menu_CompetitionStats competitionStatsMenu;
extern Menu_CareerOver careerOverMenu;
extern Menu_Career careerMenu;
extern Menu_ViewCompetitions viewCompetitionsMenu;
extern Menu_Contest contestMenu;
extern Menu_ManagementRecord managementRecordMenu;
extern Menu_SelectTeamsForTactics selectTeamsForTacticsMenu;
extern Menu_SelectTeams selectTeamsMenu;
extern Menu_ChooseTeams chooseTeamsMenu;
extern Menu_SelectFiles selectFilesMenu;
extern Menu_Continue continueMenu;
extern Menu_ContinueAbort continueAbortMenu;
extern Menu_SWOSMainMenu swosMainMenu;
extern Menu_Offers offersMenu;
extern Menu_PlayerTransfer playerTransferMenu;
extern Menu_ThirdPartyOffer thirdPartyOfferMenu;
extern Menu_PlayerOffers playerOffersMenu;
extern Menu_ViewSquad viewSquadMenu;
extern Menu_BuyOtherPlayers buyOtherPlayersMenu;
extern Menu_ShowPlayerMarket showPlayerMarketMenu;
extern Menu_BuyPlayer buyPlayerMenu;
extern Menu_TransferMarket transferMarketMenu;
extern Menu_SelectNationalTeam selectNationalTeamMenu;
extern Menu_NationalTeamAddPlayer nationalTeamAddPlayerMenu;
extern Menu_PlayMatch playMatchMenu;
extern Menu_JobOffers jobOffersMenu;
extern Menu_GotJobOffer gotJobOfferMenu;
extern Menu_ChairmanMemos chairmanMemosMenu;
extern Menu_ClubBusiness clubBusinessMenu;
extern Menu_ChooseTactics chooseTacticsMenu;
extern Menu_EditTactics editTacticsMenu;
extern Menu_ImportTactics importTacticsMenu;
extern Menu_Stadium stadiumMenu;
extern Menu_Highlights highlightsMenu;
