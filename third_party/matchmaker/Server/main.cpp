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
    int port = 19602;
    if (argc > 1) {
        int requested = atoi(argv[1]);
        if (requested <= 0 || requested > 65535) {
            std::cerr << "usage: " << argv[0] << " [port]   (1-65535, default 19602)\n";
            return 2;
        }
        port = requested;
    }
    Connections connections(port);
    // loop
    bool loop = true;
    std::cout << "SimpleMatchmaking Server running on Port " << port << "\n";
    std::cout.flush();
    while (loop) {
    ENetEvent event;
        connections.Update();
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