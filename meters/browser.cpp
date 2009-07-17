/****************************************************************************
**
** Copyright (C) 2004-2006 Trolltech ASA. All rights reserved.
**
** This file is part of the demonstration applications of the Qt Toolkit.
**
** This file may be used under the terms of the GNU General Public
** License version 2.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of
** this file.  Please review the following information to ensure GNU
** General Public Licensing requirements will be met:
** http://www.trolltech.com/products/qt/opensource.html
**
** If you are unsure which license is appropriate for your use, please
** review the following information:
** http://www.trolltech.com/products/qt/licensing.html or contact the
** sales department at sales@trolltech.com.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

#include "browser.h"

#include <QtCore>
#include <QtGui>
#include <QtSql>
#include <qthread.h>

#include "chasewidget.h"
//#include "toolbarsearch.h"

extern void EthernetMambaNetMessageTransmitCallback(unsigned char *buffer, unsigned char buffersize, unsigned char hardware_address[16]);
extern void EthernetMambaNetMessageReceiveCallback(unsigned long int ToAddress, unsigned long int FromAddress, unsigned char Ack, unsigned long int MessageID, unsigned int MessageType, unsigned char *Data, unsigned char DataLength, unsigned char *FromHardwareAddress);
extern void EthernetMambaNetAddressTableChangeCallback(MAMBANET_ADDRESS_STRUCT *AddressTable, MambaNetAddressTableStatus Status, int Index);

Browser::Browser(DEFAULT_NODE_OBJECTS_STRUCT *NewDefaultObjects, CUSTOM_OBJECT_INFORMATION_STRUCT *NewCustomObjectInformation, unsigned int NewNumberOfCustomObjects, char *InterfaceName, QWidget *parent)
    : QWidget(parent)
{
   setupUi(this);

/*	m_NavigationBar = new QToolBar(label_7);

	m_Back = new QAction(label_7);
	m_Back->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));

	m_Forward = new QAction(label_7);
	m_Forward->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));

	m_StopReload = new QAction(label_7);
	m_StopReload->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));

//	m_ToolbarSearch = new ToolbarSearch(m_NavigationBar);

	m_ChaseWidget = new ChaseWidget(label_7);

	m_NavigationBar->addAction(m_Back);
	m_NavigationBar->addAction(m_Forward);
	m_NavigationBar->addAction(m_StopReload);
//	m_NavigationBar->addWidget(m_ToolbarSearch);
	m_NavigationBar->addWidget(m_ChaseWidget);

//	connect(m_ToolbarSearch, SIGNAL(search(const QUrl&)), SLOT(loadUrl(const QUrl&)));
	connect(m_Back, SIGNAL(triggered()), webView, SLOT(back()));
	connect(m_Forward, SIGNAL(triggered()), webView, SLOT(forward()));
	connect(m_StopReload, SIGNAL(triggered()), webView, SLOT(reload()));*/

	cntSecond = 0;

	startTimer(10);

	MeterData[0] = -60;
	MeterData[1] = -60;
	MeterData[2] = -60;
	MeterData[3] = -60;

   LocalMACAddress[0] = 0x00;
   LocalMACAddress[1] = 0x00;
   LocalMACAddress[2] = 0x00;
   LocalMACAddress[3] = 0x00;
   LocalMACAddress[4] = 0x00;
   LocalMACAddress[5] = 0x00;

	cntEthernetMambaNetDecodeBuffer = 0;

   MambaNetSocket = -1;
   EthernetInterfaceIndex = -1;

	InitializeMambaNetStack(NewDefaultObjects, NewCustomObjectInformation, NewNumberOfCustomObjects);

	OpenDevice(InterfaceName);
}

Browser::~Browser()
{
    if (EthernetInterfaceIndex != -1)
    {
        UnregisterMambaNetInterface(EthernetInterfaceIndex);
    }

    if (MambaNetSocket >= 0)
    {
        ::close(MambaNetSocket);
    }}

int Browser::OpenDevice(char *InterfaceName)
{
    bool error = 0;
    ifreq ethreq;

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
        ::close(MambaNetSocket);
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

void Browser::ReadNetwork()
{
	 long cnt;
    unsigned char buffer[20480];
    sockaddr_ll fromAddress;
    int fromlen = sizeof(fromAddress);
	 long numRead;

            if (MambaNetSocket>=0)
            {   //Test if the network generated an event.
                    numRead = recvfrom(MambaNetSocket, buffer, 20480, 0, (struct sockaddr *)&fromAddress, (socklen_t *)&fromlen);

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
										  unsigned char ReadedByte;
                                for (cnt=0; cnt<numRead; cnt++)
                                {
                                    ReadedByte = buffer[cnt];

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
                                }
                            }
                        }
                        numRead = recvfrom(MambaNetSocket, buffer, 20480, 0, (struct sockaddr *)&fromAddress, (socklen_t *)&fromlen);
                    }
            }
}

void Browser::SecondTimerTick()
{
	MambaNetReservationTimerTick();
}




void Browser::timerEvent(QTimerEvent *Event)
{
	cntSecond++;
	if (cntSecond>100)
	{
		cntSecond=0;
		MambaNetReservationTimerTick();
	}

	MeterRelease();
}

void Browser::MeterRelease()
{
/*
#ifdef Q_OS_WIN32
	LARGE_INTEGER freq, newTime;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&newTime);
	double newNumberOfSeconds = (double)newTime.QuadPart/freq.QuadPart;
#else
   timespec newTime;
   clock_gettime(CLOCK_MONOTONIC, &newTime);
   double newNumberOfSeconds = newTime.tv_sec+((double)newTime.tv_nsec/1000000000);
#endif
   double elapsedTime = newNumberOfSeconds - previousNumberOfSeconds;

	previousNumberOfSeconds = newNumberOfSeconds;
//	printf("time:%g - delta: %g\n", newNumberOfSeconds, elapsedTime);
*/

   if (NewDNRPPMMeter->FdBPosition>-50)
   {     
		if (MeterData[0] < NewDNRPPMMeter->FdBPosition)
      {         
			NewDNRPPMMeter->FdBPosition-=0.15;      
//			NewDNRPPMMeter->repaint();
			NewDNRPPMMeter->update();
		}
	}

   if (NewDNRPPMMeter_2->FdBPosition>-50)
   {     
		if (MeterData[1] < NewDNRPPMMeter_2->FdBPosition)
      {         
			NewDNRPPMMeter_2->FdBPosition-=0.15;      
//			NewDNRPPMMeter_2->repaint();
			NewDNRPPMMeter_2->update();
		}
	}   

   if (NewDNRPPMMeter_3->FdBPosition>-50)
   {     
		if (MeterData[2] < NewDNRPPMMeter_3->FdBPosition)
      {         
			NewDNRPPMMeter_3->FdBPosition-=0.15;      
			NewDNRPPMMeter_3->update();
//			NewDNRPPMMeter_3->repaint();
		}   
	}

   if (NewDNRPPMMeter_4->FdBPosition>-50)
   {     
		if (MeterData[3] < NewDNRPPMMeter_4->FdBPosition)
      {         
			NewDNRPPMMeter_4->FdBPosition-=0.15;      
			NewDNRPPMMeter_4->update();
//			NewDNRPPMMeter_4->repaint();
		}   
	}
}

/* for webbrowsing */
//void Browser::slotLoadProgress(int progress)
//{
//	if (progress < 100 && progress > 0) 
//	{
//		m_chaseWidget->setAnimated(true);
      
/*		disconnect(m_stopReload, SIGNAL(triggered()), m_reload, SLOT(trigger()));
      if (m_stopIcon.isNull())
      	m_stopIcon = style()->standardIcon(QStyle::SP_BrowserStop);
      m_stopReload->setIcon(m_stopIcon);
     	connect(m_stopReload, SIGNAL(triggered()), m_stop, SLOT(trigger()));
		m_stopReload->setToolTip(tr("Stop loading the current page"));*/
//   } 
//	else 
//	{
//		m_chaseWidget->setAnimated(false);
		
/*		disconnect(m_stopReload, SIGNAL(triggered()), m_stop, SLOT(trigger()));
		m_stopReload->setIcon(m_reloadIcon);
		connect(m_stopReload, SIGNAL(triggered()), m_reload, SLOT(trigger()));
		m_stopReload->setToolTip(tr("Reload the current page"));*/
//	}
//}


