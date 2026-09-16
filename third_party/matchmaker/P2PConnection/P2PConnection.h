#pragma once
#include <functional>
#include <enet/enet.h>
#include <string>
#include "Utils.h"
#include <vector>
#include <string>
#include "ServerConnection.h"
#include <chrono>

struct GGPOStartInfo
{
    int yourPlayerNumber; // 1 or 2;

    uint16_t yourPort;
    uint16_t opponentPort;
    std::string opponentIP;
};

struct P2PCallbacks
{
    std::function<void()> Connected;
    std::function<void()> Disconncted;
    std::function<void()> Timeout;
    std::function<void()> ReadyStatusChanged; // true - ready, false - not ready
    std::function<void(const GGPOStartInfo&)> StartGame; // Ready to start game
    std::function<void()> CancelGame; // Ready to cancel game  
    std::function<void(const void*,size_t)> ReceiveUserMessage;
};

class PingHandler
{
public:
    PingHandler();
    void Update(ENetPeer* peerToPing);
    void OnPong();
    double GetPing();
    double PingHandler::GetPing() const;
    // False while still chasing a possible warm-up spike down (see m_warmedUp's own comment) - a
    // caller can use this to show "measuring..." instead of a number that might still jump/drop,
    // rather than displaying a value we ourselves don't trust as representative yet.
    bool IsWarmedUp() const { return m_warmedUp; }
    // 0-100: how good this connection currently is for playing, combining both the ping level
    // (scaled to the current game style's own frame time - see this function's own comment in the
    // .cpp for why) and how much the connection is currently jittering - a stable-but-high ping is
    // still not "good", so both are scored independently and the WORSE of the two wins, rather
    // than averaging one good aspect against a bad one. frameTimeInGameMs is 1000/fps for whatever
    // game style is currently active (MPGame::getMPFps()) - this class has no game-layer
    // dependency of its own, so it can't look that up itself.
    double GetQualityPercent(double frameTimeInGameMs) const;
    // Raw jitter in ms (smoothed mean absolute deviation from the ping EMA - see m_pingJitter's own
    // comment) - exposed on its own for callers that need the raw number rather than a percentage,
    // e.g. MPGame::getAutoFrameDelay()'s own low-ping/low-jitter bypass.
    double GetJitterMs() const { return m_pingJitter / 1000.0; }

private:
    // Exponential moving average, taken three a second, over the last 20 seconds
    double m_pingEMA = 0;
    double m_pingMean = 0;
    const int emaPeriodMS = 10 * 1000;
    const int pingPeriodMS = 250;
    const int m_nSamples = emaPeriodMS / pingPeriodMS;
    const double EMA_Constant = 2 / (1.0 + m_nSamples);
    unsigned int m_bPingSent =false;
    std::chrono::steady_clock::time_point lastPing;
    // A genuinely fresh P2P connection's very first few round-trips are often skewed by one-time
    // NAT/firewall punch-through cost on this exact peer pair, not real steady-state latency - a
    // reconnect to the SAME opponent shortly after reuses that already-open NAT mapping and skips
    // it entirely ("neu verbinden = hoch, gleich nochmal mit demselben Gegner = normal"). This can
    // show up as a spike that falls away, OR - confirmed via a real reconnect test - as an
    // unrealistically LOW first reading (near-zero, presumably near-idle ping/pong traffic before
    // real game data starts flowing) that then climbs to the true value. Directly seeding
    // m_pingEMA from the first sample and blending everything through the normal (~0.05
    // EMA_Constant, ~10s window) smoothing would leave the displayed ping stuck near that skewed
    // opening value for a long time either way.
    //
    // Warm-up here tracks a running streak of consecutive samples that stay within
    // kStableBandRatio of each other, in EITHER direction - "was würde man als Mensch tun? Man
    // würde beobachten, bis es sich in ein gewisses Band einpendelt", not just react to a single
    // drop or a single reversal (too sensitive to ordinary jitter). A sample that falls outside
    // the current streak is held back as a candidate rather than acted on immediately - a single
    // strong outlier shouldn't restart the streak or move the displayed value on its own - and is
    // only treated as a genuine trend shift once a SECOND sample confirms it (the two agree with
    // each other). kMaxWarmupSamples is a hard cap so warm-up can't run forever if the connection
    // never settles into a clean streak.
    bool m_warmedUp = false;
    static constexpr double kStableBandRatio = 0.2;   // how close two samples must be (relative to the larger) to count as "the same streak"
    static constexpr int kStableStreakNeeded = 3;     // this many consecutive samples within one band = settled
    static constexpr int kMaxWarmupSamples = 20;      // ~5s at pingPeriodMS=250 - safety cap so warm-up can't run forever
    double m_streakMin = 0, m_streakMax = 0;
    int m_streakLen = 0;
    int m_warmupSampleCount = 0;
    // A single sample that fell outside the current streak but hasn't yet been confirmed by a
    // second, matching sample - see OnPong()'s own comment.
    bool m_haveOutlierCandidate = false;
    double m_outlierCandidate = 0;
    // Smoothed mean absolute deviation of each sample from m_pingEMA - "how much is the connection
    // currently jittering", the same idea as TCP's own RTTVAR estimate for its retransmission
    // timeout. Only tracked once warmed up (before that, m_pingEMA itself is still catching up, so
    // deviation from it wouldn't mean anything yet).
    double m_pingJitter = 0;
    std::vector<double> m_pings;
};


class P2PConnection
{
    // Can't copy or move
    P2PConnection(const P2PConnection&) = delete;
    P2PConnection& operator=(const P2PConnection&) = delete;
    P2PConnection(const P2PConnection&&) = delete;
    P2PConnection& operator=(const P2PConnection&&) = delete;

public:
    P2PConnection(GameStartInfo info,std::function<void(const std::string&)> logger);
    ~P2PConnection();   
    void SendReady(bool status);
    void ToggleReady();
    void Update(P2PCallbacks& callbacks);
    void TryStart();
    void TryCancel();
    double GetPing() const;
    bool PingWarmedUp() const { return m_pingHandler.IsWarmedUp(); }
    double PingQualityPercent(double frameTimeInGameMs) const { return m_pingHandler.GetQualityPercent(frameTimeInGameMs); }
    double PingJitterMs() const { return m_pingHandler.GetJitterMs(); }
    void SendUserMessage(char* buffer, size_t length);
    bool ImReady()const { return m_bMeReady; }
    bool OtherReady() const { return m_bOtherReady; }  
    void Info();

private:
    const GameStartInfo m_info;
    const ENetAddress localAddress;
    ENetHostPtr local;
    std::vector<ENetAddress> peerCandidateAddresses;   
    std::vector<ENetPeer*> outGoingPeerCandidates;
    std::vector<ENetPeer*> peerConnections;
    bool m_bMeReady=false;
    bool m_bOtherReady=false;
    bool m_TryStart = false;   
    bool m_Start = false;
    bool m_TryCancel = false;  
    bool m_Cancel = false;
    bool CanSend()const;   
    void OnReadyChange();
    bool ReadyToStart() const;
    bool ReadyToCancel() const;
    ENetAddress m_PrimaryConnection;
    bool m_bPrimaryConnectionEstablished = false;
    size_t TotalActivePeers() const {
        return outGoingPeerCandidates.size() + peerConnections.size();
    }
    std::function<void(std::string)> m_logger;
    void CleanRedundantConnections();
    enum class P2PState { Connecting, Connected, Ready};

public:
    PingHandler m_pingHandler;
};