/*
 * swos_settings.h — read/write SWOS.ini for persistent display + audio
 * settings.
 *
 * Order of operations at startup:
 *   1. main() seeds PlatformInitOptions with hard-coded defaults.
 *   2. Settings_Load() reads SWOS.ini (if present) and overrides.
 *   3. CLI parsing overrides anything the user passed on the command line.
 *   4. Platform_Init() consumes the final options.
 *
 * On clean shutdown, Settings_Save() snapshots the *current* runtime
 * state (Platform_GetWindowScale / Platform_GetFullscreenMode handle
 * the runtime-changeable bits) plus the launch-time renderer / vsync /
 * audio choice, and writes SWOS.ini back next to the binary. Missing
 * file is fine — we'll create it.
 */

#ifndef SWOS_SETTINGS_H
#define SWOS_SETTINGS_H

#include "platform_sdl.h"

/* Parse `path` (typically "SWOS.ini") and apply each known key onto
 * `*opts`. Unknown keys / sections are silently ignored. Missing or
 * unreadable file is a no-op. */
void Settings_Load(PlatformInitOptions *opts, const char *path);

/* Write current settings to `path`. Reads runtime-mutable values
 * (window scale, fullscreen mode) via the Platform_Get* getters so
 * any +/- and Alt+Enter changes survive across launches. */
void Settings_Save(const PlatformInitOptions *opts, const char *path);

#endif /* SWOS_SETTINGS_H */
