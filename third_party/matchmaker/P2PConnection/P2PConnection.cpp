#include "P2PConnection.h"
#include "Message.h"
#include "Sender.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <string.h>
using namespace std::chrono;
P2PConnection::P2PConnection(GameStartInfo info, std::function<void(const std::string&)> logger) :
    m_info(info),
    localAddress{ ENET_HOST_ANY,info.port},  
    m_logger(logger),
    local(nullptr, enet_host_destroy),
    peerCandidateAddresses(m_info.peerAddresses)
{
    peerCandidateAddresses = m_info.peerAddresses;

    auto lastlocalPort = m_info.peerAddresses.back().port;
    auto remoteAddr = m_info.peerAddresses.front();


    local = ENetHostPtr(enet_host_create(&localAddress, peerCandidateAddresses.size() * 2, 0, 0, 0),
        enet_host_destroy);

    for(auto& address : peerCandidateAddresses)
        outGoingPeerCandidates.push_back(enet_host_connect(local.get(), &address, 0, 0));

    logger("********Start P2P connection process with " + info.peerName + ". Try connections on: \n");
    for (const auto& addr : peerCandidateAddresses)
        logger(ToReadableString(addr) + "\n");
    logger("****************\n");
}

P2PConnection::~P2PConnection()
{
    if (local)
    {
        for (auto& peer : peerConnections)
            enet_peer_disconnect(peer,0);

        enet_host_flush(local.get());
    }
}

void P2PConnection::TryStart()
{
    if (m_info.playerNumber != 1) {
        m_logger("Only player 1 can start the game\n");
        return;
    }
    if (!(m_bMeReady && m_bOtherReady))
    {
        m_logger("Cannot start yet, both players are not ready\n");
        return;
    }
    if (m_Start)
    {
        m_logger("Already initiated Start process\n");
        return;
    }
    if (m_TryStart)
    {
        m_logger("Already initiated TryStart process\n");
        return;
    }
    if (!peerConnections.size())
    {
        m_logger("No peer connection\n");
        return;
    }
    if (!m_bPrimaryConnectionEstablished)
    {
        m_logger("No primary connection agreed\n");
        return;
    }    
   
    m_TryStart = true;
    m_logger("Initiating Start process: Send Start message, wait for commit, then disconnect\n");

    Message::Make(MessageType::Info, "Start").OnData(SendTo(peerConnections[0]));    
}

void P2PConnection::TryCancel()
{
    if (m_Cancel)
    {
        m_logger("Already initiated Cancel process\n");
        return;
    }
    if (m_TryCancel)
    {
        m_logger("Already initiated TryCancel process\n");
        return;
    }
    if (!peerConnections.size())
    {
        m_logger("No peer connection\n");
        return;
    }
    if (!m_bPrimaryConnectionEstablished)
    {
        m_logger("No primary connection agreed\n");
        return;
    }

    m_TryCancel = true;
    m_logger("Initiating Start process: Send Start message, wait for commit, then disconnect\n");

    Message::Make(MessageType::Info, "Cancel").OnData(SendTo(peerConnections[0]));
}

void P2PConnection::ToggleReady()
{
    if (m_Start || m_TryStart) {
        m_logger("Cannot change ready status now that start process has begun\n");
        return;
    }

    m_bMeReady = !m_bMeReady;

    if (peerConnections.size())
    {
        Message::Make(MessageType::Ready, m_bMeReady ? "1" : "0").OnData(SendTo(peerConnections[0]));
        OnReadyChange();
    }
}

void P2PConnection::SendReady(bool status)
{
    if (m_Start || m_TryStart) {
        m_logger("Cannot change ready status now that start process has begun\n");
        return;
    }
 
    m_bMeReady = status;
 
    if (peerConnections.size())
    {
        Message::Make(MessageType::Ready, m_bMeReady ? "1" : "0").OnData(SendTo(peerConnections[0]));
        OnReadyChange();
    }
}
 
void P2PConnection::OnReadyChange()
{
#if (0)
    if (m_bMeReady)
        m_logger("I'm ready, ");
    else
        m_logger("I'm not ready, ");

    if (m_bOtherReady)
        m_logger("other player is ready\n");
    else
        m_logger("other player is not ready\n");
#endif

    if (m_bMeReady && m_bOtherReady)
        m_logger(m_info.playerNumber == 1 ? "Press S to start game\n" : "Waiting for player 1 to start game\n");
    else
    {
        if(m_TryStart)
            m_logger("Abort start\n");
        m_Start = false;
        m_TryStart = false;
    }        
}
bool P2PConnection::ReadyToStart() const
{
    bool allConnectionsDead = peerConnections.size() + outGoingPeerCandidates.size() == 0;
    bool ready = m_Start && !m_Cancel && allConnectionsDead;
    return ready;
}
 
bool P2PConnection::ReadyToCancel() const
{
    bool allConnectionsDead = peerConnections.size() + outGoingPeerCandidates.size() == 0;
    bool ready = !m_Start && m_Cancel && allConnectionsDead;
    return ready;
}
 
void P2PConnection::Info()
{
    m_logger( "**************Lobby Connection Info:***********\n");
    m_logger(std::string("Number of active connections : ") + std::to_string(peerConnections.size()) +"\n");
    m_logger(std::string("Number of pending connections : ")+ std::to_string(outGoingPeerCandidates.size()) + "\n");
    if (m_bPrimaryConnectionEstablished && peerConnections.size())
        m_logger(std::string("Primary connection is to :") + ToReadableString(peerConnections[0]->address) + "\n");
    m_logger("*********\n");
}

void P2PConnection::CleanRedundantConnections()
{
    if (m_bPrimaryConnectionEstablished && TotalActivePeers() > 1)
    {
        for (size_t i = 1; i < peerConnections.size(); i++)
            enet_peer_reset(peerConnections[i]);

        eraseAndRemoveIfNot(peerConnections, peerConnections[0]);

        for (size_t i = 0; i < outGoingPeerCandidates.size(); i++)
            enet_peer_reset(outGoingPeerCandidates[i]);

        outGoingPeerCandidates.clear();
        Info();
    }
}

//#include <iostream>
double PingHandler::GetPing() const 
{ 
   // std::cout << "EMA: " << m_pingEMA << ", mean: " << m_pingMean << "\n";
    return m_pingEMA; 
}

PingHandler::PingHandler() : lastPing(steady_clock::now())
{
 
}
 
void PingHandler::Update(ENetPeer* peerToPing)
{
    if (m_bPingSent)
        return;
 
    if (!peerToPing)
        return;
 
    //printf("PingHandler::Update...\n");  // <- sometimes not run (zeo ping)

    auto now = steady_clock::now();
    auto timeSinceLastPing = (int)duration_cast<milliseconds>(now - lastPing).count();
    //printf("timeSinceLastPing = %d, pingPeriodMS = %d\n", timeSinceLastPing, pingPeriodMS);
    if (timeSinceLastPing >= pingPeriodMS)
    {
        Message::Make(MessageType::Info, "Ping").OnData(SendTo(peerToPing));
        lastPing = now;
        m_bPingSent = true;
    }
}
//#include <iostream>
void PingHandler::OnPong()
{
    m_bPingSent = false;
    //printf("m_bPingSent = false\n");

    auto thisPing = (double)duration_cast<microseconds>(steady_clock::now() - lastPing).count();

    // ignore silly big pings, these are probably not representative
    if (thisPing > 1 *1e6)
        return;

    if (m_pingEMA == 0) {
        // First sample ever - seeds both the displayed value and the first candidate streak.
        m_pingEMA = thisPing;
        m_streakMin = m_streakMax = thisPing;
        m_streakLen = 1;
        return;
    }

    if (!m_warmedUp) {
        m_warmupSampleCount++;
        double newMin = (std::min)(m_streakMin, thisPing);
        double newMax = (std::max)(m_streakMax, thisPing);
        if (newMax - newMin <= newMax * kStableBandRatio) {
            // Still within the current streak's own band - extend it, whichever direction this
            // came from. Clears any held-back outlier candidate too: the streak itself just
            // absorbed this sample, so there's nothing left to confirm against.
            m_streakMin = newMin;
            m_streakMax = newMax;
            m_streakLen++;
            m_haveOutlierCandidate = false;
            m_pingEMA = thisPing;
        }
        else if (m_haveOutlierCandidate) {
            double candMin = (std::min)(m_outlierCandidate, thisPing);
            double candMax = (std::max)(m_outlierCandidate, thisPing);
            if (candMax - candMin <= candMax * kStableBandRatio) {
                // Two samples in a row now agree with each other, outside the OLD streak's band -
                // a genuine trend shift, not a one-off blip. Start a fresh streak from both.
                m_streakMin = candMin;
                m_streakMax = candMax;
                m_streakLen = 2;
                m_haveOutlierCandidate = false;
                m_pingEMA = thisPing;
            }
            else {
                // Doesn't match the old streak OR the pending candidate - replace the candidate
                // with this one instead of accumulating unrelated one-offs. Display/streak
                // untouched either way.
                m_outlierCandidate = thisPing;
            }
        }
        else {
            // First sample to fall outside the current streak - hold it back rather than acting
            // on it immediately (see this function's own header comment: a single strong outlier
            // shouldn't restart the streak or move the displayed value on its own).
            m_haveOutlierCandidate = true;
            m_outlierCandidate = thisPing;
        }

        if (m_streakLen >= kStableStreakNeeded || m_warmupSampleCount >= kMaxWarmupSamples)
            m_warmedUp = true; // from the NEXT sample on, normal EMA smoothing takes over
        return;
    }

    double deviation = std::abs(thisPing - m_pingEMA);
    m_pingJitter = (deviation * EMA_Constant) + (m_pingJitter * (1 - EMA_Constant));
    m_pingEMA = (thisPing * EMA_Constant) + (m_pingEMA * (1 - EMA_Constant));
}

double PingHandler::GetQualityPercent(double frameTimeInGameMs) const
{
    // Linear falloff from 100% (perfect) to 0% (worst case) between the two bounds below - the
    // ping level and the jitter are scored independently, then combined as a geometric mean
    // (sqrt(a*b), not a plain average) so BOTH always matter: a good, low-jitter connection isn't
    // dragged down to nothing by one bad aspect the way a plain min() would, but a good level also
    // can't fully hide a badly jittering connection the way a plain average would let it. E.g. a
    // good-but-jittery connection (level 90%, jitter 60%) still clearly beats a bad-but-stable one
    // (level 30%, jitter 95%): sqrt(90*60)=73.5% vs. sqrt(30*95)=53.4% - a plain average would get
    // this backwards-feeling result closer together (75% vs. 62.5%), and min() would only look at
    // the worse number in each pair (60% vs. 30%) and ignore the other entirely.
    //
    // The level bounds scale with frameTimeInGameMs (1000/fps for whatever game style is
    // currently active - PC DOS/Amiga/Amiga 60fps/Xbox all run at different fps, see
    // MPGame::getMPFps()) rather than being one fixed ms constant for every style. This mirrors
    // MPGame::getAutoFrameDelay()'s own approach (it divides the round trip by 2, then by the
    // style's own frame time, before turning it into a frame count - the same GGPO-rollback
    // halving discussed where this function is declared), but WITHOUT that function's minimum-2
    // clamp and its separate, inconsistent per-style "always 1" bypass thresholds
    // (getMinFrameDelayForPCDOSGameStyle() and friends use 19ms/33ms/50ms - three independently
    // hand-picked numbers with no clean shared relationship, confirmed by checking several
    // candidate ratios against each other). Bounds here instead come from one single, consistent
    // rule applied to every style alike:
    //
    //   best  = 2 * frameTimeInGameMs (in an unclamped version of getAutoFrameDelay()'s own
    //           formula, this is exactly the ping at which a fresh connection would compute a
    //           "2" instead of rounding down to "1" - i.e. as good as it gets before the very
    //           first real step down)
    //   worst = 15 * frameTimeInGameMs. Originally 12 (matching the same formula's old threshold
    //           for "7", the highest frame count AUTO mode would ever pick) - raised to 15
    //           deliberately so a mediocre-to-bad connection reads less harshly negative overall
    //           (e.g. PC DOS: a 150ms ping used to score 15%, now scores ~35%), at the cost of the
    //           tidy link to getAutoFrameDelay()'s own thresholds this bound used to have.
    //
    // Coincidentally, this lines up almost exactly with the real "AMIGA (60FPS)"/"XBOX" bypass
    // threshold (2 * 16.7ms = 33.3ms vs. the real 33ms) while landing on cleaner, rounder numbers
    // for PC DOS (28.6ms) and original Amiga (40ms) than their real, seemingly independently
    // chosen 19ms/50ms.
    auto pct = [](double value, double best, double worst) {
        if (value <= best)  return 100.0;
        if (value >= worst) return 0.0;
        return 100.0 * (worst - value) / (worst - best);
    };
    double levelUs  = frameTimeInGameMs * 1000.0; // ms -> us, to match m_pingEMA's own unit
    double levelPct = pct(m_pingEMA, 2.0 * levelUs, 15.0 * levelUs);
    // Jitter bounds now scale with frameTimeInGameMs too (same idea as the level bounds above),
    // rather than being one fixed ms range for every style - jitter smaller than a single frame is
    // effectively free (rollback/frame-delay already absorbs it). best=1.5 frames (bumped up from
    // a first try at 1 frame, which still scored real excellent connections too harshly per live
    // testing); worst=6 frames - chosen independently of the level score's own bounds above (not
    // tied to matching its ratio, despite an earlier version of this comment claiming that): jitter
    // falls off over a narrower range on purpose, since unlike a stable-but-high ping (which a
    // higher frame delay simply absorbs), jitter spikes stay visible as rollbacks regardless of the
    // chosen delay, so they earn a stricter scale here.
    double jitterPct = pct(m_pingJitter, 1.5 * levelUs, 6.0 * levelUs);
    return std::sqrt(levelPct * jitterPct);
}

double P2PConnection::GetPing() const 
{
    return m_pingHandler.GetPing()/1e3;
}

void P2PConnection::SendUserMessage(char* buffer, size_t length)
{
    if (CanSend())
        Message::Make(MessageType::UserMessage, std::string(buffer,length)).OnData(SendTo(peerConnections[0]));
}

bool P2PConnection::CanSend() const
{
    /*
    // -- original
    if (m_bMeReady)
        return false;
    if (m_Start)
        return false;
    if (m_TryStart)
        return false;
    if (m_Cancel)
        return false;
    if (m_TryCancel)
        return false;  
    if (!m_bPrimaryConnectionEstablished)
        return false;
    if (peerConnections.size() != 1)
        return false;
    return true;
    */

    // -- updated
    if (m_Start)
        return false;
    if (m_TryStart)
        return false;
    if (m_Cancel)
        return false;
    if (m_TryCancel)
        return false;
    if (!m_bPrimaryConnectionEstablished)
        return false;
    //if (peerConnections.size() != 1)
    //if (peerConnections.size() != 1)
    //    return false;
    if (peerConnections.size() == 0) return false;
    return true;
}
 
void P2PConnection::Update(P2PCallbacks& callbacks)
{
    ENetEvent event;
    CleanRedundantConnections();

#if (0)
    // old code
    if (CanSend())
        m_pingHandler.Update(peerConnections[0]);
#else
    // new code
    /*
    if (CanSend()) {
        for (int i = 0; i <= (int)peerConnections.size() - 1; i++) {
            if (m_pingHandler.Update(peerConnections[i])) break;
        }
    }
    */
    if (CanSend())
        m_pingHandler.Update(peerConnections[0]);
#endif

    while (enet_host_service(local.get(), &event, 1) > 0)
    {
        switch (event.type) {
        case ENET_EVENT_TYPE_CONNECT:
        {
            m_logger(std::string("connect event, ip: ") + ToReadableString(event.peer->address) + " id: " + std::to_string(event.peer->connectID) + "\n");

            auto matchAddressOnly = [&](const ENetAddress& a)->bool { return a.host == event.peer->address.host; };
            // If the incoming connection is from the candidate list, accept it and clear the candidate list
            if (contains(peerCandidateAddresses, matchAddressOnly))
                {

                    m_logger("Connected to a Peer \n");
                
                    peerConnections.push_back(event.peer);
 
                    if (contains(outGoingPeerCandidates, event.peer)) {
                        eraseAndRemove(outGoingPeerCandidates, event.peer);
                        m_logger("Peer was in our candidate list: we connected to them\n");
                    }
                    else
                        m_logger("Peer was not in our outgoing list: they connected to us\n");
 
                    // Is this the first connection?
                    if (peerConnections.size() == 1)
                    {  
                        // If we are player1, we tell the other player to use this connction as the primary one
                        if (m_info.playerNumber == 1)
                        {
                            callbacks.Connected();                         
                            m_bPrimaryConnectionEstablished = true;
                            m_PrimaryConnection = peerConnections[0]->address;;                         
                            Message::Make(MessageType::Info, "PRIMARY").OnData(SendTo(event.peer));
                        }
                    }
                    else
                    {
                        m_logger("Redundant connection established\n");
                    }
                }
                else
                {
                    if (contains(m_info.peerAddresses, event.peer->address))
                    {
                        m_logger(std::string("Incoming connection was from a potential candidate that we no longer want to use: ") + ToReadableString(event.peer->address) + " bin it\n");
                        enet_peer_reset(event.peer); 
                    }
                    else
					{
                        m_logger(std::string("Incoming connection from unexpected peer: ") + ToReadableString(event.peer->address) + " bin it\n");
                        enet_peer_reset(event.peer);
					}	
                }
				Info();
                break;
            }
            case ENET_EVENT_TYPE_RECEIVE:
            {
                EnetPacketRAIIGuard guard(event.packet);
               
 
                auto msg = Message::Parse(event.packet->data, event.packet->dataLength);                 
               
                if (msg.Type() == MessageType::UserMessage)
                {
                    msg.OnPayload(callbacks.ReceiveUserMessage);
                }
                else if (msg.Type() == MessageType::Ready)
                {
                    m_bOtherReady = std::stoi(msg.Content());
                    OnReadyChange();
                    callbacks.ReadyStatusChanged();
                }               
                else if (msg.Type() == MessageType::Info)
                {
                    if (strcmp(msg.Content().c_str(), "Ping") != 0 && strcmp(msg.Content().c_str(), "Pong"))
                    {
                        m_logger("We received a message: ");
                        msg.ToConsole();
                    }
 
                    if (!strcmp(msg.Content().c_str(), "PRIMARY"))
                    {
                        m_bPrimaryConnectionEstablished = true;
                        if (peerConnections.size() && (peerConnections[0] == event.peer))
                        {
                            m_logger("Agree with player 1 on primary connection\n");
                        }
                        else
                        {
                            auto it = std::find(RANGE(peerConnections), event.peer);
                            if (it == peerConnections.end())
                                m_logger("Player 1's primary connection is not even present for us.. this should never happen\n");
                            else
                            {
                                std::iter_swap(peerConnections.begin(), it);
                                m_logger("Did't agree with player 1 on primary connection, swapped ours to match.\n");
                            }
                        }
                        m_PrimaryConnection = peerConnections[0]->address;
                        callbacks.Connected();                     
                    }
                    if (!strcmp(msg.Content().c_str(), "Ping")) {
                        //printf("Message::Make(MessageType::Info, ""Pong"").OnData(SendTo(event.peer));\n");
                        Message::Make(MessageType::Info, "Pong").OnData(SendTo(event.peer));
                    }
                    else {
                        //printf("Message::Make(MessageType::Info, ""Pong"").OnData(SendTo(event.peer)); - SKIPPED\n");
                    }
                    
                    if (!strcmp(msg.Content().c_str(), "Pong"))
                    {
                        if (CanSend()) {
                            //printf("m_pingHandler.OnPong();\n");
                            m_pingHandler.OnPong();
                        }
                        else {
                            //printf("m_pingHandler.OnPong(); SKIPPED\n");
                        }
                    }
                    if (!strcmp(msg.Content().c_str(), "Start"))
                    {
                        // If we are player 1, this is a response from player 2 saying that they are committed to the start - so are we!
                        // Let's begin the disconnection process
                        if (m_TryStart)
                        {
                            m_logger("Start confirmation from player 2, disconnect now\n");
                            m_Start = true;
                            for (auto& peer : peerConnections)
                                enet_peer_disconnect_later(peer, 0);
                        }
                        else
                        {                       
                            if (m_bMeReady && m_bOtherReady)
                            {
                                m_logger("Start initiated from player 1, we confirm, wait for disconnect\n");
                                // If we are player 2, and we can start, send a response to confirm, then wait for diconnection
                                m_Start = true;
                                Message::Make(MessageType::Info, "Start").OnData(SendTo(peerConnections[0]));
                            }
                            else
                            {
                                m_logger("Start initiated from player 1, we cannot proceed\n");
                            }
                        }
                    }
                    if (!strcmp(msg.Content().c_str(), "Cancel"))
                    {
                        // If we are player 1, this is a response from player 2 saying that they are committed to the cancel - so are we!
                        // Let's begin the disconnection process
                        if (m_TryCancel)
                        {
                            m_logger("Cancel confirmation from player 2, disconnect now\n");
                            m_Cancel = true;
                            for (auto& peer : peerConnections)
                                enet_peer_disconnect_later(peer, 0);
                        }
                        else
                        {                       
                            m_logger("Cancel initiated from player 1, we confirm, wait for disconnect\n");
                            m_Cancel = true;
                            Message::Make(MessageType::Info, "Cancel").OnData(SendTo(peerConnections[0]));
                        }
                    }      
					/*            
                    if (!strcmp(msg.Content(), "Ready"))
                    {
                        m_bOtherReady = true;
                        OnReadyChange();
                    }
                    if (!strcmp(msg.Content(), "Unready"))
                    {
                        m_bOtherReady = false;
                        OnReadyChange();
                    }
					/*/
                }
 
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT:
            {
                m_logger(std::string("disconnect event ip ") + ToReadableString(event.peer->address) + "\n");
                if (contains(peerConnections,event.peer))
                {
                    if(event.peer== peerConnections[0])
                        m_logger(std::string("We Disconnected from primary connection: ")+ ToReadableString(event.peer->address) + "\n");
                    else
                        m_logger(std::string("We Disconnected from redundant connection: ") + ToReadableString(event.peer->address) + "\n");
                    eraseAndRemove(peerConnections, event.peer);
                }
                else if (contains(outGoingPeerCandidates, event.peer))
                {
                    m_logger(std::string("Abort connection attempt to ") + ToReadableString(event.peer->address) + "\n");
                    eraseAndRemove(outGoingPeerCandidates, event.peer);
                }
                else
                {
                    m_logger("Disconnect from unknown\n");
                }
                m_logger("ENET_EVENT_TYPE_DISCONNECT\n");
                Info();
                if (ReadyToStart())
                {
				    // We don't do anything after this, so it's ok to delete this object in the callback                              
                    m_logger("All connections closed, Start is set, let's go!!!\n");
                   
                    GGPOStartInfo info;
                    info.opponentIP = ToReadableIPv4String(m_PrimaryConnection);
                    info.opponentPort = m_PrimaryConnection.port;
                    info.yourPlayerNumber = m_info.playerNumber;
                    info.yourPort = m_info.port;
 
                    callbacks.StartGame(info);
                    return;
                }
                else if (ReadyToCancel())
                {
                    // We don't do anything after this               
                    m_logger("All connections closed, cancel is set\n");
 
                    callbacks.CancelGame();
                    return;
                }              
                else if (peerConnections.size() == 0 && outGoingPeerCandidates.size() == 0)
                {
                    if (m_bPrimaryConnectionEstablished)
                        callbacks.Disconncted();
                    else
                        callbacks.Timeout();
                    return;
                   
                }
              
                break;
            }
           }
        }
}

