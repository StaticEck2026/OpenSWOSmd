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

# ---- File lists ----
# .c files that exist on both sides (copied to dest src/)
SRC_FILES=(
    SWOS.c adverts.c animated_patterns.c asset_loader.c asset_sys.c
    audio.c audio_irq.c
    ball_physics.c bench_subs.c blitter.c
    career_init.c career_season.c career_status.c career_teams.c
    display.c diy_competition.c edit_tactics.c file_io.c
    game_hud.c game_init.c game_loop.c game_timers.c game_utils.c
    image_cmd.c
    goal_scored.c highlights_menu.c interrupts.c job_offers.c
    loader_intro.c match_sim.c menu_system.c menus.c
    pitch.c play_match_menu.c player_anim.c referee.c rnc.c
    sprites.c statistics.c text_render.c
    strings_data.c teams.c transfer_market.c update_players.c
    competition_data.c tactics_data.c fixture_data.c
    simulation_data.c player_data.c render_data.c palette_data.c
    general_data.c menu_data.c team_colors.c
    mjson.c save_format_json.c save_pointer_lut.c swos_io.c
    resload.c
    swos_globals.c swos_globals_career_overlay.c
    swos_amiga_data_imports.c swos_amiga_ptr_imports.c
    swos_data_bulk_imports.c swos_data_typed_stubs.c
    swos_amiga_data_stubs.c swos_amiga_strings.c
    swos_amiga_func_stubs.c platform_amiga_stubs.c
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
        -h|--help)
            sed -n '2,/^$/p' "$0" | sed 's/^# \?//'
            exit 0
            ;;
        *) echo "unknown arg: $1" >&2; exit 2 ;;
    esac
done

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

if [[ "$MODE" == "dry-run" ]]; then
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
echo
echo "Next: review on the Amiga side"
echo "  cd $AMIGA_ROOT"
echo "  git diff src/ include/"
