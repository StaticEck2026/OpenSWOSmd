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
#include "mm_client.h"         /* the matchmaking server (docs/NETPLAY.md section 7) */
#include "p2p_menu.h"          /* OpenSWOS's own JSON-only menu */
#include "swos2020_menus.h"
#include "menu_load_json.h"
#include "menu_symbols.h"
#include "menu_system.h"
#include "input_text.h"
#include "p2p_link.h"         /* the peer connection the handoff makes */
#include "netplay_lobby.h"    /* where the two players set the match up */
#include "netplay_match.h"    /* the live GGPO session */
#include "game_types.h"       /* TeamFileEx, for the match the handoff starts */
#include "platform_sdl.h"     /* Platform_SetFrameHook: the link outlives a modal menu */
#include "teams.h"            /* LoadTeamFile, for the lobby's test knobs */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SWOS2020_MENU(name) "swos2020/" #name

/* SWOS 2020 mnu2h colour constants used below (mnu2h/Constants.py). */
enum {
    kGray = 7, kLightBrownWithYellowFrame = 6, kRed = 10, kPurple = 11,
    kLightBrownWithRedFrame = 12, kLightBlue = 13, kGreen = 14,
};

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

/* Which server to talk to. There is no account service -- the matchmaker has no
 * notion of a password, it takes a name and hands out introductions -- so LOGIN
 * connects with the username typed above and the password is not sent anywhere.
 *
 * Default is a matchmaker on this machine (scripts/run_matchmaker.sh), which is
 * what the whole local setup exists for. SWOS_MM_HOST / SWOS_MM_PORT point it
 * somewhere else without a rebuild. */
static const char *mm_host(void) { return MmClient_DefaultHost(); }
static uint16_t    mm_port(void) { return MmClient_DefaultPort(); }

/* LOGIN: start connecting, then show matchmaking. The connection is not waited
 * for here -- matchMakingMenu's OnDraw polls it and shows CONNECTING / ONLINE /
 * the failure, so a server that is not there leaves a usable menu saying so
 * rather than a frozen one. */
static void logInMenuLogInContinue_OnSelect(void)
{
    if (!s_username[0])
        snprintf(s_username, sizeof s_username, "PLAYER");
    MmClient_Connect(mm_host(), mm_port(), s_username, NULL);
    OnlineMenus_Show(SWOS2020_MENU(matchMakingMenu));
}

static void logInMenuNews_OnSelect(void)
{
    OnlineMenus_Show(SWOS2020_MENU(newsMenu));
}

/* ======================================================================
 * matchMakingMenu (swos-2020/src/menus/matchMaking.mnu; handlers in mpgame.cpp)
 * ====================================================================== */
/* The lobby is redrawn from MmClient's state every frame, so these buffers hold
 * the text the menu entries point at. A MenuEntry stores a POINTER, so the
 * storage has to outlive the call that sets it -- a local would leave the menu
 * drawing freed stack. */
static char s_hostRow[MatchMakingMenu_kGS_MaxRows][MM_NAME_MAX + 8];
static char s_modeRow[MatchMakingMenu_kGS_MaxRows][16];
static char s_userRow[MatchMakingMenu_kGS_MaxRows2][MM_NAME_MAX + 4];
static char s_statusText[48];
static char s_numUsersText[24];
static char s_descText[64];

/* Which open game each visible host row stands for, so a click can resolve back
 * to a name. -1 = the row is empty. */
static int32_t s_rowGame[MatchMakingMenu_kGS_MaxRows];

/* Who is waiting on our yes or no (hostGameMessageMenu). */
static char s_joinAsker[MM_NAME_MAX];

static void matchMakingMenu_OnInit(void)
{
    int32_t row;

    for (row = 0; row < MatchMakingMenu_kGS_MaxRows; row++) {
        set_visible(MatchMakingMenu_host_00 + row, 0);
        set_visible(MatchMakingMenu_mode_00 + row, 0);
        set_visible(MatchMakingMenu_action_00 + row, 0);
        s_rowGame[row] = -1;
    }
    for (row = 0; row < MatchMakingMenu_kGS_MaxRows2; row++)
        set_visible(MatchMakingMenu_users_00 + row, 0);

    set_text(MatchMakingMenu_userName, s_username);
    set_entry(MatchMakingMenu_menuExit, "EXIT", 1, kLightBrownWithRedFrame);
}

/* Runs every frame (the menu is flagged dos_redraw). This is also where the
 * client is pumped: there is no other per-frame hook while a menu is up, and
 * MmClient_Poll() is cheap when nothing has arrived. */
static void matchMakingMenu_OnDraw(void)
{
    const MmUser *users;
    const MmGame *games;
    const MmGameInfo *gi;
    int32_t numUsers = 0, numGames = 0, row;
    MmState state;
    char who[MM_NAME_MAX];

    MmClient_Poll();
    state = MmClient_State();

    /* Someone is asking to join OUR game: ask, the way SWOS 2020 does, rather
     * than accepting for the player. One-shot, so the prompt opens once per
     * request however many frames it takes to answer. */
    if (MmClient_TakeJoinRequest(who, sizeof who)) {
        snprintf(s_joinAsker, sizeof s_joinAsker, "%s", who);
        OnlineMenus_Show(SWOS2020_MENU(hostGameMessageMenu));
        return;
    }

    if (MmClient_TakeRemoved())
        set_entry(MatchMakingMenu_desc, "THE HOST LEFT THE GAME", 0, 0);

    switch (state) {
    case MM_CONNECTING:
        snprintf(s_statusText, sizeof s_statusText, "CONNECTING");
        break;
    case MM_CONNECTED:
        snprintf(s_statusText, sizeof s_statusText, "ONLINE");
        break;
    case MM_FAILED:
        snprintf(s_statusText, sizeof s_statusText, "%s", MmClient_FailureText());
        break;
    default:
        snprintf(s_statusText, sizeof s_statusText, "OFFLINE");
        break;
    }
    set_text(MatchMakingMenu_connectionStatus, s_statusText);

    users = MmClient_Users(&numUsers);
    games = MmClient_OpenGames(&numGames);
    gi    = MmClient_GameInfo();

    snprintf(s_numUsersText, sizeof s_numUsersText, "%d ONLINE", numUsers);
    set_text(MatchMakingMenu_numOfUsers, state == MM_CONNECTED ? s_numUsersText : "");

    /* The open games, one per host row. */
    for (row = 0; row < MatchMakingMenu_kGS_MaxRows; row++) {
        int32_t hasGame = (state == MM_CONNECTED) && (row < numGames);
        s_rowGame[row] = hasGame ? row : -1;
        if (hasGame) {
            snprintf(s_hostRow[row], sizeof s_hostRow[row], "%s", games[row].owner);
            snprintf(s_modeRow[row], sizeof s_modeRow[row], "FRIENDLY");
            set_text(MatchMakingMenu_host_00 + row, s_hostRow[row]);
            set_text(MatchMakingMenu_mode_00 + row, s_modeRow[row]);
        }
        set_visible(MatchMakingMenu_host_00 + row, hasGame);
        set_visible(MatchMakingMenu_mode_00 + row, hasGame);
        set_visible(MatchMakingMenu_action_00 + row, 0);
    }

    /* Who else is on. */
    for (row = 0; row < MatchMakingMenu_kGS_MaxRows2; row++) {
        int32_t hasUser = (state == MM_CONNECTED) && (row < numUsers);
        if (hasUser) {
            snprintf(s_userRow[row], sizeof s_userRow[row], "%s", users[row].name);
            set_text(MatchMakingMenu_users_00 + row, s_userRow[row]);
        }
        set_visible(MatchMakingMenu_users_00 + row, hasUser);
    }

    /* The description line and what the two buttons can do, from where we are. */
    if (state != MM_CONNECTED) {
        set_entry(MatchMakingMenu_desc,
                  state == MM_CONNECTING ? "CONNECTING TO SERVER" : "SERVER NOT CONNECTED", 0, 0);
        set_entry(MatchMakingMenu_openGame, "OPEN", 0, kGray);
        set_entry(MatchMakingMenu_cancelGame, "LEAVE", 0, kGray);
    } else if (gi) {
        if (gi->numRequested > 0)
            snprintf(s_descText, sizeof s_descText, "%s WANTS TO JOIN", gi->requested[0].name);
        else if (gi->numJoined > 0)
            snprintf(s_descText, sizeof s_descText, "%s JOINED - STARTING", gi->joined[0].name);
        else
            snprintf(s_descText, sizeof s_descText, "WAITING FOR AN OPPONENT");
        set_entry(MatchMakingMenu_desc, s_descText, 0, 0);
        set_entry(MatchMakingMenu_openGame, "OPEN", 0, kGray);
        set_entry(MatchMakingMenu_cancelGame, "LEAVE", 1, kLightBrownWithRedFrame);
    } else {
        set_entry(MatchMakingMenu_desc,
                  numGames ? "PICK A GAME, OR OPEN YOUR OWN" : "OPEN A GAME AND WAIT", 0, 0);
        set_entry(MatchMakingMenu_openGame, "OPEN", 1, kLightBrownWithRedFrame);
        set_entry(MatchMakingMenu_cancelGame, "LEAVE", 0, kGray);
    }

    /* The server has paired us with somebody: hand over to the match. */
    if (MmClient_StartInfo())
        OnlineMenus_StartMatchFromMatchmaking();
}

/* ======================================================================
 * hostGameMessageMenu -- "X wants to play. Yes or no?"
 * ======================================================================
 * The host decides. Accepting sends APPROVE, which the server turns into the
 * START both peers need; refusing ejects them and leaves the game open for
 * somebody else. Either way the lobby is what we return to.
 * ====================================================================== */
static char s_joinPrompt[64];

static void hostGameMessageMenu_OnInit(void)
{
    snprintf(s_joinPrompt, sizeof s_joinPrompt, "%s WANTS TO PLAY", s_joinAsker);
    set_text(HostGameMessageMenu_header,   "A CHALLENGER");
    set_text(HostGameMessageMenu_message1, s_joinPrompt);
    set_text(HostGameMessageMenu_message2, "");
    set_text(HostGameMessageMenu_message3, "");
    set_entry(HostGameMessageMenu_button1, "ACCEPT", 1, kLightBrownWithRedFrame);
    set_entry(HostGameMessageMenu_button2, "REFUSE", 1, kLightBrownWithRedFrame);
    set_text(HostGameMessageMenu_exit,     "EXIT");
}

/* The lobby is not being drawn while this prompt is up, and its OnDraw is what
 * pumps the client -- so this one has to, or the connection would time out
 * exactly while the player is deciding. */
static void hostGameMessageMenu_OnDraw(void)
{
    MmClient_Poll();
    if (MmClient_State() != MM_CONNECTED)
        SetExitMenuFlag();          /* nothing left to accept */
}

static void hostGameMessageMenu_button1_OnSelect(void)   /* ACCEPT */
{
    if (s_joinAsker[0])
        MmClient_Approve(s_joinAsker);
    s_joinAsker[0] = '\0';
    SetExitMenuFlag();
}

static void hostGameMessageMenu_button2_OnSelect(void)   /* REFUSE */
{
    if (s_joinAsker[0])
        MmClient_Eject(s_joinAsker);
    s_joinAsker[0] = '\0';
    SetExitMenuFlag();
}

/* ======================================================================
 * The handoff: matchmaking is over, connect to the peer
 * ======================================================================
 * The server has paired us and given us the other player's addresses. From here
 * the server is out of the loop and the two of us talk directly: both punch to all
 * of the other's addresses over p2p_link (docs/NETPLAY.md sections 6 and 7), then
 * the lobby (mpGameSetupMenu below) sets the match up and the match is played. */

static char s_matchStatus[64];
static int32_t s_matchPlayer;      /* 1 = we created the game, 2 = we joined */
static char    s_myName[MM_NAME_MAX];

void OnlineMenus_StartMatchFromMatchmaking(void)
{
    const MmStartInfo *st = MmClient_StartInfo();
    if (!st)
        return;

    /* Take it once: this runs from a per-frame draw, and without clearing it we
     * would try to connect again on every one of them. */
    MmClient_ClearStart();

    s_matchPlayer  = st->playerNumber;
    /* Our login name, for the lobby's player column: read it before the disconnect
     * below forgets it. */
    snprintf(s_myName, sizeof s_myName, "%s", MmClient_UserName());

    /* NAT traversal: BOTH peers punch to all of the other's candidates at once.
     * Neither hosts -- a peer that only listens is unreachable from behind a
     * router, because a NAT lets a datagram in only after one has gone out to
     * that address. The player number decides who is player 1 and nothing else.
     *
     * The local port is the one the matchmaker saw us on, so the mapping its own
     * traffic already opened is the one the peer's packets arrive through. */
    {
        const char *list[MM_MAX_PEER_ADDRS];
        int32_t i;
        for (i = 0; i < st->numAddrs && i < MM_MAX_PEER_ADDRS; i++)
            list[i] = st->addrs[i];
        if (i == 0 || P2PLink_Punch(list, i, st->localPort) != 0)
            snprintf(s_matchStatus, sizeof s_matchStatus, "NO WAY TO REACH %s", st->peerName);
        else
            snprintf(s_matchStatus, sizeof s_matchStatus, "CONNECTING TO %s (%d route%s)",
                     st->peerName, i, i == 1 ? "" : "s");
    }

    /* The lobby is done with; the link takes over from here. */
    MmClient_Disconnect();
    OnlineMenus_Show(SWOS2020_MENU(mpWaitingMenu));
}

const char *OnlineMenus_MatchStatus(void)
{
    return s_matchStatus;
}

/* ======================================================================
 * mpWaitingMenu -- until the two players are connected
 * ====================================================================== */
static char s_waitLine[80];

/* Connected and the input delay agreed: into the lobby. It returns when the match has
 * been played or somebody has left, and either way this menu is done too. */
static void mpWaitingMenu_OnDraw(void)
{
    P2PLinkState st;

    P2PLink_Poll();
    st = P2PLink_State();

    if (st == P2P_CONNECTED && P2PLink_AgreedDelay() >= 0) {
        OnlineMenus_EnterLobby(s_matchPlayer, s_myName);
        SetExitMenuFlag();
        return;
    }

    switch (st) {
    case P2P_CONNECTED: {
        int32_t ping = P2PLink_PingMs();
        if (ping >= 0)
            snprintf(s_waitLine, sizeof s_waitLine, "CONNECTED TO %s - %d MS",
                     P2PLink_PeerText(), ping);
        else
            snprintf(s_waitLine, sizeof s_waitLine, "CONNECTED TO %s", P2PLink_PeerText());
        break;
    }
    case P2P_HOSTING:
    case P2P_JOINING:
        snprintf(s_waitLine, sizeof s_waitLine, "%s", s_matchStatus);
        break;
    case P2P_FAILED:
        snprintf(s_waitLine, sizeof s_waitLine, "CONNECTION FAILED");
        break;
    default:
        snprintf(s_waitLine, sizeof s_waitLine, "%s", s_matchStatus);
        break;
    }
    set_text(MpWaitingMenu_message1, s_waitLine);
}

static void mpWaiting_menuExit_OnSelect(void)
{
    P2PLink_Close();
    SetExitMenuFlag();
}

/* ======================================================================
 * mpGameSetupMenu -- the lobby (swos-2020/src/menus/mpGameSetup.mnu)
 * ======================================================================
 * The protocol is src/netplay_lobby.c; this is only its face. Row 01 is player 1
 * (the host, the top team), row 02 player 2. A player can change their own row --
 * TEAMS opens the game's team list, TACTICS steps through the formations and the
 * USER tactics, GO says ready -- and the host the rules on the right. START needs
 * both GO lights on.
 *
 * Kept from SWOS 2020's screen: the columns, the rules, the latency, START and
 * CANCEL. Hidden until they exist here: the location flags, the ranked and
 * tournament modes, the team database choice, cup legs, the chat and the live
 * cast. */

/* The team list is a MENU of its own, modal, and while it is up nothing calls this
 * screen's onDraw -- so nothing would poll the link, and the other player would
 * see this one go quiet and drop the connection after six seconds. The frame hook
 * keeps it breathing: Platform_VBlank runs it on every frame of every menu. */
static void lobby_frame_hook(void)
{
    P2PLink_Poll();
}

/* Test knobs for looking at the lobby without a pad (docs/NETPLAY.md, "The lobby"):
 * SWOS_LOBBY_TEAM=<file>:<ordinal> in hex picks a team as the lobby opens,
 * SWOS_LOBBY_READY=1 says ready, and SWOS_LOBBY_START=1 has the host press START the
 * moment it can. */
static void lobby_test_knobs(void)
{
    extern uint8_t _teamFileBuffer[];
    const char *team = getenv("SWOS_LOBBY_TEAM");
    unsigned file, ordinal;

    if (team && sscanf(team, "%x:%x", &file, &ordinal) == 2 && LoadTeamFile((uint16_t)file) == 0) {
        uint16_t count = *(uint16_t *)_teamFileBuffer, i;     /* host-native count, then records */
        TeamFileEx *rec = (TeamFileEx *)(_teamFileBuffer + 2);
        for (i = 0; i < count; i++) {
            if (rec[i].team.countryNumber == file && rec[i].team.teamOrdinal == ordinal) {
                NetplayLobby_SetTeam(&rec[i]);
                break;
            }
        }
    }
    if (getenv("SWOS_LOBBY_READY"))
        NetplayLobby_SetReady(1);
}

void OnlineMenus_EnterLobby(int32_t player, const char *name)
{
    OnlineMenus_Init();
    NetplayLobby_Begin(player, name);
    lobby_test_knobs();
    OnlineMenus_Show(SWOS2020_MENU(mpGameSetupMenu));
    NetplayLobby_End();
    P2PLink_Close();
    Platform_SetFrameHook(NULL);
}

static char s_lobbyHeader[40], s_lobbyStyle[24], s_lobbyConn[24], s_lobbyPing[16];
static char s_lobbyActivity[64];
static char s_rowName[2][LOBBY_NAME_MAX + 1];
static char s_rowTeam[2][18];
static char s_rowTactic[2][10];
static char s_ruleMin[8], s_ruleMax[8], s_ruleDelay[12];

/* The ordinal of this machine's entry in the column whose header row is `column00`. */
static int32_t my_row(int32_t column00)    { return column00 + NetplayLobby_LocalPlayer(); }

static void lobby_hide(int32_t ordinal)    { set_visible(ordinal, 0); }

static const char *tactic_name(const LobbyPlayer *p, char *buf, uint32_t cap)
{
    extern void *_tacticsStringTable[];
    extern uint8_t _teamTacticsUSERA[], _teamTacticsUSERB[], _teamTacticsUSERC[];
    extern uint8_t _teamTacticsUSERD[], _teamTacticsUSERE[], _teamTacticsUSERF[];
    static uint8_t *const slots[6] = {
        _teamTacticsUSERA, _teamTacticsUSERB, _teamTacticsUSERC,
        _teamTacticsUSERD, _teamTacticsUSERE, _teamTacticsUSERF,
    };
    const char *name;

    if (!p->hasTeam)
        return "-";
    if (p->tactic < TACTIC_USER_A)
        name = (const char *)_tacticsStringTable[p->tactic];
    else if (p == NetplayLobby_Player(NetplayLobby_LocalPlayer()))
        name = (const char *)slots[p->tactic - TACTIC_USER_A];     /* our own slot, as it is */
    else
        name = (const char *)p->userTactic.name;                   /* theirs, as sent */
    snprintf(buf, cap, "%.9s", name);
    return buf;
}

static const char *pitch_name(uint16_t pitch)
{
    extern const char _aRandom[], _aFrozen[], _aMuddy[], _aWet[], _aSoft[];
    extern const char _aNormal[], _aDry[], _aHard[];
    static const char *const names[7] = { _aFrozen, _aMuddy, _aWet, _aSoft, _aNormal, _aDry, _aHard };
    return pitch < 7 ? names[pitch] : _aRandom;
}

static const char *length_name(uint16_t length)
{
    extern const char _a3Mins[], _a5Mins[], _a7Mins[], _a10Mins[];
    static const char *const names[4] = { _a3Mins, _a5Mins, _a7Mins, _a10Mins };
    return names[length < 4 ? length : 0];
}

/* Where the cursor goes from an entry. SWOS 2020 wires this screen in code too
 * (setEntry in mpGameSetupMenu_OnDraw): the .mnu gives most of these entries no
 * neighbours at all, and a direction with none goes nowhere. A disabled or hidden
 * neighbour is passed over along ITS link in the same direction (MenuProc), so one
 * layout serves both players: the joiner's DOWN runs through the host's rules to
 * CANCEL. */
static void lobby_nav(int32_t ordinal, int32_t left, int32_t right, int32_t up, int32_t down)
{
    MenuEntry *e = entry_at(ordinal);
    e->leftEntry  = (int8_t)left;
    e->rightEntry = (int8_t)right;
    e->upEntry    = (int8_t)up;
    e->downEntry  = (int8_t)down;
    e->directionLeft = e->directionRight = e->directionUp = e->directionDown = -1;
}

/* What this screen hides, labels and links. Applied on EVERY draw, not once in onInit:
 * coming back from the team list restores the menu from its template -- every entry
 * visible again, the column headers blank -- and there is no onReturn to redo it in. */
static void lobby_layout(void)
{
    int32_t i;
    int32_t row = NetplayLobby_LocalPlayer();       /* 1 or 2: our row */
    int32_t teams = MpGameSetupMenu_teams_00 + row;
    int32_t tactics = MpGameSetupMenu_tactics_00 + row;
    int32_t go = MpGameSetupMenu_go_00 + row;

    lobby_nav(teams,   -1,      tactics, -1, MpGameSetupMenu_gameLength1);
    lobby_nav(tactics, teams,   go,      -1, MpGameSetupMenu_gameLength1);
    lobby_nav(go,      tactics, -1,      -1, MpGameSetupMenu_gameLength1);
    lobby_nav(MpGameSetupMenu_gameLength1, -1, -1, teams, MpGameSetupMenu_pitchType1);
    lobby_nav(MpGameSetupMenu_pitchType1,  -1, -1, MpGameSetupMenu_gameLength1, MpGameSetupMenu_minSubs1);
    lobby_nav(MpGameSetupMenu_minSubs1, -1, MpGameSetupMenu_maxSubs1,
              MpGameSetupMenu_pitchType1, MpGameSetupMenu_cancel);
    lobby_nav(MpGameSetupMenu_maxSubs1, MpGameSetupMenu_minSubs1, -1,
              MpGameSetupMenu_pitchType1, MpGameSetupMenu_start);
    lobby_nav(MpGameSetupMenu_cancel, -1, MpGameSetupMenu_start, MpGameSetupMenu_minSubs1, -1);
    lobby_nav(MpGameSetupMenu_start, MpGameSetupMenu_cancel, -1, MpGameSetupMenu_maxSubs1, -1);

    lobby_hide(MpGameSetupMenu_teamDB);
    lobby_hide(MpGameSetupMenu_latency);
    lobby_hide(MpGameSetupMenu_latencyPercent);
    for (i = 0; i < MpGameSetupMenu_kGS_MaxRows; i++)
        lobby_hide(MpGameSetupMenu_location_00 + i);
    lobby_hide(MpGameSetupMenu_rankedMode1Label);
    lobby_hide(MpGameSetupMenu_rankedModeOption1);
    lobby_hide(MpGameSetupMenu_teamDB1Label);
    lobby_hide(MpGameSetupMenu_teamDB1);
    lobby_hide(MpGameSetupMenu_etPen1Label);
    lobby_hide(MpGameSetupMenu_legOpt1);
    lobby_hide(MpGameSetupMenu_etPen1);
    lobby_hide(MpGameSetupMenu_ggpoFrameDelayDec);
    lobby_hide(MpGameSetupMenu_ggpoFrameDelayInc);
    lobby_hide(MpGameSetupMenu_matchInfo);
    lobby_hide(MpGameSetupMenu_chatLabel);
    lobby_hide(MpGameSetupMenu_liveCastUrlBtn);
    for (i = MpGameSetupMenu_chatMsg00; i <= MpGameSetupMenu_chatMsg09; i++)
        lobby_hide(i);
    lobby_hide(MpGameSetupMenu_scrollUp);
    lobby_hide(MpGameSetupMenu_scrollDown);
    lobby_hide(MpGameSetupMenu_inputChatMsg);

    set_entry(MpGameSetupMenu_cPlayers_00, "PLAYERS", 0, kGray);
    set_entry(MpGameSetupMenu_teams_00,    "TEAMS",   0, kGray);
    set_entry(MpGameSetupMenu_tactics_00,  "TACTICS", 0, kGray);
    set_entry(MpGameSetupMenu_go_00,       "GO",      0, kGray);
    set_text(MpGameSetupMenu_latencyLabel, "LATENCY:");
    set_entry(MpGameSetupMenu_gameStyle1, "", 0, kGray);        /* the link already made both agree */
    set_entry(MpGameSetupMenu_ggpoFrameDelay_, "", 0, kGray);   /* agreed by the link, not chosen here */
}

static void mpGameSetupMenu_OnInit(void)
{
    Platform_SetFrameHook(lobby_frame_hook);
    lobby_layout();

    /* Start on this player's TEAMS button: the first thing to do. */
    {
        extern uint8_t _currentMenu[];
        ((Menu *)_currentMenu)->selectedEntry = entry_at(my_row(MpGameSetupMenu_teams_00));
    }
}

static void run_live_match(void);
static int32_t current_ordinal(void);

static void mpGameSetupMenu_OnDraw(void)
{
    extern int _gDosGameplay;
    const LobbyPlayer *players[2];
    const LobbyRules *rules;
    LobbyState st;
    int32_t me, host, open, row;

    NetplayLobby_Poll();
    if (getenv("SWOS_LOBBY_START") && NetplayLobby_CanStart())
        (void)NetplayLobby_Start();
    st = NetplayLobby_State();
    if (st == LOBBY_KICKOFF) {
        run_live_match();
        return;
    }

    lobby_layout();
    me    = NetplayLobby_LocalPlayer();
    host  = NetplayLobby_IsHost();
    open  = st == LOBBY_OPEN;
    rules = NetplayLobby_Rules();
    players[0] = NetplayLobby_Player(1);
    players[1] = NetplayLobby_Player(2);

    snprintf(s_lobbyHeader, sizeof s_lobbyHeader, "FRIENDLY GAME - LOBBY (%s)", host ? "HOST" : "CLIENT");
    set_text(MpGameSetupMenu_header, s_lobbyHeader);
    snprintf(s_lobbyStyle, sizeof s_lobbyStyle, "%s", _gDosGameplay ? "PC (70FPS)" : "AMIGA (50FPS)");
    set_text(MpGameSetupMenu_gameStyle, s_lobbyStyle);
    set_text(MpGameSetupMenu_gameStyle1, s_lobbyStyle);
    snprintf(s_lobbyConn, sizeof s_lobbyConn, "%s", st == LOBBY_FAILED ? "DISCONNECTED" : "CONNECTED");
    set_text(MpGameSetupMenu_connectionStatus, s_lobbyConn);
    if (P2PLink_PingMs() >= 0)
        snprintf(s_lobbyPing, sizeof s_lobbyPing, "%dMS", P2PLink_PingMs());
    else
        snprintf(s_lobbyPing, sizeof s_lobbyPing, "...");
    set_text(MpGameSetupMenu_latencyPing, s_lobbyPing);

    for (row = 0; row < 2; row++) {
        const LobbyPlayer *p = players[row];
        int32_t mine = (row + 1 == me);
        int32_t canEdit = mine && open;

        if (p->known)
            snprintf(s_rowName[row], sizeof s_rowName[row], "%s", p->name);
        else
            snprintf(s_rowName[row], sizeof s_rowName[row], "WAITING...");
        set_entry(MpGameSetupMenu_cPlayers_01 + row, s_rowName[row], 0, kPurple);

        if (p->hasTeam)
            snprintf(s_rowTeam[row], sizeof s_rowTeam[row], "%.17s", (char *)p->team.team.teamName);
        else
            snprintf(s_rowTeam[row], sizeof s_rowTeam[row], "%s", mine ? "SELECT TEAM" : "-");
        set_entry(MpGameSetupMenu_teams_01 + row, s_rowTeam[row], canEdit,
                  canEdit ? kLightBrownWithYellowFrame : kGray);

        set_entry(MpGameSetupMenu_tactics_01 + row,
                  tactic_name(p, s_rowTactic[row], sizeof s_rowTactic[row]),
                  canEdit && p->hasTeam, canEdit && p->hasTeam ? kLightBrownWithYellowFrame : kGray);

        set_entry(MpGameSetupMenu_go_01 + row, p->ready ? "READY" : "-",
                  canEdit && p->hasTeam, p->ready ? kGreen : (canEdit && p->hasTeam ? kRed : kGray));
    }

    /* The rules: the host's to change, everyone's to read. */
    set_entry(MpGameSetupMenu_gameLength1, length_name(rules->gameLength), host && open,
              host && open ? kLightBlue : kGray);
    set_entry(MpGameSetupMenu_pitchType1, pitch_name(rules->pitchType), host && open,
              host && open ? kLightBlue : kGray);
    snprintf(s_ruleMin, sizeof s_ruleMin, "%u", (unsigned)rules->minSubs);
    snprintf(s_ruleMax, sizeof s_ruleMax, "%u", (unsigned)rules->maxSubs);
    set_entry(MpGameSetupMenu_minSubs1, s_ruleMin, host && open && rules->maxSubs > 0,
              host && open && rules->maxSubs > 0 ? kLightBlue : kGray);
    set_entry(MpGameSetupMenu_maxSubs1, s_ruleMax, host && open, host && open ? kLightBlue : kGray);
    {
        int32_t delay;
        NetplayLobby_Peer(NULL, 0, NULL, NULL, &delay);
        snprintf(s_ruleDelay, sizeof s_ruleDelay, "%d", delay);
        set_text(MpGameSetupMenu_ggpoFrameDelay_, s_ruleDelay);
    }

    if (host) {
        int32_t can = NetplayLobby_CanStart();
        set_entry(MpGameSetupMenu_start, "START", can, can ? kRed : kGray);
    } else {
        set_visible(MpGameSetupMenu_start, 0);
    }

    /* One line: what went wrong, what the other player is doing, or what to do next. */
    {
        const LobbyPlayer *other = players[2 - me];
        const LobbyPlayer *self  = players[me - 1];
        const char *line = NetplayLobby_Notice();
        if (!line[0]) {
            if (st == LOBBY_STARTING)
                line = "STARTING...";
            else if (other->known && other->activity == LOBBY_ACTIVITY_CHOOSING_TEAM)
                line = "YOUR OPPONENT IS CHOOSING A TEAM";
            else if (!self->hasTeam)
                line = "CHOOSE YOUR TEAM";
            else if (!self->ready)
                line = "PRESS GO WHEN YOU ARE READY";
            else if (!other->ready)
                line = "WAITING FOR YOUR OPPONENT";
            else
                line = host ? "BOTH READY - PRESS START" : "BOTH READY - THE HOST STARTS";
        }
        snprintf(s_lobbyActivity, sizeof s_lobbyActivity, "%s", line);
        set_text(MpGameSetupMenu_oppoActivity, s_lobbyActivity);
    }
}

/* Whether the entry just selected is on this player's row of `column00`'s column. */
static int32_t on_my_row(int32_t column00)
{
    return current_ordinal() == my_row(column00) && NetplayLobby_State() == LOBBY_OPEN;
}

/* TEAMS: the game's own team list -- clubs, national sides and custom teams -- the
 * way IMPORT TEAM uses it (edit_tactics.c). */
static void mpGameSetup_teams_onSelect(void)
{
    extern void ChooseTeamsDialog(uint16_t showClub, uint16_t showNational, uint16_t showOrigCustom,
                                  uint8_t *title, void *onSelect);
    extern uint16_t    _selectedTeamCountryNumber;
    extern TeamFileEx *_selectedTeamPtr;
    static char title[] = "CHOOSE YOUR TEAM";

    if (!on_my_row(MpGameSetupMenu_teams_00))
        return;

    /* Tell the other player first -- and take ready down, so a start cannot be
     * offered while this player is away in the list. */
    NetplayLobby_SetReady(0);
    NetplayLobby_SetActivity(LOBBY_ACTIVITY_CHOOSING_TEAM);
    NetplayLobby_Poll();

    ChooseTeamsDialog(1, 1, 1, (uint8_t *)title, (void *)0);

    if ((int16_t)_selectedTeamCountryNumber != -1 && _selectedTeamPtr)
        NetplayLobby_SetTeam(_selectedTeamPtr);
    NetplayLobby_SetActivity(LOBBY_ACTIVITY_NONE);
}

/* TACTICS: the next formation, through the USER tactics and round again. */
static void mpGameSetup_tactics_onSelect(void)
{
    const LobbyPlayer *p = NetplayLobby_Player(NetplayLobby_LocalPlayer());
    if (!on_my_row(MpGameSetupMenu_tactics_00) || !p->hasTeam)
        return;
    NetplayLobby_SetTactic(p->tactic >= LOBBY_TACTIC_MAX ? 0 : p->tactic + 1);
}

/* GO: ready, or not. */
static void mpGameSetup_go_onSelect(void)
{
    const LobbyPlayer *p = NetplayLobby_Player(NetplayLobby_LocalPlayer());
    if (!on_my_row(MpGameSetupMenu_go_00))
        return;
    NetplayLobby_SetReady(!p->ready);
}

static void change_rules(void (*edit)(LobbyRules *r))
{
    LobbyRules r;
    if (!NetplayLobby_IsHost() || NetplayLobby_State() != LOBBY_OPEN)
        return;
    r = *NetplayLobby_Rules();
    edit(&r);
    NetplayLobby_SetRules(&r);
}

static void next_length(LobbyRules *r) { r->gameLength = (uint16_t)((r->gameLength + 1) % 4); }

/* RANDOM, FROZEN .. HARD, RANDOM: the friendly menu's order (FriendlyPitchTypeProc). */
static void next_pitch(LobbyRules *r)
{
    r->pitchType = r->pitchType == 0xFFFF ? 0 : r->pitchType >= 6 ? 0xFFFF : (uint16_t)(r->pitchType + 1);
}

/* The friendly menu's substitutes rules (MinSubstitutes / MaxSubstitutes, menus.c). */
static void next_min_subs(LobbyRules *r)
{
    if (r->maxSubs == 0)
        return;
    r->minSubs = (uint16_t)(r->minSubs >= r->maxSubs ? 1 : r->minSubs + 1);
}

static void next_max_subs(LobbyRules *r)
{
    r->maxSubs = (uint16_t)(r->maxSubs >= 5 ? 0 : r->maxSubs + 1);
    if (r->maxSubs == 0)
        r->minSubs = 0;
    else if (r->minSubs == 0 || r->minSubs > r->maxSubs)
        r->minSubs = 1;
}

static void selectGameLength1(void) { change_rules(next_length); }
static void selectPitchType1(void)  { change_rules(next_pitch); }
static void selectMinSubs1(void)    { change_rules(next_min_subs); }
static void selectMaxSubs1(void)    { change_rules(next_max_subs); }

static void selectMPSettingStart(void)
{
    (void)NetplayLobby_Start();
}

static void selectMPSettingExit(void)
{
    Platform_SetFrameHook(NULL);
    SetExitMenuFlag();              /* OnlineMenus_EnterLobby closes the link behind us */
}

/* Both sides agreed: play.
 *
 * The link is CLOSED before the session opens: GGPO wants its own socket, on the same
 * local port, which is also what keeps a NAT mapping open across the handover. The
 * lobby took the peer's address while the link was up.
 *
 * InitAndPlayGame runs the whole match and returns when it is over, exactly as it
 * does for the friendly menu and for arcade mode. */
static void run_live_match(void)
{
    extern uint16_t    _gFrameStepInput;
    extern TeamFileEx  _selectedTeams[100];
    extern void        InitAndPlayGame(TeamFileEx *team1, TeamFileEx *team2);

    char ip[24];
    uint16_t peerPort, localPort;
    int32_t delay;

    Platform_SetFrameHook(NULL);
    NetplayLobby_Peer(ip, sizeof ip, &peerPort, &localPort, &delay);
    NetplayLobby_End();
    P2PLink_Close();

    if (!ip[0] || NetplayLobby_ApplyMatch() != 0 ||
        NetplayMatch_StartLive(NetplayLobby_LocalPlayer(), ip, peerPort, localPort, delay) != 0) {
        snprintf(s_matchStatus, sizeof s_matchStatus, "COULD NOT START THE MATCH");
        NetplayLobby_RestoreTactics();
        SetExitMenuFlag();
        return;
    }

    snprintf(s_matchStatus, sizeof s_matchStatus, "PLAYING");
    _gFrameStepInput = 1;               /* one frame of input at a time (game_loop.h) */
    InitAndPlayGame(&_selectedTeams[0], &_selectedTeams[1]);
    NetplayMatch_Finish();
    NetplayLobby_RestoreTactics();
    SetExitMenuFlag();
}

/* OPEN: advertise a game and wait. */
static void matchMaking_openGame_onSelect(void)
{
    MmClient_CreateGame();
}

/* LEAVE: stop advertising, or withdraw a join request. */
static void matchMaking_cancelGame_onSelect(void)
{
    MmClient_LeaveGame();
}

/* A host row: ask to join that game. Which row was clicked is recovered from the
 * menu's selected entry -- CalcMenuEntryAddress is ordinal -> address, so this
 * is the inverse. */
static int32_t current_ordinal(void)
{
    /* The live menu's backing buffer. Declared as a byte array to match its real
     * storage in swos_globals.c (`currentMenu: ds.b $16` + the adjacent
     * menuEntries) -- the same spelling animated_patterns.c and
     * highlights_menu.c use. */
    extern uint8_t _currentMenu[];
    const Menu *menu = (const Menu *)_currentMenu;
    const uint8_t *base = _currentMenu + MENU_HEADER_SIZE;
    const uint8_t *sel  = (const uint8_t *)menu->selectedEntry;
    if (!sel || sel < base)
        return -1;
    return (int32_t)((size_t)(sel - base) / MENUENTRY_STRIDE);
}

static void matchMaking_host_onSelect(void)
{
    int32_t ord = current_ordinal();
    int32_t row = ord - MatchMakingMenu_host_00;
    int32_t numGames = 0;
    const MmGame *games = MmClient_OpenGames(&numGames);

    if (row < 0 || row >= MatchMakingMenu_kGS_MaxRows)
        return;
    if (s_rowGame[row] < 0 || s_rowGame[row] >= numGames)
        return;
    MmClient_JoinGame(games[s_rowGame[row]].owner);
}

/* A user row. Upstream opens playerInfoMenu here; nothing to show yet. */
static void matchMaking_users_onSelect(void)
{
}

/* EXIT: leave the lobby properly rather than abandoning a socket -- otherwise
 * the server keeps advertising a game nobody is behind until the ENET timeout. */
static void matchMaking_exit_onSelect(void)
{
    MmClient_Disconnect();
    SetExitMenuFlag();
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
    HANDLER(matchMakingMenu_OnDraw),
    HANDLER(matchMaking_menuSettings_onSelect),
    HANDLER(matchMaking_rankings_onSelect),
    HANDLER(matchMaking_openGame_onSelect),
    HANDLER(matchMaking_cancelGame_onSelect),
    HANDLER(matchMaking_host_onSelect),
    HANDLER(matchMaking_users_onSelect),
    HANDLER(matchMaking_exit_onSelect),

    HANDLER(hostGameMessageMenu_OnInit),
    HANDLER(hostGameMessageMenu_OnDraw),
    HANDLER(hostGameMessageMenu_button1_OnSelect),
    HANDLER(hostGameMessageMenu_button2_OnSelect),

    HANDLER(mpWaitingMenu_OnDraw),
    HANDLER(mpWaiting_menuExit_OnSelect),

    HANDLER(mpGameSetupMenu_OnInit),
    HANDLER(mpGameSetupMenu_OnDraw),
    HANDLER(mpGameSetup_teams_onSelect),
    HANDLER(mpGameSetup_tactics_onSelect),
    HANDLER(mpGameSetup_go_onSelect),
    HANDLER(selectGameLength1),
    HANDLER(selectPitchType1),
    HANDLER(selectMinSubs1),
    HANDLER(selectMaxSubs1),
    HANDLER(selectMPSettingStart),
    HANDLER(selectMPSettingExit),

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
    HANDLER_AS("matchView_exit_onSelect",            SetExitMenuFlag),
    HANDLER_AS("messageMenu_no_OnSelect",            SetExitMenuFlag),
    HANDLER_AS("messageMenu_yes_OnSelect",           SetExitMenuFlag),
    HANDLER_AS("myFriendlyStatsMenu_exit_OnSelect",  SetExitMenuFlag),
    HANDLER_AS("myRankedStatsMenu_exit_OnSelect",    SetExitMenuFlag),
    HANDLER_AS("selectExitOpenLobby",                SetExitMenuFlag),
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
 * --menu=<name>, --matchmaker-menu[=name]
 * ====================================================================== */
static char s_startMenu[64];
static char s_autoLogin[MM_NAME_MAX];

void OnlineMenus_SetAutoLogin(const char *name)
{
    snprintf(s_autoLogin, sizeof s_autoLogin, "%s", (name && name[0]) ? name : "PLAYER");
    snprintf(s_username, sizeof s_username, "%s", s_autoLogin);
    OnlineMenus_SetStartMenu("swos2020/matchMakingMenu");
}

void OnlineMenus_SetStartMenu(const char *name)
{
    snprintf(s_startMenu, sizeof s_startMenu, "%s", name);
}

/* --netplay-lobby=host|join: two instances on one machine meet over loopback and open
 * the lobby, the way a matchmaking handoff would -- how the lobby menu gets tried
 * without a server or a second computer. */
static int32_t s_loopbackLobby;     /* 0 off, 1 host, 2 join */

int32_t OnlineMenus_SetLoopbackLobby(const char *role)
{
    if (role && strcmp(role, "host") == 0)      s_loopbackLobby = 1;
    else if (role && strcmp(role, "join") == 0) s_loopbackLobby = 2;
    else                                        return 1;
    return 0;
}

void OnlineMenus_ShowStartMenu(void)
{
    if (s_loopbackLobby) {
        enum { kHostPort = 23921, kJoinPort = 23922 };
        char peer[24];
        const char *list[1] = { peer };
        int32_t player = s_loopbackLobby;

        s_loopbackLobby = 0;        /* once, like --menu */
        OnlineMenus_Init();
        snprintf(peer, sizeof peer, "127.0.0.1:%d", player == 1 ? kJoinPort : kHostPort);
        s_matchPlayer = player;
        snprintf(s_myName, sizeof s_myName, "%s", player == 1 ? "HOST" : "JOIN");
        snprintf(s_matchStatus, sizeof s_matchStatus, "CONNECTING OVER LOOPBACK");
        if (P2PLink_Punch(list, 1, (uint16_t)(player == 1 ? kHostPort : kJoinPort)) == 0)
            OnlineMenus_Show(SWOS2020_MENU(mpWaitingMenu));
        return;
    }
    if (!s_startMenu[0])
        return;
    char name[sizeof s_startMenu];
    memcpy(name, s_startMenu, sizeof name);
    s_startMenu[0] = '\0';      /* once: the front end can be re-entered after a match */
    OnlineMenus_Init();
    /* --matchmaker-menu: log in before the menu opens, so it comes up live.
     * Driving the lobby under gdb does not work -- a process stopped at a
     * breakpoint sends no ENET keepalives and the server times it out -- so this
     * is how the real menu gets looked at. */
    if (s_autoLogin[0]) {
        MmClient_Connect(mm_host(), mm_port(), s_autoLogin, NULL);
        s_autoLogin[0] = '\0';
    }
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
