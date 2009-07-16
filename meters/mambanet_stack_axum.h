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

#ifndef MAMBANET_STACK_AXUM_H
#define MAMBANET_STACK_AXUM_H

#if defined(_MSC_VER) || defined(__BORLANDC__)
  #ifdef __DLL__
    #define DLLEXPORT __declspec(dllexport)
  #else
    #define DLLEXPORT __declspec(dllimport)
  #endif
  #define DLLLOCAL
  #define STDCALL __stdcall
#else
  #ifdef HAVE_GCCVISIBILITYPATCH
    #define DLLEXPORT __attribute__ ((visibility("default")))
    #define DLLLOCAL __attribute__ ((visibility("hidden")))
  #else
    #define DLLEXPORT
    #define DLLLOCAL
  #endif
  #define STDCALL
#endif

#define PROTOCOL_MAJOR_VERSION   0
#define PROTOCOL_MINOR_VERSION   1

/**************************************************************
Virtual D&R protocol number, used for testing/debugging
**************************************************************/
#define ETH_P_DNR		0x8820

/**************************************************************
Defines for the MambaNet Protocol
**************************************************************/
#define MAX_MAMBANET_PACKET_SIZE     128		//Number of 7 bits bytes
#define MAX_MAMBANET_DATA_SIZE		 98		//Number of 8 bit data bytes
#define PROTOCOL_OVERHEAD          	 16		//Number of transport protocol defined bytes

#define MAMBANET_ADDRESS_RESERVATION_MESSAGETYPE   0
   //MAMBANET_ADDRESS_RESERVATION_
   #define MAMBANET_ADDRESS_RESERVATION_TYPE_INFO           0
   #define MAMBANET_ADDRESS_RESERVATION_TYPE_PING           1
   #define MAMBANET_ADDRESS_RESERVATION_TYPE_RESPONSE       2
   
#define MAMBANET_OBJECT_MESSAGETYPE                1
	//Object Info   
	#define MAMBANET_OBJECT_ACTION_GET_INFORMATION           0
	#define MAMBANET_OBJECT_ACTION_INFORMATION_RESPONSE      1
   #define MAMBANET_OBJECT_ACTION_GET_ENGINE_ADDRESS        2
   #define MAMBANET_OBJECT_ACTION_ENGINE_ADDRESS_RESPONSE   3
   #define MAMBANET_OBJECT_ACTION_SET_ENGINE_ADDRESS        4
   #define MAMBANET_OBJECT_ACTION_GET_OBJECT_FREQUENCY      5
   #define MAMBANET_OBJECT_ACTION_OBJECT_FREQUENCY_RESPONSE 6
   #define MAMBANET_OBJECT_ACTION_SET_OBJECT_FREQUENCY      7

	//Sensor
	#define MAMBANET_OBJECT_ACTION_GET_SENSOR_DATA          32
	#define MAMBANET_OBJECT_ACTION_SENSOR_DATA_RESPONSE     33
	#define MAMBANET_OBJECT_ACTION_SENSOR_DATA_CHANGED      34
	#define MAMBANET_OBJECT_ACTION_SET_SENSOR_DESTINATION   35
	//Actuator
	#define MAMBANET_OBJECT_ACTION_GET_ACTUATOR_DATA        64
	#define MAMBANET_OBJECT_ACTION_ACTUATOR_DATA_RESPONSE   65
	#define MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA        66

#define MAMBANET_DEBUG_MESSAGETYPE                 16383

#define NO_DATA_DATATYPE                        0
#define UNSIGNED_INTEGER_DATATYPE               1
#define SIGNED_INTEGER_DATATYPE                 2
#define STATE_DATATYPE                          3
#define OCTET_STRING_DATATYPE                   4
#define FLOAT_DATATYPE                     		5
#define BIT_STRING_DATATYPE                     6
#define OBJECT_INFORMATION_DATATYPE             128
#define ERROR_DATATYPE                          255

#define ALIVE_TIME                              75

enum MambaNetInterfaceTypeEnum   {NO_INTERFACE, CAN, UART, UART_CAN, ETHERNET, FIREWIRE, TCPIP};
enum MambaNetInterfaceFilter     {ALL_MESSAGES, NO_RESERVATION_INFO, NO_BROADCASTS, NO_RESERVATION_INFO_FROM_OTHERS, NO_BROADCASTS_FROM_OTHERS};
enum MambaNetAddressTableStatus  {ADDRESS_TABLE_ENTRY_ADDED, ADDRESS_TABLE_ENTRY_CHANGED, ADDRESS_TABLE_ENTRY_ACTIVATED, ADDRESS_TABLE_ENTRY_TIMEOUT, ADDRESS_TABLE_ENTRY_TO_REMOVE};

typedef struct
{
   unsigned char  Description[64];           //Fixed
   unsigned char  Name[32];                  //Variable, must be stored
   unsigned int   ManufacturerID;            //Fixed
   unsigned int   ProductID;                 //Fixed
   unsigned int   UniqueIDPerProduct;        //Fixed
   unsigned char  HardwareMajorRevision;     //Fixed
   unsigned char  HardwareMinorRevision;     //Fixed
   unsigned char  FirmwareMajorRevision;     //Fixed
   unsigned char  FirmwareMinorRevision;     //Fixed
   unsigned char  ProtocolMajorRevision;     //Fixed
   unsigned char  ProtocolMinorRevision;     //Fixed
   unsigned int   NumberOfObjects;           //Fixed
   unsigned int   DefaultEngineMambaNetAddress; //Variable
   unsigned char  Parent[6];                 //Variable
   unsigned int   MambaNetAddress;           //Variable
   unsigned char  Services;                  //Fixed/Variables
} DEFAULT_NODE_OBJECTS_STRUCT;

typedef struct
{
   unsigned char DataType;
   unsigned char DataSize;
   unsigned long int DataMinimal;   // is max 32 bits in AVR but could be 64 bits in the protocol
   unsigned long int DataMaximal;   // is max 32 bits in AVR but could be 64 bits in the protocol
} SENSOR_DATA_STRUCT;

typedef struct
{
   unsigned char DataType;
   unsigned char DataSize;
   unsigned long int DataMinimal;   // is max 32 bits in AVR but could be 64 bits in the protocol
   unsigned long int DataMaximal;   // is max 32 bits in AVR but could be 64 bits in the protocol
   unsigned long int DefaultData;   // is max 32 bits in AVR but could be 64 bits in the protocol
} ACTUATOR_DATA_STRUCT;

typedef struct
{
   unsigned char Description[32];
   unsigned char Services;
   unsigned long int EngineMambaNetAddress;
   unsigned char UpdateFrequency;

   SENSOR_DATA_STRUCT Sensor;
   ACTUATOR_DATA_STRUCT Actuator;
} CUSTOM_OBJECT_INFORMATION_STRUCT;

typedef struct
{
	unsigned short 				ManufacturerID;
	unsigned short 				ProductID;
	unsigned short 				UniqueIDPerProduct;
	unsigned long int 		MambaNetAddress;
  unsigned long int     DefaultEngineMambaNetAddress;
  unsigned char         NodeServices;
  unsigned char         HardwareAddress[16];

	int                   ReceivedInterfaceIndex;
  int                   Alive;
} MAMBANET_ADDRESS_STRUCT;

typedef void STDCALL (*interface_transmit_callback)(unsigned char *buffer, unsigned char buffersize, unsigned char hardware_address[16]);
typedef void STDCALL (*interface_receive_callback)(unsigned long int ToAddress, unsigned long int FromAddress, unsigned char Ack, unsigned long int MessageID, unsigned int MessageType, unsigned char *Data, unsigned char DataLength, unsigned char *FromHardwareAddress);
typedef void STDCALL (*interface_address_table_change_callback)(MAMBANET_ADDRESS_STRUCT *AddressTable, MambaNetAddressTableStatus Status, int Index);

typedef struct
{
	MambaNetInterfaceTypeEnum	Type;
	unsigned char					HardwareAddress[16];
   interface_transmit_callback TransmitCallback;
   interface_receive_callback ReceiveCallback;
   interface_address_table_change_callback AddressTableChangeCallback;
   MambaNetInterfaceFilter    TransmitFilter;

   long cntMessageReceived;
   long cntMessageTransmitted;
} INTERFACE_PARAMETER_STRUCT;

extern "C"
{
   DLLEXPORT void STDCALL InitializeMambaNetStack(DEFAULT_NODE_OBJECTS_STRUCT *NewDefaultObjects, CUSTOM_OBJECT_INFORMATION_STRUCT *NewCustomObjectInformation, unsigned int NewNumberOfCustomObjects);

   DLLEXPORT int STDCALL RegisterMambaNetInterface(INTERFACE_PARAMETER_STRUCT *InterfaceParameters);
   DLLEXPORT void STDCALL UnregisterMambaNetInterface(int InterfaceIndex);

   DLLEXPORT void STDCALL MambaNetReservationTimerTick(); //Must be called every second

   DLLEXPORT void STDCALL SendMambaNetMessage(unsigned long int ToAddress, unsigned long int FromAddress, unsigned char Ack, unsigned int MessageID, unsigned int MessageType, unsigned char *Data, unsigned char DataLength, int InterfaceIndexToUse=-1);

   DLLEXPORT unsigned char STDCALL VariableFloat2Float(unsigned char *VariableFloatBuffer, unsigned char VariableFloatBufferSize, float *ReturnFloatValue);
   DLLEXPORT unsigned char STDCALL Float2VariableFloat(float InputFloat, unsigned char VariableFloatBufferSize, unsigned char *FloatBuffer);

   DLLEXPORT unsigned char STDCALL Data2ASCIIString(char ASCIIString[128], unsigned char DataType, unsigned char DataSize, unsigned char *PtrData);

   DLLEXPORT void STDCALL ChangeMambaNetStackTrace(unsigned char Value);

   DLLEXPORT void STDCALL DecodeRawMambaNetMessage(unsigned char *Buffer, unsigned char BufferLength, int FromInterfaceIndex, unsigned char FromHardwareAddress[16]=NULL);
}

void SendMambaNetReservationInfo();
void ProcessMambaNetMessage(unsigned long int ToAddress, unsigned long int FromAddress, unsigned char Ack, unsigned long int MessageID, unsigned int MessageType, unsigned char *Data, unsigned char DataLength, int FromInterfaceIndex, unsigned char FromHardwareAddress[16]=NULL);

unsigned char Encode8to7bits(unsigned char *Buffer, unsigned char BufferLength, unsigned char *Buffer7Bit);
unsigned char Decode7to8bits(unsigned char *Buffer, unsigned char BufferLength, unsigned char *Buffer8Bit);



#endif
