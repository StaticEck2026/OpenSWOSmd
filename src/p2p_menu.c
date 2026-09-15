/*
 * p2p_menu.c -- the MULTIPLAYER menu: two players connect to each other directly, by
 * IP address, with no server in between (SDL builds; docs/NETPLAY.md, "The MULTIPLAYER
 * menu"). It is what the main menu's MULTIPLAYER button opens.
 *
 * The menu itself is JSON only -- bin/data_json/menus/p2p/p2pMenu.json, built on first
 * use by MenuJson_Load (docs/MENUS.md section 10) -- and the connection is
 * src/p2p_link.c. The menu is flagged dos_redraw, so its onDraw runs every frame: that
 * is where the link is polled and the status panel written.
 *
 * What it does today is connect and check the two games match, which is the part a
 * tester can try over the internet: the address, the port, the router. Setting the
 * match up over the link and playing it through GGPO comes next, and the input delay
 * chosen here is for that.
 *
 * SWOS 2020's own online menus are imported separately and are a different thing
 * entirely -- a login, a lobby and a matchmaking server (src/online_menus.c).
 */
#include "p2p_menu.h"

#ifdef SWOS_SDL_BUILD

#include "p2p_link.h"
#include "input_text.h"
#include "menu_load_json.h"
#include "menu_symbols.h"
#include "menu_system.h"

#include <stdio.h>
#include <string.h>

/* The menu's entries, in the order p2pMenu.json lists them. */
enum {
    P2PMenu_header,
    P2PMenu_subTitle,
    P2PMenu_modeLabel,
    P2PMenu_mode,
    P2PMenu_addressLabel,
    P2PMenu_address,
    P2PMenu_portLabel,
    P2PMenu_port,
    P2PMenu_delayLabel,
    P2PMenu_delay,
    P2PMenu_panel,
    P2PMenu_status1,
    P2PMenu_status2,
    P2PMenu_start,
    P2PMenu_exit,
    P2PMenu_kNumEntries
};

/* SWOS 2020's colour constants (mnu2h/Constants.py), the ones this menu uses. */
enum { kGray = 7, kLightBlue = 13 };

enum { kMaxInputDelay = 6 };    /* frames; GGPO rolls back at most 8 (docs/NETPLAY.md) */

static int32_t s_hosting = 1;               /* host a game, or join one */
static char    s_address[16];               /* the host's IPv4, typed in */
static char    s_port[6] = "47885";         /* SWOS 2020's GGPO port (mpgame.h GGPO_PORT1) */
static int32_t s_delay = 2;

static char s_delayText[24];
static char s_status1[64];
static char s_status2[64];

/* What the last thing the player did had to say, until they do the next. */
static const char *s_notice1;
static const char *s_notice2;

/* ---- entries ---- */

static MenuEntry *entry_at(int32_t ordinal)
{
    return CalcMenuEntryAddress((uint16_t)ordinal);
}

static void set_text(int32_t ordinal, const char *text)
{
    entry_at(ordinal)->fg = (uintptr_t)text;
}

/* A settings row: its text, whether it can be chosen now, and the colour that says so. */
static void set_row(int32_t ordinal, const char *text, int32_t enabled)
{
    MenuEntry *e = entry_at(ordinal);
    e->fg         = (uintptr_t)text;
    e->isDisabled = enabled ? 0 : 1;
    e->bgColor    = (uint16_t)(enabled ? kLightBlue : kGray);
}

static void notice(const char *line1, const char *line2)
{
    s_notice1 = line1;
    s_notice2 = line2;
}

/* ---- the settings ---- */

/* The port as a number, or 0 if what was typed is not one this can bind. Ports below
 * 1024 need privileges on every platform we ship, so they are not offered. */
static uint16_t parse_port(const char *text)
{
    int32_t value = 0;

    if (!text[0])
        return 0;
    for (const char *p = text; *p; p++) {
        if (*p < '0' || *p > '9')
            return 0;
        value = value * 10 + (*p - '0');
        if (value > 65535)
            return 0;
    }
    return value < 1024 ? 0 : (uint16_t)value;
}

static const char *delay_text(void)
{
    if (s_delay == 0)
        snprintf(s_delayText, sizeof s_delayText, "NONE");
    else if (s_delay == 1)
        snprintf(s_delayText, sizeof s_delayText, "1 FRAME (20 MS)");
    else
        snprintf(s_delayText, sizeof s_delayText, "%d FRAMES (%d MS)", s_delay, s_delay * 20);
    return s_delayText;
}

/* ---- the status panel ---- */

static void failure_text(P2PLinkFailure failure, const char **line1, const char **line2)
{
    switch (failure) {
    case P2P_FAIL_UNAVAILABLE:
        *line1 = "NETWORK PLAY IS NOT IN THIS BUILD"; *line2 = ""; break;
    case P2P_FAIL_BAD_ADDRESS:
        *line1 = "THAT IS NOT AN IP ADDRESS"; *line2 = "TYPE IT LIKE 192.168.1.20"; break;
    case P2P_FAIL_PORT_IN_USE:
        *line1 = "THAT UDP PORT IS ALREADY IN USE"; *line2 = "CHOOSE ANOTHER ONE"; break;
    case P2P_FAIL_SOCKET:
        *line1 = "THIS MACHINE WOULD NOT OPEN A SOCKET"; *line2 = ""; break;
    case P2P_FAIL_NO_ANSWER:
        *line1 = "THE HOST DID NOT ANSWER"; *line2 = "CHECK THE ADDRESS, PORT AND FIREWALL"; break;
    case P2P_FAIL_LOST:
        *line1 = "THE CONNECTION WENT QUIET"; *line2 = ""; break;
    case P2P_FAIL_PEER_LEFT:
        *line1 = "THE OTHER PLAYER LEFT"; *line2 = ""; break;
    case P2P_FAIL_HOST_BUSY:
        *line1 = "THE HOST ALREADY HAS A PLAYER"; *line2 = ""; break;
    case P2P_FAIL_PROTOCOL:
    case P2P_FAIL_STATE:
        *line1 = "THE OTHER PLAYER HAS ANOTHER VERSION"; *line2 = "BOTH OF YOU NEED THE SAME BUILD"; break;
    case P2P_FAIL_GAMEPLAY:
        *line1 = "YOUR GAMEPLAY STYLES ARE DIFFERENT"; *line2 = "SET THE SAME ONE IN OPTIONS"; break;
    case P2P_FAIL_WIDTH:
        *line1 = "YOUR SCREEN WIDTHS ARE DIFFERENT"; *line2 = "BOTH OF YOU NEED THE SAME ONE"; break;
    default:
        *line1 = ""; *line2 = ""; break;
    }
}

static void set_status(const char *line1, const char *line2)
{
    snprintf(s_status1, sizeof s_status1, "%s", line1);
    snprintf(s_status2, sizeof s_status2, "%s", line2);
}

static void update_status(P2PLinkState state)
{
    const char *line1, *line2;

    switch (state) {
    case P2P_IDLE:
        if (s_notice1)
            set_status(s_notice1, s_notice2 ? s_notice2 : "");
        else if (s_hosting)
            set_status("YOUR FRIEND NEEDS YOUR IP ADDRESS",
                       "AND THIS PORT, OPEN ON YOUR ROUTER");
        else
            set_status("TYPE THE HOST'S IP ADDRESS AND PORT", "THEN CHOOSE JOIN");
        break;

    case P2P_HOSTING:
        snprintf(s_status1, sizeof s_status1, "WAITING FOR A PLAYER ON PORT %u",
                 (unsigned)P2PLink_LocalPort());
        if (P2PLink_Notice() != P2P_FAIL_NONE) {
            failure_text(P2PLink_Notice(), &line1, &line2);
            snprintf(s_status2, sizeof s_status2, "%s", line1);
        } else {
            snprintf(s_status2, sizeof s_status2, "CHOOSE CANCEL TO STOP");
        }
        break;

    case P2P_JOINING:
        snprintf(s_status1, sizeof s_status1, "CONNECTING TO %s", P2PLink_PeerText());
        snprintf(s_status2, sizeof s_status2, "WAITING FOR THE HOST TO ANSWER");
        break;

    case P2P_CONNECTED:
        snprintf(s_status1, sizeof s_status1, "CONNECTED TO %s", P2PLink_PeerText());
        if (P2PLink_PingMs() >= 0)
            snprintf(s_status2, sizeof s_status2, "PING %d MS. PLAYING A MATCH COMES NEXT",
                     P2PLink_PingMs());
        else
            snprintf(s_status2, sizeof s_status2, "MEASURING THE CONNECTION");
        break;

    case P2P_FAILED:
        failure_text(P2PLink_Failure(), &line1, &line2);
        set_status(line1, line2);
        break;
    }
}

/* ---- handlers ---- */

static int32_t link_is_open(P2PLinkState state)
{
    return state == P2P_HOSTING || state == P2P_JOINING || state == P2P_CONNECTED;
}

static void p2pMenu_OnInit(void)
{
    /* A link left open by the last visit -- the menu was left while hosting, say. */
    P2PLink_Close();
    notice(NULL, NULL);
    set_status("", "");
}

/* Every frame (the menu is flagged dos_redraw), so the panel follows the link without
 * anything having to push it. */
static void p2pMenu_OnDraw(void)
{
    P2PLink_Poll();

    P2PLinkState state = P2PLink_State();
    int32_t      open  = link_is_open(state);

    set_row(P2PMenu_mode, s_hosting ? "HOST A GAME" : "JOIN A GAME", !open);
    set_row(P2PMenu_address,
            s_hosting ? "NOT NEEDED WHEN HOSTING" : (s_address[0] ? s_address : "TYPE IT HERE"),
            !open && !s_hosting);
    set_row(P2PMenu_port, s_port, !open);
    set_row(P2PMenu_delay, delay_text(), !open);

    set_text(P2PMenu_start, !open ? (s_hosting ? "HOST" : "JOIN")
                                  : (state == P2P_CONNECTED ? "DISCONNECT" : "CANCEL"));

    update_status(state);
    set_text(P2PMenu_status1, s_status1);
    set_text(P2PMenu_status2, s_status2);
}

static void p2pMenu_mode_OnSelect(void)
{
    if (link_is_open(P2PLink_State()))
        return;
    s_hosting = !s_hosting;
    notice(NULL, NULL);
}

static void p2pMenu_address_OnSelect(void)
{
    MenuEntry *e = entry_at(P2PMenu_address);

    if (s_hosting || link_is_open(P2PLink_State()))
        return;

    notice(NULL, NULL);
    e->fg = (uintptr_t)s_address;       /* edit the address itself, not the prompt */
    InputText(sizeof s_address, (uint8_t *)e);
    if (s_address[0] && !P2PLink_IsAddress(s_address))
        notice("THAT IS NOT AN IP ADDRESS", "TYPE IT LIKE 192.168.1.20");
}

static void p2pMenu_port_OnSelect(void)
{
    char previous[sizeof s_port];

    if (link_is_open(P2PLink_State()))
        return;

    notice(NULL, NULL);
    memcpy(previous, s_port, sizeof previous);
    InputText(sizeof s_port, (uint8_t *)entry_at(P2PMenu_port));
    if (!parse_port(s_port)) {
        memcpy(s_port, previous, sizeof s_port);
        notice("THE PORT IS A NUMBER FROM 1024 TO 65535", "");
    }
}

static void p2pMenu_delay_OnSelect(void)
{
    if (link_is_open(P2PLink_State()))
        return;
    s_delay = (s_delay + 1) % (kMaxInputDelay + 1);
}

static void p2pMenu_start_OnSelect(void)
{
    uint16_t port;

    if (link_is_open(P2PLink_State())) {
        P2PLink_Close();
        notice(NULL, NULL);
        return;
    }

    notice(NULL, NULL);
    port = parse_port(s_port);
    if (!port) {
        notice("THE PORT IS A NUMBER FROM 1024 TO 65535", "");
        return;
    }

    if (s_hosting)
        P2PLink_Host(port);
    else
        P2PLink_Join(s_address, port);
    /* Either way the link now says what happened, and OnDraw shows it. */
}

static void p2pMenu_exit_OnSelect(void)
{
    P2PLink_Close();
    SetExitMenuFlag();
}

/* ---- the menu ---- */

#define HANDLER(fn) { #fn, (void *)fn, SYM_FUNC }

static const MenuSymbol k_handlers[] = {
    HANDLER(p2pMenu_OnInit),
    HANDLER(p2pMenu_OnDraw),
    HANDLER(p2pMenu_mode_OnSelect),
    HANDLER(p2pMenu_address_OnSelect),
    HANDLER(p2pMenu_port_OnSelect),
    HANDLER(p2pMenu_delay_OnSelect),
    HANDLER(p2pMenu_start_OnSelect),
    HANDLER(p2pMenu_exit_OnSelect),
};

#undef HANDLER

void P2PMenu_Init(void)
{
    static int32_t registered;

    if (registered)
        return;
    registered = 1;
    MenuJson_RegisterSymbols(k_handlers, (int)(sizeof k_handlers / sizeof k_handlers[0]));
}

void P2PMenu_Show(void)
{
    uint8_t *menu;

    P2PMenu_Init();
    menu = MenuJson_Load(P2P_MENU_NAME);
    if (!menu) {
        fprintf(stderr, "[p2p-menu] cannot show %s\n", P2P_MENU_NAME);
        return;
    }
    ShowMenu(menu);
}

#endif /* SWOS_SDL_BUILD */
