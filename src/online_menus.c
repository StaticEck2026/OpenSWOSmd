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
#include "netplay_setup.h"    /* the match setup the host sends */
#include "netplay_match.h"    /* the live GGPO session */
#include "game_types.h"       /* TeamFileEx, for the match the handoff starts */

#include <stdio.h>
#include <stdlib.h>
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
 * the server is out of the loop and the two of us talk directly, over the same
 * p2p_link the direct-IP menu used (docs/NETPLAY.md sections 6 and 7).
 *
 * WHAT THIS DOES NOT DO YET: play the match. Opening the GGPO session -- the
 * host sending the match setup, live pad input, running the session over the
 * link -- does not exist on EITHER path (NETPLAY.md section 6, "What is
 * missing"), so matchmaking now ends where the direct-IP menu ended: two peers
 * connected and pinging. That is the next chunk, and it is the same chunk for
 * both entry points.
 *
 * Port: p2p_link binds its own socket, so the matchmaker's addresses supply the
 * peer's IP but not a port to meet on. Both sides use the same fixed port as the
 * direct-IP menu (SWOS 2020's GGPO port). That is fine on one machine and on a
 * LAN -- p2p_link takes the next free port when that one is busy, which is what
 * makes two instances on one box work -- but it does NOT punch through NAT. Real
 * NAT traversal is the reason the server sends several candidate addresses, and
 * using them properly is part of the same unfinished chunk. */
#define MM_MATCH_PORT 47885

static char s_matchStatus[64];
static int32_t s_matchPlayer;      /* 1 = we created the game, 2 = we joined */
static int32_t s_setupSent;        /* host: the setup is on its way */
static int32_t s_setupApplied;     /* joiner: the host's setup is ours now */

/* The peer's address, taken while the link is CERTAINLY up rather than at
 * handover. The other side closes its link as soon as it has what it needs, and
 * the BYE that follows clears our peer -- so reading it late gives an empty
 * address and a session that will not start. */
static char     s_peerAddr[24];
static uint16_t s_peerPort;
static uint16_t s_localPort;

void OnlineMenus_StartMatchFromMatchmaking(void)
{
    const MmStartInfo *st = MmClient_StartInfo();
    if (!st)
        return;

    /* Take it once: this runs from a per-frame draw, and without clearing it we
     * would try to connect again on every one of them. */
    MmClient_ClearStart();

    s_matchPlayer  = st->playerNumber;
    s_setupSent    = 0;
    s_setupApplied = 0;
    s_peerAddr[0]  = '\0';
    s_peerPort     = 0;
    s_localPort    = 0;

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
 * mpWaitingMenu -- where the two connected players wait
 * ====================================================================== */
static char s_waitLine[80];

/* Exchange the match setup, then play.
 *
 * Both peers must start from the same teams, tactics, options and random seed,
 * so the host sends its setup and the joiner adopts it. Then BOTH run the same
 * deterministic init over it -- which is how the corpus two-peer test gets two
 * identical machines, and is why this sends the setup rather than a copy of the
 * host's post-init state.
 *
 * The link is CLOSED before the session opens: GGPO wants its own socket, on
 * the same local port, which is also what keeps a NAT mapping open across the
 * handover. Everything the session needs is therefore read out first.
 *
 * InitAndPlayGame runs the whole match and returns when it is over, exactly as
 * it does for the friendly menu and for arcade mode. */
/* Remember where the peer is, the moment we know. */
static void capture_peer(void)
{
    char peer[24], *colon;
    if (s_peerAddr[0])
        return;
    snprintf(peer, sizeof peer, "%s", P2PLink_PeerText());
    if (!peer[0])
        return;
    snprintf(s_peerAddr, sizeof s_peerAddr, "%s", peer);
    colon = strchr(s_peerAddr, ':');
    if (colon) {
        *colon = '\0';
        s_peerPort = (uint16_t)atoi(colon + 1);
    }
    if (!s_peerPort)
        s_peerPort = MM_MATCH_PORT;
    s_localPort = P2PLink_LocalPort();
}

static void run_live_match(void)
{
    /* Declared where they are used: this file is menus, and these three belong
     * to the match. Spellings as game_loop.h / game_types.h define them. */
    extern uint16_t    _gFrameStepInput;
    extern TeamFileEx  _selectedTeams[100];
    extern void        InitAndPlayGame(TeamFileEx *team1, TeamFileEx *team2);

    int32_t delay = P2PLink_AgreedDelay();

    capture_peer();                     /* if it has not happened already */
    if (delay < 0)
        delay = 2;                      /* both sides fall back the same way */
    if (!s_peerAddr[0]) {
        snprintf(s_matchStatus, sizeof s_matchStatus, "LOST THE OPPONENT'S ADDRESS");
        return;
    }

    P2PLink_Close();

    if (NetplayMatch_StartLive(s_matchPlayer, s_peerAddr, s_peerPort, s_localPort, delay) != 0) {
        snprintf(s_matchStatus, sizeof s_matchStatus, "COULD NOT START THE MATCH");
        return;
    }

    snprintf(s_matchStatus, sizeof s_matchStatus, "PLAYING");
    _gFrameStepInput = 1;               /* one frame of input at a time (game_loop.h) */
    InitAndPlayGame(&_selectedTeams[0], &_selectedTeams[1]);
    NetplayMatch_Finish();
    SetExitMenuFlag();
}

static void mpWaitingMenu_OnDraw(void)
{
    P2PLinkState st;

    P2PLink_Poll();
    st = P2PLink_State();

    if (st == P2P_CONNECTED)
        capture_peer();

    /* Connected and the delay agreed: swap the setup, then kick off. */
    if (st == P2P_CONNECTED && P2PLink_AgreedDelay() >= 0) {
        if (s_matchPlayer == 1) {
            if (!s_setupSent) {
                uint8_t *buf = (uint8_t *)malloc(NetplaySetup_MaxBytes());
                int32_t n = buf ? NetplaySetup_Encode(buf, NetplaySetup_MaxBytes()) : -1;
                if (n > 0 && P2PLink_SendSetup(buf, (uint32_t)n) == 0)
                    s_setupSent = 1;
                free(buf);
            } else if (P2PLink_SetupSent()) {
                run_live_match();
                return;
            }
        } else {
            uint32_t n = 0;
            const uint8_t *got = P2PLink_ReceivedSetup(&n);
            if (got && !s_setupApplied) {
                if (NetplaySetup_Apply(got, n) == 0)
                    s_setupApplied = 1;
                else
                    snprintf(s_matchStatus, sizeof s_matchStatus, "BAD SETUP FROM THE HOST");
            }
            if (s_setupApplied) {
                run_live_match();
                return;
            }
        }
    }

    switch (st) {
    case P2P_CONNECTED: {
        int32_t ping = P2PLink_PingMs();
        int32_t pct  = P2PLink_SetupProgress();
        if (pct >= 0) {
            snprintf(s_waitLine, sizeof s_waitLine, "MATCH SETUP %d%%", pct);
            break;
        }
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

void OnlineMenus_ShowStartMenu(void)
{
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
