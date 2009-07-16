#ifndef NETWORKLISTENERTHREAD_H
#define NETWORKLISTENERTHREAD_H

#include "mambanet_network.h"
#include <QThread>

class NetworkListenerThread : public QThread
{
    Q_OBJECT
    public:
        NetworkListenerThread(MambaNetNetworkHandler *Handler);

    protected:
        void run();
		  void stop();

    private:
        MambaNetNetworkHandler * NetworkHandler;
};



#endif
