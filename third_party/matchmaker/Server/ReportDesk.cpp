// OpenSWOS: match reports. See include/ReportDesk.h, and include/report_proto.h in
// the game tree for the protocol.
#include "ReportDesk.h"
#include "record_proto.h"   // Record_Get: the same big-endian helpers
#include "report_proto.h"
#include <iostream>

#ifdef _WIN32

// The server has only ever been run on Linux (WSL included); the desk is POSIX.
ReportDesk::ReportDesk(bool enabled, Ledger& ledger, const std::string& dir) :
    m_ledger(ledger), m_dir(dir)
{
    if (enabled)
        std::cout << "[report] not available on a Windows build of the server\n";
}
ReportDesk::~ReportDesk() {}
void ReportDesk::Adopt(int, const std::string&) {}
void ReportDesk::Update() {}

#else

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>

namespace
{
    // The most connections sending at once, and how long one may take.
    const size_t kMaxConnections = 64;
    const uint32_t kTimeoutMs = 30000;

    std::string Tag()
    {
        return "[report] " + ServerTime::TimeOfDay() + " ";
    }

    bool TakeString(const uint8_t* p, size_t len, size_t* at, std::string* out)
    {
        if (*at >= len)
            return false;
        size_t n = p[*at];
        if (*at + 1 + n > len)
            return false;
        out->assign((const char*)p + *at + 1, n);
        *at += 1 + n;
        return true;
    }

    // Where the four strings end, or 0 if they have not all arrived.
    size_t StringsEnd(const std::vector<uint8_t>& in)
    {
        size_t at = REPORT_UPLOAD_FIXED;
        for (int i = 0; i < 4; i++)
        {
            if (at >= in.size())
                return 0;
            at += 1 + in[at];
        }
        return at <= in.size() ? at : 0;
    }
}

ReportDesk::ReportDesk(bool enabled, Ledger& ledger, const std::string& dir) :
    m_enabled(enabled), m_ledger(ledger), m_dir(dir)
{
    if (!enabled)
        return;
    if (!m_dir.empty())
        mkdir(m_dir.c_str(), 0755);                 // one level; EEXIST is the usual answer
    std::cout << "[report] on: match reports kept in " << m_dir << "/<match id>/\n";
}

ReportDesk::~ReportDesk()
{
    for (auto& c : m_connections)
        if (c->fd >= 0)
            close(c->fd);
}

void ReportDesk::Adopt(int fd, const std::string& address)
{
    if (!m_enabled || m_connections.size() >= kMaxConnections)
    {
        close(fd);
        return;
    }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    auto c = std::make_unique<Connection>();
    c->fd = fd;
    c->address = address;
    c->sinceMs = ServerTime::NowMs();
    m_connections.push_back(std::move(c));
}

void ReportDesk::Update()
{
    if (!m_enabled)
        return;
    uint32_t now = ServerTime::NowMs();
    for (auto& c : m_connections)
    {
        if (c->fd < 0)
            continue;
        if (!Read(*c) || now - c->sinceMs > kTimeoutMs)
        {
            if (now - c->sinceMs > kTimeoutMs)
                std::cout << Tag() << c->address << ": timed out after " << c->in.size() << " bytes\n";
            close(c->fd);
            c->fd = -1;
        }
    }
    for (size_t i = 0; i < m_connections.size();)
    {
        if (m_connections[i]->fd < 0)
            m_connections.erase(m_connections.begin() + i);
        else
            i++;
    }
}

bool ReportDesk::Read(Connection& c)
{
    uint8_t buf[16384];
    for (;;)
    {
        size_t want = c.need ? c.need - c.in.size() : sizeof buf;
        if (want > sizeof buf)
            want = sizeof buf;
        ssize_t n = recv(c.fd, buf, want, 0);
        if (n > 0)
        {
            c.in.insert(c.in.end(), buf, buf + n);
        }
        else if (n == 0)
        {
            std::cout << Tag() << c.address << ": closed after " << c.in.size() << " bytes, before the end\n";
            return false;
        }
        else
        {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return true;
            return false;
        }

        if (c.in.size() >= 8 &&
            (memcmp(c.in.data(), REPORT_UPLOAD_MAGIC, 4) != 0 || c.in[4] != REPORT_PROTOCOL))
        {
            std::cout << Tag() << c.address << ": not a report this server understands\n";
            Answer(c, 1);
            return false;
        }
        if (!c.need && c.in.size() >= REPORT_UPLOAD_FIXED)
        {
            uint32_t zipLen = Record_Get32(c.in.data() + 16);
            if (zipLen > REPORT_ZIP_MAX)
            {
                std::cout << Tag() << c.address << ": refused, " << zipLen << " bytes is too big\n";
                Answer(c, 1);
                return false;
            }
            size_t head = StringsEnd(c.in);
            if (head)
                c.need = head + zipLen;
            else if (c.in.size() > REPORT_UPLOAD_FIXED + 4 * 256)
                return false;
        }
        if (c.need && c.in.size() >= c.need)
        {
            Answer(c, Keep(c) ? 0 : 1);
            return false;
        }
    }
}

bool ReportDesk::Keep(Connection& c)
{
    const uint8_t* p = c.in.data();
    int player = p[5], rating = p[6];
    uint64_t id = Record_Get64(p + 8);
    uint32_t zipLen = Record_Get32(p + 16);
    size_t at = REPORT_UPLOAD_FIXED;
    std::string name, peer, build, comment;
    TakeString(p, c.in.size(), &at, &name);
    TakeString(p, c.in.size(), &at, &peer);
    TakeString(p, c.in.size(), &at, &build);
    TakeString(p, c.in.size(), &at, &comment);
    const uint8_t* zip = p + at;

    if ((player != 1 && player != 2) || rating > 5 || id == 0 || zipLen < 4 ||
        memcmp(zip, "PK\x03\x04", 4) != 0)
    {
        std::cout << Tag() << c.address << ": refused, not a report (player " << player
                  << ", match " << Hex64(id) << ")\n";
        return false;
    }

    // The match id is a hash, printed as hex: it is the only part of a path a player chose.
    std::string dir = m_dir + "/" + Hex64(id);
    mkdir(dir.c_str(), 0755);
    std::string path = dir + "/player" + std::to_string(player) + ".zip";
    std::string temp = path + ".part";
    FILE* f = fopen(temp.c_str(), "wb");
    bool ok = f && fwrite(zip, 1, zipLen, f) == zipLen;
    if (f)
        ok = (fclose(f) == 0) && ok;
    if (!ok || rename(temp.c_str(), path.c_str()) != 0)
    {
        std::cout << Tag() << "cannot write " << path << ": " << strerror(errno) << "\n";
        remove(temp.c_str());
        return false;
    }

    std::cout << Tag() << "match " << Hex64(id) << ": player " << player << " (" << name
              << ", build " << build << ") -- " << (rating ? std::to_string(rating) + " of 5" : "no rating")
              << (comment.empty() ? "" : ", \"" + comment + "\"") << "; " << zipLen << " bytes from "
              << c.address << "\n";
    m_ledger.Write("{\"kind\":\"report\",\"match\":\"" + Hex64(id) + "\""
        + ",\"at\":" + JsonString(ServerTime::WallClock())
        + ",\"player\":" + std::to_string(player)
        + ",\"name\":" + JsonString(name)
        + ",\"peer\":" + JsonString(peer)
        + ",\"build\":" + JsonString(build)
        + ",\"rating\":" + (rating ? std::to_string(rating) : std::string("null"))
        + ",\"comment\":" + JsonString(comment)
        + ",\"bytes\":" + std::to_string(zipLen)
        + ",\"from\":" + JsonString(c.address)
        + ",\"file\":" + JsonString(path)
        + "}");
    return true;
}

void ReportDesk::Answer(Connection& c, uint8_t status)
{
    uint8_t answer[5];
    memcpy(answer, REPORT_UPLOAD_MAGIC, 4);
    answer[4] = status;
    // Five bytes into a fresh socket's empty send buffer: never partial in practice,
    // and a player who does not get it logs "no answer" -- nothing worse.
    (void)send(c.fd, answer, sizeof answer, MSG_NOSIGNAL);
}

#endif
