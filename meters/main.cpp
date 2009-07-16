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

#include <unistd.h>/* for close */
#include <fcntl.h>/* for open */
#include <sys/ioctl.h>/* for ioctl */
#include <sys/mman.h>/* for mmap */
#include <stdio.h>

#include <QtCore>
#include <QtGui>
#include <QtSql>

#include <ddpci2040.h>
#include <browser.h>

#include <errno.h>

#include "ThreadListener.h"
#include "mambanet_network.h"

#define PCB_MAJOR_VERSION        1
#define PCB_MINOR_VERSION        0

#define FIRMWARE_MAJOR_VERSION   0
#define FIRMWARE_MINOR_VERSION   1

#define MANUFACTURER_ID          0x0001	//D&R
#define PRODUCT_ID               0x0018	//Axum linux meters

#define NR_OF_STATIC_OBJECTS    (1029-1023)
#define NR_OF_OBJECTS            NR_OF_STATIC_OBJECTS

extern unsigned int AddressTableCount;

void EthernetMambaNetMessageTransmitCallback(unsigned char *buffer, unsigned char buffersize, unsigned char hardware_address[16]);
void EthernetMambaNetMessageReceiveCallback(unsigned long int ToAddress, unsigned long int FromAddress, unsigned char Ack, unsigned long int MessageID, unsigned int MessageType, unsigned char *Data, unsigned char DataLength, unsigned char *FromHardwareAddress);
void EthernetMambaNetAddressTableChangeCallback(MAMBANET_ADDRESS_STRUCT *AddressTable, MambaNetAddressTableStatus Status, int Index);

Browser *browser = NULL;

/********************************/
/* global declarations          */
/********************************/
DEFAULT_NODE_OBJECTS_STRUCT AxumPPMMeterDefaultObjects =
{
   "Axum PPM Meters (Linux)",				   //Description
   "Axum-PPM-Meters",			            //Name is stored in EEPROM, see above
   MANUFACTURER_ID,                       //ManufacturerID
   PRODUCT_ID,                            //ProductID
  	1,                                     //UniqueIDPerProduct (1=fader)
   PCB_MAJOR_VERSION,                     //HardwareMajorRevision
   PCB_MINOR_VERSION,                     //HardwareMinorRevision
   FIRMWARE_MAJOR_VERSION,                //FirmwareMajorRevision
   FIRMWARE_MINOR_VERSION,                //FirmwareMinorRevision
   PROTOCOL_MAJOR_VERSION,                //ProtocolMajorRevision
   PROTOCOL_MINOR_VERSION,                //ProtocolMinorRevision
   NR_OF_OBJECTS,                         //NumberOfObjects
   0x00000000,                            //DefaultEngineAddress
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  //Parent
   0x00000000,                            //MambaNetAddress
	0x00												//Services
};

#if (NR_OF_OBJECTS != 0)
CUSTOM_OBJECT_INFORMATION_STRUCT AxumPPMMeterCustomObjectInformation[NR_OF_STATIC_OBJECTS] =
{
   // Description             , Services
   //                         , sensor {type, size, min, max}
   //                         , actuator {type, size, min, max, default}
   { "Meter 1 Left dB"        , 0x00, 0x00000000, 0x00
                              , {NO_DATA_DATATYPE           ,  0, 0     , 0      }
                              , {FLOAT_DATATYPE             ,  2, 0xD240, 0x4900, 0xD240 }},
   { "Meter 1 Right dB"       , 0x00, 0x00000000, 0x00
                              , {NO_DATA_DATATYPE           ,  0, 0     , 0      }
                              , {FLOAT_DATATYPE             ,  2, 0xD240, 0x4900, 0xD240 }},
   { "Meter 1 Label"          , 0x00, 0x00000000, 0x00
                              , {NO_DATA_DATATYPE           ,  0, 0     , 0      }
                              , {OCTET_STRING_DATATYPE      ,  8, 0		, 127	  , 0		  }},
   { "Meter 2 Left dB"        , 0x00, 0x00000000, 0x00
                              , {NO_DATA_DATATYPE           ,  0, 0     , 0      }
                              , {FLOAT_DATATYPE             ,  2, 0xD240, 0x4900, 0xD240 }},
   { "Meter 2 Right dB"       , 0x00, 0x00000000, 0x00
                              , {NO_DATA_DATATYPE           ,  0, 0     , 0      }
                              , {FLOAT_DATATYPE             ,  2, 0xD240, 0x4900, 0xD240 }},
   { "Meter 2 Label"          , 0x00, 0x00000000, 0x00
                              , {NO_DATA_DATATYPE           ,  0, 0     , 0      }
                              , {OCTET_STRING_DATATYPE      ,  8, 0		, 127	  , 0		  }}
};
#else
CUSTOM_OBJECT_INFORMATION_STRUCT AxumPPMMeterCustomObjectInformation[1];
#endif


void addConnectionsFromCommandline(const QStringList &args, Browser *browser)
{
/*   for (int i = 1; i < args.count(); ++i) 
   {
      QUrl url(args.at(i), QUrl::TolerantMode);
      if (!url.isValid()) 
      {
         qWarning("Invalid URL: %s", qPrintable(args.at(i)));
         continue;
      }

      QSqlError err = browser->addConnection(url.scheme(), url.path().mid(1), url.host(), url.userName(), url.password(), url.port(-1));
      if (err.type() != QSqlError::NoError)
         qDebug() << "Unable to open connection:" << err;
   }*/
}

int delay_us(double sleep_time)
{
   struct timespec tv; 

   sleep_time /= 1000000;

   /* Construct the timespec from the number of whole seconds... */ 
   tv.tv_sec = (time_t) sleep_time; 
   /* ... and the remainder in nanoseconds. */ 
   tv.tv_nsec = (long) ((sleep_time - tv.tv_sec) * 1e+9);

   while (1)
   {
      /* Sleep for the time specified in tv. If interrupted by a signal, place the remaining time left to sleep back into tv. */
      int rval = nanosleep (&tv, &tv);
      if (rval == 0)   
      {
         /* Completed the entire sleep time; all done. */
         return 0;
      }
      else if (errno == EINTR)
      {
         /* Interrupted by a signal. Try again. */
         continue;
      }
      else
      {
         /* Some other error; bail out. */
         return rval;
      }
   }
   return 0;
}

int main(int argc, char *argv[])
{
	int UniqueIDPerProduct = -1;
	char FilenameUniqueID[64];
	char UniqueIDString[8];
	sprintf(FilenameUniqueID, "%s.dat", argv[0]);

	int FileHandleUniqueID = open(FilenameUniqueID, O_RDONLY);
	if (FileHandleUniqueID == -1)
	{
		perror(FilenameUniqueID);
		exit(1);
	}

	read(FileHandleUniqueID, UniqueIDString, 8);

	UniqueIDPerProduct = atoi(UniqueIDString);
	if ((UniqueIDPerProduct < 1) || (UniqueIDPerProduct > 65535))
	{
		fprintf(stderr, "Unique ID not found or out of range\n");
		exit(1);
	}
	AxumPPMMeterDefaultObjects.UniqueIDPerProduct = UniqueIDPerProduct;

   QApplication app(argc, argv);

   QMainWindow mainWin;
   browser = new Browser(&AxumPPMMeterDefaultObjects, AxumPPMMeterCustomObjectInformation, NR_OF_STATIC_OBJECTS, "eth0", &mainWin);
   mainWin.setCentralWidget(browser);


//	QWebView *test = new QWebView(browser->tabWidget);
//	test->load(QUrl("http://Service:Service@192.168.0.200/new/skin_table.html?file=main.php"));
//	test->show();
   
//   addConnectionsFromCommandline(app.arguments(), browser);
	mainWin.showFullScreen();
	mainWin.setGeometry(0,0, 1024,800);

//   QObject::connect(browser, SIGNAL(statusMessage(QString)), mainWin.statusBar(), SLOT(showMessage(QString)));

   return app.exec();

	printf("\n");
}

void EthernetMambaNetMessageTransmitCallback(unsigned char *buffer, unsigned char buffersize, unsigned char hardware_address[16])
{
//	 printf("Transmit size: %d, address %08X:%08X:%08X:%08X:%08X:%08X\n", buffersize, hardware_address[0], hardware_address[1], hardware_address[2], hardware_address[3], hardware_address[4], hardware_address[5]);

    //Setup the socket datastruct to transmit data.
	
	 if (browser != NULL)
	 {
    struct sockaddr_ll socket_address;
    socket_address.sll_family   = AF_PACKET;
    socket_address.sll_protocol = htons(ETH_P_DNR);
    socket_address.sll_ifindex  = browser->LinuxIfIndex;
    socket_address.sll_hatype    = ARPHRD_ETHER;
    socket_address.sll_pkttype  = PACKET_OTHERHOST;
    socket_address.sll_halen     = ETH_ALEN;
    socket_address.sll_addr[0]  = hardware_address[0];
    socket_address.sll_addr[1]  = hardware_address[1];
    socket_address.sll_addr[2]  = hardware_address[2];
    socket_address.sll_addr[3]  = hardware_address[3];
    socket_address.sll_addr[4]  = hardware_address[4];
    socket_address.sll_addr[5]  = hardware_address[5];
    socket_address.sll_addr[6]  = 0x00;
    socket_address.sll_addr[7]  = 0x00;

    if (browser->MambaNetSocket >= 0)
    {
        int CharactersSend = 0;
        unsigned char cntRetry=0;

        while ((CharactersSend < buffersize) && (cntRetry<10))
        {
            CharactersSend = sendto(browser->MambaNetSocket, buffer, buffersize, 0, (struct sockaddr *) &socket_address, sizeof(socket_address));

            if (cntRetry != 0)
            {
                printf("[NET] Retry %d\r\n", cntRetry++);
            }
        }

        if (CharactersSend < 0)
        {
            perror("cannot send data.\r\n");
        }
        else
        {
//          TRACE_PACKETS("[ETHERNET] SendMambaMessage(0x%08X, 0x%08X, 0x%04X, %d)", ToAddress, FromAddress, MessageType, DataLength);
        }
    }
	 }
}

void EthernetMambaNetMessageReceiveCallback(unsigned long int ToAddress, unsigned long int FromAddress, unsigned char Ack, unsigned long int MessageID, unsigned int MessageType, unsigned char *Data, unsigned char DataLength, unsigned char *FromHardwareAddress)
{
	bool MessageProcessed = false;

   switch (MessageType)
   {
      case 0x0000:
      {
         MessageProcessed = 1;
      }
      break;
      case 0x0001:
      {
         int ObjectNr;
         unsigned char Action;

         ObjectNr = Data[0];
         ObjectNr <<=8;
         ObjectNr |= Data[1];
         Action  = Data[2];

			//printf("message received in MambaNetMessageReceiveCallback(0x%08lx, 0x%08lx, 0x%06lx, 0x%04x, %d) ObjectNr:%d\n", ToAddress, FromAddress, MessageID, MessageType, DataLength, ObjectNr);

         if (Action == MAMBANET_OBJECT_ACTION_GET_ACTUATOR_DATA)
         {
         }
         else if (Action == MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA)
         {
	         switch (ObjectNr)
            {
               case 1024:
               {
                  if ((Data[3] == FLOAT_DATATYPE) && (Data[4]==2))
                  {
                     float Value;
                     if (VariableFloat2Float(&Data[5], Data[4], &Value) == 0)
                     {
                        float dB = Value+20;

	                     browser->MeterData[0] = dB;
								if (dB>browser->NewDNRPPMMeter->FdBPosition)
								{
									browser->NewDNRPPMMeter->FdBPosition = dB;
									browser->NewDNRPPMMeter->update();
								}
                     }
                  }
               }
               break;
               case 1025:
               {
                  if (Data[3] == FLOAT_DATATYPE)
                  {
                     float Value;
                     if (VariableFloat2Float(&Data[5], Data[4], &Value) == 0)
                     {
                        float dB = Value+20;

	                     browser->MeterData[1] = dB;
								if (dB>browser->NewDNRPPMMeter_2->FdBPosition)
								{
									browser->NewDNRPPMMeter_2->FdBPosition = dB;
//									browser->NewDNRPPMMeter_2->repaint();
									browser->NewDNRPPMMeter_2->update();
								}
                     }
                  }
               }
               break;
               case 1026:
               {
                  if (Data[3] == OCTET_STRING_DATATYPE)
                  {
							char TempString[256] = "";            
							strncpy(TempString, (char *)&Data[5], Data[4]);
							TempString[Data[4]] = 0;

	                  browser->label_3->setText(QString(TempString));
						}
					}
					break;
               case 1027:
               {
                  if ((Data[3] == FLOAT_DATATYPE) && (Data[4]==2))
                  {
                     float Value;
                     if (VariableFloat2Float(&Data[5], Data[4], &Value) == 0)
                     {
                        float dB = Value+20;

	                     browser->MeterData[2] = dB;
								if (dB>browser->NewDNRPPMMeter_3->FdBPosition)
								{
									browser->NewDNRPPMMeter_3->FdBPosition = dB;
//									browser->NewDNRPPMMeter_3->repaint();
									browser->NewDNRPPMMeter_3->update();
								}
                     }
                  }
               }
               break;
               case 1028:
               {
                  if (Data[3] == FLOAT_DATATYPE)
                  {
                     float Value;
                     if (VariableFloat2Float(&Data[5], Data[4], &Value) == 0)
                     {
                        float dB = Value+20;

	                     browser->MeterData[3] = dB;
								if (dB>browser->NewDNRPPMMeter_4->FdBPosition)
								{
									browser->NewDNRPPMMeter_4->FdBPosition = dB;
//									browser->NewDNRPPMMeter_4->repaint();
									browser->NewDNRPPMMeter_4->update();
								}
                     }
                  }
               }
               break;
					case 1029:
					{
						char TempString[256] = "";            
						strncpy(TempString, (char *)&Data[5], Data[4]);
						TempString[Data[4]] = 0;

                  browser->label_4->setText(QString(TempString));
					}
					break;
            }
	         MessageProcessed = 1;
         }
      }
      break;
      case MAMBANET_DEBUG_MESSAGETYPE:
      {
      }
      break;
   }

   if (!MessageProcessed)
   {
		printf("Unkown message received in MambaNetMessageReceiveCallback(0x%08lx, 0x%08lx, 0x%06lx, 0x%04x, %d)\n", ToAddress, FromAddress, MessageID, MessageType, DataLength);
   }
}

void EthernetMambaNetAddressTableChangeCallback(MAMBANET_ADDRESS_STRUCT *AddressTable, MambaNetAddressTableStatus Status, int Index)
{
	printf("AddressTableChange: 0x%08lX\n", AddressTable[Index].MambaNetAddress);
}

