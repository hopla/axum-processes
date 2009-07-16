#include "ThreadListener.h"

NetworkListenerThread::NetworkListenerThread(MambaNetNetworkHandler *Handler)
{
//    assert(Handle!=NULL);
    NetworkHandler = Handler;
//    setTerminationEnabled();
}

void NetworkListenerThread::run()
{
    printf("Network listener thread started\n");
    NetworkHandler->StartListenOnNetwork();
    printf("Network listener thread stopped\n");
}

void NetworkListenerThread::stop()
{
    NetworkHandler->StopListenOnNetwork();
}
