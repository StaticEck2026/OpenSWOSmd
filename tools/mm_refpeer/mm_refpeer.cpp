/*
 * mm_refpeer -- a matchmaking peer driven by the REFERENCE client library.
 *
 * Our client is src/mm_client.c: C, written from the protocol, and until now
 * only ever tested against itself (scripts/matchmaker_pair_test.sh runs two
 * copies of it). Two implementations of one protocol can agree with each other
 * about something the protocol does not say, and nothing notices.
 *
 * So this drives the OTHER implementation -- upstream's C++
 * ServerConnection (third_party/matchmaker/ServerConnection), the same code
 * SWOS 2020 ships -- through the same host/join sequence, and asserts the same
 * things our client asserts. scripts/matchmaker_interop_test.sh then pairs one
 * of each, both ways round, so every message our client sends is parsed by the
 * reference and vice versa.
 *
 * It is a TEST TOOL, not part of the game: it is built by the matchmaker's
 * CMakeLists, never by the SDL or Amiga makefiles, and nothing in src/ knows it
 * exists.
 *
 *   mm_refpeer --role host            log in as MMHOST, create a game, approve
 *                                     whoever asks, expect to be player 1
 *   mm_refpeer --role join            log in as MMJOIN, join MMHOST's game,
 *                                     expect to be player 2
 *
 * The names default to the ones our client's pair test uses, so it can stand in
 * for either side of it without our client being told anything.
 */
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

#include "ServerConnection.h"
#include "Utils.h"

namespace {

struct Options {
    std::string role   = "host";
    std::string name;                      /* defaults from the role */
    std::string peer;                      /* the other side's name */
    std::string host   = "127.0.0.1";
    int         port   = 19602;
    /* The reference client's own tag. Ours sends OPENSWOS, and the server
     * accepts both -- Connections::VerifyVersion is an empty TODO. Keeping the
     * reference value here is deliberate: it is what a SWOS 2020 client sends,
     * so the test says whether a differing tag matters today. */
    std::string gameId = "SWOS_MatchMaker";
    int         blob   = 0;                /* bytes of CREATE payload */
    int         timeoutMs = 60000;
    bool        verbose = false;
};

bool s_started, s_failed;
GameStartInfo s_start;

void usage(const char *argv0)
{
    std::printf(
        "usage: %s --role host|join [options]\n"
        "  --name NAME     log in as NAME (default MMHOST / MMJOIN)\n"
        "  --peer NAME     the other side (default MMJOIN / MMHOST)\n"
        "  --host IP       matchmaking server (default 127.0.0.1)\n"
        "  --port N        its port (default 19602)\n"
        "  --game-id TAG   the VERSION: tag (default SWOS_MatchMaker)\n"
        "  --blob N        send N bytes of open-game data with CREATE\n"
        "  --timeout MS    give up after MS (default 60000)\n"
        "  --verbose       every message the library logs\n", argv0);
}

} /* namespace */

int main(int argc, char **argv)
{
    Options o;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&](const char *what) -> std::string {
            if (i + 1 >= argc) { std::printf("mm_refpeer: %s needs a value\n", what); std::exit(2); }
            return argv[++i];
        };
        if      (a == "--role")    o.role = next("--role");
        else if (a == "--name")    o.name = next("--name");
        else if (a == "--peer")    o.peer = next("--peer");
        else if (a == "--host")    o.host = next("--host");
        else if (a == "--port")    o.port = std::atoi(next("--port").c_str());
        else if (a == "--game-id") o.gameId = next("--game-id");
        else if (a == "--blob")    o.blob = std::atoi(next("--blob").c_str());
        else if (a == "--timeout") o.timeoutMs = std::atoi(next("--timeout").c_str());
        else if (a == "--verbose") o.verbose = true;
        else if (a == "-h" || a == "--help") { usage(argv[0]); return 0; }
        else { std::printf("mm_refpeer: unknown option '%s'\n", a.c_str()); usage(argv[0]); return 2; }
    }
    const bool isHost = (o.role == "host");
    if (!isHost && o.role != "join") { usage(argv[0]); return 2; }
    if (o.name.empty()) o.name = isHost ? "MMHOST" : "MMJOIN";
    if (o.peer.empty()) o.peer = isHost ? "MMJOIN" : "MMHOST";

    /* Unbuffered: this runs under a test script that reads its output while it
     * is still alive, and a block-buffered stdout there is indistinguishable
     * from a peer that has hung. */
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    std::printf("[refpeer] pair test, role=%s, as %s (version tag %s)\n",
                o.role.c_str(), o.name.c_str(), o.gameId.c_str());

    if (enet_initialize() != 0) { std::printf("  FAIL enet_initialize\n"); return 1; }

    auto logger = [&](const std::string &s) { if (o.verbose) std::printf("  ..   %s", s.c_str()); };
    ServerConnection sc(logger);

    std::vector<char> userData;            /* the LOGIN: blob; ours sends none */
    if (!sc.Connect(o.host, o.port, o.name, userData.data(), userData.size(), o.gameId)) {
        /* Connect() refuses before it sends anything when the machine has no
         * non-loopback IPv4 -- it has no candidate address to report. */
        std::printf("  FAIL Connect() refused: no server, or no non-loopback IPv4 on this machine\n");
        return 1;
    }

    bool created = false, asked = false, approved = false;
    std::string joiner;

    ServerCallbacks cb;
    /* Every callback must be set: the library calls them unconditionally, and an
     * empty std::function throws rather than doing nothing. */
    cb.Connected = [&]() {
        std::printf("  ok   logged in\n");
        if (isHost && !created) {
            std::vector<char> blob((size_t)(o.blob > 0 ? o.blob : 0), 'x');
            sc.CreateGame(blob.data(), (uint8_t)blob.size());
            if (o.blob > 0)
                std::printf("  ..   CREATE carries %d bytes of open-game data\n", o.blob);
        }
    };
    cb.GameCreatedOK = [&]() {
        created = true;
        std::printf("  ok   hosting, waiting for a joiner...\n");
    };
    cb.OpenGames = [&](const std::vector<OpenGameInfo> &games) {
        if (isHost || asked)
            return;
        for (const auto &g : games) {
            if (g.name != o.peer)
                continue;
            std::printf("  ok   found %s's game; asking to join\n", o.peer.c_str());
            asked = true;
            sc.RequestToJoinGame(o.peer);
            return;
        }
    };
    cb.JoinRequestFromOtherPlayer = [&](const std::string &who) {
        if (approved)
            return;
        joiner = who;
        approved = true;
        std::printf("  ok   '%s' asked to join; approving\n", who.c_str());
        /* The library starts the game itself when the server echoes APPROVE. */
        sc.ApproveJoinRequest(who);
    };
    cb.JoinRequestOK = [&]() { std::printf("  ok   join request accepted, waiting for the host\n"); };
    cb.Approved      = [&](const std::string &who) { std::printf("  ok   approved: %s\n", who.c_str()); };
    cb.StartP2P      = [&](const GameStartInfo &info) { s_start = info; s_started = true; };
    cb.Timeout       = [&]() { std::printf("  FAIL timed out connecting to the server\n"); s_failed = true; };
    cb.Disconnected  = [&]() {
        /* The server drops a user it did not understand, so this is where a
         * protocol disagreement surfaces -- see the CREATE data blob. */
        if (!s_started) { std::printf("  FAIL the server disconnected us\n"); s_failed = true; }
    };
    cb.RemovedFromGame = [&]() { std::printf("  ..   removed from the game\n"); };
    cb.LeftGameOK      = [&]() { std::printf("  ..   left the game\n"); };
    cb.UserList        = [&](const std::vector<PlayerInfo> &) {};
    cb.GameInfo        = [&](const GameInfoStruct &) {};
    cb.ServerMessage   = [&](const std::string &m) { if (o.verbose) std::printf("  ..   server: %s\n", m.c_str()); };

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(o.timeoutMs);
    while (!s_started && !s_failed) {
        sc.Update(cb);                     /* enet_host_service blocks 1 ms inside */
        if (std::chrono::steady_clock::now() > deadline) {
            std::printf("  FAIL nothing happened within %d ms (no START)\n", o.timeoutMs);
            s_failed = true;
        }
    }

    int failed = s_failed ? 1 : 0;
    if (s_started) {
        std::printf("  ok   START: player %d, peer '%s', local port %u, %u address(es):",
                    s_start.playerNumber, s_start.peerName.c_str(),
                    (unsigned)s_start.port, (unsigned)s_start.peerAddresses.size());
        for (const auto &a : s_start.peerAddresses)
            std::printf(" %s", ToReadableString(a).c_str());
        std::printf("\n");

        /* The same three checks our client makes, so a crossed handoff fails
         * here too rather than in a match that desyncs. */
        const int wantPlayer = isHost ? 1 : 2;
        if (s_start.playerNumber != wantPlayer) {
            std::printf("  FAIL expected to be player %d, got %d\n", wantPlayer, s_start.playerNumber);
            failed = 1;
        }
        if (s_start.peerName != o.peer) {
            std::printf("  FAIL expected peer '%s', got '%s'\n", o.peer.c_str(), s_start.peerName.c_str());
            failed = 1;
        }
        if (s_start.peerAddresses.empty()) {
            std::printf("  FAIL no peer addresses -- nothing to connect to\n");
            failed = 1;
        }
    }

    std::printf("[refpeer] pair test (%s) %s\n", o.role.c_str(), failed ? "FAILED" : "PASSED");
    enet_deinitialize();
    return failed;
}
