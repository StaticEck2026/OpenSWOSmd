/*
 * netplay_lobby.c -- where two connected players set their match up.
 * include/netplay_lobby.h has the model; docs/NETPLAY.md, "The lobby", the story.
 *
 * THREE MESSAGES, each on a p2p_link channel of its own (include/p2p_link.h):
 *
 *   STATE   each player's own side: team, tactic, ready, name. The host's also
 *           carries the match rules and their generation; the joiner's echoes the
 *           rules generation it has seen. Re-sent on every change; the link keeps
 *           only the newest.
 *   START   host -> joiner: the whole match setup (netplay_setup.c), built from
 *           both sides, plus WHICH of the joiner's STATE messages it was built from.
 *   ANSWER  joiner -> host: yes or no to that START.
 *
 * WHY START HAS AN ANSWER. A START is built from the joiner's side as the host last
 * saw it, and the joiner may have changed it since -- un-readied, picked another
 * team -- with the change still in flight. So the joiner accepts a START only if it
 * names the joiner's newest STATE, and says no otherwise; the host goes back to the
 * lobby and says why. Without the answer the host would kick off a match the joiner
 * never agreed to, and the two would desync on frame 0 at best.
 *
 * WHEN EACH SIDE KICKS OFF. The host when the ANSWER arrives; the joiner when the
 * link reports its ANSWER delivered. Both then close the link, and the joiner's BYE
 * cannot beat the ANSWER to the host, because delivery means the host already has
 * it. Messages stay readable after a link fails, so the lobby reads them before it
 * looks at the link state.
 *
 * READY GOES DOWN when what you agreed to changes: your own team or tactic, the
 * OTHER player's team or tactic, or (for the joiner) the host's rules. A match only
 * starts from two ready flags set after the last change either of them could see.
 *
 * THE USER TACTICS. Six slots, USER A-F, shared by both teams. Each player's USER
 * tactic travels with their STATE, and the match puts player 1's in USER A and
 * player 2's in USER B -- SWOS 2020's convention -- so two players who both use
 * their own "USER A" do not overwrite each other. Both machines keep a copy of
 * their six slots from before the match and put it back afterwards
 * (NetplayLobby_RestoreTactics): playing online never changes a player's library.
 */
#include "netplay_lobby.h"

#ifdef SWOS_SDL_BUILD

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "endian_helpers.h"
#include "netplay_match.h"
#include "netplay_setup.h"
#include "p2p_link.h"

#include <SDL.h>                        /* SDL_GetPerformanceCounter: key entropy fallback */
#include <time.h>

#ifdef SWOS_NETPLAY

extern TeamFileEx _selectedTeams[100];
extern uint16_t   _numSelectedTeams;
extern uint8_t    _teamTacticsUSERA[], _teamTacticsUSERB[], _teamTacticsUSERC[];
extern uint8_t    _teamTacticsUSERD[], _teamTacticsUSERE[], _teamTacticsUSERF[];
extern uint16_t   _gameLength, _minSubstitutes, _maxSubstitutes;
extern uint16_t   _pitchTypeOrSeason, _friendlyPitchType, _pitchType;
extern uint16_t   _gameType, _trainingGame, _isCareer;

enum { CH_STATE = 0, CH_START = 1, CH_ANSWER = 2 };

enum {
    TACTIC_BYTES   = 370,
    TEAM_PLAYER_CONTROLLED = 2,         /* TEAM_PLAYER_COACH (data/SWOS2_IDA.h TeamControls) */
    RANDOM_PITCH   = 0xFFFF,
};

#define STATE_MAGIC   0x53554C42u       /* "SULB" */
#define START_MAGIC   0x53555354u       /* "SUST" */
#define ANSWER_MAGIC  0x5355414Eu       /* "SUAN" */
#define LOBBY_VERSION 2     /* 2: START carries the match's packet key */

/* STATE: magic, version, player, ready, activity, hasTeam, tactic, rulesGen, the four
 * rules, the name, the team, the USER tactic. */
enum { STATE_BYTES = 4 + 2 + 4 + 2 + 2 + 8 + LOBBY_NAME_MAX + NETPLAY_TEAM_BYTES + TACTIC_BYTES };
/* START: magic, version, the joiner STATE generation it was built on, rulesGen, the
 * packet key, setup. */
enum { KEY_BYTES = 16, START_HEADER = 4 + 2 + 2 + 2 + KEY_BYTES };
/* ANSWER: magic, version, the START generation, accepted, pad. */
enum { ANSWER_BYTES = 4 + 2 + 2 + 1 + 1 };

static struct {
    LobbyState  state;
    int32_t     me;                     /* 1 host, 2 joiner */
    LobbyPlayer players[2];
    LobbyRules  rules;
    uint16_t    rulesGen;               /* host: bumped per change. joiner: the host's, as seen */
    uint16_t    peerRulesSeen;          /* host: what the joiner last echoed */
    int32_t     dirty;                  /* our STATE needs sending */

    uint16_t    peerStateGen;           /* the peer's STATE we last applied */
    uint16_t    startGen;               /* host: the START on offer */
    uint16_t    startHandled;           /* joiner: newest START answered */
    uint16_t    answerHandled;          /* host: newest ANSWER read */
    int32_t     accepted;               /* joiner: said yes to the START on offer */

    char        peerIp[24];
    uint16_t    peerPort, localPort;
    int32_t     delay;

    int32_t     tacticsSaved;
    uint8_t     savedTactics[6 * TACTIC_BYTES];

    uint8_t    *setup;                  /* the agreed match setup, both sides (ApplyMatch) */
    uint32_t    setupLen;
    uint8_t     packetKey[KEY_BYTES];   /* ...and the key its GGPO packets are checksummed with */

    char        notice[64];
} s_lobby;

static uint8_t *tactic_slot(int32_t i)
{
    switch (i) {
    case 0:  return _teamTacticsUSERA;
    case 1:  return _teamTacticsUSERB;
    case 2:  return _teamTacticsUSERC;
    case 3:  return _teamTacticsUSERD;
    case 4:  return _teamTacticsUSERE;
    default: return _teamTacticsUSERF;
    }
}

static LobbyPlayer *mine(void)   { return &s_lobby.players[s_lobby.me - 1]; }
static LobbyPlayer *theirs(void) { return &s_lobby.players[2 - s_lobby.me]; }

static void set_notice(const char *text)
{
    snprintf(s_lobby.notice, sizeof s_lobby.notice, "%s", text ? text : "");
}

static void clamp_rules(LobbyRules *r)
{
    if (r->gameLength > 3)
        r->gameLength = 0;
    if (r->pitchType != RANDOM_PITCH && r->pitchType > 6)
        r->pitchType = RANDOM_PITCH;
    if (r->maxSubs > 5)
        r->maxSubs = 5;
    if (r->maxSubs == 0)
        r->minSubs = 0;
    else if (r->minSubs < 1 || r->minSubs > r->maxSubs)
        r->minSubs = 1;
}

/* ---- the STATE message ---- */

static int32_t encode_state(uint8_t *out)
{
    LobbyPlayer *p = mine();
    uint8_t *q = out;

    /* The USER tactic is read out of its slot now, not when it was chosen: the slot
     * is the player's own and may have been edited since. */
    if (p->tactic >= TACTIC_USER_A)
        memcpy(&p->userTactic, tactic_slot(p->tactic - TACTIC_USER_A), TACTIC_BYTES);

    BE_WRITE_U32(q, STATE_MAGIC);                 q += 4;
    BE_WRITE_U16(q, LOBBY_VERSION);               q += 2;
    q[0] = (uint8_t)s_lobby.me;
    q[1] = (uint8_t)(p->ready ? 1 : 0);
    q[2] = (uint8_t)p->activity;
    q[3] = (uint8_t)(p->hasTeam ? 1 : 0);         q += 4;
    BE_WRITE_U16(q, (uint16_t)p->tactic);         q += 2;
    BE_WRITE_U16(q, s_lobby.rulesGen);            q += 2;
    BE_WRITE_U16(q, s_lobby.rules.gameLength);    q += 2;
    BE_WRITE_U16(q, s_lobby.rules.pitchType);     q += 2;
    BE_WRITE_U16(q, s_lobby.rules.minSubs);       q += 2;
    BE_WRITE_U16(q, s_lobby.rules.maxSubs);       q += 2;
    memset(q, 0, LOBBY_NAME_MAX);
    memcpy(q, p->name, strnlen(p->name, LOBBY_NAME_MAX)); q += LOBBY_NAME_MAX;
    NetplaySetup_WriteTeam(q, &p->team);          q += NETPLAY_TEAM_BYTES;
    memcpy(q, &p->userTactic, TACTIC_BYTES);      q += TACTIC_BYTES;
    return (int32_t)(q - out);
}

static void send_state(void)
{
    uint8_t buf[STATE_BYTES];
    int32_t n = encode_state(buf);
    if (P2PLink_Send(CH_STATE, buf, (uint32_t)n) == 0)
        s_lobby.dirty = 0;
}

/* The other player's side is what they will play; ours stops being "agreed" when it
 * changes in any way that matters to the match. */
static int32_t side_differs(const LobbyPlayer *a, const LobbyPlayer *b)
{
    if (a->hasTeam != b->hasTeam || a->tactic != b->tactic)
        return 1;
    if (a->hasTeam && memcmp(&a->team, &b->team, sizeof a->team) != 0)
        return 1;
    if (a->tactic >= TACTIC_USER_A && memcmp(&a->userTactic, &b->userTactic, TACTIC_BYTES) != 0)
        return 1;
    return 0;
}

static void drop_my_ready(void)
{
    if (mine()->ready) {
        mine()->ready = 0;
        s_lobby.dirty = 1;
    }
}

static void read_peer_state(void)
{
    uint32_t len = 0;
    uint16_t gen = 0;
    const uint8_t *m = P2PLink_Received(CH_STATE, &len, &gen);
    const uint8_t *q;
    LobbyPlayer got;
    LobbyRules rules;
    uint16_t rulesGen;

    if (!m || gen == s_lobby.peerStateGen)
        return;
    s_lobby.peerStateGen = gen;
    if (len < (uint32_t)STATE_BYTES || BE_READ_U32(m) != STATE_MAGIC ||
        BE_READ_U16(m + 4) != LOBBY_VERSION || m[6] != (uint8_t)(3 - s_lobby.me))
        return;                                 /* not the other player's, in our format */

    memset(&got, 0, sizeof got);
    q = m + 6;
    got.known    = 1;
    got.ready    = q[1] ? 1 : 0;
    got.activity = q[2];
    got.hasTeam  = q[3] ? 1 : 0;                  q += 4;
    got.tactic   = BE_READ_U16(q);                q += 2;
    rulesGen     = BE_READ_U16(q);                q += 2;
    rules.gameLength = BE_READ_U16(q);            q += 2;
    rules.pitchType  = BE_READ_U16(q);            q += 2;
    rules.minSubs    = BE_READ_U16(q);            q += 2;
    rules.maxSubs    = BE_READ_U16(q);            q += 2;
    memcpy(got.name, q, LOBBY_NAME_MAX);          q += LOBBY_NAME_MAX;
    got.name[LOBBY_NAME_MAX] = '\0';
    NetplaySetup_ReadTeam(&got.team, q);          q += NETPLAY_TEAM_BYTES;
    memcpy(&got.userTactic, q, TACTIC_BYTES);
    if (got.tactic < 0 || got.tactic > LOBBY_TACTIC_MAX)
        got.tactic = 0;

    if (theirs()->known && side_differs(theirs(), &got))
        drop_my_ready();
    *theirs() = got;

    if (s_lobby.me == 2) {
        /* The host's rules. A change after the first sighting means the joiner has
         * not agreed to these yet. */
        if (rulesGen != s_lobby.rulesGen) {
            if (s_lobby.rulesGen != 0)
                drop_my_ready();
            clamp_rules(&rules);
            s_lobby.rules    = rules;
            s_lobby.rulesGen = rulesGen;
            s_lobby.dirty    = 1;               /* echo the generation we now have */
        }
    } else {
        s_lobby.peerRulesSeen = rulesGen;
    }
}

/* ---- the USER tactics a match borrows ---- */

static void save_tactics(void)
{
    int32_t i;
    if (s_lobby.tacticsSaved)
        return;
    for (i = 0; i < 6; i++)
        memcpy(s_lobby.savedTactics + i * TACTIC_BYTES, tactic_slot(i), TACTIC_BYTES);
    s_lobby.tacticsSaved = 1;
}

void NetplayLobby_RestoreTactics(void)
{
    int32_t i;
    if (!s_lobby.tacticsSaved)
        return;
    for (i = 0; i < 6; i++)
        memcpy(tactic_slot(i), s_lobby.savedTactics + i * TACTIC_BYTES, TACTIC_BYTES);
    s_lobby.tacticsSaved = 0;
}

/* ---- START and ANSWER ---- */

/* The match's packet key (ggpo_set_packet_key): 16 bytes nobody off the path can guess,
 * so nobody off the path can make a packet the session accepts -- before this, one
 * forged packet with the disconnect bit set ended a match. It crosses the link in the
 * clear, so it is no defence against somebody who can see this traffic; but they can
 * drop packets anyway, which no checksum prevents. */
static void make_packet_key(uint8_t *key)
{
    FILE *f = fopen("/dev/urandom", "rb");      /* Linux, macOS, and Cygwin all have it */
    size_t got = f ? fread(key, 1, KEY_BYTES, f) : 0;
    if (f)
        fclose(f);
    if (got != KEY_BYTES) {
        /* No entropy source: still different on every START, just not secret. */
        uint64_t x = SDL_GetPerformanceCounter() ^ ((uint64_t)time(NULL) << 32) ^
                     (uint64_t)(uintptr_t)&x;
        for (int32_t i = 0; i < KEY_BYTES; i++) {
            x += 0x9E3779B97F4A7C15ull;
            uint64_t z = x;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
            key[i] = (uint8_t)((z ^ (z >> 31)) >> 24);
        }
    }
}

/* Host: put both sides and the rules where a match starts from. */
static void assemble_match(void)
{
    int32_t p;

    save_tactics();
    for (p = 0; p < 2; p++) {
        const LobbyPlayer *side = &s_lobby.players[p];
        TeamFileEx team = side->team;
        int32_t tactic = side->tactic;

        if (tactic >= TACTIC_USER_A) {
            memcpy(tactic_slot(p), &side->userTactic, TACTIC_BYTES);   /* A, or B */
            tactic = TACTIC_USER_A + p;
        }
        team.team.teamControls = TEAM_PLAYER_CONTROLLED;
        team.team.tactics      = (uint8_t)tactic;
        _selectedTeams[p] = team;
    }
    _numSelectedTeams = 2;

    _gameLength        = s_lobby.rules.gameLength;
    _pitchTypeOrSeason = 0;
    _friendlyPitchType = s_lobby.rules.pitchType;
    _minSubstitutes    = s_lobby.rules.minSubs;
    _maxSubstitutes    = s_lobby.rules.maxSubs;
    _gameType          = 0;                     /* a friendly */
    _trainingGame      = 0;
    _isCareer          = 0;
}

static void keep_setup(const uint8_t *setup, uint32_t len)
{
    free(s_lobby.setup);
    s_lobby.setup = (uint8_t *)malloc(len);
    s_lobby.setupLen = s_lobby.setup ? len : 0;
    if (s_lobby.setup)
        memcpy(s_lobby.setup, setup, len);
}

static void send_answer(uint16_t startGen, int32_t accepted)
{
    uint8_t buf[ANSWER_BYTES];
    BE_WRITE_U32(buf, ANSWER_MAGIC);
    BE_WRITE_U16(buf + 4, LOBBY_VERSION);
    BE_WRITE_U16(buf + 6, startGen);
    buf[8] = (uint8_t)(accepted ? 1 : 0);
    buf[9] = 0;
    (void)P2PLink_Send(CH_ANSWER, buf, sizeof buf);
}

/* Joiner. */
static void read_start(void)
{
    uint32_t len = 0;
    uint16_t gen = 0;
    const uint8_t *m = P2PLink_Received(CH_START, &len, &gen);
    int32_t ok;

    if (!m || gen == s_lobby.startHandled)
        return;
    s_lobby.startHandled = gen;
    if (s_lobby.state != LOBBY_OPEN)
        return;

    ok = len > (uint32_t)START_HEADER &&
         BE_READ_U32(m) == START_MAGIC && BE_READ_U16(m + 4) == LOBBY_VERSION &&
         /* built on our newest side, which we have not changed since */
         BE_READ_U16(m + 6) == P2PLink_SentGeneration(CH_STATE) && !s_lobby.dirty &&
         BE_READ_U16(m + 8) == s_lobby.rulesGen &&
         mine()->ready && mine()->hasTeam;

    if (ok) {
        save_tactics();
        if (NetplaySetup_Apply(m + START_HEADER, len - START_HEADER) != 0) {
            NetplayLobby_RestoreTactics();
            ok = 0;
        } else {
            keep_setup(m + START_HEADER, len - START_HEADER);
            memcpy(s_lobby.packetKey, m + 10, KEY_BYTES);
        }
    }
    send_answer(gen, ok);
    if (ok) {
        s_lobby.accepted = 1;
        s_lobby.state = LOBBY_STARTING;
        set_notice("STARTING");
    } else {
        set_notice("THE HOST STARTED BEFORE YOUR CHANGE ARRIVED");
    }
}

/* Host. */
static void read_answer(void)
{
    uint32_t len = 0;
    uint16_t gen = 0;
    const uint8_t *m = P2PLink_Received(CH_ANSWER, &len, &gen);

    if (!m || gen == s_lobby.answerHandled)
        return;
    s_lobby.answerHandled = gen;
    if (s_lobby.state != LOBBY_STARTING || len < (uint32_t)ANSWER_BYTES ||
        BE_READ_U32(m) != ANSWER_MAGIC || BE_READ_U16(m + 4) != LOBBY_VERSION ||
        BE_READ_U16(m + 6) != s_lobby.startGen)
        return;
    if (m[8]) {
        s_lobby.state = LOBBY_KICKOFF;
        set_notice("");
    } else {
        NetplayLobby_RestoreTactics();
        s_lobby.state = LOBBY_OPEN;
        set_notice("THE OTHER PLAYER CHANGED THEIR SIDE - TRY AGAIN");
    }
}

/* ---- the lobby ---- */

/* Where the match will be played: the connected peer, and our end of the link. */
static void take_peer_address(void)
{
    const char *peer = P2PLink_PeerText();
    char *colon;

    snprintf(s_lobby.peerIp, sizeof s_lobby.peerIp, "%s", peer);
    colon = strchr(s_lobby.peerIp, ':');
    if (colon) {
        *colon = '\0';
        s_lobby.peerPort = (uint16_t)atoi(colon + 1);
    }
    s_lobby.localPort = P2PLink_LocalPort();
    s_lobby.delay     = P2PLink_AgreedDelay();
    if (s_lobby.delay < 0)
        s_lobby.delay = 2;                      /* both sides fall back the same way */
}

void NetplayLobby_Begin(int32_t localPlayer, const char *localName)
{
    free(s_lobby.setup);
    memset(&s_lobby, 0, sizeof s_lobby);
    s_lobby.me    = localPlayer == 2 ? 2 : 1;
    s_lobby.state = LOBBY_OPEN;
    mine()->known = 1;
    snprintf(mine()->name, sizeof mine()->name, "%s",
             (localName && localName[0]) ? localName : (s_lobby.me == 1 ? "PLAYER 1" : "PLAYER 2"));

    /* Where the match will be played. Read now, while the link is certainly up: the
     * other side closes its link as soon as it kicks off, and the BYE that follows
     * clears the peer's address. */
    take_peer_address();

    if (s_lobby.me == 1) {
        /* What the friendly menu would start from (FriendlyMenu, menus.c): the game
         * length and a fixed pitch from OPTIONS, otherwise a random pitch, and two
         * substitutes from five. */
        s_lobby.rules.gameLength = _gameLength;
        s_lobby.rules.pitchType  = _pitchType <= 6 ? _pitchType : RANDOM_PITCH;
        s_lobby.rules.minSubs    = 2;
        s_lobby.rules.maxSubs    = 5;
        clamp_rules(&s_lobby.rules);
        s_lobby.rulesGen = 1;
    }
    s_lobby.dirty = 1;
}

void NetplayLobby_End(void)
{
    s_lobby.state = LOBBY_CLOSED;
}

int32_t NetplayLobby_Rematch(const char *notice)
{
    char addr[32];
    const char *list[1] = { addr };

    NetplayLobby_RestoreTactics();
    free(s_lobby.setup);
    s_lobby.setup    = NULL;
    s_lobby.setupLen = 0;

    /* Message generations start again at 1 with every connection (p2p_link.h), so the
     * ones this lobby has already handled would hide the new link's first messages. */
    s_lobby.peerStateGen  = 0;
    s_lobby.startGen      = 0;
    s_lobby.startHandled  = 0;
    s_lobby.answerHandled = 0;
    s_lobby.peerRulesSeen = 0;          /* the joiner echoes the rules again */
    s_lobby.accepted      = 0;

    /* Both sides stay; neither is ready for a match nobody has agreed to yet. */
    mine()->ready      = 0;
    mine()->activity   = LOBBY_ACTIVITY_NONE;
    theirs()->ready    = 0;
    theirs()->activity = LOBBY_ACTIVITY_NONE;
    s_lobby.dirty      = 1;
    set_notice(notice);

    if (!s_lobby.peerIp[0]) {
        s_lobby.state = LOBBY_FAILED;
        set_notice("THE OTHER PLAYER'S ADDRESS WAS LOST");
        return 1;
    }
    snprintf(addr, sizeof addr, "%s:%u", s_lobby.peerIp, (unsigned)s_lobby.peerPort);
    if (P2PLink_Punch(list, 1, s_lobby.localPort) != 0) {
        s_lobby.state = LOBBY_FAILED;
        set_notice("COULD NOT RECONNECT");
        return 1;
    }
    s_lobby.state = LOBBY_RECONNECTING;
    return 0;
}

void NetplayLobby_Poll(void)
{
    LobbyState st = s_lobby.state;

    if (st == LOBBY_CLOSED || st == LOBBY_FAILED || st == LOBBY_KICKOFF)
        return;
    P2PLink_Poll();

    if (st == LOBBY_RECONNECTING) {
        if (P2PLink_State() == P2P_FAILED) {
            s_lobby.state = LOBBY_FAILED;
            set_notice("YOUR OPPONENT DID NOT COME BACK");
            return;
        }
        /* The same wait mpWaitingMenu makes before a first lobby: linked, and the
         * input delay agreed between the two. */
        if (P2PLink_State() != P2P_CONNECTED || P2PLink_AgreedDelay() < 0)
            return;
        take_peer_address();                /* a NAT may have moved the peer's port */
        s_lobby.state = LOBBY_OPEN;
    }

    /* Messages before the link state: they stay readable after the link goes. */
    read_peer_state();
    if (s_lobby.me == 2)
        read_start();
    else
        read_answer();

    if (s_lobby.state == LOBBY_STARTING && s_lobby.me == 2 && s_lobby.accepted &&
        P2PLink_Delivered(CH_ANSWER))
        s_lobby.state = LOBBY_KICKOFF;
    if (s_lobby.state == LOBBY_KICKOFF)
        return;

    if (P2PLink_State() != P2P_CONNECTED) {
        P2PLinkFailure why = P2PLink_State() == P2P_FAILED ? P2PLink_Failure() : P2PLink_Notice();
        NetplayLobby_RestoreTactics();          /* a start that was under way borrowed them */
        s_lobby.state = LOBBY_FAILED;
        set_notice(why == P2P_FAIL_PEER_LEFT ? "THE OTHER PLAYER LEFT" :
                   why == P2P_FAIL_LOST      ? "LOST THE CONNECTION" :
                                               "THE CONNECTION FAILED");
        return;
    }

    if (s_lobby.state == LOBBY_OPEN && s_lobby.dirty)
        send_state();
}

LobbyState         NetplayLobby_State(void)       { return s_lobby.state; }
int32_t            NetplayLobby_LocalPlayer(void) { return s_lobby.me; }
int32_t            NetplayLobby_IsHost(void)      { return s_lobby.me == 1; }
const LobbyRules  *NetplayLobby_Rules(void)       { return &s_lobby.rules; }
const char        *NetplayLobby_Notice(void)      { return s_lobby.notice; }

const LobbyPlayer *NetplayLobby_Player(int32_t player)
{
    if (player != 1 && player != 2)
        return NULL;
    return &s_lobby.players[player - 1];
}

void NetplayLobby_SetTeam(const TeamFileEx *team)
{
    LobbyPlayer *p = mine();
    if (s_lobby.state != LOBBY_OPEN || !team)
        return;
    p->team    = *team;
    p->hasTeam = 1;
    /* The team's own formation, as the friendly menu would start it. A team record
     * naming a USER slot keeps it only when that slot is ours to send. */
    p->tactic  = team->team.tactics <= LOBBY_TACTIC_MAX ? team->team.tactics : 0;
    p->ready   = 0;
    s_lobby.dirty = 1;
    set_notice("");
}

void NetplayLobby_SetTactic(int32_t tactic)
{
    LobbyPlayer *p = mine();
    if (s_lobby.state != LOBBY_OPEN || tactic < 0 || tactic > LOBBY_TACTIC_MAX)
        return;
    p->tactic = tactic;
    p->ready  = 0;
    s_lobby.dirty = 1;
    set_notice("");
}

void NetplayLobby_SetReady(int32_t ready)
{
    LobbyPlayer *p = mine();
    if (s_lobby.state != LOBBY_OPEN || (ready && !p->hasTeam))
        return;
    if (p->ready != (ready ? 1 : 0)) {
        p->ready = ready ? 1 : 0;
        s_lobby.dirty = 1;
    }
    set_notice("");
}

void NetplayLobby_SetActivity(int32_t activity)
{
    if (s_lobby.state != LOBBY_OPEN || mine()->activity == activity)
        return;
    mine()->activity = activity;
    s_lobby.dirty = 1;
}

void NetplayLobby_SetRules(const LobbyRules *rules)
{
    LobbyRules r;
    if (s_lobby.state != LOBBY_OPEN || s_lobby.me != 1 || !rules)
        return;
    r = *rules;
    clamp_rules(&r);
    if (memcmp(&r, &s_lobby.rules, sizeof r) == 0)
        return;
    s_lobby.rules = r;
    s_lobby.rulesGen++;
    s_lobby.dirty = 1;
    set_notice("");
}

int32_t NetplayLobby_CanStart(void)
{
    const LobbyPlayer *host = &s_lobby.players[0], *joiner = &s_lobby.players[1];
    return s_lobby.me == 1 && s_lobby.state == LOBBY_OPEN &&
           P2PLink_State() == P2P_CONNECTED &&
           host->hasTeam && host->ready && joiner->known && joiner->hasTeam && joiner->ready &&
           s_lobby.peerRulesSeen == s_lobby.rulesGen &&
           /* the joiner has seen our side as it is now */
           !s_lobby.dirty && P2PLink_Delivered(CH_STATE);
}

int32_t NetplayLobby_Start(void)
{
    uint8_t *buf;
    int32_t n;

    if (!NetplayLobby_CanStart())
        return 1;
    buf = (uint8_t *)malloc(START_HEADER + NetplaySetup_MaxBytes());
    if (!buf)
        return 1;

    assemble_match();
    make_packet_key(s_lobby.packetKey);
    BE_WRITE_U32(buf, START_MAGIC);
    BE_WRITE_U16(buf + 4, LOBBY_VERSION);
    BE_WRITE_U16(buf + 6, s_lobby.peerStateGen);
    BE_WRITE_U16(buf + 8, s_lobby.rulesGen);
    memcpy(buf + 10, s_lobby.packetKey, KEY_BYTES);
    n = NetplaySetup_Encode(buf + START_HEADER, NetplaySetup_MaxBytes());
    if (n <= 0 || P2PLink_Send(CH_START, buf, (uint32_t)(START_HEADER + n)) != 0) {
        free(buf);
        NetplayLobby_RestoreTactics();
        return 1;
    }
    keep_setup(buf + START_HEADER, (uint32_t)n);
    free(buf);
    s_lobby.startGen = P2PLink_SentGeneration(CH_START);
    s_lobby.state = LOBBY_STARTING;
    set_notice("STARTING");
    return 0;
}

int32_t NetplayLobby_ApplyMatch(void)
{
    extern void InitTeams(void);
    int32_t rc;

    if (s_lobby.state != LOBBY_KICKOFF && s_lobby.state != LOBBY_CLOSED)
        return 1;
    if (!s_lobby.setup)
        return 1;
    rc = NetplaySetup_Apply(s_lobby.setup, s_lobby.setupLen);
    free(s_lobby.setup);
    s_lobby.setup = NULL;
    s_lobby.setupLen = 0;
    if (rc != 0)
        return rc;
    NetplayMatch_SetPacketKey(s_lobby.packetKey);
    InitTeams();            /* as the friendly menu does before PlayFriendly (ChooseTeamsMenu) */
    return 0;
}

void NetplayLobby_Peer(char *ip, uint32_t ipCap, uint16_t *peerPort, uint16_t *localPort,
                       int32_t *delay)
{
    if (ip && ipCap)
        snprintf(ip, ipCap, "%s", s_lobby.peerIp);
    if (peerPort)  *peerPort  = s_lobby.peerPort;
    if (localPort) *localPort = s_lobby.localPort;
    if (delay)     *delay     = s_lobby.delay;
}

#else  /* !SWOS_NETPLAY -- no link to set a match up over */

static LobbyRules  s_noRules;

void               NetplayLobby_Begin(int32_t localPlayer, const char *localName) { (void)localPlayer; (void)localName; }
void               NetplayLobby_Poll(void) {}
void               NetplayLobby_End(void) {}
int32_t            NetplayLobby_Rematch(const char *notice) { (void)notice; return 1; }
LobbyState         NetplayLobby_State(void) { return LOBBY_CLOSED; }
int32_t            NetplayLobby_LocalPlayer(void) { return 0; }
const LobbyPlayer *NetplayLobby_Player(int32_t player) { (void)player; return NULL; }
const LobbyRules  *NetplayLobby_Rules(void) { return &s_noRules; }
const char        *NetplayLobby_Notice(void) { return ""; }
void               NetplayLobby_SetTeam(const TeamFileEx *team) { (void)team; }
void               NetplayLobby_SetTactic(int32_t tactic) { (void)tactic; }
void               NetplayLobby_SetReady(int32_t ready) { (void)ready; }
void               NetplayLobby_SetActivity(int32_t activity) { (void)activity; }
void               NetplayLobby_SetRules(const LobbyRules *rules) { (void)rules; }
int32_t            NetplayLobby_IsHost(void) { return 0; }
int32_t            NetplayLobby_CanStart(void) { return 0; }
int32_t            NetplayLobby_Start(void) { return 1; }
void               NetplayLobby_Peer(char *ip, uint32_t ipCap, uint16_t *peerPort,
                                     uint16_t *localPort, int32_t *delay)
{
    if (ip && ipCap) ip[0] = '\0';
    if (peerPort) *peerPort = 0;
    if (localPort) *localPort = 0;
    if (delay) *delay = 0;
}
void               NetplayLobby_RestoreTactics(void) {}
int32_t            NetplayLobby_ApplyMatch(void) { return 1; }

#endif /* SWOS_NETPLAY */
#endif /* SWOS_SDL_BUILD */
