/*
 * online_menus.c -- SWOS 2020's online-play menus, running in OpenSWOS.
 *
 * The menus are imported data: scripts/import_swos2020_menus.py turns the .mnu
 * files in swos-2020/src/menus into JSON under bin/data_json/menus/swos2020 and
 * into include/swos2020_menus.h (docs/MENUS.md section 10). This file supplies their
 * handlers to the JSON loader, under SWOS 2020's own names, so a handler ported
 * from swos-2020 (game.cpp, mpgame.cpp) keeps its name and its entry ordinals
 * (LogInMenu::header -> LogInMenu_header).
 *
 * Handlers get ported as the online features they drive arrive: the login service,
 * the matchmaking server, rollback netplay (src/netplay.c). A handler that is not
 * ported yet is bound to a no-op by the loader and logged once; --menu-selftest
 * reports the count.
 *
 * With no server, what works now: every imported menu opens, lays out and
 * navigates, and every exit button leaves its menu; the login form edits a username
 * and password and toggles its options; LOGIN goes to matchmaking in SWOS 2020's
 * own "server not connected" state, from where SETTINGS and RANKINGS open.
 */
#include "online_menus.h"
#include "p2p_menu.h"          /* OpenSWOS's own JSON-only menu */
#include "swos2020_menus.h"
#include "menu_load_json.h"
#include "menu_symbols.h"
#include "menu_system.h"
#include "input_text.h"

#include <stdio.h>
#include <string.h>

#define SWOS2020_MENU(name) "swos2020/" #name

/* SWOS 2020 mnu2h colour constants used below (mnu2h/Constants.py). */
enum { kGray = 7, kLightBrownWithRedFrame = 12 };

/* ---- entry access, the spelling SWOS 2020's handlers use (swos.h MenuEntry) ---- */
static MenuEntry *entry_at(int32_t ordinal)
{
    return CalcMenuEntryAddress((uint16_t)ordinal);
}

static void set_text(int32_t ordinal, const char *text)
{
    entry_at(ordinal)->fg = (uintptr_t)text;
}

static void set_visible(int32_t ordinal, int32_t visible)
{
    entry_at(ordinal)->isInvisible = visible ? 0 : 1;
}

/* SWOS 2020 setEntry(): text, selectable or not, background colour. */
static void set_entry(int32_t ordinal, const char *text, int32_t enabled, uint16_t color)
{
    MenuEntry *e = entry_at(ordinal);
    e->fg = (uintptr_t)text;
    e->isDisabled = enabled ? 0 : 1;
    e->bgColor = color;
}

void OnlineMenus_Show(const char *name)
{
    uint8_t *menu = MenuJson_Load(name);
    if (!menu) {
        fprintf(stderr, "[online-menus] cannot show %s\n", name);
        return;
    }
    ShowMenu(menu);
}

/* ======================================================================
 * logInMenu (swos-2020/src/menus/logIn.mnu; handlers in game.cpp)
 * ====================================================================== */
static char s_username[33];
static char s_password[33];
static char s_passwordMask[33];
static int32_t s_rememberLogin = 1;   /* the .mnu's initial ON/OFF texts */
static int32_t s_showPassword = 0;
static int32_t s_liveCast = 1;

static const char *password_text(void)
{
    if (s_showPassword)
        return s_password;
    size_t len = strlen(s_password);
    memset(s_passwordMask, '*', len);
    s_passwordMask[len] = '\0';
    return s_passwordMask;
}

static void logInMenu_OnInit(void)
{
    set_text(LogInMenu_header, "SERVER LOGIN");
    set_text(LogInMenu_subTitle, "LOGIN TO YOUR ONLINE ACCOUNT");
    set_text(LogInMenu_usernameInput, s_username);
    set_text(LogInMenu_passwordInput, password_text());
    set_text(LogInMenu_logInContinue, "LOGIN");
    set_text(LogInMenu_OfflinelogOut, "PLAY OFFLINE");
    set_text(LogInMenu_news, "NEWS");
}

/* Runs every frame (the menu is flagged dos_redraw), so the toggles just follow state. */
static void logInMenu_OnDraw(void)
{
    set_text(LogInMenu_rememberLogInInfo, s_rememberLogin ? "ON" : "OFF");
    set_text(LogInMenu_showPassword, s_showPassword ? "ON" : "OFF");
    set_text(LogInMenu_liveCast, s_liveCast ? "ON" : "OFF");
    set_text(LogInMenu_passwordInput, password_text());
}

static void usernameInput_OnSelect(void)
{
    InputText(sizeof s_username, (uint8_t *)entry_at(LogInMenu_usernameInput));
}

static void passwordInput_OnSelect(void)
{
    MenuEntry *e = entry_at(LogInMenu_passwordInput);
    e->fg = (uintptr_t)s_password;        /* edit the text itself, not the mask */
    InputText(sizeof s_password, (uint8_t *)e);
    e->fg = (uintptr_t)password_text();
}

static void rememberLogInInfo_OnSelect(void) { s_rememberLogin = !s_rememberLogin; }
static void showPassword_OnSelect(void)      { s_showPassword = !s_showPassword; }
static void liveCast_OnSelect(void)          { s_liveCast = !s_liveCast; }

/* There is no login service yet: go straight to matchmaking, which shows SWOS
 * 2020's own "server not connected" state. */
static void logInMenuLogInContinue_OnSelect(void)
{
    OnlineMenus_Show(SWOS2020_MENU(matchMakingMenu));
}

static void logInMenuNews_OnSelect(void)
{
    OnlineMenus_Show(SWOS2020_MENU(newsMenu));
}

/* ======================================================================
 * matchMakingMenu (swos-2020/src/menus/matchMaking.mnu; handlers in mpgame.cpp)
 * ====================================================================== */
static void matchMakingMenu_OnInit(void)
{
    /* No lobby list without a server: hide the host/mode/action rows and the user list. */
    for (int32_t row = 0; row < MatchMakingMenu_kGS_MaxRows; row++) {
        set_visible(MatchMakingMenu_host_00 + row, 0);
        set_visible(MatchMakingMenu_mode_00 + row, 0);
        set_visible(MatchMakingMenu_action_00 + row, 0);
    }
    for (int32_t row = 0; row < MatchMakingMenu_kGS_MaxRows2; row++)
        set_visible(MatchMakingMenu_users_00 + row, 0);

    set_text(MatchMakingMenu_connectionStatus, "OFFLINE");
    set_text(MatchMakingMenu_numOfUsers, "");
    set_text(MatchMakingMenu_userName, s_username);

    /* setMatchMakingMenu_Standard(), menuMode MMM_X_SERVER_NOT_CONNECTED. */
    set_entry(MatchMakingMenu_desc, "SERVER NOT CONNECTED", 0, 0);
    set_entry(MatchMakingMenu_openGame, "OPEN", 0, kGray);
    set_entry(MatchMakingMenu_cancelGame, "CANCEL", 0, kGray);
    set_entry(MatchMakingMenu_menuExit, "EXIT", 1, kLightBrownWithRedFrame);
}

static void matchMaking_menuSettings_onSelect(void)
{
    OnlineMenus_Show(SWOS2020_MENU(mmSettingsMenu));
}

static void matchMaking_rankings_onSelect(void)
{
    OnlineMenus_Show(SWOS2020_MENU(rankingsMenu));
}

/* ======================================================================
 * Handler table. Unlisted names are bound to a no-op by the loader.
 * ====================================================================== */
#define HANDLER(fn)            { #fn, (void *)fn, SYM_FUNC }
#define HANDLER_AS(name, fn)   { name, (void *)fn, SYM_FUNC }

static const MenuSymbol k_handlers[] = {
    /* ~SetExitMenuFlag in the .mnu files is the SWOS routine of the same name. */
    HANDLER(SetExitMenuFlag),

    HANDLER(logInMenu_OnInit),
    HANDLER(logInMenu_OnDraw),
    HANDLER(usernameInput_OnSelect),
    HANDLER(passwordInput_OnSelect),
    HANDLER(rememberLogInInfo_OnSelect),
    HANDLER(showPassword_OnSelect),
    HANDLER(liveCast_OnSelect),
    HANDLER(logInMenuLogInContinue_OnSelect),
    HANDLER(logInMenuNews_OnSelect),

    HANDLER(matchMakingMenu_OnInit),
    HANDLER(matchMaking_menuSettings_onSelect),
    HANDLER(matchMaking_rankings_onSelect),

    /* Exit and cancel buttons. Offline there is nothing to cancel or leave on a
     * server, so all of them just close their menu. */
    HANDLER_AS("cupMatchView_exit_onSelect",         SetExitMenuFlag),
    HANDLER_AS("exitNewsMenu_OnSelect",              SetExitMenuFlag),
    HANDLER_AS("exitRankingMenu_OnSelect",           SetExitMenuFlag),
    HANDLER_AS("hostGameMessageMenu_exit_OnSelect",  SetExitMenuFlag),
    HANDLER_AS("leagueInfo1_exit_onSelect",          SetExitMenuFlag),
    HANDLER_AS("leagueInfo2_exit_onSelect",          SetExitMenuFlag),
    HANDLER_AS("leagueInfoMain_exit_onSelect",       SetExitMenuFlag),
    HANDLER_AS("leagueInfoView_exit_onSelect",       SetExitMenuFlag),
    HANDLER_AS("logInMenuCancel_OnSelect",           SetExitMenuFlag),
    HANDLER_AS("logInMenuOfflinelogOut_OnSelect",    SetExitMenuFlag),
    HANDLER_AS("mLogReaderMenu_exit_onSelect",       SetExitMenuFlag),
    HANDLER_AS("mLogStatsMenu_exit_onSelect",        SetExitMenuFlag),
    HANDLER_AS("mLogTickerMenu_exit_onSelect",       SetExitMenuFlag),
    HANDLER_AS("matchMaking_exit_onSelect",          SetExitMenuFlag),
    HANDLER_AS("matchView_exit_onSelect",            SetExitMenuFlag),
    HANDLER_AS("messageMenu_no_OnSelect",            SetExitMenuFlag),
    HANDLER_AS("messageMenu_yes_OnSelect",           SetExitMenuFlag),
    HANDLER_AS("mpWaiting_menuExit_OnSelect",        SetExitMenuFlag),
    HANDLER_AS("myFriendlyStatsMenu_exit_OnSelect",  SetExitMenuFlag),
    HANDLER_AS("myRankedStatsMenu_exit_OnSelect",    SetExitMenuFlag),
    HANDLER_AS("selectExitOpenLobby",                SetExitMenuFlag),
    HANDLER_AS("selectMPSettingExit",                SetExitMenuFlag),
};

#undef HANDLER
#undef HANDLER_AS

static const void *find_handler(const char *name)
{
    for (size_t i = 0; i < sizeof k_handlers / sizeof k_handlers[0]; i++)
        if (strcmp(k_handlers[i].name, name) == 0)
            return k_handlers[i].ptr;
    return NULL;
}

void OnlineMenus_Init(void)
{
    static int32_t registered;
    if (registered)
        return;
    registered = 1;
    MenuJson_RegisterSymbols(k_handlers, (int)(sizeof k_handlers / sizeof k_handlers[0]));
}

/* ======================================================================
 * --menu=<name>
 * ====================================================================== */
static char s_startMenu[64];

void OnlineMenus_SetStartMenu(const char *name)
{
    snprintf(s_startMenu, sizeof s_startMenu, "%s", name);
}

void OnlineMenus_ShowStartMenu(void)
{
    if (!s_startMenu[0])
        return;
    char name[sizeof s_startMenu];
    memcpy(name, s_startMenu, sizeof name);
    s_startMenu[0] = '\0';      /* once: the front end can be re-entered after a match */
    OnlineMenus_Init();
    P2PMenu_Init();             /* --menu=p2p/p2pMenu wants its handlers too */
    OnlineMenus_Show(name);
}

/* ======================================================================
 * --menu-selftest
 * ====================================================================== */
int32_t OnlineMenus_SelfTest(void)
{
    OnlineMenus_Init();
    printf("[online-menus] self-test: the JSON-only menus\n");

    int32_t menus = 0, entries = 0, failed = 0;
#define VERIFY_MENU(menu, key, prefix, count)                                        \
    do {                                                                             \
        int bad = MenuJson_VerifyDynamic(key, count);                                \
        menus++;                                                                     \
        entries += count;                                                            \
        if (bad) {                                                                   \
            failed++;                                                                \
            printf("  FAIL %-26s %s\n", #menu, bad < 0 ? "did not load" : "mismatch (see log)"); \
        }                                                                            \
    } while (0);
    SWOS2020_MENUS(VERIFY_MENU)
#undef VERIFY_MENU

    /* OpenSWOS's own, checked the same way (src/p2p_menu.c). */
    P2PMenu_Init();
    {
        int bad = MenuJson_VerifyDynamic(P2P_MENU_NAME, P2P_MENU_ENTRIES);
        menus++;
        entries += P2P_MENU_ENTRIES;
        if (bad) {
            failed++;
            printf("  FAIL %-26s %s\n", "p2pMenu", bad < 0 ? "did not load" : "mismatch (see log)");
        }
    }

#define NAME_STRING(fn) #fn,
    static const char *const handlers[] = { SWOS2020_MENU_CALLBACKS(NAME_STRING) };
#undef NAME_STRING
    int32_t numHandlers = (int32_t)(sizeof handlers / sizeof handlers[0]), ported = 0;
    for (int32_t i = 0; i < numHandlers; i++)
        if (find_handler(handlers[i]) || menu_resolve(handlers[i]))
            ported++;

    printf("  %d menus, %d entries unpacked and checked against their JSON: %d failed\n",
           menus, entries, failed);
    printf("  handlers: %d of %d ported, the rest run as no-ops\n", ported, numHandlers);
    printf("[online-menus] self-test %s\n", failed ? "FAILED" : "passed");
    fflush(stdout);
    return failed != 0;
}
