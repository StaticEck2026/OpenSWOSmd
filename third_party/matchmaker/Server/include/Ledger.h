#pragma once
// OpenSWOS: what the relay and the recorder share (not in upstream) -- the
// ledger they both append to, and the clocks and formatting they both use.
//
// The ledger is one JSON object per line (--ledger FILE): "relay" lines from
// Relay.cpp when a relay session closes, "match" lines from Recorder.cpp when a
// recorded match closes, and a "desync" line the moment the recorder sees one --
// written then, not at the end, so a desync survives the server being killed.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>

namespace ServerTime
{
    // Milliseconds on a MONOTONIC clock -- for timeouts. Not enet_time_get: that is
    // gettimeofday, which a time sync can step (it closed a relay session under two
    // players when WSL2's clock moved).
    inline uint32_t NowMs()
    {
        return (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    // "2026-09-18T23:47:21" -- for the ledger.
    inline std::string WallClock()
    {
        char buf[32];
        std::time_t t = std::time(nullptr);
        std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%S", std::localtime(&t));
        return buf;
    }

    // "23:47:21.982" -- for a console line. Every relay and recorder line is timed,
    // because the question those logs answer is usually "when".
    inline std::string TimeOfDay()
    {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        int ms = (int)(std::chrono::duration_cast<std::chrono::milliseconds>(
                           now.time_since_epoch()).count() % 1000);
        char hms[16], out[24];
        std::strftime(hms, sizeof hms, "%H:%M:%S", std::localtime(&t));
        snprintf(out, sizeof out, "%s.%03d", hms, ms);
        return out;
    }
}

inline std::string JsonString(const std::string& s)
{
    std::string out = "\"";
    for (unsigned char c : s)
    {
        if (c == '"' || c == '\\') { out += '\\'; out += (char)c; }
        else if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); out += b; }
        else out += (char)c;
    }
    return out + "\"";
}

inline std::string Hex64(uint64_t v)
{
    char buf[17];
    snprintf(buf, sizeof buf, "%016llx", (unsigned long long)v);
    return buf;
}

class Ledger
{
public:
    explicit Ledger(const std::string& path)
    {
        if (!path.empty())
            m_file = fopen(path.c_str(), "a");
    }
    ~Ledger()
    {
        if (m_file)
            fclose(m_file);
    }
    bool IsOpen() const { return m_file != nullptr; }
    // One JSON object, without the newline.
    void Write(const std::string& json)
    {
        if (!m_file)
            return;
        fputs(json.c_str(), m_file);
        fputc('\n', m_file);
        fflush(m_file);
    }

private:
    FILE* m_file = nullptr;
    Ledger(const Ledger&) = delete;
    Ledger& operator=(const Ledger&) = delete;
};
