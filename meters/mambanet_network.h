#ifndef MAMBANETNETWORK_H
#define MAMBANETNETWORK_H

#include <QObject>
#include <QTimer>
#include <QSocketNotifier>
#include <stdio.h>
#include <string.h>         //for memcpy/strncpy
#include <unistd.h>         //for STDIN_FILENO/close/write
#include <fcntl.h>          //for GET_FL/SET_FL/O_XXXXXX/FNDELAY
#include <arpa/inet.h>      //for AF_PACKET/SOCK_DGRAM/htons/ntohs/socket/bind/sendto
#include <linux/if_arp.h>   //for ETH_P_ALL/ifreq/sockaddr_ll/ETH_ALEN etc...
#include <sys/ioctl.h>          //for ioctl

#include "mambanet_stack_axum.h"

class MambaNetNetworkHandler : public QObject
{
	 	  Q_OBJECT
    public:
		  int LinuxIfIndex;
        int MambaNetSocket;
		  QSocketNotifier *NetworkNotifier;

        MambaNetNetworkHandler();
        ~MambaNetNetworkHandler();

		  void InitializeTheMambaNetStack(DEFAULT_NODE_OBJECTS_STRUCT *NewDefaultObjects, CUSTOM_OBJECT_INFORMATION_STRUCT *NewCustomObjectInformation, unsigned int NewNumberOfCustomObjects);

        int OpenDevice(char *Name);

        void StartListenOnNetwork();

        void StopListenOnNetwork();

	public slots:
		  void SecondTimerTick(void);
		  void ReadNetwork();

    private:
        bool ExitListenLoop;

        int EthernetInterfaceIndex;


        char LocalMACAddress[6];

//        int cntEthernetReceiveBufferTop;
//        int cntEthernetReceiveBufferBottom;
        int cntEthernetMambaNetDecodeBuffer;

//        unsigned char EthernetReceiveBuffer[4096];
        unsigned char EthernetMambaNetDecodeBuffer[128];

};


#endif
