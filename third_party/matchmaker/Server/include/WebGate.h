// OpenSWOS: the WebSocket gate -- how a BROWSER plays (the web build).
//
// A browser has no UDP. Emscripten's socket layer carries each datagram the game
// sends as ONE WebSocket message to ws://<server>:<port> -- the same port number the
// datagram was for, over TCP -- and delivers each message back as one datagram. So
// this gate takes those connections and gives each browser socket a UDP socket of its
// own on this machine, talking to the server's own UDP port. Matchmaking, the relay
// and GGPO's packets through it then see an ordinary UDP client at 127.0.0.1:<port>,
// and nothing behind the gate knows a browser is there.
//
// One browser socket, one address. Emscripten opens a WebSocket per socket and, when
// the socket was bound, sends its port first ("\xFF\xFF\xFF\xFFport" + 2 bytes). The
// game's match link and then GGPO's socket are bound to the same port, one after the
// other, and the relay knows a player by the address its packets come from -- so a
// tunnel is keyed by (the browser's IP, that port) and outlives its WebSocket for a
// while, and the socket that follows on the same port gets the same address.
//
// TcpGate owns the TCP listener, because the recorder (Recorder.h) listens on the same
// port number: a connection that opens with "GET " is a WebSocket, anything else goes
// to the recorder, whose stream opens with its magic "SWRC".
#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

class Recorder;

class WebGate
{
public:
    WebGate(bool enabled, uint16_t serverPort);
    ~WebGate();

    bool Enabled() const { return m_enabled; }

    // A connection TcpGate has accepted, whose first bytes are "GET ".
    void Adopt(int fd, const std::string& ip, uint16_t port);

    // Once per main-loop pass: read and write every connection and tunnel.
    void Update();

private:
    struct Connection;
    struct Tunnel
    {
        int udp = -1;
        std::string key;                // "ip:port" of a bound browser socket, or unique
        uint16_t localPort = 0;         // the UDP port the server sees it on
        Connection* conn = nullptr;     // the WebSocket its replies go to, if any
        uint32_t idleSinceMs = 0;       // when it was last left without one
        uint64_t toServer = 0, toBrowser = 0;
    };
    struct Connection
    {
        int fd = -1;
        std::string ip, address;
        std::vector<uint8_t> in, out;
        bool upgraded = false;          // the HTTP handshake is done
        bool closing = false;           // flush `out`, then close
        bool first = true;              // no message yet: it may be the port message
        Tunnel* tunnel = nullptr;
        std::vector<uint8_t> message;   // a fragmented message being put together
        uint8_t messageOp = 0;
        uint32_t openedMs = 0;
        uint16_t browserPort = 0;
    };

    bool Handshake(Connection& c);                 // false = close it
    bool Frames(Connection& c);                    // false = close it
    void Deliver(Connection& c, const uint8_t* p, size_t len);
    void SendFrame(Connection& c, uint8_t op, const uint8_t* p, size_t len);
    bool Flush(Connection& c);                     // false = close it
    Tunnel* TunnelFor(const std::string& key);
    void Close(Connection& c, const char* why);
    void PumpTunnels();

    bool m_enabled;
    uint16_t m_serverPort;
    uint32_t m_nextUnique = 1;
    std::vector<std::unique_ptr<Connection>> m_connections;
    std::map<std::string, std::unique_ptr<Tunnel>> m_tunnels;
};

class TcpGate
{
public:
    // Listens only if there is someone to hand a connection to.
    TcpGate(uint16_t port, WebGate* web, Recorder* recorder);
    ~TcpGate();

    void Update();

private:
    struct Pending
    {
        int fd = -1;
        std::string ip;
        uint16_t port = 0;
        uint32_t sinceMs = 0;
    };
    int m_listen = -1;
    WebGate* m_web;
    Recorder* m_recorder;
    std::vector<Pending> m_pending;
};
