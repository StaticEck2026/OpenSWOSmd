#!/usr/bin/env python3
"""The DOS recorded-frame layout, pinned in ONE readable place.

`rgd_convert.py --emit` reads a `.RGD` through these tables and writes a
`.swr` file of NAMED FIELDS (docs/TESTS.md section 5a). The C replay rig
(`src/replay_test.c`) never sees a DOS byte offset: it binds each name to
one of its own globals and compares value by value. That is what lets the
two trees' structs diverge (pointer growth, the untangle) without the test
data going stale — the same rule the save format follows.

Offsets below are into the DOS structs as swos-port declares them
(`src/swos/swos.h`, `src/sprites/Sprite.h`), all `#pragma pack(1)`,
little-endian. They coincide with OUR 32-bit offsets for every non-pointer
field, which is the cross-check that they are right: TeamGame/PlayerInfo/
TeamStatisticsData are byte-identical to ours, and Sprite/TeamGeneralInfo
match up to pointer width. Pointer fields are deliberately absent — a
pointer identity ("which sprite is this") is a later tier, and a raw
address is never comparable across the two builds.

Names are the snake_case of OUR `include/game_types.h` member names, since
the rig's registry is generated from our structs. A name that no longer
resolves is reported at load time as unbound rather than silently skipped.

TYPES follow OUR struct member's signedness, not swos-port's. Theirs types
most of these as `word` where ours are int16_t; the two hold the same bits,
and the rig compares bit patterns at the recorded width, so the choice only
affects how a mismatch PRINTS — "-1" reads better than "65535".

Field tuple:  (name, type, dos_offset[, cmp])
    type: u8 s8 u16 s16 u32 s32
    cmp:  "eq"   exact match (default)
          "bool" compare truthiness only — the field is a flag whose exact
                 non-zero value is an implementation detail
          "view" depends on the VIEW GEOMETRY the recording was made at, so
                 it is only comparable when ours matches (see below)
          "na"   the slot has no counterpart in this port at all, whatever the
                 geometry — never compared
          "spr"  a sprite-bank index, compared after subtracting the
                 recording's `sprite_index_offset` (the DOS bank has 114
                 entries our Amiga bank does not — DOS_TO_AMIGA_OFFSET in
                 include/dos_graphics.h, and exactly the delta this rig
                 measures on every one of the 33 sprites)
          "ptr:<kind>"  a POINTER field. The recording does not store an
                 address: the recorder writes a semantic identity, which
                 swos-port's own rig decodes the same way
                 (RecordedDataTest::verifySpritePointer /
                 verifyShotChanceTable). Kinds:
                   team          0 = topTeamData,   1 = bottomTeamData
                   teamgame      0 = topTeamInGame, 1 = bottomTeamInGame
                   stats         0 = team1StatsData,1 = team2StatsData
                   sprites_table 0 = team1SpritesTable, 1 = team2SpritesTable
                   sprite        -1 = null, else an index into the recording's
                                 own 33-sprite order (only 4..25, the players)
                   shotchance    -1 = null, 0 = the outfield table,
                                 else kGoalieSkillTables[value - 1]
          "in"   recorded input, injected rather than compared
          "meta" bookkeeping, neither injected nor compared

SWOS's camera is the view's top-left corner in world space, and every
constant around it is derived from the view size: the kick-off camera is
((pitchWidth - viewWidth) / 2, pitchBottom - viewHeight), MoveCamera centres
on `target - viewWidth/2`, and the pan range is `pitchWidth - viewWidth`.
DOS renders 320x200 and our Amiga-derived port renders 336x272, so a DOS
recording's camera is 176,664 where ours is 168,592 — not one constant
offset, since X is centred (half the difference) and Y is bottom-anchored
(the whole difference). Sprite coordinates are WORLD coordinates and are
unaffected; only the camera, its velocities, and the derived on-screen flag
are. Those are tagged "view" so the rig can skip exactly them, and only
them, when replaying a recording made at a different view size.
"""

# ---- levels (a run asserts every level up to the one selected) -----------
LEVELS = ["scalars", "teams", "info", "sprites"]

# The view a .RGD was recorded at. The DOS port renders 320x200; see the
# "view" cmp class in the module docstring.
DOS_VIEW_WIDTH = 320
DOS_VIEW_HEIGHT = 200

# DOS sprite indices are the Amiga index + 114 (include/dos_graphics.h
# DOS_TO_AMIGA_OFFSET). Confirmed independently: the replay rig sees exactly
# this delta on Sprite.imageIndex for all 33 sprites at kick-off.
DOS_SPRITE_INDEX_OFFSET = 114

# ---- DOS frame geometry (see docs/TESTS.md section 2) -------------------
OFF_TOP_INFO        = 8
OFF_TOP_PLAYERS     = 153
OFF_BOTTOM_INFO     = 164
OFF_BOTTOM_PLAYERS  = 309
OFF_TOP_GAME        = 320
OFF_BOTTOM_GAME     = 2024
OFF_TOP_STATS       = 3728
OFF_BOTTOM_STATS    = 3742
OFF_CAMERA          = 3756
OFF_SPRITES         = 3774
SPRITE_STRIDE       = 110
NUM_SPRITES         = 33
# Slot 31 of the recording is a corner flag, and WHICH corner flag is chosen by
# the camera: swos-port keeps FOUR of them and picks one with
#   if (cameraX >= 176) idx += cameraY >= 349 ? 3 : 1; else if (cameraY >= 349) idx += 2;
# (RecordedDataTest.cpp). Our port has the original's single _cornerFlagSprite,
# so there is no slot here that corresponds to it — not even when the geometry
# matches, because the recording is describing one of four objects we do not
# have. Every field carries cmp="na".
CORNER_FLAG_SPRITE  = 31
# Slot 32 is the "current player name" HUD sprite, parked off-pitch when hidden
# at a position derived from the view. swos-port's rig does not field-compare it
# either — it `continue`s past the slot outright and instead checks semantically
# that the right player's name is displayed (kCurrentPlayerNameSprite). cmp="na".
PLAYER_NAME_SPRITE  = 32
PLAYERINFO_STRIDE   = 61
NUM_PLAYERS         = 16

TYPE_SIZE = {"u8": 1, "s8": 1, "u16": 2, "s16": 2, "u32": 4, "s32": 4}


# ---- Sprite (110 bytes; ours is 110 on 32-bit, 118 on 64-bit) -----------
# Pointers omitted: animationTable@6, frameIndicesTable@18. Unlike the
# TeamGeneralInfo pointers these are recorded as raw VM offsets into
# swos-port's image, with no small identity to decode.
SPRITE_FIELDS = [
    ("team_number",           "s16",   0),
    ("player_ordinal",        "s16",   2),
    ("frame_offset",          "s16",   4),
    ("starting_direction",    "s16",  10),
    ("player_state",          "u8",   12),
    ("player_down_timer",     "s8",   13),
    ("field_e",               "s16",  14),
    ("field_10",              "s16",  16),
    ("frame_index",           "s16",  22),
    ("frame_delay",           "s16",  24),
    ("cycle_frames_timer",    "s16",  26),
    ("frame_switch_counter",  "s16",  28),
    ("x",                     "s32",  30),
    ("y",                     "s32",  34),
    ("z",                     "s32",  38),
    ("direction",             "s16",  42),
    ("speed",                 "s16",  44),
    ("delta_x",               "s32",  46),
    ("delta_y",               "s32",  50),
    ("delta_z",               "s32",  54),
    ("dest_x",                "s16",  58),
    ("dest_y",                "s16",  60),
    ("field_3e[0]",           "u8",   62),
    ("field_3e[1]",           "u8",   63),
    ("field_3e[2]",           "u8",   64),
    ("field_3e[3]",           "u8",   65),
    ("field_3e[4]",           "u8",   66),
    ("field_3e[5]",           "u8",   67),
    ("visible",               "s16",  68),
    ("image_index",           "s16",  70, "spr"),
    ("save_sprite",           "s16",  72),
    ("ball_distance",         "s32",  74),
    ("field_4e",              "s16",  78),
    ("field_50",              "s16",  80),
    ("full_direction",        "s16",  82),
    ("on_screen",             "s16",  84, "view"),
    ("field_56",              "s16",  86),
    ("field_58",              "s16",  88),
    ("field_5a",              "s16",  90),
    ("player_direction",      "s16",  92),
    # SWOS2.s:29122 writes this 16-bit field with a BYTE `sne` and reads it
    # back with cmp.b (31666), so "set" is 0x00FF little-endian / 0xFF00
    # big-endian; ours says -1, which is both bytes 0xFF. Truth test only.
    ("is_moving",             "s16",  94, "bool"),
    ("tackle_state",          "s16",  96),
    ("heading",               "s16",  98),
    ("dest_reached_state",    "s16", 100),
    ("cards",                 "s16", 102),
    ("injury_level",          "s16", 104),
    ("tackling_timer",        "s16", 106),
    ("sent_away",             "s16", 108),
]

# ---- TeamGeneralInfo (theirs 145 = our 144 + a trailing secondaryFire) ---
# The nine pointer fields ARE included, as identities rather than addresses —
# see "ptr:<kind>" in the module docstring. They are the only place the rig can
# see "we are controlling a different player", which no amount of scalar
# comparison reveals: every scalar can agree while the two ports drive
# different men.
TEAMINFO_FIELDS = [
    # --- pointers, compared as identities (see the module docstring) ---------
    ("opponents_team",                    "s32",   0, "ptr:team"),
    ("in_game_team_ptr",                  "s32",  10, "ptr:teamgame"),
    ("team_stats_ptr",                    "s32",  14, "ptr:stats"),
    ("sprites_table",                     "s32",  20, "ptr:sprites_table"),
    ("shot_chance_table",                 "s32",  24, "ptr:shotchance"),
    ("controlled_player",                 "s32",  32, "ptr:sprite"),
    ("pass_to_player_ptr",                "s32",  36, "ptr:sprite"),
    ("last_heading_tackling_player",      "s32",  72, "ptr:sprite"),
    ("passing_kicking_player",            "s32", 104, "ptr:sprite"),
    # --- scalars -------------------------------------------------------------
    ("player_number",                     "s16",   4),
    ("player_coach_number",               "s16",   6),
    ("is_pl_coach",                       "s16",   8),
    ("team_number",                       "s16",  18),
    ("tactics",                           "s16",  28),
    ("update_player_index",               "s16",  30),
    ("player_has_ball",                   "s16",  40),
    ("allowed_directions",                "s16",  42),
    ("current_allowed_direction",         "s16",  44),
    ("direction",                         "s16",  46),
    ("quick_fire",                        "s8",   48),
    ("normal_fire",                       "s8",   49),
    ("fire_pressed",                      "s8",   50),
    # The reference rig compares this one as a boolean; the exact non-zero
    # value differs between the ports and is never read as a number.
    ("fire_this_frame",                   "s8",   51, "bool"),
    ("header_or_tackle",                  "s16",  52),
    ("fire_counter",                      "s16",  54),
    ("controlled_pl_direction",           "s16",  56),
    ("shooting",                          "s16",  58),
    ("field_3c",                          "s8",   60),
    ("pl_very_close_to_ball",             "s8",   61),
    ("pl_close_to_ball",                  "s8",   62),
    ("pl_not_far_from_ball",              "s8",   63),
    ("ball_less_equal4",                  "s8",   64),
    ("ball4_to8",                         "s8",   65),
    ("ball8_to12",                        "s8",   66),
    ("ball12_to17",                       "s8",   67),
    ("ball_above17",                      "s8",   68),
    ("prev_pl_very_close_to_ball",        "s8",   69),
    ("field_46",                          "s16",  70),
    ("goalkeeper_saved_comment_timer",    "s16",  76),
    ("field_4e",                          "s16",  78),
    ("goalkeeper_diving_right",           "s16",  80),
    ("goalkeeper_diving_left",            "s16",  82),
    # SWOS2.s writes these two 16-bit fields with a BYTE `sne` (lines 29236-29243,
    # 25076/25147 read them back with tst.w), so "set" is 0x00FF on the
    # little-endian DOS build and 0xFF00 on big-endian m68k. Ours says -1, which
    # is the same flag with none of the byte trick. Every read is a truth test.
    ("ball_out_of_play_or_keeper",        "s16",  84, "bool"),
    ("goalie_playing_or_out",             "s16",  86, "bool"),
    ("passing_ball",                      "s16",  88),
    ("passing_to_player",                 "s16",  90),
    ("player_switch_timer",               "s16",  92),
    ("ball_in_play",                      "s16",  94),
    ("ball_out_of_play",                  "s16",  96),
    ("ball_x",                            "s16",  98),
    ("ball_y",                            "s16", 100),
    ("pass_kick_timer",                   "s16", 102),
    ("ball_control_turn_timer",           "s16", 108),
    ("ball_can_be_controlled",            "s16", 110),
    ("ball_controlling_player_direction", "s16", 112),
    ("field_72",                          "s16", 114),
    ("field_74",                          "s16", 116),
    ("spin_timer",                        "s16", 118),
    ("left_spin",                         "s16", 120),
    ("right_spin",                        "s16", 122),
    ("long_pass",                         "s16", 124),
    ("long_spin_pass",                    "s16", 126),
    ("pass_in_progress",                  "s16", 128),
    ("ai_timer",                          "s16", 130),
    ("field_84",                          "s16", 132),
    ("ai_after_touch_strength",           "s16", 134),
    ("ai_ball_spin_direction",            "s16", 136),
    ("won_the_ball_timer",                "s16", 138),
    ("goalkeeper_playing",                "s16", 140),
    ("reset_controls",                    "s16", 142),
]

# ---- PlayerInfo (61 bytes, embedded in TeamGame) ------------------------
# Name strings (short_name[15]@12, full_name[23]@38) are static for the
# whole match — they are asserted once at setup, not every frame.
PLAYERINFO_FIELDS = [
    ("skip",              "s8",   0),
    ("player_index",      "s8",   1),
    ("goals_scored",      "s8",   2),
    ("shirt_number",      "s8",   3),
    ("position",          "s8",   4),
    ("face",              "s8",   5),
    ("is_injured",        "s8",   6),
    ("field_7",           "s8",   7),
    ("field_8",           "s8",   8),
    ("previous_cards",    "s8",   9),
    ("num_yellow_cards",  "s8",  10),
    ("field_b",           "s8",  11),
    ("passing",           "s8",  27),
    ("shooting",          "s8",  28),
    ("heading",           "s8",  29),
    ("tackling",          "s8",  30),
    ("ball_control",      "s8",  31),
    ("speed",             "s8",  32),
    ("finishing",         "s8",  33),
    ("goalie_skill",      "s8",  34),
    ("injuries_bitfield", "s8",  35),
    ("half_played",       "s8",  36),
    ("face2",             "s8",  37),
]

# ---- TeamGame header (before players[] at +42) --------------------------
TEAMGAME_FIELDS = [
    ("pr_shirt_type",   "s16",  0),
    ("pr_shirt_col",    "s16",  2),
    ("pr_stripes_col",  "s16",  4),
    ("pr_shorts_col",   "s16",  6),
    ("pr_socks_col",    "s16",  8),
    ("sec_shirt_type",  "s16", 10),
    ("sec_shirt_col",   "s16", 12),
    ("sec_stripes_col", "s16", 14),
    ("sec_shorts_col",  "s16", 16),
    ("sec_socks_col",   "s16", 18),
    ("marked_player",   "s16", 20),
    ("num_own_goals",   "s8",  40),
]

TEAMSTATS_FIELDS = [
    ("ball_possession", "s16",  0),
    ("corners_won",     "s16",  2),
    ("fouls_conceded",  "s16",  4),
    ("bookings",        "s16",  6),
    ("sendings_off",    "s16",  8),
    ("goal_attempts",   "s16", 10),
    ("on_target",       "s16", 12),
]


def _f(name, ftype, off, level, cmp="eq"):
    return {"name": name, "type": ftype, "off": off, "level": level, "cmp": cmp}


def build_schema(major, minor, level):
    """Ordered list of frame fields for a recording of this version.

    `level` is one of LEVELS; every level at or below it is included, so
    "sprites" is the full compare and "scalars" is the cheap first pass.
    """
    want = LEVELS.index(level)
    out = []

    # -- level 0: scalars, input and bookkeeping --------------------------
    out.append(_f("frame_no",       "u32", 0, 0, "meta"))
    out.append(_f("p1_controls",    "s16", 4, 0, "in"))
    out.append(_f("p2_controls",    "s16", 6, 0, "in"))
    # The bench button, per TEAM. swos-port's TeamGeneralInfo is our 144 plus a
    # trailing `secondaryFire` byte (see TEAMINFO_FIELDS below) that it sets
    # from its own kGameEventBench, so the recording says which team's bench a
    # press belongs to instead of leaving the rig to infer it from who drives
    # whom. Injected, never compared — we have no such field. Its RISING edge
    # is on the same frame as the matching pN_controls bench bit in every press
    # in the corpus (the FALL lags a frame, which is why the rig uses the edge).
    out.append(_f("top_bench_button",    "u8", OFF_TOP_INFO + 144, 0, "in"))
    out.append(_f("bottom_bench_button", "u8", OFF_BOTTOM_INFO + 144, 0, "in"))
    out.append(_f("camera_x",       "s32", OFF_CAMERA + 0,  0, "view"))
    out.append(_f("camera_y",       "s32", OFF_CAMERA + 4,  0, "view"))
    out.append(_f("camera_x_vel",   "s8",  OFF_CAMERA + 8,  0, "view"))
    out.append(_f("camera_y_vel",   "s8",  OFF_CAMERA + 9,  0, "view"))
    for i in range(4):
        out.append(_f("game_time[%d]" % i, "u8", OFF_CAMERA + 10 + i, 0))
    out.append(_f("team1_goals",    "u8",  OFF_CAMERA + 14, 0))
    out.append(_f("team2_goals",    "u8",  OFF_CAMERA + 15, 0))
    out.append(_f("game_state",     "u8",  OFF_CAMERA + 16, 0))
    out.append(_f("game_state_pl",  "u8",  OFF_CAMERA + 17, 0))

    ver = (major, minor)
    if ver >= (1, 3) and want >= 1:
        # ---- BenchDataV1, the substitutes-menu state (71 bytes at 7404) ----
        # swos-port's own snapshot of the bench state machine. Wiring it in is
        # what makes the menu comparable at all: the two substitution bugs in
        # docs/TESTS.md were found by reading these bytes in Python first.
        # `int` is 32-bit and the struct is packed, hence the odd offsets.
        b = 7404
        for n, t, o, c in (
            ("pl1_tap_count",        "u8",  0,  "eq"),
            ("pl2_tap_count",        "u8",  1,  "eq"),
            ("pl1_prev_direction",   "s8",  2,  "eq"),
            ("pl2_prev_direction",   "s8",  3,  "eq"),
            ("pl1_block_hold",       "u8",  4,  "eq"),
            ("pl2_block_hold",       "u8",  5,  "eq"),
            # swos-port recomputes its own `m_controls` every frame; SWOS's
            # subsDirection is only written on frames where the sampled team
            # has a human, so ours is legitimately stale in between.
            ("controls",             "s8",  6,  "na"),
            # The two team pointers are recorded as small ids whose encoding we
            # have not pinned; the same information is in benchTeam anyway.
            ("team_data",            "s8",  7,  "na"),
            ("team_game",            "s8",  8,  "na"),
            ("state",                "u8",  9,  "eq"),
            ("go_to_bench_timer",    "s32", 10, "eq"),
            ("bench1_called",        "u8",  14, "eq"),
            ("bench2_called",        "u8",  15, "eq"),
            ("block_directions",     "u8",  16, "eq"),
            ("fire_timer",           "s32", 17, "eq"),
            ("block_fire",           "u8",  21, "eq"),
            # Same: m_lastDirection is swos-port's live copy, updated every
            # frame in its filterControls; SWOS writes subsPrevDirection only
            # while the menu is up (and starts it at 0, not -1).
            ("last_direction",       "s8",  22, "na"),
            ("movement_delay_timer", "s32", 23, "eq"),
            ("training_top_team",    "u8",  27, "eq"),
            ("teams_swapped",        "u8",  28, "eq"),
            # The DOS-only team-pick counter — the Amiga tests frameCount
            # instead, so this is only meaningful with DOS gameplay on. See
            # BenchCheckControls in src/bench_subs.c.
            ("alternate_teams_timer", "s32", 29, "eq"),
            # swos-port's m_arrowPlayerIndex is our _benchPlayerIndex: which
            # bench slot the menu arrow is on.
            ("arrow_player_index",   "s8",  33, "eq"),
            ("selected_menu_player", "s8",  34, "na"),
            ("player_to_enter",      "s8",  35, "eq"),
            ("player_to_sub_pos",    "s8",  36, "eq"),
            ("player_to_sub_ord",    "s8",  37, "eq"),
            ("selected_formation",   "s8",  38, "eq"),
        ):
            out.append(_f("bench.%s" % n, t, b + o, 1, c))
        # shirtNumberTable[2 * 16] — a display cache we do not keep.
        for i in range(32):
            out.append(_f("bench.shirt_numbers[%d]" % i, "u8", b + 39 + i, 1, "na"))

    if ver >= (1, 4):
        # after FrameV1p0 (7404) + BenchDataV1 (71)
        b = 7404 + 71
        out.append(_f("user_key",     "u8",  b + 0, 0, "in"))
        out.append(_f("fire_blocked", "u8",  b + 1, 0))
        # The reference compares statsTimer as a boolean: our tree ticks it
        # down on a different schedule but the gate is "is it running".
        out.append(_f("stats_timer",  "u16", b + 2, 0, "bool"))
        out.append(_f("result_timer", "u16", b + 4, 0))

    # -- level 1: the in-game team structures -----------------------------
    if want >= 1:
        for pre, base in (("top_game", OFF_TOP_GAME), ("bottom_game", OFF_BOTTOM_GAME)):
            for n, t, o in TEAMGAME_FIELDS:
                out.append(_f("%s.%s" % (pre, n), t, base + o, 1))
            for p in range(NUM_PLAYERS):
                pb = base + 42 + p * PLAYERINFO_STRIDE
                for n, t, o in PLAYERINFO_FIELDS:
                    out.append(_f("%s.players[%d].%s" % (pre, p, n), t, pb + o, 1))
        for pre, base in (("top_stats", OFF_TOP_STATS), ("bottom_stats", OFF_BOTTOM_STATS)):
            for n, t, o in TEAMSTATS_FIELDS:
                out.append(_f("%s.%s" % (pre, n), t, base + o, 1))

    # -- level 2: the per-team control/AI block ---------------------------
    if want >= 2:
        for pre, base in (("top_info", OFF_TOP_INFO), ("bottom_info", OFF_BOTTOM_INFO)):
            for fld in TEAMINFO_FIELDS:
                n, t, o = fld[0], fld[1], fld[2]
                c = fld[3] if len(fld) > 3 else "eq"
                out.append(_f("%s.%s" % (pre, n), t, base + o, 2, c))

    # -- level 3: every sprite --------------------------------------------
    if want >= 3:
        for s in range(NUM_SPRITES):
            base = OFF_SPRITES + s * SPRITE_STRIDE
            for fld in SPRITE_FIELDS:
                n, t, o = fld[0], fld[1], fld[2]
                c = fld[3] if len(fld) > 3 else "eq"
                if s in (CORNER_FLAG_SPRITE, PLAYER_NAME_SPRITE):
                    c = "na"
                out.append(_f("sprite[%d].%s" % (s, n), t, base + o, 3, c))

    return out
