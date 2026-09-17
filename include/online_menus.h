/*
 * online_menus.h -- SWOS 2020's online-play menus in OpenSWOS (SDL builds).
 * See src/online_menus.c and docs/MENUS.md section 10.
 */
#ifndef ONLINE_MENUS_H
#define ONLINE_MENUS_H

#include "swos_types.h"

/* Register the menus' handlers with the JSON loader. Idempotent. */
void    OnlineMenus_Init(void);

/* Show a JSON-only menu by its loader name, e.g. "swos2020/logInMenu". */
void    OnlineMenus_Show(const char *name);

/* --menu=<name>: remember a menu for OnlineMenus_ShowStartMenu to open once, when
 * the front end first starts. */
void    OnlineMenus_SetStartMenu(const char *name);
void    OnlineMenus_ShowStartMenu(void);

/* --menu-selftest: load, unpack and check every imported menu, report how many
 * of their handlers are ported. 0 on success. */
int32_t OnlineMenus_SelfTest(void);

/* The matchmaking server has paired us with an opponent: leave the lobby and
 * connect to them directly (docs/NETPLAY.md section 7). Called from the
 * matchmaking menu's draw when MmClient_StartInfo() turns up. */
void        OnlineMenus_StartMatchFromMatchmaking(void);

/* The link is connected and the input delay agreed: open the lobby as `player` (1 the
 * host, 2 the joiner) and play the match set up there. Returns when it has been
 * played or someone left, with the link closed. Both ways in use it -- matchmaking
 * and the direct-IP menu. */
void        OnlineMenus_EnterLobby(int32_t player, const char *name);

/* --netplay-lobby=host|join: meet another instance on this machine and open the lobby
 * as the front end starts. Non-zero for an unknown role. */
int32_t     OnlineMenus_SetLoopbackLobby(const char *role);

/* --matchmaker-menu[=name]: log in to the matchmaking server as `name` and open
 * the matchmaking menu at startup, so the live lobby can be looked at without
 * navigating to it (and without gdb, which cannot hold the connection). */
void        OnlineMenus_SetAutoLogin(const char *name);
const char *OnlineMenus_MatchStatus(void);

#endif /* ONLINE_MENUS_H */
