#ifndef MENUS_H
#define MENUS_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef short int16_t;

void _PrepareMenuBackground(void);
void _InitMainMenuStuff(void);
void _ZeroOutStars(void);
void _SetDefaultOptions(void);
void _SetDefaultNameAndSurname(void);
void _InitCareerVariables(uint16_t gameTypeParam);
void _InitUserTactics(void);
void _InitializeTacticsPositions(void);
void _SaveOptions(void);
void _RestoreOptions(void);
void _ShowErrorMenu(uint8_t *errorTextStr);
int16_t _DoContinueAbortMenu(uint8_t *header, uint8_t *changing, uint8_t *contText, uint8_t *abortTextStr, uint16_t defaultEntry);
void _ContinueAbortInit(void);
void _ContinueSelected(void);
void _AbortSelected(void);
void _ContinueInit(void);
void _ContinueMenuContinueSelected(void);

/* --- Stadium menu (Chunk 89) --- */
struct TeamGame;
void _ShowStadiumInit(void);
void _StadiumPlayerSpritesBeforeDraw(struct TeamGame *team, uint16_t playerIndex);
void _StadiumBeforeDrawLeftTeam(void *entry);
void _StadiumBeforeDrawRightTeam(void *entry);

#endif
