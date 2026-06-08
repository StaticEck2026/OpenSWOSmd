#!/usr/bin/env bash
# Sync source/header files from SWOS_C_SDL (this repo) to the Amiga C
# port at C:\Projects\AI\SWOS\SWOS_C.
#
# Layout mapping:
#   SDL  src/foo.c → Amiga src/foo.c
#   SDL  src/foo.h → Amiga include/foo.h
#
# Files skipped (SDL-only by design — no Amiga equivalent needed):
#   (none — swos_platform.h is now sync'd; the Amiga branch is empty)
#
# Usage:
#   scripts/sync_to_amiga_port.sh             # interactive prompt + copy
#   scripts/sync_to_amiga_port.sh --dry-run   # show plan, copy nothing
#   scripts/sync_to_amiga_port.sh --apply     # copy without prompting
#   scripts/sync_to_amiga_port.sh --dest /alt/path  # override destination
#   scripts/sync_to_amiga_port.sh --saves     # ALSO copy bin/save/* save
#                                             # files into the Amiga UAE
#                                             # save dir (see --saves-dest)
#   scripts/sync_to_amiga_port.sh --saves-only  # copy ONLY saves, skip src
#   scripts/sync_to_amiga_port.sh --saves-dest /alt/save  # override save dest
#
# Save sync rationale: the SDL build writes saves to bin/save/*.CAR etc,
# but the Amiga (FS-UAE) build loads them from a virtual-HD path
# (uae/dh0/data/save/). They are NOT the same directory, so a save made
# on the SDL side must be copied across before the Amiga can load it —
# forgetting this means testing STALE save data. --saves automates that.
#
# Safety:
#   - Refuses to overwrite if Amiga port has uncommitted changes in
#     src/ or include/ (clobber protection). Override with --force.
#   - Always runs interactive confirm unless --apply or --force.

set -euo pipefail

# ---- Defaults ----
SDL_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
AMIGA_ROOT_DEFAULT="/mnt/c/Projects/AI/SWOS/SWOS_C"
AMIGA_ROOT="$AMIGA_ROOT_DEFAULT"
MODE="prompt"   # prompt | apply | dry-run
FORCE=0
SYNC_SAVES=0    # also copy bin/save/* into the Amiga UAE save dir
SAVES_ONLY=0    # copy ONLY saves; skip the src/include sync
SDL_SAVE_DIR="$SDL_ROOT/bin/save"
# Default Amiga save dest: the FS-UAE virtual-HD save path. Override with
# --saves-dest. Left relative to AMIGA_ROOT so --dest composes with it
# unless an absolute --saves-dest is given.
AMIGA_SAVE_DIR=""   # resolved after arg parse (depends on AMIGA_ROOT)

# ---- File lists ----
# .c files that exist on both sides (copied to dest src/)
SRC_FILES=(
    SWOS.c adverts.c animated_patterns.c asset_loader.c asset_sys.c
    audio.c audio_irq.c
    ball_physics.c bench_subs.c blitter.c
    career_eurocups.c career_init.c career_season.c career_status.c career_teams.c
    display.c diy_competition.c edit_tactics.c file_io.c
    game_hud.c game_init.c game_loop.c game_timers.c game_utils.c
    image_cmd.c
    goal_scored.c highlights_menu.c interrupts.c job_offers.c
    loader_intro.c match_sim.c menu_system.c menus.c
    pitch.c play_match_menu.c player_anim.c referee.c rnc.c
    color_sprites.c player_sprites.c sprites.c statistics.c text_render.c
    strings_data.c teams.c transfer_market.c update_players.c
    competition_data.c tactics_data.c fixture_data.c
    simulation_data.c player_data.c render_data.c palette_data.c
    general_data.c menu_data.c team_colors.c sprite_data.c
    mjson.c save_format_json.c save_pointer_lut.c swos_io.c
    resload.c
    swos_globals.c swos_globals_career_overlay.c swos_competition_slots.c
    career_finances.c career_season_record.c career_finance_tail.c
    career_status_flags.c career_manager_identity.c career_board_confidence.c
    career_trophy_flags.c career_selector_state.c career_setup.c
    career_counters.c career_settings.c
    career_unavailable_players.c career_index_tables.c career_match_state.c
    swos_amiga_data_imports.c swos_amiga_ptr_imports.c
    swos_data_bulk_imports.c swos_data_typed_stubs.c
    swos_amiga_data_stubs.c swos_amiga_strings.c
    swos_amiga_func_stubs.c platform_amiga_stubs.c
    ai.c ball_possession.c goalkeeper.c headers.c match_setup.c
    player_positioning.c tackle_foul.c training_match.c frontend.c
    fade.c input.c input_text.c loader_bootstrap.c math68k.c panic.c
    random.c string_utils.c team_control.c tactics_menu.c
    game_state.c camera.c move_camera.c scroll_blit.c highlight.c
    intro_menu_image_script_cmds.inc
    menu_bg_image_script_cmds.inc
    menu_color_text_script_cmds.inc
)

# .h files (copied to dest include/)
HDR_FILES=(
    swos_types.h
    diy_competition.h menu_system.h teams.h game_types.h
    mjson.h save_format_json.h
    rnc_magic.h endian_helpers.h auto_skip_debug.h
    swos_io.h audio.h input.h swos_platform.h swos_sections.h
    competition_data.h
)

# ---- Arg parsing ----
while [[ $# -gt 0 ]]; do
    case "$1" in
        --dry-run) MODE="dry-run"; shift ;;
        --apply)   MODE="apply";   shift ;;
        --force)   FORCE=1;        shift ;;
        --dest)    AMIGA_ROOT="$2"; shift 2 ;;
        --saves)        SYNC_SAVES=1; shift ;;
        --saves-only)   SYNC_SAVES=1; SAVES_ONLY=1; shift ;;
        --saves-dest)   AMIGA_SAVE_DIR="$2"; SYNC_SAVES=1; shift 2 ;;
        -h|--help)
            sed -n '2,/^$/p' "$0" | sed 's/^# \?//'
            exit 0
            ;;
        *) echo "unknown arg: $1" >&2; exit 2 ;;
    esac
done

# Resolve the Amiga save dest now that AMIGA_ROOT is final. Default to the
# FS-UAE virtual-HD save path under the Amiga root; an explicit
# --saves-dest (absolute or relative) overrides it verbatim.
if [[ -z "$AMIGA_SAVE_DIR" ]]; then
    AMIGA_SAVE_DIR="$AMIGA_ROOT/uae/dh0/data/save"
fi

# ---- copy_saves: mirror bin/save/* into the Amiga UAE save dir ----
# Copies SWOS save files (.CAR/.SEA/.PRE/.DIY/.HIL). Reports per-file.
# Honours dry-run.
copy_saves() {
    if [[ ! -d "$SDL_SAVE_DIR" ]]; then
        echo "  (no SDL save dir $SDL_SAVE_DIR — nothing to copy)"; return 0
    fi
    if [[ ! -d "$AMIGA_SAVE_DIR" ]]; then
        echo "ERROR: Amiga save dir not found: $AMIGA_SAVE_DIR" >&2
        echo "       pass --saves-dest /path to override." >&2
        return 1
    fi
    shopt -s nullglob nocaseglob
    local files=( "$SDL_SAVE_DIR"/*.CAR "$SDL_SAVE_DIR"/*.SEA \
                  "$SDL_SAVE_DIR"/*.PRE "$SDL_SAVE_DIR"/*.DIY \
                  "$SDL_SAVE_DIR"/*.HIL )
    shopt -u nullglob nocaseglob
    echo "Save sync:"
    echo "  from: $SDL_SAVE_DIR"
    echo "  to:   $AMIGA_SAVE_DIR"
    if [[ ${#files[@]} -eq 0 ]]; then
        echo "  (no save files found)"; return 0
    fi
    local n=0
    for f in "${files[@]}"; do
        local b; b="$(basename "$f")"
        if [[ "$MODE" == "dry-run" ]]; then
            echo "    would copy $b"
        else
            cp "$f" "$AMIGA_SAVE_DIR/$b"
            echo "    copied $b"
            n=$((n + 1))
        fi
    done
    [[ "$MODE" != "dry-run" ]] && echo "  Copied $n save file(s)."
    return 0
}

# --saves-only: skip the entire src/include sync, just copy saves and exit.
if [[ "$SAVES_ONLY" -eq 1 ]]; then
    copy_saves
    exit $?
fi

# ---- Sanity checks ----
[[ -d "$SDL_ROOT/src" ]]            || { echo "missing $SDL_ROOT/src" >&2; exit 1; }
[[ -d "$AMIGA_ROOT/src" ]]          || { echo "missing $AMIGA_ROOT/src — set --dest" >&2; exit 1; }
[[ -d "$AMIGA_ROOT/include" ]]      || { echo "missing $AMIGA_ROOT/include — set --dest" >&2; exit 1; }

# Clobber protection: if Amiga port has uncommitted src/ or include/
# changes, refuse unless --force.
if [[ "$FORCE" -eq 0 && -d "$AMIGA_ROOT/.git" ]]; then
    dirty=$(cd "$AMIGA_ROOT" && git status --porcelain src/ include/ 2>/dev/null | head -1)
    if [[ -n "$dirty" ]]; then
        echo "ERROR: Amiga port has uncommitted changes in src/ or include/:" >&2
        (cd "$AMIGA_ROOT" && git status --short src/ include/) >&2
        echo "Commit/stash those first, or pass --force to overwrite anyway." >&2
        exit 1
    fi
fi

# ---- Classify files ----
new_src=()    # exists in SDL, missing in Amiga
mod_src=()    # exists in both
missing_src=()  # in list but missing in SDL (shouldn't happen)

for f in "${SRC_FILES[@]}"; do
    if [[ ! -f "$SDL_ROOT/src/$f" ]]; then
        missing_src+=("$f")
    elif [[ ! -f "$AMIGA_ROOT/src/$f" ]]; then
        new_src+=("$f")
    else
        mod_src+=("$f")
    fi
done

new_hdr=(); mod_hdr=(); missing_hdr=()
for f in "${HDR_FILES[@]}"; do
    if [[ ! -f "$SDL_ROOT/src/$f" ]]; then
        missing_hdr+=("$f")
    elif [[ ! -f "$AMIGA_ROOT/include/$f" ]]; then
        new_hdr+=("$f")
    else
        mod_hdr+=("$f")
    fi
done

# ---- Plan summary ----
echo "Sync plan:"
echo "  source: $SDL_ROOT"
echo "  dest:   $AMIGA_ROOT"
echo "  src/    ${#mod_src[@]} overwrite, ${#new_src[@]} new"
echo "  include/${#mod_hdr[@]} overwrite, ${#new_hdr[@]} new"
[[ ${#new_src[@]} -gt 0 ]] && printf '    NEW src: %s\n' "${new_src[@]}"
[[ ${#new_hdr[@]} -gt 0 ]] && printf '    NEW include: %s\n' "${new_hdr[@]}"
if [[ ${#missing_src[@]} -gt 0 || ${#missing_hdr[@]} -gt 0 ]]; then
    echo "  WARNING: files listed but missing from SDL side (skipped):"
    [[ ${#missing_src[@]} -gt 0 ]] && printf '    src/%s\n' "${missing_src[@]}"
    [[ ${#missing_hdr[@]} -gt 0 ]] && printf '    include/%s\n' "${missing_hdr[@]}"
fi
[[ "$SYNC_SAVES" -eq 1 ]] && echo "  saves:  bin/save/* -> $AMIGA_SAVE_DIR (--saves)"

if [[ "$MODE" == "dry-run" ]]; then
    [[ "$SYNC_SAVES" -eq 1 ]] && copy_saves
    echo "Dry-run: no files written. Pass --apply to copy."
    exit 0
fi

if [[ "$MODE" == "prompt" ]]; then
    read -rp "Proceed with copy? [y/N] " ans
    case "$ans" in [Yy]*) ;; *) echo "aborted"; exit 0 ;; esac
fi

# ---- Copy ----
copied=0
for f in "${mod_src[@]}" "${new_src[@]}"; do
    cp "$SDL_ROOT/src/$f" "$AMIGA_ROOT/src/$f"
    copied=$((copied + 1))
done
for f in "${mod_hdr[@]}" "${new_hdr[@]}"; do
    cp "$SDL_ROOT/src/$f" "$AMIGA_ROOT/include/$f"
    copied=$((copied + 1))
done

echo "Done. Copied $copied files."

# Save files (opt-in via --saves / --saves-dest).
if [[ "$SYNC_SAVES" -eq 1 ]]; then
    echo
    copy_saves
fi

echo
echo "Next: review on the Amiga side"
echo "  cd $AMIGA_ROOT"
echo "  git diff src/ include/"
