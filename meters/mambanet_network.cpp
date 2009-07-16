/****************************************************************************
**
** Copyright (C) 2007-2009 D&R Electronica Weesp B.V. All rights reserved.
**
** This file is part of the Axum/MambaNet digital mixing system.
**
** This file may be used under the terms of the GNU General Public
** License version 2.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of
** this file.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

#include "mambanet_network.h"

extern void EthernetMambaNetMessageTransmitCallback(unsigned char *buffer, unsigned char buffersize, unsigned char hardware_address[16]);
extern void EthernetMambaNetMessageReceiveCallback(unsigned long int ToAddress, unsigned long int FromAddress, unsigned char Ack, unsigned long int MessageID, unsigned int MessageType, unsigned char *Data, unsigned char DataLength, unsigned char *FromHardwareAddress);
extern void EthernetMambaNetAddressTableChangeCallback(MAMBANET_ADDRESS_STRUCT *AddressTable, MambaNetAddressTableStatus Status, int Index);

MambaNetNetworkHandler::MambaNetNetworkHandler() : QObject()
{
    ExitListenLoop = false;
    LocalMACAddress[0] = 0x00;
    LocalMACAddress[1] = 0x00;
    LocalMACAddress[2] = 0x00;
    LocalMACAddress[3] = 0x00;
    LocalMACAddress[4] = 0x00;
    LocalMACAddress[5] = 0x00;

//    cntEthernetReceiveBufferTop = 0;
//    cntEthernetReceiveBufferBottom = 0;
    cntEthernetMambaNetDecodeBuffer = 0;

	QTimer *timer = new QTimer(this);
   connect(timer, SIGNAL(timeout()), this, SLOT(SecondTimerTick()));
   timer->start(1000);

    MambaNetSocket = -1;
    EthernetInterfaceIndex = -1;
}

MambaNetNetworkHandler::~MambaNetNetworkHandler()
{
    ExitListenLoop = true;

    if (EthernetInterfaceIndex != -1)
    {
        UnregisterMambaNetInterface(EthernetInterfaceIndex);
    }

    if (MambaNetSocket >= 0)
    {
        close(MambaNetSocket);
    }
}

void MambaNetNetworkHandler::InitializeTheMambaNetStack(DEFAULT_NODE_OBJECTS_STRUCT *NewDefaultObjects, CUSTOM_OBJECT_INFORMATION_STRUCT *NewCustomObjectInformation, unsigned int NewNumberOfCustomObjects)
{
	InitializeMambaNetStack(NewDefaultObjects, NewCustomObjectInformation, NewNumberOfCustomObjects);
//	ChangeMambaNetStackTrace(3);
}

int MambaNetNetworkHandler::OpenDevice(char *InterfaceName)
{
    bool error = 0;
    ifreq ethreq;

//    cntEthernetReceiveBufferTop = 0;
//    cntEthernetReceiveBufferBottom = 0;
    cntEthernetMambaNetDecodeBuffer = 0;

    //Open a socket for PACKETs if Datagram. This means we receive
    //only the data (without the header -> in our case without ethernet headers).
    //this socket receives all protocoltypes of ethernet (ETH_P_ALL)
    //ETH_P_ALL is required else we could not receive outgoing
    //packets sended by other processes else we could use ETH_P_DNR
//  int MambaNetSocket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    MambaNetSocket = socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_DNR));
    if (MambaNetSocket<0)
    {
        perror("Open MambaNet socket failed");
        error = 1;
    }

    // Get index of interface identified by NetworkInterface.
	 printf("Try to open:");
	 printf(InterfaceName);
	 printf("\n");
    strncpy(ethreq.ifr_name,InterfaceName,IFNAMSIZ);
    if (ioctl(MambaNetSocket,SIOCGIFFLAGS,&ethreq) < 0)
    {
        perror("could not find network interface");
        error = 1;
    }
    else
    {
        //printf("%X\r\n", ethreq.ifr_flags);
    }

    if (ioctl(MambaNetSocket, SIOCGIFINDEX, &ethreq) < 0)
    {
        perror("could not get the index of the network interface");
        error = 1;
    }
    else
    {
        //printf("network interface %s is located at index %d\r\n", NetworkInterface, ethreq.ifr_ifindex);
        LinuxIfIndex = ethreq.ifr_ifindex;
    }

    if (ioctl(MambaNetSocket, SIOCGIFHWADDR, &ethreq) < 0)
    {
        perror("could not get the hardware address of the network interface");
        error = 1;
    }
    else
    {
        printf("Network hardware address: %02X:%02X:%02X:%02X:%02X:%02X\r\n", (unsigned char)ethreq.ifr_hwaddr.sa_data[0], (unsigned char)ethreq.ifr_hwaddr.sa_data[1], (unsigned char)ethreq.ifr_hwaddr.sa_data[2], (unsigned char)ethreq.ifr_hwaddr.sa_data[3], (unsigned char)ethreq.ifr_hwaddr.sa_data[4], (unsigned char)ethreq.ifr_hwaddr.sa_data[5]);
        LocalMACAddress[0] = ethreq.ifr_hwaddr.sa_data[0];
        LocalMACAddress[1] = ethreq.ifr_hwaddr.sa_data[1];
        LocalMACAddress[2] = ethreq.ifr_hwaddr.sa_data[2];
        LocalMACAddress[3] = ethreq.ifr_hwaddr.sa_data[3];
        LocalMACAddress[4] = ethreq.ifr_hwaddr.sa_data[4];
        LocalMACAddress[5] = ethreq.ifr_hwaddr.sa_data[5];
    }

    //Use the socket and interface to listen to the Ethernet, so bind the socket with the interface.
    struct sockaddr_ll socket_address;
    socket_address.sll_family   = AF_PACKET;
    socket_address.sll_protocol = htons(ETH_P_ALL);
//  socket_address.sll_protocol = htons(ETH_P_DNR);
    socket_address.sll_ifindex = LinuxIfIndex;
    if (bind(MambaNetSocket, (struct sockaddr *) & socket_address, sizeof(socket_address)) < 0)
    {
        perror("Error binding socket");
        error = 1;
    }

    if (error)
    {
        close(MambaNetSocket);
        MambaNetSocket = -1;
    }
    else
    {
       INTERFACE_PARAMETER_STRUCT InterfaceParameters;
       InterfaceParameters.Type = ETHERNET;
       InterfaceParameters.HardwareAddress[0] = LocalMACAddress[0];
       InterfaceParameters.HardwareAddress[1] = LocalMACAddress[1];
       InterfaceParameters.HardwareAddress[2] = LocalMACAddress[2];
       InterfaceParameters.HardwareAddress[3] = LocalMACAddress[3];
       InterfaceParameters.HardwareAddress[4] = LocalMACAddress[4];
       InterfaceParameters.HardwareAddress[5] = LocalMACAddress[5];
       InterfaceParameters.TransmitCallback = EthernetMambaNetMessageTransmitCallback;
       InterfaceParameters.ReceiveCallback = EthernetMambaNetMessageReceiveCallback;
       InterfaceParameters.AddressTableChangeCallback = EthernetMambaNetAddressTableChangeCallback;
       InterfaceParameters.TransmitFilter = ALL_MESSAGES;
       InterfaceParameters.cntMessageReceived = 0;
       InterfaceParameters.cntMessageTransmitted = 0;

       EthernetInterfaceIndex = RegisterMambaNetInterface(&InterfaceParameters);
    }

    if (fcntl(MambaNetSocket, F_SETOWN, getpid()) == -1)
    {
        perror("F_SETOWN");
    }

    // Get the currently set flags for stdin, remember them to set back later.
    int oldflags = fcntl(MambaNetSocket, F_GETFL);
    if (oldflags < 0)
    {
        perror("error in fcntl(MambaNetSocket, F_GETFL)");
    }

    // Set the O_NONBLOCK flag for stdin
    if (fcntl(MambaNetSocket, F_SETFL, oldflags | FNONBLOCK | FNDELAY)<0)
    {
        perror("error in fcntl(MambaNetSocket, F_SETFL)");
    }

	 NetworkNotifier = new QSocketNotifier(MambaNetSocket, QSocketNotifier::Read, this);
	 connect(NetworkNotifier, SIGNAL(activated(int)), this, SLOT(ReadNetwork()));

	 return error;
}

void MambaNetNetworkHandler::ReadNetwork()
{
	printf("Must read\n");

	 int cnt;
    unsigned char buffer[2048];
    sockaddr_ll fromAddress;
    int fromlen = sizeof(fromAddress);
	 int numRead;

            if (MambaNetSocket>=0)
            {   //Test if the network generated an event.
                    numRead = recvfrom(MambaNetSocket, buffer, 2048, 0, (struct sockaddr *)&fromAddress, (socklen_t *)&fromlen);

	                 while (numRead > 0)
                    {   // bytes received, check the ethernet protocol.
                        if (ntohs(fromAddress.sll_protocol) == ETH_P_DNR)
                        {
                            char AllowedToProcess = 1;
                                //If MambaNet over ethernet
                                //eventual you can check the type of a packet
                                //fromAddress.sll_pkttype:
                                //PACKET_HOST
                                //PACKET_BROADCAST
                                //PACKET_MULTICAST
                                //PACKET_OTHERHOST:
                                //PACKET_OUTGOING:
//                              if (fromAddress.sll_pkttype == PACKET_OUTGOING)
//                              {
//                                  //Check MAC-Address, if not addressed to us drop it.
//                                  printf("%02X:%02X:%02X:%02X:%02X:%02X\r\n", fromAddress.sll_addr[0], fromAddress.sll_addr[1], fromAddress.sll_addr[2], fromAddress.sll_addr[3], fromAddress.sll_addr[4], fromAddress.sll_addr[5]);
//                                  if  (   !(  (LocalMACAddress[0] == fromAddress.sll_addr[0]) &&
//                                              (LocalMACAddress[1] == fromAddress.sll_addr[1]) &&
//                                              (LocalMACAddress[2] == fromAddress.sll_addr[2]) &&
//                                              (LocalMACAddress[3] == fromAddress.sll_addr[3]) &&
//                                              (LocalMACAddress[4] == fromAddress.sll_addr[4]) &&
//                                              (LocalMACAddress[5] == fromAddress.sll_addr[5])))
//                                  {
//                                      AllowedToProcess = 0;
//                                  }
//                              }

                            if (AllowedToProcess)
                            {
                                for (cnt=0; cnt<numRead; cnt++)
                                {
                                    unsigned char ReadedByte = buffer[cnt];

												switch (ReadedByte&0xC0)
												{
													case 0x80: //0x80, 0x81, 0x82
													{
                                            cntEthernetMambaNetDecodeBuffer = 0;
                                            EthernetMambaNetDecodeBuffer[cntEthernetMambaNetDecodeBuffer++] = ReadedByte;
                                       }
                                       break;
													case 0xC0: //0xFF
													{
														if (ReadedByte == 0xFF)
														{
                                       		EthernetMambaNetDecodeBuffer[cntEthernetMambaNetDecodeBuffer++] = ReadedByte;
                                       		DecodeRawMambaNetMessage(EthernetMambaNetDecodeBuffer, cntEthernetMambaNetDecodeBuffer, EthernetInterfaceIndex, fromAddress.sll_addr);
														}
                                       }
                                       break;
                                       default:
                                       {
                                       	EthernetMambaNetDecodeBuffer[cntEthernetMambaNetDecodeBuffer++] = ReadedByte;
                                       }
                                       break;
                                    }
												if (cntEthernetMambaNetDecodeBuffer>128)
												{
													cntEthernetMambaNetDecodeBuffer=0;
												}
                                }
                            }
                        }
                        numRead = recvfrom(MambaNetSocket, buffer, 2048, 0, (struct sockaddr *)&fromAddress, (socklen_t *)&fromlen);
                    }
            }
}

void MambaNetNetworkHandler::StartListenOnNetwork(){
/*    fd_set readfs;
	 int maxfd = MambaNetSocket+1;

	 int cnt;
    unsigned char buffer[2048];
    sockaddr_ll fromAddress;
    int fromlen = sizeof(fromAddress);
	 int numRead;

    ExitListenLoop = false;
	 printf("Start\n");

    while (!ExitListenLoop)
    {
        FD_ZERO(&readfs);
        if (MambaNetSocket >= 0)
        {
            FD_SET(MambaNetSocket, &readfs);
        }

        int ReturnValue = select(maxfd, &readfs, NULL, NULL, NULL);
//                int ReturnValue = pselect(maxfd, &readfs, NULL, NULL, NULL, &old_sigmask);

        if (ReturnValue == -1)
        {//no error or non-blocked signal)
           //perror("pselect:");
        }
        if (ReturnValue != -1)
        {//no error or non-blocked signal)
            if (MambaNetSocket>=0)
            {   //Test if the network generated an event.
                if (FD_ISSET(MambaNetSocket, &readfs))
                {
                    numRead = recvfrom(MambaNetSocket, buffer, 2048, 0, (struct sockaddr *)&fromAddress, (socklen_t *)&fromlen);

	                 while (numRead > 0)
                    {   // bytes received, check the ethernet protocol.
                        if (ntohs(fromAddress.sll_protocol) == ETH_P_DNR)
                        {
                            char AllowedToProcess = 1;
                                //If MambaNet over ethernet
                                //eventual you can check the type of a packet
                                //fromAddress.sll_pkttype:
                                //PACKET_HOST
                                //PACKET_BROADCAST
                                //PACKET_MULTICAST
                                //PACKET_OTHERHOST:
                                //PACKET_OUTGOING:
//                              if (fromAddress.sll_pkttype == PACKET_OUTGOING)
//                              {
//                                  //Check MAC-Address, if not addressed to us drop it.
//                                  printf("%02X:%02X:%02X:%02X:%02X:%02X\r\n", fromAddress.sll_addr[0], fromAddress.sll_addr[1], fromAddress.sll_addr[2], fromAddress.sll_addr[3], fromAddress.sll_addr[4], fromAddress.sll_addr[5]);
//                                  if  (   !(  (LocalMACAddress[0] == fromAddress.sll_addr[0]) &&
//                                              (LocalMACAddress[1] == fromAddress.sll_addr[1]) &&
//                                              (LocalMACAddress[2] == fromAddress.sll_addr[2]) &&
//                                              (LocalMACAddress[3] == fromAddress.sll_addr[3]) &&
//                                              (LocalMACAddress[4] == fromAddress.sll_addr[4]) &&
//                                              (LocalMACAddress[5] == fromAddress.sll_addr[5])))
//                                  {
//                                      AllowedToProcess = 0;
//                                  }
//                              }

                            if (AllowedToProcess)
                            {
                                for (cnt=0; cnt<numRead; cnt++)
                                {
                                    unsigned char ReadedByte = buffer[cnt];

                                    switch (ReadedByte)
                                    {
                                        case 0x80:
                                        case 0x81:
                                        {
                                            cntEthernetMambaNetDecodeBuffer = 0;
                                            EthernetMambaNetDecodeBuffer[cntEthernetMambaNetDecodeBuffer++] = ReadedByte;
                                        }
                                        break;
                                        case 0xFF:
                                        {
                                            EthernetMambaNetDecodeBuffer[cntEthernetMambaNetDecodeBuffer++] = ReadedByte;
                                            DecodeRawMambaNetMessage(EthernetMambaNetDecodeBuffer, cntEthernetMambaNetDecodeBuffer, EthernetInterfaceIndex, fromAddress.sll_addr);
                                        }
                                        break;
                                        default:
                                        {
                                            EthernetMambaNetDecodeBuffer[cntEthernetMambaNetDecodeBuffer++] = ReadedByte;
                                        }
                                        break;
                                    }
												if (cntEthernetMambaNetDecodeBuffer>128)
												{
													cntEthernetMambaNetDecodeBuffer=0;
												}
                                }
                            }
                        }
                        numRead = recvfrom(MambaNetSocket, buffer, 2048, 0, (struct sockaddr *)&fromAddress, (socklen_t *)&fromlen);
                    }
                }
            }
        }
    }*/
}

void MambaNetNetworkHandler::StopListenOnNetwork()
{
    ExitListenLoop = true;
}

void MambaNetNetworkHandler::SecondTimerTick()
{
	MambaNetReservationTimerTick();
}

