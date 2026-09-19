#include <stdio.h>
#include <enet/enet.h>
#include <stdint.h>
#include <chrono>
#include <string>
#include <map>
#include <vector>
#include "StateMachine.h"
#include "Message.h"
#include "Connections.h"
#include "Sender.h"
#include <iostream>
#include "Utils.h"
#include "Relay.h"
#include "Recorder.h"
#include "WebGate.h"
#include <csignal>

// OpenSWOS: a clean stop on SIGINT/SIGTERM, so the relay can close its
// sessions and write their ledger lines. Upstream's loop never ended.
static volatile std::sig_atomic_t g_stop = 0;
static void OnStopSignal(int) { g_stop = 1; }

int main(int argc, char** argv)
{
    EnetInitialiser enetObj;
    // OpenSWOS: unbuffered logging. This server exists to be watched while
    // something else is debugged against it, and std::cout to a pipe is block
    // buffered -- so "the server printed nothing" meant only "the buffer has not
    // filled yet", which is indistinguishable from "the client never connected"
    // and wasted a debugging pass. Every line now appears when it happens.
    std::cout << std::unitbuf;
    // Upstream hardcoded 19602 while its README documented 19601 and "pass a
    // different port as an optional argument". Honour the argument, and keep
    // 19602 as the default so an existing swos-2020 client still finds us.
    //
    // OpenSWOS: and the relay's options (Relay.h). The port stays the first bare
    // number, so `swos-matchmaker 19602` means what it always did.
    int port = 19602;
    RelayOptions relayOptions;
    bool record = false;
    bool websocket = false;
    std::string recordings = "recordings";
    std::string ledgerPath;
    auto usage = [&]() {
        std::cerr << "usage: " << argv[0] << " [port] [--relay] [--netem PROFILE|cycle]"
                     " [--netem-seed N] [--record] [--recordings DIR] [--ledger FILE]\n"
                     "       [--websocket]\n"
                     "  port        UDP, 1-65535, default 19602\n"
                     "  --relay     OpenSWOS pairs play through this server\n"
                     "  --netem     a misbehaving network in the middle of every relayed\n"
                     "              match: a profile (";
        int32_t count = 0;
        const NetemProfile* all = NetemCore_Profiles(&count);
        for (int32_t i = 0; i < count; i++)
            std::cerr << (i ? " " : "") << all[i].name;
        std::cerr << "), or cycle for a different one each match\n"
                     "  --record    players may stream their matches here (TCP, the same port\n"
                     "              number), relayed or peer to peer; both players' frames are\n"
                     "              compared and a desync is reported the moment it happens;\n"
                     "              a full recording is kept as a .SRP\n"
                     "  --recordings  where those go (default: recordings/)\n"
                     "  --ledger    append a JSON line per relay session, recorded match and desync\n"
                     "  --websocket browsers (the web build) connect over WebSocket on TCP, the\n"
                     "              same port number, and any match with one in it is relayed\n";
        return 2;
    };
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--relay") {
            relayOptions.enabled = true;
        } else if (a == "--netem" && i + 1 < argc) {
            relayOptions.netem = argv[++i];
            if (relayOptions.netem != "cycle" && relayOptions.netem != "none" &&
                !NetemCore_FindProfile(relayOptions.netem.c_str()))
                return usage();
            if (relayOptions.netem == "none")
                relayOptions.netem.clear();
        } else if (a == "--netem-seed" && i + 1 < argc) {
            relayOptions.netemSeed = (uint32_t)strtoul(argv[++i], nullptr, 0);
        } else if (a == "--record") {
            record = true;
        } else if (a == "--websocket") {
            websocket = true;
        } else if (a == "--recordings" && i + 1 < argc) {
            recordings = argv[++i];
        } else if (a == "--ledger" && i + 1 < argc) {
            ledgerPath = argv[++i];
        } else {
            int requested = atoi(a.c_str());
            if (requested <= 0 || requested > 65535)
                return usage();
            port = requested;
        }
    }
    if (!relayOptions.netem.empty() && !relayOptions.enabled) {
        std::cerr << "--netem acts on relayed matches: add --relay\n";
        return 2;
    }
    relayOptions.web = websocket;
    Connections connections(port);
    Ledger ledger(ledgerPath);
    if (!ledgerPath.empty())
        std::cout << (ledger.IsOpen() ? "ledger: " : "cannot open the ledger ") << ledgerPath << "\n";
    Relay relay(connections.Host(), relayOptions, ledger);
    connections.SetRelay(&relay);
    Recorder recorder(record, (uint16_t)port, ledger, recordings);
    // Both take TCP on the port number the game uses for UDP: a browser's WebSocket
    // arrives there, because the browser build sends its datagrams to that port.
    WebGate web(websocket, (uint16_t)port);
    TcpGate tcp((uint16_t)port, web.Enabled() ? &web : nullptr, recorder.Enabled() ? &recorder : nullptr);
    std::signal(SIGINT, OnStopSignal);
    std::signal(SIGTERM, OnStopSignal);
    // loop
    bool loop = true;
    std::cout << "SimpleMatchmaking Server running on Port " << port << "\n";
    std::cout.flush();
    while (loop && !g_stop) {
    ENetEvent event;
        connections.Update();
        relay.Update();
        tcp.Update();
        web.Update();
        recorder.Update();
        while (enet_host_service(connections.Host(), &event, 1) > 0)
        {
            switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
            { 
                connections.NewConnection(event.peer);                    
                break;
            }
            case ENET_EVENT_TYPE_RECEIVE:
            {
                EnetPacketRAIIGuard guard(event.packet);                
                connections.ReceiveMessage(event.peer, event.packet->data, event.packet->dataLength);  
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT:
            {
                connections.LostConnection(event.peer);
                break;
            }
            case ENET_EVENT_TYPE_NONE:
                break;
            }
        }
    }

}