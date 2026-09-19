#pragma once
// OpenSWOS: the relay (not in upstream). include/relay_proto.h in the game tree
// has the protocol and the reasons for it; this is the server's half.
//
// A relayed match is two players who each send their game packets to this server,
// which forwards every datagram unchanged to the other one from the same port. It
// shares the matchmaker's UDP port through ENet's intercept hook, so it needs no
// port of its own, and it only ever forwards between the two addresses that
// registered with a session's token -- never for anybody else.
//
// Each direction of a session can run through a network emulator (--netem), the
// same one the game carries (include/netem_core.h): a real match between two real
// machines, with a misbehaving network in the middle that neither of them knows is
// there.
#include <enet/enet.h>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include "netem_core.h"
#include "relay_proto.h"
#include "Ledger.h"

struct RelayOptions
{
    bool enabled = false;       // --relay: OpenSWOS pairs play through the server
    std::string netem;          // --netem: "", a profile name, or "cycle"
    uint32_t netemSeed = 0;     // --netem-seed: 0 = from the clock
};

class Relay
{
public:
    // `ledger` gets one line per finished session (Ledger.h); it may be closed.
    Relay(ENetHost* host, const RelayOptions& options, Ledger& ledger);
    ~Relay();

    bool Enabled() const { return m_options.enabled; }

    // At START. Returns the session's token, or 0 when this match is not relayed.
    uint64_t CreateSession(const std::string& name1, const ENetAddress& seen1,
                           const std::string& name2, const ENetAddress& seen2);

    // Once per main-loop pass: send what the emulator has made due, and close
    // sessions nobody is using any more.
    void Update();

    // Installed as host->intercept. Returns 1 for a datagram that was ours.
    static int ENET_CALLBACK Intercept(ENetHost* host, ENetEvent* event);

private:
    struct Endpoint
    {
        ENetAddress addr{};
        bool registered = false;
        uint64_t packets = 0;   // forwarded FROM this player
        uint64_t bytes = 0;
    };
    struct Session
    {
        uint64_t token = 0;
        std::string names[2];
        ENetAddress seen[2]{};  // where the matchmaker saw each player
        Endpoint ep[2];
        NetemLink netem[2];     // [i] = the direction FROM player i+1
        std::string netemName;
        std::string netemDesc;
        uint32_t createdMs = 0;
        uint32_t lastMs = 0;    // last packet either way
        uint32_t linkedMs = 0;  // when both had registered
        uint64_t early = 0;     // arrived before the other side had registered
        std::string createdAt;  // wall clock, for the ledger
    };

    int OnDatagram(const ENetAddress& from, const uint8_t* data, size_t len);
    void OnControl(const ENetAddress& from, const RelayControl& c);
    void Forward(Session& s, int from, const uint8_t* data, size_t len);
    void SendRaw(const ENetAddress& to, const uint8_t* data, size_t len);
    void Close(Session& s, const char* why);
    std::string PickNetem();

    static uint64_t Key(const ENetAddress& a) { return (uint64_t(a.host) << 16) | a.port; }

    ENetHost* m_host;
    RelayOptions m_options;
    std::map<uint64_t, std::unique_ptr<Session>> m_sessions;        // by token
    std::map<uint64_t, std::pair<Session*, int>> m_byAddress;       // registered source -> session, player index
    uint64_t m_rng;
    uint32_t m_created = 0;
    uint32_t m_lastSweepMs = 0;
    Ledger& m_ledger;

    static Relay* s_instance;   // for Intercept, which ENet calls with only the host
};
