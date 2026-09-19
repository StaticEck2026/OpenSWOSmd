#pragma once
// OpenSWOS: the recorder (not in upstream). include/record_proto.h in the game
// tree has the protocol and the reasons for it; this is the server's half.
//
// Players stream a live match here over TCP -- the matchmaker's port number, TCP
// instead of UDP -- whether the match is played through the relay or peer to peer,
// and whether this server is the one that introduced them or just one they were
// pointed at (SWOS_RECORD=host:port). Both players' streams are paired by match id
// and compared frame by frame as they arrive; the first frame whose SIM-tier hashes
// differ is a desync, reported the moment it is seen -- on the console, and as a
// "desync" line in the ledger -- with the two builds, platforms, teams and times.
//
// One thread, polled from the main loop like the relay: no locks, nothing blocks.
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "Ledger.h"
#include "srp_writer.h"

class Recorder
{
public:
    // Listens on TCP `port` when `enabled`; `ledger` gets the match and desync lines,
    // and a full recording's .SRP goes into `recordingsDir`.
    Recorder(bool enabled, uint16_t port, Ledger& ledger, const std::string& recordingsDir);
    ~Recorder();

    bool Listening() const { return m_listen >= 0; }

    // Once per main-loop pass: take new connections, read what arrived, compare,
    // and close matches that are over.
    void Update();

private:
    struct Side
    {
        bool present = false;       // its HELLO has arrived
        bool open = false;          // its connection is still up
        bool ended = false;         // its END has arrived
        std::string address;
        std::string build, platform, name, peer, team1, team2;
        int gameplay = 0, mode = 0, delay = 0, simWidth = 0;
        uint64_t relayToken = 0;
        std::vector<uint64_t> hashes;   // [frame]
        int outcome = -1;
        uint32_t endFrame = 0;
        uint64_t finalHash = 0;
        std::string why;

        // A FULL recording: the match itself, laid out as a .SRP by srp_writer.h --
        // the same code that wrote the player's own copy.
        bool replay = false;                    // its META has arrived
        uint16_t tickRate = 0, quirks = 0;
        std::vector<uint8_t> meta, frms;
        std::vector<uint32_t> offsets;
        std::vector<SrpAudioEvent> audio;
        std::vector<SrpChapter> chapters;
        bool haveScore = false;
        uint16_t score[2] = { 0, 0 };
        uint32_t writtenFrames = 0;             // frames in the last file written

        // A RESUMABLE recording (replay_v2.h): the parts SIMV, SETP, STAT, INPT hold.
        // HASH comes from `hashes` above -- the frames it compares.
        bool resume = false;
        uint64_t layout = 0;
        uint8_t rules = 0;
        uint16_t resumeSimWidth = 0;
        std::vector<uint8_t> setup, kickoff, inputs;   // inputs: SRP_INPUT_BYTES a frame
        bool inputsBroken = false;              // a batch came without its inputs
    };
    struct Match
    {
        uint64_t id = 0;
        Side side[2];
        std::string started;
        uint32_t lastMs = 0;
        uint32_t compared = 0;
        int64_t desyncFrame = -1;
        uint64_t desyncHash[2] = { 0, 0 };
        std::string desyncAt;
        std::string stamp;                      // "20260919-001308", for the file name
        std::string recording;                  // the .SRP written, once there is one
    };
    struct Connection
    {
        int fd = -1;
        std::string address;
        std::vector<uint8_t> in;
        Match* match = nullptr;
        int player = 0;             // 1 or 2 once its HELLO has arrived
    };

    void Accept();
    bool Read(Connection& c);                       // false = close it
    bool Handle(Connection& c, uint8_t type, const uint8_t* p, uint32_t len);
    bool OnHello(Connection& c, const uint8_t* p, uint32_t len);
    void OnFrames(Connection& c, const uint8_t* p, uint32_t len);
    void OnEnd(Connection& c, const uint8_t* p, uint32_t len);
    void OnReplayMeta(Connection& c, const uint8_t* p, uint32_t len);
    bool OnReplayFrames(Connection& c, const uint8_t* p, uint32_t len);
    void OnReplayScore(Connection& c, const uint8_t* p, uint32_t len);
    void OnReplayResume(Connection& c, const uint8_t* p, uint32_t len);
    void WriteReplay(Match& m, bool last);
    void Compare(Match& m, int from, uint32_t frame);
    void CloseConnection(Connection& c, const char* why);
    void Finish(Match& m, const char* why);
    std::string Label(const Match& m) const;

    int m_listen = -1;
    Ledger& m_ledger;
    std::string m_dir;
    std::vector<std::unique_ptr<Connection>> m_connections;
    std::map<uint64_t, std::unique_ptr<Match>> m_matches;   // by match id
    uint32_t m_lastSweepMs = 0;
};
