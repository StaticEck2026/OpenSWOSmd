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

#endif /* ONLINE_MENUS_H */
