/*
 * netplay_live_test.c -- two processes play a LIVE match against each other.
 *
 * The end-to-end check for the chain the menus drive:
 *
 *      p2p_link connects  ->  the delay is agreed  ->  the lobby: each player picks
 *      a team and a tactic, the host sets the rules, both are ready, the host starts
 *      and the joiner confirms  ->  both open a GGPO session and play
 *
 * The lobby is driven through the same API the lobby menu uses
 * (include/netplay_lobby.h), so what is tested here is what a player does -- minus
 * the menu that turns key presses into those calls.
 *
 * It goes straight to p2p_link rather than through the matchmaking server,
 * because matchmaking -> START -> the peer's address is already covered by
 * scripts/matchmaker_pair_test.sh. What is new, and what this exercises, is
 * everything after that.
 *
 * NO PAD IS TOUCHED. Both sides play with nothing held, which is the point: the
 * two machines must still agree frame for frame, and any disagreement is the
 * session's or the setup's rather than an input the test invented. A match with
 * no input still runs the AI, the clock, the crowd and the RNG, so it is a real
 * simulation, not an idle loop.
 *
 *   bin/SWOS.x86_64 --netplay-live-pair=host
 *   bin/SWOS.x86_64 --netplay-live-pair=join
 *
 * scripts/netplay_live_test.sh runs both and compares what they report.
 */
#include "netplay_live_test.h"

#ifdef SWOS_SDL_BUILD

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

/* Every line, as it happens. stdout to a file or a pipe is block buffered, so a
 * run that is killed mid-way loses precisely the progress that would say where
 * it stopped -- which cost a debugging pass here. */
#define SAY(...) do { printf(__VA_ARGS__); fflush(stdout); } while (0)

#ifdef SWOS_NETPLAY

#include "p2p_link.h"
#include "netplay_lobby.h"
#include "netplay_match.h"
#include "sim_state.h"
#include "game_types.h"
#include "image_cmd.h"      /* ProcessImageCommandStream */
#include "match_sim.h"      /* InitTeams */
#include "teams.h"          /* LoadTeamFile */
#include "game_loop.h"      /* _gFrameStepInput */

extern TeamFileEx _selectedTeams[100];
extern uint8_t    _teamFileBuffer[];
extern uint8_t    _teamTacticsUSERB[], _teamTacticsUSERC[];
extern uint16_t   _gameLength, _friendlyPitchType, _minSubstitutes, _maxSubstitutes;
extern uint16_t   _numSelectedTeams;
extern void       InitAndPlayGame(TeamFileEx *team1, TeamFileEx *team2);
extern uint16_t   _menuBGImageCmdStream[];
extern uint8_t   *_menuImageStreamBaseEnd;

/* The port the two halves meet on. Fixed: both processes are started by the
 * same script and there is no lobby here to hand one out. Below 32768, so it is
 * outside every system's ephemeral range -- Linux's (32768-60999), Windows'
 * (49152-65535), and the block WSL2's mirrored networking reserves on Windows
 * (44620-48715 here), which is where the old 47901 was. */
#define LIVE_TEST_PORT 23901

/* ---- the synthetic player -------------------------------------------------
 *
 * Each peer plays its own made-up game: a stick direction and a fire button
 * that change every few frames, from a generator seeded DIFFERENTLY on each
 * side. That asymmetry is the whole point. With both peers sending nothing,
 * GGPO's prediction ("the same input as last frame") is always right, no
 * rollback ever happens, and a run over a lossy link proves only that nothing
 * crashed. Input the other side cannot guess is what turns latency into
 * mispredictions, and mispredictions into the rollbacks worth testing.
 *
 * It is NOT the game's generator. Rand() is match state that both peers must
 * agree on; driving the test's input from it would change the match being
 * tested and desync it. This one is private and its divergence is deliberate.
 *
 * It holds each input for a few frames rather than changing every frame: a
 * stick that flips every frame is not a player, and it would also make every
 * single prediction wrong, which is as unrealistic as none of them being. */
static uint32_t s_inputRng;

static uint8_t live_test_input(void)
{
    static uint8_t  held;
    static int32_t  framesLeft;

    if (framesLeft-- > 0)
        return held;

    {
        uint32_t x = s_inputRng;
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        s_inputRng = x ? x : 0x9E3779B9u;

        /* One of the eight directions, or nothing, plus fire about a fifth of
         * the time -- close enough to a player to exercise the same code. */
        held = 0;
        switch (x % 9u) {
        case 0: held = 0x01; break;                 /* up    */
        case 1: held = 0x02; break;                 /* down  */
        case 2: held = 0x04; break;                 /* left  */
        case 3: held = 0x08; break;                 /* right */
        case 4: held = (uint8_t)(0x01 | 0x04); break;
        case 5: held = (uint8_t)(0x01 | 0x08); break;
        case 6: held = (uint8_t)(0x02 | 0x04); break;
        case 7: held = (uint8_t)(0x02 | 0x08); break;
        default: break;                              /* nothing held */
        }
        if (((x >> 8) % 5u) == 0)
            held |= 0x20;                            /* fire */

        framesLeft = (int32_t)((x >> 16) % 12u) + 3; /* 3..14 frames */
    }
    return held;
}

static int32_t s_armed;
static int32_t s_isHost;

int32_t NetplayLiveTest_Arm(const char *role)
{
    if (!role)
        return 1;
    if (strcmp(role, "host") == 0)      s_isHost = 1;
    else if (strcmp(role, "join") == 0) s_isHost = 0;
    else                                return 1;
    s_armed = 1;
    return 0;
}

int32_t NetplayLiveTest_Armed(void) { return s_armed; }

/* Poll the link until `done`, or give up. Returns 1 if it happened. */
static int32_t wait_for(int32_t (*done)(void), uint32_t ms, const char *what)
{
    uint32_t start = SDL_GetTicks();
    while (SDL_GetTicks() - start < ms) {
        P2PLink_Poll();
        if (done())
            return 1;
        if (P2PLink_State() == P2P_FAILED) {
            SAY("  FAIL the link failed while waiting for %s\n", what);
            return 0;
        }
        SDL_Delay(2);
    }
    SAY("  FAIL timed out waiting for %s\n", what);
    return 0;
}

static int32_t link_up(void)     { return P2PLink_State() == P2P_CONNECTED; }
static int32_t delay_agreed(void){ return P2PLink_AgreedDelay() >= 0; }

/* The joiner's first START gets caught in a change of mind: the joiner touches its own
 * side the moment the START has arrived, before the lobby has read it -- exactly the
 * race of a player changing something while the host presses START -- so the lobby
 * has to refuse it and the host has to go back and try again. Reaching into the
 * link's channel 1 (the lobby's START) is test knowledge of netplay_lobby.c. */
static int32_t s_refusedOnce;
static int32_t s_readyDrops;        /* joiner: times the lobby took its ready flag down before START */

static void change_of_mind(void)
{
    uint32_t len = 0;
    uint16_t gen = 0;
    if (s_isHost || s_refusedOnce)
        return;
    P2PLink_Poll();
    if (P2PLink_Received(1, &len, &gen)) {
        NetplayLobby_SetTactic(TACTIC_USER_C);  /* the same tactic, re-chosen: ready drops */
        s_refusedOnce = 1;
        SAY("  ok   the joiner changes its mind as the start arrives\n");
    }
}

/* The lobby's own wait: NetplayLobby_Poll pumps the link, and a failed lobby ends it. */
static int32_t lobby_wait(int32_t (*done)(void), uint32_t ms, const char *what)
{
    uint32_t start = SDL_GetTicks();
    while (SDL_GetTicks() - start < ms) {
        change_of_mind();
        NetplayLobby_Poll();
        if (done())
            return 1;
        if (NetplayLobby_State() == LOBBY_FAILED) {
            SAY("  FAIL the lobby failed while waiting for %s: %s\n", what, NetplayLobby_Notice());
            return 0;
        }
        /* The joiner stands in for a player who confirms again whenever their ready
         * flag is taken down -- which is what the host's rule change below does.
         *
         * It is the JOINER that checks the flag went down, not the host: the joiner
         * says ready again at once, and when both of its STATE messages reach the host
         * in one poll the link rightly keeps only the newer, so the host may never see
         * the drop (Windows, measured). What protects the match is that a start needs
         * the joiner to have echoed the new rules generation (NetplayLobby_CanStart). */
        if (!s_isHost && NetplayLobby_State() == LOBBY_OPEN) {
            const LobbyPlayer *me = NetplayLobby_Player(2);
            if (me && me->hasTeam && !me->ready) {
                if (!s_refusedOnce && s_readyDrops++ == 0)  /* before the START: the rules */
                    SAY("  ok   the host's rule change took my ready down; ready again\n");
                NetplayLobby_SetReady(1);
            }
        }
        SDL_Delay(2);
    }
    SAY("  FAIL timed out waiting for %s\n", what);
    return 0;
}

static int32_t can_start(void)      { return NetplayLobby_CanStart(); }
static int32_t kicked_off(void)     { return NetplayLobby_State() == LOBBY_KICKOFF; }
static int32_t start_settled(void)  { return NetplayLobby_State() != LOBBY_STARTING; }
static int32_t peer_heard(void)     { return NetplayLobby_Player(3 - NetplayLobby_LocalPlayer())->known; }

/* A national team out of its team file, the way arcade mode loads one. */
static int32_t load_team(uint8_t file, uint8_t ordinal, TeamFileEx *out)
{
    uint16_t count, i;
    TeamFileEx *rec;

    if (LoadTeamFile(file) != 0)
        return 1;
    count = *(uint16_t *)_teamFileBuffer;       /* host-native count, then the records */
    rec = (TeamFileEx *)(_teamFileBuffer + 2);
    for (i = 0; i < count; i++) {
        if (rec[i].team.countryNumber == file && rec[i].team.teamOrdinal == ordinal) {
            *out = rec[i];
            return 0;
        }
    }
    return 1;
}

/* A tactic's content, reduced to one number both peers can print and compare. */
static uint32_t tactic_hash(const uint8_t *t)
{
    uint32_t h = 2166136261u;
    for (int32_t i = 0; i < 370; i++)
        h = (h ^ t[i]) * 16777619u;
    return h;
}

/* The session is over and every frame is confirmed: report and go. Reporting
 * after InitAndPlayGame returns would mean going through the post-match
 * screens, which wait for input nobody is giving here. */
static void live_test_finished(void)
{
    SAY("  ok   the session finished\n");
    SAY("[live] final state hash %016llx\n",
        (unsigned long long)SimState_Hash(SIM_TIER_ALL));

    /* After the hash: the match borrowed USER B for the joiner's tactic, and the
     * player's own library has to come back. */
    NetplayLobby_RestoreTactics();
    if (memcmp(((TeamTactics *)(void *)_teamTacticsUSERB)->name, "LIVETEST", 8) == 0) {
        SAY("  FAIL USER B still holds the match's tactic after it\n");
        exit(1);
    }
    SAY("  ok   the USER tactics are this player's own again\n");
    exit(NetplayMatch_Finish());
}

void NetplayLiveTest_Run(void)
{
    char ip[24];
    uint16_t peerPort = LIVE_TEST_PORT, localPort;
    int32_t delay, player, simWidth;

    SAY("[live] %s on port %d\n", s_isHost ? "host" : "join", LIVE_TEST_PORT);

    /* Punch, the same way a real match does -- both peers to all of the other's
     * candidates -- rather than host/join. Loopback cannot test a NAT, but it
     * does test the path the real thing takes, so this test and a real match do
     * not diverge. The two roles differ only in which fixed port each binds. */
    {
        char peerAddr[32];
        const char *list[1];
        uint16_t myPort   = s_isHost ? LIVE_TEST_PORT : (uint16_t)(LIVE_TEST_PORT + 1);
        uint16_t theirPort = s_isHost ? (uint16_t)(LIVE_TEST_PORT + 1) : LIVE_TEST_PORT;
        snprintf(peerAddr, sizeof peerAddr, "127.0.0.1:%u", (unsigned)theirPort);
        list[0] = peerAddr;
        if (P2PLink_Punch(list, 1, myPort) != 0) {
            SAY("  FAIL could not start punching: %d\n", (int)P2PLink_Failure());
            exit(1);
        }
    }

    if (!wait_for(link_up, 30000, "the link"))
        exit(1);
    SAY("  ok   linked to %s\n", P2PLink_PeerText());

    player = s_isHost ? 1 : 2;

    if (!wait_for(delay_agreed, 15000, "the agreed delay"))
        exit(1);
    delay = P2PLink_AgreedDelay();
    SAY("  ok   delay agreed: %d (mine %d, theirs %d)\n",
           delay, P2PLink_ProposedDelay(), P2PLink_PeerProposedDelay());

    /* ---- the lobby ----
     *
     * Each side picks a DIFFERENT team, so a peer that played its own choice on
     * both sides, or the two the wrong way round, cannot pass. The joiner plays a
     * USER tactic from slot C, which the match has to move to slot B on both
     * machines. The host changes the rules AFTER the joiner is ready, which has to
     * take the joiner's ready flag down before a start is allowed. */
    NetplayLobby_Begin(player, s_isHost ? "HOST PLAYER" : "JOIN PLAYER");
    {
        TeamFileEx team;
        uint8_t file = s_isHost ? 0x50 : 0x52, ordinal = s_isHost ? 0x0B : 0x02;   /* ENG, BRA */
        if (load_team(file, ordinal, &team) != 0) {
            SAY("  FAIL could not load team %02X.%02X\n", file, ordinal);
            exit(1);
        }
        NetplayLobby_SetTeam(&team);
        if (s_isHost) {
            NetplayLobby_SetTactic(TACTIC_4_3_3);
        } else {
            TeamTactics *c = (TeamTactics *)(void *)_teamTacticsUSERC;
            memset(c->name, 0, sizeof c->name);
            memcpy(c->name, "LIVETEST", 8);
            ((uint8_t *)c)[9 + 3 * 35 + 7] ^= 0x5A;     /* a position byte: not the stock 4-4-2 */
            NetplayLobby_SetTactic(TACTIC_USER_C);
        }
        NetplayLobby_SetReady(1);
        SAY("  ok   in the lobby with %.17s\n", (char *)team.team.teamName);
    }

    if (!lobby_wait(peer_heard, 15000, "the other player's side"))
        exit(1);

    if (s_isHost) {
        LobbyRules rules = *NetplayLobby_Rules();
        rules.gameLength = 1;
        rules.pitchType  = 4;                   /* NORMAL */
        rules.minSubs    = 2;
        rules.maxSubs    = 3;
        NetplayLobby_SetRules(&rules);
        if (!lobby_wait(can_start, 20000, "both sides ready"))
            exit(1);
        SAY("  ok   both ready\n");

        /* The shortest match the game offers: a full-length one is minutes of wall
         * clock, and this test is about agreement, not about playing 90 minutes. */
        rules.gameLength = 0;
        NetplayLobby_SetRules(&rules);
        if (NetplayLobby_CanStart()) {
            SAY("  FAIL a start is allowed before the joiner has seen the new rules\n");
            exit(1);
        }
        if (!lobby_wait(can_start, 20000, "the joiner to agree to the new rules"))
            exit(1);
        SAY("  ok   the joiner agrees to the changed rules\n");

        /* The first start is refused (change_of_mind); the second has to work. */
        for (int32_t attempt = 1; ; attempt++) {
            if (NetplayLobby_Start() != 0) {
                SAY("  FAIL the host could not start\n");
                exit(1);
            }
            SAY("  ok   the host starts (attempt %d)\n", attempt);
            if (!lobby_wait(start_settled, 20000, "an answer to the start"))
                exit(1);
            if (NetplayLobby_State() == LOBBY_KICKOFF)
                break;
            if (attempt >= 2) {
                SAY("  FAIL refused again: %s\n", NetplayLobby_Notice());
                exit(1);
            }
            SAY("  ok   refused, back in the lobby: %s\n", NetplayLobby_Notice());
            if (!lobby_wait(can_start, 20000, "both ready again"))
                exit(1);
        }
    }
    if (!lobby_wait(kicked_off, 30000, "kick-off"))
        exit(1);
    if (!s_isHost && s_readyDrops == 0) {
        SAY("  FAIL the host changed the rules and my ready flag never went down\n");
        exit(1);
    }
    SAY("  ok   both agree: kick-off\n");

    /* What both of them are about to play. The script requires these lines to be
     * IDENTICAL on the two peers; each peer checks its own side is the one it picked. */
    {
        const TeamTactics *b = (const TeamTactics *)(void *)_teamTacticsUSERB;
        const char *mineName   = (const char *)NetplayLobby_Player(player)->team.team.teamName;
        const char *placedName = (const char *)_selectedTeams[player - 1].team.teamName;
        SAY("[live] match: %.17s (tactic %u) v %.17s (tactic %u); USER B %.9s %08X; "
            "length %u, pitch %u, subs %u of %u\n",
            (char *)_selectedTeams[0].team.teamName, (unsigned)_selectedTeams[0].team.tactics,
            (char *)_selectedTeams[1].team.teamName, (unsigned)_selectedTeams[1].team.tactics,
            (char *)b->name, (unsigned)tactic_hash(_teamTacticsUSERB),
            (unsigned)_gameLength, (unsigned)_friendlyPitchType,
            (unsigned)_minSubstitutes, (unsigned)_maxSubstitutes);
        if (strncmp(mineName, placedName, 17) != 0) {
            SAY("  FAIL my team is %.17s but the match has %.17s on my side\n", mineName, placedName);
            exit(1);
        }
        SAY("  ok   my side of the match is the team I picked\n");
    }

    NetplayLobby_Peer(ip, sizeof ip, &peerPort, &localPort, &delay, &simWidth);
    if (NetplayLobby_ApplyMatch() != 0) {
        SAY("  FAIL the agreed setup would not apply at kick-off\n");
        exit(1);
    }
    NetplayLobby_End();

    /* GGPO takes the same local port, which is also what holds a NAT mapping
     * open across the handover. */
    P2PLink_Close();

    SAY("  ..   player %d, peer %s:%u, local port %u, delay %d, simulation width %d\n",
           player, ip, (unsigned)peerPort, (unsigned)localPort, delay, simWidth);

    if (NetplayMatch_StartLive(player, ip, peerPort, localPort, delay, simWidth) != 0) {
        SAY("  FAIL the session would not start\n");
        exit(1);
    }

    /* The front end was skipped to get here (this runs in its place, like the
     * replay rig), so the menu sprite graphics the stadium line-up draws were
     * never extracted from menus.raw and DrawMenuItem walks into a missing
     * header. Run just the sprite extraction -- not PrepareMenuBackground,
     * whose second step overwrites the frontbuffer. The replay rig and arcade
     * mode both do exactly this, for exactly this reason; without it both peers
     * segfault a moment after kick-off. */
    ProcessImageCommandStream(_menuBGImageCmdStream, _menuImageStreamBaseEnd);
    InitTeams();

    /* Dump the state the session is about to start from. If the two peers do
     * not agree here, they never will -- GGPO reports a desync at frame 0 and
     * says only "checksum 8F16 vs AC6A", which does not name anything. The
     * dumps are diffed by --netplay-state-diff, which walks the same registry
     * and names the entries. */
    {
        const char *dir = getenv("SWOS_LIVE_DUMP");
        if (dir && dir[0]) {
            char path[512];
            uint32_t n = SimState_Size(SIM_TIER_ALL);
            uint8_t *buf = (uint8_t *)malloc(n);
            snprintf(path, sizeof path, "%s/state-%s.bin", dir, s_isHost ? "host" : "join");
            if (buf) {
                FILE *f;
                SimState_Save(buf, SIM_TIER_ALL);
                f = fopen(path, "wb");
                if (f) { fwrite(buf, 1, n, f); fclose(f); SAY("  ok   state dumped to %s (%u bytes)\n", path, n); }
                free(buf);
            }
        }
    }

    NetplayMatch_SetFinishedCallback(live_test_finished);

    /* Drive the local half from the generator above, not from a pad nobody is
     * holding. Seeded per ROLE so the two peers genuinely differ. */
    s_inputRng = s_isHost ? 0xA5A5F00Du : 0x5A5A0FF0u;
    NetplayMatch_SetInputSource(live_test_input);

    _gFrameStepInput = 1;
    if (_numSelectedTeams < 2)
        _numSelectedTeams = 2;
    SAY("  ok   kick-off\n");
    fflush(stdout);

    InitAndPlayGame(&_selectedTeams[0], &_selectedTeams[1]);

    SAY("  ok   the match ended\n");
    SAY("[live] final state hash %016llx\n",
           (unsigned long long)SimState_Hash(SIM_TIER_ALL));
    fflush(stdout);
    exit(NetplayMatch_Finish());
}

/* --netplay-state-diff=A,B -- name the registry entries two kick-off dumps
 * disagree on. GGPO's desync report gives two 16-bit checksums and nothing
 * else; this says which part of the match state actually differs. */
int32_t NetplayLiveTest_StateDiff(const char *spec)
{
    char a[256], b[256];
    const char *comma;
    uint8_t *ba = NULL, *bb = NULL;
    uint32_t n = SimState_Size(SIM_TIER_ALL), offset = 0;
    int32_t i, differing = 0;
    FILE *f;

    if (!spec || !(comma = strchr(spec, ',')))
        return 1;
    snprintf(a, sizeof a, "%.*s", (int)(comma - spec), spec);
    snprintf(b, sizeof b, "%s", comma + 1);

    ba = (uint8_t *)malloc(n);
    bb = (uint8_t *)malloc(n);
    if (!ba || !bb) { free(ba); free(bb); return 1; }

    if (!(f = fopen(a, "rb")) || fread(ba, 1, n, f) != n) {
        printf("cannot read %s\n", a); if (f) fclose(f); free(ba); free(bb); return 1;
    }
    fclose(f);
    if (!(f = fopen(b, "rb")) || fread(bb, 1, n, f) != n) {
        printf("cannot read %s\n", b); if (f) fclose(f); free(ba); free(bb); return 1;
    }
    fclose(f);

    printf("[state-diff] %s vs %s (%u bytes, %d regions)\n", a, b, n, g_simStateRegionCount);
    for (i = 0; i < g_simStateRegionCount; i++) {
        const SimStateRegion *reg = &g_simStateRegions[i];
        if (!SimState_RegionActive(i, SIM_TIER_ALL))
            continue;
        if (memcmp(ba + offset, bb + offset, reg->size) != 0) {
            uint32_t k = 0;
            while (k < reg->size && ba[offset + k] == bb[offset + k])
                k++;
            if (differing < 40)
                printf("  %-40s size %6u  first differs at byte %u: %02X vs %02X\n",
                       reg->name, reg->size, k, ba[offset + k], bb[offset + k]);
            differing++;
        }
        offset += reg->size;
    }
    printf("[state-diff] %d region(s) differ\n", differing);
    free(ba); free(bb);
    return differing != 0;
}

#else  /* !SWOS_NETPLAY */

int32_t NetplayLiveTest_Arm(const char *role) { (void)role; return 1; }
int32_t NetplayLiveTest_Armed(void) { return 0; }
void    NetplayLiveTest_Run(void) {}
int32_t NetplayLiveTest_StateDiff(const char *spec) { (void)spec; return 1; }

#endif /* SWOS_NETPLAY */
#endif /* SWOS_SDL_BUILD */
