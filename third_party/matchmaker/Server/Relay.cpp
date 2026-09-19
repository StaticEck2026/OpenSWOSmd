// OpenSWOS: the relay. See include/Relay.h, and include/relay_proto.h in the
// game tree for the protocol.
#include "Relay.h"
#include "relay_proto.h"
#include "Utils.h"
#include <iostream>
#include <random>
#include <vector>

Relay* Relay::s_instance = nullptr;

namespace
{
    // An unregistered session is a match whose players never arrived; a linked
    // one that goes quiet is a match that is over. Rematches keep a session busy,
    // so one session carries every match two players play in a row.
    const uint32_t kUnlinkedTimeoutMs = 120000;
    const uint32_t kIdleTimeoutMs = 60000;

    std::string Tag()
    {
        return "[relay] " + ServerTime::TimeOfDay() + " ";
    }

    void EmitThroughRelay(void* ctx, const uint8_t* data, int32_t len,
                          const void* dst, int32_t dstLen, int32_t flags)
    {
        (void)dstLen; (void)flags;
        ENetAddress to;
        memcpy(&to, dst, sizeof to);
        ENetBuffer buffer;
        buffer.data = (void*)data;
        buffer.dataLength = (size_t)len;
        enet_socket_send(static_cast<ENetHost*>(ctx)->socket, &to, &buffer, 1);
    }
}

Relay::Relay(ENetHost* host, const RelayOptions& options, Ledger& ledger) :
    m_host(host),
    m_options(options),
    m_ledger(ledger)
{
    std::random_device rd;
    m_rng = (uint64_t(rd()) << 32) ^ rd() ^
            (uint64_t)std::chrono::high_resolution_clock::now().time_since_epoch().count();

    if (!m_options.enabled)
        return;
    s_instance = this;
    m_host->intercept = &Relay::Intercept;
    std::cout << "[relay] on: OpenSWOS pairs play through this server";
    if (!m_options.netem.empty())
        std::cout << ", network emulation " << m_options.netem;
    std::cout << "\n";
}

Relay::~Relay()
{
    while (!m_sessions.empty())
        Close(*m_sessions.begin()->second, "server stopping");
    if (s_instance == this)
    {
        m_host->intercept = nullptr;
        s_instance = nullptr;
    }
}

std::string Relay::PickNetem()
{
    if (m_options.netem != "cycle")
        return m_options.netem;
    // Every profile in turn, with a clean link first as the control.
    int32_t count = 0;
    const NetemProfile* all = NetemCore_Profiles(&count);
    uint32_t pick = m_created % (uint32_t)(count + 1);
    return pick == 0 ? std::string("none") : std::string(all[pick - 1].name);
}

uint64_t Relay::CreateSession(const std::string& name1, const ENetAddress& seen1,
                              const std::string& name2, const ENetAddress& seen2)
{
    if (!m_options.enabled)
        return 0;

    // xorshift64*, seeded from the device: a token is what stands between the
    // relay and anyone who would like it to forward for them.
    uint64_t token = 0;
    while (token == 0 || m_sessions.count(token))
    {
        m_rng ^= m_rng >> 12; m_rng ^= m_rng << 25; m_rng ^= m_rng >> 27;
        token = m_rng * 2685821657736338717ULL;
    }

    auto s = std::make_unique<Session>();
    s->token = token;
    s->names[0] = name1;
    s->names[1] = name2;
    s->seen[0] = seen1;
    s->seen[1] = seen2;
    s->createdMs = s->lastMs = ServerTime::NowMs();
    s->createdAt = ServerTime::WallClock();

    s->netemName = PickNetem();
    const NetemProfile* profile = NetemCore_FindProfile(s->netemName.c_str());
    NetemConfig cfg;
    NetemCore_ConfigFromProfile(&cfg, profile);
    uint32_t base = m_options.netemSeed ? m_options.netemSeed
                                        : (uint32_t)(m_rng >> 32) | 1u;
    for (int i = 0; i < 2; i++)
        NetemCore_Init(&s->netem[i], &cfg, base * 2654435761u + (uint32_t)token + (uint32_t)i * 0x9E3779B9u);
    char desc[320];
    NetemCore_Describe(&s->netem[0].cfg, profile ? profile->name : nullptr, desc, sizeof desc);
    s->netemDesc = desc[0] ? desc : "a clean link";
    m_created++;

    std::cout << Tag() << Hex64(token) << " " << name1 << " vs " << name2
              << ": session created, " << s->netemDesc << "\n";
    m_sessions[token] = std::move(s);
    return token;
}

int ENET_CALLBACK Relay::Intercept(ENetHost* host, ENetEvent* event)
{
    (void)event;
    if (!s_instance || host != s_instance->m_host)
        return 0;
    return s_instance->OnDatagram(host->receivedAddress, host->receivedData,
                                  host->receivedDataLength);
}

int Relay::OnDatagram(const ENetAddress& from, const uint8_t* data, size_t len)
{
    RelayControl c;
    if (len == RELAY_CONTROL_LEN && Relay_ReadControl(data, (int32_t)len, &c) == 0 &&
        c.type == RELAY_REGISTER)
    {
        OnControl(from, c);
        return 1;
    }
    auto it = m_byAddress.find(Key(from));
    if (it == m_byAddress.end())
        return 0;   // matchmaking traffic: ENet's
    Forward(*it->second.first, it->second.second, data, len);
    return 1;
}

void Relay::OnControl(const ENetAddress& from, const RelayControl& c)
{
    RelayControl reply{};
    reply.type = RELAY_REGISTERED;
    reply.player = c.player;
    reply.token = c.token;

    auto it = m_sessions.find(c.token);
    if (it == m_sessions.end())
        reply.status = RELAY_UNKNOWN;
    else if (c.player != 1 && c.player != 2)
        reply.status = RELAY_BAD_PLAYER;
    else
    {
        Session& s = *it->second;
        int i = c.player - 1;
        Endpoint& ep = s.ep[i];
        bool moved = ep.registered && ep.addr != from;
        if (!ep.registered || moved)
        {
            if (moved)
                m_byAddress.erase(Key(ep.addr));
            // An address can only be one player: whatever it was before -- a
            // previous session this client played in -- it is this one now.
            auto old = m_byAddress.find(Key(from));
            if (old != m_byAddress.end())
            {
                old->second.first->ep[old->second.second].registered = false;
                m_byAddress.erase(old);
            }
            ep.addr = from;
            ep.registered = true;
            m_byAddress[Key(from)] = { &s, i };
            std::cout << Tag() << Hex64(s.token) << " player " << (int)c.player
                      << " (" << s.names[i] << ") " << (moved ? "moved to " : "registered from ")
                      << ToReadableString(from) << "\n";
            if (s.ep[0].registered && s.ep[1].registered && !s.linkedMs)
            {
                s.linkedMs = ServerTime::NowMs();
                std::cout << Tag() << Hex64(s.token) << " both players here: forwarding\n";
            }
        }
        s.lastMs = ServerTime::NowMs();
        reply.status = RELAY_OK;
    }

    uint8_t buf[RELAY_CONTROL_LEN];
    Relay_WriteControl(buf, &reply);
    SendRaw(from, buf, sizeof buf);
}

void Relay::Forward(Session& s, int from, const uint8_t* data, size_t len)
{
    uint32_t now = ServerTime::NowMs();
    s.lastMs = now;
    const Endpoint& to = s.ep[1 - from];
    if (!to.registered)
    {
        // Only possible in the moment between the two registrations; the sender
        // is punching and will say it again in half a second.
        s.early++;
        return;
    }
    s.ep[from].packets++;
    s.ep[from].bytes += len;
    ENetAddress dst = to.addr;
    if (NetemCore_Send(&s.netem[from], now, data, (int32_t)len, &dst, (int32_t)sizeof dst, 0,
                       EmitThroughRelay, m_host) == 1)
        SendRaw(dst, data, len);
}

void Relay::SendRaw(const ENetAddress& to, const uint8_t* data, size_t len)
{
    ENetBuffer buffer;
    buffer.data = (void*)data;
    buffer.dataLength = len;
    enet_socket_send(m_host->socket, &to, &buffer, 1);
}

void Relay::Update()
{
    if (!m_options.enabled)
        return;
    uint32_t now = ServerTime::NowMs();
    for (auto& entry : m_sessions)
        for (int i = 0; i < 2; i++)
            NetemCore_Pump(&entry.second->netem[i], now, EmitThroughRelay, m_host);

    if (now - m_lastSweepMs < 1000)
        return;
    m_lastSweepMs = now;
    std::vector<Session*> done;
    for (auto& entry : m_sessions)
    {
        Session& s = *entry.second;
        if (!s.linkedMs && now - s.createdMs > kUnlinkedTimeoutMs)
            done.push_back(&s);
        else if (s.linkedMs && now - s.lastMs > kIdleTimeoutMs)
            done.push_back(&s);
    }
    for (Session* s : done)
        Close(*s, s->linkedMs ? "idle" : "the players never both arrived");
}

void Relay::Close(Session& s, const char* why)
{
    uint32_t playedMs = s.linkedMs ? s.lastMs - s.linkedMs : 0;
    char summary[2][256];
    for (int i = 0; i < 2; i++)
        NetemCore_Summary(&s.netem[i], summary[i], sizeof summary[i]);

    std::cout << Tag() << Hex64(s.token) << " closed (" << why << ") after "
              << playedMs / 1000 << " s linked: "
              << s.names[0] << " sent " << s.ep[0].packets << " packets / " << s.ep[0].bytes << " bytes, "
              << s.names[1] << " sent " << s.ep[1].packets << " packets / " << s.ep[1].bytes << " bytes";
    if (s.early)
        std::cout << ", " << s.early << " before the other side was here";
    std::cout << "\n";
    for (int i = 0; i < 2; i++)
        if (summary[i][0])
            std::cout << Tag() << "  netem " << s.names[i] << " -> " << s.names[1 - i]
                      << ": " << summary[i] << "\n";

    {
        std::string line = "{\"kind\":\"relay\",\"token\":\"" + Hex64(s.token) + "\""
            + ",\"created\":" + JsonString(s.createdAt)
            + ",\"closed\":" + JsonString(ServerTime::WallClock())
            + ",\"why\":" + JsonString(why)
            + ",\"players\":[" + JsonString(s.names[0]) + "," + JsonString(s.names[1]) + "]"
            + ",\"addresses\":[" + JsonString(s.ep[0].registered ? ToReadableString(s.ep[0].addr) : "")
            + "," + JsonString(s.ep[1].registered ? ToReadableString(s.ep[1].addr) : "") + "]"
            + ",\"linked\":" + (s.linkedMs ? "true" : "false")
            + ",\"linkedSeconds\":" + std::to_string(playedMs / 1000)
            + ",\"packets\":[" + std::to_string(s.ep[0].packets) + "," + std::to_string(s.ep[1].packets) + "]"
            + ",\"bytes\":[" + std::to_string(s.ep[0].bytes) + "," + std::to_string(s.ep[1].bytes) + "]"
            + ",\"early\":" + std::to_string(s.early)
            + ",\"netem\":" + JsonString(s.netemName.empty() ? "none" : s.netemName)
            + ",\"netemSummary\":[" + JsonString(summary[0]) + "," + JsonString(summary[1]) + "]"
            + "}";
        m_ledger.Write(line);
    }
    for (int i = 0; i < 2; i++)
    {
        if (s.ep[i].registered)
        {
            auto it = m_byAddress.find(Key(s.ep[i].addr));
            if (it != m_byAddress.end() && it->second.first == &s)
                m_byAddress.erase(it);
        }
        NetemCore_Free(&s.netem[i]);
    }
    m_sessions.erase(s.token);
}
