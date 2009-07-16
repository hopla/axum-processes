/****************************************************************************
**
** Copyright (C) 2007-2008 D&R Electronica Weesp B.V. All rights reserved.
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

#include <stdio.h>
#include <string.h>			//for memset/memcpy/strncpy
#include "mambanet_stack_axum.h"

unsigned char TraceOptions;

#define TRACE_PACKETS(code)   \
if (TraceOptions & 0x01)      \
{                             \
   code                       \
}                             \

#define TRACE_ADDRESSTABLE(code)    \
if (TraceOptions & 0x02)            \
{                                   \
   code                             \
}                                   \

DEFAULT_NODE_OBJECTS_STRUCT *DefaultObjects;
CUSTOM_OBJECT_INFORMATION_STRUCT *CustomObjectInformation;

unsigned int AddressTableCount = 0;
unsigned int InterfaceCount = 0;
unsigned int NumberOfCustomObjects  = 0;

//Global counter to determine when to send an address reservation information packet
int timerReservationInfo = 1;

//Global variable to remember the index of the Network interface.
int cntMambaNetMessageReceivedFromCAN;
int cntMambaNetMessageReceivedFromUART;
int cntMambaNetMessageReceivedFromUART_CAN;
int cntMambaNetMessageReceivedFromEthernet;
int cntMambaNetMessageReceivedFromFirewire;
int cntMambaNetMessageReceivedFromTCPIP;

//Maximal 16 different interfaces can be hooked
#define INTERFACE_TABLE_SIZE  16
INTERFACE_PARAMETER_STRUCT Interfaces[INTERFACE_TABLE_SIZE];

//Currently in array (not in a list) the AddressReservation Table.
//This table stores to combination of:
// - UniqueMediaAccessID
// - MambaNet address
// - (local) Interface its reservations was received at.
// - Hardware address its reservations was send from.
#define ADDRESS_TABLE_SIZE 65536
MAMBANET_ADDRESS_STRUCT AddressTable[ADDRESS_TABLE_SIZE];

//Function should be implemented by the process using the MambaNet stack.
//extern void MambaNetMessageReceived(unsigned long int ToAddress, unsigned long int FromAddress, unsigned long int MessageID, unsigned int MessageType, unsigned char *Data, unsigned char DataLength, INTERFACE_PARAMETER_STRUCT *FromInterface, unsigned char *FromHardwareAddress=NULL);

//Function that initializes all global variables of the MambaNet stack.
void STDCALL InitializeMambaNetStack(DEFAULT_NODE_OBJECTS_STRUCT *NewDefaultObjects, CUSTOM_OBJECT_INFORMATION_STRUCT *NewCustomObjectInformation, unsigned int NewNumberOfCustomObjects)
{
   DefaultObjects = NewDefaultObjects;
   CustomObjectInformation = NewCustomObjectInformation;
   NumberOfCustomObjects = NewNumberOfCustomObjects;

	TraceOptions = 0x00;

	for (long cntAddress=0; cntAddress<ADDRESS_TABLE_SIZE; cntAddress++)
	{
		AddressTable[cntAddress].ManufacturerID				   = 0;
		AddressTable[cntAddress].ProductID						   = 0;
		AddressTable[cntAddress].UniqueIDPerProduct			   = 0;
		AddressTable[cntAddress].MambaNetAddress				   = 0x00000000;
      AddressTable[cntAddress].DefaultEngineMambaNetAddress = 0x00000000;
      AddressTable[cntAddress].NodeServices                 = 0x00;

      memset(AddressTable[cntAddress].HardwareAddress, 0x00, 16);
		AddressTable[cntAddress].ReceivedInterfaceIndex       = -1;
      AddressTable[cntAddress].Alive                        = 0;
	}
   AddressTableCount = 0;

   for (int cntInterface=0; cntInterface<INTERFACE_TABLE_SIZE; cntInterface++)
   {
   	Interfaces[cntInterface].Type = NO_INTERFACE;
   	memset(Interfaces[cntInterface].HardwareAddress, 0x00, 16);
      Interfaces[cntInterface].TransmitCallback = NULL;
      Interfaces[cntInterface].ReceiveCallback = NULL;
      Interfaces[cntInterface].AddressTableChangeCallback = NULL;
      Interfaces[cntInterface].TransmitFilter = ALL_MESSAGES;

      Interfaces[cntInterface].cntMessageReceived = 0;
      Interfaces[cntInterface].cntMessageTransmitted = 0;
   }
   InterfaceCount = 0;
}

//Function that initializes all global variables of the MambaNet stack.
void CloseMambaNetStack()
{
}

//This supporting function is used to convert 8 bits data to 7 bits data.
// 8 bits:
//    aaaa aaaa
//    bbbb bbbb
//		cccc cccc
//
//Wil be in 7 bits:
//    0aaa aaaa
//		0bbb bbba	<- 1 MSB bit of 'a'
//		0ccc ccbb	<- 2 MSB bits of 'b'
//		0000 0ccc	<- 3 MSB bits of 'c'
//
//All data within MambaNet messages is 7 bits
unsigned char Encode8to7bits(unsigned char *Buffer, unsigned char BufferLength, unsigned char *Buffer7Bit)
{
   unsigned char cntByte;
   unsigned char cntBuffer7Bit;
   unsigned char Mask1;
   unsigned char Mask2;
   unsigned char BitShift;

   cntBuffer7Bit = 0;
   Buffer7Bit[cntBuffer7Bit] = 0;
   for (cntByte=0; cntByte<BufferLength; cntByte++)
   {
      BitShift = cntByte%7;
      Mask1 = 0x7F>>BitShift;
      Mask2 = Mask1^0xFF;

      Buffer7Bit[cntBuffer7Bit++] |= (Buffer[cntByte]&Mask1)<<BitShift;
      Buffer7Bit[cntBuffer7Bit  ]  = (Buffer[cntByte]&Mask2)>>(7-BitShift);
      if (Mask2 == 0xFE)
      {
         cntBuffer7Bit++;
         Buffer7Bit[cntBuffer7Bit] = 0x00;
      }
   }
   if ((cntByte%7) != 0)
   {
      cntBuffer7Bit++;
   }

   return cntBuffer7Bit;
}

//This supporting function is used to convert 7 bits data to 8 bits data.
//7 bits:
//    0aaa aaaa
//		0bbb bbba	<- 1 MSB bit of 'a'
//		0ccc ccbb	<- 2 MSB bits of 'b'
//		0000 0ccc	<- 3 MSB bits of 'c'
//
//Will be in 8 bits:
//    aaaa aaaa
//    bbbb bbbb
//		cccc cccc
//
//All data within MambaNet messages is 7 bits
unsigned char Decode7to8bits(unsigned char *Buffer, unsigned char BufferLength, unsigned char *Buffer8Bit)
{
   unsigned char cntByte;
   unsigned char cntBuffer8Bit;
   unsigned char Mask1;
   unsigned char Mask2;

   cntBuffer8Bit = 0;
   Buffer8Bit[cntBuffer8Bit] = Buffer[0]&0x7F;
   for (cntByte=1; cntByte<BufferLength; cntByte++)
   {
      Mask1 = (0x7F>>(cntByte&0x07))<<(cntByte&0x07);
      Mask2 = Mask1^0x7F;

      if (Mask2 != 0x00)
      {
         Buffer8Bit[cntBuffer8Bit++] |= (Buffer[cntByte]&Mask2)<<(8-(cntByte&0x07));
      }
      Buffer8Bit[cntBuffer8Bit] = (Buffer[cntByte]&Mask1)>>(cntByte&0x07);
   }

   return cntBuffer8Bit;
}

//This is a global function to transmit data to a MambaNet node.
//Information required to give is:
//
//- ToAddress
//  is the MambaNet address used to determine (in his address table) the interface and
//  hardware adress used for transmitting this package
//
//- FromAddress
//  is the MambaNet address of the ORGINAL sender.
//
//- MessageType
//	 defines the format of the data payload.
//
//- Data
//	 pointer to the data which is in 8 bit (unsigned char) format.
//
//- DataLength
//	 the amount if 8 bits data bytes in the data buffer.
//
//- InterfaceToUse
//  default this is NULL and the function will determine the interface to use depeding
//  on the address table (stores via which interface a MambaNet node is registered).
//
//	 if assigned a value the message will be force to use the given interface
//  (this could be necessary for broadcast messages and gateways).
void STDCALL SendMambaNetMessage(unsigned long int ToAddress, unsigned long int FromAddress, unsigned char Ack, unsigned int MessageID, unsigned int MessageType, unsigned char *Data, unsigned char DataLength, int InterfaceIndexToUse)
{
   unsigned char TransmitMessageBuffer[MAX_MAMBANET_PACKET_SIZE];
   unsigned char cntTransmitMessageBuffer;
   unsigned char TransmitMessageBufferLength;

	//initialize the interface which should be used.
	INTERFACE_PARAMETER_STRUCT *InterfaceUsedForTransmit = NULL;

   //initialize physical address to use.
   unsigned char HardwareAddressToUse[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

   if ((DefaultObjects->Services&0x80) || (MessageType == MAMBANET_ADDRESS_RESERVATION_MESSAGETYPE))
   {
   	//if we have to much data, don't send it.
      if (DataLength <= MAX_MAMBANET_DATA_SIZE)
      {
         unsigned char cntByte;
         unsigned char MessageLength;
         unsigned char NewDataLength;

         cntTransmitMessageBuffer = 0;
         //StartOfMessage
			
			if ((MessageID) && (!((ToAddress>>28)&0x01)) && ((ToAddress&0x0FFFFFFF) != 0x00000000) && (Ack))
			{
	         TransmitMessageBuffer[cntTransmitMessageBuffer++] = 0x82;
			}
			else
			{
	         TransmitMessageBuffer[cntTransmitMessageBuffer++] = 0x80 | ((ToAddress>>28)&0x01);
			}

         //ToAddress
         TransmitMessageBuffer[cntTransmitMessageBuffer++] = (ToAddress>>21)&0x7F;   //7 bits
         TransmitMessageBuffer[cntTransmitMessageBuffer++] = (ToAddress>>14)&0x7F;   //7 bits
         TransmitMessageBuffer[cntTransmitMessageBuffer++] = (ToAddress>> 7)&0x7F;   //7 bits
         TransmitMessageBuffer[cntTransmitMessageBuffer++] =  ToAddress     &0x7F;   //7 bits
         //FromAddress
         TransmitMessageBuffer[cntTransmitMessageBuffer++] = (FromAddress>>21)&0x7F;   //7 bits
         TransmitMessageBuffer[cntTransmitMessageBuffer++] = (FromAddress>>14)&0x7F;   //7 bits
         TransmitMessageBuffer[cntTransmitMessageBuffer++] = (FromAddress>> 7)&0x7F;   //7 bits
         TransmitMessageBuffer[cntTransmitMessageBuffer++] =  FromAddress     &0x7F;   //7 bits

   		//MessageID
         TransmitMessageBuffer[cntTransmitMessageBuffer++] = (MessageID>>14)&0x7F;   //7 bits
         TransmitMessageBuffer[cntTransmitMessageBuffer++] = (MessageID>> 7)&0x7F;   //7 bits
         TransmitMessageBuffer[cntTransmitMessageBuffer++] =  MessageID     &0x7F;   //7 bits

         //MessageType
         TransmitMessageBuffer[cntTransmitMessageBuffer++] = (MessageType>>7)&0x7F;   //7 bits
         TransmitMessageBuffer[cntTransmitMessageBuffer++] = (MessageType   )&0x7F;   //7 bits
         //MessageLength
         NewDataLength = Encode8to7bits(Data, DataLength, &TransmitMessageBuffer[cntTransmitMessageBuffer+1]);
         MessageLength = (NewDataLength+PROTOCOL_OVERHEAD);
         TransmitMessageBuffer[cntTransmitMessageBuffer++] = NewDataLength;

         cntTransmitMessageBuffer += NewDataLength;

         //EndOfMessage
         TransmitMessageBuffer[cntTransmitMessageBuffer++] = 0xFF;
         TransmitMessageBufferLength = MessageLength;

   		//Clear the rest of the buffer
         for (cntByte=cntTransmitMessageBuffer; cntByte<MAX_MAMBANET_PACKET_SIZE; cntByte++)
         {
            TransmitMessageBuffer[cntByte] = 0x00;
         }

   		//Check if we are forces to use an interface.
   		if (InterfaceIndexToUse == -1)
   		{	//We have to determine the interface
   			long cnt;
   			bool Found;

   			cnt = 0;
   			Found = false;
   			while ((!Found) && (cnt<AddressTableCount))
   			{
   				//If Mamba address found in the list, store the interface.
   				if (ToAddress == AddressTable[cnt].MambaNetAddress)
   				{
                  memcpy(HardwareAddressToUse, AddressTable[cnt].HardwareAddress, 16);
                  if (AddressTable[cnt].ReceivedInterfaceIndex != -1)
                  {
   					   InterfaceUsedForTransmit = &Interfaces[AddressTable[cnt].ReceivedInterfaceIndex];
                  }
   					Found = true;
   				}
   				cnt++;
   			}
   		}
   		else
   		{	//We have to use the given interface
   			InterfaceUsedForTransmit = &Interfaces[InterfaceIndexToUse];
   		}

   		//Depening on the interface type we have to use different hardware addresses in the transmit functions
         if (InterfaceUsedForTransmit != NULL)
         {
            if (InterfaceUsedForTransmit->TransmitCallback != NULL)
            {
               unsigned char FilterActive;

               FilterActive = 0;
               switch (InterfaceUsedForTransmit->TransmitFilter)
               {
                  case ALL_MESSAGES:
                  {  //no filter
                     FilterActive = 0;
                  }
                  break;
                  case NO_RESERVATION_INFO:
                  {
                     if ((MessageType == 0) && (Data[0] == 0))
                     {  //Address reservation info
                        FilterActive = 1;
                     }
                  }
                  break;
                  case NO_BROADCASTS:
                  {
                     if (ToAddress == 0x10000000)
                     {
                        FilterActive = 1;
                     }
                  }
                  break;
                  case NO_RESERVATION_INFO_FROM_OTHERS:
                  {
                     if ((MessageType == 0) && (Data[0] == 0) && (FromAddress != DefaultObjects->MambaNetAddress))
                     {  //Address reservation info that originates from an other node (we are possible a gateway)
                        FilterActive = 1;
                     }
                  }
                  break;
                  case NO_BROADCASTS_FROM_OTHERS:
                  {
                     if ((ToAddress == 0x10000000) && (FromAddress != DefaultObjects->MambaNetAddress))
                     {  //Broadcasts that originates from an other node (we are possible a gateway)
                        FilterActive = 1;
                     }
                  }
                  break;
               }

               if (!FilterActive)
               {
            		switch (InterfaceUsedForTransmit->Type)
            		{
                     case CAN:
                     {
                        TRACE_PACKETS(printf("[CAN] SendMambaMessage(0x%08X, 0x%08X, 0x%04X, %d)\n", ToAddress, FromAddress, MessageType, DataLength);)
                     }
                     break;
            			case UART:
            			{
                        InterfaceUsedForTransmit->TransmitCallback(TransmitMessageBuffer, TransmitMessageBufferLength, NULL);

            				TRACE_PACKETS(printf("[UART] SendMambaMessage(0x%08X, 0x%08X, 0x%04X, %d)\n", ToAddress, FromAddress, MessageType, DataLength);)
            			}
            			break;
            			case UART_CAN:
            			{
            				//If default we use the can broadcast address.
            				//when the MambaNet address is found, we use a specific ethernet address.
            				unsigned char ToHardwareAddress[2] = {0,0};
								unsigned char ToServices = 0x80;	//default enable transmit, required for broadcasts

            				if (ToAddress == 0x10000000)
            				{	//Default the broadcast address is used!
            				}
            				else
            				{	//We look for a specific ethernet address.
            					long cnt;
            					bool Found;

            					cnt = 0;
            					Found = false;
            					while ((!Found) && (cnt<AddressTableCount))
            					{
            						if (ToAddress == AddressTable[cnt].MambaNetAddress)
            						{
            							if (	!((AddressTable[cnt].HardwareAddress[0] == 0x00) &&
            									(AddressTable[cnt].HardwareAddress[1] == 0x00) &&
            									(AddressTable[cnt].HardwareAddress[2] == 0x00) &&
            									(AddressTable[cnt].HardwareAddress[3] == 0x00) &&
            									(AddressTable[cnt].HardwareAddress[4] == 0x00) &&
            									(AddressTable[cnt].HardwareAddress[5] == 0x00)))
            							{
            								ToHardwareAddress[0] = AddressTable[cnt].HardwareAddress[0];
            								ToHardwareAddress[1] = AddressTable[cnt].HardwareAddress[1];
												ToServices = AddressTable[cnt].NodeServices;
            							}
            							Found = true;
            						}
            						cnt++;
            					}
            				}

								if (ToServices&0x80)
								{
	                        InterfaceUsedForTransmit->TransmitCallback(TransmitMessageBuffer, TransmitMessageBufferLength, ToHardwareAddress);	
								}

               			TRACE_PACKETS(printf("[UART_CAN] SendMambaMessage(0x%08X, 0x%08X, 0x%04X, %d)\n", ToAddress, FromAddress, MessageType, DataLength);)
            			}
            			break;
            			case ETHERNET:
            			{
            				//If default we use the ethernet broadcast address.
            				//when the MambaNet address is found, we use a specific ethernet address.
            				unsigned char ToHardwareAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
								unsigned char ToServices = 0x80;	//default enable transmit, required for broadcasts

            				if (ToAddress == 0x10000000)
            				{	//Default the broadcast address is used!
            				}
            				else
            				{	//We look for a specific ethernet address.
            					long cnt;
            					bool Found;

            					cnt = 0;
            					Found = false;
            					while ((!Found) && (cnt<AddressTableCount))
            					{
            						if (ToAddress == AddressTable[cnt].MambaNetAddress)
            						{
            							if (	!((AddressTable[cnt].HardwareAddress[0] == 0x00) &&
            									(AddressTable[cnt].HardwareAddress[1] == 0x00) &&
            									(AddressTable[cnt].HardwareAddress[2] == 0x00) &&
            									(AddressTable[cnt].HardwareAddress[3] == 0x00) &&
            									(AddressTable[cnt].HardwareAddress[4] == 0x00) &&
            									(AddressTable[cnt].HardwareAddress[5] == 0x00)))
            							{
            								ToHardwareAddress[0] = AddressTable[cnt].HardwareAddress[0];
            								ToHardwareAddress[1] = AddressTable[cnt].HardwareAddress[1];
            								ToHardwareAddress[2] = AddressTable[cnt].HardwareAddress[2];
            								ToHardwareAddress[3] = AddressTable[cnt].HardwareAddress[3];
            								ToHardwareAddress[4] = AddressTable[cnt].HardwareAddress[4];
            								ToHardwareAddress[5] = AddressTable[cnt].HardwareAddress[5];
												ToServices = AddressTable[cnt].NodeServices;
            							}
            							Found = true;
            						}
            						cnt++;
            					}
            				}

								if (ToServices&0x80)
								{
	                        InterfaceUsedForTransmit->TransmitCallback(TransmitMessageBuffer, TransmitMessageBufferLength, ToHardwareAddress);	
								}

               			TRACE_PACKETS(printf("[ETHERNET] SendMambaMessage(0x%08X, 0x%08X, 0x%04X, %d)\n", ToAddress, FromAddress, MessageType, DataLength);)
            			}
            			break;
                     case FIREWIRE:
                     {
               			TRACE_PACKETS(printf("[FIREWIRE] SendMambaMessage(0x%08X, 0x%08X, 0x%04X, %d)\n", ToAddress, FromAddress, MessageType, DataLength);)
                     }
                     break;
                     case TCPIP:
                     {
            				//If default we use the ethernet broadcast address.
            				//when the MambaNet address is found, we use a specific ethernet address.
            				unsigned char ToHardwareAddress[4] = {0xFF, 0xFF, 0xFF, 0xFF};

            				if (ToAddress == 0x10000000)
            				{	//Default the broadcast address is used!
            				}
            				else
            				{	//We look for a specific ethernet address.
            					long cnt;
            					bool Found;

            					cnt = 0;
            					Found = false;
            					while ((!Found) && (cnt<AddressTableCount))
            					{
            						if (ToAddress == AddressTable[cnt].MambaNetAddress)
            						{
            							if (	!((AddressTable[cnt].HardwareAddress[0] == 0x00) &&
            									(AddressTable[cnt].HardwareAddress[1] == 0x00) &&
            									(AddressTable[cnt].HardwareAddress[2] == 0x00) &&
            									(AddressTable[cnt].HardwareAddress[3] == 0x00)))
            							{
            								ToHardwareAddress[0] = AddressTable[cnt].HardwareAddress[0];
            								ToHardwareAddress[1] = AddressTable[cnt].HardwareAddress[1];
            								ToHardwareAddress[2] = AddressTable[cnt].HardwareAddress[2];
            								ToHardwareAddress[3] = AddressTable[cnt].HardwareAddress[3];
            							}
            							Found = true;
            						}
            						cnt++;
            					}
            				}

                        InterfaceUsedForTransmit->TransmitCallback(TransmitMessageBuffer, TransmitMessageBufferLength, ToHardwareAddress);

               			TRACE_PACKETS(printf("[TCPIP] SendMambaMessage(0x%08X, 0x%08X, 0x%04X, %d)\n", ToAddress, FromAddress, MessageType, DataLength);)
                     }
                     break;
            			default:
            			{
                        printf("MambaMessage could not be send, no interface found for 0x%08X!\n", ToAddress);
            			}
            			break;
                  }
         		}
            }
            else
            {
               printf("MambaMessage could not be send, no transmit callback function found for this interface!\n");
            }
         }
         else if (ToAddress == 0x10000000)
         {  //No interface found and broadcast, send to all available interfaces
            for (int cntInterface=0; cntInterface<InterfaceCount; cntInterface++)
            {
               if (Interfaces[cntInterface].TransmitCallback != NULL)
               {
                  unsigned char FilterActive;

                  FilterActive = 0;
                  switch (Interfaces[cntInterface].TransmitFilter)
                  {
                     case ALL_MESSAGES:
                     {  //no filter
                        FilterActive = 0;
                     }
                     break;
                     case NO_RESERVATION_INFO:
                     {
                        if ((MessageType == 0) && (Data[0] == 0))
                        {  //Address reservation info
                           FilterActive = 1;
                        }
                     }
                     break;
                     case NO_BROADCASTS:
                     {
                        if (ToAddress == 0x10000000)
                        {
                           FilterActive = 1;
                        }
                     }
                     break;
                     case NO_RESERVATION_INFO_FROM_OTHERS:
                     {
                        if ((MessageType == 0) && (Data[0] == 0) && (FromAddress != DefaultObjects->MambaNetAddress))
                        {  //Address reservation info that originates from an other node (we are possible a gateway)
                           FilterActive = 1;
                        }
                     }
                     break;
                     case NO_BROADCASTS_FROM_OTHERS:
                     {
                        if ((ToAddress == 0x10000000) && (FromAddress != DefaultObjects->MambaNetAddress))
                        {  //Broadcasts that originates from an other node (we are possible a gateway)
                           FilterActive = 1;
                        }
                     }
                     break;
                  }

                  if (!FilterActive)
                  {
               		switch (Interfaces[cntInterface].Type)
               		{
                        case CAN:
                        {
                           TRACE_PACKETS(printf("[CAN] SendMambaMessage(0x%08X, 0x%08X, 0x%04X, %d)\n", ToAddress, FromAddress, MessageType, DataLength);)
                        }
                        break;
               			case UART:
               			{
                           Interfaces[cntInterface].TransmitCallback(TransmitMessageBuffer, TransmitMessageBufferLength, NULL);

               				TRACE_PACKETS(printf("[UART] SendMambaMessage(0x%08X, 0x%08X, 0x%04X, %d)\n", ToAddress, FromAddress, MessageType, DataLength);)
               			}
               			break;
               			case UART_CAN:
               			{
               				unsigned char ToHardwareAddress[2] = {0,0};

                           Interfaces[cntInterface].TransmitCallback(TransmitMessageBuffer, TransmitMessageBufferLength, ToHardwareAddress);
                  			TRACE_PACKETS(printf("[UART_CAN] SendMambaMessage(0x%08X, 0x%08X, 0x%04X, %d)\n", ToAddress, FromAddress, MessageType, DataLength);)
               			}
               			break;
               			case ETHERNET:
               			{
               				//If default we use the ethernet broadcast address.
               				//when the MambaNet address is found, we use a specific ethernet address.
               				unsigned char ToHardwareAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

                           Interfaces[cntInterface].TransmitCallback(TransmitMessageBuffer, TransmitMessageBufferLength, ToHardwareAddress);
                  			TRACE_PACKETS(printf("[ETHERNET] SendMambaMessage(0x%08X, 0x%08X, 0x%04X, %d)\n", ToAddress, FromAddress, MessageType, DataLength);)
               			}
               			break;
                        case FIREWIRE:
                        {
                  			TRACE_PACKETS(printf("[FIREWIRE] SendMambaMessage(0x%08X, 0x%08X, 0x%04X, %d)\n", ToAddress, FromAddress, MessageType, DataLength);)
                        }
                        break;
                        case TCPIP:
                        {
               				//If default we use the ethernet broadcast address.
               				//when the MambaNet address is found, we use a specific ethernet address.
               				unsigned char ToHardwareAddress[4] = {0xFF, 0xFF, 0xFF, 0xFF};

                           Interfaces[cntInterface].TransmitCallback(TransmitMessageBuffer, TransmitMessageBufferLength, ToHardwareAddress);

                  			TRACE_PACKETS(printf("[TCPIP] SendMambaMessage(0x%08X, 0x%08X, 0x%04X, %d)\n", ToAddress, FromAddress, MessageType, DataLength);)
                        }
                        break;
               			default:
               			{
                           printf("MambaMessage could not be send, no interface found for 0x%08X!\n", ToAddress);
               			}
               			break;
                     }
            		}
               }
               else
               {
                  printf("MambaMessage could not be send, no transmit callback function found for this interface!\n");
               }
            }
         }
         else
         {
            printf("MambaMessage could not be send, no interface found!\n");
         }
      }
   	else
   	{
         printf("tried to send a to large databuffer in a MambaNet message.\n");
   	}
   }
}

// This supporting function will decode a message if byte-array to a understandable MambaNet message.
// It analizes the header and converts the data to 8 bit.
//
//	- Buffer
//   A buffer that start with the MambaNet SOM and end with the EOM must be supplied.
//
// - BufferLength
//	  The number of bytes that are in the buffer, must correspond with the EOM position.
//
// - FromInterface
//	  Tells us which interface did receive this package
//
// - FromHardwareAddress
//   Default is NULL, but if not NULL it gives the hardware address of device that transmitted
//   this package (does not have to be the orginal transmitter, could be a gateway/router).
void STDCALL DecodeRawMambaNetMessage(unsigned char *Buffer, unsigned char BufferLength, int FromInterfaceIndex, unsigned char FromHardwareAddress[16])
{
	unsigned char Buffer8Bit[MAX_MAMBANET_DATA_SIZE];
	unsigned char Buffer8BitLength;
	unsigned long int ToAddress;
	unsigned long int FromAddress;
	unsigned long int MessageID;
	unsigned int MessageType;
	unsigned char DataLength;
	unsigned char Ack;

	Ack = 0;
	if (Buffer[0] == 0x82)
	{
		Ack = 1;
	}

	//Decode the ToAddress (which is the MambaNet Address of the node which must receive the message)
	ToAddress  = ((unsigned long int)Buffer[0]<<28)&0x10000000;
   ToAddress |= ((unsigned long int)Buffer[1]<<21)&0x0FE00000;
   ToAddress |= ((unsigned long int)Buffer[2]<<14)&0x001FC000;
   ToAddress |= ((unsigned long int)Buffer[3]<< 7)&0x00003F80;
   ToAddress |= ((unsigned long int)Buffer[4]    )&0x0000007F;

	//Decode the FromAddress (which is the MambaNet Address of the original transmitter)
   FromAddress  = ((unsigned long int)Buffer[5]<<21)&0x0FE00000;
   FromAddress |= ((unsigned long int)Buffer[6]<<14)&0x001FC000;
   FromAddress |= ((unsigned long int)Buffer[7]<< 7)&0x00003F80;
   FromAddress |= ((unsigned long int)Buffer[8]    )&0x0000007F;

	//Decode the MessageID
   MessageID    = ((unsigned long int)Buffer[9] <<14)&0x001FC000;
   MessageID   |= ((unsigned long int)Buffer[10]<< 7)&0x00003F80;
   MessageID	|= ((unsigned long int)Buffer[11]    )&0x0000007F;

	//Decode the ToAddress (which is a MambaNet Address)
   MessageType  = ((unsigned int)Buffer[12]<<7)&0x3F80;
   MessageType |= ((unsigned int)Buffer[13]   )&0x007F;

	//Decode the message length, but not used for now.
   DataLength = ((unsigned int)Buffer[14]   )&0x007F;

	//Call the supporting function to convert the 7 bits data into 8 bits data.
   Buffer8BitLength = Decode7to8bits(&Buffer[PROTOCOL_OVERHEAD-1], BufferLength-PROTOCOL_OVERHEAD, Buffer8Bit);

   unsigned char Found = 0;
   for (int cntAddress=0; cntAddress<AddressTableCount; cntAddress++)
   {
      if (AddressTable[cntAddress].MambaNetAddress == FromAddress)
      {
         if (AddressTable[cntAddress].ReceivedInterfaceIndex == -1)
         {
            AddressTable[cntAddress].ReceivedInterfaceIndex = FromInterfaceIndex;
         }
         Found = 1;
      }
   }
   if (!Found)
   {  //Add basic info
      int FreeSlot = AddressTableCount;
      AddressTable[FreeSlot].MambaNetAddress = FromAddress;
      if (FromHardwareAddress != NULL)
      {
         memcpy(AddressTable[FreeSlot].HardwareAddress, FromHardwareAddress, 16);
      }
      AddressTable[FreeSlot].ReceivedInterfaceIndex = FromInterfaceIndex;

      AddressTableCount = FreeSlot+1;
   }

	//Call a function which does processing of some messages and calls the user receive function.
   ProcessMambaNetMessage(ToAddress, FromAddress, Ack, MessageID, MessageType, Buffer8Bit, Buffer8BitLength, FromInterfaceIndex, FromHardwareAddress);

	switch (Interfaces[FromInterfaceIndex].Type)
	{
		case NO_INTERFACE:
      {
      }
      break;
      case CAN:
      {
			cntMambaNetMessageReceivedFromCAN++;
      }
      break;
		case UART:
		{
			cntMambaNetMessageReceivedFromUART++;
		}
		break;
		case UART_CAN:
		{
			cntMambaNetMessageReceivedFromUART_CAN++;
		}
		break;
		case ETHERNET:
		{
			cntMambaNetMessageReceivedFromEthernet++;
		}
		break;
		case FIREWIRE:
      {
			cntMambaNetMessageReceivedFromFirewire++;
      }
      break;
      case TCPIP:
      {
			cntMambaNetMessageReceivedFromTCPIP++;
      }
      break;
	}
}

//This supporting function does some basic MambaNet message processing and
//calls the user receive function.
//The basis processing is used to maintain the address table.
//
//- ToAddress
//  Mamba address where this message is address to
//
//- FromAddress
//  Mamba address of the orginal transmitter.
//
//- MessageType
//	 defines the format of the data payload.
//
//- Data
//	 pointer to the data which is in 8 bit (unsigned char) format.
//
//- DataLength
//	 the amount if 8 bits data bytes in the data buffer.
//
// - FromInterface
//	  Tells us which interface did receive this package
//
// - FromHardwareAddress
//   Default is NULL, but if not NULL it gives the hardware address of device that transmitted
//   this package (does not have to be the orginal transmitter, could be a gateway/router).
void ProcessMambaNetMessage(unsigned long int ToAddress, unsigned long int FromAddress, unsigned char Ack, unsigned long int MessageID, unsigned int MessageType, unsigned char *Data, unsigned char DataLength, int FromInterfaceIndex, unsigned char FromHardwareAddress[16])
{
   unsigned char MambaNetMessageProcessed;

   if (MessageType == 1)
   {
      MambaNetMessageProcessed = 1;
   }

   MambaNetMessageProcessed = 0;

   if (FromAddress != DefaultObjects->MambaNetAddress)
   {
      if (ToAddress == 0x10000000)
      {  //Gateway function
         for (int cntInterface=0; cntInterface<InterfaceCount; cntInterface++)
         {
            if (cntInterface != FromInterfaceIndex)
            {
               if (Interfaces[cntInterface].Type != NO_INTERFACE)
               {
                  SendMambaNetMessage(ToAddress, FromAddress, Ack, MessageID, MessageType, Data, DataLength, cntInterface);
               }
            }
         }
      }
      else if (ToAddress != DefaultObjects->MambaNetAddress)
      {  //Gateway function
         long cnt;
         bool Found;

         cnt = 0;
         Found = false;
         while ((!Found) && (cnt<AddressTableCount))
         {
            if (ToAddress == AddressTable[cnt].MambaNetAddress)
            {
               if (AddressTable[cnt].ReceivedInterfaceIndex != FromInterfaceIndex)
               {
                  SendMambaNetMessage(ToAddress, FromAddress, Ack, MessageID, MessageType, Data, DataLength, AddressTable[cnt].ReceivedInterfaceIndex);
                  Found = true;
               }
            }
            cnt++;
         }
      }

      if ((ToAddress == 0x10000000) || (ToAddress == DefaultObjects->MambaNetAddress))
      {
      	switch (Interfaces[FromInterfaceIndex].Type)
      	{
      		case CAN:
            {
      			TRACE_PACKETS(printf("[CAN] ProcessMambaNetMessage(0x%08X, 0x%08X, 0x%06X, 0x%04X, %d)\n", ToAddress, FromAddress, MessageID, MessageType, DataLength);)
            }
            break;
      		case UART:
      		{
      			TRACE_PACKETS(printf("[UART] ProcessMambaNetMessage(0x%08X, 0x%08X, 0x%06X, 0x%04X, %d)\n", ToAddress, FromAddress, MessageID, MessageType, DataLength);)
      		}
      		break;
      		case UART_CAN:
      		{
      			TRACE_PACKETS(printf("[UART_CAN] ProcessMambaNetMessage(0x%08X, 0x%08X, 0x%06X, 0x%04X, %d)\n", ToAddress, FromAddress, MessageID, MessageType, DataLength);)
      		}
      		break;
      		case ETHERNET:
      		{
      			TRACE_PACKETS(printf("[ETHERNET] ProcessMambaNetMessage(0x%08X, 0x%08X, 0x%06X, 0x%04X, %d)\n", ToAddress, FromAddress, MessageID, MessageType, DataLength);)
      		}
      		break;
            case FIREWIRE:
            {
      			TRACE_PACKETS(printf("[FIREWIRE] ProcessMambaNetMessage(0x%08X, 0x%08X, 0x%06X, 0x%04X, %d)\n", ToAddress, FromAddress, MessageID, MessageType, DataLength);)
            }
            break;
            case TCPIP:
            {
      			TRACE_PACKETS(printf("[TCPIP] ProcessMambaNetMessage(0x%08X, 0x%08X, 0x%06X, 0x%04X, %d)\n", ToAddress, FromAddress, MessageID, MessageType, DataLength);)
            }
            break;
      		default:
      		{
      			TRACE_PACKETS(printf("[---] ProcessMambaNetMessage(0x%08X, 0x%08X, 0x%06X, 0x%04X, %d)\n", ToAddress, FromAddress, MessageID, MessageType, DataLength);)
      		}
      		break;
      	}

      	switch (MessageType)
      	{
      		case MAMBANET_ADDRESS_RESERVATION_MESSAGETYPE:
      		{	//Address Reservation Information
               unsigned char Type;
               unsigned int ReceivedManufacturerID;
               unsigned int ReceivedProductID;
               unsigned int ReceivedUniqueIDPerProduct;
               unsigned long int ReceivedMambaNetAddress;
               unsigned long int ReceivedDefaultEngineMambaNetAddress;
               unsigned char ReceivedNodeServices;

               Type                          = Data[0];
      			ReceivedManufacturerID        = ((unsigned int)Data[1]<<8) | ((unsigned int)Data[2]);
               ReceivedProductID             = ((unsigned int)Data[3]<<8) | ((unsigned int)Data[4]);
               ReceivedUniqueIDPerProduct    = ((unsigned int)Data[5]<<8) | ((unsigned int)Data[6]);
               ReceivedMambaNetAddress       = ((unsigned long int)Data[7]<<24);
               ReceivedMambaNetAddress      |= ((unsigned long int)Data[8]<<16);
               ReceivedMambaNetAddress      |= ((unsigned long int)Data[9]<< 8);
               ReceivedMambaNetAddress      |= ((unsigned long int)Data[10]   );
               ReceivedDefaultEngineMambaNetAddress   = ((unsigned long int)Data[11]<<24);
               ReceivedDefaultEngineMambaNetAddress   |= ((unsigned long int)Data[12]<<16);
               ReceivedDefaultEngineMambaNetAddress   |= ((unsigned long int)Data[13]<< 8);
               ReceivedDefaultEngineMambaNetAddress   |= ((unsigned long int)Data[14]    );
               ReceivedNodeServices = Data[15];

      			if ((Type == MAMBANET_ADDRESS_RESERVATION_TYPE_INFO) && (DefaultObjects->MambaNetAddress != 0x00000000))
      			{	//Reservation info.
      				long cnt;
      				long FreeSlot;
      				bool Found;

      				cnt = 0;
      				FreeSlot = -1;
      				Found = false;
      				while ((!Found) && (cnt<AddressTableCount))
      				{
                     if ((AddressTable[cnt].ManufacturerID == 0) && (ReceivedMambaNetAddress != 0x00000000))
                     {
                        if (AddressTable[cnt].MambaNetAddress == ReceivedMambaNetAddress)
                        {
									FreeSlot = cnt;
         					   //AddressTable[cnt].ManufacturerID		   = ReceivedManufacturerID;
         						//AddressTable[cnt].ProductID				= ReceivedProductID;
         						//AddressTable[cnt].UniqueIDPerProduct	= ReceivedUniqueIDPerProduct;
                           //AddressTable[cnt].Alive = 0;
                        }
                     }

        					if (	(AddressTable[cnt].ManufacturerID		== ReceivedManufacturerID	  ) &&
        							(AddressTable[cnt].ProductID				== ReceivedProductID		 	  ) &&
        							(AddressTable[cnt].UniqueIDPerProduct	== ReceivedUniqueIDPerProduct))
        					{
                        unsigned char ChangeInTable = 0;

                        //Reset the timeout counter
                        if (AddressTable[cnt].Alive == 0)
                        {
                           AddressTable[cnt].NodeServices = 0x00;
                           if (Interfaces[FromInterfaceIndex].Type != NO_INTERFACE)
                           {
                              if (Interfaces[FromInterfaceIndex].AddressTableChangeCallback != NULL)
                              {
                                 Interfaces[FromInterfaceIndex].AddressTableChangeCallback(AddressTable, ADDRESS_TABLE_ENTRY_ACTIVATED, cnt);
                              }
                           }
                        }
                        AddressTable[cnt].Alive = ALIVE_TIME;

                        if (AddressTable[cnt].MambaNetAddress != ReceivedMambaNetAddress)
                        {
                           if (ReceivedNodeServices&0x80)
                           {
                              AddressTable[cnt].MambaNetAddress = ReceivedMambaNetAddress;
                              ChangeInTable = 1;
                           }
                        }
                        if (AddressTable[cnt].DefaultEngineMambaNetAddress != ReceivedDefaultEngineMambaNetAddress)
                        {
                           if (ReceivedNodeServices&0x80)
                           {
                              AddressTable[cnt].DefaultEngineMambaNetAddress = ReceivedDefaultEngineMambaNetAddress;
                              ChangeInTable = 1;
                           }
                        }
                        if (AddressTable[cnt].NodeServices != ReceivedNodeServices)
                        {
                           AddressTable[cnt].NodeServices  = ReceivedNodeServices;
                           ChangeInTable = 1;
                        }

                        if (FromHardwareAddress != NULL)
                        {
                           memcpy(AddressTable[cnt].HardwareAddress, FromHardwareAddress, 16);
                        }
                        AddressTable[cnt].ReceivedInterfaceIndex  = FromInterfaceIndex;
                        Found = true;

                        if (ChangeInTable)
                        {
                           if (Interfaces[FromInterfaceIndex].Type != NO_INTERFACE)
                           {
                              if (Interfaces[FromInterfaceIndex].AddressTableChangeCallback != NULL)
                              {
                                 Interfaces[FromInterfaceIndex].AddressTableChangeCallback(AddressTable, ADDRESS_TABLE_ENTRY_CHANGED, cnt);
                              }
                           }
                        }
                     }
                     else if (AddressTable[cnt].ManufacturerID == 0)
                     {
                        if (FreeSlot == -1)
                        {
                           FreeSlot = cnt;
                        }
                     }
      					cnt++;
      				}
      				if ((!Found) && (FreeSlot != -1))
      				{
      					AddressTable[FreeSlot].ManufacturerID 				      = ReceivedManufacturerID;
      					AddressTable[FreeSlot].ProductID 					      = ReceivedProductID;
               		AddressTable[FreeSlot].UniqueIDPerProduct 		      = ReceivedUniqueIDPerProduct;
               		AddressTable[FreeSlot].MambaNetAddress				      = ReceivedMambaNetAddress;
                     AddressTable[FreeSlot].DefaultEngineMambaNetAddress   = ReceivedDefaultEngineMambaNetAddress;
                     AddressTable[FreeSlot].NodeServices                   = ReceivedNodeServices;
                     AddressTable[FreeSlot].Alive                          = 0;

                     if (FromHardwareAddress != NULL)
                     {
                        memcpy(AddressTable[FreeSlot].HardwareAddress, FromHardwareAddress, 16);
                     }
         				AddressTable[FreeSlot].ReceivedInterfaceIndex = FromInterfaceIndex;

                     if (Interfaces[FromInterfaceIndex].Type != NO_INTERFACE)
                     {
                        if (Interfaces[FromInterfaceIndex].AddressTableChangeCallback != NULL)
                        {
                           Interfaces[FromInterfaceIndex].AddressTableChangeCallback(AddressTable, ADDRESS_TABLE_ENTRY_ADDED, FreeSlot);
                        }
                     }
                     if (FreeSlot>(AddressTableCount-1))
                     {
                        AddressTableCount = FreeSlot+1;
                     }

                     if (FromHardwareAddress != NULL)
                     {
                        switch (Interfaces[FromInterfaceIndex].Type)
                        {
                           case CAN:
                           {
                           }
                           break;
                           case UART:
                           {
                              TRACE_ADDRESSTABLE(printf("[UART] Added@%d ManID:0x%04X, ProdID:0x%04X, UniqID:0x%04X, MambaNetAddr:0x%08X HwAddr: COM%d\n", FreeSlot, ReceivedManufacturerID, ReceivedProductID, ReceivedUniqueIDPerProduct, ReceivedMambaNetAddress, FromHardwareAddress[0]);)
                           }
                           break;
                           case UART_CAN:
                           {
                              TRACE_ADDRESSTABLE(printf("[UART_CAN] Added@%d ManID:0x%04X, ProdID:0x%04X, UniqID:0x%04X, MambaNetAddr:0x%08X HwAddr: COM%d\n", FreeSlot, ReceivedManufacturerID, ReceivedProductID, ReceivedUniqueIDPerProduct, ReceivedMambaNetAddress, FromHardwareAddress[0]);)
                           }
                           break;
                           case ETHERNET:
            					{
            						TRACE_ADDRESSTABLE(printf("[ETHERNET] Added@%d ManID:0x%04X, ProdID:0x%04X, UniqID:0x%04X, MambaNetAddr:0x%08X HwAddr:%02X:%02X:%02X:%02X:%02X:%02X\n", FreeSlot, ReceivedManufacturerID, ReceivedProductID, ReceivedUniqueIDPerProduct, ReceivedMambaNetAddress, FromHardwareAddress[0], FromHardwareAddress[1], FromHardwareAddress[2], FromHardwareAddress[3], FromHardwareAddress[4], FromHardwareAddress[5]);)
                           }
                           break;
                           case FIREWIRE:
                           {
                           }
                           break;
                           case TCPIP:
                           {
                              TRACE_ADDRESSTABLE(printf("[TCPIP] Added@%d ManID:0x%04X, ProdID:0x%04X, UniqID:0x%04X, MambaNetAddr:0x%08X HwAddr:%d.%d.%d.%d\n", FreeSlot, ReceivedManufacturerID, ReceivedProductID, ReceivedUniqueIDPerProduct, ReceivedMambaNetAddress, FromHardwareAddress[0], FromHardwareAddress[1], FromHardwareAddress[2], FromHardwareAddress[3]);)
                           }
                           break;
      	   				}
                     }
                     else
                     {
                        switch (Interfaces[FromInterfaceIndex].Type)
                        {
                           case CAN:
                           {
                           }
                           break;
                           case UART:
                           {
                              TRACE_ADDRESSTABLE(printf("[UART] Added@%d ManID:0x%04X, ProdID:0x%04X, UniqID:0x%04X, MambaNetAddr:0x%08X HwAddr: NULL\n", FreeSlot, ReceivedManufacturerID, ReceivedProductID, ReceivedUniqueIDPerProduct, ReceivedMambaNetAddress);)
                           }
                           break;
                           case UART_CAN:
                           {
                              TRACE_ADDRESSTABLE(printf("[UART_CAN] Added@%d ManID:0x%04X, ProdID:0x%04X, UniqID:0x%04X, MambaNetAddr:0x%08X HwAddr: NULL\n", FreeSlot, ReceivedManufacturerID, ReceivedProductID, ReceivedUniqueIDPerProduct, ReceivedMambaNetAddress);)
                           }
                           break;
                           case ETHERNET:
            					{
            						TRACE_ADDRESSTABLE(printf("[ETHERNET] Added@%d ManID:0x%04X, ProdID:0x%04X, UniqID:0x%04X, MambaNetAddr:0x%08X HwAddr: NULL\n", FreeSlot, ReceivedManufacturerID, ReceivedProductID, ReceivedUniqueIDPerProduct, ReceivedMambaNetAddress);)
                           }
                           break;
                           case FIREWIRE:
                           {
                           }
                           break;
                           case TCPIP:
                           {
                              TRACE_ADDRESSTABLE(printf("[TCPIP] Added@%d ManID:0x%04X, ProdID:0x%04X, UniqID:0x%04X, MambaNetAddr:0x%08X HwAddr: NULL\n", FreeSlot, ReceivedManufacturerID, ReceivedProductID, ReceivedUniqueIDPerProduct, ReceivedMambaNetAddress);)
                           }
                           break;
      	   				}
                     }
      				}
                  //MambaNetMessageProcessed = 1;
      			}
               else if (Type == MAMBANET_ADDRESS_RESERVATION_TYPE_RESPONSE)
               {  //respone
                  if ((DefaultObjects->ManufacturerID == ReceivedManufacturerID) &&
                     (DefaultObjects->ProductID == ReceivedProductID) &&
                     (DefaultObjects->UniqueIDPerProduct == ReceivedUniqueIDPerProduct))
                  {
                     DefaultObjects->MambaNetAddress = ReceivedMambaNetAddress;
                     DefaultObjects->DefaultEngineMambaNetAddress = ReceivedDefaultEngineMambaNetAddress;
                     DefaultObjects->Services |= 0x80; //address validated

                     MambaNetMessageProcessed = 1;
                  }
               }
               else if (Type == MAMBANET_ADDRESS_RESERVATION_TYPE_PING)
               { // ping
                   if((ReceivedManufacturerID == 0 || DefaultObjects->ManufacturerID == ReceivedManufacturerID) &&
                      (ReceivedProductID == 0 || DefaultObjects->ProductID == ReceivedProductID) &&
                      (ReceivedUniqueIDPerProduct == 0 || DefaultObjects->UniqueIDPerProduct == ReceivedUniqueIDPerProduct) &&
                      (ReceivedMambaNetAddress == 0 || DefaultObjects->DefaultEngineMambaNetAddress == ReceivedMambaNetAddress) &&
                      (ReceivedNodeServices == 0 || DefaultObjects->Services & ReceivedNodeServices))
                   {
                       SendMambaNetReservationInfo();
                       MambaNetMessageProcessed = 1;
                   }
               }
      		}
      		break;
            case MAMBANET_OBJECT_MESSAGETYPE:
            {  //Object message
               unsigned int ObjectNr;
               unsigned char ObjectMessageAction;

               ObjectNr = ((unsigned int)Data[0]<<8) | Data[1];
               ObjectMessageAction = Data[2];

               switch (ObjectMessageAction)
               {
                  case  MAMBANET_OBJECT_ACTION_GET_INFORMATION:
                  {
                     if ((ObjectNr>=1024) && (ObjectNr<(1024+NumberOfCustomObjects)))
                     {  //Only for the non-standard objects
                        unsigned char TransmitBuffer[96];
                        char cntSize;
                        int TableNr;
                        char DataSize;
                        unsigned long int TempData;
                        char cntChar;

                        TableNr = ObjectNr-1024;

                        cntSize=0;
                        TransmitBuffer[0] = (ObjectNr>>8)&0xFF;
                        TransmitBuffer[1] = ObjectNr&0xFF;
                        TransmitBuffer[2] = MAMBANET_OBJECT_ACTION_INFORMATION_RESPONSE;
                        TransmitBuffer[3] = OBJECT_INFORMATION_DATATYPE;
                        TransmitBuffer[4] = cntSize;
                        for (cntChar=0; cntChar<32; cntChar++)
                        {
                           TransmitBuffer[5+cntSize++] = CustomObjectInformation[TableNr].Description[cntChar];
                        }
                        TransmitBuffer[5+cntSize++] = CustomObjectInformation[TableNr].Services;
                        //Sensor
                        TransmitBuffer[5+cntSize++] = CustomObjectInformation[TableNr].Sensor.DataType;
                        DataSize = CustomObjectInformation[TableNr].Sensor.DataSize;
                        TransmitBuffer[5+cntSize++] = DataSize;
                        if ((CustomObjectInformation[TableNr].Sensor.DataType == OCTET_STRING_DATATYPE) || (CustomObjectInformation[TableNr].Sensor.DataType == BIT_STRING_DATATYPE))
                        {
                           DataSize = 1;
                        }
                        TempData = CustomObjectInformation[TableNr].Sensor.DataMinimal;
                        for (cntChar=0; cntChar<DataSize; cntChar++)
                        {
                           TransmitBuffer[5+cntSize++] = (TempData>>(((DataSize-cntChar)-1)<<3))&0xFF;
                        }
                        TempData = CustomObjectInformation[TableNr].Sensor.DataMaximal;
                        for (cntChar=0; cntChar<DataSize; cntChar++)
                        {
                           TransmitBuffer[5+cntSize++] = (TempData>>(((DataSize-cntChar)-1)<<3))&0xFF;
                        }
                        //Actuator
                        TransmitBuffer[5+cntSize++] = CustomObjectInformation[TableNr].Actuator.DataType;
                        DataSize = CustomObjectInformation[TableNr].Actuator.DataSize;
                        TransmitBuffer[5+cntSize++] = DataSize;
                        if ((CustomObjectInformation[TableNr].Actuator.DataType == OCTET_STRING_DATATYPE) || (CustomObjectInformation[TableNr].Actuator.DataType == BIT_STRING_DATATYPE))
                        {
                           DataSize = 1;
                        }

                        TempData = CustomObjectInformation[TableNr].Actuator.DataMinimal;
                        for (cntChar=0; cntChar<DataSize; cntChar++)
                        {
                           TransmitBuffer[5+cntSize++] = (TempData>>(((DataSize-cntChar)-1)<<3))&0xFF;
                        }
                        TempData = CustomObjectInformation[TableNr].Actuator.DataMaximal;
                        for (cntChar=0; cntChar<DataSize; cntChar++)
                        {
                           TransmitBuffer[5+cntSize++] = (TempData>>(((DataSize-cntChar)-1)<<3))&0xFF;
                        }
                        TempData = CustomObjectInformation[TableNr].Actuator.DefaultData;
                        for (cntChar=0; cntChar<DataSize; cntChar++)
                        {
                           TransmitBuffer[5+cntSize++] = (TempData>>(((DataSize-cntChar)-1)<<3))&0xFF;
                        }
                        //Adjust the size
                        TransmitBuffer[4] = cntSize;

                        SendMambaNetMessage(FromAddress, DefaultObjects->MambaNetAddress, Ack, 0, 1, TransmitBuffer, cntSize+5);
                        MambaNetMessageProcessed = 1;
                     }
                     else if ((ObjectNr>=(1024+NumberOfCustomObjects)) && (ObjectNr<(1024+DefaultObjects->NumberOfObjects)))
                     {   //dynamic generated objects, so not processed here
                        MambaNetMessageProcessed = 0;
                     }
                     else
                     {
                        unsigned char TransmitBuffer[20];

                        TransmitBuffer[0] = (ObjectNr>>8)&0xFF;
                        TransmitBuffer[1] = ObjectNr&0xFF;
                        TransmitBuffer[2] = MAMBANET_OBJECT_ACTION_INFORMATION_RESPONSE;
                        TransmitBuffer[3] = ERROR_DATATYPE;
                        TransmitBuffer[4] = 15;
                        sprintf((char *)&TransmitBuffer[5], "Not implemented");

                        SendMambaNetMessage(FromAddress, DefaultObjects->MambaNetAddress, Ack, 0, 1, TransmitBuffer, 20);
                        MambaNetMessageProcessed = 1;
                     }
                  }
                  break;
                  case MAMBANET_OBJECT_ACTION_INFORMATION_RESPONSE:
                  {       //Not implemented
                  }
                  break;
                  case MAMBANET_OBJECT_ACTION_GET_ENGINE_ADDRESS:
                  {
                     if ((ObjectNr>=1024) && (ObjectNr<(1024+NumberOfCustomObjects)))
                     {  //Only for the non-standard objects
                        unsigned char TransmitBuffer[9];
                        unsigned long int EngineMambaNetAddress;

                        EngineMambaNetAddress = CustomObjectInformation[ObjectNr-1024].EngineMambaNetAddress;

                        TransmitBuffer[0] = (ObjectNr>>8)&0xFF;
                        TransmitBuffer[1] = ObjectNr&0xFF;
                        TransmitBuffer[2] = MAMBANET_OBJECT_ACTION_ENGINE_ADDRESS_RESPONSE;
                        TransmitBuffer[3] = UNSIGNED_INTEGER_DATATYPE;
                        TransmitBuffer[4] = 4;
                        TransmitBuffer[5] = (EngineMambaNetAddress>>24)&0xFF;
                        TransmitBuffer[6] = (EngineMambaNetAddress>>16)&0xFF;
                        TransmitBuffer[7] = (EngineMambaNetAddress>> 8)&0xFF;
                        TransmitBuffer[8] =  EngineMambaNetAddress     &0xFF;

                        SendMambaNetMessage(FromAddress, DefaultObjects->MambaNetAddress, Ack, 0, 1, TransmitBuffer, 9);
                        MambaNetMessageProcessed = 1;
                     }
                     else if ((ObjectNr>=(1024+NumberOfCustomObjects)) && (ObjectNr<(1024+DefaultObjects->NumberOfObjects)))
                     {   //dynamic generated objects, so not processed
                        MambaNetMessageProcessed = 0;
                     }
                     else
                     {
                        unsigned char TransmitBuffer[20];

                        TransmitBuffer[0] = (ObjectNr>>8)&0xFF;
                        TransmitBuffer[1] = ObjectNr&0xFF;
                        TransmitBuffer[2] = MAMBANET_OBJECT_ACTION_ENGINE_ADDRESS_RESPONSE;
                        TransmitBuffer[3] = ERROR_DATATYPE;
                        TransmitBuffer[4] = 15;
                        sprintf((char *)&TransmitBuffer[5], "Not implemented");

                        SendMambaNetMessage(FromAddress, DefaultObjects->MambaNetAddress, Ack, 0, 1, TransmitBuffer, 20);
                        MambaNetMessageProcessed = 1;
                     }
                  }
                  break;
                  case MAMBANET_OBJECT_ACTION_ENGINE_ADDRESS_RESPONSE:
                  {  //Not implemented
                  }
                  break;
                  case MAMBANET_OBJECT_ACTION_SET_ENGINE_ADDRESS:
                  {
                     if ((ObjectNr>=1024) && (ObjectNr<(1024+NumberOfCustomObjects)))
                     {  //Only for the non-standard objects
                        unsigned char DataType;
                        unsigned char DataSize;

                        DataType = Data[3];
                        DataSize = Data[4];

                        if (DataType == UNSIGNED_INTEGER_DATATYPE)
                        {
                           if (DataSize == 4)
                           {
                              unsigned long int EngineMambaNetAddress;

                              EngineMambaNetAddress = Data[5];
                              EngineMambaNetAddress <<= 8;
                              EngineMambaNetAddress |= Data[6];
                              EngineMambaNetAddress <<= 8;
                              EngineMambaNetAddress |= Data[7];
                              EngineMambaNetAddress <<= 8;
                              EngineMambaNetAddress |= Data[8];

                              CustomObjectInformation[ObjectNr-1024].EngineMambaNetAddress = EngineMambaNetAddress;

                              MambaNetMessageProcessed = 1;
                           }
                        }
                     }
                     else if ((ObjectNr>=(1024+NumberOfCustomObjects)) && (ObjectNr<(1024+DefaultObjects->NumberOfObjects)))
                     {   //dynamic generated objects, so not processed
                        MambaNetMessageProcessed = 1;
                     }
                     else
                     {
                        unsigned char TransmitBuffer[20];

                        TransmitBuffer[0] = (ObjectNr>>8)&0xFF;
                        TransmitBuffer[1] = ObjectNr&0xFF;
                        TransmitBuffer[2] = MAMBANET_OBJECT_ACTION_ENGINE_ADDRESS_RESPONSE;
                        TransmitBuffer[3] = ERROR_DATATYPE;
                        TransmitBuffer[4] = 15;
                        sprintf((char *)&TransmitBuffer[5], "Not implemented");

                        SendMambaNetMessage(FromAddress, DefaultObjects->MambaNetAddress, Ack, 0, 1, TransmitBuffer, 20);
                        MambaNetMessageProcessed = 1;
                     }
                  }
                  break;
                  case  MAMBANET_OBJECT_ACTION_GET_OBJECT_FREQUENCY:
                  {
                     if ((ObjectNr>=1024) && (ObjectNr<(1024+NumberOfCustomObjects)))
                     {  //Only for the non-standard objects
                        if (CustomObjectInformation[ObjectNr].Services != 0x00)
                        {
                           unsigned char TransmitBuffer[6];
                           unsigned char ObjectFrequency;

                           ObjectFrequency = CustomObjectInformation[ObjectNr-1024].UpdateFrequency;

                           TransmitBuffer[0] = (ObjectNr>>8)&0xFF;
                           TransmitBuffer[1] = ObjectNr&0xFF;
                           TransmitBuffer[2] = MAMBANET_OBJECT_ACTION_OBJECT_FREQUENCY_RESPONSE;
                           TransmitBuffer[3] = STATE_DATATYPE;
                           TransmitBuffer[4] = 1;
                           TransmitBuffer[5] = ObjectFrequency&0xFF;

                           SendMambaNetMessage(FromAddress, DefaultObjects->MambaNetAddress, Ack, 0, 1, TransmitBuffer, 6);
                           MambaNetMessageProcessed = 1;
                        }
                     }
                     else if ((ObjectNr>=(1024+NumberOfCustomObjects)) && (ObjectNr<(1024+DefaultObjects->NumberOfObjects)))
                     {   //dynamic generated objects, so not processed
                        MambaNetMessageProcessed = 0;
                     }
                     else
                     {
                        unsigned char TransmitBuffer[20];

                        TransmitBuffer[0] = (ObjectNr>>8)&0xFF;
                        TransmitBuffer[1] = ObjectNr&0xFF;
                        TransmitBuffer[2] = MAMBANET_OBJECT_ACTION_OBJECT_FREQUENCY_RESPONSE;
                        TransmitBuffer[3] = ERROR_DATATYPE;
                        TransmitBuffer[4] = 15;
                        sprintf((char *)&TransmitBuffer[5], "Not implemented");

                        SendMambaNetMessage(FromAddress, DefaultObjects->MambaNetAddress, Ack, 0, 1, TransmitBuffer, 20);
                        MambaNetMessageProcessed = 1;
                     }
                  }
                  break;
                  case  MAMBANET_OBJECT_ACTION_OBJECT_FREQUENCY_RESPONSE:
                  {  //No implementation required.
                  }
                  break;
                  case  MAMBANET_OBJECT_ACTION_SET_OBJECT_FREQUENCY:
                  {
                     if ((ObjectNr>=1024) && (ObjectNr<(1024+NumberOfCustomObjects)))
                     {  //Only for the non-standard objects
                        unsigned char DataType;
                        unsigned char DataSize;

                        DataType = Data[3];
                        DataSize = Data[4];

                        if (DataType == STATE_DATATYPE)
                        {
                           if (DataSize == 1)
                           {
                              unsigned char ObjectFrequency;

                              ObjectFrequency = Data[5]&0x0F;
                              CustomObjectInformation[ObjectNr-1024].UpdateFrequency = ObjectFrequency;

                              MambaNetMessageProcessed = 1;
                           }
                        }
                     }
                     else if ((ObjectNr>=(1024+NumberOfCustomObjects)) && (ObjectNr<(1024+DefaultObjects->NumberOfObjects)))
                     {   //dynamic generated objects, so not processed
                        MambaNetMessageProcessed = 0;
                     }
                     else
                     {
                        unsigned char TransmitBuffer[20];

                        TransmitBuffer[0] = (ObjectNr>>8)&0xFF;
                        TransmitBuffer[1] = ObjectNr&0xFF;
                        TransmitBuffer[2] = MAMBANET_OBJECT_ACTION_OBJECT_FREQUENCY_RESPONSE;
                        TransmitBuffer[3] = ERROR_DATATYPE;
                        TransmitBuffer[4] = 15;
                        sprintf((char *)&TransmitBuffer[5], "Not implemented");

                        SendMambaNetMessage(FromAddress, DefaultObjects->MambaNetAddress, Ack, 0, 1, TransmitBuffer, 20);
                        MambaNetMessageProcessed = 1;
                     }
                  }
                  break;
                  case  MAMBANET_OBJECT_ACTION_GET_SENSOR_DATA:
                  {
                     if (ObjectNr<1024)
                     {  //Only for the standard obects.
                        unsigned char TransmitBuffer[69];
                        char cntByte;

                        switch (ObjectNr)
                        {
                           case 0:
                           {  //Description
                              TransmitBuffer[0] = (ObjectNr>>8)&0xFF;
                              TransmitBuffer[1] = ObjectNr&0xFF;
                              TransmitBuffer[2] = MAMBANET_OBJECT_ACTION_SENSOR_DATA_RESPONSE;
                              TransmitBuffer[3] = OCTET_STRING_DATATYPE;
                              TransmitBuffer[4] = 64;
                              for (cntByte=0; cntByte<64; cntByte++)
                              {
                                 TransmitBuffer[5+cntByte] = DefaultObjects->Description[cntByte];
                              }

                              SendMambaNetMessage(FromAddress, DefaultObjects->MambaNetAddress, Ack, 0, 1, TransmitBuffer, 69);
                           }
                           break;
   //                      case 1: Name is an actuator
                           case 2:
                           {  //ManufacturerID
                              TransmitBuffer[0] = (ObjectNr>>8)&0xFF;
                              TransmitBuffer[1] = ObjectNr&0xFF;
                              TransmitBuffer[2] = MAMBANET_OBJECT_ACTION_SENSOR_DATA_RESPONSE;
                              TransmitBuffer[3] = UNSIGNED_INTEGER_DATATYPE;
                              TransmitBuffer[4] = 2;
                              TransmitBuffer[5] = (DefaultObjects->ManufacturerID>>8)&0xFF;
                              TransmitBuffer[6] = DefaultObjects->ManufacturerID&0xFF;

                              SendMambaNetMessage(FromAddress, DefaultObjects->MambaNetAddress, Ack, 0, 1, TransmitBuffer, 7);
                           }
                           break;
                           case 3:
                           {  //ProductID
                              TransmitBuffer[0] = (ObjectNr>>8)&0xFF;
                              TransmitBuffer[1] = ObjectNr&0xFF;
                              TransmitBuffer[2] = MAMBANET_OBJECT_ACTION_SENSOR_DATA_RESPONSE;
                              TransmitBuffer[3] = UNSIGNED_INTEGER_DATATYPE;
                              TransmitBuffer[4] = 2;
                              TransmitBuffer[5] = (DefaultObjects->ProductID>>8)&0xFF;
                              TransmitBuffer[6] = DefaultObjects->ProductID&0xFF;

                              SendMambaNetMessage(FromAddress, DefaultObjects->MambaNetAddress, Ack, 0, 1, TransmitBuffer, 7);
                           }
                           break;
                           case 4:
                           {  //UniqueID
                              TransmitBuffer[0] = (ObjectNr>>8)&0xFF;
                              TransmitBuffer[1] = ObjectNr&0xFF;
                              TransmitBuffer[2] = MAMBANET_OBJECT_ACTION_SENSOR_DATA_RESPONSE;
                              TransmitBuffer[3] = UNSIGNED_INTEGER_DATATYPE;
                              TransmitBuffer[4] = 2;
                              TransmitBuffer[5] = (DefaultObjects->UniqueIDPerProduct>>8)&0xFF;
                              TransmitBuffer[6] = DefaultObjects->UniqueIDPerProduct&0xFF;

                              SendMambaNetMessage(FromAddress, DefaultObjects->MambaNetAddress, Ack, 0, 1, TransmitBuffer, 7);
                           }
                           break;
                           case 5:
                           {  //HardwareMajorRevision
                              TransmitBuffer[0] = (ObjectNr>>8)&0xFF;
                              TransmitBuffer[1] = ObjectNr&0xFF;
                              TransmitBuffer[2] = MAMBANET_OBJECT_ACTION_SENSOR_DATA_RESPONSE;
                              TransmitBuffer[3] = UNSIGNED_INTEGER_DATATYPE;
                              TransmitBuffer[4] = 1;
                              TransmitBuffer[5] = DefaultObjects->HardwareMajorRevision;

                              SendMambaNetMessage(FromAddress, DefaultObjects->MambaNetAddress, Ack, 0, 1, TransmitBuffer, 6);
                           }
                           break;
                           case 6:
                           {  //HarwareMinorRevision
                              TransmitBuffer[0] = (ObjectNr>>8)&0xFF;
                              TransmitBuffer[1] = ObjectNr&0xFF;
                              TransmitBuffer[2] = MAMBANET_OBJECT_ACTION_SENSOR_DATA_RESPONSE;
                              TransmitBuffer[3] = UNSIGNED_INTEGER_DATATYPE;
                              TransmitBuffer[4] = 1;
                              TransmitBuffer[5] = DefaultObjects->HardwareMinorRevision;

                              SendMambaNetMessage(FromAddress, DefaultObjects->MambaNetAddress, Ack, 0, 1, TransmitBuffer, 6);
                           }
                           break;
                           case 7:
                           {  //FirmwareMajorRevision
                              TransmitBuffer[0] = (ObjectNr>>8)&0xFF;
                              TransmitBuffer[1] = ObjectNr&0xFF;
                              TransmitBuffer[2] = MAMBANET_OBJECT_ACTION_SENSOR_DATA_RESPONSE;
                              TransmitBuffer[3] = UNSIGNED_INTEGER_DATATYPE;
                              TransmitBuffer[4] = 1;
                              TransmitBuffer[5] = DefaultObjects->FirmwareMajorRevision;

                              SendMambaNetMessage(FromAddress, DefaultObjects->MambaNetAddress, Ack, 0, 1, TransmitBuffer, 6);
                           }
                           break;
                           case 8:
                           {  //FirmwareMinorRevision
                              TransmitBuffer[0] = (ObjectNr>>8)&0xFF;
                              TransmitBuffer[1] = ObjectNr&0xFF;
                              TransmitBuffer[2] = MAMBANET_OBJECT_ACTION_SENSOR_DATA_RESPONSE;
                              TransmitBuffer[3] = UNSIGNED_INTEGER_DATATYPE;
                              TransmitBuffer[4] = 1;
                              TransmitBuffer[5] = DefaultObjects->FirmwareMinorRevision;

                              SendMambaNetMessage(FromAddress, DefaultObjects->MambaNetAddress, Ack, 0, 1, TransmitBuffer, 6);
                           }
                           break;
                           case 9:
                           {  //FPGAFirmwareMajorRevision
                              TransmitBuffer[0] = (ObjectNr>>8)&0xFF;
                              TransmitBuffer[1] = ObjectNr&0xFF;
                              TransmitBuffer[2] = MAMBANET_OBJECT_ACTION_SENSOR_DATA_RESPONSE;
                              TransmitBuffer[3] = UNSIGNED_INTEGER_DATATYPE;
                              TransmitBuffer[4] = 1;
                              TransmitBuffer[5] = 0;

                              SendMambaNetMessage(FromAddress, DefaultObjects->MambaNetAddress, Ack, 0, 1, TransmitBuffer, 6);
                           }
                           break;
                           case 10:
                           {  //FPGAFirmwareMinorRevision
                              TransmitBuffer[0] = (ObjectNr>>8)&0xFF;
                              TransmitBuffer[1] = ObjectNr&0xFF;
                              TransmitBuffer[2] = MAMBANET_OBJECT_ACTION_SENSOR_DATA_RESPONSE;
                              TransmitBuffer[3] = UNSIGNED_INTEGER_DATATYPE;
                              TransmitBuffer[4] = 1;
                              TransmitBuffer[5] = 0;

                              SendMambaNetMessage(FromAddress, DefaultObjects->MambaNetAddress, Ack, 0, 1, TransmitBuffer, 6);
                           }
                           break;
                           case 11:
                           {  //ProtocolMajorRevision
                              TransmitBuffer[0] = (ObjectNr>>8)&0xFF;
                              TransmitBuffer[1] = ObjectNr&0xFF;
                              TransmitBuffer[2] = MAMBANET_OBJECT_ACTION_SENSOR_DATA_RESPONSE;
                              TransmitBuffer[3] = UNSIGNED_INTEGER_DATATYPE;
                              TransmitBuffer[4] = 1;
                              TransmitBuffer[5] = DefaultObjects->ProtocolMajorRevision;

                              SendMambaNetMessage(FromAddress, DefaultObjects->MambaNetAddress, Ack, 0, 1, TransmitBuffer, 6);
                           }
                           break;
                           case 12:
                           {  //ProtocolMinorRevision
                              TransmitBuffer[0] = (ObjectNr>>8)&0xFF;
                              TransmitBuffer[1] = ObjectNr&0xFF;
                              TransmitBuffer[2] = MAMBANET_OBJECT_ACTION_SENSOR_DATA_RESPONSE;
                              TransmitBuffer[3] = UNSIGNED_INTEGER_DATATYPE;
                              TransmitBuffer[4] = 1;
                              TransmitBuffer[5] = DefaultObjects->ProtocolMinorRevision;

                              SendMambaNetMessage(FromAddress, DefaultObjects->MambaNetAddress, Ack, 0, 1, TransmitBuffer, 6);
                           }
                           break;
                           case 13:
                           {  //Number of Objects
                              TransmitBuffer[0] = (ObjectNr>>8)&0xFF;
                              TransmitBuffer[1] = ObjectNr&0xFF;
                              TransmitBuffer[2] = MAMBANET_OBJECT_ACTION_SENSOR_DATA_RESPONSE;
                              TransmitBuffer[3] = UNSIGNED_INTEGER_DATATYPE;
                              TransmitBuffer[4] = 2;
                              TransmitBuffer[5] = (DefaultObjects->NumberOfObjects>>8)&0xFF;
                              TransmitBuffer[6] = DefaultObjects->NumberOfObjects&0xFF;

                              SendMambaNetMessage(FromAddress, DefaultObjects->MambaNetAddress, Ack, 0, 1, TransmitBuffer, 7);
                           }
                           break;
   //                      case 14: Default engine address is an actuator
                           case 15:
                           { //Hardware parent
                              TransmitBuffer[0] = (ObjectNr>>8)&0xFF;
                              TransmitBuffer[1] = ObjectNr&0xFF;
                              TransmitBuffer[2] = MAMBANET_OBJECT_ACTION_SENSOR_DATA_RESPONSE;
                              TransmitBuffer[3] = OCTET_STRING_DATATYPE;
                              TransmitBuffer[4] = 6;
                              TransmitBuffer[5] = DefaultObjects->Parent[0];
                              TransmitBuffer[6] = DefaultObjects->Parent[1];
                              TransmitBuffer[7] = DefaultObjects->Parent[2];
                              TransmitBuffer[8] = DefaultObjects->Parent[3];
                              TransmitBuffer[9] = DefaultObjects->Parent[4];
                              TransmitBuffer[10] = DefaultObjects->Parent[5];

                              SendMambaNetMessage(FromAddress, DefaultObjects->MambaNetAddress, Ack, 0, 1, TransmitBuffer, 11);
                           }
                           break;
                           case 16:
                           { //Service requested
                           }
                           break;
                           default:
                           {
                              TransmitBuffer[0] = (ObjectNr>>8)&0xFF;
                              TransmitBuffer[1] = ObjectNr&0xFF;
                              TransmitBuffer[2] = MAMBANET_OBJECT_ACTION_SENSOR_DATA_RESPONSE;
                              TransmitBuffer[3] = ERROR_DATATYPE;
                              TransmitBuffer[4] = 15;
                              sprintf((char *)&TransmitBuffer[5], "Not implemented");

                              SendMambaNetMessage(FromAddress, DefaultObjects->MambaNetAddress, Ack, 0, 1, TransmitBuffer, 20);
                              MambaNetMessageProcessed = 1;
                           }
                           break;
                        }
                        MambaNetMessageProcessed = 1;
                     }
                  }
                  break;
                  case  MAMBANET_OBJECT_ACTION_SENSOR_DATA_RESPONSE:
                  {       //Not implemented
                  }
                  break;
                  case  MAMBANET_OBJECT_ACTION_GET_ACTUATOR_DATA:
                  {
                     if (ObjectNr<1024)
                     {  //Only for the standard obects.
                        unsigned char TransmitBuffer[37];
                        char cntByte;

                        switch (ObjectNr)
                        {
                           case 1:
                           {  //Name
                              TransmitBuffer[0] = (ObjectNr>>8)&0xFF;
                              TransmitBuffer[1] = ObjectNr&0xFF;
                              TransmitBuffer[2] = MAMBANET_OBJECT_ACTION_ACTUATOR_DATA_RESPONSE;
                              TransmitBuffer[3] = OCTET_STRING_DATATYPE;
                              TransmitBuffer[4] = 32;
                              for (cntByte=0; cntByte<32; cntByte++)
                              {
                                 TransmitBuffer[5+cntByte] = DefaultObjects->Name[cntByte];
                              }

                              SendMambaNetMessage(FromAddress, DefaultObjects->MambaNetAddress, Ack, 0, 1, TransmitBuffer, 37);
                           }
                           break;
                           case 12:
                           {  //Default engine address
                              TransmitBuffer[0] = (ObjectNr>>8)&0xFF;
                              TransmitBuffer[1] = ObjectNr&0xFF;
                              TransmitBuffer[2] = MAMBANET_OBJECT_ACTION_ACTUATOR_DATA_RESPONSE;
                              TransmitBuffer[3] = UNSIGNED_INTEGER_DATATYPE;
                              TransmitBuffer[4] = 4;
                              TransmitBuffer[5] = (DefaultObjects->DefaultEngineMambaNetAddress>>24)&0xFF;
                              TransmitBuffer[6] = (DefaultObjects->DefaultEngineMambaNetAddress>>16)&0xFF;
                              TransmitBuffer[7] = (DefaultObjects->DefaultEngineMambaNetAddress>>8 )&0xFF;
                              TransmitBuffer[8] =  DefaultObjects->DefaultEngineMambaNetAddress     &0xFF;

                              SendMambaNetMessage(FromAddress, DefaultObjects->MambaNetAddress, Ack, 0, 1, TransmitBuffer, 9);
                           }
                           break;
                           default:
                           {
                              TransmitBuffer[0] = (ObjectNr>>8)&0xFF;
                              TransmitBuffer[1] = ObjectNr&0xFF;
                              TransmitBuffer[2] = MAMBANET_OBJECT_ACTION_ACTUATOR_DATA_RESPONSE;
                              TransmitBuffer[3] = ERROR_DATATYPE;
                              TransmitBuffer[4] = 15;
                              sprintf((char *)&TransmitBuffer[5], "Not implemented");

                              SendMambaNetMessage(FromAddress, DefaultObjects->MambaNetAddress, Ack, 0, 1, TransmitBuffer, 20);
                           }
                           break;
                        }
                        MambaNetMessageProcessed = 1;
                     }
                  }
                  break;
                  case  MAMBANET_OBJECT_ACTION_ACTUATOR_DATA_RESPONSE:
                  {       //Not implemented
                  }
                  break;
                  case  MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA:
                  {
                     if (ObjectNr<1024)
                     {
                        unsigned char DataType;
                        unsigned char DataSize;

                        DataType = Data[3];
                        DataSize = Data[4];

                        switch (ObjectNr)
                        {
                           case 1:
                           {  //Name
                              if (DataType == OCTET_STRING_DATATYPE)
                              {
                                 if (DataSize <= 32)
                                 {
                                    char cntChar;
                                    char TextString[33];

                                    for (cntChar=0; cntChar<DataSize; cntChar++)
                                    {
                                       DefaultObjects->Name[cntChar] = Data[5+cntChar];
                                    }
                                    for (cntChar=DataSize; cntChar<32; cntChar++)
                                    {
                                       TextString[cntChar] = ' ';
                                    }
                                 }
                              }
                           }
                           break;
                           case 14:
                           {  //engine address
                              if (DataType == UNSIGNED_INTEGER_DATATYPE)
                              {
                                 if (DataSize == 4)
                                 {
                                    unsigned long int EngineMambaNetAddress;

                                    EngineMambaNetAddress = Data[5];
                                    EngineMambaNetAddress <<= 8;
                                    EngineMambaNetAddress |= Data[6];
                                    EngineMambaNetAddress <<= 8;
                                    EngineMambaNetAddress |= Data[7];
                                    EngineMambaNetAddress <<= 8;
                                    EngineMambaNetAddress |= Data[8];

                                    DefaultObjects->DefaultEngineMambaNetAddress = EngineMambaNetAddress;
                                 }
                              }
                           }
                           break;
                           default:
                           {
                              unsigned char TransmitBuffer[20];

                              TransmitBuffer[0] = (ObjectNr>>8)&0xFF;
                              TransmitBuffer[1] = ObjectNr&0xFF;
                              TransmitBuffer[2] = MAMBANET_OBJECT_ACTION_ACTUATOR_DATA_RESPONSE;
                              TransmitBuffer[3] = ERROR_DATATYPE;
                              TransmitBuffer[4] = 15;
                              sprintf((char *)&TransmitBuffer[5], "Not implemented");

                              SendMambaNetMessage(FromAddress, DefaultObjects->MambaNetAddress, Ack, 0, 1, TransmitBuffer, 20);
                              MambaNetMessageProcessed = 1;
                           }
                           break;
                        }
                        MambaNetMessageProcessed = 1;
                     }
                     else
                     {
                        unsigned long int EngineMambaNetAddress;

                        EngineMambaNetAddress = 0;//ObjectWritableInformation[ObjectNr-1024].EngineMambaNetAddress;

                        //if engine address is empty or set to broadcast message, we must be processed in the application
                        if ((EngineMambaNetAddress != 0x00000000) && (EngineMambaNetAddress != 0x10000000))
                        {
                           //if engine address set but not equal to transmitter, we must NOT process the message in the application.
                           if (FromAddress != EngineMambaNetAddress)
                           {
                              MambaNetMessageProcessed = 1;
                           }
                        }
                     }
                  }
                  break;
               }
            }
            break;
      	}

         if ((!MambaNetMessageProcessed) && (DefaultObjects->Services&0x80))
         {
            if (Interfaces[FromInterfaceIndex].Type != NO_INTERFACE)
            {
               if (Interfaces[FromInterfaceIndex].ReceiveCallback != NULL)
               {
                  Interfaces[FromInterfaceIndex].ReceiveCallback(ToAddress, FromAddress, Ack, MessageID, MessageType, Data, DataLength, NULL);
               }
            }
         }
      }
   }
}

//This supporting function enables a message trace to the stdin.
//If a bit is high, it toggles the trace state.
//
// bit 0, Value = 0x01 - Packet trace
// bit 1, Value = 0x02 - Address table trace
//
void STDCALL ChangeMambaNetStackTrace(unsigned char Value)
{
	TraceOptions ^= Value;
}

//make a 32 float representation.
//8 bit  = seee mmmm
//16 bit = seee eemm mmmm mmmm
//32 bit = seee eeee emmm mmmm mmmm mmmm mmmm mmmm
unsigned char STDCALL VariableFloat2Float(unsigned char *VariableFloatBuffer, unsigned char VariableFloatBufferSize, float *ReturnFloatValue)
{
   unsigned long TemporyForCast;
   int exponent;
   unsigned long mantessa;
   char signbit;
   unsigned char ReturnStatus;
   char cntSize;
   char NonZero;

   ReturnStatus = 0;

   NonZero=0;
   for (cntSize=0; cntSize<VariableFloatBufferSize; cntSize++)
   {
      if (VariableFloatBuffer[cntSize] != 0x00)
      {
         NonZero = 1;
      }
   }

   if (NonZero)
   {
      switch (VariableFloatBufferSize)
      {
         case 1:
         {
            signbit = (VariableFloatBuffer[0]>>7)&0x01;
            exponent = (VariableFloatBuffer[0]>>4)&0x07;
            mantessa = (VariableFloatBuffer[0]   )&0x0F;
            if (exponent == 0)
            {  //denormalized
               exponent = -127;
            }
            else if (exponent == 7)
            {  //+/-INF or NaN, depends on sign
               exponent = 128;
            }
            else
            {
               exponent -= 3;
            }


            TemporyForCast = signbit;
            TemporyForCast <<= 8;
            TemporyForCast |= (exponent+127)&0xFF;
            TemporyForCast <<= 4;
            TemporyForCast |= mantessa&0x0F;
            TemporyForCast <<= 8;
            TemporyForCast <<= 8;
            TemporyForCast <<= 3;

            *ReturnFloatValue = *((float *)&TemporyForCast);
         }
         break;
         case 2:
         {
            signbit = (VariableFloatBuffer[0]>>7)&0x01;
            exponent = (VariableFloatBuffer[0]>>2)&0x1F;
            mantessa = (VariableFloatBuffer[0]   )&0x03;
            mantessa <<= 8;
            mantessa |= (VariableFloatBuffer[1]   )&0xFF;
            if (exponent == 0)
            {  //denormalized
               exponent = -127;
            }
            else if (exponent == 31)
            {  //+/-INF, NaN, depends on sign
               exponent = 128;
            }
            else
            {
               exponent -= 15;
            }

            TemporyForCast = signbit;
            TemporyForCast <<= 8;
            TemporyForCast |= (exponent+127)&0xFF;
            TemporyForCast <<= 2;
            TemporyForCast |= (mantessa>>8)&0x03;
            TemporyForCast <<= 8;
            TemporyForCast |= (mantessa   )&0xFF;
            TemporyForCast <<= 8;
            TemporyForCast <<= 5;

            *ReturnFloatValue = *((float *)&TemporyForCast);
         }
         break;
         case 4:
         {
            TemporyForCast = VariableFloatBuffer[0];
            TemporyForCast <<= 8;
            TemporyForCast |= VariableFloatBuffer[1];
            TemporyForCast <<= 8;
            TemporyForCast |= VariableFloatBuffer[2];
            TemporyForCast <<= 8;
            TemporyForCast |= VariableFloatBuffer[3];

            *ReturnFloatValue = *((float *)&TemporyForCast);
         }
         break;
         default:
         {
            ReturnStatus = 1;
         }
         break;
      }
   }
   else
   { //NonZero = 0;
      TemporyForCast = 0x00000000;
      *ReturnFloatValue = *((float *)&TemporyForCast);
   }

   return ReturnStatus;
}

//make a 32 float representation.
//8 bit  = seee mmmm
//16 bit = seee eemm mmmm mmmm
//32 bit = seee eeee emmm mmmm mmm mmmmm mmmm mmmm
unsigned char STDCALL Float2VariableFloat(float InputFloat, unsigned char VariableFloatBufferSize, unsigned char *FloatBuffer)
{
   unsigned long TemporyCastedFloat;
   int exponent;
   unsigned long mantessa;
   char signbit;
   unsigned char ReturnStatus;

   ReturnStatus = 1;

   TemporyCastedFloat = *((unsigned long *)&InputFloat);

   mantessa = TemporyCastedFloat&0x007FFFFF;

   exponent = (TemporyCastedFloat>>23)&0xFF;
   exponent -= 127;

   signbit = (TemporyCastedFloat>>(23+8))&0x01;

   if (TemporyCastedFloat == 0x00000000)
   {
      char cntSize;

      for (cntSize=0; cntSize<VariableFloatBufferSize; cntSize++)
      {
         FloatBuffer[cntSize] = 0x00;
      }
      ReturnStatus = 0;
   }
   else
   {
      switch (VariableFloatBufferSize)
      {
         case 1:
         {
            exponent += 3;
            if (exponent<7)
            {
               if (exponent<0)
               {
                  exponent = 0;
               }
               FloatBuffer[0] = signbit;
               FloatBuffer[0] <<= 3;
               FloatBuffer[0] |= (exponent&0x7);
               FloatBuffer[0] <<= 4;
               FloatBuffer[0] |= (mantessa>>19)&0x0F;

               ReturnStatus = 0;
            }
            else if (exponent > 6)
            {  //+/-INF (or NaN)
               exponent = 7;
               mantessa = 0;

               FloatBuffer[0] = signbit;
               FloatBuffer[0] <<= 3;
               FloatBuffer[0] |= (exponent&0x7);
               FloatBuffer[0] <<= 4;
               FloatBuffer[0] |= (mantessa>>19)&0x0F;

               ReturnStatus = 0;
            }
         }
         break;
         case 2:
         {
            exponent += 15;
            if (exponent<31)
            {
               if (exponent<0)
               {
                  exponent = 0;
               }
               FloatBuffer[0] = signbit;
               FloatBuffer[0] <<= 5;
               FloatBuffer[0] |= (exponent&0x1F);
               FloatBuffer[0] <<= 2;
               FloatBuffer[0] |= (mantessa>>21)&0x03;
               FloatBuffer[1]  = (mantessa>>13)&0xFF;

               ReturnStatus = 0;
            }
            else if (exponent > 30)
            {  //+/-INF (or NaN)
               exponent = 31;
               mantessa = 0;

               FloatBuffer[0] = signbit;
               FloatBuffer[0] <<= 3;
               FloatBuffer[0] |= (exponent&0x1F);
               FloatBuffer[0] <<= 4;
               FloatBuffer[0] |= (mantessa>>19)&0x0F;

               ReturnStatus = 0;
            }
         }
         break;
         case 4:
         {
            FloatBuffer[0] = (TemporyCastedFloat>>24)&0xFF;
            FloatBuffer[1] = (TemporyCastedFloat>>16)&0xFF;
            FloatBuffer[2] = (TemporyCastedFloat>> 8)&0xFF;
            FloatBuffer[3] = (TemporyCastedFloat    )&0xFF;

            ReturnStatus = 0;
         }
         break;
      }
   }

   return ReturnStatus;
}

unsigned char STDCALL Data2ASCIIString(char ASCIIString[128], unsigned char DataType, unsigned char DataSize, unsigned char *PtrData)
{
	unsigned char ReturnValue = 1;

	switch (DataType)
	{
		case NO_DATA_DATATYPE:
		{
			sprintf(ASCIIString, "[No data]");
			ReturnValue = 0;
		}
		break;
		case UNSIGNED_INTEGER_DATATYPE:
		{
			unsigned long TempData = 0;

			for (int cntByte=0; cntByte<DataSize; cntByte++)
			{
				TempData <<= 8;
				TempData |= PtrData[cntByte];
			}
			sprintf(ASCIIString, "%d", TempData);
			ReturnValue = 0;
		}
		break;
		case SIGNED_INTEGER_DATATYPE:
		{
			long TempData = 0;
			if (PtrData[0]&0x80)
			{	//signed
				TempData = (unsigned long)0xFFFFFFFF;
			}

			for (int cntByte=0; cntByte<DataSize; cntByte++)
			{
				TempData <<= 8;
				TempData |= PtrData[cntByte];
			}

			sprintf(ASCIIString, "%d", TempData);
			ReturnValue = 0;
		}
		break;
		case STATE_DATATYPE:
		{
			unsigned long TempData = 0;

			for (int cntByte=0; cntByte<DataSize; cntByte++)
			{
				TempData <<= 8;
				TempData |= PtrData[cntByte];
			}
			sprintf(ASCIIString, "%d", TempData);
			ReturnValue = 0;
		}
		break;
		case OCTET_STRING_DATATYPE:
		{
			char TempString[256] = "";
			strncpy(TempString, (char *)PtrData, DataSize);
			TempString[DataSize] = 0;

			sprintf(ASCIIString, "%s\0x00", TempString);
			ReturnValue = 0;
		}
		break;
		case FLOAT_DATATYPE:
		{
			float TempFloat = 0;
			if (VariableFloat2Float(PtrData, DataSize, &TempFloat) == 0)
			{
				sprintf(ASCIIString, "%f", TempFloat);
				ReturnValue = 0;
			}
		}
		break;
		case BIT_STRING_DATATYPE:
		{
			char cntByte;
			char cntBit;

			strcpy(ASCIIString, "");

			for (cntByte=0; cntByte<DataSize; cntByte++)
			{
				if (cntByte != 0)
				{
					strcat(ASCIIString, " ");
				}

				for (cntBit=0; cntBit<8; cntBit++)
				{
					unsigned char Mask = 0x80>>cntBit;
					if (PtrData[cntByte] & Mask)
					{
						strcat(ASCIIString, "1");
					}
					else
					{
						strcat(ASCIIString, "0");
					}
				}
			}
			ReturnValue = 0;
		}
		break;
		case ERROR_DATATYPE:
		{
			char TempString[256] = "";
			strncpy(TempString, (char *)PtrData, DataSize);
			TempString[DataSize] = 0;

			sprintf(ASCIIString, "%s\0x00", TempString);
			ReturnValue = 0;
		}
		break;
	}


	return ReturnValue;
}

int STDCALL RegisterMambaNetInterface(INTERFACE_PARAMETER_STRUCT *InterfaceParameters)
{
   int ReturnValue = -1;
   unsigned char Done = 0;
   int cntInterface=0;

   while (!Done)
   {
     	if (Interfaces[cntInterface].Type == NO_INTERFACE)
      {
         Interfaces[cntInterface].Type = InterfaceParameters->Type;
      	memcpy(Interfaces[cntInterface].HardwareAddress, InterfaceParameters->HardwareAddress, 16);
         Interfaces[cntInterface].TransmitCallback = InterfaceParameters->TransmitCallback;
         Interfaces[cntInterface].ReceiveCallback = InterfaceParameters->ReceiveCallback;
         Interfaces[cntInterface].AddressTableChangeCallback = InterfaceParameters->AddressTableChangeCallback;
         Interfaces[cntInterface].TransmitFilter = InterfaceParameters->TransmitFilter;
         Interfaces[cntInterface].cntMessageReceived = 0;
         Interfaces[cntInterface].cntMessageTransmitted = 0;

         ReturnValue = cntInterface;

         Done = 1;
      }

      cntInterface++;
      if (cntInterface>15)
      {
         Done = 1;
      }
   }
   if (cntInterface<16)
   {
      InterfaceCount = cntInterface+1;
   }

   SendMambaNetReservationInfo();

   return ReturnValue;
}

void STDCALL UnregisterMambaNetInterface(int InterfaceIndex)
{
   if (Interfaces[InterfaceIndex].Type != NO_INTERFACE)
   {
      AddressTableCount = 0;
     	for (long cntAddress=0; cntAddress<AddressTableCount; cntAddress++)
     	{
     		if (AddressTable[cntAddress].ReceivedInterfaceIndex == InterfaceIndex)
         {
            if (Interfaces[InterfaceIndex].AddressTableChangeCallback != NULL)
            {
               Interfaces[InterfaceIndex].AddressTableChangeCallback(AddressTable, ADDRESS_TABLE_ENTRY_TO_REMOVE, cntAddress);
            }
        		AddressTable[cntAddress].ManufacturerID				   = 0;
        		AddressTable[cntAddress].ProductID						   = 0;
        		AddressTable[cntAddress].UniqueIDPerProduct			   = 0;
        		AddressTable[cntAddress].MambaNetAddress				   = 0x00000000;
            AddressTable[cntAddress].DefaultEngineMambaNetAddress = 0x00000000;
            AddressTable[cntAddress].NodeServices                 = 0x00;
            memset(AddressTable[cntAddress].HardwareAddress, 0x00, 16);
        		AddressTable[cntAddress].ReceivedInterfaceIndex       = -1;
            AddressTable[cntAddress].Alive                        = 0;
      	}

         if (AddressTable[cntAddress].MambaNetAddress != 0x00000000)
         {
            if ((cntAddress+1) > AddressTableCount)
            {
               AddressTableCount = cntAddress+1;
            }
         }
      }

      Interfaces[InterfaceIndex].Type = NO_INTERFACE;
      memset(Interfaces[InterfaceIndex].HardwareAddress, 0x00, 16);

      Interfaces[InterfaceIndex].TransmitCallback = NULL;
      Interfaces[InterfaceIndex].ReceiveCallback = NULL;
      Interfaces[InterfaceIndex].AddressTableChangeCallback = NULL;
      Interfaces[InterfaceIndex].cntMessageReceived = 0;
      Interfaces[InterfaceIndex].cntMessageTransmitted = 0;
   }

   if (InterfaceCount == InterfaceIndex+1)
   {
      InterfaceCount--;
   }
}

void SendMambaNetReservationInfo()
{
   unsigned char Data[16];

   if (DefaultObjects->ManufacturerID != 0)
   {
      Data[0] = MAMBANET_ADDRESS_RESERVATION_TYPE_INFO;   //Type is reservation
      Data[1] = (DefaultObjects->ManufacturerID>>8)&0xFF;
      Data[2] = DefaultObjects->ManufacturerID&0xFF;
      Data[3] = (DefaultObjects->ProductID>>8)&0xFF;
      Data[4] = DefaultObjects->ProductID&0xFF;
      Data[5] = (DefaultObjects->UniqueIDPerProduct>>8)&0xFF;
      Data[6] = DefaultObjects->UniqueIDPerProduct&0xFF;
      Data[7] = (DefaultObjects->MambaNetAddress>>24)&0xFF;
      Data[8] = (DefaultObjects->MambaNetAddress>>16)&0xFF;
      Data[9] = (DefaultObjects->MambaNetAddress>> 8)&0xFF;
      Data[10] = (DefaultObjects->MambaNetAddress    )&0xFF;
      Data[11] = (DefaultObjects->DefaultEngineMambaNetAddress>>24)&0xFF;
      Data[12] = (DefaultObjects->DefaultEngineMambaNetAddress>>16)&0xFF;
      Data[13] = (DefaultObjects->DefaultEngineMambaNetAddress>> 8)&0xFF;
      Data[14] = (DefaultObjects->DefaultEngineMambaNetAddress    )&0xFF;
      Data[15] = DefaultObjects->Services;

      for (int cntInterface=0; cntInterface<InterfaceCount; cntInterface++)
      {
         if (Interfaces[cntInterface].Type != NO_INTERFACE)
         {
            SendMambaNetMessage(0x10000000, DefaultObjects->MambaNetAddress, 0, 0, 0, Data, 16, cntInterface);
         }
      }
   }
   
   // reset timer
   timerReservationInfo = DefaultObjects->Services & 0x80 ? 30 : 1;
}

void STDCALL MambaNetReservationTimerTick()
{
   if(--timerReservationInfo == 0)
   {
      SendMambaNetReservationInfo();
   }

   for (long cntAddressEntry=0; cntAddressEntry<AddressTableCount; cntAddressEntry++)
   {
      if (AddressTable[cntAddressEntry].ManufacturerID != 0x0000)
      {
         if (AddressTable[cntAddressEntry].Alive)
         {
            AddressTable[cntAddressEntry].Alive--;
            if (AddressTable[cntAddressEntry].Alive == 0)
            {
               //entry timeout
               TRACE_ADDRESSTABLE(printf("[---] Timeout@%d ManID:0x%04X, ProdID:0x%04X, UniqID:0x%04X, MambaNetAddr:0x%08X\n", cntAddressEntry, AddressTable[cntAddressEntry].ManufacturerID, AddressTable[cntAddressEntry].ProductID, AddressTable[cntAddressEntry].UniqueIDPerProduct, AddressTable[cntAddressEntry].MambaNetAddress);)

               AddressTable[cntAddressEntry].NodeServices &= 0x7F;

               int InterfaceIndex = AddressTable[cntAddressEntry].ReceivedInterfaceIndex;
               if (Interfaces[InterfaceIndex].Type != NO_INTERFACE)
               {
                  if (Interfaces[InterfaceIndex].AddressTableChangeCallback != NULL)
                  {
                     Interfaces[InterfaceIndex].AddressTableChangeCallback(AddressTable, ADDRESS_TABLE_ENTRY_TIMEOUT, cntAddressEntry);
                  }
               }
            }
         }
      }
   }
}


