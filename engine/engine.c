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
#include "common.h"
#include "engine.h"
#include "db.h"
#include "dsp.h"

#include <stdio.h>
#include <stdlib.h>         //for atoi
#include <unistd.h>         //for STDIN_FILENO/close/write
#include <fcntl.h>          //for GET_FL/SET_FL/O_XXXXXX/FNDELAY
#include <string.h>         //for memcpy/strncpy
#include <termios.h>            //for termios
#include <sys/ioctl.h>          //for ioctl
#include "engine_functions.h"

#include <arpa/inet.h>      //for AF_PACKET/SOCK_DGRAM/htons/ntohs/socket/bind/sendto
#include <linux/if_arp.h>   //for ETH_P_ALL/ifreq/sockaddr_ll/ETH_ALEN etc...
#include <sys/time.h>       //for setittimer
#include <sys/times.h>      //for tms and times()
#include <sys/signal.h>     //for SIGALRM
#include <sys/un.h>         //for sockaddr_un (UNIX sockets)

#include <errno.h>          //errno and EINTR
#include <time.h>               //nanosleep
#include <sys/mman.h>       //for mmap, PROT_READ, PROT_WRITE

#include <math.h>               //for pow10, log10

#include "ddpci2040.h"
#include "mambanet_stack_axum.h"

#include <pthread.h>
#include <time.h>
#include <sys/errno.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/msg.h>

#define LOG_DEBUG_ENABLED

#ifdef LOG_DEBUG_ENABLED
  #define LOG_DEBUG(...) log_write(__VA_ARGS__)
#else
  #define LOG_DEBUG(...)
#endif

#define DEFAULT_GTW_PATH    "/tmp/axum-gateway"
#define DEFAULT_ETH_DEV     "eth0"
#define DEFAULT_DB_STR      "dbname='axum' user='axum'"
#define DEFAULT_LOG_FILE    "/var/log/axum-engine.log"

#define PCB_MAJOR_VERSION        1
#define PCB_MINOR_VERSION        0

#define FIRMWARE_MAJOR_VERSION   1
#define FIRMWARE_MINOR_VERSION   0

#define MANUFACTURER_ID          0x0001 //D&R
#define PRODUCT_ID               0x000E //Axum Engine

#define NR_OF_STATIC_OBJECTS    (1023-1023)
#define NR_OF_OBJECTS            NR_OF_STATIC_OBJECTS

extern unsigned int AddressTableCount;

/********************************/
/* global declarations          */
/********************************/
DEFAULT_NODE_OBJECTS_STRUCT AxumEngineDefaultObjects =
{
  "Axum Engine (Linux)",              //Description
  "Axum-Engine",                        //Name is stored in EEPROM, see above
  MANUFACTURER_ID,                       //ManufacturerID
  PRODUCT_ID,                            //ProductID
  1,                                     //UniqueIDPerProduct (1=fader, 3=rack)
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
  0x02                        //Services, Engine
};

#if (NR_OF_OBJECTS != 0)
CUSTOM_OBJECT_INFORMATION_STRUCT AxumEngineCustomObjectInformation[NR_OF_STATIC_OBJECTS] =
{
  // Description             , Services
  //                         , sensor {type, size, min, max}
  //                         , actuator {type, size, min, max, default}
  { "Meter 1 Left dB"        , 0x00
    , {NO_DATA_DATATYPE           ,  0, 0     , 0      }
    , {FLOAT_DATATYPE             ,  2, 0xD240, 0x4900, 0xD240 }},
  { "Meter 1 Right dB"       , 0x00
    , {NO_DATA_DATATYPE           ,  0, 0     , 0      }
    , {FLOAT_DATATYPE             ,  2, 0xD240, 0x4900, 0xD240 }}
};
#else
CUSTOM_OBJECT_INFORMATION_STRUCT AxumEngineCustomObjectInformation[1];
#endif

int verbose = 0;

int AxumApplicationAndDSPInitialized = 0;

int cntDebugObject=1024;
int cntDebugNodeObject=0;
float cntFloatDebug = 0;

unsigned char VUMeter = 0;
unsigned char MeterFrequency = 5; //20Hz

AXUM_DATA_STRUCT AxumData;
matrix_sources_struct matrix_sources;

float SummingdBLevel[48];
unsigned int BackplaneMambaNetAddress = 0x00000000;

DSP_HANDLER_STRUCT *dsp_handler;

AXUM_FUNCTION_INFORMATION_STRUCT *SourceFunctions[NUMBER_OF_SOURCES][NUMBER_OF_SOURCE_FUNCTIONS];
AXUM_FUNCTION_INFORMATION_STRUCT *ModuleFunctions[NUMBER_OF_MODULES][NUMBER_OF_MODULE_FUNCTIONS];
AXUM_FUNCTION_INFORMATION_STRUCT *BussFunctions[NUMBER_OF_BUSSES][NUMBER_OF_BUSS_FUNCTIONS];
AXUM_FUNCTION_INFORMATION_STRUCT *MonitorBussFunctions[NUMBER_OF_MONITOR_BUSSES][NUMBER_OF_MONITOR_BUSS_FUNCTIONS];
AXUM_FUNCTION_INFORMATION_STRUCT *DestinationFunctions[NUMBER_OF_DESTINATIONS][NUMBER_OF_DESTINATION_FUNCTIONS];
AXUM_FUNCTION_INFORMATION_STRUCT *GlobalFunctions[NUMBER_OF_GLOBAL_FUNCTIONS];

float Position2dB[1024];
unsigned short int dB2Position[1500];
unsigned int PulseTime;

unsigned char TraceValue;           //To set the MambaNet trace (0x01=packets, 0x02=address table)
bool dump_packages;                 //To debug the incoming data

int NetworkFileDescriptor;          //identifies the used network device
int DatabaseFileDescriptor;

unsigned char EthernetReceiveBuffer[4096];
int cntEthernetReceiveBufferTop;
int cntEthernetReceiveBufferBottom;
unsigned char EthernetMambaNetDecodeBuffer[128];
unsigned char cntEthernetMambaNetDecodeBuffer;

char TTYDevice[256];                    //Buffer to store the serial device name
char NetworkInterface[256];     //Buffer to store the networ device name

unsigned char LocalMACAddress[6];  //Buffer to store local MAC Address

int EthernetInterfaceIndex = -1;

long cntMillisecondTimer;
long PreviousCount_Second;
long PreviousCount_SignalDetect;
long PreviousCount_LevelMeter;
long PreviousCount_PhaseMeter;
long PreviousCount_BroadcastPing;
long cntBroadcastPing;

int LinuxIfIndex;

void *thread(void *vargp);

#define ADDRESS_TABLE_SIZE 65536
ONLINE_NODE_INFORMATION_STRUCT OnlineNodeInformation[ADDRESS_TABLE_SIZE];

//sqlite3 *axum_engine_db;
//sqlite3 *node_templates_db;

int CallbackNodeIndex = -1;

void init(int argc, char **argv)
{
  //struct mbn_interface *itf;
  //char err[MBN_ERRSIZE];
  char ethdev[50];
  char dbstr[256];
  int c;

  strcpy(ethdev, DEFAULT_ETH_DEV);
  strcpy(dbstr, DEFAULT_DB_STR);
  strcpy(log_file, DEFAULT_LOG_FILE);
  strcpy(hwparent_path, DEFAULT_GTW_PATH);

  /* parse options */
  while((c = getopt(argc, argv, "e:d:l:g:i:f:v")) != -1) {
    switch(c) {
      case 'e':
        if(strlen(optarg) > 50) {
          fprintf(stderr, "Too long device name.\n");
          exit(1);
        }
        strcpy(ethdev, optarg);
        break;
      case 'i':
        if(sscanf(optarg, "%d", &(AxumEngineDefaultObjects.UniqueIDPerProduct)) != 1) {
          fprintf(stderr, "Invalid UniqueIDPerProduct");
          exit(1);
        }
        if ((AxumEngineDefaultObjects.UniqueIDPerProduct < 1) || (AxumEngineDefaultObjects.UniqueIDPerProduct > 65535))
        {
          fprintf(stderr, "Unique ID not found or out of range\n");
          exit(1);
        }
        break;
      case 'd':
        if(strlen(optarg) > 256) {
          fprintf(stderr, "Too long database connection string!\n");
          exit(1);
        }
        strcpy(dbstr, optarg);
        break;
      case 'g':
        strcpy(hwparent_path, optarg);
        break;
      case 'l':
        strcpy(log_file, optarg);
        break;
      case 'v':
        verbose = 1;
        break;
      case 'f':
        if (dsp_force_eeprom_prg(optarg))
        {
          fprintf(stderr, "PCI2040 EEPROM NOT programmed in forced mode (%s).\n", optarg);
        }
        else
        {
          printf("PCI2040 EEPROM programmed in forced mode (%s).\n", optarg);
        }
        exit(1);
        break;
      default:
        fprintf(stderr, "Usage: %s [-e dev] [-u path] [-g path] [-d str] [-l path] [-i id]\n", argv[0]);
        fprintf(stderr, "  -e dev   Ethernet device for MambaNet communication.\n");
        fprintf(stderr, "  -i id    UniqueIDPerProduct for the MambaNet node.\n");
        fprintf(stderr, "  -g path  Hardware parent or path to gateway socket.\n");
        fprintf(stderr, "  -l path  Path to log file.\n");
        fprintf(stderr, "  -d str   PostgreSQL database connection options.\n");
        fprintf(stderr, "  -v       Verbose output.\n");
        fprintf(stderr, "  -f dev   force EEPROM programming on device 'dev'.\n");
        exit(1);
    }
  }

  if (!verbose)
    daemonize();

  if (!verbose)
    log_open();

  log_write("----------------------------");
  log_write("Try to start the Axum engine");

  struct mbn_node_info tmp_node;
  hwparent(&tmp_node);

  AxumEngineDefaultObjects.Parent[0] = tmp_node.HardwareParent[0]>>8;
  AxumEngineDefaultObjects.Parent[1] = tmp_node.HardwareParent[0]&0xFF;
  AxumEngineDefaultObjects.Parent[2] = tmp_node.HardwareParent[1]>>8;
  AxumEngineDefaultObjects.Parent[3] = tmp_node.HardwareParent[1]&0xFF;
  AxumEngineDefaultObjects.Parent[4] = tmp_node.HardwareParent[2]>>8;
  AxumEngineDefaultObjects.Parent[5] = tmp_node.HardwareParent[2]&0xFF;

  log_write("hwparent %02X%02X:%02X%02X:%02X%02X\n", AxumEngineDefaultObjects.Parent[0], AxumEngineDefaultObjects.Parent[1], AxumEngineDefaultObjects.Parent[2], AxumEngineDefaultObjects.Parent[3], AxumEngineDefaultObjects.Parent[4], AxumEngineDefaultObjects.Parent[5]);

  db_open(dbstr);
  DatabaseFileDescriptor = db_get_fd();
  if(DatabaseFileDescriptor < 0)
  {
    printf("Invalid PostgreSQL socket\n");
    log_close();
    exit(1);
  }

  db_get_matrix_sources();

  dsp_handler = dsp_open();
  if (dsp_handler == NULL)
  {
    db_close();
    log_close();
    exit(1);
  }

  NetworkFileDescriptor = -1;
  InitializeMambaNetStack(&AxumEngineDefaultObjects, AxumEngineCustomObjectInformation, NR_OF_STATIC_OBJECTS);

  NetworkFileDescriptor = SetupNetwork(ethdev, LocalMACAddress);
  if (NetworkFileDescriptor < 0)
  {
    printf("Error opening network interface: %s\n", ethdev);
    db_close();
    log_close();
    dsp_close(dsp_handler);
    exit(1);
  }

  /* initialize the MambaNet node */
/*  if((itf = mbnEthernetOpen(ethdev, err)) == NULL) {
    fprintf(stderr, "Opening %s: %s\n", ethdev, err);
    log_close();
    db_free();
    exit(1);
  }
  if((mbn = mbnInit(&this_node, NULL, itf, err)) == NULL) {
    fprintf(stderr, "mbnInit: %s\n", err);
    log_close();
    db_free();
    exit(1);
  }
  mbnForceAddress(mbn, 0x0001FFFF);
  mbnSetAddressTableChangeCallback(mbn, mAddressTableChange);
  mbnSetSensorDataResponseCallback(mbn, mSensorDataResponse);
  mbnSetActuatorDataResponseCallback(mbn, mActuatorDataResponse);
  mbnSetReceiveMessageCallback(mbn, mReceiveMessage);
  mbnSetErrorCallback(mbn, mError);
  mbnSetAcknowledgeTimeoutCallback(mbn, mAcknowledgeTimeout);
  */
  if (!verbose)
    daemonize_finish();
  log_write("Axum Engine Initialized");
}

int main(int argc, char *argv[])
{
  int maxfd;
  fd_set readfs;

  init(argc, argv);

//**************************************************************/
//Initialize AXUM Data
//**************************************************************/
  initialize_axum_data_struct();

  /*   SetDSPCard_Interpolation();

      printf("Start initialize Parameters in DSPs\n");
      for (int cntModule=0; cntModule<128; cntModule++)
      {
      SetAxum_ModuleProcessing(cntModule);
      }

      printf("Start buss levels\n");
      for (int cntModule=0; cntModule<128; cntModule++)
      {
      SetAxum_BussLevels(cntModule);
      }

      //CRM
      printf("Start Monitor buss 0\n");
      SetAxum_MonitorBuss(0);

      //Studio
      printf("Start Monitor buss 1\n");
      SetAxum_MonitorBuss(1);*/

  AxumApplicationAndDSPInitialized = 1;
  log_write("Parameters in DSPs initialized");

  //Slot configuration, former rack organization
  db_empty_slot_config();

  //Source configuration
  db_read_src_config(1, 1280);

  //module_configuration
  db_read_module_config(1, 128, 1);

  //buss_configuration
  db_read_buss_config(1, 16);

  //monitor_buss_configuration
  db_read_monitor_buss_config(1, 16);

  //extern_source_configuration
  db_read_extern_src_config(1, 4);

  //talkback_configuration
  db_read_talkback_config(1, 16);

  //global_configuration
  db_read_global_config();

  //destination_configuration
  db_read_dest_config(1, 1280);

  //position to db
  db_read_db_to_position();

  log_write("Axum engine process started, version 2.0");

  //Update default values of EQ to the current values
  for (int cntModule=0; cntModule<128; cntModule++)
  {
    for (int cntEQBand=0; cntEQBand<6; cntEQBand++)
    {
      AxumData.ModuleData[cntModule].EQBand[cntEQBand].Level = 0;
      AxumData.ModuleData[cntModule].EQBand[cntEQBand].Frequency = AxumData.ModuleData[cntModule].EQBand[cntEQBand].DefaultFrequency;
      AxumData.ModuleData[cntModule].EQBand[cntEQBand].Bandwidth = AxumData.ModuleData[cntModule].EQBand[cntEQBand].DefaultBandwidth;
      AxumData.ModuleData[cntModule].EQBand[cntEQBand].Slope = AxumData.ModuleData[cntModule].EQBand[cntEQBand].DefaultSlope;
      AxumData.ModuleData[cntModule].EQBand[cntEQBand].Type = AxumData.ModuleData[cntModule].EQBand[cntEQBand].DefaultType;
    }
  }

  InitalizeAllObjectListPerFunction();

  for (int cntAddress=0; cntAddress<ADDRESS_TABLE_SIZE; cntAddress++)
  {
    OnlineNodeInformation[cntAddress].MambaNetAddress               = 0;
    OnlineNodeInformation[cntAddress].ManufacturerID                = 0;
    OnlineNodeInformation[cntAddress].ProductID                     = 0;
    OnlineNodeInformation[cntAddress].UniqueIDPerProduct            = 0;
    OnlineNodeInformation[cntAddress].FirmwareMajorRevision     = -1;
    OnlineNodeInformation[cntAddress].SlotNumberObjectNr   = -1;
    OnlineNodeInformation[cntAddress].InputChannelCountObjectNr = -1;
    OnlineNodeInformation[cntAddress].OutputChannelCountObjectNr = -1;


    OnlineNodeInformation[cntAddress].Parent.ManufacturerID     = 0;
    OnlineNodeInformation[cntAddress].Parent.ProductID              = 0;
    OnlineNodeInformation[cntAddress].Parent.UniqueIDPerProduct = 0;

    OnlineNodeInformation[cntAddress].NumberOfCustomObjects     = 0;
    OnlineNodeInformation[cntAddress].SensorReceiveFunction     = NULL;
    OnlineNodeInformation[cntAddress].ObjectInformation       = NULL;
  }

  log_write("using network interface device: %s [%02X:%02X:%02X:%02X:%02X:%02X]", NetworkInterface, LocalMACAddress[0], LocalMACAddress[1], LocalMACAddress[2], LocalMACAddress[3], LocalMACAddress[4], LocalMACAddress[5]);

  //Only one interface may be open for a application
  if (NetworkFileDescriptor>=0)
  {
    //**************************************************************/
    //Initialize Timer
    //**************************************************************/
    struct itimerval MillisecondTimeOut;
    struct sigaction signal_action;

    cntBroadcastPing = 6;

    //An alarm timer is used to check the denied user file for changes every minute. Reload the file if it has changed.
    MillisecondTimeOut.it_interval.tv_sec = 0;
    MillisecondTimeOut.it_interval.tv_usec = 10000;
    MillisecondTimeOut.it_value.tv_sec = 0;
    MillisecondTimeOut.it_value.tv_usec = 10000;

    sigemptyset(&signal_action.sa_mask);
    sigaddset(&signal_action.sa_mask, SIGALRM);
    signal_action.sa_flags = 0;
    signal_action.sa_handler = Timer100HzDone;
    sigaction(SIGALRM, &signal_action, 0);

    setitimer(ITIMER_REAL, &MillisecondTimeOut, NULL);


    //initalize maxfd for the idle-wait process 'select'
    maxfd = NetworkFileDescriptor;
    if (DatabaseFileDescriptor > maxfd)
      maxfd = DatabaseFileDescriptor;
    maxfd++;

    while (!main_quit)
    {
      //Set the sources which wakes the idle-wait process 'select'
      //Standard input (keyboard) and network.
      FD_ZERO(&readfs);
      FD_SET(NetworkFileDescriptor, &readfs);
      FD_SET(DatabaseFileDescriptor, &readfs);

      // block (process is in idle mode) until input becomes available
      int ReturnValue = select(maxfd, &readfs, NULL, NULL, NULL);
      if (ReturnValue != -1)
      {//no error or non-blocked signal)
        //Test if the network generated an event.
        if (FD_ISSET(NetworkFileDescriptor, &readfs))
        {
          int cnt;
          unsigned char buffer[2048];
          sockaddr_ll fromAddress;
          int fromlen = sizeof(fromAddress);
          int numRead = recvfrom(NetworkFileDescriptor, buffer, 2048, 0, (struct sockaddr *)&fromAddress, (socklen_t *)&fromlen);
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
                }
              }
            }
            numRead = recvfrom(NetworkFileDescriptor, buffer, 2048, 0, (struct sockaddr *)&fromAddress, (socklen_t *)&fromlen);
          }
        }
        //Test if the database notifier generated an event.
        if (FD_ISSET(DatabaseFileDescriptor, &readfs))
        {
          db_processnotifies();
        }
      }
    }
  }
  DeleteAllObjectListPerFunction();

  dsp_close(dsp_handler);

  CloseNetwork(NetworkFileDescriptor);

  log_write("Closing Engine");
  db_close();
  log_close();

  return 0;
}

//MambaNetMessageReceived is called from the MambaNet stack.
//The function makes the MambaNet functional in this process.
//
//The function in this process implements nothing
void MambaNetMessageReceived(unsigned long int ToAddress, unsigned long int FromAddress, unsigned long int MessageID, unsigned int MessageType, unsigned char *Data, unsigned char DataLength, unsigned char *FromHardwareAddress)
{
  if ((ToAddress == 0x10000000) || (ToAddress == AxumEngineDefaultObjects.MambaNetAddress))
  {
    int IndexOfSender = -1;
    int cntAddress = 0;

    while ((cntAddress<ADDRESS_TABLE_SIZE) && (IndexOfSender == -1))
    {
      if (OnlineNodeInformation[cntAddress].MambaNetAddress == FromAddress)
      {
        IndexOfSender = cntAddress;
      }
      cntAddress++;
    }

    if (IndexOfSender != -1)
    {
      switch (MessageType)
      {
      case MAMBANET_OBJECT_MESSAGETYPE:
      {
        unsigned int ObjectNr = Data[0]<<8;
        ObjectNr |= Data[1];

        if (Data[2] == MAMBANET_OBJECT_ACTION_INFORMATION_RESPONSE)
        {   // object information response
          printf("--- Object information ---\n");
          printf("ObjectNr          : %d\n", ObjectNr);
          if (Data[3] == OBJECT_INFORMATION_DATATYPE)
          {
            char TempString[256] = "";
            strncpy(TempString, (char *)&Data[5], 32);
            TempString[32] = 0;
            printf("Description       : %s\n", TempString);
            int cntData=32;

            unsigned char Service = Data[5+cntData++];
            printf("Service           : %d\n", Service);

            unsigned char DataType = Data[5+cntData++];
            unsigned char DataSize = Data[5+cntData++];
            unsigned char *PtrData = &Data[5+cntData];
            cntData += DataSize;

            printf("Sensor data type  : ");
            switch (DataType)
            {
            case NO_DATA_DATATYPE:
            {
              printf("NO_DATA\n");
            }
            break;
            case UNSIGNED_INTEGER_DATATYPE:
            {
              printf("UNSIGNED_INTEGER\n");
            }
            break;
            case SIGNED_INTEGER_DATATYPE:
            {
              printf("SIGNED_INTEGER\n");
            }
            break;
            case STATE_DATATYPE:
            {
              printf("STATE\n");
            }
            break;
            case OCTET_STRING_DATATYPE:
            {
              printf("OCTET_STRING\n");
            }
            break;
            case FLOAT_DATATYPE:
            {
              printf("FLOAT\n");
            }
            break;
            case BIT_STRING_DATATYPE:
            {
              printf("BIT_STRING\n");
            }
            break;
            case OBJECT_INFORMATION_DATATYPE:
            {
              printf("OBJECT_INFORMATION\n");
            }
            break;
            }
            if (Data2ASCIIString(TempString, DataType, DataSize, PtrData) == 0)
            {
              printf("Sensor minimal    : %s\n", TempString);
            }
            PtrData = &Data[5+cntData];
            cntData += DataSize;
            if (Data2ASCIIString(TempString, DataType, DataSize, PtrData) == 0)
            {
              printf("Sensor maximal    : %s\n", TempString);
            }

            DataType = Data[5+cntData++];
            DataSize = Data[5+cntData++];
            PtrData = &Data[5+cntData];
            cntData += DataSize;

            printf("Actuator data type: ");
            switch (DataType)
            {
            case NO_DATA_DATATYPE:
            {
              printf("NO_DATA\n");
            }
            break;
            case UNSIGNED_INTEGER_DATATYPE:
            {
              printf("UNSIGNED_INTEGER\n");
            }
            break;
            case SIGNED_INTEGER_DATATYPE:
            {
              printf("SIGNED_INTEGER\n");
            }
            break;
            case STATE_DATATYPE:
            {
              printf("STATE\n");
            }
            break;
            case OCTET_STRING_DATATYPE:
            {
              printf("OCTET_STRING\n");
            }
            break;
            case FLOAT_DATATYPE:
            {
              printf("FLOAT\n");
            }
            break;
            case BIT_STRING_DATATYPE:
            {
              printf("BIT_STRING\n");
            }
            break;
            case OBJECT_INFORMATION_DATATYPE:
            {
              printf("OBJECT_INFORMATION\n");
            }
            break;
            }

            if (Data2ASCIIString(TempString, DataType, DataSize, PtrData) == 0)
            {
              printf("Actuator minimal  : %s\n", TempString);
            }
            PtrData = &Data[5+cntData];
            cntData += DataSize;
            if (Data2ASCIIString(TempString, DataType, DataSize, PtrData) == 0)
            {
              printf("Actuator maximal  : %s\n", TempString);
            }
            printf("\n");
          }
          else if (Data[3] == 0)
          {
            printf("No data\n");
            printf("\n");
          }
        }
        else if (Data[2] == MAMBANET_OBJECT_ACTION_SENSOR_DATA_RESPONSE)
        {   // sensor response
          char TempString[256] = "";

          switch (Data[3])
          {
          case NO_DATA_DATATYPE:
          {
            printf(" - No data\n");
          }
          break;
          case UNSIGNED_INTEGER_DATATYPE:
          {
            if (Data2ASCIIString(TempString,Data[3], Data[4], &Data[5]) == 0)
            {
              printf(" - unsigned int: %s\n", TempString);
            }

            if (ObjectNr == 7)
            {   //Major firmware id
              if ((OnlineNodeInformation[IndexOfSender].ManufacturerID == 0x0001) &&
                  (OnlineNodeInformation[IndexOfSender].ProductID == 0x000C) &&
                  (Data[5] == 1))
              {
                unsigned int ParentManufacturerID = (AxumEngineDefaultObjects.Parent[0]<<8) | AxumEngineDefaultObjects.Parent[1];
                unsigned int ParentProductID = (AxumEngineDefaultObjects.Parent[2]<<8) | AxumEngineDefaultObjects.Parent[3];
                unsigned int ParentUniqueIDPerProduct = (AxumEngineDefaultObjects.Parent[4]<<8) | AxumEngineDefaultObjects.Parent[5];

                //printf("Backplane Found: %04x:%04x:%04x\n", ParentManufacturerID, ParentProductID, ParentUniqueIDPerProduct);
                if ((OnlineNodeInformation[IndexOfSender].ManufacturerID == ParentManufacturerID) &&
                    (OnlineNodeInformation[IndexOfSender].ProductID == ParentProductID) &&
                    (OnlineNodeInformation[IndexOfSender].UniqueIDPerProduct == ParentUniqueIDPerProduct))
                {
                  printf("2 - Backplane %08x\n", OnlineNodeInformation[IndexOfSender].MambaNetAddress);

                  if (BackplaneMambaNetAddress != OnlineNodeInformation[IndexOfSender].MambaNetAddress)
                  { //Initialize all routing
                    BackplaneMambaNetAddress = OnlineNodeInformation[IndexOfSender].MambaNetAddress;

                    SetBackplane_Clock();

                    for (int cntModule=0; cntModule<128; cntModule++)
                    {
                      SetAxum_ModuleSource(cntModule);
                      SetAxum_ModuleMixMinus(cntModule, 0);
                      SetAxum_ModuleInsertSource(cntModule);
                      SetAxum_BussLevels(cntModule);
                    }
                    SetAxum_BussMasterLevels();

                    for (int cntDSPCard=0; cntDSPCard<4; cntDSPCard++)
                    {
                      SetAxum_ExternSources(cntDSPCard);

                      //enable buss summing
                      if (dsp_card_available(dsp_handler, cntDSPCard))
                      {
                        unsigned char TransmitBuffer[64];
                        unsigned char cntTransmitBuffer;
                        unsigned int ObjectNumber = 1026+dsp_handler->dspcard[cntDSPCard].slot;

                        cntTransmitBuffer = 0;
                        TransmitBuffer[cntTransmitBuffer++] = (ObjectNumber>>8)&0xFF;
                        TransmitBuffer[cntTransmitBuffer++] = ObjectNumber&0xFF;
                        TransmitBuffer[cntTransmitBuffer++] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
                        TransmitBuffer[cntTransmitBuffer++] = STATE_DATATYPE;
                        TransmitBuffer[cntTransmitBuffer++] = 1;//size
                        TransmitBuffer[cntTransmitBuffer++] = 1;//enabled

                        SendMambaNetMessage(BackplaneMambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, 1, TransmitBuffer, cntTransmitBuffer);
                      }
                    }

                    for (int cntDestination=0; cntDestination<1280; cntDestination++)
                    {
                      SetAxum_DestinationSource(cntDestination);
                    }

                    for (int cntTalkback=0; cntTalkback<16; cntTalkback++)
                    {
                      SetAxum_TalkbackSource(cntTalkback);
                    }
                  }
                }
              }
              if (OnlineNodeInformation[IndexOfSender].FirmwareMajorRevision == -1)
              {
                OnlineNodeInformation[IndexOfSender].FirmwareMajorRevision = Data[5];

                db_read_template_info(&OnlineNodeInformation[IndexOfSender], 1);

                if (OnlineNodeInformation[IndexOfSender].SlotNumberObjectNr != -1)
                {
                  unsigned char TransmitBuffer[64];
                  unsigned char cntTransmitBuffer;
                  unsigned int ObjectNumber = OnlineNodeInformation[IndexOfSender].SlotNumberObjectNr;

                  cntTransmitBuffer = 0;
                  TransmitBuffer[cntTransmitBuffer++] = (ObjectNumber>>8)&0xFF;
                  TransmitBuffer[cntTransmitBuffer++] = ObjectNumber&0xFF;
                  TransmitBuffer[cntTransmitBuffer++] = MAMBANET_OBJECT_ACTION_GET_SENSOR_DATA;
                  TransmitBuffer[cntTransmitBuffer++] = NO_DATA_DATATYPE;

                  SendMambaNetMessage(OnlineNodeInformation[IndexOfSender].MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, 1, TransmitBuffer, cntTransmitBuffer);
                }

                if (OnlineNodeInformation[IndexOfSender].NumberOfCustomObjects>0)
                {
                  db_read_node_defaults(&OnlineNodeInformation[IndexOfSender], 1024, OnlineNodeInformation[IndexOfSender].NumberOfCustomObjects+1024);
                  db_read_node_config(&OnlineNodeInformation[IndexOfSender], 1024, OnlineNodeInformation[IndexOfSender].NumberOfCustomObjects+1024);
                }
              }
            }
            if ((ObjectNr>=1024) && (((signed int)ObjectNr) == OnlineNodeInformation[IndexOfSender].SlotNumberObjectNr))
            {
              for (int cntSlot=0; cntSlot<42; cntSlot++)
              {
                if (cntSlot != Data[5])
                { //other slot then current inserted
                  if (AxumData.RackOrganization[cntSlot] == FromAddress)
                  {
                    AxumData.RackOrganization[cntSlot] = 0;

                    db_delete_slot_config(cntSlot);
                  }
                }
              }
              AxumData.RackOrganization[Data[5]] = FromAddress;

              log_write("0x%08lX found at slot: %d", FromAddress, Data[5]+1);
              db_insert_slot_config(Data[5], FromAddress, 0, 0);

              //if a slot number exists, check the number of input/output channels.
              if (OnlineNodeInformation[IndexOfSender].InputChannelCountObjectNr != -1)
              {
                unsigned char TransmitBuffer[64];
                unsigned char cntTransmitBuffer;
                unsigned int ObjectNumber = OnlineNodeInformation[IndexOfSender].InputChannelCountObjectNr;

                printf("Get Input channel count @ ObjectNr: %d\n", ObjectNumber);

                cntTransmitBuffer = 0;
                TransmitBuffer[cntTransmitBuffer++] = (ObjectNumber>>8)&0xFF;
                TransmitBuffer[cntTransmitBuffer++] = ObjectNumber&0xFF;
                TransmitBuffer[cntTransmitBuffer++] = MAMBANET_OBJECT_ACTION_GET_SENSOR_DATA;
                TransmitBuffer[cntTransmitBuffer++] = NO_DATA_DATATYPE;
                //TransmitBuffer[cntTransmitBuffer++] = 0;

                SendMambaNetMessage(OnlineNodeInformation[IndexOfSender].MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, 1, TransmitBuffer, cntTransmitBuffer);
              }
              if (OnlineNodeInformation[IndexOfSender].OutputChannelCountObjectNr != -1)
              {
                unsigned char TransmitBuffer[64];
                unsigned char cntTransmitBuffer;
                unsigned int ObjectNumber = OnlineNodeInformation[IndexOfSender].OutputChannelCountObjectNr;

                printf("Get Output channel count @ ObjectNr: %d\n", ObjectNumber);

                cntTransmitBuffer = 0;
                TransmitBuffer[cntTransmitBuffer++] = (ObjectNumber>>8)&0xFF;
                TransmitBuffer[cntTransmitBuffer++] = ObjectNumber&0xFF;
                TransmitBuffer[cntTransmitBuffer++] = MAMBANET_OBJECT_ACTION_GET_SENSOR_DATA;
                TransmitBuffer[cntTransmitBuffer++] = NO_DATA_DATATYPE;
                //TransmitBuffer[cntTransmitBuffer++] = 0;

                SendMambaNetMessage(OnlineNodeInformation[IndexOfSender].MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, 1, TransmitBuffer, cntTransmitBuffer);
              }

              //Check for source if I/O card changed.
              for (unsigned int cntSource=0; cntSource<1280; cntSource++)
              {
                if (  (AxumData.SourceData[cntSource].InputData[0].MambaNetAddress == FromAddress) ||
                      (AxumData.SourceData[cntSource].InputData[1].MambaNetAddress == FromAddress))
                { //this source is changed, update modules!
                  printf("Found source: %d\n", cntSource);

                  //Set Phantom, Pad and Gain
                  unsigned int FunctionNrToSend = 0x05000000 | (cntSource<<12);
                  CheckObjectsToSent(FunctionNrToSend | SOURCE_FUNCTION_PHANTOM);
                  CheckObjectsToSent(FunctionNrToSend | SOURCE_FUNCTION_PAD);
                  CheckObjectsToSent(FunctionNrToSend | SOURCE_FUNCTION_GAIN);

                  for (int cntModule=0; cntModule<128; cntModule++)
                  {
                    if (AxumData.ModuleData[cntModule].Source == (cntSource+matrix_sources.src_offset.min.source))
                    {
                      printf("Found module: %d\n", cntModule);
                      SetAxum_ModuleSource(cntModule);
                      SetAxum_ModuleMixMinus(cntModule, 0);

                      unsigned int FunctionNrToSend = (cntModule<<12);
                      CheckObjectsToSent(FunctionNrToSend | MODULE_FUNCTION_SOURCE_PHANTOM);
                      CheckObjectsToSent(FunctionNrToSend | MODULE_FUNCTION_SOURCE_PAD);
                      CheckObjectsToSent(FunctionNrToSend | MODULE_FUNCTION_SOURCE_GAIN_LEVEL);
                    }
                  }
                  for (int cntDSPCard=0; cntDSPCard<4; cntDSPCard++)
                  {
                    for (int cntExt=0; cntExt<8; cntExt++)
                    {
                      if (AxumData.ExternSource[cntDSPCard].Ext[cntExt] == (cntSource+matrix_sources.src_offset.min.source))
                      {
                        printf("Found extern input @ %d\n", cntDSPCard);
                        SetAxum_ExternSources(cntDSPCard);
                      }
                    }
                  }
                  for (int cntTalkback=0; cntTalkback<16; cntTalkback++)
                  {
                    if (AxumData.Talkback[cntTalkback].Source == (cntSource+matrix_sources.src_offset.min.source))
                    {
                      printf("Found talkback @ %d\n", cntTalkback);
                      SetAxum_TalkbackSource(cntTalkback);
                    }
                  }
                }
              }

              //Check for destination if I/O card changed.
              for (int cntDestination=0; cntDestination<1280; cntDestination++)
              {
                if (  (AxumData.DestinationData[cntDestination].OutputData[0].MambaNetAddress == FromAddress) ||
                      (AxumData.DestinationData[cntDestination].OutputData[1].MambaNetAddress == FromAddress))
                { //this source is changed, update modules!
                  printf("Found destination: %d\n", cntDestination);
                  SetAxum_DestinationSource(cntDestination);

                  unsigned int FunctionNrToSend = 0x06000000 | (cntDestination<<12);
                  CheckObjectsToSent(FunctionNrToSend | DESTINATION_FUNCTION_LEVEL);
                }
              }
            }
            else if (OnlineNodeInformation[IndexOfSender].SlotNumberObjectNr != -1)
            {
              printf("Check for Channel Counts: %d, %d\n", OnlineNodeInformation[IndexOfSender].InputChannelCountObjectNr, OnlineNodeInformation[IndexOfSender].OutputChannelCountObjectNr);
              if (((signed int)ObjectNr) == OnlineNodeInformation[IndexOfSender].InputChannelCountObjectNr)
              {
                db_update_slot_config_input_ch_cnt(FromAddress, Data[5]);
              }
              else if (((signed int)ObjectNr) == OnlineNodeInformation[IndexOfSender].OutputChannelCountObjectNr)
              {
                db_update_slot_config_output_ch_cnt(FromAddress, Data[5]);
              }
            }
          }
          break;
          }
        }
        else if (Data[2] == MAMBANET_OBJECT_ACTION_SENSOR_DATA_CHANGED)
        {   // sensor changed
          if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction != NULL)
          {
            OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime = cntMillisecondTimer;

            int SensorReceiveFunctionNumber = OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].FunctionNr;
            int DataType = OnlineNodeInformation[IndexOfSender].ObjectInformation[ObjectNr-1024].SensorDataType;
            int DataSize = OnlineNodeInformation[IndexOfSender].ObjectInformation[ObjectNr-1024].SensorDataSize;
            float DataMinimal = OnlineNodeInformation[IndexOfSender].ObjectInformation[ObjectNr-1024].SensorDataMinimal;
            float DataMaximal = OnlineNodeInformation[IndexOfSender].ObjectInformation[ObjectNr-1024].SensorDataMaximal;

            char TempString[256] = "";
            printf("Sensor changed, ObjectNr %d -> Sensor receive function %08x", ObjectNr, SensorReceiveFunctionNumber);

            if (SensorReceiveFunctionNumber != -1)
            {
              unsigned char SensorReceiveFunctionType = (SensorReceiveFunctionNumber>>24)&0xFF;

              switch (SensorReceiveFunctionType)
              {
              case MODULE_FUNCTIONS:
              {   //Module
                unsigned int ModuleNr = (SensorReceiveFunctionNumber>>12)&0xFFF;
                unsigned int FunctionNr = SensorReceiveFunctionNumber&0xFFF;

                printf(" > ModuleNr %d ", ModuleNr);
                switch (FunctionNr)
                {
                case MODULE_FUNCTION_LABEL:
                {   //No sensor change available
                }
                break;
                case MODULE_FUNCTION_SOURCE:
                case MODULE_FUNCTION_SOURCE_A:
                case MODULE_FUNCTION_SOURCE_B:
                {   //Source
                  printf("Source\n");
                  int CurrentSource = AxumData.ModuleData[ModuleNr].Source;

                  if (Data[3] == SIGNED_INTEGER_DATATYPE)
                  {
                    long TempData = 0;
                    if (Data[4+Data[4]]&0x80)
                    {   //signed
                      TempData = (unsigned long)0xFFFFFFFF;
                    }

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    CurrentSource = AdjustModuleSource(CurrentSource, TempData);
                  }
                  else if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if (TempData)
                    {
                      switch (FunctionNr)
                      {
                        case MODULE_FUNCTION_SOURCE_A:
                        {
                          CurrentSource = AxumData.ModuleData[ModuleNr].SourceA;
                        }
                        break;
                        case MODULE_FUNCTION_SOURCE_B:
                        {
                          CurrentSource = AxumData.ModuleData[ModuleNr].SourceB;
                        }
                        break;
                        case MODULE_FUNCTION_SOURCE_C:
                        {
                          CurrentSource = AxumData.ModuleData[ModuleNr].SourceC;
                        }
                        break;
                        case MODULE_FUNCTION_SOURCE_D:
                        {
                          CurrentSource = AxumData.ModuleData[ModuleNr].SourceD;
                        }
                        break;
                      }

                      if (Axum_MixMinusSourceUsed(CurrentSource-1) != -1)
                      {
                        CurrentSource = AxumData.ModuleData[ModuleNr].Source;
                      }
                    }
                  }

                  if (FunctionNr == MODULE_FUNCTION_SOURCE)
                  {
                    SetNewSource(ModuleNr, CurrentSource, 0, 0);
                  }
                  else
                  {
                    SetNewSource(ModuleNr, CurrentSource, 0, 1);
                  }
                }
                break;
                case MODULE_FUNCTION_SOURCE_PHANTOM:
                {
                  printf("Source phantom\n");
                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if ((AxumData.ModuleData[ModuleNr].Source >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].Source <= matrix_sources.src_offset.max.source))
                    {
                      unsigned int SourceNr = AxumData.ModuleData[ModuleNr].Source-matrix_sources.src_offset.min.source;
                      if (TempData)
                      {
                        AxumData.SourceData[SourceNr].Phantom = !AxumData.SourceData[SourceNr].Phantom;
                      }
                      else
                      {
                        int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                        if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                        {
                          if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                          {
                            AxumData.SourceData[SourceNr].Phantom = !AxumData.SourceData[SourceNr].Phantom;
                          }
                        }
                      }

                      unsigned int DisplayFunctionNr = 0x05000000 | (SourceNr<<12) | SOURCE_FUNCTION_PHANTOM;

                      CheckObjectsToSent(DisplayFunctionNr);

                      for (int cntModule=0; cntModule<128; cntModule++)
                      {
                        if (AxumData.ModuleData[cntModule].Source == AxumData.ModuleData[ModuleNr].Source)
                        {
                          unsigned int DisplayFunctionNr = (cntModule<<12) | MODULE_FUNCTION_SOURCE_PHANTOM;
                          CheckObjectsToSent(DisplayFunctionNr);
                        }
                      }
                    }
                  }
                }
                break;
                case MODULE_FUNCTION_SOURCE_PAD:
                {
                  printf("Source pad\n");
                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if ((AxumData.ModuleData[ModuleNr].Source >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].Source <= matrix_sources.src_offset.max.source))
                    {
                      unsigned int SourceNr = AxumData.ModuleData[ModuleNr].Source-matrix_sources.src_offset.min.source;
                      if (TempData)
                      {
                        AxumData.SourceData[SourceNr].Pad = !AxumData.SourceData[SourceNr].Pad;
                      }
                      else
                      {
                        int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                        if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                        {
                          if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                          {
                            AxumData.SourceData[SourceNr].Pad = !AxumData.SourceData[SourceNr].Pad;
                          }
                        }
                      }

                      unsigned int DisplayFunctionNr = 0x05000000 | (SourceNr<<12) | SOURCE_FUNCTION_PAD;
                      CheckObjectsToSent(DisplayFunctionNr);

                      for (int cntModule=0; cntModule<128; cntModule++)
                      {
                        if (AxumData.ModuleData[cntModule].Source == AxumData.ModuleData[ModuleNr].Source)
                        {
                          DisplayFunctionNr = (cntModule<<12) | MODULE_FUNCTION_SOURCE_PAD;
                          CheckObjectsToSent(DisplayFunctionNr);
                        }
                      }
                    }
                  }
                }
                break;
                case MODULE_FUNCTION_SOURCE_GAIN_LEVEL:
                {
                  printf("Source gain\n");
                  if (Data[3] == SIGNED_INTEGER_DATATYPE)
                  {
                    long TempData = 0;
                    if (Data[4+Data[4]]&0x80)
                    {  //signed
                      TempData = (unsigned long)0xFFFFFFFF;
                    }

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if ((AxumData.ModuleData[ModuleNr].Source >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].Source <= matrix_sources.src_offset.max.source))
                    {
                      unsigned int SourceNr = AxumData.ModuleData[ModuleNr].Source-matrix_sources.src_offset.min.source;
                      AxumData.SourceData[SourceNr].Gain += (float)TempData/10;
                      if (AxumData.SourceData[SourceNr].Gain<20)
                      {
                        AxumData.SourceData[SourceNr].Gain = 20;
                      }
                      else if (AxumData.SourceData[SourceNr].Gain>20)
                      {
                        AxumData.SourceData[SourceNr].Gain = 75;
                      }

                      unsigned int DisplayFunctionNr = 0x05000000 | (SourceNr<<12) | SOURCE_FUNCTION_GAIN;
                      CheckObjectsToSent(DisplayFunctionNr);

                      for (int cntModule=0; cntModule<128; cntModule++)
                      {
                        if (AxumData.ModuleData[cntModule].Source == AxumData.ModuleData[ModuleNr].Source)
                        {
                          DisplayFunctionNr = (cntModule<<12) | MODULE_FUNCTION_SOURCE_GAIN_LEVEL;
                          CheckObjectsToSent(DisplayFunctionNr);
                        }
                      }
                    }
                  }
                }
                break;
                case MODULE_FUNCTION_SOURCE_GAIN_LEVEL_RESET:
                {
                  printf("Source gain level reset\n");
                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if ((AxumData.ModuleData[ModuleNr].Source >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].Source <= matrix_sources.src_offset.max.source))
                    {
                      unsigned int SourceNr = AxumData.ModuleData[ModuleNr].Source-matrix_sources.src_offset.min.source;
                      if (TempData)
                      {
                        AxumData.SourceData[SourceNr].Gain = 0;

                        unsigned int DisplayFunctionNr = 0x05000000 | (SourceNr<<12) | SOURCE_FUNCTION_GAIN;
                        CheckObjectsToSent(DisplayFunctionNr);

                        for (int cntModule=0; cntModule<128; cntModule++)
                        {
                          if (AxumData.ModuleData[cntModule].Source == AxumData.ModuleData[ModuleNr].Source)
                          {
                            DisplayFunctionNr = (cntModule<<12) | MODULE_FUNCTION_SOURCE_GAIN_LEVEL;
                            CheckObjectsToSent(DisplayFunctionNr);
                          }
                        }
                      }
                    }
                  }
                }
                break;
                case MODULE_FUNCTION_INSERT_ON_OFF:
                { //Insert on/off
                  printf("Insert on/off\n");
                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if (TempData)
                    {
                      AxumData.ModuleData[ModuleNr].Insert = !AxumData.ModuleData[ModuleNr].Insert;
                    }
                    else
                    {
                      int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                      if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                      {
                        if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                        {
                          AxumData.ModuleData[ModuleNr].Insert = !AxumData.ModuleData[ModuleNr].Insert;
                        }
                      }
                    }

                    SetAxum_ModuleProcessing(ModuleNr);

                    unsigned int FunctionNrToSent = (ModuleNr<<12) | MODULE_FUNCTION_INSERT_ON_OFF;
                    CheckObjectsToSent(FunctionNrToSent);
                  }
                }
                break;
                case MODULE_FUNCTION_PHASE:
                {   //Phase
                  printf("Phase\n");
                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if (TempData)
                    {
                      AxumData.ModuleData[ModuleNr].PhaseReverse = !AxumData.ModuleData[ModuleNr].PhaseReverse;
                    }
                    else
                    {
                      int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                      if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                      {
                        if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                        {
                          AxumData.ModuleData[ModuleNr].PhaseReverse = !AxumData.ModuleData[ModuleNr].PhaseReverse;
                        }
                      }
                    }

                    SetAxum_ModuleProcessing(ModuleNr);

                    unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

                    FunctionNrToSent = (ModuleNr<<12) | MODULE_FUNCTION_PHASE;
                    CheckObjectsToSent(FunctionNrToSent);
                  }
                }
                break;
                case MODULE_FUNCTION_GAIN_LEVEL:
                {   //Gain
                  printf("Gain\n");
                  if (Data[3] == SIGNED_INTEGER_DATATYPE)
                  {
                    long TempData = 0;
                    if (Data[4+Data[4]]&0x80)
                    {   //signed
                      TempData = (unsigned long)0xFFFFFFFF;
                    }

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    AxumData.ModuleData[ModuleNr].Gain += (float)TempData/10;
                    if (AxumData.ModuleData[ModuleNr].Gain<-20)
                    {
                      AxumData.ModuleData[ModuleNr].Gain = -20;
                    }
                    else if (AxumData.ModuleData[ModuleNr].Gain>20)
                    {
                      AxumData.ModuleData[ModuleNr].Gain = 20;
                    }

                    SetAxum_ModuleProcessing(ModuleNr);
                    CheckObjectsToSent(SensorReceiveFunctionNumber);

                    unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
                  }
                  else if (Data[3] == UNSIGNED_INTEGER_DATATYPE)
                  {
                    unsigned long TempData = 0;
                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }
                    printf("Min:%f, Max:%f, Temp:%ld\n", DataMinimal, DataMaximal, TempData);

                    AxumData.ModuleData[ModuleNr].Gain = (((float)40*(TempData-DataMinimal))/(DataMaximal-DataMinimal))-20;

                    SetAxum_ModuleProcessing(ModuleNr);
                    CheckObjectsToSent(SensorReceiveFunctionNumber);

                    unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
                  }
                }
                break;
                case MODULE_FUNCTION_GAIN_LEVEL_RESET:
                { //Gain reset
                  printf("Gain reset\n");
                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if (TempData)
                    {
                      AxumData.ModuleData[ModuleNr].Gain = 0;
                      SetAxum_ModuleProcessing(ModuleNr);

                      unsigned int DisplayFunctionNr = (ModuleNr<<12) | MODULE_FUNCTION_GAIN_LEVEL;
                      CheckObjectsToSent(DisplayFunctionNr);

                      unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
                    }
                  }
                }
                break;
                case MODULE_FUNCTION_LOW_CUT_FREQUENCY:
                { //Low cut frequency
                  printf("Low cut frequency\n");
                  if (Data[3] == SIGNED_INTEGER_DATATYPE)
                  {
                    long TempData = 0;
                    if (Data[4+Data[4]]&0x80)
                    {   //signed
                      TempData = (unsigned long)0xFFFFFFFF;
                    }

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if (TempData>=0)
                    {
                      AxumData.ModuleData[ModuleNr].Filter.Frequency *= 1+((float)TempData/100);
                    }
                    else
                    {
                      AxumData.ModuleData[ModuleNr].Filter.Frequency /= 1+((float)-TempData/100);
                    }
                    AxumData.ModuleData[ModuleNr].Filter.On = 1;

                    if (AxumData.ModuleData[ModuleNr].Filter.Frequency <= 20)
                    {
                      AxumData.ModuleData[ModuleNr].Filter.On = 0;
                      AxumData.ModuleData[ModuleNr].Filter.Frequency = 20;
                    }
                    else if (AxumData.ModuleData[ModuleNr].Filter.Frequency > 15000)
                    {
                      AxumData.ModuleData[ModuleNr].Filter.Frequency = 15000;
                    }

                    SetAxum_ModuleProcessing(ModuleNr);
                    CheckObjectsToSent(SensorReceiveFunctionNumber);

                    unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
                  }
                }
                break;
                case MODULE_FUNCTION_LOW_CUT_ON_OFF:
                { //Low cut on/off
                  printf("Low cut on/off\n");
                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if (TempData)
                    {
                      AxumData.ModuleData[ModuleNr].Filter.On = !AxumData.ModuleData[ModuleNr].Filter.On;
                    }
                    else
                    {
                      int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                      if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                      {
                        if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                        {
                          AxumData.ModuleData[ModuleNr].Filter.On = !AxumData.ModuleData[ModuleNr].Filter.On;
                        }
                      }
                    }

                    SetAxum_ModuleProcessing(ModuleNr);

                    CheckObjectsToSent(SensorReceiveFunctionNumber);

                    unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
                  }
                }
                break;
                case MODULE_FUNCTION_EQ_BAND_1_LEVEL:
                case MODULE_FUNCTION_EQ_BAND_2_LEVEL:
                case MODULE_FUNCTION_EQ_BAND_3_LEVEL:
                case MODULE_FUNCTION_EQ_BAND_4_LEVEL:
                case MODULE_FUNCTION_EQ_BAND_5_LEVEL:
                case MODULE_FUNCTION_EQ_BAND_6_LEVEL:
                { //EQ Level
                  int BandNr = (FunctionNr-MODULE_FUNCTION_EQ_BAND_1_LEVEL)/(MODULE_FUNCTION_EQ_BAND_2_LEVEL-MODULE_FUNCTION_EQ_BAND_1_LEVEL);
                  printf("EQ%d level\n", BandNr+1);
                  if (Data[3] == SIGNED_INTEGER_DATATYPE)
                  {
                    long TempData = 0;
                    if (Data[4+Data[4]]&0x80)
                    {   //signed
                      TempData = (unsigned long)0xFFFFFFFF;
                    }

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    AxumData.ModuleData[ModuleNr].EQBand[BandNr].Level += (float)TempData/10;
                    if (AxumData.ModuleData[ModuleNr].EQBand[BandNr].Level<-AxumData.ModuleData[ModuleNr].EQBand[BandNr].Range)
                    {
                      AxumData.ModuleData[ModuleNr].EQBand[BandNr].Level = -AxumData.ModuleData[ModuleNr].EQBand[BandNr].Range;
                    }
                    else if (AxumData.ModuleData[ModuleNr].EQBand[BandNr].Level>AxumData.ModuleData[ModuleNr].EQBand[BandNr].Range)
                    {
                      AxumData.ModuleData[ModuleNr].EQBand[BandNr].Level = AxumData.ModuleData[ModuleNr].EQBand[BandNr].Range;
                    }
                    SetAxum_EQ(ModuleNr, BandNr);
                    CheckObjectsToSent(SensorReceiveFunctionNumber);

                    unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
                  }
                }
                break;
                case MODULE_FUNCTION_EQ_BAND_1_FREQUENCY:
                case MODULE_FUNCTION_EQ_BAND_2_FREQUENCY:
                case MODULE_FUNCTION_EQ_BAND_3_FREQUENCY:
                case MODULE_FUNCTION_EQ_BAND_4_FREQUENCY:
                case MODULE_FUNCTION_EQ_BAND_5_FREQUENCY:
                case MODULE_FUNCTION_EQ_BAND_6_FREQUENCY:
                { //EQ Frequency
                  int BandNr = (FunctionNr-MODULE_FUNCTION_EQ_BAND_1_FREQUENCY)/(MODULE_FUNCTION_EQ_BAND_2_FREQUENCY-MODULE_FUNCTION_EQ_BAND_1_FREQUENCY);
                  printf("EQ%d frequency\n", BandNr+1);
                  if (Data[3] == SIGNED_INTEGER_DATATYPE)
                  {
                    long TempData = 0;
                    if (Data[4+Data[4]]&0x80)
                    {   //signed
                      TempData = (unsigned long)0xFFFFFFFF;
                    }

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if (TempData>=0)
                    {
                      AxumData.ModuleData[ModuleNr].EQBand[BandNr].Frequency *= 1+((float)TempData/100);
                    }
                    else
                    {
                      AxumData.ModuleData[ModuleNr].EQBand[BandNr].Frequency /= 1+((float)-TempData/100);
                    }

                    if (AxumData.ModuleData[ModuleNr].EQBand[BandNr].Frequency<20)
                    {
                      AxumData.ModuleData[ModuleNr].EQBand[BandNr].Frequency = 20;
                    }
                    else if (AxumData.ModuleData[ModuleNr].EQBand[BandNr].Frequency>15000)
                    {
                      AxumData.ModuleData[ModuleNr].EQBand[BandNr].Frequency = 15000;
                    }
                    SetAxum_EQ(ModuleNr, BandNr);
                    CheckObjectsToSent(SensorReceiveFunctionNumber);

                    unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
                  }
                }
                break;
                case MODULE_FUNCTION_EQ_BAND_1_BANDWIDTH:
                case MODULE_FUNCTION_EQ_BAND_2_BANDWIDTH:
                case MODULE_FUNCTION_EQ_BAND_3_BANDWIDTH:
                case MODULE_FUNCTION_EQ_BAND_4_BANDWIDTH:
                case MODULE_FUNCTION_EQ_BAND_5_BANDWIDTH:
                case MODULE_FUNCTION_EQ_BAND_6_BANDWIDTH:
                { //EQ Bandwidth
                  int BandNr = (FunctionNr-MODULE_FUNCTION_EQ_BAND_1_BANDWIDTH)/(MODULE_FUNCTION_EQ_BAND_2_BANDWIDTH-MODULE_FUNCTION_EQ_BAND_1_BANDWIDTH);
                  printf("EQ%d bandwidth\n", BandNr+1);
                  if (Data[3] == SIGNED_INTEGER_DATATYPE)
                  {
                    long TempData = 0;
                    if (Data[4+Data[4]]&0x80)
                    {   //signed
                      TempData = (unsigned long)0xFFFFFFFF;
                    }

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    AxumData.ModuleData[ModuleNr].EQBand[BandNr].Bandwidth += (float)TempData/10;

                    if (AxumData.ModuleData[ModuleNr].EQBand[BandNr].Bandwidth<0.1)
                    {
                      AxumData.ModuleData[ModuleNr].EQBand[BandNr].Bandwidth = 0.1;
                    }
                    else if (AxumData.ModuleData[ModuleNr].EQBand[BandNr].Bandwidth>10)
                    {
                      AxumData.ModuleData[ModuleNr].EQBand[BandNr].Bandwidth = 10;
                    }

                    SetAxum_EQ(ModuleNr, BandNr);
                    CheckObjectsToSent(SensorReceiveFunctionNumber);

                    unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
                  }
                }
                break;
                case MODULE_FUNCTION_EQ_BAND_1_LEVEL_RESET:
                case MODULE_FUNCTION_EQ_BAND_2_LEVEL_RESET:
                case MODULE_FUNCTION_EQ_BAND_3_LEVEL_RESET:
                case MODULE_FUNCTION_EQ_BAND_4_LEVEL_RESET:
                case MODULE_FUNCTION_EQ_BAND_5_LEVEL_RESET:
                case MODULE_FUNCTION_EQ_BAND_6_LEVEL_RESET:
                { //EQ Level reset
                  int BandNr = (FunctionNr-MODULE_FUNCTION_EQ_BAND_1_LEVEL_RESET)/(MODULE_FUNCTION_EQ_BAND_2_LEVEL_RESET-MODULE_FUNCTION_EQ_BAND_1_LEVEL_RESET);
                  printf("EQ%d level reset\n", BandNr+1);
                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;
                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if (TempData)
                    {
                      AxumData.ModuleData[ModuleNr].EQBand[BandNr].Level = 0;

                      SetAxum_EQ(ModuleNr, BandNr);
                      SensorReceiveFunctionNumber = (ModuleNr<<12);
                      CheckObjectsToSent(SensorReceiveFunctionNumber + (MODULE_FUNCTION_EQ_BAND_1_LEVEL+(BandNr*(MODULE_FUNCTION_EQ_BAND_2_LEVEL-MODULE_FUNCTION_EQ_BAND_1_LEVEL))));

                      unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
                    }
                  }
                }
                break;
                case MODULE_FUNCTION_EQ_BAND_1_FREQUENCY_RESET:
                case MODULE_FUNCTION_EQ_BAND_2_FREQUENCY_RESET:
                case MODULE_FUNCTION_EQ_BAND_3_FREQUENCY_RESET:
                case MODULE_FUNCTION_EQ_BAND_4_FREQUENCY_RESET:
                case MODULE_FUNCTION_EQ_BAND_5_FREQUENCY_RESET:
                case MODULE_FUNCTION_EQ_BAND_6_FREQUENCY_RESET:
                { //EQ Frequency reset
                  int BandNr = (FunctionNr-MODULE_FUNCTION_EQ_BAND_1_FREQUENCY_RESET)/(MODULE_FUNCTION_EQ_BAND_2_FREQUENCY_RESET-MODULE_FUNCTION_EQ_BAND_1_FREQUENCY_RESET);
                  printf("EQ%d frequency reset\n", BandNr+1);
                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;
                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if (TempData)
                    {
                      AxumData.ModuleData[ModuleNr].EQBand[BandNr].Frequency = AxumData.ModuleData[ModuleNr].EQBand[BandNr].DefaultFrequency;

                      SetAxum_EQ(ModuleNr, BandNr);

                      SensorReceiveFunctionNumber = (ModuleNr<<12);
                      CheckObjectsToSent(SensorReceiveFunctionNumber + (MODULE_FUNCTION_EQ_BAND_1_FREQUENCY+(BandNr*(MODULE_FUNCTION_EQ_BAND_2_FREQUENCY-MODULE_FUNCTION_EQ_BAND_1_FREQUENCY))));

                      unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
                    }
                  }
                }
                break;
                case MODULE_FUNCTION_EQ_BAND_1_BANDWIDTH_RESET:
                case MODULE_FUNCTION_EQ_BAND_2_BANDWIDTH_RESET:
                case MODULE_FUNCTION_EQ_BAND_3_BANDWIDTH_RESET:
                case MODULE_FUNCTION_EQ_BAND_4_BANDWIDTH_RESET:
                case MODULE_FUNCTION_EQ_BAND_5_BANDWIDTH_RESET:
                case MODULE_FUNCTION_EQ_BAND_6_BANDWIDTH_RESET:
                { //EQ Bandwidth reset
                  int BandNr = (FunctionNr-MODULE_FUNCTION_EQ_BAND_1_BANDWIDTH_RESET)/(MODULE_FUNCTION_EQ_BAND_2_BANDWIDTH_RESET-MODULE_FUNCTION_EQ_BAND_1_BANDWIDTH_RESET);
                  printf("EQ%d bandwidth reset\n", BandNr+1);
                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;
                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if (TempData)
                    {
                      AxumData.ModuleData[ModuleNr].EQBand[BandNr].Bandwidth = AxumData.ModuleData[ModuleNr].EQBand[BandNr].DefaultBandwidth;

                      SetAxum_EQ(ModuleNr, BandNr);
                      SensorReceiveFunctionNumber = (ModuleNr<<12);
                      CheckObjectsToSent(SensorReceiveFunctionNumber + (MODULE_FUNCTION_EQ_BAND_1_BANDWIDTH+(BandNr*(MODULE_FUNCTION_EQ_BAND_2_BANDWIDTH-MODULE_FUNCTION_EQ_BAND_1_BANDWIDTH))));

                      unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
                    }
                  }
                }
                break;
                case MODULE_FUNCTION_EQ_BAND_1_TYPE:
                case MODULE_FUNCTION_EQ_BAND_2_TYPE:
                case MODULE_FUNCTION_EQ_BAND_3_TYPE:
                case MODULE_FUNCTION_EQ_BAND_4_TYPE:
                case MODULE_FUNCTION_EQ_BAND_5_TYPE:
                case MODULE_FUNCTION_EQ_BAND_6_TYPE:
                { //EQ Type
                  int BandNr = (FunctionNr-MODULE_FUNCTION_EQ_BAND_1_TYPE)/(MODULE_FUNCTION_EQ_BAND_2_TYPE-MODULE_FUNCTION_EQ_BAND_1_TYPE);
                  printf("EQ%d type\n", BandNr+1);
                  if (Data[3] == SIGNED_INTEGER_DATATYPE)
                  {
                    long TempData = 0;
                    if (Data[4+Data[4]]&0x80)
                    {   //signed
                      TempData = (unsigned long)0xFFFFFFFF;
                    }

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    int Type = AxumData.ModuleData[ModuleNr].EQBand[BandNr].Type;
                    Type += TempData;

                    if (Type<OFF)
                    {
                      Type = OFF;
                    }
                    else if (Type>NOTCH)
                    {
                      Type = NOTCH;
                    }
                    AxumData.ModuleData[ModuleNr].EQBand[BandNr].Type = (FilterType)Type;
                    SetAxum_EQ(ModuleNr, BandNr);
                    CheckObjectsToSent(SensorReceiveFunctionNumber);

                    unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
                  }
                }
                break;
                case MODULE_FUNCTION_EQ_ON_OFF:
                { //EQ on/off
                  printf("EQ on/off\n");
                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if (TempData)
                    {
                      AxumData.ModuleData[ModuleNr].EQOn = !AxumData.ModuleData[ModuleNr].EQOn;
                    }
                    else
                    {
                      int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                      if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                      {
                        if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                        {
                          AxumData.ModuleData[ModuleNr].EQOn = !AxumData.ModuleData[ModuleNr].EQOn;
                        }
                      }
                    }

                    for (int cntBand=0; cntBand<6; cntBand++)
                    {
                      AxumData.ModuleData[ModuleNr].EQBand[cntBand].On = AxumData.ModuleData[ModuleNr].EQOn;
                      SetAxum_EQ(ModuleNr, cntBand);
                    }

                    unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

                    FunctionNrToSent = (ModuleNr<<12) | MODULE_FUNCTION_EQ_ON_OFF;
                    CheckObjectsToSent(FunctionNrToSent);
                  }
                }
                break;
//                              case MODULE_FUNCTION_EQ_TYPE_A:
//                              case MODULE_FUNCTION_EQ_TYPE_B:
//                              { //EQ Type A&B
//                              }
//                              break;
                case MODULE_FUNCTION_DYNAMICS_AMOUNT:
                { //Dynamics amount
                  printf("Dynamics\n");
                  if (Data[3] == SIGNED_INTEGER_DATATYPE)
                  {
                    long TempData = 0;
                    if (Data[4+Data[4]]&0x80)
                    {   //signed
                      TempData = (unsigned long)0xFFFFFFFF;
                    }

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    AxumData.ModuleData[ModuleNr].Dynamics += TempData;
                    if (AxumData.ModuleData[ModuleNr].Dynamics < 0)
                    {
                      AxumData.ModuleData[ModuleNr].Dynamics = 0;
                    }
                    else if (AxumData.ModuleData[ModuleNr].Dynamics > 100)
                    {
                      AxumData.ModuleData[ModuleNr].Dynamics = 100;
                    }
                    SetAxum_ModuleProcessing(ModuleNr);
                    CheckObjectsToSent(SensorReceiveFunctionNumber);

                    unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
                  }
                }
                break;
                case MODULE_FUNCTION_DYNAMICS_ON_OFF:
                { //Dynamics on/off
                  printf("Dynamics on/off\n");
                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if (TempData)
                    {
                      AxumData.ModuleData[ModuleNr].DynamicsOn = !AxumData.ModuleData[ModuleNr].DynamicsOn;
                    }
                    else
                    {
                      int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                      if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                      {
                        if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                        {
                          AxumData.ModuleData[ModuleNr].DynamicsOn = !AxumData.ModuleData[ModuleNr].DynamicsOn;
                        }
                      }
                    }

                    SetAxum_ModuleProcessing(ModuleNr);

                    unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

                    FunctionNrToSent = (ModuleNr<<12) | MODULE_FUNCTION_DYNAMICS_ON_OFF;
                    CheckObjectsToSent(FunctionNrToSent);
                  }
                }
                break;
                case MODULE_FUNCTION_MONO:
                { //Mono
                  printf("Mono\n");
                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if (TempData)
                    {
                      AxumData.ModuleData[ModuleNr].Mono = !AxumData.ModuleData[ModuleNr].Mono;
                    }
                    else
                    {
                      int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                      if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                      {
                        if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                        {
                          AxumData.ModuleData[ModuleNr].Mono = !AxumData.ModuleData[ModuleNr].Mono;
                        }
                      }
                    }

                    SetAxum_BussLevels(ModuleNr);

                    unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

                    FunctionNrToSent = (ModuleNr<<12) | MODULE_FUNCTION_MONO;
                    CheckObjectsToSent(FunctionNrToSent);
                  }
                }
                break;
                case MODULE_FUNCTION_PAN:
                { //Pan
                  printf("Pan\n");
                  if (Data[3] == SIGNED_INTEGER_DATATYPE)
                  {
                    long TempData = 0;
                    if (Data[4+Data[4]]&0x80)
                    {   //signed
                      TempData = (unsigned long)0xFFFFFFFF;
                    }

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    AxumData.ModuleData[ModuleNr].Panorama += TempData;
                    if (AxumData.ModuleData[ModuleNr].Panorama< 0)
                    {
                      AxumData.ModuleData[ModuleNr].Panorama = 0;
                    }
                    else if (AxumData.ModuleData[ModuleNr].Panorama > 1023)
                    {
                      AxumData.ModuleData[ModuleNr].Panorama = 1023;
                    }
                    SetAxum_BussLevels(ModuleNr);
                    CheckObjectsToSent(SensorReceiveFunctionNumber);

                    unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
                  }
                }
                break;
                case MODULE_FUNCTION_PAN_RESET:
                { //Pan reset
                  printf("Pan reset\n");
                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if (TempData)
                    {
                      AxumData.ModuleData[ModuleNr].Panorama = 512;
                      SetAxum_BussLevels(ModuleNr);
                      unsigned int DisplayFunctionNumber = (ModuleNr<<12);
                      CheckObjectsToSent(DisplayFunctionNumber | MODULE_FUNCTION_PAN);

                      unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
                    }
                  }
                }
                break;
                case MODULE_FUNCTION_MODULE_LEVEL:
                {   //Module level
                  printf("Module level\n");

                  float SensorDataMinimal = OnlineNodeInformation[IndexOfSender].ObjectInformation[ObjectNr-1024].SensorDataMinimal;
                  float SensorDataMaximal = OnlineNodeInformation[IndexOfSender].ObjectInformation[ObjectNr-1024].SensorDataMaximal;

                  float CurrentLevel = AxumData.ModuleData[ModuleNr].FaderLevel;

                  if (Data[3] == UNSIGNED_INTEGER_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    int Position = (TempData*1023)/(SensorDataMaximal-SensorDataMinimal);
                    float dB = Position2dB[Position];
                    dB -= AxumData.LevelReserve;

                    AxumData.ModuleData[ModuleNr].FaderLevel = dB;
                  }
                  else if (Data[3] == SIGNED_INTEGER_DATATYPE)
                  {
                    long TempData = 0;
                    if (Data[4+Data[4]]&0x80)
                    {   //signed
                      TempData = (unsigned long)0xFFFFFFFF;
                    }

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    AxumData.ModuleData[ModuleNr].FaderLevel += TempData;
                    if (AxumData.ModuleData[ModuleNr].FaderLevel < -140)
                    {
                      AxumData.ModuleData[ModuleNr].FaderLevel = -140;
                    }
                    else
                    {
                      if (AxumData.ModuleData[ModuleNr].FaderLevel > (10-AxumData.LevelReserve))
                      {
                        AxumData.ModuleData[ModuleNr].FaderLevel = (10-AxumData.LevelReserve);
                      }
                    }
                  }
                  float NewLevel = AxumData.ModuleData[ModuleNr].FaderLevel;

                  SetAxum_BussLevels(ModuleNr);
                  CheckObjectsToSent(SensorReceiveFunctionNumber);

                  unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
                  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

                  if (((CurrentLevel<=-80) && (NewLevel>-80)) ||
                      ((CurrentLevel>-80) && (NewLevel<=-80)))
                  { //fader on changed
                    DoAxum_ModuleStatusChanged(ModuleNr);

                    if ((AxumData.ModuleData[ModuleNr].Source >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].Source <= matrix_sources.src_offset.max.source))
                    {
                      unsigned int SourceNr = AxumData.ModuleData[ModuleNr].Source - matrix_sources.src_offset.min.source;
                      SensorReceiveFunctionNumber = 0x05000000 | (SourceNr<<12);
                      CheckObjectsToSent(SensorReceiveFunctionNumber+SOURCE_FUNCTION_MODULE_FADER_ON);
                      CheckObjectsToSent(SensorReceiveFunctionNumber+SOURCE_FUNCTION_MODULE_FADER_OFF);
                      CheckObjectsToSent(SensorReceiveFunctionNumber+SOURCE_FUNCTION_MODULE_FADER_ON_OFF);
                      CheckObjectsToSent(SensorReceiveFunctionNumber+SOURCE_FUNCTION_MODULE_FADER_AND_ON_ACTIVE);
                      CheckObjectsToSent(SensorReceiveFunctionNumber+SOURCE_FUNCTION_MODULE_FADER_AND_ON_INACTIVE);
                    }
                  }
                }
                break;
                case MODULE_FUNCTION_MODULE_ON:
                case MODULE_FUNCTION_MODULE_OFF:
                case MODULE_FUNCTION_MODULE_ON_OFF:
                {   //Module on
                  //Module off
                  //Module on/off
                  printf("Module on/off\n");

                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    int CurrentOn = AxumData.ModuleData[ModuleNr].On;
                    if (TempData)
                    {
                      switch (FunctionNr)
                      {
                      case MODULE_FUNCTION_MODULE_ON:
                      {
                        AxumData.ModuleData[ModuleNr].On = 1;
                      }
                      break;
                      case MODULE_FUNCTION_MODULE_OFF:
                      {
                        AxumData.ModuleData[ModuleNr].On = 0;
                      }
                      break;
                      case MODULE_FUNCTION_MODULE_ON_OFF:
                      {
                        AxumData.ModuleData[ModuleNr].On = !AxumData.ModuleData[ModuleNr].On;
                      }
                      break;
                      }
                    }
                    else
                    {
                      int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                      if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                      {
                        if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                        {
                          switch (FunctionNr)
                          {
                          case MODULE_FUNCTION_MODULE_ON_OFF:
                          {
                            AxumData.ModuleData[ModuleNr].On = !AxumData.ModuleData[ModuleNr].On;
                          }
                          break;
                          }
                        }
                      }
                    }
                    int NewOn = AxumData.ModuleData[ModuleNr].On;

                    SetAxum_BussLevels(ModuleNr);
                    SensorReceiveFunctionNumber &= 0xFFFFF000;
                    CheckObjectsToSent(SensorReceiveFunctionNumber+MODULE_FUNCTION_MODULE_ON);
                    CheckObjectsToSent(SensorReceiveFunctionNumber+MODULE_FUNCTION_MODULE_OFF);
                    CheckObjectsToSent(SensorReceiveFunctionNumber+MODULE_FUNCTION_MODULE_ON_OFF);

                    if (CurrentOn != NewOn)
                    { //module on changed
                      DoAxum_ModuleStatusChanged(ModuleNr);

                      if ((AxumData.ModuleData[ModuleNr].Source >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].Source <= matrix_sources.src_offset.max.source))
                      {
                        unsigned int SourceNr = AxumData.ModuleData[ModuleNr].Source-matrix_sources.src_offset.min.source;
                        SensorReceiveFunctionNumber = 0x05000000 | (SourceNr<<12);
                        CheckObjectsToSent(SensorReceiveFunctionNumber+SOURCE_FUNCTION_MODULE_ON);
                        CheckObjectsToSent(SensorReceiveFunctionNumber+SOURCE_FUNCTION_MODULE_OFF);
                        CheckObjectsToSent(SensorReceiveFunctionNumber+SOURCE_FUNCTION_MODULE_ON_OFF);
                        CheckObjectsToSent(SensorReceiveFunctionNumber+SOURCE_FUNCTION_MODULE_FADER_AND_ON_ACTIVE);
                        CheckObjectsToSent(SensorReceiveFunctionNumber+SOURCE_FUNCTION_MODULE_FADER_AND_ON_INACTIVE);
                      }
                    }
                  }
                }
                break;
                case MODULE_FUNCTION_BUSS_1_2_LEVEL:
                case MODULE_FUNCTION_BUSS_3_4_LEVEL:
                case MODULE_FUNCTION_BUSS_5_6_LEVEL:
                case MODULE_FUNCTION_BUSS_7_8_LEVEL:
                case MODULE_FUNCTION_BUSS_9_10_LEVEL:
                case MODULE_FUNCTION_BUSS_11_12_LEVEL:
                case MODULE_FUNCTION_BUSS_13_14_LEVEL:
                case MODULE_FUNCTION_BUSS_15_16_LEVEL:
                case MODULE_FUNCTION_BUSS_17_18_LEVEL:
                case MODULE_FUNCTION_BUSS_19_20_LEVEL:
                case MODULE_FUNCTION_BUSS_21_22_LEVEL:
                case MODULE_FUNCTION_BUSS_23_24_LEVEL:
                case MODULE_FUNCTION_BUSS_25_26_LEVEL:
                case MODULE_FUNCTION_BUSS_27_28_LEVEL:
                case MODULE_FUNCTION_BUSS_29_30_LEVEL:
                case MODULE_FUNCTION_BUSS_31_32_LEVEL:
                { //Buss level
                  int BussNr = (FunctionNr-MODULE_FUNCTION_BUSS_1_2_LEVEL)/(MODULE_FUNCTION_BUSS_3_4_LEVEL-MODULE_FUNCTION_BUSS_1_2_LEVEL);
                  printf("Buss %d/%d level\n", (BussNr*2)+1, (BussNr*2)+2);
                  if (Data[3] == UNSIGNED_INTEGER_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    int Position = ((TempData-DataMinimal)*1023)/(DataMaximal-DataMinimal);
                    AxumData.ModuleData[ModuleNr].Buss[BussNr].Level = Position2dB[Position];
                    AxumData.ModuleData[ModuleNr].Buss[BussNr].Level -= AxumData.LevelReserve;

                    SetAxum_BussLevels(ModuleNr);
                    CheckObjectsToSent(SensorReceiveFunctionNumber);

                    unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
                  }
                  else if (Data[3] == SIGNED_INTEGER_DATATYPE)
                  {
                    long TempData = 0;
                    if (Data[4+Data[4]]&0x80)
                    {   //signed
                      TempData = (unsigned long)0xFFFFFFFF;
                    }

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    AxumData.ModuleData[ModuleNr].Buss[BussNr].Level += TempData;
                    if (AxumData.ModuleData[ModuleNr].Buss[BussNr].Level < -140)
                    {
                      AxumData.ModuleData[ModuleNr].Buss[BussNr].Level = -140;
                    }
                    else
                    {
                      if (AxumData.ModuleData[ModuleNr].Buss[BussNr].Level > (10-AxumData.LevelReserve))
                      {
                        AxumData.ModuleData[ModuleNr].Buss[BussNr].Level = (10-AxumData.LevelReserve);
                      }
                    }
                    SetAxum_BussLevels(ModuleNr);
                    CheckObjectsToSent(SensorReceiveFunctionNumber);

                    unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
                  }
                }
                break;
                case MODULE_FUNCTION_BUSS_1_2_LEVEL_RESET:
                case MODULE_FUNCTION_BUSS_3_4_LEVEL_RESET:
                case MODULE_FUNCTION_BUSS_5_6_LEVEL_RESET:
                case MODULE_FUNCTION_BUSS_7_8_LEVEL_RESET:
                case MODULE_FUNCTION_BUSS_9_10_LEVEL_RESET:
                case MODULE_FUNCTION_BUSS_11_12_LEVEL_RESET:
                case MODULE_FUNCTION_BUSS_13_14_LEVEL_RESET:
                case MODULE_FUNCTION_BUSS_15_16_LEVEL_RESET:
                case MODULE_FUNCTION_BUSS_17_18_LEVEL_RESET:
                case MODULE_FUNCTION_BUSS_19_20_LEVEL_RESET:
                case MODULE_FUNCTION_BUSS_21_22_LEVEL_RESET:
                case MODULE_FUNCTION_BUSS_23_24_LEVEL_RESET:
                case MODULE_FUNCTION_BUSS_25_26_LEVEL_RESET:
                case MODULE_FUNCTION_BUSS_27_28_LEVEL_RESET:
                case MODULE_FUNCTION_BUSS_29_30_LEVEL_RESET:
                case MODULE_FUNCTION_BUSS_31_32_LEVEL_RESET:
                { //Buss level reset
                  int BussNr = (FunctionNr-MODULE_FUNCTION_BUSS_1_2_LEVEL_RESET)/(MODULE_FUNCTION_BUSS_3_4_LEVEL_RESET-MODULE_FUNCTION_BUSS_1_2_LEVEL_RESET);
                  printf("Buss %d/%d level reset\n", (BussNr*2)+1, (BussNr*2)+2);
                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if (TempData)
                    {
                      AxumData.ModuleData[ModuleNr].Buss[BussNr].Level = 0;
                      SetAxum_BussLevels(ModuleNr);

                      unsigned int DisplayFunctionNr = (ModuleNr<<12);
                      CheckObjectsToSent(DisplayFunctionNr | (MODULE_FUNCTION_BUSS_1_2_LEVEL+(BussNr*(MODULE_FUNCTION_BUSS_3_4_LEVEL-MODULE_FUNCTION_BUSS_1_2_LEVEL))));

                      unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
                    }
                  }
                }
                break;
                case MODULE_FUNCTION_BUSS_1_2_ON_OFF:
                case MODULE_FUNCTION_BUSS_3_4_ON_OFF:
                case MODULE_FUNCTION_BUSS_5_6_ON_OFF:
                case MODULE_FUNCTION_BUSS_7_8_ON_OFF:
                case MODULE_FUNCTION_BUSS_9_10_ON_OFF:
                case MODULE_FUNCTION_BUSS_11_12_ON_OFF:
                case MODULE_FUNCTION_BUSS_13_14_ON_OFF:
                case MODULE_FUNCTION_BUSS_15_16_ON_OFF:
                case MODULE_FUNCTION_BUSS_17_18_ON_OFF:
                case MODULE_FUNCTION_BUSS_19_20_ON_OFF:
                case MODULE_FUNCTION_BUSS_21_22_ON_OFF:
                case MODULE_FUNCTION_BUSS_23_24_ON_OFF:
                case MODULE_FUNCTION_BUSS_25_26_ON_OFF:
                case MODULE_FUNCTION_BUSS_27_28_ON_OFF:
                case MODULE_FUNCTION_BUSS_29_30_ON_OFF:
                case MODULE_FUNCTION_BUSS_31_32_ON_OFF:
                { //Buss on/off
                  int BussNr = (FunctionNr-MODULE_FUNCTION_BUSS_1_2_ON_OFF)/(MODULE_FUNCTION_BUSS_3_4_ON_OFF-MODULE_FUNCTION_BUSS_1_2_ON_OFF);
                  printf("Buss %d/%d on/off\n", (BussNr*2)+1, (BussNr*2)+2);
                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if (TempData)
                    {
                      AxumData.ModuleData[ModuleNr].Buss[BussNr].On = !AxumData.ModuleData[ModuleNr].Buss[BussNr].On;
                    }
                    else
                    {
                      int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                      if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                      {
                        if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                        {
                          AxumData.ModuleData[ModuleNr].Buss[BussNr].On = !AxumData.ModuleData[ModuleNr].Buss[BussNr].On;
                        }
                      }
                    }

                    SetBussOnOff(ModuleNr, BussNr, TempData);
                  }
                }
                break;
                case MODULE_FUNCTION_BUSS_1_2_PRE:
                case MODULE_FUNCTION_BUSS_3_4_PRE:
                case MODULE_FUNCTION_BUSS_5_6_PRE:
                case MODULE_FUNCTION_BUSS_7_8_PRE:
                case MODULE_FUNCTION_BUSS_9_10_PRE:
                case MODULE_FUNCTION_BUSS_11_12_PRE:
                case MODULE_FUNCTION_BUSS_13_14_PRE:
                case MODULE_FUNCTION_BUSS_15_16_PRE:
                case MODULE_FUNCTION_BUSS_17_18_PRE:
                case MODULE_FUNCTION_BUSS_19_20_PRE:
                case MODULE_FUNCTION_BUSS_21_22_PRE:
                case MODULE_FUNCTION_BUSS_23_24_PRE:
                case MODULE_FUNCTION_BUSS_25_26_PRE:
                case MODULE_FUNCTION_BUSS_27_28_PRE:
                case MODULE_FUNCTION_BUSS_29_30_PRE:
                case MODULE_FUNCTION_BUSS_31_32_PRE:
                { //Buss pre
                  int BussNr = (FunctionNr-MODULE_FUNCTION_BUSS_1_2_PRE)/(MODULE_FUNCTION_BUSS_3_4_PRE-MODULE_FUNCTION_BUSS_1_2_PRE);
                  printf("Buss %d/%d pre\n", (BussNr*2)+1, (BussNr*2)+2);
                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if (TempData)
                    {
                      AxumData.ModuleData[ModuleNr].Buss[BussNr].PreModuleLevel = !AxumData.ModuleData[ModuleNr].Buss[BussNr].PreModuleLevel;
                    }
                    else
                    {
                      int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                      if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                      {
                        if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                        {
                          AxumData.ModuleData[ModuleNr].Buss[BussNr].PreModuleLevel = !AxumData.ModuleData[ModuleNr].Buss[BussNr].PreModuleLevel;
                        }
                      }
                    }

                    SetAxum_BussLevels(ModuleNr);
                    CheckObjectsToSent(SensorReceiveFunctionNumber);

                    unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
                  }
                }
                break;
                case MODULE_FUNCTION_BUSS_1_2_BALANCE:
                case MODULE_FUNCTION_BUSS_3_4_BALANCE:
                case MODULE_FUNCTION_BUSS_5_6_BALANCE:
                case MODULE_FUNCTION_BUSS_7_8_BALANCE:
                case MODULE_FUNCTION_BUSS_9_10_BALANCE:
                case MODULE_FUNCTION_BUSS_11_12_BALANCE:
                case MODULE_FUNCTION_BUSS_13_14_BALANCE:
                case MODULE_FUNCTION_BUSS_15_16_BALANCE:
                case MODULE_FUNCTION_BUSS_17_18_BALANCE:
                case MODULE_FUNCTION_BUSS_19_20_BALANCE:
                case MODULE_FUNCTION_BUSS_21_22_BALANCE:
                case MODULE_FUNCTION_BUSS_23_24_BALANCE:
                case MODULE_FUNCTION_BUSS_25_26_BALANCE:
                case MODULE_FUNCTION_BUSS_27_28_BALANCE:
                case MODULE_FUNCTION_BUSS_29_30_BALANCE:
                case MODULE_FUNCTION_BUSS_31_32_BALANCE:
                { //Buss pre
                  int BussNr = (FunctionNr-MODULE_FUNCTION_BUSS_1_2_BALANCE)/(MODULE_FUNCTION_BUSS_3_4_BALANCE-MODULE_FUNCTION_BUSS_1_2_BALANCE);
                  printf("Buss %d/%d balance\n", (BussNr*2)+1, (BussNr*2)+2);

                  if (Data[3] == SIGNED_INTEGER_DATATYPE)
                  {
                    long TempData = 0;
                    if (Data[4+Data[4]]&0x80)
                    {   //signed
                      TempData = (unsigned long)0xFFFFFFFF;
                    }

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    AxumData.ModuleData[ModuleNr].Buss[BussNr].Balance += TempData;
                    if (AxumData.ModuleData[ModuleNr].Buss[BussNr].Balance< 0)
                    {
                      AxumData.ModuleData[ModuleNr].Buss[BussNr].Balance = 0;
                    }
                    else if (AxumData.ModuleData[ModuleNr].Buss[BussNr].Balance > 1023)
                    {
                      AxumData.ModuleData[ModuleNr].Buss[BussNr].Balance = 1023;
                    }
                    SetAxum_BussLevels(ModuleNr);
                    CheckObjectsToSent(SensorReceiveFunctionNumber);

                    unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
                  }
                }
                break;
                case MODULE_FUNCTION_BUSS_1_2_BALANCE_RESET:
                case MODULE_FUNCTION_BUSS_3_4_BALANCE_RESET:
                case MODULE_FUNCTION_BUSS_5_6_BALANCE_RESET:
                case MODULE_FUNCTION_BUSS_7_8_BALANCE_RESET:
                case MODULE_FUNCTION_BUSS_9_10_BALANCE_RESET:
                case MODULE_FUNCTION_BUSS_11_12_BALANCE_RESET:
                case MODULE_FUNCTION_BUSS_13_14_BALANCE_RESET:
                case MODULE_FUNCTION_BUSS_15_16_BALANCE_RESET:
                case MODULE_FUNCTION_BUSS_17_18_BALANCE_RESET:
                case MODULE_FUNCTION_BUSS_19_20_BALANCE_RESET:
                case MODULE_FUNCTION_BUSS_21_22_BALANCE_RESET:
                case MODULE_FUNCTION_BUSS_23_24_BALANCE_RESET:
                case MODULE_FUNCTION_BUSS_25_26_BALANCE_RESET:
                case MODULE_FUNCTION_BUSS_27_28_BALANCE_RESET:
                case MODULE_FUNCTION_BUSS_29_30_BALANCE_RESET:
                case MODULE_FUNCTION_BUSS_31_32_BALANCE_RESET:
                { //Buss pre
                  int BussNr = (FunctionNr-MODULE_FUNCTION_BUSS_1_2_BALANCE_RESET)/(MODULE_FUNCTION_BUSS_3_4_BALANCE_RESET-MODULE_FUNCTION_BUSS_1_2_BALANCE_RESET);
                  printf("Buss %d/%d balance reset\n", (BussNr*2)+1, (BussNr*2)+2);

                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if (TempData)
                    {
                      AxumData.ModuleData[ModuleNr].Buss[BussNr].Balance = 512;
                      SetAxum_BussLevels(ModuleNr);
                      unsigned int DisplayFunctionNumber = (ModuleNr<<12);
                      CheckObjectsToSent(DisplayFunctionNumber | (MODULE_FUNCTION_BUSS_1_2_BALANCE+(BussNr*(MODULE_FUNCTION_BUSS_3_4_BALANCE-MODULE_FUNCTION_BUSS_1_2_BALANCE))));

                      unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
                    }
                  }
                }
                break;
                case MODULE_FUNCTION_SOURCE_START:
                case MODULE_FUNCTION_SOURCE_STOP:
                case MODULE_FUNCTION_SOURCE_START_STOP:
                {   //Start
                  printf("Source start/stop\n");
                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if ((AxumData.ModuleData[ModuleNr].Source >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].Source <= matrix_sources.src_offset.max.source))
                    {
                      unsigned int SourceNr = AxumData.ModuleData[ModuleNr].Source-matrix_sources.src_offset.min.source;
                      char UpdateObjects = 0;

                      if (TempData)
                      {
                        switch (FunctionNr)
                        {
                        case MODULE_FUNCTION_SOURCE_START:
                        {
                          AxumData.SourceData[SourceNr].Start = 1;
                        }
                        break;
                        case MODULE_FUNCTION_SOURCE_STOP:
                        {
                          AxumData.SourceData[SourceNr].Start = 0;
                        }
                        break;
                        case MODULE_FUNCTION_SOURCE_START_STOP:
                        {
                          AxumData.SourceData[SourceNr].Start = !AxumData.SourceData[SourceNr].Start;
                        }
                        break;
                        }
                        UpdateObjects = 1;
                      }
                      else
                      {
                        int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                        if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                        {
                          if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                          {
                            switch (FunctionNr)
                            {
                            case MODULE_FUNCTION_SOURCE_START_STOP:
                            {
                              AxumData.SourceData[SourceNr].Start = !AxumData.SourceData[SourceNr].Start;
                              UpdateObjects = 1;
                            }
                            break;
                            }
                          }
                        }
                      }


                      if (UpdateObjects)
                      { //Only if pushed or changed
                        unsigned int DisplayFunctionNr = 0x05000000 | (SourceNr<<12);
                        CheckObjectsToSent(DisplayFunctionNr | SOURCE_FUNCTION_START);
                        CheckObjectsToSent(DisplayFunctionNr | SOURCE_FUNCTION_STOP);
                        CheckObjectsToSent(DisplayFunctionNr | SOURCE_FUNCTION_START_STOP);

                        for (int cntModule=0; cntModule<128; cntModule++)
                        {
                          if (AxumData.ModuleData[cntModule].Source == (SourceNr+matrix_sources.src_offset.min.source))
                          {
                            DisplayFunctionNr = (cntModule<<12);
                            CheckObjectsToSent(DisplayFunctionNr | MODULE_FUNCTION_SOURCE_START);
                            CheckObjectsToSent(DisplayFunctionNr | MODULE_FUNCTION_SOURCE_STOP);
                            CheckObjectsToSent(DisplayFunctionNr | MODULE_FUNCTION_SOURCE_START_STOP);
                          }
                        }
                      }
                    }
                  }
                }
                break;
                case MODULE_FUNCTION_COUGH_ON_OFF:
                { //Cough on/off
                  printf("Cough on/off\n");
                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    AxumData.ModuleData[ModuleNr].Cough = TempData;
                    SetAxum_BussLevels(ModuleNr);

                    CheckObjectsToSent(SensorReceiveFunctionNumber);
                    if ((AxumData.ModuleData[ModuleNr].Source >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].Source <= matrix_sources.src_offset.max.source))
                    {
                      unsigned int SourceNr = AxumData.ModuleData[ModuleNr].Source-matrix_sources.src_offset.min.source;
                      SensorReceiveFunctionNumber = 0x05000000 | (SourceNr<<12);
                      CheckObjectsToSent(SensorReceiveFunctionNumber+SOURCE_FUNCTION_MODULE_COUGH_ON_OFF);
                    }
                  }
                }
                break;
                case MODULE_FUNCTION_SOURCE_ALERT:
                { //Alert
                  printf("Source alert\n");
                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if ((AxumData.ModuleData[ModuleNr].Source >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].Source <= matrix_sources.src_offset.max.source))
                    {
                      unsigned int SourceNr = AxumData.ModuleData[ModuleNr].Source-matrix_sources.src_offset.min.source;

                      if (TempData)
                      {
                        AxumData.SourceData[SourceNr].Alert = !AxumData.SourceData[SourceNr].Alert;
                      }
                      else
                      {
                        int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                        if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                        {
                          if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                          {
                            AxumData.SourceData[SourceNr].Alert = !AxumData.SourceData[SourceNr].Alert;
                          }
                        }
                      }

                      unsigned int DisplayFunctionNr = 0x05000000 | (SourceNr<<12) | SOURCE_FUNCTION_ALERT;
                      CheckObjectsToSent(DisplayFunctionNr);

                      for (int cntModule=0; cntModule<128; cntModule++)
                      {
                        if (AxumData.ModuleData[cntModule].Source == (SourceNr+matrix_sources.src_offset.min.source))
                        {
                          DisplayFunctionNr = (cntModule<<12) | MODULE_FUNCTION_SOURCE_ALERT;
                          CheckObjectsToSent(DisplayFunctionNr);
                        }
                      }
                    }
                  }
                }
                break;
                case MODULE_FUNCTION_CONTROL_1:
                case MODULE_FUNCTION_CONTROL_2:
                case MODULE_FUNCTION_CONTROL_3:
                case MODULE_FUNCTION_CONTROL_4:
                {   //Control 1-4
                  ModeControllerSensorChange(SensorReceiveFunctionNumber, Data, DataType, DataSize, DataMinimal, DataMaximal);
                }
                break;
                case MODULE_FUNCTION_CONTROL_1_LABEL:
                case MODULE_FUNCTION_CONTROL_2_LABEL:
                case MODULE_FUNCTION_CONTROL_3_LABEL:
                case MODULE_FUNCTION_CONTROL_4_LABEL:
                {   //Control 1 label, no receive
                }
                break;
                case MODULE_FUNCTION_CONTROL_1_RESET:
                case MODULE_FUNCTION_CONTROL_2_RESET:
                case MODULE_FUNCTION_CONTROL_3_RESET:
                case MODULE_FUNCTION_CONTROL_4_RESET:
                {   //Control 1 reset
                  ModeControllerResetSensorChange(SensorReceiveFunctionNumber, Data, DataType, DataSize, DataMinimal, DataMaximal);
                }
                break;
                }
              }
              break;
              case BUSS_FUNCTIONS:
              {   //Busses
                unsigned int BussNr = (SensorReceiveFunctionNumber>>12)&0xFFF;
                unsigned int FunctionNr = SensorReceiveFunctionNumber&0xFFF;

                printf(" > BussNr %d ", BussNr);
                switch (FunctionNr)
                {
                case BUSS_FUNCTION_MASTER_LEVEL:
                {
                  if (Data[3] == UNSIGNED_INTEGER_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    int Position = ((TempData-DataMinimal)*1023)/(DataMaximal-DataMinimal);
                    AxumData.BussMasterData[BussNr].Level = Position2dB[Position];
                    AxumData.BussMasterData[BussNr].Level -= 10;//AxumData.LevelReserve;
                  }
                  else if (Data[3] == SIGNED_INTEGER_DATATYPE)
                  {
                    long TempData = 0;
                    if (Data[4+Data[4]]&0x80)
                    {   //signed
                      TempData = (unsigned long)0xFFFFFFFF;
                    }

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    AxumData.BussMasterData[BussNr].Level += TempData;
                    if (AxumData.BussMasterData[BussNr].Level < -140)
                    {
                      AxumData.BussMasterData[BussNr].Level = -140;
                    }
                    else
                    {
                      if (AxumData.BussMasterData[BussNr].Level > 0)
                      {
                        AxumData.BussMasterData[BussNr].Level = 0;
                      }
                    }
                  }

                  SetAxum_BussMasterLevels();
                  CheckObjectsToSent(SensorReceiveFunctionNumber);

                  unsigned int FunctionNrToSent = 0x04000000;
                  CheckObjectsToSent(FunctionNrToSent | GLOBAL_FUNCTION_MASTER_CONTROL_1);
                  CheckObjectsToSent(FunctionNrToSent | GLOBAL_FUNCTION_MASTER_CONTROL_2);
                  CheckObjectsToSent(FunctionNrToSent | GLOBAL_FUNCTION_MASTER_CONTROL_3);
                  CheckObjectsToSent(FunctionNrToSent | GLOBAL_FUNCTION_MASTER_CONTROL_4);
                }
                break;
                case BUSS_FUNCTION_MASTER_LEVEL_RESET:
                {
                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if (TempData)
                    {
                      AxumData.BussMasterData[BussNr].Level = 0;

                      SetAxum_BussMasterLevels();

                      SensorReceiveFunctionNumber = 0x01000000 | (BussNr<<12);
                      CheckObjectsToSent(SensorReceiveFunctionNumber | BUSS_FUNCTION_MASTER_LEVEL);

                      unsigned int FunctionNrToSent = 0x04000000;
                      CheckObjectsToSent(FunctionNrToSent | GLOBAL_FUNCTION_MASTER_CONTROL_1);
                      CheckObjectsToSent(FunctionNrToSent | GLOBAL_FUNCTION_MASTER_CONTROL_2);
                      CheckObjectsToSent(FunctionNrToSent | GLOBAL_FUNCTION_MASTER_CONTROL_3);
                      CheckObjectsToSent(FunctionNrToSent | GLOBAL_FUNCTION_MASTER_CONTROL_4);
                    }
                  }
                }
                break;
                case BUSS_FUNCTION_MASTER_ON_OFF:
                {
                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if (TempData)
                    {
                      AxumData.BussMasterData[BussNr].On = !AxumData.BussMasterData[BussNr].On;
                    }
                    else
                    {
                      int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                      if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                      {
                        if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                        {
                          AxumData.BussMasterData[BussNr].On = !AxumData.BussMasterData[BussNr].On;
                        }
                      }
                    }

                    SetAxum_BussMasterLevels();

                    CheckObjectsToSent(SensorReceiveFunctionNumber);
                  }
                }
                break;
                case BUSS_FUNCTION_MASTER_PRE:
                {
                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if (TempData)
                    {
                      AxumData.BussMasterData[BussNr].PreModuleLevel = !AxumData.BussMasterData[BussNr].PreModuleLevel;
                    }
                    else
                    {
                      int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                      if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                      {
                        if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                        {
                          AxumData.BussMasterData[BussNr].PreModuleLevel = !AxumData.BussMasterData[BussNr].PreModuleLevel;
                        }
                      }
                    }

                    for (int cntModule=0; cntModule<128; cntModule++)
                    {
                      SetAxum_BussLevels(cntModule);
                    }

                    CheckObjectsToSent(SensorReceiveFunctionNumber);

                    if (  (AxumData.BussMasterData[BussNr].PreModuleOn) &&
                          (AxumData.BussMasterData[BussNr].PreModuleLevel))
                    {
                      printf("Have to check monitor routing and muting\n");
                      DoAxum_ModulePreStatusChanged(BussNr);
                    }
                  }
                }
                break;
                case BUSS_FUNCTION_LABEL:
                {   //No sensor change available
                }
                break;
                default:
                { //not implemented function
                  printf("FunctionNr %d\n", FunctionNr);
                }
                break;
                }
              }
              break;
              case MONITOR_BUSS_FUNCTIONS:
              {   //Monitor Busses
                unsigned int MonitorBussNr = (SensorReceiveFunctionNumber>>12)&0xFFF;
                int FunctionNr = SensorReceiveFunctionNumber&0xFFF;

                printf(" > MonitorBussNr %d ", MonitorBussNr);
                if ((FunctionNr>=MONITOR_BUSS_FUNCTION_BUSS_1_2_ON_OFF) && (FunctionNr<MONITOR_BUSS_FUNCTION_MUTE))
                {
                  printf("Monitor source %d on/off\n", FunctionNr);
                  if (Data[3] == STATE_DATATYPE)
                  {
                    bool *MonitorSwitchState;
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    switch (FunctionNr)
                    {
                    case MONITOR_BUSS_FUNCTION_BUSS_1_2_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_BUSS_3_4_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_BUSS_5_6_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_BUSS_7_8_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_BUSS_9_10_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_BUSS_11_12_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_BUSS_13_14_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_BUSS_15_16_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_BUSS_17_18_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_BUSS_19_20_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_BUSS_21_22_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_BUSS_23_24_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_BUSS_25_26_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_BUSS_27_28_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_BUSS_29_30_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_BUSS_31_32_ON_OFF:
                    {
                      int BussNr = FunctionNr - MONITOR_BUSS_FUNCTION_BUSS_1_2_ON_OFF;
                      MonitorSwitchState = &AxumData.Monitor[MonitorBussNr].Buss[BussNr];
                    }
                    break;
                    case MONITOR_BUSS_FUNCTION_EXT_1_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_EXT_2_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_EXT_3_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_EXT_4_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_EXT_5_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_EXT_6_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_EXT_7_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_EXT_8_ON_OFF:
                    {
                      int ExtNr = FunctionNr - MONITOR_BUSS_FUNCTION_EXT_1_ON_OFF;
                      MonitorSwitchState = &AxumData.Monitor[MonitorBussNr].Ext[ExtNr];
                    }
                    break;
                    }

                    if (MonitorSwitchState != NULL)
                    {
                      if (TempData)
                      {
                        *MonitorSwitchState = !*MonitorSwitchState;
                      }
                      else
                      {
                        int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                        if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                        {
                          if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                          {
                            *MonitorSwitchState = !*MonitorSwitchState;
                          }
                        }
                      }
                    }

                    if (AxumData.Monitor[MonitorBussNr].Interlock)
                    {
                      switch (FunctionNr)
                      {
                      case MONITOR_BUSS_FUNCTION_BUSS_1_2_ON_OFF:
                      case MONITOR_BUSS_FUNCTION_BUSS_3_4_ON_OFF:
                      case MONITOR_BUSS_FUNCTION_BUSS_5_6_ON_OFF:
                      case MONITOR_BUSS_FUNCTION_BUSS_7_8_ON_OFF:
                      case MONITOR_BUSS_FUNCTION_BUSS_9_10_ON_OFF:
                      case MONITOR_BUSS_FUNCTION_BUSS_11_12_ON_OFF:
                      case MONITOR_BUSS_FUNCTION_BUSS_13_14_ON_OFF:
                      case MONITOR_BUSS_FUNCTION_BUSS_15_16_ON_OFF:
                      case MONITOR_BUSS_FUNCTION_BUSS_17_18_ON_OFF:
                      case MONITOR_BUSS_FUNCTION_BUSS_19_20_ON_OFF:
                      case MONITOR_BUSS_FUNCTION_BUSS_21_22_ON_OFF:
                      case MONITOR_BUSS_FUNCTION_BUSS_23_24_ON_OFF:
                      case MONITOR_BUSS_FUNCTION_BUSS_25_26_ON_OFF:
                      case MONITOR_BUSS_FUNCTION_BUSS_27_28_ON_OFF:
                      case MONITOR_BUSS_FUNCTION_BUSS_29_30_ON_OFF:
                      case MONITOR_BUSS_FUNCTION_BUSS_31_32_ON_OFF:
                      {
                        int BussNr = FunctionNr - MONITOR_BUSS_FUNCTION_BUSS_1_2_ON_OFF;

                        for (int cntBuss=0; cntBuss<16; cntBuss++)
                        {
                          if (cntBuss != BussNr)
                          {
                            if (AxumData.Monitor[MonitorBussNr].Buss[cntBuss])
                            {
                              AxumData.Monitor[MonitorBussNr].Buss[cntBuss] = 0;

                              unsigned int FunctionNrToSent = 0x02000000 | (MonitorBussNr<<12);
                              CheckObjectsToSent(FunctionNrToSent | (MONITOR_BUSS_FUNCTION_BUSS_1_2_ON_OFF+cntBuss));
                            }
                          }
                        }
                        for (int cntExt=0; cntExt<8; cntExt++)
                        {
                          if (AxumData.Monitor[MonitorBussNr].Ext[cntExt])
                          {
                            AxumData.Monitor[MonitorBussNr].Ext[cntExt] = 0;

                            unsigned int FunctionNrToSent = 0x02000000 | (MonitorBussNr<<12);
                            CheckObjectsToSent(FunctionNrToSent | (MONITOR_BUSS_FUNCTION_EXT_1_ON_OFF+cntExt));
                          }
                        }
                      }
                      break;
                      case MONITOR_BUSS_FUNCTION_EXT_1_ON_OFF:
                      case MONITOR_BUSS_FUNCTION_EXT_2_ON_OFF:
                      case MONITOR_BUSS_FUNCTION_EXT_3_ON_OFF:
                      case MONITOR_BUSS_FUNCTION_EXT_4_ON_OFF:
                      case MONITOR_BUSS_FUNCTION_EXT_5_ON_OFF:
                      case MONITOR_BUSS_FUNCTION_EXT_6_ON_OFF:
                      case MONITOR_BUSS_FUNCTION_EXT_7_ON_OFF:
                      case MONITOR_BUSS_FUNCTION_EXT_8_ON_OFF:
                      {
                        int ExtNr = FunctionNr - MONITOR_BUSS_FUNCTION_EXT_1_ON_OFF;

                        for (int cntBuss=0; cntBuss<16; cntBuss++)
                        {
                          if (AxumData.Monitor[MonitorBussNr].Buss[cntBuss])
                          {
                            AxumData.Monitor[MonitorBussNr].Buss[cntBuss] = 0;

                            unsigned int FunctionNrToSent = 0x02000000 | (MonitorBussNr<<12);
                            CheckObjectsToSent(FunctionNrToSent | (MONITOR_BUSS_FUNCTION_BUSS_1_2_ON_OFF+cntBuss));
                          }
                        }
                        for (int cntExt=0; cntExt<8; cntExt++)
                        {
                          if (ExtNr != cntExt)
                          {
                            if (AxumData.Monitor[MonitorBussNr].Ext[cntExt])
                            {
                              AxumData.Monitor[MonitorBussNr].Ext[cntExt] = 0;

                              unsigned int FunctionNrToSent = 0x02000000 | (MonitorBussNr<<12);
                              CheckObjectsToSent(FunctionNrToSent | (MONITOR_BUSS_FUNCTION_EXT_1_ON_OFF+cntExt));
                            }
                          }
                        }
                      }
                      break;
                      case MONITOR_BUSS_FUNCTION_LABEL:
                      {   //No sensor change available
                      }
                      break;
                      }
                    }

                    unsigned int MonitorBussActive = 0;
                    int cntBuss;
                    for (cntBuss=0; cntBuss<16; cntBuss++)
                    {
                      if (AxumData.Monitor[MonitorBussNr].Buss[cntBuss])
                      {
                        MonitorBussActive = 1;
                      }
                    }
                    int cntExt;
                    for (cntExt=0; cntExt<8; cntExt++)
                    {
                      if (AxumData.Monitor[MonitorBussNr].Ext[cntExt])
                      {
                        MonitorBussActive = 1;
                      }
                    }

                    if (!MonitorBussActive)
                    {
                      int DefaultSelection = AxumData.Monitor[MonitorBussNr].DefaultSelection;
                      if (DefaultSelection<16)
                      {
                        int BussNr = DefaultSelection;
                        AxumData.Monitor[MonitorBussNr].Buss[BussNr] = 1;

                        unsigned int FunctionNrToSent = 0x02000000 | (MonitorBussNr<<12);
                        CheckObjectsToSent(FunctionNrToSent | (MONITOR_BUSS_FUNCTION_BUSS_1_2_ON_OFF+BussNr));
                      }
                      else if (DefaultSelection<24)
                      {
                        int ExtNr = DefaultSelection-16;
                        AxumData.Monitor[MonitorBussNr].Ext[ExtNr] = 1;

                        unsigned int FunctionNrToSent = 0x02000000 | (MonitorBussNr<<12);
                        CheckObjectsToSent(FunctionNrToSent | (MONITOR_BUSS_FUNCTION_EXT_1_ON_OFF+ExtNr));
                      }
                    }

                    switch (FunctionNr)
                    {
                    case MONITOR_BUSS_FUNCTION_BUSS_1_2_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_BUSS_3_4_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_BUSS_5_6_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_BUSS_7_8_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_BUSS_9_10_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_BUSS_11_12_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_BUSS_13_14_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_BUSS_15_16_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_BUSS_17_18_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_BUSS_19_20_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_BUSS_21_22_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_BUSS_23_24_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_BUSS_25_26_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_BUSS_27_28_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_BUSS_29_30_ON_OFF:
                    case MONITOR_BUSS_FUNCTION_BUSS_31_32_ON_OFF:
                    {
                      int BussNr = FunctionNr - MONITOR_BUSS_FUNCTION_BUSS_1_2_ON_OFF;

                      if (  (AxumData.BussMasterData[BussNr].PreModuleOn) &&
                            (AxumData.BussMasterData[BussNr].PreModuleLevel))
                      {
                        printf("Have to check monitor routing and muting\n");
                        DoAxum_ModulePreStatusChanged(BussNr);
                      }
                    }
                    break;
                    }

                    SetAxum_MonitorBuss(MonitorBussNr);
                    CheckObjectsToSent(SensorReceiveFunctionNumber);
                  }
                }
                else
                {
                  switch (FunctionNr)
                  {
                  case MONITOR_BUSS_FUNCTION_MUTE:
                  {
                    printf("Mute\n");
                    if (Data[3] == STATE_DATATYPE)
                    {
                      unsigned long TempData = 0;

                      for (int cntByte=0; cntByte<Data[4]; cntByte++)
                      {
                        TempData <<= 8;
                        TempData |= Data[5+cntByte];
                      }

                      if (TempData)
                      {
                        AxumData.Monitor[MonitorBussNr].Mute = !AxumData.Monitor[MonitorBussNr].Mute;
                      }
                      else
                      {
                        int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                        if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                        {
                          if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                          {
                            AxumData.Monitor[MonitorBussNr].Mute = !AxumData.Monitor[MonitorBussNr].Mute;
                          }
                        }
                      }

                      CheckObjectsToSent(SensorReceiveFunctionNumber);

                      for (int cntDestination=0; cntDestination<1280; cntDestination++)
                      {
                        if (AxumData.DestinationData[cntDestination].Source == (matrix_sources.src_offset.min.monitor_buss+MonitorBussNr))
                        {
                          unsigned int DisplayFunctionNr = 0x06000000 | (cntDestination<<12);
                          CheckObjectsToSent(DisplayFunctionNr | DESTINATION_FUNCTION_MUTE_AND_MONITOR_MUTE);
                        }
                      }
                    }
                  }
                  break;
                  case MONITOR_BUSS_FUNCTION_DIM:
                  {
                    printf("Dim\n");
                    if (Data[3] == STATE_DATATYPE)
                    {
                      unsigned long TempData = 0;

                      for (int cntByte=0; cntByte<Data[4]; cntByte++)
                      {
                        TempData <<= 8;
                        TempData |= Data[5+cntByte];
                      }

                      if (TempData)
                      {
                        AxumData.Monitor[MonitorBussNr].Dim = !AxumData.Monitor[MonitorBussNr].Dim;
                      }
                      else
                      {
                        int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                        if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                        {
                          if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                          {
                            AxumData.Monitor[MonitorBussNr].Dim = !AxumData.Monitor[MonitorBussNr].Dim;
                          }
                        }
                      }

                      CheckObjectsToSent(SensorReceiveFunctionNumber);

                      for (int cntDestination=0; cntDestination<1280; cntDestination++)
                      {
                        if (AxumData.DestinationData[cntDestination].Source == (matrix_sources.src_offset.min.monitor_buss+MonitorBussNr))
                        {
                          unsigned int DisplayFunctionNr = 0x06000000 | (cntDestination<<12);
                          CheckObjectsToSent(DisplayFunctionNr | DESTINATION_FUNCTION_DIM_AND_MONITOR_DIM);
                        }
                      }
                    }
                  }
                  break;
                  case MONITOR_BUSS_FUNCTION_PHONES_LEVEL:
                  {
                    printf("Phones level\n");
                    if (Data[3] == UNSIGNED_INTEGER_DATATYPE)
                    {
                      unsigned long TempData = 0;

                      for (int cntByte=0; cntByte<Data[4]; cntByte++)
                      {
                        TempData <<= 8;
                        TempData |= Data[5+cntByte];
                      }

                      float SensorDataMinimal = OnlineNodeInformation[IndexOfSender].ObjectInformation[ObjectNr-1024].SensorDataMinimal;
                      float SensorDataMaximal = OnlineNodeInformation[IndexOfSender].ObjectInformation[ObjectNr-1024].SensorDataMaximal;

                      int Position = (TempData*1023)/(SensorDataMaximal-SensorDataMinimal);
                      float dB = Position2dB[Position];
                      dB +=10; //20dB reserve

                      AxumData.Monitor[MonitorBussNr].PhonesLevel = dB;

                      CheckObjectsToSent(SensorReceiveFunctionNumber);

                      for (int cntDestination=0; cntDestination<1280; cntDestination++)
                      {
                        if (AxumData.DestinationData[cntDestination].Source == (matrix_sources.src_offset.min.monitor_buss+MonitorBussNr))
                        {
                          unsigned int DisplayFunctionNr = 0x06000000 | (cntDestination<<12);
                          CheckObjectsToSent(DisplayFunctionNr | DESTINATION_FUNCTION_MONITOR_PHONES_LEVEL);
                        }
                      }
                    }
                    else if (Data[3] == SIGNED_INTEGER_DATATYPE)
                    {
                      long TempData = 0;
                      if (Data[4+Data[4]]&0x80)
                      {   //signed
                        TempData = (unsigned long)0xFFFFFFFF;
                      }

                      for (int cntByte=0; cntByte<Data[4]; cntByte++)
                      {
                        TempData <<= 8;
                        TempData |= Data[5+cntByte];
                      }

                      AxumData.Monitor[MonitorBussNr].PhonesLevel += (float)TempData/10;
                      if (AxumData.Monitor[MonitorBussNr].PhonesLevel<-140)
                      {
                        AxumData.Monitor[MonitorBussNr].PhonesLevel = -140;
                      }
                      else
                      {
                        if (AxumData.Monitor[MonitorBussNr].PhonesLevel > 20)
                        {
                          AxumData.Monitor[MonitorBussNr].PhonesLevel = 20;
                        }
                      }
                      CheckObjectsToSent(SensorReceiveFunctionNumber);

                      for (int cntDestination=0; cntDestination<1280; cntDestination++)
                      {
                        if (AxumData.DestinationData[cntDestination].Source == (matrix_sources.src_offset.min.monitor_buss+MonitorBussNr))
                        {
                          unsigned int DisplayFunctionNr = 0x06000000 | (cntDestination<<12);
                          CheckObjectsToSent(DisplayFunctionNr | DESTINATION_FUNCTION_MONITOR_PHONES_LEVEL);
                        }
                      }
                    }
                  }
                  break;
                  case MONITOR_BUSS_FUNCTION_MONO:
                  {
                    printf("Mono\n");
                    if (Data[3] == STATE_DATATYPE)
                    {
                      unsigned long TempData = 0;

                      for (int cntByte=0; cntByte<Data[4]; cntByte++)
                      {
                        TempData <<= 8;
                        TempData |= Data[5+cntByte];
                      }

                      if (TempData)
                      {
                        AxumData.Monitor[MonitorBussNr].Mono = !AxumData.Monitor[MonitorBussNr].Mono;
                      }
                      else
                      {
                        int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                        if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                        {
                          if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                          {
                            AxumData.Monitor[MonitorBussNr].Mono = !AxumData.Monitor[MonitorBussNr].Mono;
                          }
                        }
                      }

                      CheckObjectsToSent(SensorReceiveFunctionNumber);

                      for (int cntDestination=0; cntDestination<1280; cntDestination++)
                      {
                        if (AxumData.DestinationData[cntDestination].Source == (matrix_sources.src_offset.min.monitor_buss+MonitorBussNr))
                        {
                          unsigned int DisplayFunctionNr = 0x06000000 | (cntDestination<<12);
                          CheckObjectsToSent(DisplayFunctionNr | DESTINATION_FUNCTION_MONO_AND_MONITOR_MONO);
                        }
                      }
                    }
                  }
                  break;
                  case MONITOR_BUSS_FUNCTION_PHASE:
                  {
                    printf("Phase\n");
                    if (Data[3] == STATE_DATATYPE)
                    {
                      unsigned long TempData = 0;

                      for (int cntByte=0; cntByte<Data[4]; cntByte++)
                      {
                        TempData <<= 8;
                        TempData |= Data[5+cntByte];
                      }

                      if (TempData)
                      {
                        AxumData.Monitor[MonitorBussNr].Phase = !AxumData.Monitor[MonitorBussNr].Phase;
                      }
                      else
                      {
                        int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                        if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                        {
                          if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                          {
                            AxumData.Monitor[MonitorBussNr].Phase = !AxumData.Monitor[MonitorBussNr].Phase;
                          }
                        }
                      }
                      CheckObjectsToSent(SensorReceiveFunctionNumber);

                      for (int cntDestination=0; cntDestination<1280; cntDestination++)
                      {
                        if (AxumData.DestinationData[cntDestination].Source == (matrix_sources.src_offset.min.monitor_buss+MonitorBussNr))
                        {
                          unsigned int DisplayFunctionNr = 0x06000000 | (cntDestination<<12);
                          CheckObjectsToSent(DisplayFunctionNr | DESTINATION_FUNCTION_PHASE_AND_MONITOR_PHASE);
                        }
                      }
                    }
                  }
                  break;
                  case MONITOR_BUSS_FUNCTION_SPEAKER_LEVEL:
                  {
                    printf("Speaker level\n");
                    if (Data[3] == UNSIGNED_INTEGER_DATATYPE)
                    {
                      unsigned long TempData = 0;

                      for (int cntByte=0; cntByte<Data[4]; cntByte++)
                      {
                        TempData <<= 8;
                        TempData |= Data[5+cntByte];
                      }

                      float SensorDataMinimal = OnlineNodeInformation[IndexOfSender].ObjectInformation[ObjectNr-1024].SensorDataMinimal;
                      float SensorDataMaximal = OnlineNodeInformation[IndexOfSender].ObjectInformation[ObjectNr-1024].SensorDataMaximal;

                      int Position = (TempData*1023)/(SensorDataMaximal-SensorDataMinimal);
                      float dB = Position2dB[Position];
                      dB += 10;

                      AxumData.Monitor[MonitorBussNr].SpeakerLevel = dB;
                      CheckObjectsToSent(SensorReceiveFunctionNumber);

                      for (int cntDestination=0; cntDestination<1280; cntDestination++)
                      {
                        if (AxumData.DestinationData[cntDestination].Source == (matrix_sources.src_offset.min.monitor_buss+MonitorBussNr))
                        {
                          unsigned int DisplayFunctionNr = 0x06000000 | (cntDestination<<12);
                          CheckObjectsToSent(DisplayFunctionNr | DESTINATION_FUNCTION_MONITOR_SPEAKER_LEVEL);
                        }
                      }
                    }
                    else if (Data[3] == SIGNED_INTEGER_DATATYPE)
                    {
                      long TempData = 0;
                      if (Data[4+Data[4]]&0x80)
                      {   //signed
                        TempData = (unsigned long)0xFFFFFFFF;
                      }

                      for (int cntByte=0; cntByte<Data[4]; cntByte++)
                      {
                        TempData <<= 8;
                        TempData |= Data[5+cntByte];
                      }

                      AxumData.Monitor[MonitorBussNr].SpeakerLevel += (float)TempData/10;
                      if (AxumData.Monitor[MonitorBussNr].SpeakerLevel<-140)
                      {
                        AxumData.Monitor[MonitorBussNr].SpeakerLevel = -140;
                      }
                      else
                      {
                        if (AxumData.Monitor[MonitorBussNr].SpeakerLevel > 20)
                        {
                          AxumData.Monitor[MonitorBussNr].SpeakerLevel = 20;
                        }
                      }
                      CheckObjectsToSent(SensorReceiveFunctionNumber);

                      for (int cntDestination=0; cntDestination<1280; cntDestination++)
                      {
                        if (AxumData.DestinationData[cntDestination].Source == (matrix_sources.src_offset.min.monitor_buss+MonitorBussNr))
                        {
                          unsigned int DisplayFunctionNr = 0x06000000 | (cntDestination<<12);
                          CheckObjectsToSent(DisplayFunctionNr | DESTINATION_FUNCTION_MONITOR_SPEAKER_LEVEL);
                        }
                      }
                    }
                  }
                  break;
                  case MONITOR_BUSS_FUNCTION_TALKBACK_1:
                  case MONITOR_BUSS_FUNCTION_TALKBACK_2:
                  case MONITOR_BUSS_FUNCTION_TALKBACK_3:
                  case MONITOR_BUSS_FUNCTION_TALKBACK_4:
                  case MONITOR_BUSS_FUNCTION_TALKBACK_5:
                  case MONITOR_BUSS_FUNCTION_TALKBACK_6:
                  case MONITOR_BUSS_FUNCTION_TALKBACK_7:
                  case MONITOR_BUSS_FUNCTION_TALKBACK_8:
                  case MONITOR_BUSS_FUNCTION_TALKBACK_9:
                  case MONITOR_BUSS_FUNCTION_TALKBACK_10:
                  case MONITOR_BUSS_FUNCTION_TALKBACK_11:
                  case MONITOR_BUSS_FUNCTION_TALKBACK_12:
                  case MONITOR_BUSS_FUNCTION_TALKBACK_13:
                  case MONITOR_BUSS_FUNCTION_TALKBACK_14:
                  case MONITOR_BUSS_FUNCTION_TALKBACK_15:
                  case MONITOR_BUSS_FUNCTION_TALKBACK_16:
                  {
                    int TalkbackNr = FunctionNr-MONITOR_BUSS_FUNCTION_TALKBACK_1;
                    printf("Talkback %d\n", TalkbackNr);
                    if (Data[3] == STATE_DATATYPE)
                    {
                      unsigned long TempData = 0;

                      for (int cntByte=0; cntByte<Data[4]; cntByte++)
                      {
                        TempData <<= 8;
                        TempData |= Data[5+cntByte];
                      }

                      if (TempData)
                      {
                        AxumData.Monitor[MonitorBussNr].Talkback[TalkbackNr] = !AxumData.Monitor[MonitorBussNr].Talkback[TalkbackNr];
                      }
                      else
                      {
                        int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                        if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                        {
                          if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                          {
                            AxumData.Monitor[MonitorBussNr].Talkback[TalkbackNr] = !AxumData.Monitor[MonitorBussNr].Talkback[TalkbackNr];
                          }
                        }
                      }

                      //Check talkbacks, if dim is required
                      int TalkbackActive = 0;
                      for (int cntTalkback=0; cntTalkback<16; cntTalkback++)
                      {
                        TalkbackActive |= AxumData.Monitor[MonitorBussNr].Talkback[cntTalkback];
                      }
                      AxumData.Monitor[MonitorBussNr].Dim = TalkbackActive;

                      CheckObjectsToSent(SensorReceiveFunctionNumber);

                      unsigned int FunctionNrToSent = 0x02000000 | (MonitorBussNr<<12);
                      CheckObjectsToSent(FunctionNrToSent | (MONITOR_BUSS_FUNCTION_DIM));

                      for (int cntDestination=0; cntDestination<1280; cntDestination++)
                      {
                        if (AxumData.DestinationData[cntDestination].Source == (matrix_sources.src_offset.min.monitor_buss+MonitorBussNr))
                        {
                          unsigned int DisplayFunctionNr = 0x06000000 | (cntDestination<<12);

                          CheckObjectsToSent(DisplayFunctionNr | (DESTINATION_FUNCTION_TALKBACK_1_AND_MONITOR_TALKBACK_1 + ((DESTINATION_FUNCTION_TALKBACK_2_AND_MONITOR_TALKBACK_2-DESTINATION_FUNCTION_TALKBACK_1_AND_MONITOR_TALKBACK_1)*TalkbackNr)));
                          CheckObjectsToSent(DisplayFunctionNr | DESTINATION_FUNCTION_DIM_AND_MONITOR_DIM);
                        }
                      }
                    }
                  }
                  break;
                  default:
                  { //not implemented function
                    printf("FunctionNr %d\n", FunctionNr);
                  }
                  break;
                  }
                }
              }
              break;
              case GLOBAL_FUNCTIONS:
              {   //Global
                unsigned int GlobalNr = (SensorReceiveFunctionNumber>>12)&0xFFF;
                unsigned int FunctionNr = SensorReceiveFunctionNumber&0xFFF;

                printf(" > Global ");

                if (GlobalNr == 0)
                {
                  if (((signed)FunctionNr>=GLOBAL_FUNCTION_REDLIGHT_1) && (FunctionNr<GLOBAL_FUNCTION_CONTROL_1_MODE_SOURCE))
                  { //all states
                    if (Data[3] == STATE_DATATYPE)
                    {
                      unsigned long TempData = 0;

                      for (int cntByte=0; cntByte<Data[4]; cntByte++)
                      {
                        TempData <<= 8;
                        TempData |= Data[5+cntByte];
                      }

                      switch (FunctionNr)
                      {
                      case GLOBAL_FUNCTION_REDLIGHT_1:
                      case GLOBAL_FUNCTION_REDLIGHT_2:
                      case GLOBAL_FUNCTION_REDLIGHT_3:
                      case GLOBAL_FUNCTION_REDLIGHT_4:
                      case GLOBAL_FUNCTION_REDLIGHT_5:
                      case GLOBAL_FUNCTION_REDLIGHT_6:
                      case GLOBAL_FUNCTION_REDLIGHT_7:
                      case GLOBAL_FUNCTION_REDLIGHT_8:
                      {
                        printf("Redlight %d\n", FunctionNr-GLOBAL_FUNCTION_REDLIGHT_1);

                        if (TempData)
                        {
                          AxumData.Redlight[FunctionNr-GLOBAL_FUNCTION_REDLIGHT_1] = !AxumData.Redlight[FunctionNr-GLOBAL_FUNCTION_REDLIGHT_1];
                        }
                        else
                        {
                          int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                          if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                          {
                            if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                            {
                              AxumData.Redlight[FunctionNr-GLOBAL_FUNCTION_REDLIGHT_1] = !AxumData.Redlight[FunctionNr-GLOBAL_FUNCTION_REDLIGHT_1];
                            }
                          }
                        }
                        CheckObjectsToSent(SensorReceiveFunctionNumber);
                      }
                      break;
                      case GLOBAL_FUNCTION_BUSS_1_2_RESET:
                      case GLOBAL_FUNCTION_BUSS_3_4_RESET:
                      case GLOBAL_FUNCTION_BUSS_5_6_RESET:
                      case GLOBAL_FUNCTION_BUSS_7_8_RESET:
                      case GLOBAL_FUNCTION_BUSS_9_10_RESET:
                      case GLOBAL_FUNCTION_BUSS_11_12_RESET:
                      case GLOBAL_FUNCTION_BUSS_13_14_RESET:
                      case GLOBAL_FUNCTION_BUSS_15_16_RESET:
                      case GLOBAL_FUNCTION_BUSS_17_18_RESET:
                      case GLOBAL_FUNCTION_BUSS_19_20_RESET:
                      case GLOBAL_FUNCTION_BUSS_21_22_RESET:
                      case GLOBAL_FUNCTION_BUSS_23_24_RESET:
                      case GLOBAL_FUNCTION_BUSS_25_26_RESET:
                      case GLOBAL_FUNCTION_BUSS_27_28_RESET:
                      case GLOBAL_FUNCTION_BUSS_29_30_RESET:
                      case GLOBAL_FUNCTION_BUSS_31_32_RESET:
                      { //Do buss reset
                        int BussNr = FunctionNr-GLOBAL_FUNCTION_BUSS_1_2_RESET;
                        printf("Buss %d/%d reset\n", (BussNr*2)+1, (BussNr*2)+2);

                        if (TempData)
                        {
                          DoAxum_BussReset(BussNr);
                        }
                      }
                      break;
                      }
                    }
                  }
                  else if ((FunctionNr>=GLOBAL_FUNCTION_CONTROL_1_MODE_SOURCE) && (FunctionNr<GLOBAL_FUNCTION_CONTROL_2_MODE_SOURCE))
                  { //Control 1 modes
                    printf("Control 1 modes\n");
                    if (Data[3] == STATE_DATATYPE)
                    {
                      unsigned long TempData = 0;

                      for (int cntByte=0; cntByte<Data[4]; cntByte++)
                      {
                        TempData <<= 8;
                        TempData |= Data[5+cntByte];
                      }

                      int NewControl1Mode = -2;
                      if (TempData)
                      {
                        NewControl1Mode = FunctionNr-GLOBAL_FUNCTION_CONTROL_1_MODE_SOURCE;
                      }
                      else
                      {
                        int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                        if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                        {
                          if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                          {
                            if (AxumData.Control1Mode == ((signed int)(FunctionNr-GLOBAL_FUNCTION_CONTROL_1_MODE_SOURCE)))
                            {
                              NewControl1Mode = AxumData.Control1Mode;
                            }
                          }
                        }
                      }

                      if (NewControl1Mode > -2)
                      {
                        if (AxumData.Control1Mode == NewControl1Mode)
                        {
                          AxumData.Control1Mode = -1;
                          CheckObjectsToSent(SensorReceiveFunctionNumber);
                        }
                        else
                        {
                          unsigned int OldFunctionNumber = 0x04000000 | (AxumData.Control1Mode+GLOBAL_FUNCTION_CONTROL_1_MODE_SOURCE);
                          AxumData.Control1Mode = NewControl1Mode;
                          CheckObjectsToSent(OldFunctionNumber);
                          CheckObjectsToSent(SensorReceiveFunctionNumber);
                        }

                        for (int cntModule=0; cntModule<128; cntModule++)
                        {
                          unsigned int FunctionNrToSent = (cntModule<<12);
                          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1_LABEL);
                        }
                      }
                    }
                  }
                  else if ((FunctionNr>=GLOBAL_FUNCTION_CONTROL_2_MODE_SOURCE) && (FunctionNr<GLOBAL_FUNCTION_CONTROL_3_MODE_SOURCE))
                  { //Control 2 modes
                    printf("Control 2 modes\n");
                    if (Data[3] == STATE_DATATYPE)
                    {
                      unsigned long TempData = 0;

                      for (int cntByte=0; cntByte<Data[4]; cntByte++)
                      {
                        TempData <<= 8;
                        TempData |= Data[5+cntByte];
                      }

                      int NewControl2Mode = -2;
                      if (TempData)
                      {
                        NewControl2Mode = FunctionNr-GLOBAL_FUNCTION_CONTROL_2_MODE_SOURCE;
                      }
                      else
                      {
                        int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                        if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                        {
                          if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                          {
                            if (AxumData.Control2Mode == ((signed int)(FunctionNr-GLOBAL_FUNCTION_CONTROL_2_MODE_SOURCE)))
                            {
                              NewControl2Mode = AxumData.Control2Mode;
                            }
                          }
                        }
                      }


                      if (NewControl2Mode > -2)
                      {
                        if (AxumData.Control2Mode == NewControl2Mode)
                        {
                          AxumData.Control2Mode = -1;
                          CheckObjectsToSent(SensorReceiveFunctionNumber);
                        }
                        else
                        {
                          unsigned int OldFunctionNumber = 0x04000000 | (AxumData.Control2Mode+GLOBAL_FUNCTION_CONTROL_2_MODE_SOURCE);
                          AxumData.Control2Mode = NewControl2Mode;
                          CheckObjectsToSent(OldFunctionNumber);
                          CheckObjectsToSent(SensorReceiveFunctionNumber);
                        }

                        for (int cntModule=0; cntModule<128; cntModule++)
                        {
                          unsigned int FunctionNrToSent = (cntModule<<12);
                          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2_LABEL);
                        }
                      }
                    }
                  }
                  else if ((FunctionNr>=GLOBAL_FUNCTION_CONTROL_3_MODE_SOURCE) && (FunctionNr<GLOBAL_FUNCTION_CONTROL_4_MODE_SOURCE))
                  { //Control 3 modes
                    printf("Control 3 modes\n");
                    if (Data[3] == STATE_DATATYPE)
                    {
                      unsigned long TempData = 0;

                      for (int cntByte=0; cntByte<Data[4]; cntByte++)
                      {
                        TempData <<= 8;
                        TempData |= Data[5+cntByte];
                      }

                      int NewControl3Mode = -2;
                      if (TempData)
                      {
                        NewControl3Mode = FunctionNr-GLOBAL_FUNCTION_CONTROL_3_MODE_SOURCE;
                      }
                      else
                      {
                        int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                        if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                        {
                          if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                          {
                            if (AxumData.Control3Mode == ((signed int)(FunctionNr-GLOBAL_FUNCTION_CONTROL_3_MODE_SOURCE)))
                            {
                              NewControl3Mode = AxumData.Control3Mode;
                            }
                          }
                        }
                      }

                      printf("NewControl3Mode: %d\n", NewControl3Mode);

                      if (NewControl3Mode > -2)
                      {
                        if (AxumData.Control3Mode == NewControl3Mode)
                        {
                          AxumData.Control3Mode = -1;
                          CheckObjectsToSent(SensorReceiveFunctionNumber);
                        }
                        else
                        {
                          unsigned int OldFunctionNumber = 0x04000000 | (AxumData.Control3Mode+GLOBAL_FUNCTION_CONTROL_3_MODE_SOURCE);
                          AxumData.Control3Mode = NewControl3Mode;
                          CheckObjectsToSent(OldFunctionNumber);
                          CheckObjectsToSent(SensorReceiveFunctionNumber);
                        }

                        for (int cntModule=0; cntModule<128; cntModule++)
                        {
                          unsigned int FunctionNrToSent = (cntModule<<12);
                          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3_LABEL);
                        }
                      }
                    }
                  }
                  else if ((FunctionNr>=GLOBAL_FUNCTION_CONTROL_4_MODE_SOURCE) && (FunctionNr<GLOBAL_FUNCTION_MASTER_CONTROL_1_MODE_BUSS_1_2))
                  { //control 4 modes
                    printf("Control 4 modes\n");
                    if (Data[3] == STATE_DATATYPE)
                    {
                      unsigned long TempData = 0;

                      for (int cntByte=0; cntByte<Data[4]; cntByte++)
                      {
                        TempData <<= 8;
                        TempData |= Data[5+cntByte];
                      }

                      int NewControl4Mode = -2;
                      if (TempData)
                      {
                        NewControl4Mode = FunctionNr-GLOBAL_FUNCTION_CONTROL_4_MODE_SOURCE;
                      }
                      else
                      {
                        int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                        if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                        {
                          if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                          {
                            if (AxumData.Control4Mode == ((signed int)(FunctionNr-GLOBAL_FUNCTION_CONTROL_4_MODE_SOURCE)))
                            {
                              NewControl4Mode = AxumData.Control4Mode;
                            }
                          }
                        }
                      }

                      if (NewControl4Mode > -2)
                      {
                        if (AxumData.Control4Mode == NewControl4Mode)
                        {
                          AxumData.Control4Mode = -1;
                          CheckObjectsToSent(SensorReceiveFunctionNumber);
                        }
                        else
                        {
                          unsigned int OldFunctionNumber = 0x04000000 | (AxumData.Control4Mode+GLOBAL_FUNCTION_CONTROL_4_MODE_SOURCE);
                          AxumData.Control4Mode = NewControl4Mode;
                          CheckObjectsToSent(OldFunctionNumber);
                          CheckObjectsToSent(SensorReceiveFunctionNumber);
                        }

                        for (int cntModule=0; cntModule<128; cntModule++)
                        {
                          unsigned int FunctionNrToSent = (cntModule<<12);
                          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
                          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4_LABEL);
                        }
                      }
                    }
                  }
                  else if ((FunctionNr>=GLOBAL_FUNCTION_MASTER_CONTROL_1_MODE_BUSS_1_2) && (FunctionNr<GLOBAL_FUNCTION_MASTER_CONTROL_2_MODE_BUSS_1_2))
                  { //Master control 1 mode
                    printf("Master control 1 mode\n");
                    if (Data[3] == STATE_DATATYPE)
                    {
                      unsigned long TempData = 0;

                      for (int cntByte=0; cntByte<Data[4]; cntByte++)
                      {
                        TempData <<= 8;
                        TempData |= Data[5+cntByte];
                      }

                      if (TempData)
                      {
                        char NewMasterControl1Mode = FunctionNr-GLOBAL_FUNCTION_MASTER_CONTROL_1_MODE_BUSS_1_2;

                        unsigned int OldFunctionNumber = 0x04000000 | (AxumData.MasterControl1Mode+GLOBAL_FUNCTION_MASTER_CONTROL_1_MODE_BUSS_1_2);
                        AxumData.MasterControl1Mode = NewMasterControl1Mode;
                        CheckObjectsToSent(OldFunctionNumber);
                        CheckObjectsToSent(SensorReceiveFunctionNumber);
                        CheckObjectsToSent(0x04000000 | GLOBAL_FUNCTION_MASTER_CONTROL_1);
                      }
                    }
                  }
                  else if ((FunctionNr>=GLOBAL_FUNCTION_MASTER_CONTROL_2_MODE_BUSS_1_2) && (FunctionNr<GLOBAL_FUNCTION_MASTER_CONTROL_3_MODE_BUSS_1_2))
                  { //Master control 2 mode
                    printf("Master control 2 mode\n");
                    if (Data[3] == STATE_DATATYPE)
                    {
                      unsigned long TempData = 0;

                      for (int cntByte=0; cntByte<Data[4]; cntByte++)
                      {
                        TempData <<= 8;
                        TempData |= Data[5+cntByte];
                      }

                      if (TempData)
                      {
                        char NewMasterControl2Mode = FunctionNr-GLOBAL_FUNCTION_MASTER_CONTROL_2_MODE_BUSS_1_2;

                        unsigned int OldFunctionNumber = 0x04000000 | (AxumData.MasterControl2Mode+GLOBAL_FUNCTION_MASTER_CONTROL_2_MODE_BUSS_1_2);
                        AxumData.MasterControl2Mode = NewMasterControl2Mode;
                        CheckObjectsToSent(OldFunctionNumber);
                        CheckObjectsToSent(SensorReceiveFunctionNumber);
                        CheckObjectsToSent(0x04000000 | GLOBAL_FUNCTION_MASTER_CONTROL_2);
                      }
                    }
                  }
                  else if ((FunctionNr>=GLOBAL_FUNCTION_MASTER_CONTROL_3_MODE_BUSS_1_2) && (FunctionNr<GLOBAL_FUNCTION_MASTER_CONTROL_4_MODE_BUSS_1_2))
                  { //Master control 3 mode
                    printf("Master control 3 mode\n");
                    if (Data[3] == STATE_DATATYPE)
                    {
                      unsigned long TempData = 0;

                      for (int cntByte=0; cntByte<Data[4]; cntByte++)
                      {
                        TempData <<= 8;
                        TempData |= Data[5+cntByte];
                      }

                      if (TempData)
                      {
                        char NewMasterControl3Mode = FunctionNr-GLOBAL_FUNCTION_MASTER_CONTROL_3_MODE_BUSS_1_2;

                        unsigned int OldFunctionNumber = 0x04000000 | (AxumData.MasterControl3Mode+GLOBAL_FUNCTION_MASTER_CONTROL_3_MODE_BUSS_1_2);
                        AxumData.MasterControl3Mode = NewMasterControl3Mode;
                        CheckObjectsToSent(OldFunctionNumber);
                        CheckObjectsToSent(SensorReceiveFunctionNumber);
                        CheckObjectsToSent(0x04000000 | GLOBAL_FUNCTION_MASTER_CONTROL_3);
                      }
                    }
                  }
                  else if ((FunctionNr>=GLOBAL_FUNCTION_MASTER_CONTROL_4_MODE_BUSS_1_2) && (FunctionNr<GLOBAL_FUNCTION_MASTER_CONTROL_1))
                  { //Master control 1 mode
                    printf("Master control 4 mode\n");
                    if (Data[3] == STATE_DATATYPE)
                    {
                      unsigned long TempData = 0;

                      for (int cntByte=0; cntByte<Data[4]; cntByte++)
                      {
                        TempData <<= 8;
                        TempData |= Data[5+cntByte];
                      }

                      if (TempData)
                      {
                        char NewMasterControl4Mode = FunctionNr-GLOBAL_FUNCTION_MASTER_CONTROL_4_MODE_BUSS_1_2;

                        unsigned int OldFunctionNumber = 0x04000000 | (AxumData.MasterControl4Mode+GLOBAL_FUNCTION_MASTER_CONTROL_4_MODE_BUSS_1_2);
                        AxumData.MasterControl4Mode = NewMasterControl4Mode;
                        CheckObjectsToSent(OldFunctionNumber);
                        CheckObjectsToSent(SensorReceiveFunctionNumber);
                        CheckObjectsToSent(0x04000000 | GLOBAL_FUNCTION_MASTER_CONTROL_4);
                      }
                    }
                  }
                  else
                  {
                    switch (FunctionNr)
                    {
                    case GLOBAL_FUNCTION_MASTER_CONTROL_1:
                    case GLOBAL_FUNCTION_MASTER_CONTROL_2:
                    case GLOBAL_FUNCTION_MASTER_CONTROL_3:
                    case GLOBAL_FUNCTION_MASTER_CONTROL_4:
                    {
                      MasterModeControllerSensorChange(SensorReceiveFunctionNumber, Data, DataType, DataSize, DataMinimal, DataMaximal);
                    }
                    break;
                    case GLOBAL_FUNCTION_MASTER_CONTROL_1_RESET:
                    case GLOBAL_FUNCTION_MASTER_CONTROL_2_RESET:
                    case GLOBAL_FUNCTION_MASTER_CONTROL_3_RESET:
                    case GLOBAL_FUNCTION_MASTER_CONTROL_4_RESET:
                    {
                      MasterModeControllerResetSensorChange(SensorReceiveFunctionNumber, Data, DataType, DataSize, DataMinimal, DataMaximal);
                    }
                    break;
                    default:
                    { //not implemented function
                      printf("FunctionNr %d\n", FunctionNr);
                    }
                    break;
                    }
                  }
                }
              }
              break;
              case SOURCE_FUNCTIONS:
              { //Source
                unsigned int SourceNr = ((SensorReceiveFunctionNumber>>12)&0xFFF);
                unsigned int FunctionNr = SensorReceiveFunctionNumber&0xFFF;

                printf(" > SourceNr %d ", SourceNr);
                switch (FunctionNr)
                {
                case SOURCE_FUNCTION_MODULE_ON:
                case SOURCE_FUNCTION_MODULE_OFF:
                case SOURCE_FUNCTION_MODULE_ON_OFF:
                {   //Module on
                  //Module off
                  //Module on/off
                  printf("Source module on/off\n");

                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    for (int cntModule=0; cntModule<128; cntModule++)
                    {
                      if (AxumData.ModuleData[cntModule].Source == (SourceNr+matrix_sources.src_offset.min.source))
                      {
                        int CurrentOn = AxumData.ModuleData[cntModule].On;

                        if (TempData)
                        {
                          switch (FunctionNr)
                          {
                          case SOURCE_FUNCTION_MODULE_ON:
                          {
                            AxumData.ModuleData[cntModule].On = 1;
                          }
                          break;
                          case SOURCE_FUNCTION_MODULE_OFF:
                          {
                            AxumData.ModuleData[cntModule].On = 0;
                          }
                          break;
                          case SOURCE_FUNCTION_MODULE_ON_OFF:
                          {
                            AxumData.ModuleData[cntModule].On = !AxumData.ModuleData[cntModule].On;
                          }
                          break;
                          }
                        }
                        else
                        {
                          int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                          if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                          {
                            if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                            {
                              switch (FunctionNr)
                              {
                              case SOURCE_FUNCTION_MODULE_ON_OFF:
                              {
                                AxumData.ModuleData[cntModule].On = !AxumData.ModuleData[cntModule].On;
                              }
                              break;
                              }
                            }
                          }
                        }
                        int NewOn = AxumData.ModuleData[cntModule].On;

                        SetAxum_BussLevels(cntModule);

                        SensorReceiveFunctionNumber = ((cntModule)<<12);
                        CheckObjectsToSent(SensorReceiveFunctionNumber+MODULE_FUNCTION_MODULE_ON);
                        CheckObjectsToSent(SensorReceiveFunctionNumber+MODULE_FUNCTION_MODULE_OFF);
                        CheckObjectsToSent(SensorReceiveFunctionNumber+MODULE_FUNCTION_MODULE_ON_OFF);

                        if (CurrentOn != NewOn)
                        { //module on changed
                          DoAxum_ModuleStatusChanged(cntModule);

                          SensorReceiveFunctionNumber = 0x05000000 | (SourceNr<<12);
                          CheckObjectsToSent(SensorReceiveFunctionNumber+SOURCE_FUNCTION_MODULE_ON);
                          CheckObjectsToSent(SensorReceiveFunctionNumber+SOURCE_FUNCTION_MODULE_OFF);
                          CheckObjectsToSent(SensorReceiveFunctionNumber+SOURCE_FUNCTION_MODULE_ON_OFF);
                          CheckObjectsToSent(SensorReceiveFunctionNumber+SOURCE_FUNCTION_MODULE_FADER_AND_ON_ACTIVE);
                          CheckObjectsToSent(SensorReceiveFunctionNumber+SOURCE_FUNCTION_MODULE_FADER_AND_ON_INACTIVE);
                        }
                      }
                    }
                  }
                }
                break;
                case SOURCE_FUNCTION_MODULE_FADER_ON:
                case SOURCE_FUNCTION_MODULE_FADER_OFF:
                case SOURCE_FUNCTION_MODULE_FADER_ON_OFF:
                {   //fader on
                  //fader off
                  //fader on/off
                  printf("Source fader on/off\n");

                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    for (int cntModule=0; cntModule<128; cntModule++)
                    {
                      if (AxumData.ModuleData[cntModule].Source == (SourceNr+matrix_sources.src_offset.min.source))
                      {
                        float CurrentLevel = AxumData.ModuleData[cntModule].FaderLevel;
                        if (TempData)
                        {
                          switch (FunctionNr)
                          {
                          case SOURCE_FUNCTION_MODULE_FADER_ON:
                          {
                            AxumData.ModuleData[cntModule].FaderLevel = 0;
                          }
                          break;
                          case SOURCE_FUNCTION_MODULE_FADER_OFF:
                          {
                            AxumData.ModuleData[cntModule].FaderLevel = -140;
                          }
                          break;
                          case SOURCE_FUNCTION_MODULE_FADER_ON_OFF:
                          {
                            if (AxumData.ModuleData[cntModule].FaderLevel>-80)
                            {
                              AxumData.ModuleData[cntModule].FaderLevel = -140;
                            }
                            else
                            {
                              AxumData.ModuleData[cntModule].FaderLevel = 0;
                            }
                          }
                          break;
                          }
                        }
                        else
                        {
                          int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                          if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                          {
                            if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                            {
                              switch (FunctionNr)
                              {
                              case SOURCE_FUNCTION_MODULE_FADER_ON_OFF:
                              {
                                if (AxumData.ModuleData[cntModule].FaderLevel>-80)
                                {
                                  AxumData.ModuleData[cntModule].FaderLevel = -140;
                                }
                                else
                                {
                                  AxumData.ModuleData[cntModule].FaderLevel = 0;
                                }
                              }
                              break;
                              }
                            }
                          }
                        }
                        float NewLevel = AxumData.ModuleData[cntModule].FaderLevel;
                        SetAxum_BussLevels(cntModule);

                        SensorReceiveFunctionNumber = ((cntModule)<<12);
                        CheckObjectsToSent(SensorReceiveFunctionNumber+MODULE_FUNCTION_MODULE_LEVEL);

                        if (((CurrentLevel<=-80) && (NewLevel>-80)) ||
                            ((CurrentLevel>-80) && (NewLevel<=-80)))
                        { //fader on changed
                          DoAxum_ModuleStatusChanged(cntModule);

                          SensorReceiveFunctionNumber = 0x05000000 | (SourceNr<<12);
                          CheckObjectsToSent(SensorReceiveFunctionNumber+SOURCE_FUNCTION_MODULE_FADER_ON);
                          CheckObjectsToSent(SensorReceiveFunctionNumber+SOURCE_FUNCTION_MODULE_FADER_OFF);
                          CheckObjectsToSent(SensorReceiveFunctionNumber+SOURCE_FUNCTION_MODULE_FADER_ON_OFF);
                          CheckObjectsToSent(SensorReceiveFunctionNumber+SOURCE_FUNCTION_MODULE_FADER_AND_ON_ACTIVE);
                          CheckObjectsToSent(SensorReceiveFunctionNumber+SOURCE_FUNCTION_MODULE_FADER_AND_ON_INACTIVE);
                        }
                      }
                    }
                  }
                }
                break;
                case SOURCE_FUNCTION_MODULE_FADER_AND_ON_ACTIVE:
                case SOURCE_FUNCTION_MODULE_FADER_AND_ON_INACTIVE:
                {   //fader on and on active
                  //fader on and on inactive
                  printf("Source fader on and on\n");

                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    for (int cntModule=0; cntModule<128; cntModule++)
                    {
                      if (AxumData.ModuleData[cntModule].Source == (SourceNr+matrix_sources.src_offset.min.source))
                      {
                        float CurrentLevel = AxumData.ModuleData[cntModule].FaderLevel;
                        float CurrentOn = AxumData.ModuleData[cntModule].On;
                        if (TempData)
                        {
                          switch (FunctionNr)
                          {
                          case SOURCE_FUNCTION_MODULE_FADER_AND_ON_ACTIVE:
                          {
                            AxumData.ModuleData[cntModule].FaderLevel = 0;
                            AxumData.ModuleData[cntModule].On = 1;
                          }
                          break;
                          case SOURCE_FUNCTION_MODULE_FADER_AND_ON_INACTIVE:
                          {
                            AxumData.ModuleData[cntModule].FaderLevel = -140;
                            AxumData.ModuleData[cntModule].On = 0;
                          }
                          break;
                          }
                        }
                        else
                        {
                          int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                          if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                          {
                            if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                            {
                            }
                          }
                        }
                        float NewLevel = AxumData.ModuleData[cntModule].FaderLevel;
                        int NewOn = AxumData.ModuleData[cntModule].On;

                        SetAxum_BussLevels(cntModule);

                        SensorReceiveFunctionNumber = ((cntModule)<<12);
                        CheckObjectsToSent(SensorReceiveFunctionNumber+MODULE_FUNCTION_MODULE_ON);
                        CheckObjectsToSent(SensorReceiveFunctionNumber+MODULE_FUNCTION_MODULE_OFF);
                        CheckObjectsToSent(SensorReceiveFunctionNumber+MODULE_FUNCTION_MODULE_ON_OFF);
                        CheckObjectsToSent(SensorReceiveFunctionNumber+MODULE_FUNCTION_MODULE_LEVEL);

                        if (((CurrentLevel<=-80) && (NewLevel>-80)) ||
                            ((CurrentLevel>-80) && (NewLevel<=-80)) ||
                            (CurrentOn != NewOn))
                        { //fader on changed
                          DoAxum_ModuleStatusChanged(cntModule);

                          SensorReceiveFunctionNumber = 0x05000000 | (SourceNr<<12);
                          CheckObjectsToSent(SensorReceiveFunctionNumber+SOURCE_FUNCTION_MODULE_ON);
                          CheckObjectsToSent(SensorReceiveFunctionNumber+SOURCE_FUNCTION_MODULE_OFF);
                          CheckObjectsToSent(SensorReceiveFunctionNumber+SOURCE_FUNCTION_MODULE_ON_OFF);
                          CheckObjectsToSent(SensorReceiveFunctionNumber+SOURCE_FUNCTION_MODULE_FADER_ON);
                          CheckObjectsToSent(SensorReceiveFunctionNumber+SOURCE_FUNCTION_MODULE_FADER_OFF);
                          CheckObjectsToSent(SensorReceiveFunctionNumber+SOURCE_FUNCTION_MODULE_FADER_ON_OFF);
                          CheckObjectsToSent(SensorReceiveFunctionNumber+SOURCE_FUNCTION_MODULE_FADER_AND_ON_ACTIVE);
                          CheckObjectsToSent(SensorReceiveFunctionNumber+SOURCE_FUNCTION_MODULE_FADER_AND_ON_INACTIVE);
                        }
                      }
                    }
                  }
                }
                break;
                break;
                case SOURCE_FUNCTION_MODULE_BUSS_1_2_ON:
                case SOURCE_FUNCTION_MODULE_BUSS_3_4_ON:
                case SOURCE_FUNCTION_MODULE_BUSS_5_6_ON:
                case SOURCE_FUNCTION_MODULE_BUSS_7_8_ON:
                case SOURCE_FUNCTION_MODULE_BUSS_9_10_ON:
                case SOURCE_FUNCTION_MODULE_BUSS_11_12_ON:
                case SOURCE_FUNCTION_MODULE_BUSS_13_14_ON:
                case SOURCE_FUNCTION_MODULE_BUSS_15_16_ON:
                case SOURCE_FUNCTION_MODULE_BUSS_17_18_ON:
                case SOURCE_FUNCTION_MODULE_BUSS_19_20_ON:
                case SOURCE_FUNCTION_MODULE_BUSS_21_22_ON:
                case SOURCE_FUNCTION_MODULE_BUSS_23_24_ON:
                case SOURCE_FUNCTION_MODULE_BUSS_25_26_ON:
                case SOURCE_FUNCTION_MODULE_BUSS_27_28_ON:
                case SOURCE_FUNCTION_MODULE_BUSS_29_30_ON:
                case SOURCE_FUNCTION_MODULE_BUSS_31_32_ON:
                {  //Buss 1/2 on
                  int BussNr = (FunctionNr-SOURCE_FUNCTION_MODULE_BUSS_1_2_ON)/(SOURCE_FUNCTION_MODULE_BUSS_3_4_ON-SOURCE_FUNCTION_MODULE_BUSS_1_2_ON);
                  printf("Source buss %d/%d on\n", (BussNr*2)+1, (BussNr*2)+2);

                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if (TempData)
                    {
                      unsigned int FunctionNrToSent;

                      //Do interlock
                      if (AxumData.BussMasterData[BussNr].Interlock)
                      {
                        for (int cntModule=0; cntModule<128; cntModule++)
                        {
                          if (AxumData.ModuleData[cntModule].Buss[BussNr].On)
                          {
                            AxumData.ModuleData[cntModule].Buss[BussNr].On = 0;

                            SetAxum_BussLevels(cntModule);
                            SetAxum_ModuleMixMinus(cntModule, 0);

                            unsigned int FunctionNrToSent = ((cntModule<<12)&0xFFF000);
                            CheckObjectsToSent(FunctionNrToSent | FunctionNr);

                            FunctionNrToSent = ((cntModule<<12)&0xFFF000);
                            CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                            CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                            CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                            CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

                            if ((AxumData.ModuleData[cntModule].Source >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[cntModule].Source <= matrix_sources.src_offset.max.source))
                            {
                              int SourceNr = AxumData.ModuleData[cntModule].Source-matrix_sources.src_offset.min.source;
                              FunctionNrToSent = 0x05000000 | (SourceNr<<12);
                              CheckObjectsToSent(FunctionNrToSent | (SOURCE_FUNCTION_MODULE_BUSS_1_2_ON+(BussNr*(SOURCE_FUNCTION_MODULE_BUSS_3_4_ON-SOURCE_FUNCTION_MODULE_BUSS_1_2_ON))));
                              CheckObjectsToSent(FunctionNrToSent | (SOURCE_FUNCTION_MODULE_BUSS_1_2_OFF+(BussNr*(SOURCE_FUNCTION_MODULE_BUSS_3_4_OFF-SOURCE_FUNCTION_MODULE_BUSS_1_2_OFF))));
                              CheckObjectsToSent(FunctionNrToSent | (SOURCE_FUNCTION_MODULE_BUSS_1_2_ON_OFF+(BussNr*(SOURCE_FUNCTION_MODULE_BUSS_3_4_ON_OFF-SOURCE_FUNCTION_MODULE_BUSS_1_2_ON_OFF))));
                            }
                          }
                        }
                      }

                      for (int cntModule=0; cntModule<128; cntModule++)
                      {
                        if (AxumData.ModuleData[cntModule].Source == (SourceNr+matrix_sources.src_offset.min.source))
                        {
                          AxumData.ModuleData[cntModule].Buss[BussNr].On = 1;
                          SetAxum_BussLevels(cntModule);
                          SetAxum_ModuleMixMinus(cntModule, 0);

                          FunctionNrToSent = ((cntModule)<<12);
                          CheckObjectsToSent(FunctionNrToSent | (MODULE_FUNCTION_BUSS_1_2_ON_OFF+(BussNr*(MODULE_FUNCTION_BUSS_3_4_ON_OFF-MODULE_FUNCTION_BUSS_1_2_ON_OFF))));

                          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
                        }
                      }

                      FunctionNrToSent = 0x05000000 | (SourceNr<<12);
                      CheckObjectsToSent(FunctionNrToSent | (SOURCE_FUNCTION_MODULE_BUSS_1_2_ON+(BussNr*(SOURCE_FUNCTION_MODULE_BUSS_3_4_ON-SOURCE_FUNCTION_MODULE_BUSS_1_2_ON))));
                      CheckObjectsToSent(FunctionNrToSent | (SOURCE_FUNCTION_MODULE_BUSS_1_2_OFF+(BussNr*(SOURCE_FUNCTION_MODULE_BUSS_3_4_OFF-SOURCE_FUNCTION_MODULE_BUSS_1_2_OFF))));
                      CheckObjectsToSent(FunctionNrToSent | (SOURCE_FUNCTION_MODULE_BUSS_1_2_ON_OFF+(BussNr*(SOURCE_FUNCTION_MODULE_BUSS_3_4_ON_OFF-SOURCE_FUNCTION_MODULE_BUSS_1_2_ON))));

                      //Buss reset
                      FunctionNrToSent = 0x04000000;
                      CheckObjectsToSent(FunctionNrToSent | (GLOBAL_FUNCTION_BUSS_1_2_RESET+(BussNr*(GLOBAL_FUNCTION_BUSS_3_4_RESET-GLOBAL_FUNCTION_BUSS_1_2_RESET))));

                      //Check active buss auto switching
                      char BussActive = 0;
                      for (int cntModule=0; cntModule<128; cntModule++)
                      {
                        if (AxumData.ModuleData[cntModule].Buss[BussNr].On)
                        {
                          BussActive = 1;
                        }
                      }

                      for (int cntMonitorBuss=0; cntMonitorBuss<16; cntMonitorBuss++)
                      {
                        if (AxumData.Monitor[cntMonitorBuss].AutoSwitchingBuss[BussNr])
                        {
                          AxumData.Monitor[cntMonitorBuss].Buss[BussNr] = BussActive;
                        }
                      }

                      for (int cntMonitorBuss=0; cntMonitorBuss<16; cntMonitorBuss++)
                      {
                        unsigned int MonitorBussActive = 0;
                        int cntBuss;
                        for (cntBuss=0; cntBuss<16; cntBuss++)
                        {
                          if (AxumData.Monitor[cntMonitorBuss].Buss[cntBuss])
                          {
                            MonitorBussActive = 1;
                          }
                        }
                        int cntExt;
                        for (cntExt=0; cntExt<8; cntExt++)
                        {
                          if (AxumData.Monitor[cntMonitorBuss].Ext[cntExt])
                          {
                            MonitorBussActive = 1;
                          }
                        }

                        if (!MonitorBussActive)
                        {
                          int DefaultSelection = AxumData.Monitor[cntMonitorBuss].DefaultSelection;
                          if (DefaultSelection<16)
                          {
                            int BussNr = DefaultSelection;
                            AxumData.Monitor[cntMonitorBuss].Buss[BussNr] = 1;

                            unsigned int FunctionNrToSent = 0x02000000 | (cntMonitorBuss<<12);
                            CheckObjectsToSent(FunctionNrToSent | (MONITOR_BUSS_FUNCTION_BUSS_1_2_ON_OFF+BussNr));
                          }
                          else if (DefaultSelection<24)
                          {
                            int ExtNr = DefaultSelection-16;
                            AxumData.Monitor[cntMonitorBuss].Ext[ExtNr] = 1;

                            unsigned int FunctionNrToSent = 0x02000000 | (cntMonitorBuss<<12);
                            CheckObjectsToSent(FunctionNrToSent | (MONITOR_BUSS_FUNCTION_EXT_1_ON_OFF+ExtNr));
                          }
                        }
                      }

                      if (  (AxumData.BussMasterData[BussNr].PreModuleOn) &&
                            (AxumData.BussMasterData[BussNr].PreModuleLevel))
                      {
                        printf("Have to check monitor routing and muting\n");
                        DoAxum_ModulePreStatusChanged(BussNr);
                      }

                      for (int cntMonitorBuss=0; cntMonitorBuss<16; cntMonitorBuss++)
                      {
                        if (AxumData.Monitor[cntMonitorBuss].AutoSwitchingBuss[BussNr])
                        {
                          SetAxum_MonitorBuss(cntMonitorBuss);

                          unsigned int FunctionNrToSent = 0x02000000 | (cntMonitorBuss<<12);
                          CheckObjectsToSent(FunctionNrToSent | (MONITOR_BUSS_FUNCTION_BUSS_1_2_ON_OFF+BussNr));
                        }
                      }
                    }
                  }
                }
                break;
                case SOURCE_FUNCTION_MODULE_BUSS_1_2_OFF:
                case SOURCE_FUNCTION_MODULE_BUSS_3_4_OFF:
                case SOURCE_FUNCTION_MODULE_BUSS_5_6_OFF:
                case SOURCE_FUNCTION_MODULE_BUSS_7_8_OFF:
                case SOURCE_FUNCTION_MODULE_BUSS_9_10_OFF:
                case SOURCE_FUNCTION_MODULE_BUSS_11_12_OFF:
                case SOURCE_FUNCTION_MODULE_BUSS_13_14_OFF:
                case SOURCE_FUNCTION_MODULE_BUSS_15_16_OFF:
                case SOURCE_FUNCTION_MODULE_BUSS_17_18_OFF:
                case SOURCE_FUNCTION_MODULE_BUSS_19_20_OFF:
                case SOURCE_FUNCTION_MODULE_BUSS_21_22_OFF:
                case SOURCE_FUNCTION_MODULE_BUSS_23_24_OFF:
                case SOURCE_FUNCTION_MODULE_BUSS_25_26_OFF:
                case SOURCE_FUNCTION_MODULE_BUSS_27_28_OFF:
                case SOURCE_FUNCTION_MODULE_BUSS_29_30_OFF:
                case SOURCE_FUNCTION_MODULE_BUSS_31_32_OFF:
                {  //Buss 1/2 off
                  int BussNr = (FunctionNr-SOURCE_FUNCTION_MODULE_BUSS_1_2_OFF)/(SOURCE_FUNCTION_MODULE_BUSS_3_4_OFF-SOURCE_FUNCTION_MODULE_BUSS_1_2_OFF);
                  printf("Source buss %d/%d off\n", (BussNr*2)+1, (BussNr*2)+2);

                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if (TempData)
                    {
                      unsigned int FunctionNrToSent;

                      for (int cntModule=0; cntModule<128; cntModule++)
                      {
                        if (AxumData.ModuleData[cntModule].Source == (SourceNr+matrix_sources.src_offset.min.source))
                        {
                          AxumData.ModuleData[cntModule].Buss[BussNr].On = 0;
                          SetAxum_BussLevels(cntModule);
                          SetAxum_ModuleMixMinus(cntModule, 0);

                          FunctionNrToSent = ((cntModule)<<12);
                          CheckObjectsToSent(FunctionNrToSent | (MODULE_FUNCTION_BUSS_1_2_ON_OFF+(BussNr*(MODULE_FUNCTION_BUSS_3_4_ON_OFF-MODULE_FUNCTION_BUSS_1_2_ON_OFF))));

                          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
                        }
                      }

                      FunctionNrToSent = 0x05000000 | (SourceNr<<12);
                      CheckObjectsToSent(FunctionNrToSent | (SOURCE_FUNCTION_MODULE_BUSS_1_2_ON+(BussNr*(SOURCE_FUNCTION_MODULE_BUSS_3_4_ON-SOURCE_FUNCTION_MODULE_BUSS_1_2_ON))));
                      CheckObjectsToSent(FunctionNrToSent | (SOURCE_FUNCTION_MODULE_BUSS_1_2_OFF+(BussNr*(SOURCE_FUNCTION_MODULE_BUSS_3_4_OFF-SOURCE_FUNCTION_MODULE_BUSS_1_2_OFF))));
                      CheckObjectsToSent(FunctionNrToSent | (SOURCE_FUNCTION_MODULE_BUSS_1_2_ON_OFF+(BussNr*(SOURCE_FUNCTION_MODULE_BUSS_3_4_ON_OFF-SOURCE_FUNCTION_MODULE_BUSS_1_2_ON))));

                      FunctionNrToSent = 0x04000000;
                      CheckObjectsToSent(FunctionNrToSent | (GLOBAL_FUNCTION_BUSS_1_2_RESET+(BussNr*(GLOBAL_FUNCTION_BUSS_3_4_RESET-GLOBAL_FUNCTION_BUSS_1_2_RESET))));

                      //Check active buss auto switching
                      char BussActive = 0;
                      for (int cntModule=0; cntModule<128; cntModule++)
                      {
                        if (AxumData.ModuleData[cntModule].Buss[BussNr].On)
                        {
                          BussActive = 1;
                        }
                      }

                      for (int cntMonitorBuss=0; cntMonitorBuss<16; cntMonitorBuss++)
                      {
                        if (AxumData.Monitor[cntMonitorBuss].AutoSwitchingBuss[BussNr])
                        {
                          AxumData.Monitor[cntMonitorBuss].Buss[BussNr] = BussActive;
                        }
                      }

                      for (int cntMonitorBuss=0; cntMonitorBuss<16; cntMonitorBuss++)
                      {
                        unsigned int MonitorBussActive = 0;
                        int cntBuss;
                        for (cntBuss=0; cntBuss<16; cntBuss++)
                        {
                          if (AxumData.Monitor[cntMonitorBuss].Buss[cntBuss])
                          {
                            MonitorBussActive = 1;
                          }
                        }
                        int cntExt;
                        for (cntExt=0; cntExt<8; cntExt++)
                        {
                          if (AxumData.Monitor[cntMonitorBuss].Ext[cntExt])
                          {
                            MonitorBussActive = 1;
                          }
                        }

                        if (!MonitorBussActive)
                        {
                          int DefaultSelection = AxumData.Monitor[cntMonitorBuss].DefaultSelection;
                          if (DefaultSelection<16)
                          {
                            int BussNr = DefaultSelection;
                            AxumData.Monitor[cntMonitorBuss].Buss[BussNr] = 1;

                            unsigned int FunctionNrToSent = 0x02000000 | (cntMonitorBuss<<12);
                            CheckObjectsToSent(FunctionNrToSent | (MONITOR_BUSS_FUNCTION_BUSS_1_2_ON_OFF+BussNr));
                          }
                          else if (DefaultSelection<24)
                          {
                            int ExtNr = DefaultSelection-16;
                            AxumData.Monitor[cntMonitorBuss].Ext[ExtNr] = 1;

                            unsigned int FunctionNrToSent = 0x02000000 | (cntMonitorBuss<<12);
                            CheckObjectsToSent(FunctionNrToSent | (MONITOR_BUSS_FUNCTION_EXT_1_ON_OFF+ExtNr));
                          }
                        }
                      }

                      if (  (AxumData.BussMasterData[BussNr].PreModuleOn) &&
                            (AxumData.BussMasterData[BussNr].PreModuleLevel))
                      {
                        printf("Have to check monitor routing and muting\n");
                        DoAxum_ModulePreStatusChanged(BussNr);
                      }

                      for (int cntMonitorBuss=0; cntMonitorBuss<16; cntMonitorBuss++)
                      {
                        if (AxumData.Monitor[cntMonitorBuss].AutoSwitchingBuss[BussNr])
                        {
                          SetAxum_MonitorBuss(cntMonitorBuss);

                          unsigned int FunctionNrToSent = 0x02000000 | (cntMonitorBuss<<12);
                          CheckObjectsToSent(FunctionNrToSent | (MONITOR_BUSS_FUNCTION_BUSS_1_2_ON_OFF+BussNr));
                        }
                      }
                    }
                  }
                }
                break;
                case SOURCE_FUNCTION_MODULE_BUSS_1_2_ON_OFF:
                case SOURCE_FUNCTION_MODULE_BUSS_3_4_ON_OFF:
                case SOURCE_FUNCTION_MODULE_BUSS_5_6_ON_OFF:
                case SOURCE_FUNCTION_MODULE_BUSS_7_8_ON_OFF:
                case SOURCE_FUNCTION_MODULE_BUSS_9_10_ON_OFF:
                case SOURCE_FUNCTION_MODULE_BUSS_11_12_ON_OFF:
                case SOURCE_FUNCTION_MODULE_BUSS_13_14_ON_OFF:
                case SOURCE_FUNCTION_MODULE_BUSS_15_16_ON_OFF:
                case SOURCE_FUNCTION_MODULE_BUSS_17_18_ON_OFF:
                case SOURCE_FUNCTION_MODULE_BUSS_19_20_ON_OFF:
                case SOURCE_FUNCTION_MODULE_BUSS_21_22_ON_OFF:
                case SOURCE_FUNCTION_MODULE_BUSS_23_24_ON_OFF:
                case SOURCE_FUNCTION_MODULE_BUSS_25_26_ON_OFF:
                case SOURCE_FUNCTION_MODULE_BUSS_27_28_ON_OFF:
                case SOURCE_FUNCTION_MODULE_BUSS_29_30_ON_OFF:
                case SOURCE_FUNCTION_MODULE_BUSS_31_32_ON_OFF:
                {  //Buss 1/2 on/off
                  int BussNr = (FunctionNr-SOURCE_FUNCTION_MODULE_BUSS_1_2_ON_OFF)/(SOURCE_FUNCTION_MODULE_BUSS_3_4_ON_OFF-SOURCE_FUNCTION_MODULE_BUSS_1_2_ON_OFF);
                  printf("Source buss %d/%d on/off\n", (BussNr*2)+1, (BussNr*2)+2);

                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    unsigned int FunctionNrToSent;

                    for (int cntModule=0; cntModule<128; cntModule++)
                    {
                      if (AxumData.ModuleData[cntModule].Source == (SourceNr+matrix_sources.src_offset.min.source))
                      {
                        if (TempData)
                        {
                          AxumData.ModuleData[cntModule].Buss[BussNr].On = !AxumData.ModuleData[cntModule].Buss[BussNr].On;
                        }
                        else
                        {
                          int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                          if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                          {
                            if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                            {
                              AxumData.ModuleData[cntModule].Buss[BussNr].On = !AxumData.ModuleData[cntModule].Buss[BussNr].On;
                            }
                          }
                        }

                        SetAxum_BussLevels(cntModule);
                        SetAxum_ModuleMixMinus(cntModule, 0);

                        FunctionNrToSent = ((cntModule)<<12);
                        CheckObjectsToSent(FunctionNrToSent | (MODULE_FUNCTION_BUSS_1_2_ON_OFF+(BussNr*(MODULE_FUNCTION_BUSS_3_4_ON_OFF-MODULE_FUNCTION_BUSS_1_2_ON_OFF))));

                        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
                      }
                      else
                      {
                        //Do interlock
                        if (AxumData.BussMasterData[BussNr].Interlock)
                        {
                          if (AxumData.ModuleData[cntModule].Buss[BussNr].On)
                          {
                            AxumData.ModuleData[cntModule].Buss[BussNr].On = 0;

                            SetAxum_BussLevels(cntModule);
                            SetAxum_ModuleMixMinus(cntModule, 0);

                            FunctionNrToSent = ((cntModule)<<12);
                            CheckObjectsToSent(FunctionNrToSent | (MODULE_FUNCTION_BUSS_1_2_ON_OFF+(BussNr*(MODULE_FUNCTION_BUSS_3_4_ON_OFF-MODULE_FUNCTION_BUSS_1_2_ON_OFF))));

                            CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                            CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                            CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                            CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
                          }
                        }
                      }
                    }

                    FunctionNrToSent = 0x05000000 | (SourceNr<<12);
                    CheckObjectsToSent(FunctionNrToSent | (SOURCE_FUNCTION_MODULE_BUSS_1_2_ON+(BussNr*(SOURCE_FUNCTION_MODULE_BUSS_3_4_ON-SOURCE_FUNCTION_MODULE_BUSS_1_2_ON))));
                    CheckObjectsToSent(FunctionNrToSent | (SOURCE_FUNCTION_MODULE_BUSS_1_2_OFF+(BussNr*(SOURCE_FUNCTION_MODULE_BUSS_3_4_OFF-SOURCE_FUNCTION_MODULE_BUSS_1_2_OFF))));
                    CheckObjectsToSent(FunctionNrToSent | (SOURCE_FUNCTION_MODULE_BUSS_1_2_ON_OFF+(BussNr*(SOURCE_FUNCTION_MODULE_BUSS_3_4_ON_OFF-SOURCE_FUNCTION_MODULE_BUSS_1_2_ON_OFF))));

                    FunctionNrToSent = 0x04000000;
                    CheckObjectsToSent(FunctionNrToSent | (GLOBAL_FUNCTION_BUSS_1_2_RESET+(BussNr*(GLOBAL_FUNCTION_BUSS_3_4_RESET-GLOBAL_FUNCTION_BUSS_1_2_RESET))));

                    //Check active buss auto switching
                    char BussActive = 0;
                    for (int cntModule=0; cntModule<128; cntModule++)
                    {
                      if (AxumData.ModuleData[cntModule].Buss[BussNr].On)
                      {
                        BussActive = 1;
                      }
                    }

                    for (int cntMonitorBuss=0; cntMonitorBuss<16; cntMonitorBuss++)
                    {
                      if (AxumData.Monitor[cntMonitorBuss].AutoSwitchingBuss[BussNr])
                      {
                        AxumData.Monitor[cntMonitorBuss].Buss[BussNr] = BussActive;
                      }
                    }

                    for (int cntMonitorBuss=0; cntMonitorBuss<16; cntMonitorBuss++)
                    {
                      unsigned int MonitorBussActive = 0;
                      int cntBuss;
                      for (cntBuss=0; cntBuss<16; cntBuss++)
                      {
                        if (AxumData.Monitor[cntMonitorBuss].Buss[cntBuss])
                        {
                          MonitorBussActive = 1;
                        }
                      }
                      int cntExt;
                      for (cntExt=0; cntExt<8; cntExt++)
                      {
                        if (AxumData.Monitor[cntMonitorBuss].Ext[cntExt])
                        {
                          MonitorBussActive = 1;
                        }
                      }

                      if (!MonitorBussActive)
                      {
                        int DefaultSelection = AxumData.Monitor[cntMonitorBuss].DefaultSelection;
                        if (DefaultSelection<16)
                        {
                          int BussNr = DefaultSelection;
                          AxumData.Monitor[cntMonitorBuss].Buss[BussNr] = 1;

                          unsigned int FunctionNrToSent = 0x02000000 | (cntMonitorBuss<<12);
                          CheckObjectsToSent(FunctionNrToSent | (MONITOR_BUSS_FUNCTION_BUSS_1_2_ON_OFF+BussNr));
                        }
                        else if (DefaultSelection<24)
                        {
                          int ExtNr = DefaultSelection-16;
                          AxumData.Monitor[cntMonitorBuss].Ext[ExtNr] = 1;

                          unsigned int FunctionNrToSent = 0x02000000 | (cntMonitorBuss<<12);
                          CheckObjectsToSent(FunctionNrToSent | (MONITOR_BUSS_FUNCTION_EXT_1_ON_OFF+ExtNr));
                        }
                      }
                    }

                    if (  (AxumData.BussMasterData[BussNr].PreModuleOn) &&
                          (AxumData.BussMasterData[BussNr].PreModuleLevel))
                    {
                      printf("Have to check monitor routing and muting\n");
                      DoAxum_ModulePreStatusChanged(BussNr);
                    }

                    for (int cntMonitorBuss=0; cntMonitorBuss<16; cntMonitorBuss++)
                    {
                      if (AxumData.Monitor[cntMonitorBuss].AutoSwitchingBuss[BussNr])
                      {
                        SetAxum_MonitorBuss(cntMonitorBuss);

                        unsigned int FunctionNrToSent = 0x02000000 | (cntMonitorBuss<<12);
                        CheckObjectsToSent(FunctionNrToSent | (MONITOR_BUSS_FUNCTION_BUSS_1_2_ON_OFF+BussNr));
                      }
                    }
                  }
                }
                break;
                case SOURCE_FUNCTION_MODULE_COUGH_ON_OFF:
                {  //Cough
                  printf("Module cough on/off\n");

                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    for (int cntModule=0; cntModule<128; cntModule++)
                    {
                      if (AxumData.ModuleData[cntModule].Source == (SourceNr+matrix_sources.src_offset.min.source))
                      {
                        AxumData.ModuleData[cntModule].Cough = TempData;

                        SetAxum_BussLevels(cntModule);

                        SensorReceiveFunctionNumber = ((cntModule)<<12);
                        CheckObjectsToSent(SensorReceiveFunctionNumber+MODULE_FUNCTION_COUGH_ON_OFF);

                        SensorReceiveFunctionNumber = 0x05000000 | (SourceNr<<12);
                        CheckObjectsToSent(SensorReceiveFunctionNumber+SOURCE_FUNCTION_MODULE_COUGH_ON_OFF);
                      }
                    }
                  }
                }
                break;
                case SOURCE_FUNCTION_START:
                case SOURCE_FUNCTION_STOP:
                case SOURCE_FUNCTION_START_STOP:
                {
                  printf("start/stop\n");
                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;
                    char UpdateObjects = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if (TempData)
                    {
                      switch (FunctionNr)
                      {
                      case SOURCE_FUNCTION_START:
                      {
                        AxumData.SourceData[SourceNr].Start = 1;
                      }
                      break;
                      case SOURCE_FUNCTION_STOP:
                      {
                        AxumData.SourceData[SourceNr].Start = 0;
                      }
                      break;
                      case SOURCE_FUNCTION_START_STOP:
                      {
                        AxumData.SourceData[SourceNr].Start = !AxumData.SourceData[SourceNr].Start;
                      }
                      break;
                      }
                      UpdateObjects = 1;
                    }
                    else
                    {
                      int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                      if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                      {
                        if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                        {
                          switch (FunctionNr)
                          {
                          case SOURCE_FUNCTION_START_STOP:
                          {
                            AxumData.SourceData[SourceNr].Start = !AxumData.SourceData[SourceNr].Start;
                            UpdateObjects = 1;
                          }
                          break;
                          }
                        }
                      }
                    }


                    if (UpdateObjects)
                    { //only if pressed or changed
                      unsigned int DisplayFunctionNr = 0x05000000 | (SourceNr<<12);
                      CheckObjectsToSent(DisplayFunctionNr | SOURCE_FUNCTION_START);
                      CheckObjectsToSent(DisplayFunctionNr | SOURCE_FUNCTION_STOP);
                      CheckObjectsToSent(DisplayFunctionNr | SOURCE_FUNCTION_START_STOP);

                      for (int cntModule=0; cntModule<128; cntModule++)
                      {
                        if (AxumData.ModuleData[cntModule].Source == (SourceNr+matrix_sources.src_offset.min.source))
                        {
                          DisplayFunctionNr = (cntModule<<12);
                          CheckObjectsToSent(DisplayFunctionNr | MODULE_FUNCTION_SOURCE_START);
                          CheckObjectsToSent(DisplayFunctionNr | MODULE_FUNCTION_SOURCE_STOP);
                          CheckObjectsToSent(DisplayFunctionNr | MODULE_FUNCTION_SOURCE_START_STOP);
                        }
                      }
                    }
                  }
                }
                break;
                case SOURCE_FUNCTION_PHANTOM:
                {
                  printf("phantom\n");
                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if (TempData)
                    {
                      AxumData.SourceData[SourceNr].Phantom = !AxumData.SourceData[SourceNr].Phantom;
                    }
                    else
                    {
                      int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                      if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                      {
                        if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                        {
                          AxumData.SourceData[SourceNr].Phantom = !AxumData.SourceData[SourceNr].Phantom;
                        }
                      }
                    }

                    unsigned int DisplayFunctionNr = 0x05000000 | (SourceNr<<12) | SOURCE_FUNCTION_PHANTOM;
                    CheckObjectsToSent(DisplayFunctionNr);

                    for (int cntModule=0; cntModule<128; cntModule++)
                    {
                      if (AxumData.ModuleData[cntModule].Source == (SourceNr+matrix_sources.src_offset.min.source))
                      {
                        DisplayFunctionNr = (cntModule<<12) | MODULE_FUNCTION_SOURCE_PHANTOM;
                        CheckObjectsToSent(DisplayFunctionNr);
                      }
                    }
                  }
                }
                break;
                case SOURCE_FUNCTION_PAD:
                {
                  printf("Pad\n");
                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if (TempData)
                    {
                      AxumData.SourceData[SourceNr].Pad = !AxumData.SourceData[SourceNr].Pad;
                    }
                    else
                    {
                      int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                      if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                      {
                        if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                        {
                          AxumData.SourceData[SourceNr].Pad = !AxumData.SourceData[SourceNr].Pad;
                        }
                      }
                    }

                    unsigned int DisplayFunctionNr = 0x05000000 | (SourceNr<<12) | SOURCE_FUNCTION_PAD;
                    CheckObjectsToSent(DisplayFunctionNr);

                    for (int cntModule=0; cntModule<128; cntModule++)
                    {
                      if (AxumData.ModuleData[cntModule].Source == (SourceNr+matrix_sources.src_offset.min.source))
                      {
                        DisplayFunctionNr = (cntModule<<12) | MODULE_FUNCTION_SOURCE_PAD;
                        CheckObjectsToSent(DisplayFunctionNr);
                      }
                    }
                  }
                }
                break;
                case SOURCE_FUNCTION_GAIN:
                {
                  printf("Source gain\n");
                  if (Data[3] == SIGNED_INTEGER_DATATYPE)
                  {
                    long TempData = 0;
                    if (Data[4+Data[4]]&0x80)
                    {   //signed
                      TempData = (unsigned long)0xFFFFFFFF;
                    }

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    AxumData.SourceData[SourceNr].Gain += (float)TempData/10;
                    if (AxumData.SourceData[SourceNr].Gain < -20)
                    {
                      AxumData.SourceData[SourceNr].Gain = -20;
                    }
                    else if (AxumData.SourceData[SourceNr].Gain > 20)
                    {
                      AxumData.SourceData[SourceNr].Gain = 20;
                    }

                    unsigned int DisplayFunctionNr = 0x05000000 | ((SourceNr)<<12) | SOURCE_FUNCTION_GAIN;
                    CheckObjectsToSent(DisplayFunctionNr);

                    for (int cntModule=0; cntModule<128; cntModule++)
                    {
                      if (AxumData.ModuleData[cntModule].Source == (SourceNr+matrix_sources.src_offset.min.source))
                      {
                        DisplayFunctionNr = (cntModule<<12) | MODULE_FUNCTION_SOURCE_GAIN_LEVEL;
                        CheckObjectsToSent(DisplayFunctionNr);
                      }
                    }
                  }
                }
                break;
                case SOURCE_FUNCTION_ALERT:
                {
                  printf("Alert\n");
                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if (TempData)
                    {
                      AxumData.SourceData[SourceNr].Alert = !AxumData.SourceData[SourceNr].Alert;
                    }
                    else
                    {
                      int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                      if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                      {
                        if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                        {
                          AxumData.SourceData[SourceNr].Alert = !AxumData.SourceData[SourceNr].Alert;
                        }
                      }
                    }

                    unsigned int DisplayFunctionNr = 0x05000000 | (SourceNr<<12) | SOURCE_FUNCTION_ALERT;
                    CheckObjectsToSent(DisplayFunctionNr);

                    for (int cntModule=0; cntModule<128; cntModule++)
                    {
                      if (AxumData.ModuleData[cntModule].Source == (SourceNr+matrix_sources.src_offset.min.source))
                      {
                        DisplayFunctionNr = (cntModule<<12) | MODULE_FUNCTION_SOURCE_ALERT;
                        CheckObjectsToSent(DisplayFunctionNr);
                      }
                    }
                  }
                }
                break;
                }
              }
              break;
              case DESTINATION_FUNCTIONS:
              { //Destination
                unsigned int DestinationNr = ((SensorReceiveFunctionNumber>>12)&0xFFF);
                unsigned int FunctionNr = SensorReceiveFunctionNumber&0xFFF;

                printf(" > DestinationNr %d ", DestinationNr);
                switch (FunctionNr)
                {
                case DESTINATION_FUNCTION_LABEL:
                { //No sensor change available
                }
                break;
                case DESTINATION_FUNCTION_SOURCE:
                {
                  printf("Source\n");
                  if (Data[3] == SIGNED_INTEGER_DATATYPE)
                  {
                    long TempData = 0;
                    if (Data[5]&0x80)
                    {   //signed
                      TempData = (unsigned long)0xFFFFFFFF;
                    }

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    AxumData.DestinationData[DestinationNr].Source = (int)AdjustDestinationSource(AxumData.DestinationData[DestinationNr].Source, TempData);

                    SetAxum_DestinationSource(DestinationNr);

                    CheckObjectsToSent(SensorReceiveFunctionNumber);
                    SensorReceiveFunctionNumber = 0x06000000 | (DestinationNr<<12);

                    CheckObjectsToSent(SensorReceiveFunctionNumber+DESTINATION_FUNCTION_MONITOR_SPEAKER_LEVEL);
                    CheckObjectsToSent(SensorReceiveFunctionNumber+DESTINATION_FUNCTION_MONITOR_PHONES_LEVEL);
                    CheckObjectsToSent(SensorReceiveFunctionNumber+DESTINATION_FUNCTION_MUTE_AND_MONITOR_MUTE);
                    CheckObjectsToSent(SensorReceiveFunctionNumber+DESTINATION_FUNCTION_DIM_AND_MONITOR_DIM);
                    CheckObjectsToSent(SensorReceiveFunctionNumber+DESTINATION_FUNCTION_MONO_AND_MONITOR_MONO);
                    CheckObjectsToSent(SensorReceiveFunctionNumber+DESTINATION_FUNCTION_PHASE_AND_MONITOR_PHASE);
                    CheckObjectsToSent(SensorReceiveFunctionNumber+DESTINATION_FUNCTION_TALKBACK_1_AND_MONITOR_TALKBACK_1);
                    CheckObjectsToSent(SensorReceiveFunctionNumber+DESTINATION_FUNCTION_TALKBACK_2_AND_MONITOR_TALKBACK_2);
                    CheckObjectsToSent(SensorReceiveFunctionNumber+DESTINATION_FUNCTION_TALKBACK_3_AND_MONITOR_TALKBACK_3);
                    CheckObjectsToSent(SensorReceiveFunctionNumber+DESTINATION_FUNCTION_TALKBACK_4_AND_MONITOR_TALKBACK_4);
                    CheckObjectsToSent(SensorReceiveFunctionNumber+DESTINATION_FUNCTION_TALKBACK_5_AND_MONITOR_TALKBACK_5);
                    CheckObjectsToSent(SensorReceiveFunctionNumber+DESTINATION_FUNCTION_TALKBACK_6_AND_MONITOR_TALKBACK_6);
                    CheckObjectsToSent(SensorReceiveFunctionNumber+DESTINATION_FUNCTION_TALKBACK_7_AND_MONITOR_TALKBACK_7);
                    CheckObjectsToSent(SensorReceiveFunctionNumber+DESTINATION_FUNCTION_TALKBACK_8_AND_MONITOR_TALKBACK_8);
                    CheckObjectsToSent(SensorReceiveFunctionNumber+DESTINATION_FUNCTION_TALKBACK_9_AND_MONITOR_TALKBACK_9);
                    CheckObjectsToSent(SensorReceiveFunctionNumber+DESTINATION_FUNCTION_TALKBACK_10_AND_MONITOR_TALKBACK_10);
                    CheckObjectsToSent(SensorReceiveFunctionNumber+DESTINATION_FUNCTION_TALKBACK_11_AND_MONITOR_TALKBACK_11);
                    CheckObjectsToSent(SensorReceiveFunctionNumber+DESTINATION_FUNCTION_TALKBACK_12_AND_MONITOR_TALKBACK_12);
                    CheckObjectsToSent(SensorReceiveFunctionNumber+DESTINATION_FUNCTION_TALKBACK_13_AND_MONITOR_TALKBACK_13);
                    CheckObjectsToSent(SensorReceiveFunctionNumber+DESTINATION_FUNCTION_TALKBACK_14_AND_MONITOR_TALKBACK_14);
                    CheckObjectsToSent(SensorReceiveFunctionNumber+DESTINATION_FUNCTION_TALKBACK_15_AND_MONITOR_TALKBACK_15);
                    CheckObjectsToSent(SensorReceiveFunctionNumber+DESTINATION_FUNCTION_TALKBACK_16_AND_MONITOR_TALKBACK_16);
                  }
                }
                break;
                case DESTINATION_FUNCTION_MONITOR_SPEAKER_LEVEL:
                { //No sensor change available
                }
                break;
                case DESTINATION_FUNCTION_MONITOR_PHONES_LEVEL:
                { //No sensor change available
                }
                break;
                case DESTINATION_FUNCTION_LEVEL:
                {
                  printf("Level\n");
                  if (Data[3] == UNSIGNED_INTEGER_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    float SensorDataMinimal = OnlineNodeInformation[IndexOfSender].ObjectInformation[ObjectNr-1024].SensorDataMinimal;
                    float SensorDataMaximal = OnlineNodeInformation[IndexOfSender].ObjectInformation[ObjectNr-1024].SensorDataMaximal;

                    int Position = (TempData*1023)/(SensorDataMaximal-SensorDataMinimal);
                    AxumData.DestinationData[DestinationNr].Level = Position2dB[Position];
                    //AxumData.DestinationData[DestinationNr].Level -= AxumData.LevelReserve;

                    CheckObjectsToSent(SensorReceiveFunctionNumber);
                  }
                  else if (Data[3] == SIGNED_INTEGER_DATATYPE)
                  {
                    long TempData = 0;
                    if (Data[4+Data[4]]&0x80)
                    {   //signed
                      TempData = (unsigned long)0xFFFFFFFF;
                    }

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    AxumData.DestinationData[DestinationNr].Level += (float)TempData/10;
                    if (AxumData.DestinationData[DestinationNr].Level<-140)
                    {
                      AxumData.DestinationData[DestinationNr].Level = -140;
                    }
                    else
                    {
                      if (AxumData.DestinationData[DestinationNr].Level > 10)
                      {
                        AxumData.DestinationData[DestinationNr].Level = 10;
                      }
                    }
                    CheckObjectsToSent(SensorReceiveFunctionNumber);
                  }
                }
                break;
                case DESTINATION_FUNCTION_MUTE:
                {
                  printf("Mute\n");

                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if (TempData)
                    {
                      AxumData.DestinationData[DestinationNr].Mute = !AxumData.DestinationData[DestinationNr].Mute;
                    }
                    else
                    {
                      int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                      if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                      {
                        if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                        {
                          AxumData.DestinationData[DestinationNr].Mute = !AxumData.DestinationData[DestinationNr].Mute;
                        }
                      }
                    }

                    CheckObjectsToSent(SensorReceiveFunctionNumber);

                    unsigned int DisplayFunctionNr = 0x06000000 | (DestinationNr<<12);
                    CheckObjectsToSent(DisplayFunctionNr | DESTINATION_FUNCTION_MUTE_AND_MONITOR_MUTE);
                  }
                }
                break;
                case DESTINATION_FUNCTION_MUTE_AND_MONITOR_MUTE:
                { //No sensor change available
                }
                break;
                case DESTINATION_FUNCTION_DIM:
                {
                  printf("Dim\n");

                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if (TempData)
                    {
                      AxumData.DestinationData[DestinationNr].Dim = !AxumData.DestinationData[DestinationNr].Dim;
                    }
                    else
                    {
                      int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                      if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                      {
                        if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                        {
                          AxumData.DestinationData[DestinationNr].Dim = !AxumData.DestinationData[DestinationNr].Dim;
                        }
                      }
                    }

                    CheckObjectsToSent(SensorReceiveFunctionNumber);

                    unsigned int DisplayFunctionNr = 0x06000000 | (DestinationNr<<12);
                    CheckObjectsToSent(DisplayFunctionNr | DESTINATION_FUNCTION_DIM_AND_MONITOR_DIM);
                  }
                }
                break;
                case DESTINATION_FUNCTION_DIM_AND_MONITOR_DIM:
                { //No sensor change available
                }
                break;
                case DESTINATION_FUNCTION_MONO:
                {
                  printf("Mono\n");

                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if (TempData)
                    {
                      AxumData.DestinationData[DestinationNr].Mono = !AxumData.DestinationData[DestinationNr].Mono;
                    }
                    else
                    {
                      int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                      if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                      {
                        if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                        {
                          AxumData.DestinationData[DestinationNr].Mono = !AxumData.DestinationData[DestinationNr].Mono;
                        }
                      }
                    }

                    CheckObjectsToSent(SensorReceiveFunctionNumber);

                    unsigned int DisplayFunctionNr = 0x06000000 | (DestinationNr<<12);
                    CheckObjectsToSent(DisplayFunctionNr | DESTINATION_FUNCTION_MONO_AND_MONITOR_MONO);
                  }
                }
                break;
                case DESTINATION_FUNCTION_MONO_AND_MONITOR_MONO:
                { //No sensor change available
                }
                break;
                case DESTINATION_FUNCTION_PHASE:
                {
                  printf("Phase\n");

                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if (TempData)
                    {
                      AxumData.DestinationData[DestinationNr].Phase = !AxumData.DestinationData[DestinationNr].Phase;
                    }
                    else
                    {
                      int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                      if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                      {
                        if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                        {
                          AxumData.DestinationData[DestinationNr].Phase = !AxumData.DestinationData[DestinationNr].Phase;
                        }
                      }
                    }

                    CheckObjectsToSent(SensorReceiveFunctionNumber);

                    unsigned int DisplayFunctionNr = 0x06000000 | (DestinationNr<<12);
                    CheckObjectsToSent(DisplayFunctionNr | DESTINATION_FUNCTION_PHASE_AND_MONITOR_PHASE);
                  }
                }
                break;
                case DESTINATION_FUNCTION_PHASE_AND_MONITOR_PHASE:
                { //No sensor change available
                }
                break;
                case DESTINATION_FUNCTION_TALKBACK_1:
                case DESTINATION_FUNCTION_TALKBACK_2:
                case DESTINATION_FUNCTION_TALKBACK_3:
                case DESTINATION_FUNCTION_TALKBACK_4:
                case DESTINATION_FUNCTION_TALKBACK_5:
                case DESTINATION_FUNCTION_TALKBACK_6:
                case DESTINATION_FUNCTION_TALKBACK_7:
                case DESTINATION_FUNCTION_TALKBACK_8:
                case DESTINATION_FUNCTION_TALKBACK_9:
                case DESTINATION_FUNCTION_TALKBACK_10:
                case DESTINATION_FUNCTION_TALKBACK_11:
                case DESTINATION_FUNCTION_TALKBACK_12:
                case DESTINATION_FUNCTION_TALKBACK_13:
                case DESTINATION_FUNCTION_TALKBACK_14:
                case DESTINATION_FUNCTION_TALKBACK_15:
                case DESTINATION_FUNCTION_TALKBACK_16:
                {
                  int TalkbackNr = (FunctionNr-DESTINATION_FUNCTION_TALKBACK_1)/(DESTINATION_FUNCTION_TALKBACK_2-DESTINATION_FUNCTION_TALKBACK_1);
                  printf("Talkback %d\n", TalkbackNr);

                  if (Data[3] == STATE_DATATYPE)
                  {
                    unsigned long TempData = 0;

                    for (int cntByte=0; cntByte<Data[4]; cntByte++)
                    {
                      TempData <<= 8;
                      TempData |= Data[5+cntByte];
                    }

                    if (TempData)
                    {
                      AxumData.DestinationData[DestinationNr].Talkback[TalkbackNr] = !AxumData.DestinationData[DestinationNr].Talkback[TalkbackNr];
                    }
                    else
                    {
                      int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                      if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                      {
                        if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                        {
                          AxumData.DestinationData[DestinationNr].Talkback[TalkbackNr] = !AxumData.DestinationData[DestinationNr].Talkback[TalkbackNr];
                        }
                      }
                    }

                    int TalkbackActive = 0;
                    for (int cntTalkback=0; cntTalkback<16; cntTalkback++)
                    {
                      TalkbackActive |= AxumData.DestinationData[DestinationNr].Talkback[cntTalkback];
                    }
                    AxumData.DestinationData[DestinationNr].Dim = TalkbackActive;

                    CheckObjectsToSent(SensorReceiveFunctionNumber);

                    unsigned int FunctionNrToSent = 0x06000000 | (DestinationNr<<12);
                    CheckObjectsToSent(FunctionNrToSent | DESTINATION_FUNCTION_DIM);
                    CheckObjectsToSent(FunctionNrToSent | (DESTINATION_FUNCTION_TALKBACK_1_AND_MONITOR_TALKBACK_1 + ((DESTINATION_FUNCTION_TALKBACK_2_AND_MONITOR_TALKBACK_2-DESTINATION_FUNCTION_TALKBACK_1_AND_MONITOR_TALKBACK_1)*TalkbackNr)));
                    CheckObjectsToSent(FunctionNrToSent | DESTINATION_FUNCTION_DIM_AND_MONITOR_DIM);
                  }
                }
                break;
                }
              }
              break;
              }
            }

            switch (Data[3])
            {
            case NO_DATA_DATATYPE:
            {
              printf(" - No data\r\n");
            }
            break;
            case UNSIGNED_INTEGER_DATATYPE:
            {
              if (Data2ASCIIString(TempString,Data[3], Data[4], &Data[5]) == 0)
              {
                printf(" - unsigned int: %s\r\n", TempString);
              }
            }
            break;
            case SIGNED_INTEGER_DATATYPE:
            {
              if (Data2ASCIIString(TempString,Data[3], Data[4], &Data[5]) == 0)
              {
                printf(" - signed int: %s\r\n", TempString);
              }
            }
            break;
            case STATE_DATATYPE:
            {
              if (Data2ASCIIString(TempString,Data[3], Data[4], &Data[5]) == 0)
              {
                printf(" - State: %s\r\n", TempString);
              }
            }
            break;
            case OCTET_STRING_DATATYPE:
            {
              if (ObjectNr == 13)
              {
                printf(" - Parent: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X \r\n", Data[5], Data[6], Data[7], Data[8], Data[9], Data[10]);
              }
              else
              {
                if (Data2ASCIIString(TempString,Data[3], Data[4], &Data[5]) == 0)
                {
                  printf("- Octet string: %s\r\n", TempString);
                }
              }
            }
            break;
            case FLOAT_DATATYPE:
            {
              if (Data2ASCIIString(TempString,Data[3], Data[4], &Data[5]) == 0)
              {
                printf("- float: %s\r\n", TempString);
              }
            }
            break;
            case BIT_STRING_DATATYPE:
            {
              if (Data2ASCIIString(TempString,Data[3], Data[4], &Data[5]) == 0)
              {
                printf("- Bit string: %s\r\n", TempString);
              }
            }
            break;
            }
            OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime = OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime;
          }
        }
      }
      break;
      }
    }

  }
  else
  {
  }

  if (MessageType == MAMBANET_DEBUG_MESSAGETYPE)
  {   //Debug Message
    char TextString[256] = "Debug: ";
    strncat(TextString, (char *)Data, DataLength);
    TextString[7+DataLength] = 0;
    strcat(TextString, "\r\n");
    printf(TextString);
  }
  MessageID = 0;
  FromHardwareAddress = NULL;
}

//function used for debug, dumping packages to the stdout
void dump_block(const unsigned char *block, unsigned int length)
{
  unsigned int i,j;
  char linebuf[16*3+1];

  for (i=0; i<length; i+=16)
  {
    for (j=0; (j<16) && (j+i<length); j++)
    {
      sprintf(linebuf+3*j,"%02x ",block[i+j]);
    }
    linebuf[3*j]='\0';
    printf("%s\r\n",linebuf);
  }
}

//This function opens a network interface and set the
//interface Axum/MambaNet compatible settings:
//
//open a socket to read packets:
//AF_PACKET, SOCK_DGRAM, ETH_P_ALL
//
//ETH_P_ALL is required else we could not receive outgoing
//packets sended by other processes else we could use ETH_P_DNR
//
//example NetworkInterface = "eth0"
int SetupNetwork(char *NetworkInterface, unsigned char *LocalMACAddress)
{
  bool error = 0;
  ifreq ethreq;

  cntEthernetReceiveBufferTop = 0;
  cntEthernetReceiveBufferBottom = 0;
  cntEthernetMambaNetDecodeBuffer = 0;

  //Open a socket for PACKETs if Datagram. This means we receive
  //only the data (without the header -> in our case without ethernet headers).
  //this socket receives all protocoltypes of ethernet (ETH_P_ALL)
  //ETH_P_ALL is required else we could not receive outgoing
  //packets sended by other processes else we could use ETH_P_DNR
//  int MambaNetSocket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
  int MambaNetSocket = socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_DNR));
  if (MambaNetSocket<0)
  {
    perror("Open Mamba socket failed");
    error = 1;
  }

  // Get index of interface identified by NetworkInterface.
  strncpy(ethreq.ifr_name,NetworkInterface,IFNAMSIZ);
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
    //printf("Network hardware address: %02X:%02X:%02X:%02X:%02X:%02X\r\n", (unsigned char)ethreq.ifr_hwaddr.sa_data[0], (unsigned char)ethreq.ifr_hwaddr.sa_data[1], (unsigned char)ethreq.ifr_hwaddr.sa_data[2], (unsigned char)ethreq.ifr_hwaddr.sa_data[3], (unsigned char)ethreq.ifr_hwaddr.sa_data[4], (unsigned char)ethreq.ifr_hwaddr.sa_data[5]);
    LocalMACAddress[0] = ethreq.ifr_hwaddr.sa_data[0];
    LocalMACAddress[1] = ethreq.ifr_hwaddr.sa_data[1];
    LocalMACAddress[2] = ethreq.ifr_hwaddr.sa_data[2];
    LocalMACAddress[3] = ethreq.ifr_hwaddr.sa_data[3];
    LocalMACAddress[4] = ethreq.ifr_hwaddr.sa_data[4];
    LocalMACAddress[5] = ethreq.ifr_hwaddr.sa_data[5];
  }

//  int data_value = 1;
//  if (ioctl(MambaNetSocket, FIOASYNC, &data_value) < 0)
//  {
//      perror("FIOASYNC");
//  }

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

  /*  struct sigaction sigio_action;
      sigemptyset(&sigio_action.sa_mask);
      sigaddset(&sigio_action.sa_mask, SIGIO);
      sigio_action.sa_flags = 0;
      sigio_action.sa_handler = SIGIOSignal;
      sigaction(SIGIO, &sigio_action, NULL);*/

//  sigprocmask(SIG_UNBLOCK, &sigio_action.sa_mask, &old_sigmask);



  {
//      struct sched_param sp;
//      memset(&sp, 0, sizeof(sp));
//      sp.sched_priority = MIN(99, sched_get_priority_max(SCHED_RR));
//      sched_setscheduler(0, SCHED_RR, &sp);
//      setpriority(PRIO_PROCESS, getpid(), -20);
  }
  //If ok, return the FileDescriptor of the socket/network.
  return MambaNetSocket;
}

//Close the network port
void CloseNetwork(int NetworkFileDescriptor)
{
  if (EthernetInterfaceIndex != -1)
  {
    UnregisterMambaNetInterface(EthernetInterfaceIndex);
  }

  if (NetworkFileDescriptor >= 0)
  {
    close(NetworkFileDescriptor);
  }
}


void EthernetMambaNetMessageTransmitCallback(unsigned char *buffer, unsigned char buffersize, unsigned char hardware_address[16])
{
  //Setup the socket datastruct to transmit data.
  struct sockaddr_ll socket_address;
  socket_address.sll_family   = AF_PACKET;
  socket_address.sll_protocol = htons(ETH_P_DNR);
  socket_address.sll_ifindex  = LinuxIfIndex;
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

  if (NetworkFileDescriptor >= 0)
  {
    int CharactersSend = 0;

    while (CharactersSend < buffersize)
    {
      CharactersSend = sendto(NetworkFileDescriptor, buffer, buffersize, 0, (struct sockaddr *) &socket_address, sizeof(socket_address));
    }

    if (CharactersSend < 0)
    {
      log_write("sendto error: cannot send data.");
    }
  }
}

void EthernetMambaNetMessageReceiveCallback(unsigned long int ToAddress, unsigned long int FromAddress, unsigned char Ack, unsigned long int MessageID, unsigned int MessageType, unsigned char *Data, unsigned char DataLength, unsigned char *FromHardwareAddress)
{
  unsigned char MessageProcessed = 0;

  switch (MessageType)
  {
  case 0x0001:
  {
    int ObjectNumber;
    unsigned char Action;

    ObjectNumber = Data[0];
    ObjectNumber <<=8;
    ObjectNumber |= Data[1];
    Action  = Data[2];
    if (Action == MAMBANET_OBJECT_ACTION_GET_ACTUATOR_DATA)
    {
      if (ObjectNumber == 1)
      {
        unsigned char TransmitBuffer[64];
        unsigned char cntTransmitBuffer;

        cntTransmitBuffer = 0;
        TransmitBuffer[cntTransmitBuffer++] = (ObjectNumber>>8)&0xFF;
        TransmitBuffer[cntTransmitBuffer++] = ObjectNumber&0xFF;
        TransmitBuffer[cntTransmitBuffer++] = MAMBANET_OBJECT_ACTION_ACTUATOR_DATA_RESPONSE;
        TransmitBuffer[cntTransmitBuffer++] = OCTET_STRING_DATATYPE;
        TransmitBuffer[cntTransmitBuffer++] = 11;
        TransmitBuffer[cntTransmitBuffer++] = 'A';
        TransmitBuffer[cntTransmitBuffer++] = 'x';
        TransmitBuffer[cntTransmitBuffer++] = 'u';
        TransmitBuffer[cntTransmitBuffer++] = 'm';
        TransmitBuffer[cntTransmitBuffer++] = ' ';
        TransmitBuffer[cntTransmitBuffer++] = 'E';
        TransmitBuffer[cntTransmitBuffer++] = 'n';
        TransmitBuffer[cntTransmitBuffer++] = 'g';
        TransmitBuffer[cntTransmitBuffer++] = 'i';
        TransmitBuffer[cntTransmitBuffer++] = 'n';
        TransmitBuffer[cntTransmitBuffer++] = 'e';

        SendMambaNetMessage(FromAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, 1, TransmitBuffer, cntTransmitBuffer);

        MessageProcessed = 1;
      }
    }
  }
  break;
  }

  if (!MessageProcessed)
  {
    MambaNetMessageReceived(ToAddress, FromAddress, MessageID, MessageType, Data, DataLength, FromHardwareAddress);
  }
  Ack = 0;
}

void EthernetMambaNetAddressTableChangeCallback(MAMBANET_ADDRESS_STRUCT *AddressTable, MambaNetAddressTableStatus Status, int Index)
{
  switch (Status)
  {
  case ADDRESS_TABLE_ENTRY_ADDED:
  {
    printf("ADDRESS_TABLE_ENTRY_ADDED\n");
  }
  break;
  case ADDRESS_TABLE_ENTRY_CHANGED:
  {
    printf("ADDRESS_TABLE_ENTRY_CHANGED\n");

    if (AddressTable[Index].NodeServices & 0x80)
    {   //Address is validated
      OnlineNodeInformation[Index].MambaNetAddress                = AddressTable[Index].MambaNetAddress;
      OnlineNodeInformation[Index].ManufacturerID             = AddressTable[Index].ManufacturerID;
      OnlineNodeInformation[Index].ProductID                      = AddressTable[Index].ProductID;
      OnlineNodeInformation[Index].UniqueIDPerProduct         = AddressTable[Index].UniqueIDPerProduct;
      OnlineNodeInformation[Index].FirmwareMajorRevision  = -1;

      unsigned char TransmitBuffer[64];
      unsigned char cntTransmitBuffer;
      unsigned int ObjectNumber = 7; //Firmware major revision

      cntTransmitBuffer = 0;
      TransmitBuffer[cntTransmitBuffer++] = (ObjectNumber>>8)&0xFF;
      TransmitBuffer[cntTransmitBuffer++] = ObjectNumber&0xFF;
      TransmitBuffer[cntTransmitBuffer++] = MAMBANET_OBJECT_ACTION_GET_SENSOR_DATA;
      TransmitBuffer[cntTransmitBuffer++] = NO_DATA_DATATYPE;

      SendMambaNetMessage(AddressTable[Index].MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, 1, TransmitBuffer, cntTransmitBuffer);
      printf("Get firmware 0x%08lX\n", AddressTable[Index].MambaNetAddress);

    }
  }
  break;
  case ADDRESS_TABLE_ENTRY_TO_REMOVE:
  {
    printf("ADDRESS_TABLE_ENTRY_TO_REMOVE\n");

    if (OnlineNodeInformation[Index].SensorReceiveFunction != NULL)
    {
      delete[] OnlineNodeInformation[Index].SensorReceiveFunction;
      delete[] OnlineNodeInformation[Index].ObjectInformation;
      OnlineNodeInformation[Index].SensorReceiveFunction = NULL;
      OnlineNodeInformation[Index].ObjectInformation = NULL;
    }
  }
  break;
  case ADDRESS_TABLE_ENTRY_ACTIVATED:
  {
    printf("ADDRESS_TABLE_ENTRY_ACTIVATED\n");
  }
  break;
  case ADDRESS_TABLE_ENTRY_TIMEOUT:
  {
    printf("ADDRESS_TABLE_ENTRY_TIMEOUT\n");

    OnlineNodeInformation[Index].MambaNetAddress                = 0;
    OnlineNodeInformation[Index].ManufacturerID                 = 0;
    OnlineNodeInformation[Index].ProductID                          = 0;
    OnlineNodeInformation[Index].UniqueIDPerProduct             = 0;
    OnlineNodeInformation[Index].FirmwareMajorRevision          = -1;
    OnlineNodeInformation[Index].SlotNumberObjectNr        = -1;
    OnlineNodeInformation[Index].InputChannelCountObjectNr    = -1;
    OnlineNodeInformation[Index].OutputChannelCountObjectNr     = -1;

    OnlineNodeInformation[Index].Parent.ManufacturerID          = 0;
    OnlineNodeInformation[Index].Parent.ProductID               = 0;
    OnlineNodeInformation[Index].Parent.UniqueIDPerProduct  = 0;

    OnlineNodeInformation[Index].NumberOfCustomObjects      = 0;
    if (OnlineNodeInformation[Index].SensorReceiveFunction != NULL)
    {
      delete[] OnlineNodeInformation[Index].SensorReceiveFunction;
      delete[] OnlineNodeInformation[Index].ObjectInformation;
      OnlineNodeInformation[Index].SensorReceiveFunction = NULL;
      OnlineNodeInformation[Index].ObjectInformation = NULL;
    }
  }
  break;
  }
  printf("Index:%d ManID:0x%04X, ProdID:0x%04X, UniqID:0x%04X, MambaNetAddr:0x%08lX Services:%02X\n", Index, AddressTable[Index].ManufacturerID, AddressTable[Index].ProductID, AddressTable[Index].UniqueIDPerProduct, AddressTable[Index].MambaNetAddress, AddressTable[Index].NodeServices);
}

void Timer100HzDone(int Value)
{
  if ((cntMillisecondTimer-PreviousCount_LevelMeter)>MeterFrequency)
  {
    PreviousCount_LevelMeter = cntMillisecondTimer;
    dsp_read_buss_meters(dsp_handler, SummingdBLevel);

    //buss audio level
    for (int cntBuss=0; cntBuss<16; cntBuss++)
    {
      CheckObjectsToSent(0x01000000 | (cntBuss<<12) | BUSS_FUNCTION_AUDIO_LEVEL_LEFT);
      CheckObjectsToSent(0x01000000 | (cntBuss<<12) | BUSS_FUNCTION_AUDIO_LEVEL_RIGHT);
    }

    //monitor buss audio level
    for (int cntMonitorBuss=0; cntMonitorBuss<16; cntMonitorBuss++)
    {
      CheckObjectsToSent(0x02000000 | (cntMonitorBuss<<12) | MONITOR_BUSS_FUNCTION_AUDIO_LEVEL_LEFT);
      CheckObjectsToSent(0x02000000 | (cntMonitorBuss<<12) | MONITOR_BUSS_FUNCTION_AUDIO_LEVEL_RIGHT);
    }
  }

  if ((cntMillisecondTimer-PreviousCount_SignalDetect)>10)
  {
    float dBLevel[256];
    PreviousCount_SignalDetect = cntMillisecondTimer;
    dsp_read_module_meters(dsp_handler, dBLevel);

    for (int cntModule=0; cntModule<128; cntModule++)
    {
      int FirstChannelNr = cntModule<<1;
      unsigned int DisplayFunctionNumber;

      if ((dBLevel[FirstChannelNr]>-70) || (dBLevel[FirstChannelNr+1]>-70))
      {   //Signal
        if (!AxumData.ModuleData[cntModule].Signal)
        {
          AxumData.ModuleData[cntModule].Signal = 1;

          DisplayFunctionNumber = (cntModule<<12) | MODULE_FUNCTION_SIGNAL;
          CheckObjectsToSent(DisplayFunctionNumber);
        }

        if ((dBLevel[FirstChannelNr]>-3) || (dBLevel[FirstChannelNr+1]>-3))
        {   //Peak
          if (!AxumData.ModuleData[cntModule].Peak)
          {
            AxumData.ModuleData[cntModule].Peak = 1;
            DisplayFunctionNumber = (cntModule<<12) | MODULE_FUNCTION_PEAK;
            CheckObjectsToSent(DisplayFunctionNumber);
          }
        }
        else
        {
          if (AxumData.ModuleData[cntModule].Peak)
          {
            AxumData.ModuleData[cntModule].Peak = 0;
            DisplayFunctionNumber = (cntModule<<12) | MODULE_FUNCTION_PEAK;
            CheckObjectsToSent(DisplayFunctionNumber);
          }
        }
      }
      else
      {
        if (AxumData.ModuleData[cntModule].Signal)
        {
          AxumData.ModuleData[cntModule].Signal = 0;
          DisplayFunctionNumber = (cntModule<<12) | MODULE_FUNCTION_SIGNAL;
          CheckObjectsToSent(DisplayFunctionNumber);

          if (AxumData.ModuleData[cntModule].Peak)
          {
            AxumData.ModuleData[cntModule].Peak = 0;
            DisplayFunctionNumber = (cntModule<<12) | MODULE_FUNCTION_PEAK;
            CheckObjectsToSent(DisplayFunctionNumber);
          }
        }
      }
    }
  }

  cntMillisecondTimer++;
  if ((cntMillisecondTimer-PreviousCount_Second)>100)
  {
    PreviousCount_Second = cntMillisecondTimer;
    MambaNetReservationTimerTick();

    //Check for firmware requests
    unsigned int cntIndex=0;
    while (cntIndex<AddressTableCount)
    {
      if ((OnlineNodeInformation[cntIndex].MambaNetAddress != 0x00000000) &&
          (OnlineNodeInformation[cntIndex].FirmwareMajorRevision == -1))
      { //Read firmware...
        unsigned char TransmitBuffer[64];
        unsigned char cntTransmitBuffer;
        unsigned int ObjectNumber = 7; //Firmware major revision

        cntTransmitBuffer = 0;
        TransmitBuffer[cntTransmitBuffer++] = (ObjectNumber>>8)&0xFF;
        TransmitBuffer[cntTransmitBuffer++] = ObjectNumber&0xFF;
        TransmitBuffer[cntTransmitBuffer++] = MAMBANET_OBJECT_ACTION_GET_SENSOR_DATA;
        TransmitBuffer[cntTransmitBuffer++] = NO_DATA_DATATYPE;

        SendMambaNetMessage(OnlineNodeInformation[cntIndex].MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, 1, TransmitBuffer, cntTransmitBuffer);
        printf("timer: Get firmware 0x%08X\n", OnlineNodeInformation[cntIndex].MambaNetAddress);
      }
      cntIndex++;
    }
  }

  if (cntBroadcastPing)
  {
    if ((cntMillisecondTimer-PreviousCount_BroadcastPing)> 500)
    {
      unsigned char TransmitBuffer[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

      TransmitBuffer[0] = MAMBANET_ADDRESS_RESERVATION_TYPE_PING;
      SendMambaNetMessage(0x10000000, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, 0, TransmitBuffer, 16);

      cntBroadcastPing--;
      PreviousCount_BroadcastPing = cntMillisecondTimer;
    }
  }
  Value = 0;
}

int delay_ms(double sleep_time)
{
  struct timespec tv;

  sleep_time /= 1000;

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

float CalculateEQ(float *Coefficients, float Gain, int Frequency, float Bandwidth, float Slope, FilterType Type)
{
  double a0=1, a1=0 ,a2=0; //<- Zero coefficients
  double b0=1, b1=0 ,b2=0; //<- Pole coefficients
  unsigned char Zolzer = 1;
  unsigned int FSamplerate = AxumData.Samplerate;

  if (!Zolzer)
  {
    double A = pow(10, (double)Gain/40);
    double Omega = (2*M_PI*Frequency)/FSamplerate;
    double Cs = cos(Omega);
    double Sn = sin(Omega);
    double Q = Sn/(log(2)*Bandwidth*Omega);
    double Alpha;

    if (Type==PEAKINGEQ)
    {
      Alpha = Sn*sinhl(1/(2*Q))*2*pow(10, (double)abs(Gain)/40);
    }
    else
    {
      Alpha = Sn*sinhl(1/(2*Q));
    }

    switch (Type)
    {
    case OFF:
    {
      a0 = 1;
      a1 = 0;
      a2 = 0;

      b0 = 1;
      b1 = 0;
      b2 = 0;
    }
    break;
    case LPF:
    {// LPF
      a0 = (1 - Cs)/2;
      a1 = 1 - Cs;
      a2 = (1 - Cs)/2;

      b0 = 1 + Alpha;
      b1 = -2*Cs;
      b2 = 1 - Alpha;
    }
    break;
    case HPF:
    {// HPF
      a0 = (1 + Cs)/2;
      a1 = -1 - Cs;
      a2 = (1 + Cs)/2;

      b0 = 1 + Alpha;
      b1 = -2*Cs;
      b2 = 1 - Alpha;
    }
    break;
    case BPF:
    {// BPF
      a0 = Alpha;
      a1 = 0;
      a2 = -Alpha;

      b0 = 1 + Alpha;
      b1 = -2*Cs;
      b2 = 1 - Alpha;
    }
    break;
    case NOTCH:
    {// notch
      if (Gain<0)
      {
        a0 = 1 + pow(10, (double)Gain/20)*Sn*sinh(1/(2*Q));
        a1 = -2*Cs;
        a2 = 1 - pow(10, (double)Gain/20)*Sn*sinh(1/(2*Q));

        b0 = 1 + Sn*sinh(1/(2*Q));
        b1 = -2*Cs;
        b2 = 1 - Sn*sinh(1/(2*Q));
      }
      else
      {
        a0 = 1 + Sn*sinh(1/(2*Q));
        a1 = -2*Cs;
        a2 = 1 - Sn*sinh(1/(2*Q));

        b0 = 1 + pow(10, (double)-Gain/20)*Sn*sinh(1/(2*Q));
        b1 = -2*Cs;
        b2 = 1 - pow(10, (double)-Gain/20)*Sn*sinh(1/(2*Q));
      }
    }
    break;
    case PEAKINGEQ:
    {   //Peaking EQ
      a0 = 1 + Alpha*A;
      a1 = -2*Cs;
      a2 = 1 - Alpha*A;

      b0 = 1 + Alpha/A;
      b1 = -2*Cs;
      b2 = 1 - Alpha/A;
    }
    break;
    case LOWSHELF:
    {// lowShelf
      a0 =   A*((A+1) - ((A-1)*Cs));// + (Beta*Sn));
      a1 = 2*A*((A-1) - ((A+1)*Cs));
      a2 =   A*((A+1) - ((A-1)*Cs));// - (Beta*Sn));

      b0 =      (A+1) + ((A-1)*Cs);// + (Beta*Sn);
      b1 =    -2*((A-1) + ((A+1)*Cs));
      b2 =         (A+1) + ((A-1)*Cs);// - (Beta*Sn);
    }
    break;
    case HIGHSHELF:
    {// highShelf
      a0 =    A*((A+1) + ((A-1)*Cs));// + (Beta*Sn));
      a1 = -2*A*((A-1) + ((A+1)*Cs));
      a2 =    A*((A+1) + ((A-1)*Cs));// - (Beta*Sn));

      b0 =      (A+1) - ((A-1)*Cs);// + (Beta*Sn);
      b1 =     2*((A-1) - ((A+1)*Cs));
      b2 =         (A+1) - ((A-1)*Cs);// - (Beta*Sn);
    }
    break;
    }
  }
  else
  {
    double Omega = (2*M_PI*Frequency)/FSamplerate;
    double Cs = cos(Omega);
    double Sn = sin(Omega);
    double Q = Sn/(log(2)*Bandwidth*Omega);
    double Alpha;

    if (Type==PEAKINGEQ)
    {
      Alpha = Sn*sinhl(1/(2*Q))*2*pow(10, (double)abs(Gain)/40);
    }
    else
    {
      Alpha = Sn*sinhl(1/(2*Q));
    }

    double K = tan(Omega/2);
    switch (Type)
    {
    case OFF:
    {
      a0 = 1;
      a1 = 0;
      a2 = 0;

      b0 = 1;
      b1 = 0;
      b2 = 0;
    }
    break;
    case LPF:
    {
      a0 = (K*K)/(1+sqrt(2)*K+(K*K));
      a1 = (2*(K*K))/(1+sqrt(2)*K+(K*K));
      a2 = (K*K)/(1+sqrt(2)*K+(K*K));

      b0 = 1;
      b1 = (2*((K*K)-1))/(1+sqrt(2)*K+(K*K));
      b2 = (1-sqrt(2)*K+(K*K))/(1+sqrt(2)*K+(K*K));
    }
    break;
    case HPF:
    {// HPF
      a0 = 1/(1+sqrt(2)*K+(K*K));
      a1 = -2/(1+sqrt(2)*K+(K*K));
      a2 = 1/(1+sqrt(2)*K+(K*K));

      b0 = 1;
      b1 = (2*((K*K)-1))/(1+sqrt(2)*K+(K*K));
      b2 = (1-sqrt(2)*K+(K*K))/(1+sqrt(2)*K+(K*K));
    }
    break;
    case BPF:
    {// BPF
      a0 = Alpha;
      a1 = 0;
      a2 = -Alpha;

      b0 = 1 + Alpha;
      b1 = -2*Cs;
      b2 = 1 - Alpha;
    }
    break;
    case NOTCH:
    {// notch
      if (Gain<0)
      {
        a0 = 1+ pow(10,(double)Gain/20)*Alpha;
        a1 = -2*Cs;
        a2 = 1- pow(10,(double)Gain/20)*Alpha;

        b0 = 1 + Alpha;
        b1 = -2*Cs;
        b2 = 1 - Alpha;
      }
      else
      {
        double A = pow(10,(double)Gain/20);

        a0 = (1+((A*K)/Q)+(K*K))/(1+(K/Q)+(K*K));
        a1 = (2*((K*K)-1))/(1+(K/Q)+(K*K));
        a2 = (1-((A*K)/Q)+(K*K))/(1+(K/Q)+(K*K));

        b0 = 1;
        b1 = (2*((K*K)-1))/(1+(K/Q)+(K*K));
        b2 = (1-(K/Q)+(K*K))/(1+(K/Q)+(K*K));
      }
    }
    break;
    case PEAKINGEQ:
    {   //Peaking EQ
      if (Gain>0)
      {
        float A = pow(10,(double)Gain/20);
        a0 = (1+((A*K)/Q)+(K*K))/(1+(K/Q)+(K*K));
        a1 = (2*((K*K)-1))/(1+(K/Q)+(K*K));
        a2 = (1-((A*K)/Q)+(K*K))/(1+(K/Q)+(K*K));

        b0 = 1;
        b1 = (2*((K*K)-1))/(1+(K/Q)+(K*K));
        b2 = (1-(K/Q)+(K*K))/(1+(K/Q)+(K*K));
      }
      else
      {
        float A = pow(10,(double)-Gain/20);

        a0 = (1+(K/Q)+(K*K))/(1+((A*K)/Q)+(K*K));
        a1 = (2*((K*K)-1))/(1+((A*K)/Q)+(K*K));
        a2 = (1-(K/Q)+(K*K))/(1+((A*K)/Q)+(K*K));

        b0 = 1;
        b1 = (2*((K*K)-1))/(1+((A*K)/Q)+(K*K));
        b2 = (1-((A*K)/Q)+(K*K))/(1+((A*K)/Q)+(K*K));
      }
    }
    break;
    case LOWSHELF:
    {// lowShelf
      if (Gain>0)
      {
        double A = pow(10,(double)Gain/20);
        a0 = (1+(sqrt(2*A*Slope)*K)+(A*K*K))/(1+(sqrt(2*Slope)*K)+(K*K));
        a1 = (2*((A*K*K)-1))/(1+(sqrt(2*Slope)*K)+(K*K));
        a2 = (1-(sqrt(2*A*Slope)*K)+(A*K*K))/(1+(sqrt(2*Slope)*K)+(K*K));

        b0 = 1;
        b1 = (2*((K*K)-1))/(1+(sqrt(2*Slope)*K)+(K*K));
        b2 = (1-(sqrt(2*Slope)*K)+(K*K))/(1+(sqrt(2*Slope)*K)+(K*K));
      }
      else
      {
        double A = pow(10,(double)-Gain/20);
        a0 = (1+(sqrt(2*Slope)*K)+(K*K))/(1+(sqrt(2*A*Slope)*K)+(A*K*K));
        a1 = (2*((K*K)-1))/(1+(sqrt(2*A*Slope)*K)+(A*K*K));
        a2 = (1-(sqrt(2*Slope)*K)+(K*K))/(1+(sqrt(2*A*Slope)*K)+(A*K*K));

        b0 = 1;
        b1 = (2*((A*K*K)-1))/(1+(sqrt(2*A*Slope)*K)+(A*K*K));
        b2 = (1-(sqrt(2*A*Slope)*K)+(A*K*K))/(1+(sqrt(2*A*Slope)*K)+(A*K*K));
      }
    }
    break;
    case HIGHSHELF:
    {// highShelf
      if (Gain>0)
      {
        double A = pow(10,(double)Gain/20);
        a0 = (A+(sqrt(2*A*Slope)*K)+(K*K))/(1+(sqrt(2*Slope)*K)+(K*K));
        a1 = (2*((K*K)-A))/(1+(sqrt(2*Slope)*K)+(K*K));
        a2 = (A-(sqrt(2*A*Slope)*K)+(K*K))/(1+(sqrt(2*Slope)*K)+(K*K));

        b0 = 1;
        b1 = (2*((K*K)-1))/(1+(sqrt(2*Slope)*K)+(K*K));
        b2 = (1-(sqrt(2*Slope)*K)+(K*K))/(1+(sqrt(2*Slope)*K)+(K*K));
      }
      else
      {
        double A = pow(10,(double)-Gain/20);
        a0 = (1+(sqrt(2*Slope)*K)+(K*K))/(A+(sqrt(2*A*Slope)*K)+(K*K));
        a1 = (2*((K*K)-1))/(A+(sqrt(2*A*Slope)*K)+(K*K));
        a2 = (1-(sqrt(2*Slope)*K)+(K*K))/(A+(sqrt(2*A*Slope)*K)+(K*K));

        b0 = 1;
        b1 = (2*(((K*K)/A)-1))/(1+(sqrt((2*Slope)/A)*K)+((K*K)/A));
        b2 = (1-(sqrt((2*Slope)/A)*K)+((K*K)/A))/(1+(sqrt((2*Slope)/A)*K)+((K*K)/A));
      }
    }
    break;
    }
  }

  Coefficients[0] = a0;
  Coefficients[1] = a1;
  Coefficients[2] = a2;
  Coefficients[3] = b0;
  Coefficients[4] = b1;
  Coefficients[5] = b2;

  return a0/b0;
}

void SetBackplane_Source(unsigned int FormInputNr, unsigned int ChannelNr)
{
  printf("[SetBackplane_Source(%d,%d)]\n", FormInputNr, ChannelNr);

  int ObjectNr = 1032+ChannelNr;
  unsigned char TransmitData[32];

  TransmitData[0] = (ObjectNr>>8)&0xFF;
  TransmitData[1] = ObjectNr&0xFF;
  TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
  TransmitData[3] = UNSIGNED_INTEGER_DATATYPE;
  TransmitData[4] = 2;
  TransmitData[5] = (FormInputNr>>8)&0xFF;
  TransmitData[6] = FormInputNr&0xFF;

  if (BackplaneMambaNetAddress != 0x00000000)
  {
    printf("SendMambaNetMessage(0x%08X, 0x%08X, ...\n", BackplaneMambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress);
    SendMambaNetMessage(BackplaneMambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 7);
  }
  else
  {
    printf("BackplaneMambaNetAddress == null\n");
  }
}

void SetBackplane_Clock()
{
  int ObjectNr = 1030;
  unsigned char TransmitData[32];

  TransmitData[0] = (ObjectNr>>8)&0xFF;
  TransmitData[1] = ObjectNr&0xFF;
  TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
  TransmitData[3] = STATE_DATATYPE;
  TransmitData[4] = 1;
  TransmitData[5] = AxumData.ExternClock;

  if (BackplaneMambaNetAddress != 0x00000000)
  {
    SendMambaNetMessage(BackplaneMambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 7);
  }

  ObjectNr = 1024;
  TransmitData[0] = (ObjectNr>>8)&0xFF;
  TransmitData[1] = ObjectNr&0xFF;
  TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
  TransmitData[3] = STATE_DATATYPE;
  TransmitData[4] = 1;
  switch (AxumData.Samplerate)
  {
  case 32000:
  {
    TransmitData[5] = 0;
  }
  break;
  case 44100:
  {
    TransmitData[5] = 1;
  }
  break;
  default:
  {
    TransmitData[5] = 2;
  }
  break;
  }

  if (BackplaneMambaNetAddress != 0x00000000)
  {
    SendMambaNetMessage(BackplaneMambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 7);
  }
}

void SetAxum_EQ(unsigned char ModuleNr, unsigned char BandNr)
{
  unsigned int SystemChannelNr = ModuleNr<<1;
  unsigned char DSPCardNr = (SystemChannelNr/64);
  unsigned char DSPCardChannelNr = SystemChannelNr%64;

  DSPCARD_STRUCT *dspcard = &dsp_handler->dspcard[DSPCardNr];

  for (int cntChannel=0; cntChannel<2; cntChannel++)
  {
    dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].EQBand[BandNr].On = AxumData.ModuleData[ModuleNr].EQBand[BandNr].On;
    dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].EQBand[BandNr].Level = AxumData.ModuleData[ModuleNr].EQBand[BandNr].Level;
    dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].EQBand[BandNr].Frequency = AxumData.ModuleData[ModuleNr].EQBand[BandNr].Frequency;
    dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].EQBand[BandNr].Bandwidth = AxumData.ModuleData[ModuleNr].EQBand[BandNr].Bandwidth;
    dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].EQBand[BandNr].Slope  = AxumData.ModuleData[ModuleNr].EQBand[BandNr].Slope;
    dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].EQBand[BandNr].Type = AxumData.ModuleData[ModuleNr].EQBand[BandNr].Type;
  }

  for (int cntChannel=0; cntChannel<2; cntChannel++)
  {
    dsp_set_eq(dsp_handler, SystemChannelNr+cntChannel, BandNr);
  }
}

void SetAxum_ModuleProcessing(unsigned int ModuleNr)
{
  unsigned int SystemChannelNr = ModuleNr<<1;
  unsigned char DSPCardNr = (SystemChannelNr/64);
  unsigned char DSPCardChannelNr = SystemChannelNr%64;

  DSPCARD_STRUCT *dspcard = &dsp_handler->dspcard[DSPCardNr];

  for (int cntChannel=0; cntChannel<2; cntChannel++)
  {
    dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Insert = AxumData.ModuleData[ModuleNr].Insert;
    dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Gain = AxumData.ModuleData[ModuleNr].Gain;

    dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Filter.On = AxumData.ModuleData[ModuleNr].Filter.On;
    dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Filter.Level = AxumData.ModuleData[ModuleNr].Filter.Level;
    dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Filter.Frequency = AxumData.ModuleData[ModuleNr].Filter.Frequency;
    dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Filter.Bandwidth = AxumData.ModuleData[ModuleNr].Filter.Bandwidth;
    dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Filter.Slope = AxumData.ModuleData[ModuleNr].Filter.Slope;
    dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Filter.Type = AxumData.ModuleData[ModuleNr].Filter.Type;

    dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Dynamics.Percent = AxumData.ModuleData[ModuleNr].Dynamics;
    dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Dynamics.On = AxumData.ModuleData[ModuleNr].DynamicsOn;
  }
  dspcard->data.ChannelData[DSPCardChannelNr+1].PhaseReverse = AxumData.ModuleData[ModuleNr].PhaseReverse;


  for (int cntChannel=0; cntChannel<2; cntChannel++)
  {
    dsp_set_ch(dsp_handler, SystemChannelNr+cntChannel);
  }
}

void SetAxum_BussLevels(unsigned int ModuleNr)
{
  unsigned int SystemChannelNr = ModuleNr<<1;
  unsigned char DSPCardNr = (SystemChannelNr/64);
  unsigned char DSPCardChannelNr = SystemChannelNr%64;
  float PanoramadB[2] = {-200, -200};

  DSPCARD_STRUCT *dspcard = &dsp_handler->dspcard[DSPCardNr];

  //Panorama
  float RightPos = AxumData.ModuleData[ModuleNr].Panorama;
  float LeftPos = 1023-RightPos;
  if (LeftPos>0)
  {
    PanoramadB[0] = 20*log10(LeftPos/1023);
  }
  if (RightPos>0)
  {
    PanoramadB[1] = 20*log10(RightPos/1023);
  }
  //Balance
  if ((LeftPos>0) && (LeftPos<512))
  {
    PanoramadB[0] = 20*log10(LeftPos/512);
  }
  else if (LeftPos>=512)
  {
    PanoramadB[0] = 0;
  }

  if ((RightPos>0) && (RightPos<512))
  {
    PanoramadB[1] = 20*log10(RightPos/512);
  }
  else if (RightPos>=512)
  {
    PanoramadB[1] = 0;
  }

  //Buss 1-16
  for (int cntBuss=0; cntBuss<16; cntBuss++)
  {
    float BussBalancedB[2] = {-200, -200};

    //Buss balance
    float RightPos = AxumData.ModuleData[ModuleNr].Buss[cntBuss].Balance;
    float LeftPos = 1023-RightPos;
    if (LeftPos>0)
    {
      BussBalancedB[0] = 20*log10(LeftPos/1023);
    }
    if (RightPos>0)
    {
      BussBalancedB[1] = 20*log10(RightPos/1023);
    }
    //Balance
    if ((LeftPos>0) && (LeftPos<512))
    {
      BussBalancedB[0] = 20*log10(LeftPos/512);
    }
    else if (LeftPos>=512)
    {
      BussBalancedB[0] = 0;
    }

    if ((RightPos>0) && (RightPos<512))
    {
      BussBalancedB[1] = 20*log10(RightPos/512);
    }
    else if (RightPos>=512)
    {
      BussBalancedB[1] = 0;
    }

    for (int cntChannel=0; cntChannel<2; cntChannel++)
    {
      dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+0].Level = -140; //0 dB?
      dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+1].Level = -140; //0 dB?
      dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+0].On = 0;
      dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+1].On = 0;

      if (AxumData.ModuleData[ModuleNr].Buss[cntBuss].Active)
      {
        if (AxumData.ModuleData[ModuleNr].Mono)
        {
          dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+0].On = AxumData.ModuleData[ModuleNr].Buss[cntBuss].On;
          dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+1].On = AxumData.ModuleData[ModuleNr].Buss[cntBuss].On;
          dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+0].Level = AxumData.ModuleData[ModuleNr].Buss[cntBuss].Level;
          dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+1].Level = AxumData.ModuleData[ModuleNr].Buss[cntBuss].Level;
          dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+0].Level += BussBalancedB[0];
          dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+1].Level += BussBalancedB[1];
          dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+0].Level += -6;
          dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+1].Level += -6;

          if ((!AxumData.ModuleData[ModuleNr].Buss[cntBuss].PreModuleLevel) && (!AxumData.BussMasterData[cntBuss].PreModuleLevel))
          {
            dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+0].Level += AxumData.ModuleData[ModuleNr].FaderLevel;
            dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+1].Level += AxumData.ModuleData[ModuleNr].FaderLevel;
          }
          if (!AxumData.BussMasterData[cntBuss].PreModuleBalance)
          {
            dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+0].Level += PanoramadB[0];
            dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+1].Level += PanoramadB[1];
          }
          if (!AxumData.BussMasterData[cntBuss].PreModuleOn)
          {
            if ((!AxumData.ModuleData[ModuleNr].On) || (AxumData.ModuleData[ModuleNr].Cough))
            {
              dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+0].On = 0;
              dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+1].On = 0;
            }
          }
        }
        else
        { //Stereo
          dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+cntChannel].On = AxumData.ModuleData[ModuleNr].Buss[cntBuss].On;
          dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+cntChannel].Level = AxumData.ModuleData[ModuleNr].Buss[cntBuss].Level;
          dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+cntChannel].Level += BussBalancedB[cntChannel];

          if ((!AxumData.ModuleData[ModuleNr].Buss[cntBuss].PreModuleLevel) && (!AxumData.BussMasterData[cntBuss].PreModuleLevel))
          {
            dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+cntChannel].Level += AxumData.ModuleData[ModuleNr].FaderLevel;
          }

          if (!AxumData.BussMasterData[cntBuss].PreModuleBalance)
          {
            dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+cntChannel].Level += PanoramadB[cntChannel];
          }

          if (!AxumData.BussMasterData[cntBuss].PreModuleOn)
          {
            if ((!AxumData.ModuleData[ModuleNr].On) || (AxumData.ModuleData[ModuleNr].Cough))
            {
              dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+cntChannel].On = 0;
            }
          }
        }
      }
    }
  }

  for (int cntChannel=0; cntChannel<2; cntChannel++)
  {
    dsp_set_buss_lvl(dsp_handler, SystemChannelNr+cntChannel);
  }
}

void axum_get_mtrx_chs_from_src(unsigned int src, unsigned int *l_ch, unsigned int *r_ch)
{
  *l_ch = 0;
  *r_ch = 0;
  if (src == 0)
  {
    printf("src: None\n");
  }
  else if ((src>=matrix_sources.src_offset.min.buss) && (src<=matrix_sources.src_offset.max.buss))
  {
    int BussNr = src-matrix_sources.src_offset.min.buss;
    printf("src: Total buss %d\n", BussNr+1);
    *l_ch = 1793+(BussNr*2)+0;
    *r_ch = 1793+(BussNr*2)+1;
  }
  else if ((src>=matrix_sources.src_offset.min.insert_out) && (src<=matrix_sources.src_offset.max.insert_out))
  {
    int ModuleNr = src-matrix_sources.src_offset.min.insert_out;
    printf("src: Module insert out %d\n", ModuleNr+1);

    unsigned char DSPCardNr = (ModuleNr/32);
    if (dsp_card_available(dsp_handler, DSPCardNr))
    {
      DSPCARD_STRUCT *dspcard = &dsp_handler->dspcard[DSPCardNr];

      unsigned int FirstDSPChannelNr = 481+(dspcard->slot*5*32);

      *l_ch = FirstDSPChannelNr+(ModuleNr*2)+0;
      *r_ch = FirstDSPChannelNr+(ModuleNr*2)+1;
    }
    else
    {
      printf("src: Module insert out %d muted, no DSP Card\n", ModuleNr+1);
    }
  }
  else if ((src>=matrix_sources.src_offset.min.monitor_buss) && (src<=matrix_sources.src_offset.max.monitor_buss))
  {
    int BussNr = src-matrix_sources.src_offset.min.monitor_buss;
    printf("src: Monitor buss %d\n", BussNr+1);

    unsigned char DSPCardNr = (BussNr/4);

    if (dsp_card_available(dsp_handler, DSPCardNr))
    {
      DSPCARD_STRUCT *dspcard = &dsp_handler->dspcard[DSPCardNr];

      unsigned int FirstDSPChannelNr = 577+(dspcard->slot*5*32);

      *l_ch = FirstDSPChannelNr+(BussNr*2)+0;
      *r_ch = FirstDSPChannelNr+(BussNr*2)+1;
    }
    else
    {
      printf("src: Monitor buss %d muted, no DSP Card\n", BussNr+1);
    }
  }
  else if ((src>=matrix_sources.src_offset.min.mixminus) && (src<=matrix_sources.src_offset.max.mixminus))
  {
    int ModuleNr = src-matrix_sources.src_offset.min.mixminus;
    printf("src: N-1 out %d\n", ModuleNr+1);

    unsigned char DSPCardNr = (ModuleNr/32);
    if (dsp_card_available(dsp_handler, DSPCardNr))
    {
      DSPCARD_STRUCT *dspcard = &dsp_handler->dspcard[DSPCardNr];

      unsigned int FirstDSPChannelNr = 545+(dspcard->slot*5*32);

      //N-1 are mono's
      *l_ch = FirstDSPChannelNr+ModuleNr;
      *r_ch = FirstDSPChannelNr+ModuleNr;
    }
    else
    {
      printf("src: Module N-1 %d muted, no DSP Card\n", ModuleNr+1);
    }
  }
  else if ((src>=matrix_sources.src_offset.min.source) && (src<=matrix_sources.src_offset.max.source))
  {
    int SourceNr = src-matrix_sources.src_offset.min.source;
    printf("src: Source %d\n", SourceNr+1);

    //Get slot number from MambaNet Address
    for (int cntSlot=0; cntSlot<15; cntSlot++)
    {
      if (AxumData.RackOrganization[cntSlot] == AxumData.SourceData[SourceNr].InputData[0].MambaNetAddress)
      {
        *l_ch = cntSlot*32;
      }
      if (AxumData.RackOrganization[cntSlot] == AxumData.SourceData[SourceNr].InputData[1].MambaNetAddress)
      {
        *r_ch = cntSlot*32;
      }
    }
    for (int cntSlot=15; cntSlot<19; cntSlot++)
    {
      if (AxumData.RackOrganization[cntSlot] == AxumData.SourceData[SourceNr].InputData[0].MambaNetAddress)
      {
        *l_ch = 480+((cntSlot-15)*32*5);
      }
      if (AxumData.RackOrganization[cntSlot] == AxumData.SourceData[SourceNr].InputData[1].MambaNetAddress)
      {
        *r_ch = 480+((cntSlot-15)*32*5);
      }
    }
    for (int cntSlot=21; cntSlot<42; cntSlot++)
    {
      if (AxumData.RackOrganization[cntSlot] == AxumData.SourceData[SourceNr].InputData[0].MambaNetAddress)
      {
        *l_ch = 1120+((cntSlot-21)*32);
      }
      if (AxumData.RackOrganization[cntSlot] == AxumData.SourceData[SourceNr].InputData[0].MambaNetAddress)
      {
        *r_ch = 1120+((cntSlot-21)*32);
      }
    }
    *l_ch += AxumData.SourceData[SourceNr].InputData[0].SubChannel;
    *r_ch += AxumData.SourceData[SourceNr].InputData[1].SubChannel;

    //Because 0 = mute, add one per channel
    *l_ch += 1;
    *r_ch += 1;
  }
}

void SetAxum_ModuleSource(unsigned int ModuleNr)
{
  unsigned int SystemChannelNr = ModuleNr<<1;
  unsigned char DSPCardNr = (SystemChannelNr/64);
  unsigned int ToChannelNr;
  unsigned int Input1, Input2;

  if (dsp_card_available(dsp_handler, DSPCardNr))
  {
    DSPCARD_STRUCT *dspcard = &dsp_handler->dspcard[DSPCardNr];

    ToChannelNr = 480+(dspcard->slot*5*32)+(SystemChannelNr%64);

    axum_get_mtrx_chs_from_src(AxumData.ModuleData[ModuleNr].Source, &Input1, &Input2);

    printf("In1:%d, In2:%d\n", Input1, Input2);
    SetBackplane_Source(Input1, ToChannelNr+0);
    SetBackplane_Source(Input2, ToChannelNr+1);
  }
}

void SetAxum_ExternSources(unsigned int MonitorBussPerFourNr)
{
  unsigned char DSPCardNr = MonitorBussPerFourNr;
  if (dsp_card_available(dsp_handler, DSPCardNr))
  {
    DSPCARD_STRUCT *dspcard = &dsp_handler->dspcard[DSPCardNr];

    //four stereo busses
    for (int cntExt=0; cntExt<4; cntExt++)
    {
      unsigned int ToChannelNr = 480+(dspcard->slot*5*32)+128+(cntExt*2);
      unsigned int Input1, Input2;

      axum_get_mtrx_chs_from_src(AxumData.ExternSource[MonitorBussPerFourNr].Ext[cntExt], &Input1, &Input2);

      printf("In1:%d, In2:%d -> %d, %d\n", Input1, Input2, ToChannelNr+0, ToChannelNr+1);
      SetBackplane_Source(Input1, ToChannelNr+0);
      SetBackplane_Source(Input2, ToChannelNr+1);
    }
  }
}

void SetAxum_ModuleMixMinus(unsigned int ModuleNr, unsigned int OldSource)
{
  unsigned int SystemChannelNr = ModuleNr<<1;
  unsigned char DSPCardNr = (SystemChannelNr/64);
  unsigned char DSPCardChannelNr = SystemChannelNr%64;
  int cntDestination=0;
  int DestinationNr = -1;

  DSPCARD_STRUCT *dspcard = &dsp_handler->dspcard[DSPCardNr];

  while (cntDestination<1280)
  {
    if ((OldSource != 0) && (AxumData.DestinationData[cntDestination].MixMinusSource == OldSource))
    {
      DestinationNr = cntDestination;

      if ((OldSource>=matrix_sources.src_offset.min.source) && (OldSource<=matrix_sources.src_offset.max.source))
      {
        printf("MixMinus@%s need to be removed, use default src.\n", AxumData.DestinationData[DestinationNr].DestinationName);
        AxumData.DestinationData[DestinationNr].MixMinusActive = 0;
        SetAxum_DestinationSource(DestinationNr);
      }
    }
    if (AxumData.DestinationData[cntDestination].MixMinusSource == AxumData.ModuleData[ModuleNr].Source)
    {
      DestinationNr = cntDestination;

      if ((AxumData.ModuleData[ModuleNr].Source>=matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].Source<=matrix_sources.src_offset.max.source))
      {
        printf ("MixMinus@%s\n", AxumData.DestinationData[DestinationNr].DestinationName);

        int BussToUse = -1;
        for (int cntBuss=0; cntBuss<16; cntBuss++)
        {
          if ((AxumData.ModuleData[ModuleNr].Buss[cntBuss].Active) && (AxumData.ModuleData[ModuleNr].Buss[cntBuss].On))
          {
            if (BussToUse == -1)
            {
              BussToUse = cntBuss;
            }
          }
        }

        if (BussToUse != -1)
        {
          printf("Use buss %d, for MixMinus\n", BussToUse);

          dspcard->data.MixMinusData[DSPCardChannelNr+0].Buss = (BussToUse<<1)+0;
          dspcard->data.MixMinusData[DSPCardChannelNr+1].Buss = (BussToUse<<1)+1;

          for (int cntChannel=0; cntChannel<2; cntChannel++)
          {
            dsp_set_mixmin(dsp_handler, SystemChannelNr+cntChannel);
          }

          AxumData.DestinationData[DestinationNr].MixMinusActive = 1;
          SetAxum_DestinationSource(DestinationNr);
        }
        else
        {
          printf("No buss routed, use default src\n");

          AxumData.DestinationData[DestinationNr].MixMinusActive = 0;
          SetAxum_DestinationSource(DestinationNr);
        }
      }
    }
    cntDestination++;
  }
}

void SetAxum_ModuleInsertSource(unsigned int ModuleNr)
{
  unsigned int SystemChannelNr = ModuleNr<<1;
  unsigned char DSPCardNr = (SystemChannelNr/64);

  if (dsp_card_available(dsp_handler, DSPCardNr))
  {
    DSPCARD_STRUCT *dspcard = &dsp_handler->dspcard[DSPCardNr];

    unsigned int ToChannelNr = 480+64+(dspcard->slot*5*32)+(SystemChannelNr%64);
    unsigned int Input1, Input2;

    axum_get_mtrx_chs_from_src(AxumData.ModuleData[ModuleNr].InsertSource, &Input1, &Input2);

    printf("Insert-In1:%d, Insert-In2:%d\n", Input1, Input2);
    SetBackplane_Source(Input1, ToChannelNr+0);
    SetBackplane_Source(Input2, ToChannelNr+1);
  }
}

void SetAxum_DestinationSource(unsigned int DestinationNr)
{
  int Output1 = -1;
  int Output2 = -1;
  unsigned int FromChannel1 = 0;
  unsigned int FromChannel2 = 0;

  //Get slot number from MambaNet Address
  for (int cntSlot=0; cntSlot<15; cntSlot++)
  {
    if (AxumData.DestinationData[DestinationNr].OutputData[0].MambaNetAddress)
    {
      if (AxumData.RackOrganization[cntSlot] == AxumData.DestinationData[DestinationNr].OutputData[0].MambaNetAddress)
      {
        Output1 = cntSlot*32;
      }
    }
    if (AxumData.DestinationData[DestinationNr].OutputData[1].MambaNetAddress)
    {
      if (AxumData.RackOrganization[cntSlot] == AxumData.DestinationData[DestinationNr].OutputData[1].MambaNetAddress)
      {
        Output2 = cntSlot*32;
      }
    }
  }
  for (int cntSlot=15; cntSlot<19; cntSlot++)
  {
    if (AxumData.DestinationData[DestinationNr].OutputData[0].MambaNetAddress)
    {
      if (AxumData.RackOrganization[cntSlot] == AxumData.DestinationData[DestinationNr].OutputData[0].MambaNetAddress)
      {
        Output1 = 480+((cntSlot-15)*32*5);
      }
    }
    if (AxumData.DestinationData[DestinationNr].OutputData[1].MambaNetAddress)
    {
      if (AxumData.RackOrganization[cntSlot] == AxumData.DestinationData[DestinationNr].OutputData[1].MambaNetAddress)
      {
        Output2 = 480+((cntSlot-15)*32*5);
      }
    }
  }
  for (int cntSlot=21; cntSlot<42; cntSlot++)
  {
    if (AxumData.DestinationData[DestinationNr].OutputData[0].MambaNetAddress)
    {
      if (AxumData.RackOrganization[cntSlot] == AxumData.DestinationData[DestinationNr].OutputData[0].MambaNetAddress)
      {
        Output1 = 1120+((cntSlot-21)*32);
      }
    }
    if (AxumData.DestinationData[DestinationNr].OutputData[1].MambaNetAddress)
    {
      if (AxumData.RackOrganization[cntSlot] == AxumData.DestinationData[DestinationNr].OutputData[1].MambaNetAddress)
      {
        Output2 = 1120+((cntSlot-21)*32);
      }
    }
  }

  if (Output1 != -1)
  {
    Output1 += AxumData.DestinationData[DestinationNr].OutputData[0].SubChannel;
  }
  if (Output2 != -1)
  {
    Output2 += AxumData.DestinationData[DestinationNr].OutputData[1].SubChannel;
  }

  //N-1
  if ((Output1>-1) || (Output2>-1))
  {
    if ((AxumData.DestinationData[DestinationNr].MixMinusSource != 0) && (AxumData.DestinationData[DestinationNr].MixMinusActive))
    {
      int cntModule=0;
      int MixMinusNr = -1;
      while ((MixMinusNr == -1) && (cntModule<128))
      {
        if ((AxumData.ModuleData[cntModule].Source == AxumData.DestinationData[DestinationNr].MixMinusSource))
        {
          MixMinusNr = cntModule;
        }
        cntModule++;
      }
      printf("src: MixMinus %d\n", (MixMinusNr+1));

      unsigned char DSPCardNr = (MixMinusNr/32);

      if (dsp_card_available(dsp_handler, DSPCardNr))
      {
        DSPCARD_STRUCT *dspcard = &dsp_handler->dspcard[DSPCardNr];

        unsigned int FirstDSPChannelNr = 545+(dspcard->slot*5*32);

        FromChannel1 = FirstDSPChannelNr+(MixMinusNr&0x1F);
        FromChannel2 = FirstDSPChannelNr+(MixMinusNr&0x1F);
      }
      else
      {
        printf("src: MixMinus %d muted, no DSP card\n", (MixMinusNr+1));
        FromChannel1 = 0;
        FromChannel2 = 0;
      }
    }
    else
    {
      axum_get_mtrx_chs_from_src(AxumData.DestinationData[DestinationNr].Source, &FromChannel1, &FromChannel2);
    }

    if (Output1>-1)
    {
      printf("Out1:%d\n", Output1);
      SetBackplane_Source(FromChannel1, Output1);
    }
    if (Output2>-1)
    {
      printf("Out2:%d\n", Output2);
      SetBackplane_Source(FromChannel2, Output2);
    }
  }
}

void SetAxum_TalkbackSource(unsigned int TalkbackNr)
{
  int Output1 = 1792+(TalkbackNr*2);
  int Output2 = Output1+1;
  unsigned int FromChannel1 = 0;
  unsigned int FromChannel2 = 0;

  axum_get_mtrx_chs_from_src(AxumData.Talkback[TalkbackNr].Source, &FromChannel1, &FromChannel2);

  if (Output1>-1)
  {
    printf("Out1:%d\n", Output1);
    SetBackplane_Source(FromChannel1, Output1);
  }
  if (Output2>-1)
  {
    printf("Out2:%d\n", Output2);
    SetBackplane_Source(FromChannel2, Output2);
  }
}

void SetAxum_BussMasterLevels()
{
  for (int cntDSPCard=0; cntDSPCard<4; cntDSPCard++)
  {
    DSPCARD_STRUCT *dspcard = &dsp_handler->dspcard[cntDSPCard];

    //stereo buss 1-16
    for (int cntBuss=0; cntBuss<16; cntBuss++)
    {
      dspcard->data.BussMasterData[(cntBuss*2)+0].Level = AxumData.BussMasterData[cntBuss].Level;
      dspcard->data.BussMasterData[(cntBuss*2)+1].Level = AxumData.BussMasterData[cntBuss].Level;
      dspcard->data.BussMasterData[(cntBuss*2)+0].On = AxumData.BussMasterData[cntBuss].On;
      dspcard->data.BussMasterData[(cntBuss*2)+1].On = AxumData.BussMasterData[cntBuss].On;
    }
  }
  dsp_set_buss_mstr_lvl(dsp_handler);
}

void SetAxum_MonitorBuss(unsigned int MonitorBussNr)
{
  unsigned int MonitorChannelNr = MonitorBussNr<<1;
  unsigned char DSPCardNr = (MonitorChannelNr/8);
  unsigned char DSPCardMonitorChannelNr = MonitorChannelNr%8;

  DSPCARD_STRUCT *dspcard = &dsp_handler->dspcard[DSPCardNr];

  for (int cntMonitorInput=0; cntMonitorInput<48; cntMonitorInput++)
  {
    dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+0].Level[cntMonitorInput] = -140;
    dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+1].Level[cntMonitorInput] = -140;
  }

  //Check if an automatic switch buss is active
  char MonitorBussDimActive = 0;
  for (int cntBuss=0; cntBuss<16; cntBuss++)
  {
    if ((AxumData.Monitor[MonitorBussNr].AutoSwitchingBuss[cntBuss]) &&
        (AxumData.Monitor[MonitorBussNr].Buss[cntBuss]))
    {
      MonitorBussDimActive = 1;
    }
  }

  for (int cntBuss=0; cntBuss<16; cntBuss++)
  {
    if (AxumData.Monitor[MonitorBussNr].Buss[cntBuss])
    {
      dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+0].Level[(cntBuss*2)+0] = 0;
      dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+1].Level[(cntBuss*2)+1] = 0;

      if (MonitorBussDimActive)
      {
        if (!AxumData.Monitor[MonitorBussNr].AutoSwitchingBuss[cntBuss])
        {
          dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+0].Level[(cntBuss*2)+0] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
          dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+1].Level[(cntBuss*2)+1] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
        }
      }
    }
  }

  if (AxumData.Monitor[MonitorBussNr].Ext[0])
  {
    dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+0].Level[32] = 0;
    dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+1].Level[33] = 0;
    if (MonitorBussDimActive)
    {
      dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+0].Level[32] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
      dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+1].Level[33] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
    }
  }
  if (AxumData.Monitor[MonitorBussNr].Ext[1])
  {
    dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+0].Level[34] = 0;
    dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+1].Level[35] = 0;
    if (MonitorBussDimActive)
    {
      dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+0].Level[34] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
      dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+1].Level[35] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
    }
  }
  if (AxumData.Monitor[MonitorBussNr].Ext[2])
  {
    dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+0].Level[36] = 0;
    dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+1].Level[37] = 0;
    if (MonitorBussDimActive)
    {
      dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+0].Level[36] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
      dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+1].Level[37] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
    }
  }
  if (AxumData.Monitor[MonitorBussNr].Ext[3])
  {
    dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+0].Level[38] = 0;
    dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+1].Level[39] = 0;
    if (MonitorBussDimActive)
    {
      dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+0].Level[38] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
      dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+1].Level[39] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
    }
  }
  if (AxumData.Monitor[MonitorBussNr].Ext[4])
  {
    dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+0].Level[40] = 0;
    dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+1].Level[41] = 0;
    if (MonitorBussDimActive)
    {
      dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+0].Level[40] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
      dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+1].Level[41] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
    }
  }
  if (AxumData.Monitor[MonitorBussNr].Ext[5])
  {
    dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+0].Level[42] = 0;
    dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+1].Level[43] = 0;
    if (MonitorBussDimActive)
    {
      dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+0].Level[42] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
      dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+1].Level[43] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
    }
  }
  if (AxumData.Monitor[MonitorBussNr].Ext[6])
  {
    dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+0].Level[44] = 0;
    dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+1].Level[45] = 0;
    if (MonitorBussDimActive)
    {
      dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+0].Level[44] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
      dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+1].Level[45] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
    }
  }
  if (AxumData.Monitor[MonitorBussNr].Ext[7])
  {
    dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+0].Level[46] = 0;
    dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+1].Level[47] = 0;
    if (MonitorBussDimActive)
    {
      dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+0].Level[46] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
      dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+1].Level[47] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
    }
  }

  dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+0].MasterLevel = 0;
  dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr+1].MasterLevel = 0;

  dsp_set_monitor_buss(dsp_handler, MonitorChannelNr+0);
  dsp_set_monitor_buss(dsp_handler, MonitorChannelNr+1);
}

void CheckObjectsToSent(unsigned int SensorReceiveFunctionNumber, unsigned int MambaNetAddress)
{
  unsigned char FunctionType = (SensorReceiveFunctionNumber>>24)&0xFF;
  unsigned int FunctionNumber = (SensorReceiveFunctionNumber>>12)&0xFFF;
  unsigned int Function = SensorReceiveFunctionNumber&0xFFF;
  AXUM_FUNCTION_INFORMATION_STRUCT *WalkAxumFunctionInformationStruct = NULL;

  //Clear function list
  switch (FunctionType)
  {
  case MODULE_FUNCTIONS:
  {   //Module
    WalkAxumFunctionInformationStruct = ModuleFunctions[FunctionNumber][Function];
  }
  break;
  case BUSS_FUNCTIONS:
  {   //Buss
    WalkAxumFunctionInformationStruct = BussFunctions[FunctionNumber][Function];
  }
  break;
  case MONITOR_BUSS_FUNCTIONS:
  {   //Monitor Buss
    WalkAxumFunctionInformationStruct = MonitorBussFunctions[FunctionNumber][Function];
  }
  break;
  case GLOBAL_FUNCTIONS:
  {   //Global
    WalkAxumFunctionInformationStruct = GlobalFunctions[Function];
  }
  break;
  case SOURCE_FUNCTIONS:
  {   //Source
    WalkAxumFunctionInformationStruct = SourceFunctions[FunctionNumber][Function];
  }
  break;
  case DESTINATION_FUNCTIONS:
  {   //Destination
    WalkAxumFunctionInformationStruct = DestinationFunctions[FunctionNumber][Function];
  }
  break;
  }
  while (WalkAxumFunctionInformationStruct != NULL)
  {
    if ((MambaNetAddress == 0x00000000) || (MambaNetAddress == WalkAxumFunctionInformationStruct->MambaNetAddress))
    {
      SentDataToObject(SensorReceiveFunctionNumber, WalkAxumFunctionInformationStruct->MambaNetAddress, WalkAxumFunctionInformationStruct->ObjectNr, WalkAxumFunctionInformationStruct->ActuatorDataType, WalkAxumFunctionInformationStruct->ActuatorDataSize, WalkAxumFunctionInformationStruct->ActuatorDataMinimal, WalkAxumFunctionInformationStruct->ActuatorDataMaximal);
    }
    WalkAxumFunctionInformationStruct = (AXUM_FUNCTION_INFORMATION_STRUCT *)WalkAxumFunctionInformationStruct->Next;
  }
}

void SentDataToObject(unsigned int SensorReceiveFunctionNumber, unsigned int MambaNetAddress, unsigned int ObjectNr, unsigned char DataType, unsigned char DataSize, float DataMinimal, float DataMaximal)
{
  unsigned char SensorReceiveFunctionType = (SensorReceiveFunctionNumber>>24)&0xFF;

  switch (SensorReceiveFunctionType)
  {
  case MODULE_FUNCTIONS:
  {   //Module
    unsigned int ModuleNr = (SensorReceiveFunctionNumber>>12)&0xFFF;
    unsigned int FunctionNr = SensorReceiveFunctionNumber&0xFFF;
    unsigned int Active = 0;
    unsigned char TransmitData[32];
    char LCDText[9];

    switch (FunctionNr)
    {
    case MODULE_FUNCTION_LABEL:
    { //Label
      switch (DataType)
      {
      case OCTET_STRING_DATATYPE:
      {
        if (ModuleNr<9)
        {
          sprintf(LCDText, " Mod %d  ", ModuleNr+1);
        }
        else if (ModuleNr<99)
        {
          sprintf(LCDText, " Mod %d ", ModuleNr+1);
        }
        else
        {
          sprintf(LCDText, "Mod %d ", ModuleNr+1);
        }

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = OCTET_STRING_DATATYPE;
        TransmitData[4] = 8;
        TransmitData[5] = LCDText[0];
        TransmitData[6] = LCDText[1];
        TransmitData[7] = LCDText[2];
        TransmitData[8] = LCDText[3];
        TransmitData[9] = LCDText[4];
        TransmitData[10] = LCDText[5];
        TransmitData[11] = LCDText[6];
        TransmitData[12] = LCDText[7];

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 13);
      }
      break;
      }
    }
    break;
    case MODULE_FUNCTION_SOURCE:
    { //Source
      switch (DataType)
      {
      case OCTET_STRING_DATATYPE:
      {
        GetSourceLabel(AxumData.ModuleData[ModuleNr].Source, LCDText, 8);

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = OCTET_STRING_DATATYPE;
        TransmitData[4] = 8;
        TransmitData[5] = LCDText[0];
        TransmitData[6] = LCDText[1];
        TransmitData[7] = LCDText[2];
        TransmitData[8] = LCDText[3];
        TransmitData[9] = LCDText[4];
        TransmitData[10] = LCDText[5];
        TransmitData[11] = LCDText[6];
        TransmitData[12] = LCDText[7];

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 13);
      }
      break;
      }
    }
    break;
    case MODULE_FUNCTION_SOURCE_A: //Source A
    case MODULE_FUNCTION_SOURCE_B: //Source B
    { //Not implemented
      switch (DataType)
      {
      case STATE_DATATYPE:
      {
        int Active = 0;
        switch (FunctionNr)
        {
        case MODULE_FUNCTION_SOURCE_A: //Source A
        {
          if (AxumData.ModuleData[ModuleNr].SourceA != 0)
          {
            if (AxumData.ModuleData[ModuleNr].Source == AxumData.ModuleData[ModuleNr].SourceA)
            {
              Active = 1;
            }
          }
        }
        break;
        case MODULE_FUNCTION_SOURCE_B: //Source B
        {
          if (AxumData.ModuleData[ModuleNr].SourceB != 0)
          {
            if (AxumData.ModuleData[ModuleNr].Source == AxumData.ModuleData[ModuleNr].SourceB)
            {
              Active = 1;
            }
          }
        }
        break;
        case MODULE_FUNCTION_SOURCE_C: //Source C
        {
          if (AxumData.ModuleData[ModuleNr].SourceC != 0)
          {
            if (AxumData.ModuleData[ModuleNr].Source == AxumData.ModuleData[ModuleNr].SourceC)
            {
              Active = 1;
            }
          }
        }
        break;
        case MODULE_FUNCTION_SOURCE_D: //Source D
        {
          if (AxumData.ModuleData[ModuleNr].SourceD != 0)
          {
            if (AxumData.ModuleData[ModuleNr].Source == AxumData.ModuleData[ModuleNr].SourceD)
            {
              Active = 1;
            }
          }
        }
        break;
        }

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = STATE_DATATYPE;
        TransmitData[4] = 1;
        TransmitData[5] = Active;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
      }
      break;
      }
    }
    break;
    case MODULE_FUNCTION_SOURCE_PHANTOM:
    {
      switch (DataType)
      {
        case STATE_DATATYPE:
        {
          if ((AxumData.ModuleData[ModuleNr].Source >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].Source <= matrix_sources.src_offset.max.source))
          {
            int SourceNr = AxumData.ModuleData[ModuleNr].Source-matrix_sources.src_offset.min.source;

            TransmitData[0] = (ObjectNr>>8)&0xFF;
            TransmitData[1] = ObjectNr&0xFF;
            TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
            TransmitData[3] = STATE_DATATYPE;
            TransmitData[4] = 1;
            TransmitData[5] = 0;
            if (AxumData.ModuleData[ModuleNr].Source != 0)
            {
              TransmitData[5] = AxumData.SourceData[SourceNr].Phantom&0xFF;
            }

            SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
          }
        }
        break;
      }
    }
    break;
    case MODULE_FUNCTION_SOURCE_PAD:
    {
      switch (DataType)
      {
        case STATE_DATATYPE:
        {
          if ((AxumData.ModuleData[ModuleNr].Source >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].Source <= matrix_sources.src_offset.max.source))
          {
            int SourceNr = AxumData.ModuleData[ModuleNr].Source-matrix_sources.src_offset.min.source;

            TransmitData[0] = (ObjectNr>>8)&0xFF;
            TransmitData[1] = ObjectNr&0xFF;
            TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
            TransmitData[3] = STATE_DATATYPE;
            TransmitData[4] = 1;
            TransmitData[5] = 0;
            if (AxumData.ModuleData[ModuleNr].Source != 0)
            {
              TransmitData[5] = AxumData.SourceData[SourceNr].Pad&0xFF;
            }

            SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
          }
        }
        break;
      }
    }
    break;
    case MODULE_FUNCTION_SOURCE_GAIN_LEVEL:
    {
      switch (DataType)
      {
        case OCTET_STRING_DATATYPE:
        {
          if ((AxumData.ModuleData[ModuleNr].Source >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].Source <= matrix_sources.src_offset.max.source))
          {
            int SourceNr = AxumData.ModuleData[ModuleNr].Source-matrix_sources.src_offset.min.source;
            sprintf(LCDText,     "%5.1fdB", AxumData.SourceData[SourceNr].Gain);
          }
          else
          {
            sprintf(LCDText, "  - dB  ");
          }
          TransmitData[0] = (ObjectNr>>8)&0xFF;
          TransmitData[1] = ObjectNr&0xFF;
          TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
          TransmitData[3] = OCTET_STRING_DATATYPE;
          TransmitData[4] = 8;
          TransmitData[5] = LCDText[0];
          TransmitData[6] = LCDText[1];
          TransmitData[7] = LCDText[2];
          TransmitData[8] = LCDText[3];
          TransmitData[9] = LCDText[4];
          TransmitData[10] = LCDText[5];
          TransmitData[11] = LCDText[6];
          TransmitData[12] = LCDText[7];

          SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 13);
        }
        break;
      }
    }
    break;
    case MODULE_FUNCTION_SOURCE_GAIN_LEVEL_RESET:
    {
    }
    break;
    case MODULE_FUNCTION_INSERT_ON_OFF:
    { //Insert on/off
      switch (DataType)
      {
      case STATE_DATATYPE:
      {
        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = STATE_DATATYPE;
        TransmitData[4] = 1;
        TransmitData[5] = AxumData.ModuleData[ModuleNr].Insert&0xFF;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
      }
      break;
      }
    }
    break;
    case MODULE_FUNCTION_PHASE: //Phase
    {
      switch (DataType)
      {
      case STATE_DATATYPE:
      {
        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = STATE_DATATYPE;
        TransmitData[4] = 1;
        TransmitData[5] = AxumData.ModuleData[ModuleNr].PhaseReverse&0xFF;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
      }
      break;
      }
    }
    break;
    case MODULE_FUNCTION_GAIN_LEVEL: //Gain level
    {
      switch (DataType)
      {
      case UNSIGNED_INTEGER_DATATYPE:
      {
        int Position = ((AxumData.ModuleData[ModuleNr].Gain+20)*1023)/40;

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = UNSIGNED_INTEGER_DATATYPE;
        TransmitData[4] = 2;
        TransmitData[5] = (Position>>8)&0xFF;
        TransmitData[6] = Position&0xFF;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 7);
      }
      break;
      case OCTET_STRING_DATATYPE:
      {
        sprintf(LCDText,     "%5.1fdB", AxumData.ModuleData[ModuleNr].Gain);

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = OCTET_STRING_DATATYPE;
        TransmitData[4] = 8;
        TransmitData[5] = LCDText[0];
        TransmitData[6] = LCDText[1];
        TransmitData[7] = LCDText[2];
        TransmitData[8] = LCDText[3];
        TransmitData[9] = LCDText[4];
        TransmitData[10] = LCDText[5];
        TransmitData[11] = LCDText[6];
        TransmitData[12] = LCDText[7];

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 13);
      }
      break;
      }
    }
    break;
    case MODULE_FUNCTION_GAIN_LEVEL_RESET:  //Gain level reset
    {
    }
    break;
    case MODULE_FUNCTION_LOW_CUT_FREQUENCY: //Low cut frequency
    {
      switch (DataType)
      {
      case OCTET_STRING_DATATYPE:
      {
        if (!AxumData.ModuleData[ModuleNr].Filter.On)
        {
          sprintf(LCDText, "  Off  ");
        }
        else
        {
          sprintf(LCDText, "%5dHz", AxumData.ModuleData[ModuleNr].Filter.Frequency);
        }

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = OCTET_STRING_DATATYPE;
        TransmitData[4] = 8;
        TransmitData[5] = LCDText[0];
        TransmitData[6] = LCDText[1];
        TransmitData[7] = LCDText[2];
        TransmitData[8] = LCDText[3];
        TransmitData[9] = LCDText[4];
        TransmitData[10] = LCDText[5];
        TransmitData[11] = LCDText[6];
        TransmitData[12] = LCDText[7];

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 13);
      }
      break;
      }
    }
    break;
    case MODULE_FUNCTION_LOW_CUT_ON_OFF:  //Low cut on/off
    {
      switch (DataType)
      {
      case STATE_DATATYPE:
      {
        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = STATE_DATATYPE;
        TransmitData[4] = 1;
        TransmitData[5] = AxumData.ModuleData[ModuleNr].Filter.On&0xFF;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
      }
      break;
      }
    }
    break;
    case MODULE_FUNCTION_EQ_BAND_1_LEVEL: //EQ level
    case MODULE_FUNCTION_EQ_BAND_2_LEVEL:
    case MODULE_FUNCTION_EQ_BAND_3_LEVEL:
    case MODULE_FUNCTION_EQ_BAND_4_LEVEL:
    case MODULE_FUNCTION_EQ_BAND_5_LEVEL:
    case MODULE_FUNCTION_EQ_BAND_6_LEVEL:
    {
      int BandNr = (FunctionNr-MODULE_FUNCTION_EQ_BAND_1_LEVEL)/(MODULE_FUNCTION_EQ_BAND_2_LEVEL-MODULE_FUNCTION_EQ_BAND_1_LEVEL);
      switch (DataType)
      {
      case OCTET_STRING_DATATYPE:
      {
        sprintf(LCDText, "%5.1fdB", AxumData.ModuleData[ModuleNr].EQBand[BandNr].Level);

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = OCTET_STRING_DATATYPE;
        TransmitData[4] = 8;
        TransmitData[5] = LCDText[0];
        TransmitData[6] = LCDText[1];
        TransmitData[7] = LCDText[2];
        TransmitData[8] = LCDText[3];
        TransmitData[9] = LCDText[4];
        TransmitData[10] = LCDText[5];
        TransmitData[11] = LCDText[6];
        TransmitData[12] = LCDText[7];

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 13);
      }
      break;
      }
    }
    break;
    case MODULE_FUNCTION_EQ_BAND_1_FREQUENCY: //EQ frequency
    case MODULE_FUNCTION_EQ_BAND_2_FREQUENCY:
    case MODULE_FUNCTION_EQ_BAND_3_FREQUENCY:
    case MODULE_FUNCTION_EQ_BAND_4_FREQUENCY:
    case MODULE_FUNCTION_EQ_BAND_5_FREQUENCY:
    case MODULE_FUNCTION_EQ_BAND_6_FREQUENCY:
    {
      int BandNr = (FunctionNr-MODULE_FUNCTION_EQ_BAND_1_FREQUENCY)/(MODULE_FUNCTION_EQ_BAND_2_FREQUENCY-MODULE_FUNCTION_EQ_BAND_1_FREQUENCY);

      switch (DataType)
      {
      case OCTET_STRING_DATATYPE:
      {
        sprintf(LCDText, "%5dHz", AxumData.ModuleData[ModuleNr].EQBand[BandNr].Frequency);

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = OCTET_STRING_DATATYPE;
        TransmitData[4] = 8;
        TransmitData[5] = LCDText[0];
        TransmitData[6] = LCDText[1];
        TransmitData[7] = LCDText[2];
        TransmitData[8] = LCDText[3];
        TransmitData[9] = LCDText[4];
        TransmitData[10] = LCDText[5];
        TransmitData[11] = LCDText[6];
        TransmitData[12] = LCDText[7];

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 13);
      }
      break;
      }
    }
    break;
    case MODULE_FUNCTION_EQ_BAND_1_BANDWIDTH: //EQ bandwidth
    case MODULE_FUNCTION_EQ_BAND_2_BANDWIDTH:
    case MODULE_FUNCTION_EQ_BAND_3_BANDWIDTH:
    case MODULE_FUNCTION_EQ_BAND_4_BANDWIDTH:
    case MODULE_FUNCTION_EQ_BAND_5_BANDWIDTH:
    case MODULE_FUNCTION_EQ_BAND_6_BANDWIDTH:
    {
      int BandNr = (FunctionNr-MODULE_FUNCTION_EQ_BAND_1_BANDWIDTH)/(MODULE_FUNCTION_EQ_BAND_2_BANDWIDTH-MODULE_FUNCTION_EQ_BAND_1_BANDWIDTH);

      switch (DataType)
      {
      case OCTET_STRING_DATATYPE:
      {
        sprintf(LCDText, "%5.1f Q", AxumData.ModuleData[ModuleNr].EQBand[BandNr].Bandwidth);

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = OCTET_STRING_DATATYPE;
        TransmitData[4] = 8;
        TransmitData[5] = LCDText[0];
        TransmitData[6] = LCDText[1];
        TransmitData[7] = LCDText[2];
        TransmitData[8] = LCDText[3];
        TransmitData[9] = LCDText[4];
        TransmitData[10] = LCDText[5];
        TransmitData[11] = LCDText[6];
        TransmitData[12] = LCDText[7];

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 13);
      }
      break;
      }
    }
    break;
    case MODULE_FUNCTION_EQ_BAND_1_LEVEL_RESET: //EQ level reset
    case MODULE_FUNCTION_EQ_BAND_2_LEVEL_RESET:
    case MODULE_FUNCTION_EQ_BAND_3_LEVEL_RESET:
    case MODULE_FUNCTION_EQ_BAND_4_LEVEL_RESET:
    case MODULE_FUNCTION_EQ_BAND_5_LEVEL_RESET:
    case MODULE_FUNCTION_EQ_BAND_6_LEVEL_RESET:
    {
    }
    break;
    case MODULE_FUNCTION_EQ_BAND_1_FREQUENCY_RESET: //EQ frequency reset
    case MODULE_FUNCTION_EQ_BAND_2_FREQUENCY_RESET:
    case MODULE_FUNCTION_EQ_BAND_3_FREQUENCY_RESET:
    case MODULE_FUNCTION_EQ_BAND_4_FREQUENCY_RESET:
    case MODULE_FUNCTION_EQ_BAND_5_FREQUENCY_RESET:
    case MODULE_FUNCTION_EQ_BAND_6_FREQUENCY_RESET:
    {
    }
    break;
    case MODULE_FUNCTION_EQ_BAND_1_BANDWIDTH_RESET: //EQ bandwidth reset
    case MODULE_FUNCTION_EQ_BAND_2_BANDWIDTH_RESET:
    case MODULE_FUNCTION_EQ_BAND_3_BANDWIDTH_RESET:
    case MODULE_FUNCTION_EQ_BAND_4_BANDWIDTH_RESET:
    case MODULE_FUNCTION_EQ_BAND_5_BANDWIDTH_RESET:
    case MODULE_FUNCTION_EQ_BAND_6_BANDWIDTH_RESET:
    {
    }
    break;
    case MODULE_FUNCTION_EQ_BAND_1_TYPE:  //EQ type
    case MODULE_FUNCTION_EQ_BAND_2_TYPE:
    case MODULE_FUNCTION_EQ_BAND_3_TYPE:
    case MODULE_FUNCTION_EQ_BAND_4_TYPE:
    case MODULE_FUNCTION_EQ_BAND_5_TYPE:
    case MODULE_FUNCTION_EQ_BAND_6_TYPE:
    {
      int BandNr = (FunctionNr-MODULE_FUNCTION_EQ_BAND_1_TYPE)/(MODULE_FUNCTION_EQ_BAND_2_TYPE-MODULE_FUNCTION_EQ_BAND_1_TYPE);
      switch (DataType)
      {
      case OCTET_STRING_DATATYPE:
      {
        switch (AxumData.ModuleData[ModuleNr].EQBand[BandNr].Type)
        {
        case OFF:
        {
          sprintf(LCDText, "  Off   ");
        }
        break;
        case HPF:
        {
          sprintf(LCDText, "HighPass");
        }
        break;
        case LOWSHELF:
        {
          sprintf(LCDText, "LowShelf");
        }
        break;
        case PEAKINGEQ:
        {
          sprintf(LCDText, "Peaking ");
        }
        break;
        case HIGHSHELF:
        {
          sprintf(LCDText, "Hi Shelf");
        }
        break;
        case LPF:
        {
          sprintf(LCDText, "Low Pass");
        }
        break;
        case BPF:
        {
          sprintf(LCDText, "BandPass");
        }
        break;
        case NOTCH:
        {
          sprintf(LCDText, "  Notch ");
        }
        break;
        }

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = OCTET_STRING_DATATYPE;
        TransmitData[4] = 8;
        TransmitData[5] = LCDText[0];
        TransmitData[6] = LCDText[1];
        TransmitData[7] = LCDText[2];
        TransmitData[8] = LCDText[3];
        TransmitData[9] = LCDText[4];
        TransmitData[10] = LCDText[5];
        TransmitData[11] = LCDText[6];
        TransmitData[12] = LCDText[7];

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 13);
      }
      break;
      }
    }
    break;
    case MODULE_FUNCTION_EQ_ON_OFF:
    { //EQ on/off
      switch (DataType)
      {
      case STATE_DATATYPE:
      {
        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = STATE_DATATYPE;
        TransmitData[4] = 1;
        TransmitData[5] = AxumData.ModuleData[ModuleNr].EQOn&0xFF;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
      }
      break;
      }
    }
    break;
//        case MODULE_FUNCTION_EQ_TYPE_A:
//        case MODULE_FUNCTION_EQ_TYPE_B:
//        {
//        }
//        break;
    case MODULE_FUNCTION_DYNAMICS_AMOUNT:
    { //Dynamics amount
      switch (DataType)
      {
      case OCTET_STRING_DATATYPE:
      {
        sprintf(LCDText, "  %3d%%  ", AxumData.ModuleData[ModuleNr].Dynamics);

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = OCTET_STRING_DATATYPE;
        TransmitData[4] = 8;
        TransmitData[5] = LCDText[0];
        TransmitData[6] = LCDText[1];
        TransmitData[7] = LCDText[2];
        TransmitData[8] = LCDText[3];
        TransmitData[9] = LCDText[4];
        TransmitData[10] = LCDText[5];
        TransmitData[11] = LCDText[6];
        TransmitData[12] = LCDText[7];

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 13);
      }
      break;
      }
    }
    break;
    case MODULE_FUNCTION_DYNAMICS_ON_OFF:
    { //Dynamics on/off
      switch (DataType)
      {
      case STATE_DATATYPE:
      {
        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = STATE_DATATYPE;
        TransmitData[4] = 1;
        TransmitData[5] = AxumData.ModuleData[ModuleNr].DynamicsOn&0xFF;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
      }
      break;
      }
    }
    break;
    case MODULE_FUNCTION_MONO:
    { //Mono
      switch (DataType)
      {
      case STATE_DATATYPE:
      {
        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = STATE_DATATYPE;
        TransmitData[4] = 1;
        TransmitData[5] = AxumData.ModuleData[ModuleNr].Mono&0xFF;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
      }
      break;
      }
    }
    break;
    case MODULE_FUNCTION_PAN:
    { //Pan
      switch (DataType)
      {
      case OCTET_STRING_DATATYPE:
      {
        unsigned char Types[4] = {'[','|','|',']'};
        unsigned char Pos = AxumData.ModuleData[ModuleNr].Panorama/128;
        unsigned char Type = (AxumData.ModuleData[ModuleNr].Panorama%128)/32;

        sprintf(LCDText, "        ");
        if (AxumData.ModuleData[ModuleNr].Panorama == 0)
        {
          sprintf(LCDText, "Left    ");
        }
        else if ((AxumData.ModuleData[ModuleNr].Panorama == 511) || (AxumData.ModuleData[ModuleNr].Panorama == 512))
        {
          sprintf(LCDText, " Center ");
        }
        else if (AxumData.ModuleData[ModuleNr].Panorama == 1023)
        {
          sprintf(LCDText, "   Right");
        }
        else
        {
          LCDText[Pos] = Types[Type];
        }

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = OCTET_STRING_DATATYPE;
        TransmitData[4] = 8;
        TransmitData[5] = LCDText[0];
        TransmitData[6] = LCDText[1];
        TransmitData[7] = LCDText[2];
        TransmitData[8] = LCDText[3];
        TransmitData[9] = LCDText[4];
        TransmitData[10] = LCDText[5];
        TransmitData[11] = LCDText[6];
        TransmitData[12] = LCDText[7];

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 13);
      }
      break;
      }
    }
    break;
    case MODULE_FUNCTION_PAN_RESET:
    { //Pan reset
    }
    break;
    case MODULE_FUNCTION_MODULE_LEVEL:
    { //Module level
      switch (DataType)
      {
      case UNSIGNED_INTEGER_DATATYPE:
      {
        int dB = 0;

        dB = ((AxumData.ModuleData[ModuleNr].FaderLevel+AxumData.LevelReserve)*10)+1400;

        if (dB<0)
        {
          dB = 0;
        }
        else if (dB>=1500)
        {
          dB = 1499;
        }
        int Position = dB2Position[dB];
        Position = ((dB2Position[dB]*(DataMaximal-DataMinimal))/1023)+DataMinimal;
        if (Position<DataMinimal)
        {
          Position = DataMinimal;
        }
        else if (Position>DataMaximal)
        {
          Position = DataMaximal;
        }

        int cntTransmitData = 0;
        TransmitData[cntTransmitData++] = (ObjectNr>>8)&0xFF;
        TransmitData[cntTransmitData++] = ObjectNr&0xFF;
        TransmitData[cntTransmitData++] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[cntTransmitData++] = UNSIGNED_INTEGER_DATATYPE;
        TransmitData[cntTransmitData++] = DataSize;
        for (int cntByte=0; cntByte<DataSize; cntByte++)
        {
          char BitsToShift = ((DataSize-1)-cntByte)*8;
          TransmitData[cntTransmitData++] = (Position>>BitsToShift)&0xFF;
        }
        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, cntTransmitData);
      }
      break;
      case OCTET_STRING_DATATYPE:
      {
        sprintf(LCDText, " %4.0f dB", AxumData.ModuleData[ModuleNr].FaderLevel);

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = OCTET_STRING_DATATYPE;
        TransmitData[4] = 8;
        TransmitData[5] = LCDText[0];
        TransmitData[6] = LCDText[1];
        TransmitData[7] = LCDText[2];
        TransmitData[8] = LCDText[3];
        TransmitData[9] = LCDText[4];
        TransmitData[10] = LCDText[5];
        TransmitData[11] = LCDText[6];
        TransmitData[12] = LCDText[7];

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 13);
      }
      break;
      }
    }
    break;
    case MODULE_FUNCTION_MODULE_ON:
    { //Module on
      switch (DataType)
      {
      case STATE_DATATYPE:
      {
        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = STATE_DATATYPE;
        TransmitData[4] = 1;
        TransmitData[5] = AxumData.ModuleData[ModuleNr].On&0xFF;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
      }
      break;
      }
    }
    break;
    case MODULE_FUNCTION_MODULE_OFF:
    { //Module off
      switch (DataType)
      {
      case STATE_DATATYPE:
      {
        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = STATE_DATATYPE;
        TransmitData[4] = 1;
        TransmitData[5] = !AxumData.ModuleData[ModuleNr].On;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
      }
      break;
      }
    }
    break;
    case MODULE_FUNCTION_MODULE_ON_OFF:
    { //Module on/off
      switch (DataType)
      {
      case STATE_DATATYPE:
      {
        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = STATE_DATATYPE;
        TransmitData[4] = 1;
        TransmitData[5] = AxumData.ModuleData[ModuleNr].On&0xFF;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
      }
      break;
      }
    }
    break;
    case MODULE_FUNCTION_BUSS_1_2_LEVEL:
    case MODULE_FUNCTION_BUSS_3_4_LEVEL:
    case MODULE_FUNCTION_BUSS_5_6_LEVEL:
    case MODULE_FUNCTION_BUSS_7_8_LEVEL:
    case MODULE_FUNCTION_BUSS_9_10_LEVEL:
    case MODULE_FUNCTION_BUSS_11_12_LEVEL:
    case MODULE_FUNCTION_BUSS_13_14_LEVEL:
    case MODULE_FUNCTION_BUSS_15_16_LEVEL:
    case MODULE_FUNCTION_BUSS_17_18_LEVEL:
    case MODULE_FUNCTION_BUSS_19_20_LEVEL:
    case MODULE_FUNCTION_BUSS_21_22_LEVEL:
    case MODULE_FUNCTION_BUSS_23_24_LEVEL:
    case MODULE_FUNCTION_BUSS_25_26_LEVEL:
    case MODULE_FUNCTION_BUSS_27_28_LEVEL:
    case MODULE_FUNCTION_BUSS_29_30_LEVEL:
    case MODULE_FUNCTION_BUSS_31_32_LEVEL:
    { //Aux Level
      int BussNr = (FunctionNr-MODULE_FUNCTION_BUSS_1_2_LEVEL)/(MODULE_FUNCTION_BUSS_3_4_LEVEL-MODULE_FUNCTION_BUSS_1_2_LEVEL);
      switch (DataType)
      {
      case UNSIGNED_INTEGER_DATATYPE:
      {
        int dB = 0;
        dB = ((AxumData.ModuleData[ModuleNr].Buss[BussNr].Level+AxumData.LevelReserve)*10)+1400;

        if (dB<0)
        {
          dB = 0;
        }
        else if (dB>=1500)
        {
          dB = 1499;
        }
        int Position = dB2Position[dB];
        Position = ((dB2Position[dB]*(DataMaximal-DataMinimal))/1023)+DataMinimal;
        if (Position<DataMinimal)
        {
          Position = DataMinimal;
        }
        else if (Position>DataMaximal)
        {
          Position = DataMaximal;
        }

        int cntTransmitData = 0;
        TransmitData[cntTransmitData++] = (ObjectNr>>8)&0xFF;
        TransmitData[cntTransmitData++] = ObjectNr&0xFF;
        TransmitData[cntTransmitData++] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[cntTransmitData++] = UNSIGNED_INTEGER_DATATYPE;
        TransmitData[cntTransmitData++] = DataSize;
        for (int cntByte=0; cntByte<DataSize; cntByte++)
        {
          char BitsToShift = ((DataSize-1)-cntByte)*8;
          TransmitData[cntTransmitData++] = (Position>>BitsToShift)&0xFF;
        }
        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, cntTransmitData);
      }
      break;
      case OCTET_STRING_DATATYPE:
      {
        if (AxumData.ModuleData[ModuleNr].Buss[BussNr].On)
        {
          sprintf(LCDText, " %4.0f dB", AxumData.ModuleData[ModuleNr].Buss[BussNr].Level);
        }
        else
        {
          sprintf(LCDText, "  Off   ");
        }

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = OCTET_STRING_DATATYPE;
        TransmitData[4] = 8;
        TransmitData[5] = LCDText[0];
        TransmitData[6] = LCDText[1];
        TransmitData[7] = LCDText[2];
        TransmitData[8] = LCDText[3];
        TransmitData[9] = LCDText[4];
        TransmitData[10] = LCDText[5];
        TransmitData[11] = LCDText[6];
        TransmitData[12] = LCDText[7];

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 13);
      }
      break;
      }
    }
    break;
    case MODULE_FUNCTION_BUSS_1_2_LEVEL_RESET:
    case MODULE_FUNCTION_BUSS_3_4_LEVEL_RESET:
    case MODULE_FUNCTION_BUSS_5_6_LEVEL_RESET:
    case MODULE_FUNCTION_BUSS_7_8_LEVEL_RESET:
    case MODULE_FUNCTION_BUSS_9_10_LEVEL_RESET:
    case MODULE_FUNCTION_BUSS_11_12_LEVEL_RESET:
    case MODULE_FUNCTION_BUSS_13_14_LEVEL_RESET:
    case MODULE_FUNCTION_BUSS_15_16_LEVEL_RESET:
    case MODULE_FUNCTION_BUSS_17_18_LEVEL_RESET:
    case MODULE_FUNCTION_BUSS_19_20_LEVEL_RESET:
    case MODULE_FUNCTION_BUSS_21_22_LEVEL_RESET:
    case MODULE_FUNCTION_BUSS_23_24_LEVEL_RESET:
    case MODULE_FUNCTION_BUSS_25_26_LEVEL_RESET:
    case MODULE_FUNCTION_BUSS_27_28_LEVEL_RESET:
    case MODULE_FUNCTION_BUSS_29_30_LEVEL_RESET:
    case MODULE_FUNCTION_BUSS_31_32_LEVEL_RESET:
    { //Aux Level reset
    }
    break;
    case MODULE_FUNCTION_BUSS_1_2_ON_OFF:
    case MODULE_FUNCTION_BUSS_3_4_ON_OFF:
    case MODULE_FUNCTION_BUSS_5_6_ON_OFF:
    case MODULE_FUNCTION_BUSS_7_8_ON_OFF:
    case MODULE_FUNCTION_BUSS_9_10_ON_OFF:
    case MODULE_FUNCTION_BUSS_11_12_ON_OFF:
    case MODULE_FUNCTION_BUSS_13_14_ON_OFF:
    case MODULE_FUNCTION_BUSS_15_16_ON_OFF:
    case MODULE_FUNCTION_BUSS_17_18_ON_OFF:
    case MODULE_FUNCTION_BUSS_19_20_ON_OFF:
    case MODULE_FUNCTION_BUSS_21_22_ON_OFF:
    case MODULE_FUNCTION_BUSS_23_24_ON_OFF:
    case MODULE_FUNCTION_BUSS_25_26_ON_OFF:
    case MODULE_FUNCTION_BUSS_27_28_ON_OFF:
    case MODULE_FUNCTION_BUSS_29_30_ON_OFF:
    case MODULE_FUNCTION_BUSS_31_32_ON_OFF:
    { //Buss on/off
      int BussNr = (FunctionNr-MODULE_FUNCTION_BUSS_1_2_ON_OFF)/(MODULE_FUNCTION_BUSS_3_4_ON_OFF-MODULE_FUNCTION_BUSS_1_2_ON_OFF);
      switch (DataType)
      {
      case STATE_DATATYPE:
      {
        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = STATE_DATATYPE;
        TransmitData[4] = 1;
        TransmitData[5] = AxumData.ModuleData[ModuleNr].Buss[BussNr].On&0xFF;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
      }
      break;
      }
    }
    break;
    case MODULE_FUNCTION_BUSS_1_2_PRE:
    case MODULE_FUNCTION_BUSS_3_4_PRE:
    case MODULE_FUNCTION_BUSS_5_6_PRE:
    case MODULE_FUNCTION_BUSS_7_8_PRE:
    case MODULE_FUNCTION_BUSS_9_10_PRE:
    case MODULE_FUNCTION_BUSS_11_12_PRE:
    case MODULE_FUNCTION_BUSS_13_14_PRE:
    case MODULE_FUNCTION_BUSS_15_16_PRE:
    case MODULE_FUNCTION_BUSS_17_18_PRE:
    case MODULE_FUNCTION_BUSS_19_20_PRE:
    case MODULE_FUNCTION_BUSS_21_22_PRE:
    case MODULE_FUNCTION_BUSS_23_24_PRE:
    case MODULE_FUNCTION_BUSS_25_26_PRE:
    case MODULE_FUNCTION_BUSS_27_28_PRE:
    case MODULE_FUNCTION_BUSS_29_30_PRE:
    case MODULE_FUNCTION_BUSS_31_32_PRE:
    { //Buss pre
      int BussNr = (FunctionNr-MODULE_FUNCTION_BUSS_1_2_PRE)/(MODULE_FUNCTION_BUSS_3_4_PRE-MODULE_FUNCTION_BUSS_1_2_PRE);
      switch (DataType)
      {
      case STATE_DATATYPE:
      {
        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = STATE_DATATYPE;
        TransmitData[4] = 1;
        TransmitData[5] = AxumData.ModuleData[ModuleNr].Buss[BussNr].PreModuleLevel&0xFF;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
      }
      break;
      }
    }
    break;
    case MODULE_FUNCTION_BUSS_1_2_BALANCE:
    case MODULE_FUNCTION_BUSS_3_4_BALANCE:
    case MODULE_FUNCTION_BUSS_5_6_BALANCE:
    case MODULE_FUNCTION_BUSS_7_8_BALANCE:
    case MODULE_FUNCTION_BUSS_9_10_BALANCE:
    case MODULE_FUNCTION_BUSS_11_12_BALANCE:
    case MODULE_FUNCTION_BUSS_13_14_BALANCE:
    case MODULE_FUNCTION_BUSS_15_16_BALANCE:
    case MODULE_FUNCTION_BUSS_17_18_BALANCE:
    case MODULE_FUNCTION_BUSS_19_20_BALANCE:
    case MODULE_FUNCTION_BUSS_21_22_BALANCE:
    case MODULE_FUNCTION_BUSS_23_24_BALANCE:
    case MODULE_FUNCTION_BUSS_25_26_BALANCE:
    case MODULE_FUNCTION_BUSS_27_28_BALANCE:
    case MODULE_FUNCTION_BUSS_29_30_BALANCE:
    case MODULE_FUNCTION_BUSS_31_32_BALANCE:
    { //Buss balance
      int BussNr = (FunctionNr-MODULE_FUNCTION_BUSS_1_2_BALANCE)/(MODULE_FUNCTION_BUSS_3_4_BALANCE-MODULE_FUNCTION_BUSS_1_2_BALANCE);
      switch (DataType)
      {
      case OCTET_STRING_DATATYPE:
      {
        unsigned char Types[4] = {'[','|','|',']'};
        unsigned char Pos = AxumData.ModuleData[ModuleNr].Buss[BussNr].Balance/128;
        unsigned char Type = (AxumData.ModuleData[ModuleNr].Buss[BussNr].Balance%128)/32;

        sprintf(LCDText, "        ");
        if (AxumData.ModuleData[ModuleNr].Buss[BussNr].Balance == 0)
        {
          sprintf(LCDText, "Left    ");
        }
        else if ((AxumData.ModuleData[ModuleNr].Buss[BussNr].Balance == 511) || (AxumData.ModuleData[ModuleNr].Buss[BussNr].Balance == 512))
        {
          sprintf(LCDText, " Center ");
        }
        else if (AxumData.ModuleData[ModuleNr].Buss[BussNr].Balance == 1023)
        {
          sprintf(LCDText, "   Right");
        }
        else
        {
          LCDText[Pos] = Types[Type];
        }

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = OCTET_STRING_DATATYPE;
        TransmitData[4] = 8;
        TransmitData[5] = LCDText[0];
        TransmitData[6] = LCDText[1];
        TransmitData[7] = LCDText[2];
        TransmitData[8] = LCDText[3];
        TransmitData[9] = LCDText[4];
        TransmitData[10] = LCDText[5];
        TransmitData[11] = LCDText[6];
        TransmitData[12] = LCDText[7];

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 13);
      }
      break;
      }
    }
    break;
    case MODULE_FUNCTION_BUSS_1_2_BALANCE_RESET:
    case MODULE_FUNCTION_BUSS_3_4_BALANCE_RESET:
    case MODULE_FUNCTION_BUSS_5_6_BALANCE_RESET:
    case MODULE_FUNCTION_BUSS_7_8_BALANCE_RESET:
    case MODULE_FUNCTION_BUSS_9_10_BALANCE_RESET:
    case MODULE_FUNCTION_BUSS_11_12_BALANCE_RESET:
    case MODULE_FUNCTION_BUSS_13_14_BALANCE_RESET:
    case MODULE_FUNCTION_BUSS_15_16_BALANCE_RESET:
    case MODULE_FUNCTION_BUSS_17_18_BALANCE_RESET:
    case MODULE_FUNCTION_BUSS_19_20_BALANCE_RESET:
    case MODULE_FUNCTION_BUSS_21_22_BALANCE_RESET:
    case MODULE_FUNCTION_BUSS_23_24_BALANCE_RESET:
    case MODULE_FUNCTION_BUSS_25_26_BALANCE_RESET:
    case MODULE_FUNCTION_BUSS_27_28_BALANCE_RESET:
    case MODULE_FUNCTION_BUSS_29_30_BALANCE_RESET:
    case MODULE_FUNCTION_BUSS_31_32_BALANCE_RESET:
    { //Buss balance reset
    }
    break;
    }
    if ((FunctionNr>=MODULE_FUNCTION_SOURCE_START) && (FunctionNr<MODULE_FUNCTION_CONTROL_1))
    { //all state functions
      switch (FunctionNr)
      {
      case MODULE_FUNCTION_SOURCE_START:
      { //Start
        Active = 0;
        if ((AxumData.ModuleData[ModuleNr].Source >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].Source <= matrix_sources.src_offset.max.source))
        {
          int SourceNr = AxumData.ModuleData[ModuleNr].Source-matrix_sources.src_offset.min.source;
          Active = AxumData.SourceData[SourceNr].Start;
        }
      }
      break;
      case MODULE_FUNCTION_SOURCE_STOP:
      { //Stop
        Active = 0;
        if ((AxumData.ModuleData[ModuleNr].Source >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].Source <= matrix_sources.src_offset.max.source))
        {
          int SourceNr = AxumData.ModuleData[ModuleNr].Source-matrix_sources.src_offset.min.source;
          Active = !AxumData.SourceData[SourceNr].Start;
        }
      }
      break;
      case MODULE_FUNCTION_SOURCE_START_STOP:
      { //Start/Stop
        Active = 0;
        if ((AxumData.ModuleData[ModuleNr].Source >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].Source <= matrix_sources.src_offset.max.source))
        {
          int SourceNr = AxumData.ModuleData[ModuleNr].Source-matrix_sources.src_offset.min.source;
          Active = AxumData.SourceData[SourceNr].Start;
        }
      }
      break;
      case MODULE_FUNCTION_COUGH_ON_OFF:
      { //Cough on/off
        Active = AxumData.ModuleData[ModuleNr].Cough;
      }
      break;
      case MODULE_FUNCTION_SOURCE_ALERT:
      { //Alert
        Active = 0;
        if ((AxumData.ModuleData[ModuleNr].Source >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].Source <= matrix_sources.src_offset.max.source))
        {
          int SourceNr = AxumData.ModuleData[ModuleNr].Source-matrix_sources.src_offset.min.source;
          Active = AxumData.SourceData[SourceNr].Alert;
        }
      }
      break;
      }
      TransmitData[0] = (ObjectNr>>8)&0xFF;
      TransmitData[1] = ObjectNr&0xFF;
      TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
      TransmitData[3] = STATE_DATATYPE;
      TransmitData[4] = 1;
      TransmitData[5] = Active&0xFF;

      SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
    }
    switch (FunctionNr)
    {
    case MODULE_FUNCTION_CONTROL_1:
    case MODULE_FUNCTION_CONTROL_2:
    case MODULE_FUNCTION_CONTROL_3:
    case MODULE_FUNCTION_CONTROL_4:
    { //Control 1-4
      ModeControllerSetData(SensorReceiveFunctionNumber, MambaNetAddress, ObjectNr, DataType, DataSize, DataMinimal, DataMaximal);
    }
    break;
    case MODULE_FUNCTION_CONTROL_1_LABEL:
    case MODULE_FUNCTION_CONTROL_2_LABEL:
    case MODULE_FUNCTION_CONTROL_3_LABEL:
    case MODULE_FUNCTION_CONTROL_4_LABEL:
    { //Control 1-4 label
      ModeControllerSetLabel(SensorReceiveFunctionNumber, MambaNetAddress, ObjectNr, DataType, DataSize, DataMinimal, DataMaximal);
    }
    break;
    case MODULE_FUNCTION_CONTROL_1_RESET:
    case MODULE_FUNCTION_CONTROL_2_RESET:
    case MODULE_FUNCTION_CONTROL_3_RESET:
    case MODULE_FUNCTION_CONTROL_4_RESET:
    { //Control 1-4 reset
    }
    break;
    case MODULE_FUNCTION_PEAK:
    { //Peak
      TransmitData[0] = (ObjectNr>>8)&0xFF;
      TransmitData[1] = ObjectNr&0xFF;
      TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
      TransmitData[3] = STATE_DATATYPE;
      TransmitData[4] = 1;
      TransmitData[5] = AxumData.ModuleData[ModuleNr].Peak&0xFF;

      SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
    }
    break;
    case MODULE_FUNCTION_SIGNAL:
    { //Signal
      TransmitData[0] = (ObjectNr>>8)&0xFF;
      TransmitData[1] = ObjectNr&0xFF;
      TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
      TransmitData[3] = STATE_DATATYPE;
      TransmitData[4] = 1;
      TransmitData[5] = AxumData.ModuleData[ModuleNr].Signal&0xFF;

      SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
    }
    break;
    }
  }
  break;
  case BUSS_FUNCTIONS:
  {   //Busses
    unsigned int BussNr = (SensorReceiveFunctionNumber>>12)&0xFFF;
    unsigned int FunctionNr = SensorReceiveFunctionNumber&0xFFF;
    unsigned char TransmitData[32];

    switch (FunctionNr)
    {
    case BUSS_FUNCTION_MASTER_LEVEL:
    {
      switch (DataType)
      {
      case UNSIGNED_INTEGER_DATATYPE:
      {
        int dB = 0;
        dB = ((AxumData.BussMasterData[BussNr].Level+10)*10)+1400;

        if (dB<0)
        {
          dB = 0;
        }
        else if (dB>=1500)
        {
          dB = 1499;
        }
        int Position = dB2Position[dB];
        Position = ((dB2Position[dB]*(DataMaximal-DataMinimal))/1023)+DataMinimal;
        if (Position<DataMinimal)
        {
          Position = DataMinimal;
        }
        else if (Position>DataMaximal)
        {
          Position = DataMaximal;
        }

        int cntTransmitData = 0;
        TransmitData[cntTransmitData++] = (ObjectNr>>8)&0xFF;
        TransmitData[cntTransmitData++] = ObjectNr&0xFF;
        TransmitData[cntTransmitData++] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[cntTransmitData++] = UNSIGNED_INTEGER_DATATYPE;
        TransmitData[cntTransmitData++] = DataSize;
        for (int cntByte=0; cntByte<DataSize; cntByte++)
        {
          char BitsToShift = ((DataSize-1)-cntByte)*8;
          TransmitData[cntTransmitData++] = (Position>>BitsToShift)&0xFF;
        }
        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, cntTransmitData);
      }
      break;
      case OCTET_STRING_DATATYPE:
      {
        char LCDText[9];
        sprintf(LCDText, " %4.0f dB", AxumData.BussMasterData[BussNr].Level);

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = OCTET_STRING_DATATYPE;
        TransmitData[4] = 8;
        TransmitData[5] = LCDText[0];
        TransmitData[6] = LCDText[1];
        TransmitData[7] = LCDText[2];
        TransmitData[8] = LCDText[3];
        TransmitData[9] = LCDText[4];
        TransmitData[10] = LCDText[5];
        TransmitData[11] = LCDText[6];
        TransmitData[12] = LCDText[7];

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 13);
      }
      break;
      case FLOAT_DATATYPE:
      {
        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = FLOAT_DATATYPE;
        TransmitData[4] = 2;

        float Level = AxumData.BussMasterData[BussNr].Level;
        if (Level<DataMinimal)
        {
          Level = DataMinimal;
        }
        else if (Level>DataMaximal)
        {
          Level = DataMaximal;
        }

        Float2VariableFloat(Level, 2, &TransmitData[5]);

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 7);
      }
      break;
      }
    }
    break;
    case BUSS_FUNCTION_MASTER_LEVEL_RESET:
    { //No implementation
    }
    break;
    case BUSS_FUNCTION_MASTER_ON_OFF:
    {
      switch (DataType)
      {
      case STATE_DATATYPE:
      {
        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = STATE_DATATYPE;
        TransmitData[4] = 1;
        TransmitData[5] = AxumData.BussMasterData[BussNr].On&0xFF;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
      }
      break;
      }
    }
    break;
    case BUSS_FUNCTION_MASTER_PRE:
    {
      switch (DataType)
      {
      case STATE_DATATYPE:
      {
        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = STATE_DATATYPE;
        TransmitData[4] = 1;
        TransmitData[5] = AxumData.BussMasterData[BussNr].PreModuleLevel&0xFF;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
      }
      break;
      }
    }
    break;
    case BUSS_FUNCTION_LABEL:
    { //Buss label
      switch (DataType)
      {
      case OCTET_STRING_DATATYPE:
      {
        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = OCTET_STRING_DATATYPE;
        TransmitData[4] = 8;
        TransmitData[5] = AxumData.BussMasterData[BussNr].Label[0];
        TransmitData[6] = AxumData.BussMasterData[BussNr].Label[1];
        TransmitData[7] = AxumData.BussMasterData[BussNr].Label[2];
        TransmitData[8] = AxumData.BussMasterData[BussNr].Label[3];
        TransmitData[9] = AxumData.BussMasterData[BussNr].Label[4];
        TransmitData[10] = AxumData.BussMasterData[BussNr].Label[5];
        TransmitData[11] = AxumData.BussMasterData[BussNr].Label[6];
        TransmitData[12] = AxumData.BussMasterData[BussNr].Label[7];

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 13);
      }
      break;
      }
    }
    break;
    case BUSS_FUNCTION_AUDIO_LEVEL_LEFT:
    {
      TransmitData[0] = (ObjectNr>>8)&0xFF;
      TransmitData[1] = ObjectNr&0xFF;
      TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
      TransmitData[3] = FLOAT_DATATYPE;
      TransmitData[4] = 2;

      unsigned char VariableFloat[2] = {0x00, 0x00};
      Float2VariableFloat(SummingdBLevel[(BussNr*2)+0], 2, VariableFloat);
      TransmitData[5] = VariableFloat[0];
      TransmitData[6] = VariableFloat[1];

      SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 7);
    }
    break;
    case BUSS_FUNCTION_AUDIO_LEVEL_RIGHT:
    {
      TransmitData[0] = (ObjectNr>>8)&0xFF;
      TransmitData[1] = ObjectNr&0xFF;
      TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
      TransmitData[3] = FLOAT_DATATYPE;
      TransmitData[4] = 2;

      unsigned char VariableFloat[2] = {0x00, 0x00};
      Float2VariableFloat(SummingdBLevel[(BussNr*2)+1], 2, VariableFloat);
      TransmitData[5] = VariableFloat[0];
      TransmitData[6] = VariableFloat[1];

      SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 7);
    }
    break;
    }
  }
  break;
  case MONITOR_BUSS_FUNCTIONS:
  {   //Monitor Busses
    unsigned int MonitorBussNr = (SensorReceiveFunctionNumber>>12)&0xFFF;
    unsigned int FunctionNr = SensorReceiveFunctionNumber&0xFFF;
    unsigned int Active = 0;
    unsigned char TransmitData[32];

    if (FunctionNr<24)
    {
      switch (FunctionNr)
      {
      case MONITOR_BUSS_FUNCTION_BUSS_1_2_ON_OFF:
      case MONITOR_BUSS_FUNCTION_BUSS_3_4_ON_OFF:
      case MONITOR_BUSS_FUNCTION_BUSS_5_6_ON_OFF:
      case MONITOR_BUSS_FUNCTION_BUSS_7_8_ON_OFF:
      case MONITOR_BUSS_FUNCTION_BUSS_9_10_ON_OFF:
      case MONITOR_BUSS_FUNCTION_BUSS_11_12_ON_OFF:
      case MONITOR_BUSS_FUNCTION_BUSS_13_14_ON_OFF:
      case MONITOR_BUSS_FUNCTION_BUSS_15_16_ON_OFF:
      case MONITOR_BUSS_FUNCTION_BUSS_17_18_ON_OFF:
      case MONITOR_BUSS_FUNCTION_BUSS_19_20_ON_OFF:
      case MONITOR_BUSS_FUNCTION_BUSS_21_22_ON_OFF:
      case MONITOR_BUSS_FUNCTION_BUSS_23_24_ON_OFF:
      case MONITOR_BUSS_FUNCTION_BUSS_25_26_ON_OFF:
      case MONITOR_BUSS_FUNCTION_BUSS_27_28_ON_OFF:
      case MONITOR_BUSS_FUNCTION_BUSS_29_30_ON_OFF:
      { //Prog
        int BussNr = FunctionNr-MONITOR_BUSS_FUNCTION_BUSS_1_2_ON_OFF;
        Active = AxumData.Monitor[MonitorBussNr].Buss[BussNr];
      }
      break;
      case MONITOR_BUSS_FUNCTION_EXT_1_ON_OFF:
      { //Ext 1
        Active = AxumData.Monitor[MonitorBussNr].Ext[0];
      }
      break;
      case MONITOR_BUSS_FUNCTION_EXT_2_ON_OFF:
      { //Ext 2
        Active = AxumData.Monitor[MonitorBussNr].Ext[1];
      }
      break;
      case MONITOR_BUSS_FUNCTION_EXT_3_ON_OFF:
      { //Ext 3
        Active = AxumData.Monitor[MonitorBussNr].Ext[2];
      }
      break;
      case MONITOR_BUSS_FUNCTION_EXT_4_ON_OFF:
      { //Ext 4
        Active = AxumData.Monitor[MonitorBussNr].Ext[3];
      }
      break;
      case MONITOR_BUSS_FUNCTION_EXT_5_ON_OFF:
      { //Ext 5
        Active = AxumData.Monitor[MonitorBussNr].Ext[4];
      }
      break;
      case MONITOR_BUSS_FUNCTION_EXT_6_ON_OFF:
      { //Ext 6
        Active = AxumData.Monitor[MonitorBussNr].Ext[5];
      }
      break;
      case MONITOR_BUSS_FUNCTION_EXT_7_ON_OFF:
      { //Ext 7
        Active = AxumData.Monitor[MonitorBussNr].Ext[6];
      }
      break;
      case MONITOR_BUSS_FUNCTION_EXT_8_ON_OFF:
      { //Ext 8
        Active = AxumData.Monitor[MonitorBussNr].Ext[7];
      }
      break;
      }

      TransmitData[0] = (ObjectNr>>8)&0xFF;
      TransmitData[1] = ObjectNr&0xFF;
      TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
      TransmitData[3] = STATE_DATATYPE;
      TransmitData[4] = 1;
      TransmitData[5] = Active&0xFF;

      SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
    }
    else
    {
      switch (FunctionNr)
      {
      case MONITOR_BUSS_FUNCTION_MUTE:
      { //Mute
        switch (DataType)
        {
        case STATE_DATATYPE:
        {
          unsigned char Mute = AxumData.Monitor[MonitorBussNr].Mute;

          TransmitData[0] = (ObjectNr>>8)&0xFF;
          TransmitData[1] = ObjectNr&0xFF;
          TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
          TransmitData[3] = STATE_DATATYPE;
          TransmitData[4] = 1;
          TransmitData[5] = Mute&0xFF;

          SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
        }
        break;
        }
      }
      break;
      case MONITOR_BUSS_FUNCTION_DIM:
      { //Dim
        switch (DataType)
        {
        case STATE_DATATYPE:
        {
          TransmitData[0] = (ObjectNr>>8)&0xFF;
          TransmitData[1] = ObjectNr&0xFF;
          TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
          TransmitData[3] = STATE_DATATYPE;
          TransmitData[4] = 1;
          TransmitData[5] = AxumData.Monitor[MonitorBussNr].Dim&0xFF;

          SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
        }
        break;
        }
      }
      break;
      case MONITOR_BUSS_FUNCTION_PHONES_LEVEL:
      { //Phones level
        switch (DataType)
        {
        case UNSIGNED_INTEGER_DATATYPE:
        {
          int dB = 0;
          dB = ((AxumData.Monitor[MonitorBussNr].PhonesLevel-20)*10)+1400;

          if (dB<0)
          {
            dB = 0;
          }
          else if (dB>=1500)
          {
            dB = 1499;
          }
          int Position = dB2Position[dB];
          Position = ((dB2Position[dB]*(DataMaximal-DataMinimal))/1023)+DataMinimal;
          if (Position<DataMinimal)
          {
            Position = DataMinimal;
          }
          else if (Position>DataMaximal)
          {
            Position = DataMaximal;
          }

          int cntTransmitData = 0;
          TransmitData[cntTransmitData++] = (ObjectNr>>8)&0xFF;
          TransmitData[cntTransmitData++] = ObjectNr&0xFF;
          TransmitData[cntTransmitData++] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
          TransmitData[cntTransmitData++] = UNSIGNED_INTEGER_DATATYPE;
          TransmitData[cntTransmitData++] = DataSize;
          for (int cntByte=0; cntByte<DataSize; cntByte++)
          {
            char BitsToShift = ((DataSize-1)-cntByte)*8;
            TransmitData[cntTransmitData++] = (Position>>BitsToShift)&0xFF;
          }
          SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, cntTransmitData);
        }
        break;
        case OCTET_STRING_DATATYPE:
        {
          char LCDText[9];
          sprintf(LCDText, " %4.0f dB", AxumData.Monitor[MonitorBussNr].PhonesLevel);

          TransmitData[0] = (ObjectNr>>8)&0xFF;
          TransmitData[1] = ObjectNr&0xFF;
          TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
          TransmitData[3] = OCTET_STRING_DATATYPE;
          TransmitData[4] = 8;
          TransmitData[5] = LCDText[0];
          TransmitData[6] = LCDText[1];
          TransmitData[7] = LCDText[2];
          TransmitData[8] = LCDText[3];
          TransmitData[9] = LCDText[4];
          TransmitData[10] = LCDText[5];
          TransmitData[11] = LCDText[6];
          TransmitData[12] = LCDText[7];

          SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 13);
        }
        break;
        case FLOAT_DATATYPE:
        {
          TransmitData[0] = (ObjectNr>>8)&0xFF;
          TransmitData[1] = ObjectNr&0xFF;
          TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
          TransmitData[3] = FLOAT_DATATYPE;
          TransmitData[4] = 2;

          float Level = AxumData.Monitor[MonitorBussNr].PhonesLevel;
          if (Level<DataMinimal)
          {
            Level = DataMinimal;
          }
          else if (Level>DataMaximal)
          {
            Level = DataMaximal;
          }

          Float2VariableFloat(Level, 2, &TransmitData[5]);

          SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 7);
        }
        break;
        }
      }
      break;
      case MONITOR_BUSS_FUNCTION_MONO:
      { //Mono
        switch (DataType)
        {
        case STATE_DATATYPE:
        {
          TransmitData[0] = (ObjectNr>>8)&0xFF;
          TransmitData[1] = ObjectNr&0xFF;
          TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
          TransmitData[3] = STATE_DATATYPE;
          TransmitData[4] = 1;
          TransmitData[5] = AxumData.Monitor[MonitorBussNr].Mono&0xFF;

          SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
        }
        break;
        }
      }
      break;
      case MONITOR_BUSS_FUNCTION_PHASE:
      { //Phase
        switch (DataType)
        {
        case STATE_DATATYPE:
        {
          TransmitData[0] = (ObjectNr>>8)&0xFF;
          TransmitData[1] = ObjectNr&0xFF;
          TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
          TransmitData[3] = STATE_DATATYPE;
          TransmitData[4] = 1;
          TransmitData[5] = AxumData.Monitor[MonitorBussNr].Phase&0xFF;

          SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
        }
        break;
        }
      }
      break;
      case MONITOR_BUSS_FUNCTION_SPEAKER_LEVEL:
      { //Speaker level
        switch (DataType)
        {
        case UNSIGNED_INTEGER_DATATYPE:
        {
          int dB = 0;
          dB = ((AxumData.Monitor[MonitorBussNr].SpeakerLevel-20)*10)+1400;

          if (dB<0)
          {
            dB = 0;
          }
          else if (dB>=1500)
          {
            dB = 1499;
          }
          int Position = dB2Position[dB];
          Position = ((dB2Position[dB]*(DataMaximal-DataMinimal))/1023)+DataMinimal;
          if (Position<DataMinimal)
          {
            Position = DataMinimal;
          }
          else if (Position>DataMaximal)
          {
            Position = DataMaximal;
          }

          int cntTransmitData = 0;
          TransmitData[cntTransmitData++] = (ObjectNr>>8)&0xFF;
          TransmitData[cntTransmitData++] = ObjectNr&0xFF;
          TransmitData[cntTransmitData++] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
          TransmitData[cntTransmitData++] = UNSIGNED_INTEGER_DATATYPE;
          TransmitData[cntTransmitData++] = DataSize;
          for (int cntByte=0; cntByte<DataSize; cntByte++)
          {
            char BitsToShift = ((DataSize-1)-cntByte)*8;
            TransmitData[cntTransmitData++] = (Position>>BitsToShift)&0xFF;
          }
          SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, cntTransmitData);
        }
        break;
        case OCTET_STRING_DATATYPE:
        {
          char LCDText[9];
          sprintf(LCDText, " %4.0f dB", AxumData.Monitor[MonitorBussNr].SpeakerLevel);

          TransmitData[0] = (ObjectNr>>8)&0xFF;
          TransmitData[1] = ObjectNr&0xFF;
          TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
          TransmitData[3] = OCTET_STRING_DATATYPE;
          TransmitData[4] = 8;
          TransmitData[5] = LCDText[0];
          TransmitData[6] = LCDText[1];
          TransmitData[7] = LCDText[2];
          TransmitData[8] = LCDText[3];
          TransmitData[9] = LCDText[4];
          TransmitData[10] = LCDText[5];
          TransmitData[11] = LCDText[6];
          TransmitData[12] = LCDText[7];

          SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 13);
        }
        break;
        case FLOAT_DATATYPE:
        {
          TransmitData[0] = (ObjectNr>>8)&0xFF;
          TransmitData[1] = ObjectNr&0xFF;
          TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
          TransmitData[3] = FLOAT_DATATYPE;
          TransmitData[4] = 2;

          float Level = AxumData.Monitor[MonitorBussNr].SpeakerLevel;
          if (Level<DataMinimal)
          {
            Level = DataMinimal;
          }
          else if (Level>DataMaximal)
          {
            Level = DataMaximal;
          }

          Float2VariableFloat(Level, 2, &TransmitData[5]);

          SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 7);
        }
        break;
        }
      }
      break;
      case MONITOR_BUSS_FUNCTION_TALKBACK_1:
      case MONITOR_BUSS_FUNCTION_TALKBACK_2:
      case MONITOR_BUSS_FUNCTION_TALKBACK_3:
      case MONITOR_BUSS_FUNCTION_TALKBACK_4:
      case MONITOR_BUSS_FUNCTION_TALKBACK_5:
      case MONITOR_BUSS_FUNCTION_TALKBACK_6:
      case MONITOR_BUSS_FUNCTION_TALKBACK_7:
      case MONITOR_BUSS_FUNCTION_TALKBACK_8:
      case MONITOR_BUSS_FUNCTION_TALKBACK_9:
      case MONITOR_BUSS_FUNCTION_TALKBACK_10:
      case MONITOR_BUSS_FUNCTION_TALKBACK_11:
      case MONITOR_BUSS_FUNCTION_TALKBACK_12:
      case MONITOR_BUSS_FUNCTION_TALKBACK_13:
      case MONITOR_BUSS_FUNCTION_TALKBACK_14:
      case MONITOR_BUSS_FUNCTION_TALKBACK_15:
      case MONITOR_BUSS_FUNCTION_TALKBACK_16:
      {
        int TalkbackNr = FunctionNr - MONITOR_BUSS_FUNCTION_TALKBACK_1;
        switch (DataType)
        {
        case STATE_DATATYPE:
        {
          TransmitData[0] = (ObjectNr>>8)&0xFF;
          TransmitData[1] = ObjectNr&0xFF;
          TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
          TransmitData[3] = STATE_DATATYPE;
          TransmitData[4] = 1;
          TransmitData[5] = AxumData.Monitor[MonitorBussNr].Talkback[TalkbackNr]&0xFF;

          SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
        }
        break;
        }
      }
      break;
      case MONITOR_BUSS_FUNCTION_AUDIO_LEVEL_LEFT:
      {
        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = FLOAT_DATATYPE;
        TransmitData[4] = 2;

        unsigned char VariableFloat[2] = {0x00, 0x00};
        Float2VariableFloat(SummingdBLevel[32+(MonitorBussNr*2)+0], 2, VariableFloat);
        TransmitData[5] = VariableFloat[0];
        TransmitData[6] = VariableFloat[1];

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 7);
      }
      break;
      case MONITOR_BUSS_FUNCTION_AUDIO_LEVEL_RIGHT:
      {
        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = FLOAT_DATATYPE;
        TransmitData[4] = 2;

        unsigned char VariableFloat[2] = {0x00, 0x00};
        Float2VariableFloat(SummingdBLevel[32+(MonitorBussNr*2)+1], 2, VariableFloat);
        TransmitData[5] = VariableFloat[0];
        TransmitData[6] = VariableFloat[1];

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 7);
      }
      break;
      case MONITOR_BUSS_FUNCTION_LABEL:
      { //Buss label
        switch (DataType)
        {
        case OCTET_STRING_DATATYPE:
        {
          TransmitData[0] = (ObjectNr>>8)&0xFF;
          TransmitData[1] = ObjectNr&0xFF;
          TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
          TransmitData[3] = OCTET_STRING_DATATYPE;
          TransmitData[4] = 8;
          TransmitData[5] = AxumData.Monitor[MonitorBussNr].Label[0];
          TransmitData[6] = AxumData.Monitor[MonitorBussNr].Label[1];
          TransmitData[7] = AxumData.Monitor[MonitorBussNr].Label[2];
          TransmitData[8] = AxumData.Monitor[MonitorBussNr].Label[3];
          TransmitData[9] = AxumData.Monitor[MonitorBussNr].Label[4];
          TransmitData[10] = AxumData.Monitor[MonitorBussNr].Label[5];
          TransmitData[11] = AxumData.Monitor[MonitorBussNr].Label[6];
          TransmitData[12] = AxumData.Monitor[MonitorBussNr].Label[7];

          SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 13);
        }
        break;
        }
      }
      break;
      }
    }
  }
  break;
  case GLOBAL_FUNCTIONS:
  {   //Global
    unsigned int GlobalNr = (SensorReceiveFunctionNumber>>12)&0xFFF;
    unsigned int FunctionNr = SensorReceiveFunctionNumber&0xFFF;

    if (GlobalNr == 0)
    {
      if ((((signed int)FunctionNr)>=GLOBAL_FUNCTION_REDLIGHT_1) && (FunctionNr<GLOBAL_FUNCTION_CONTROL_1_MODE_SOURCE))
      { //all states
        unsigned char Active = 0;
        unsigned char TransmitData[32];

        switch (FunctionNr)
        {
        case GLOBAL_FUNCTION_REDLIGHT_1:
        case GLOBAL_FUNCTION_REDLIGHT_2:
        case GLOBAL_FUNCTION_REDLIGHT_3:
        case GLOBAL_FUNCTION_REDLIGHT_4:
        case GLOBAL_FUNCTION_REDLIGHT_5:
        case GLOBAL_FUNCTION_REDLIGHT_6:
        case GLOBAL_FUNCTION_REDLIGHT_7:
        case GLOBAL_FUNCTION_REDLIGHT_8:
        {
          Active = AxumData.Redlight[FunctionNr-GLOBAL_FUNCTION_REDLIGHT_1];
        }
        break;
        case GLOBAL_FUNCTION_BUSS_1_2_RESET:
        case GLOBAL_FUNCTION_BUSS_3_4_RESET:
        case GLOBAL_FUNCTION_BUSS_5_6_RESET:
        case GLOBAL_FUNCTION_BUSS_7_8_RESET:
        case GLOBAL_FUNCTION_BUSS_9_10_RESET:
        case GLOBAL_FUNCTION_BUSS_11_12_RESET:
        case GLOBAL_FUNCTION_BUSS_13_14_RESET:
        case GLOBAL_FUNCTION_BUSS_15_16_RESET:
        case GLOBAL_FUNCTION_BUSS_17_18_RESET:
        case GLOBAL_FUNCTION_BUSS_19_20_RESET:
        case GLOBAL_FUNCTION_BUSS_21_22_RESET:
        case GLOBAL_FUNCTION_BUSS_23_24_RESET:
        case GLOBAL_FUNCTION_BUSS_25_26_RESET:
        case GLOBAL_FUNCTION_BUSS_27_28_RESET:
        case GLOBAL_FUNCTION_BUSS_29_30_RESET:
        case GLOBAL_FUNCTION_BUSS_31_32_RESET:
        {
          int BussNr = (FunctionNr-GLOBAL_FUNCTION_BUSS_1_2_RESET)/(GLOBAL_FUNCTION_BUSS_3_4_RESET-GLOBAL_FUNCTION_BUSS_1_2_RESET);

          for (int cntModule=0; cntModule<128; cntModule++)
          {
            if (AxumData.ModuleData[cntModule].Buss[BussNr].On)
            {
              Active = 1;
            }
          }
        }
        break;
        }

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = STATE_DATATYPE;
        TransmitData[4] = 1;
        TransmitData[5] = Active&0xFF;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
      }
      else if ((FunctionNr>=GLOBAL_FUNCTION_CONTROL_1_MODE_SOURCE) && (FunctionNr<GLOBAL_FUNCTION_CONTROL_2_MODE_SOURCE))
      { //Control 1 mode
        unsigned char Active = 0;
        unsigned char TransmitData[32];
        unsigned char CorrespondingControlMode = FunctionNr-GLOBAL_FUNCTION_CONTROL_1_MODE_SOURCE;
        if (AxumData.Control1Mode == CorrespondingControlMode)
        {
          Active = 1;
        }

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = STATE_DATATYPE;
        TransmitData[4] = 1;
        TransmitData[5] = Active&0xFF;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
      }
      else if ((FunctionNr>=GLOBAL_FUNCTION_CONTROL_2_MODE_SOURCE) && (FunctionNr<GLOBAL_FUNCTION_CONTROL_3_MODE_SOURCE))
      { //Control 2 mode
        unsigned char Active = 0;
        unsigned char TransmitData[32];
        unsigned char CorrespondingControlMode = FunctionNr-GLOBAL_FUNCTION_CONTROL_2_MODE_SOURCE;
        if (AxumData.Control2Mode == CorrespondingControlMode)
        {
          Active = 1;
        }

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = STATE_DATATYPE;
        TransmitData[4] = 1;
        TransmitData[5] = Active&0xFF;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
      }
      else if ((FunctionNr>=GLOBAL_FUNCTION_CONTROL_3_MODE_SOURCE) && (FunctionNr<GLOBAL_FUNCTION_CONTROL_4_MODE_SOURCE))
      { //Control 3 mode
        unsigned char Active = 0;
        unsigned char TransmitData[32];
        unsigned char CorrespondingControlMode = FunctionNr-GLOBAL_FUNCTION_CONTROL_3_MODE_SOURCE;
        if (AxumData.Control3Mode == CorrespondingControlMode)
        {
          Active = 1;
        }

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = STATE_DATATYPE;
        TransmitData[4] = 1;
        TransmitData[5] = Active&0xFF;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
      }
      else if ((FunctionNr>=GLOBAL_FUNCTION_CONTROL_4_MODE_SOURCE) && (FunctionNr<GLOBAL_FUNCTION_MASTER_CONTROL_1_MODE_BUSS_1_2))
      { //Control 4 mode
        unsigned char Active = 0;
        unsigned char TransmitData[32];
        unsigned char CorrespondingControlMode = FunctionNr-GLOBAL_FUNCTION_CONTROL_4_MODE_SOURCE;
        if (AxumData.Control4Mode == CorrespondingControlMode)
        {
          Active = 1;
        }

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = STATE_DATATYPE;
        TransmitData[4] = 1;
        TransmitData[5] = Active&0xFF;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
      }
      else if ((FunctionNr>=GLOBAL_FUNCTION_MASTER_CONTROL_1_MODE_BUSS_1_2) && (FunctionNr<GLOBAL_FUNCTION_MASTER_CONTROL_2_MODE_BUSS_1_2))
      { //Master control 1 mode
        unsigned char Active = 0;
        unsigned char TransmitData[32];
        unsigned char CorrespondingControlMode = FunctionNr-GLOBAL_FUNCTION_MASTER_CONTROL_1_MODE_BUSS_1_2;
        if (AxumData.MasterControl1Mode == CorrespondingControlMode)
        {
          Active = 1;
        }

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = STATE_DATATYPE;
        TransmitData[4] = 1;
        TransmitData[5] = Active&0xFF;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
      }
      else if ((FunctionNr>=GLOBAL_FUNCTION_MASTER_CONTROL_2_MODE_BUSS_1_2) && (FunctionNr<GLOBAL_FUNCTION_MASTER_CONTROL_3_MODE_BUSS_1_2))
      { //Master control 2 mode
        unsigned char Active = 0;
        unsigned char TransmitData[32];
        unsigned char CorrespondingControlMode = FunctionNr-GLOBAL_FUNCTION_MASTER_CONTROL_2_MODE_BUSS_1_2;
        if (AxumData.MasterControl2Mode == CorrespondingControlMode)
        {
          Active = 1;
        }

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = STATE_DATATYPE;
        TransmitData[4] = 1;
        TransmitData[5] = Active&0xFF;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
      }
      else if ((FunctionNr>=GLOBAL_FUNCTION_MASTER_CONTROL_3_MODE_BUSS_1_2) && (FunctionNr<GLOBAL_FUNCTION_MASTER_CONTROL_4_MODE_BUSS_1_2))
      { //Master control 3 mode
        unsigned char Active = 0;
        unsigned char TransmitData[32];
        unsigned char CorrespondingControlMode = FunctionNr-GLOBAL_FUNCTION_MASTER_CONTROL_3_MODE_BUSS_1_2;
        if (AxumData.MasterControl3Mode == CorrespondingControlMode)
        {
          Active = 1;
        }

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = STATE_DATATYPE;
        TransmitData[4] = 1;
        TransmitData[5] = Active&0xFF;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
      }
      else if ((FunctionNr>=GLOBAL_FUNCTION_MASTER_CONTROL_4_MODE_BUSS_1_2) && (FunctionNr<GLOBAL_FUNCTION_MASTER_CONTROL_1))
      { //Master control 1 mode
        unsigned char Active = 0;
        unsigned char TransmitData[32];
        unsigned char CorrespondingControlMode = FunctionNr-GLOBAL_FUNCTION_MASTER_CONTROL_4_MODE_BUSS_1_2;
        if (AxumData.MasterControl4Mode == CorrespondingControlMode)
        {
          Active = 1;
        }

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = STATE_DATATYPE;
        TransmitData[4] = 1;
        TransmitData[5] = Active&0xFF;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
      }
      else if ((FunctionNr==GLOBAL_FUNCTION_MASTER_CONTROL_1) ||
               (FunctionNr==GLOBAL_FUNCTION_MASTER_CONTROL_2) ||
               (FunctionNr==GLOBAL_FUNCTION_MASTER_CONTROL_3) ||
               (FunctionNr==GLOBAL_FUNCTION_MASTER_CONTROL_4))
      { //Master control 1-4
        MasterModeControllerSetData(SensorReceiveFunctionNumber, MambaNetAddress, ObjectNr, DataType, DataSize, DataMinimal, DataMaximal);
      }
    }
  }
  break;
  case SOURCE_FUNCTIONS:
  {   //Source
    unsigned int SourceNr = ((SensorReceiveFunctionNumber>>12)&0xFFF);
    unsigned int FunctionNr = SensorReceiveFunctionNumber&0xFFF;
    unsigned int Active = 0;
    unsigned char TransmitData[32];

    switch (FunctionNr)
    {
    case SOURCE_FUNCTION_MODULE_ON:
    case SOURCE_FUNCTION_MODULE_OFF:
    case SOURCE_FUNCTION_MODULE_ON_OFF:
    {
      Active = 0;
      for (int cntModule=0; cntModule<128; cntModule++)
      {
        if (AxumData.ModuleData[cntModule].Source == (SourceNr+matrix_sources.src_offset.min.source))
        {
          if (AxumData.ModuleData[cntModule].On)
          {
            Active = 1;
          }
        }
      }

      TransmitData[0] = (ObjectNr>>8)&0xFF;
      TransmitData[1] = ObjectNr&0xFF;
      TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
      TransmitData[3] = STATE_DATATYPE;
      TransmitData[4] = 1;
      if (FunctionNr == SOURCE_FUNCTION_MODULE_OFF)
      {
        TransmitData[5] = !Active;
      }
      else
      {
        TransmitData[5] = Active;
      }

      SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
    }
    break;
    case SOURCE_FUNCTION_MODULE_FADER_ON:
    case SOURCE_FUNCTION_MODULE_FADER_OFF:
    case SOURCE_FUNCTION_MODULE_FADER_ON_OFF:
    {
      Active = 0;
      for (int cntModule=0; cntModule<128; cntModule++)
      {
        if (AxumData.ModuleData[cntModule].Source == (SourceNr+matrix_sources.src_offset.min.source))
        {
          if (AxumData.ModuleData[cntModule].FaderLevel>-80)
          {
            Active = 1;
          }
        }
      }

      TransmitData[0] = (ObjectNr>>8)&0xFF;
      TransmitData[1] = ObjectNr&0xFF;
      TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
      TransmitData[3] = STATE_DATATYPE;
      TransmitData[4] = 1;
      if (FunctionNr == SOURCE_FUNCTION_MODULE_FADER_OFF)
      {
        TransmitData[5] = !Active;
      }
      else
      {
        TransmitData[5] = Active;
      }

      SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
    }
    break;
    case SOURCE_FUNCTION_MODULE_FADER_AND_ON_ACTIVE:
    case SOURCE_FUNCTION_MODULE_FADER_AND_ON_INACTIVE:
    {
      Active = 0;
      for (int cntModule=0; cntModule<128; cntModule++)
      {
        if (AxumData.ModuleData[cntModule].Source == (SourceNr+matrix_sources.src_offset.min.source))
        {
          if (AxumData.ModuleData[cntModule].FaderLevel>-80)
          {
            if (AxumData.ModuleData[cntModule].On)
            {
              Active = 1;
            }
          }
        }
      }

      TransmitData[0] = (ObjectNr>>8)&0xFF;
      TransmitData[1] = ObjectNr&0xFF;
      TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
      TransmitData[3] = STATE_DATATYPE;
      TransmitData[4] = 1;
      if (FunctionNr == SOURCE_FUNCTION_MODULE_FADER_AND_ON_INACTIVE)
      {
        TransmitData[5] = !Active;
      }
      else
      {
        TransmitData[5] = Active;
      }

      SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
    }
    break;
    case SOURCE_FUNCTION_MODULE_BUSS_1_2_ON:
    case SOURCE_FUNCTION_MODULE_BUSS_3_4_ON:
    case SOURCE_FUNCTION_MODULE_BUSS_5_6_ON:
    case SOURCE_FUNCTION_MODULE_BUSS_7_8_ON:
    case SOURCE_FUNCTION_MODULE_BUSS_9_10_ON:
    case SOURCE_FUNCTION_MODULE_BUSS_11_12_ON:
    case SOURCE_FUNCTION_MODULE_BUSS_13_14_ON:
    case SOURCE_FUNCTION_MODULE_BUSS_15_16_ON:
    case SOURCE_FUNCTION_MODULE_BUSS_17_18_ON:
    case SOURCE_FUNCTION_MODULE_BUSS_19_20_ON:
    case SOURCE_FUNCTION_MODULE_BUSS_21_22_ON:
    case SOURCE_FUNCTION_MODULE_BUSS_23_24_ON:
    case SOURCE_FUNCTION_MODULE_BUSS_25_26_ON:
    case SOURCE_FUNCTION_MODULE_BUSS_27_28_ON:
    case SOURCE_FUNCTION_MODULE_BUSS_29_30_ON:
    case SOURCE_FUNCTION_MODULE_BUSS_31_32_ON:
    {
      int BussNr = (FunctionNr-SOURCE_FUNCTION_MODULE_BUSS_1_2_ON)/(SOURCE_FUNCTION_MODULE_BUSS_3_4_ON-SOURCE_FUNCTION_MODULE_BUSS_1_2_ON);

      Active = 0;
      for (int cntModule=0; cntModule<128; cntModule++)
      {
        if (AxumData.ModuleData[cntModule].Source == (SourceNr+matrix_sources.src_offset.min.source))
        {
          if (AxumData.ModuleData[cntModule].Buss[BussNr].On)
          {
            Active = 1;
          }
        }
      }

      TransmitData[0] = (ObjectNr>>8)&0xFF;
      TransmitData[1] = ObjectNr&0xFF;
      TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
      TransmitData[3] = STATE_DATATYPE;
      TransmitData[4] = 1;
      TransmitData[5] = Active;

      SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
    }
    break;
    case SOURCE_FUNCTION_MODULE_BUSS_1_2_OFF:
    case SOURCE_FUNCTION_MODULE_BUSS_3_4_OFF:
    case SOURCE_FUNCTION_MODULE_BUSS_5_6_OFF:
    case SOURCE_FUNCTION_MODULE_BUSS_7_8_OFF:
    case SOURCE_FUNCTION_MODULE_BUSS_9_10_OFF:
    case SOURCE_FUNCTION_MODULE_BUSS_11_12_OFF:
    case SOURCE_FUNCTION_MODULE_BUSS_13_14_OFF:
    case SOURCE_FUNCTION_MODULE_BUSS_15_16_OFF:
    case SOURCE_FUNCTION_MODULE_BUSS_17_18_OFF:
    case SOURCE_FUNCTION_MODULE_BUSS_19_20_OFF:
    case SOURCE_FUNCTION_MODULE_BUSS_21_22_OFF:
    case SOURCE_FUNCTION_MODULE_BUSS_23_24_OFF:
    case SOURCE_FUNCTION_MODULE_BUSS_25_26_OFF:
    case SOURCE_FUNCTION_MODULE_BUSS_27_28_OFF:
    case SOURCE_FUNCTION_MODULE_BUSS_29_30_OFF:
    case SOURCE_FUNCTION_MODULE_BUSS_31_32_OFF:
    {
      int BussNr = (FunctionNr-SOURCE_FUNCTION_MODULE_BUSS_1_2_OFF)/(SOURCE_FUNCTION_MODULE_BUSS_3_4_OFF-SOURCE_FUNCTION_MODULE_BUSS_1_2_OFF);

      Active = 0;
      for (int cntModule=0; cntModule<128; cntModule++)
      {
        if (AxumData.ModuleData[cntModule].Source == (SourceNr+matrix_sources.src_offset.min.source))
        {
          if (AxumData.ModuleData[cntModule].Buss[BussNr].On)
          {
            Active = 1;
          }
        }
      }

      TransmitData[0] = (ObjectNr>>8)&0xFF;
      TransmitData[1] = ObjectNr&0xFF;
      TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
      TransmitData[3] = STATE_DATATYPE;
      TransmitData[4] = 1;
      TransmitData[5] = !Active;

      SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
    }
    break;
    case SOURCE_FUNCTION_MODULE_BUSS_1_2_ON_OFF:
    case SOURCE_FUNCTION_MODULE_BUSS_3_4_ON_OFF:
    case SOURCE_FUNCTION_MODULE_BUSS_5_6_ON_OFF:
    case SOURCE_FUNCTION_MODULE_BUSS_7_8_ON_OFF:
    case SOURCE_FUNCTION_MODULE_BUSS_9_10_ON_OFF:
    case SOURCE_FUNCTION_MODULE_BUSS_11_12_ON_OFF:
    case SOURCE_FUNCTION_MODULE_BUSS_13_14_ON_OFF:
    case SOURCE_FUNCTION_MODULE_BUSS_15_16_ON_OFF:
    case SOURCE_FUNCTION_MODULE_BUSS_17_18_ON_OFF:
    case SOURCE_FUNCTION_MODULE_BUSS_19_20_ON_OFF:
    case SOURCE_FUNCTION_MODULE_BUSS_21_22_ON_OFF:
    case SOURCE_FUNCTION_MODULE_BUSS_23_24_ON_OFF:
    case SOURCE_FUNCTION_MODULE_BUSS_25_26_ON_OFF:
    case SOURCE_FUNCTION_MODULE_BUSS_27_28_ON_OFF:
    case SOURCE_FUNCTION_MODULE_BUSS_29_30_ON_OFF:
    case SOURCE_FUNCTION_MODULE_BUSS_31_32_ON_OFF:
    {
      int BussNr = (FunctionNr-SOURCE_FUNCTION_MODULE_BUSS_1_2_ON_OFF)/(SOURCE_FUNCTION_MODULE_BUSS_3_4_ON_OFF-SOURCE_FUNCTION_MODULE_BUSS_1_2_ON_OFF);

      Active = 0;
      for (int cntModule=0; cntModule<128; cntModule++)
      {
        if (AxumData.ModuleData[cntModule].Source == (SourceNr+matrix_sources.src_offset.min.source))
        {
          if (AxumData.ModuleData[cntModule].Buss[BussNr].On)
          {
            Active = 1;
          }
        }
      }
      printf("Func:%d, Active=%d\n", FunctionNr, Active);

      TransmitData[0] = (ObjectNr>>8)&0xFF;
      TransmitData[1] = ObjectNr&0xFF;
      TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
      TransmitData[3] = STATE_DATATYPE;
      TransmitData[4] = 1;
      TransmitData[5] = Active;

      SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
    }
    break;
    case SOURCE_FUNCTION_MODULE_COUGH_ON_OFF:
    {
      Active = 0;
      for (int cntModule=0; cntModule<128; cntModule++)
      {
        if (AxumData.ModuleData[cntModule].Source == (SourceNr+matrix_sources.src_offset.min.source))
        {
          if (AxumData.ModuleData[cntModule].Cough)
          {
            Active = 1;
          }
        }
      }

      TransmitData[0] = (ObjectNr>>8)&0xFF;
      TransmitData[1] = ObjectNr&0xFF;
      TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
      TransmitData[3] = STATE_DATATYPE;
      TransmitData[4] = 1;
      TransmitData[5] = Active;

      SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
    }
    break;
    case SOURCE_FUNCTION_START:
    case SOURCE_FUNCTION_STOP:
    case SOURCE_FUNCTION_START_STOP:
    {
      TransmitData[0] = (ObjectNr>>8)&0xFF;
      TransmitData[1] = ObjectNr&0xFF;
      TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
      TransmitData[3] = STATE_DATATYPE;
      TransmitData[4] = 1;
      if (FunctionNr == SOURCE_FUNCTION_STOP)
      {
        TransmitData[5] = !AxumData.SourceData[SourceNr].Start;
      }
      else
      {
        TransmitData[5] = AxumData.SourceData[SourceNr].Start;
      }

      SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
    }
    break;
    case SOURCE_FUNCTION_PHANTOM:
    {
      TransmitData[0] = (ObjectNr>>8)&0xFF;
      TransmitData[1] = ObjectNr&0xFF;
      TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
      TransmitData[3] = STATE_DATATYPE;
      TransmitData[4] = 1;
      TransmitData[5] = AxumData.SourceData[SourceNr].Phantom;

      SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
    }
    break;
    case SOURCE_FUNCTION_PAD:
    {
      TransmitData[0] = (ObjectNr>>8)&0xFF;
      TransmitData[1] = ObjectNr&0xFF;
      TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
      TransmitData[3] = STATE_DATATYPE;
      TransmitData[4] = 1;
      TransmitData[5] = AxumData.SourceData[SourceNr].Pad;

      SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
    }
    break;
    case SOURCE_FUNCTION_GAIN:
    {
      switch (DataType)
      {
      case FLOAT_DATATYPE:
      {
        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = FLOAT_DATATYPE;
        TransmitData[4] = 2;

        float Level = AxumData.SourceData[SourceNr].Gain;
        if (Level<DataMinimal)
        {
          Level = DataMinimal;
        }
        else if (Level>DataMaximal)
        {
          Level = DataMaximal;
        }

        Float2VariableFloat(Level, 2, &TransmitData[5]);

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 7);
      }
      break;
      }
    }
    break;
    case SOURCE_FUNCTION_ALERT:
    {
      TransmitData[0] = (ObjectNr>>8)&0xFF;
      TransmitData[1] = ObjectNr&0xFF;
      TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
      TransmitData[3] = STATE_DATATYPE;
      TransmitData[4] = 1;
      TransmitData[5] = AxumData.SourceData[SourceNr].Alert;

      SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
    }
    break;
    }
  }
  break;
  case DESTINATION_FUNCTIONS:
  {   //Destination
    unsigned int DestinationNr = ((SensorReceiveFunctionNumber>>12)&0xFFF);
    unsigned int FunctionNr = SensorReceiveFunctionNumber&0xFFF;
    unsigned int Active = 0;
    unsigned char TransmitData[32];

    switch (FunctionNr)
    {
    case DESTINATION_FUNCTION_LABEL:
    {
      switch (DataType)
      {
      case OCTET_STRING_DATATYPE:
      {
        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = OCTET_STRING_DATATYPE;
        TransmitData[4] = 8;
        TransmitData[5] = AxumData.DestinationData[DestinationNr].DestinationName[0];
        TransmitData[6] = AxumData.DestinationData[DestinationNr].DestinationName[1];
        TransmitData[7] = AxumData.DestinationData[DestinationNr].DestinationName[2];
        TransmitData[8] = AxumData.DestinationData[DestinationNr].DestinationName[3];
        TransmitData[9] = AxumData.DestinationData[DestinationNr].DestinationName[4];
        TransmitData[10] = AxumData.DestinationData[DestinationNr].DestinationName[5];
        TransmitData[11] = AxumData.DestinationData[DestinationNr].DestinationName[6];
        TransmitData[12] = AxumData.DestinationData[DestinationNr].DestinationName[7];

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 13);
      }
      }
    }
    break;
    case DESTINATION_FUNCTION_SOURCE:
    {
      switch (DataType)
      {
        case OCTET_STRING_DATATYPE:
        {
          char LCDText[9];

          GetSourceLabel(AxumData.DestinationData[DestinationNr].Source, LCDText, 8);

          TransmitData[0] = (ObjectNr>>8)&0xFF;
          TransmitData[1] = ObjectNr&0xFF;
          TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
          TransmitData[3] = OCTET_STRING_DATATYPE;
          TransmitData[4] = 8;
          TransmitData[5] = LCDText[0];
          TransmitData[6] = LCDText[1];
          TransmitData[7] = LCDText[2];
          TransmitData[8] = LCDText[3];
          TransmitData[9] = LCDText[4];
          TransmitData[10] = LCDText[5];
          TransmitData[11] = LCDText[6];
          TransmitData[12] = LCDText[7];

          SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 13);
        }
        break;
      }
    }
    break;
    case DESTINATION_FUNCTION_MONITOR_SPEAKER_LEVEL:
    {
      if ((AxumData.DestinationData[DestinationNr].Source>=matrix_sources.src_offset.min.monitor_buss) && (AxumData.DestinationData[DestinationNr].Source<=matrix_sources.src_offset.max.monitor_buss))
      {
        int MonitorBussNr = AxumData.DestinationData[DestinationNr].Source-matrix_sources.src_offset.min.monitor_buss;

        switch (DataType)
        {
          case UNSIGNED_INTEGER_DATATYPE:
          {
            int dB = (AxumData.Monitor[MonitorBussNr].SpeakerLevel*10)+1400;
            if (dB<0)
            {
              dB = 0;
            }
            else if (dB>=1500)
            {
              dB = 1499;
            }
            int Position = dB2Position[dB];
            Position = ((dB2Position[dB]*(DataMaximal-DataMinimal))/1023)+DataMinimal;
            if (Position<DataMinimal)
            {
              Position = DataMinimal;
            }
            else if (Position>DataMaximal)
            {
              Position = DataMaximal;
            }

            int cntTransmitData = 0;
            TransmitData[cntTransmitData++] = (ObjectNr>>8)&0xFF;
            TransmitData[cntTransmitData++] = ObjectNr&0xFF;
            TransmitData[cntTransmitData++] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
            TransmitData[cntTransmitData++] = UNSIGNED_INTEGER_DATATYPE;
            TransmitData[cntTransmitData++] = DataSize;
            for (int cntByte=0; cntByte<DataSize; cntByte++)
            {
              char BitsToShift = ((DataSize-1)-cntByte)*8;
              TransmitData[cntTransmitData++] = (Position>>BitsToShift)&0xFF;
            }
            SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, cntTransmitData);
          }
          break;
          case OCTET_STRING_DATATYPE:
          {
            char LCDText[9];
            sprintf(LCDText, " %4.0f dB", AxumData.Monitor[MonitorBussNr].SpeakerLevel);

            TransmitData[0] = (ObjectNr>>8)&0xFF;
            TransmitData[1] = ObjectNr&0xFF;
            TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
            TransmitData[3] = OCTET_STRING_DATATYPE;
            TransmitData[4] = 8;
            TransmitData[5] = LCDText[0];
            TransmitData[6] = LCDText[1];
            TransmitData[7] = LCDText[2];
            TransmitData[8] = LCDText[3];
            TransmitData[9] = LCDText[4];
            TransmitData[10] = LCDText[5];
            TransmitData[11] = LCDText[6];
            TransmitData[12] = LCDText[7];

            SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 13);
          }
          break;
          case FLOAT_DATATYPE:
          {
            TransmitData[0] = (ObjectNr>>8)&0xFF;
            TransmitData[1] = ObjectNr&0xFF;
            TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
            TransmitData[3] = FLOAT_DATATYPE;
            TransmitData[4] = 2;

            float Level = AxumData.Monitor[MonitorBussNr].SpeakerLevel;
            if (Level<DataMinimal)
            {
              Level = DataMinimal;
            }
            else if (Level>DataMaximal)
            {
              Level = DataMaximal;
            }
            Float2VariableFloat(Level, 2, &TransmitData[5]);

            SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 7);
          }
          break;
        }
      }
    }
    break;
    case DESTINATION_FUNCTION_MONITOR_PHONES_LEVEL:
    {
      if ((AxumData.DestinationData[DestinationNr].Source>=matrix_sources.src_offset.min.monitor_buss) && (AxumData.DestinationData[DestinationNr].Source<=matrix_sources.src_offset.max.monitor_buss))
      {
        int MonitorBussNr = AxumData.DestinationData[DestinationNr].Source-matrix_sources.src_offset.min.monitor_buss;

        switch (DataType)
        {
          case UNSIGNED_INTEGER_DATATYPE:
          {
            int dB = 0;
            dB = ((AxumData.Monitor[MonitorBussNr].PhonesLevel-20)*10)+1400;

            if (dB<0)
            {
              dB = 0;
            }
            else if (dB>=1500)
            {
              dB = 1499;
            }
            int Position = dB2Position[dB];
            Position = ((dB2Position[dB]*(DataMaximal-DataMinimal))/1023)+DataMinimal;
            if (Position<DataMinimal)
            {
              Position = DataMinimal;
            }
            else if (Position>DataMaximal)
            {
              Position = DataMaximal;
            }

            int cntTransmitData = 0;
            TransmitData[cntTransmitData++] = (ObjectNr>>8)&0xFF;
            TransmitData[cntTransmitData++] = ObjectNr&0xFF;
            TransmitData[cntTransmitData++] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
            TransmitData[cntTransmitData++] = UNSIGNED_INTEGER_DATATYPE;
            TransmitData[cntTransmitData++] = DataSize;
            for (int cntByte=0; cntByte<DataSize; cntByte++)
            {
              char BitsToShift = ((DataSize-1)-cntByte)*8;
              TransmitData[cntTransmitData++] = (Position>>BitsToShift)&0xFF;
            }
            SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, cntTransmitData);
          }
          break;
          case OCTET_STRING_DATATYPE:
          {
            char LCDText[9];
            sprintf(LCDText, " %4.0f dB", AxumData.Monitor[MonitorBussNr].PhonesLevel);

            TransmitData[0] = (ObjectNr>>8)&0xFF;
            TransmitData[1] = ObjectNr&0xFF;
            TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
            TransmitData[3] = OCTET_STRING_DATATYPE;
            TransmitData[4] = 8;
            TransmitData[5] = LCDText[0];
            TransmitData[6] = LCDText[1];
            TransmitData[7] = LCDText[2];
            TransmitData[8] = LCDText[3];
            TransmitData[9] = LCDText[4];
            TransmitData[10] = LCDText[5];
            TransmitData[11] = LCDText[6];
            TransmitData[12] = LCDText[7];

            SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 13);
          }
          break;
          case FLOAT_DATATYPE:
          {
            TransmitData[0] = (ObjectNr>>8)&0xFF;
            TransmitData[1] = ObjectNr&0xFF;
            TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
            TransmitData[3] = FLOAT_DATATYPE;
            TransmitData[4] = 2;

            float Level = AxumData.Monitor[MonitorBussNr].PhonesLevel;
            if (Level<DataMinimal)
            {
              Level = DataMinimal;
            }
            else if (Level>DataMaximal)
            {
              Level = DataMaximal;
            }
            Float2VariableFloat(Level, 2, &TransmitData[5]);

            SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 7);
          }
          break;
        }
      }
    }
    break;
    case DESTINATION_FUNCTION_LEVEL:
    {
      switch (DataType)
      {
      case UNSIGNED_INTEGER_DATATYPE:
      {
        int dB = 0;
        dB = ((AxumData.DestinationData[DestinationNr].Level)*10)+1400;

        if (dB<0)
        {
          dB = 0;
        }
        else if (dB>=1500)
        {
          dB = 1499;
        }
        int Position = dB2Position[dB];
        Position = ((dB2Position[dB]*(DataMaximal-DataMinimal))/1023)+DataMinimal;
        if (Position<DataMinimal)
        {
          Position = DataMinimal;
        }
        else if (Position>DataMaximal)
        {
          Position = DataMaximal;
        }

        int cntTransmitData = 0;
        TransmitData[cntTransmitData++] = (ObjectNr>>8)&0xFF;
        TransmitData[cntTransmitData++] = ObjectNr&0xFF;
        TransmitData[cntTransmitData++] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[cntTransmitData++] = UNSIGNED_INTEGER_DATATYPE;
        TransmitData[cntTransmitData++] = DataSize;
        for (int cntByte=0; cntByte<DataSize; cntByte++)
        {
          char BitsToShift = ((DataSize-1)-cntByte)*8;
          TransmitData[cntTransmitData++] = (Position>>BitsToShift)&0xFF;
        }
        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, cntTransmitData);
      }
      break;
      case OCTET_STRING_DATATYPE:
      {
        char LCDText[9];
        sprintf(LCDText, " %4.0f dB", AxumData.DestinationData[DestinationNr].Level);

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = OCTET_STRING_DATATYPE;
        TransmitData[4] = 8;
        TransmitData[5] = LCDText[0];
        TransmitData[6] = LCDText[1];
        TransmitData[7] = LCDText[2];
        TransmitData[8] = LCDText[3];
        TransmitData[9] = LCDText[4];
        TransmitData[10] = LCDText[5];
        TransmitData[11] = LCDText[6];
        TransmitData[12] = LCDText[7];

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 13);
      }
      break;
      case FLOAT_DATATYPE:
      {
        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = FLOAT_DATATYPE;
        TransmitData[4] = 2;

        float Level = AxumData.DestinationData[DestinationNr].Level;
        if (Level<DataMinimal)
        {
          Level = DataMinimal;
        }
        else if (Level>DataMaximal)
        {
          Level = DataMaximal;
        }
        Float2VariableFloat(Level, 2, &TransmitData[5]);

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 7);
      }
      break;
      }
    }
    break;
    case DESTINATION_FUNCTION_MUTE:
    {
      switch (DataType)
      {
      case STATE_DATATYPE:
      {
        Active = AxumData.DestinationData[DestinationNr].Mute;

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = STATE_DATATYPE;
        TransmitData[4] = 1;
        TransmitData[5] = Active;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
      }
      break;
      }
    }
    break;
    case DESTINATION_FUNCTION_MUTE_AND_MONITOR_MUTE:
    {
      switch (DataType)
      {
      case STATE_DATATYPE:
      {
        Active = AxumData.DestinationData[DestinationNr].Mute;

        if ((AxumData.DestinationData[DestinationNr].Source>=matrix_sources.src_offset.min.monitor_buss) && (AxumData.DestinationData[DestinationNr].Source<=matrix_sources.src_offset.max.monitor_buss))
        {
          int MonitorBussNr = AxumData.DestinationData[DestinationNr].Source-matrix_sources.src_offset.min.monitor_buss;
          if (AxumData.Monitor[MonitorBussNr].Mute)
          {
            Active = 1;
          }
        }

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = STATE_DATATYPE;
        TransmitData[4] = 1;
        TransmitData[5] = Active;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
      }
      break;
      }
    }
    break;
    case DESTINATION_FUNCTION_DIM:
    {
      switch (DataType)
      {
      case STATE_DATATYPE:
      {
        Active = AxumData.DestinationData[DestinationNr].Dim;

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = STATE_DATATYPE;
        TransmitData[4] = 1;
        TransmitData[5] = Active;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
      }
      break;
      }
    }
    break;
    case DESTINATION_FUNCTION_DIM_AND_MONITOR_DIM:
    {
      switch (DataType)
      {
      case STATE_DATATYPE:
      {
        Active = AxumData.DestinationData[DestinationNr].Dim;

        if ((AxumData.DestinationData[DestinationNr].Source>=matrix_sources.src_offset.min.monitor_buss) && (AxumData.DestinationData[DestinationNr].Source<=matrix_sources.src_offset.max.monitor_buss))
        {
          int MonitorBussNr = AxumData.DestinationData[DestinationNr].Source-matrix_sources.src_offset.min.monitor_buss;
          if (AxumData.Monitor[MonitorBussNr].Dim)
          {
            Active = 1;
          }
        }

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = STATE_DATATYPE;
        TransmitData[4] = 1;
        TransmitData[5] = Active;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
      }
      break;
      }
    }
    break;
    case DESTINATION_FUNCTION_MONO:
    {
      switch (DataType)
      {
      case STATE_DATATYPE:
      {
        Active = AxumData.DestinationData[DestinationNr].Mono;

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = STATE_DATATYPE;
        TransmitData[4] = 1;
        TransmitData[5] = Active;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
      }
      break;
      }
    }
    break;
    case DESTINATION_FUNCTION_MONO_AND_MONITOR_MONO:
    {
      switch (DataType)
      {
      case STATE_DATATYPE:
      {
        Active = AxumData.DestinationData[DestinationNr].Mono;

        if ((AxumData.DestinationData[DestinationNr].Source>=matrix_sources.src_offset.min.monitor_buss) && (AxumData.DestinationData[DestinationNr].Source<=matrix_sources.src_offset.max.monitor_buss))
        {
          int MonitorBussNr = AxumData.DestinationData[DestinationNr].Source-matrix_sources.src_offset.min.monitor_buss;
          if (AxumData.Monitor[MonitorBussNr].Mono)
          {
            Active = 1;
          }
        }

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = STATE_DATATYPE;
        TransmitData[4] = 1;
        TransmitData[5] = Active;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
      }
      break;
      }
    }
    break;
    case DESTINATION_FUNCTION_PHASE:
    {
      switch (DataType)
      {
      case STATE_DATATYPE:
      {
        Active = AxumData.DestinationData[DestinationNr].Phase;

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = STATE_DATATYPE;
        TransmitData[4] = 1;
        TransmitData[5] = Active;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
      }
      break;
      }
    }
    break;
    case DESTINATION_FUNCTION_PHASE_AND_MONITOR_PHASE:
    {
      switch (DataType)
      {
      case STATE_DATATYPE:
      {
        Active = AxumData.DestinationData[DestinationNr].Phase;

        if ((AxumData.DestinationData[DestinationNr].Source>=matrix_sources.src_offset.min.monitor_buss) && (AxumData.DestinationData[DestinationNr].Source<=matrix_sources.src_offset.max.monitor_buss))
        {
          int MonitorBussNr = AxumData.DestinationData[DestinationNr].Source-matrix_sources.src_offset.min.monitor_buss;
          if (AxumData.Monitor[MonitorBussNr].Phase)
          {
            Active = 1;
          }
        }

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = STATE_DATATYPE;
        TransmitData[4] = 1;
        TransmitData[5] = Active;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
      }
      break;
      }
    }
    break;
    case DESTINATION_FUNCTION_TALKBACK_1:
    case DESTINATION_FUNCTION_TALKBACK_2:
    case DESTINATION_FUNCTION_TALKBACK_3:
    case DESTINATION_FUNCTION_TALKBACK_4:
    case DESTINATION_FUNCTION_TALKBACK_5:
    case DESTINATION_FUNCTION_TALKBACK_6:
    case DESTINATION_FUNCTION_TALKBACK_7:
    case DESTINATION_FUNCTION_TALKBACK_8:
    case DESTINATION_FUNCTION_TALKBACK_9:
    case DESTINATION_FUNCTION_TALKBACK_10:
    case DESTINATION_FUNCTION_TALKBACK_11:
    case DESTINATION_FUNCTION_TALKBACK_12:
    case DESTINATION_FUNCTION_TALKBACK_13:
    case DESTINATION_FUNCTION_TALKBACK_14:
    case DESTINATION_FUNCTION_TALKBACK_15:
    case DESTINATION_FUNCTION_TALKBACK_16:
    {
      int TalkbackNr = (FunctionNr-DESTINATION_FUNCTION_TALKBACK_1)/(DESTINATION_FUNCTION_TALKBACK_2-DESTINATION_FUNCTION_TALKBACK_1);
      switch (DataType)
      {
      case STATE_DATATYPE:
      {
        Active = AxumData.DestinationData[DestinationNr].Talkback[TalkbackNr];

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = STATE_DATATYPE;
        TransmitData[4] = 1;
        TransmitData[5] = Active;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
      }
      break;
      }
    }
    break;
    case DESTINATION_FUNCTION_TALKBACK_1_AND_MONITOR_TALKBACK_1:
    case DESTINATION_FUNCTION_TALKBACK_2_AND_MONITOR_TALKBACK_2:
    case DESTINATION_FUNCTION_TALKBACK_3_AND_MONITOR_TALKBACK_3:
    case DESTINATION_FUNCTION_TALKBACK_4_AND_MONITOR_TALKBACK_4:
    case DESTINATION_FUNCTION_TALKBACK_5_AND_MONITOR_TALKBACK_5:
    case DESTINATION_FUNCTION_TALKBACK_6_AND_MONITOR_TALKBACK_6:
    case DESTINATION_FUNCTION_TALKBACK_7_AND_MONITOR_TALKBACK_7:
    case DESTINATION_FUNCTION_TALKBACK_8_AND_MONITOR_TALKBACK_8:
    case DESTINATION_FUNCTION_TALKBACK_9_AND_MONITOR_TALKBACK_9:
    case DESTINATION_FUNCTION_TALKBACK_10_AND_MONITOR_TALKBACK_10:
    case DESTINATION_FUNCTION_TALKBACK_11_AND_MONITOR_TALKBACK_11:
    case DESTINATION_FUNCTION_TALKBACK_12_AND_MONITOR_TALKBACK_12:
    case DESTINATION_FUNCTION_TALKBACK_13_AND_MONITOR_TALKBACK_13:
    case DESTINATION_FUNCTION_TALKBACK_14_AND_MONITOR_TALKBACK_14:
    case DESTINATION_FUNCTION_TALKBACK_15_AND_MONITOR_TALKBACK_15:
    case DESTINATION_FUNCTION_TALKBACK_16_AND_MONITOR_TALKBACK_16:
    {
      int TalkbackNr = (FunctionNr-DESTINATION_FUNCTION_TALKBACK_1_AND_MONITOR_TALKBACK_1)/(DESTINATION_FUNCTION_TALKBACK_2_AND_MONITOR_TALKBACK_2-DESTINATION_FUNCTION_TALKBACK_1_AND_MONITOR_TALKBACK_1);

      switch (DataType)
      {
      case STATE_DATATYPE:
      {
        Active = AxumData.DestinationData[DestinationNr].Talkback[TalkbackNr];

        if ((AxumData.DestinationData[DestinationNr].Source>=matrix_sources.src_offset.min.monitor_buss) && (AxumData.DestinationData[DestinationNr].Source<=matrix_sources.src_offset.max.monitor_buss))
        {
          int MonitorBussNr = AxumData.DestinationData[DestinationNr].Source-matrix_sources.src_offset.min.monitor_buss;
          if (AxumData.Monitor[MonitorBussNr].Talkback[TalkbackNr])
          {
            Active = 1;
          }
        }

        TransmitData[0] = (ObjectNr>>8)&0xFF;
        TransmitData[1] = ObjectNr&0xFF;
        TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
        TransmitData[3] = STATE_DATATYPE;
        TransmitData[4] = 1;
        TransmitData[5] = Active;

        SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
      }
      break;
      }
    }
    break;
    }
  }
  break;
  }
}


//Initialize object list per function
void InitalizeAllObjectListPerFunction()
{
  //Module
  for (int cntModule=0; cntModule<NUMBER_OF_MODULES; cntModule++)
  {
    for (int cntFunction=0; cntFunction<NUMBER_OF_MODULE_FUNCTIONS; cntFunction++)
    {
      ModuleFunctions[cntModule][cntFunction] = NULL;
    }
  }

  //Buss
  for (int cntBuss=0; cntBuss<NUMBER_OF_BUSSES; cntBuss++)
  {
    for (int cntFunction=0; cntFunction<NUMBER_OF_BUSS_FUNCTIONS; cntFunction++)
    {
      BussFunctions[cntBuss][cntFunction] = NULL;
    }
  }

  //Monitor Buss
  for (int cntBuss=0; cntBuss<NUMBER_OF_MONITOR_BUSSES; cntBuss++)
  {
    for (int cntFunction=0; cntFunction<NUMBER_OF_MONITOR_BUSS_FUNCTIONS; cntFunction++)
    {
      MonitorBussFunctions[cntBuss][cntFunction] = NULL;
    }
  }

  //Global
  for (int cntFunction=0; cntFunction<NUMBER_OF_GLOBAL_FUNCTIONS; cntFunction++)
  {
    GlobalFunctions[cntFunction] = NULL;
  }

  //Source
  for (int cntSource=0; cntSource<NUMBER_OF_SOURCES; cntSource++)
  {
    for (int cntFunction=0; cntFunction<NUMBER_OF_SOURCE_FUNCTIONS; cntFunction++)
    {
      SourceFunctions[cntSource][cntFunction] = NULL;
    }
  }

  //Destination
  for (int cntDestination=0; cntDestination<NUMBER_OF_DESTINATIONS; cntDestination++)
  {
    for (int cntFunction=0; cntFunction<NUMBER_OF_DESTINATION_FUNCTIONS; cntFunction++)
    {
      DestinationFunctions[cntDestination][cntFunction] = NULL;
    }
  }
}

//Make object list per functions
void MakeObjectListPerFunction(unsigned int SensorReceiveFunctionNumber)
{
  unsigned char FunctionType = (SensorReceiveFunctionNumber>>24)&0xFF;
  unsigned int FunctionNumber = (SensorReceiveFunctionNumber>>12)&0xFFF;
  unsigned int Function = SensorReceiveFunctionNumber&0xFFF;
  AXUM_FUNCTION_INFORMATION_STRUCT *WalkAxumFunctionInformationStruct = NULL;

  //Clear function list
  switch (FunctionType)
  {
  case MODULE_FUNCTIONS:
  {   //Module
    WalkAxumFunctionInformationStruct = ModuleFunctions[FunctionNumber][Function];
    ModuleFunctions[FunctionNumber][Function] = NULL;
  }
  break;
  case BUSS_FUNCTIONS:
  {   //Buss
    WalkAxumFunctionInformationStruct = BussFunctions[FunctionNumber][Function];
    BussFunctions[FunctionNumber][Function] = NULL;
  }
  break;
  case MONITOR_BUSS_FUNCTIONS:
  {   //Monitor Buss
    WalkAxumFunctionInformationStruct = MonitorBussFunctions[FunctionNumber][Function];
    MonitorBussFunctions[FunctionNumber][Function] = NULL;
  }
  break;
  case GLOBAL_FUNCTIONS:
  {   //Global
    WalkAxumFunctionInformationStruct = GlobalFunctions[Function];
    GlobalFunctions[Function] = NULL;
  }
  break;
  case SOURCE_FUNCTIONS:
  {   //Source
    WalkAxumFunctionInformationStruct = SourceFunctions[FunctionNumber][Function];
    SourceFunctions[FunctionNumber][Function] = NULL;
  }
  break;
  case DESTINATION_FUNCTIONS:
  {   //Destination
    WalkAxumFunctionInformationStruct = DestinationFunctions[FunctionNumber][Function];
    DestinationFunctions[FunctionNumber][Function] = NULL;
  }
  break;
  }
  while (WalkAxumFunctionInformationStruct != NULL)
  {
    AXUM_FUNCTION_INFORMATION_STRUCT *AxumFunctionInformationStructToDelete = WalkAxumFunctionInformationStruct;
    WalkAxumFunctionInformationStruct = (AXUM_FUNCTION_INFORMATION_STRUCT *)WalkAxumFunctionInformationStruct->Next;
    delete AxumFunctionInformationStructToDelete;
  }

  for (unsigned int cntNodes=0; cntNodes<AddressTableCount; cntNodes++)
  {
    if (OnlineNodeInformation[cntNodes].MambaNetAddress != 0)
    {
      if (OnlineNodeInformation[cntNodes].SensorReceiveFunction != NULL)
      {
        for (int cntObject=0; cntObject<OnlineNodeInformation[cntNodes].NumberOfCustomObjects; cntObject++)
        {
          if (OnlineNodeInformation[cntNodes].SensorReceiveFunction[cntObject].FunctionNr == SensorReceiveFunctionNumber)
          {
            if (OnlineNodeInformation[cntNodes].ObjectInformation[cntObject].ActuatorDataType != NO_DATA_DATATYPE)
            {
              AXUM_FUNCTION_INFORMATION_STRUCT *AxumFunctionInformationStructToAdd = new AXUM_FUNCTION_INFORMATION_STRUCT;

              AxumFunctionInformationStructToAdd->MambaNetAddress = OnlineNodeInformation[cntNodes].MambaNetAddress;
              AxumFunctionInformationStructToAdd->ObjectNr = 1024+cntObject;
              AxumFunctionInformationStructToAdd->ActuatorDataType = OnlineNodeInformation[cntNodes].ObjectInformation[cntObject].ActuatorDataType;
              AxumFunctionInformationStructToAdd->ActuatorDataSize = OnlineNodeInformation[cntNodes].ObjectInformation[cntObject].ActuatorDataSize;
              AxumFunctionInformationStructToAdd->ActuatorDataMinimal = OnlineNodeInformation[cntNodes].ObjectInformation[cntObject].ActuatorDataMinimal;
              AxumFunctionInformationStructToAdd->ActuatorDataMaximal = OnlineNodeInformation[cntNodes].ObjectInformation[cntObject].ActuatorDataMaximal;
              AxumFunctionInformationStructToAdd->Next = (void *)WalkAxumFunctionInformationStruct;

              switch (FunctionType)
              {
              case MODULE_FUNCTIONS:
              {   //Module
                ModuleFunctions[FunctionNumber][Function] = AxumFunctionInformationStructToAdd;
              }
              break;
              case BUSS_FUNCTIONS:
              {   //Buss
                BussFunctions[FunctionNumber][Function] = AxumFunctionInformationStructToAdd;
              }
              break;
              case MONITOR_BUSS_FUNCTIONS:
              {   //Monitor Buss
                MonitorBussFunctions[FunctionNumber][Function] = AxumFunctionInformationStructToAdd;
              }
              break;
              case GLOBAL_FUNCTIONS:
              {   //Global
                GlobalFunctions[Function] = AxumFunctionInformationStructToAdd;
              }
              break;
              case SOURCE_FUNCTIONS:
              {   //Source
                SourceFunctions[FunctionNumber][Function] = AxumFunctionInformationStructToAdd;
              }
              break;
              case DESTINATION_FUNCTIONS:
              {   //Output
                DestinationFunctions[FunctionNumber][Function] = AxumFunctionInformationStructToAdd;
              }
              break;
              }
              WalkAxumFunctionInformationStruct = AxumFunctionInformationStructToAdd;
            }
          }
        }
      }
    }
  }

  //Debug print the function list
  WalkAxumFunctionInformationStruct = NULL;
  switch (FunctionType)
  {
  case MODULE_FUNCTIONS:
  {   //Module
    WalkAxumFunctionInformationStruct = ModuleFunctions[FunctionNumber][Function];
  }
  break;
  case BUSS_FUNCTIONS:
  {   //Buss
    WalkAxumFunctionInformationStruct = BussFunctions[FunctionNumber][Function];
  }
  break;
  case MONITOR_BUSS_FUNCTIONS:
  {   //Monitor Buss
    WalkAxumFunctionInformationStruct = MonitorBussFunctions[FunctionNumber][Function];
  }
  break;
  case GLOBAL_FUNCTIONS:
  {   //Global
    WalkAxumFunctionInformationStruct = GlobalFunctions[Function];
  }
  break;
  case SOURCE_FUNCTIONS:
  {   //Source
    WalkAxumFunctionInformationStruct = SourceFunctions[FunctionNumber][Function];
  }
  break;
  case DESTINATION_FUNCTIONS:
  {   //Destination
    WalkAxumFunctionInformationStruct = DestinationFunctions[FunctionNumber][Function];
  }
  break;
  }
  while (WalkAxumFunctionInformationStruct != NULL)
  {
    WalkAxumFunctionInformationStruct = (AXUM_FUNCTION_INFORMATION_STRUCT *)WalkAxumFunctionInformationStruct->Next;
  }
}

//Delete all object list per function
void DeleteAllObjectListPerFunction()
{
  //Module
  for (int cntModule=0; cntModule<NUMBER_OF_MODULES; cntModule++)
  {
    for (int cntFunction=0; cntFunction<NUMBER_OF_MODULE_FUNCTIONS; cntFunction++)
    {
      AXUM_FUNCTION_INFORMATION_STRUCT *WalkAxumFunctionInformationStruct = ModuleFunctions[cntModule][cntFunction];
      ModuleFunctions[cntModule][cntFunction] = NULL;

      while (WalkAxumFunctionInformationStruct != NULL)
      {
        AXUM_FUNCTION_INFORMATION_STRUCT *AxumFunctionInformationStructToDelete = WalkAxumFunctionInformationStruct;
        WalkAxumFunctionInformationStruct = (AXUM_FUNCTION_INFORMATION_STRUCT *)WalkAxumFunctionInformationStruct->Next;
        delete AxumFunctionInformationStructToDelete;
      }
    }
  }

  //Buss
  for (int cntBuss=0; cntBuss<NUMBER_OF_BUSSES; cntBuss++)
  {
    for (int cntFunction=0; cntFunction<NUMBER_OF_BUSS_FUNCTIONS; cntFunction++)
    {
      AXUM_FUNCTION_INFORMATION_STRUCT *WalkAxumFunctionInformationStruct = BussFunctions[cntBuss][cntFunction];
      BussFunctions[cntBuss][cntFunction] = NULL;

      while (WalkAxumFunctionInformationStruct != NULL)
      {
        AXUM_FUNCTION_INFORMATION_STRUCT *AxumFunctionInformationStructToDelete = WalkAxumFunctionInformationStruct;
        WalkAxumFunctionInformationStruct = (AXUM_FUNCTION_INFORMATION_STRUCT *)WalkAxumFunctionInformationStruct->Next;
        delete AxumFunctionInformationStructToDelete;
      }
    }
  }

  //Monitor Buss
  for (int cntBuss=0; cntBuss<NUMBER_OF_MONITOR_BUSSES; cntBuss++)
  {
    for (int cntFunction=0; cntFunction<NUMBER_OF_MONITOR_BUSS_FUNCTIONS; cntFunction++)
    {
      AXUM_FUNCTION_INFORMATION_STRUCT *WalkAxumFunctionInformationStruct = MonitorBussFunctions[cntBuss][cntFunction];
      MonitorBussFunctions[cntBuss][cntFunction] = NULL;

      while (WalkAxumFunctionInformationStruct != NULL)
      {
        AXUM_FUNCTION_INFORMATION_STRUCT *AxumFunctionInformationStructToDelete = WalkAxumFunctionInformationStruct;
        WalkAxumFunctionInformationStruct = (AXUM_FUNCTION_INFORMATION_STRUCT *)WalkAxumFunctionInformationStruct->Next;
        delete AxumFunctionInformationStructToDelete;
      }
    }
  }

  //Global
  for (int cntFunction=0; cntFunction<NUMBER_OF_GLOBAL_FUNCTIONS; cntFunction++)
  {
    AXUM_FUNCTION_INFORMATION_STRUCT *WalkAxumFunctionInformationStruct = GlobalFunctions[cntFunction];
    GlobalFunctions[cntFunction] = NULL;

    while (WalkAxumFunctionInformationStruct != NULL)
    {
      AXUM_FUNCTION_INFORMATION_STRUCT *AxumFunctionInformationStructToDelete = WalkAxumFunctionInformationStruct;
      WalkAxumFunctionInformationStruct = (AXUM_FUNCTION_INFORMATION_STRUCT *)WalkAxumFunctionInformationStruct->Next;
      delete AxumFunctionInformationStructToDelete;
    }
  }

  //Source
  for (int cntSource=0; cntSource<NUMBER_OF_SOURCES; cntSource++)
  {
    for (int cntFunction=0; cntFunction<NUMBER_OF_SOURCE_FUNCTIONS; cntFunction++)
    {
      AXUM_FUNCTION_INFORMATION_STRUCT *WalkAxumFunctionInformationStruct = SourceFunctions[cntSource][cntFunction];
      SourceFunctions[cntSource][cntFunction] = NULL;

      while (WalkAxumFunctionInformationStruct != NULL)
      {
        AXUM_FUNCTION_INFORMATION_STRUCT *AxumFunctionInformationStructToDelete = WalkAxumFunctionInformationStruct;
        WalkAxumFunctionInformationStruct = (AXUM_FUNCTION_INFORMATION_STRUCT *)WalkAxumFunctionInformationStruct->Next;
        delete AxumFunctionInformationStructToDelete;
      }
    }
  }

  //Destination
  for (int cntDestination=0; cntDestination<NUMBER_OF_DESTINATIONS; cntDestination++)
  {
    for (int cntFunction=0; cntFunction<NUMBER_OF_DESTINATION_FUNCTIONS; cntFunction++)
    {
      AXUM_FUNCTION_INFORMATION_STRUCT *WalkAxumFunctionInformationStruct = DestinationFunctions[cntDestination][cntFunction];
      DestinationFunctions[cntDestination][cntFunction] = NULL;

      while (WalkAxumFunctionInformationStruct != NULL)
      {
        AXUM_FUNCTION_INFORMATION_STRUCT *AxumFunctionInformationStructToDelete = WalkAxumFunctionInformationStruct;
        WalkAxumFunctionInformationStruct = (AXUM_FUNCTION_INFORMATION_STRUCT *)WalkAxumFunctionInformationStruct->Next;
        delete AxumFunctionInformationStructToDelete;
      }
    }
  }
}

void ModeControllerSensorChange(unsigned int SensorReceiveFunctionNr, unsigned char *Data, unsigned char DataType, unsigned char DataSize, float DataMinimal, float DataMaximal)
{
  unsigned int ModuleNr = (SensorReceiveFunctionNr>>12)&0xFFF;
  unsigned int FunctionNr = SensorReceiveFunctionNr&0xFFF;
  char ControlMode = -1;

  switch (FunctionNr)
  {
  case MODULE_FUNCTION_CONTROL_1:
  {
    ControlMode = AxumData.Control1Mode;
  }
  break;
  case MODULE_FUNCTION_CONTROL_2:
  {
    ControlMode = AxumData.Control2Mode;
  }
  break;
  case MODULE_FUNCTION_CONTROL_3:
  {
    ControlMode = AxumData.Control3Mode;
  }
  break;
  case MODULE_FUNCTION_CONTROL_4:
  {
    ControlMode = AxumData.Control4Mode;
  }
  break;
  }

  if (Data[3] == SIGNED_INTEGER_DATATYPE)
  {
    long TempData = 0;
    if (Data[4+Data[4]]&0x80)
    {   //signed
      TempData = (unsigned long)0xFFFFFFFF;
    }

    for (int cntByte=0; cntByte<Data[4]; cntByte++)
    {
      TempData <<= 8;
      TempData |= Data[5+cntByte];
    }

    switch (ControlMode)
    {
    case MODULE_CONTROL_MODE_SOURCE:
    {   //Source
      int CurrentSource = AxumData.ModuleData[ModuleNr].Source;

      CurrentSource = AdjustModuleSource(CurrentSource, TempData);

      SetNewSource(ModuleNr, CurrentSource, 0, 0);
    }
    break;
    case MODULE_CONTROL_MODE_SOURCE_GAIN:
    {   //Source gain
      if ((AxumData.ModuleData[ModuleNr].Source>=matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].Source<=matrix_sources.src_offset.max.source))
      {
        int SourceNr = AxumData.ModuleData[ModuleNr].Source-matrix_sources.src_offset.min.source;

        AxumData.SourceData[SourceNr].Gain += (float)TempData/10;
        if (AxumData.SourceData[SourceNr].Gain < 20)
        {
          AxumData.SourceData[SourceNr].Gain = 20;
        }
        else if (AxumData.SourceData[SourceNr].Gain > 75)
        {
          AxumData.SourceData[SourceNr].Gain = 75;
        }

        unsigned int DisplayFunctionNr = 0x05000000 | (SourceNr<<12);
        CheckObjectsToSent(DisplayFunctionNr+SOURCE_FUNCTION_GAIN);

        for (int cntModule=0; cntModule<128; cntModule++)
        {
          if (AxumData.ModuleData[cntModule].Source == (SourceNr+matrix_sources.src_offset.min.source))
          {
            unsigned int DisplayFunctionNr = (cntModule<<12);
            CheckObjectsToSent(DisplayFunctionNr+MODULE_FUNCTION_CONTROL_1);
            CheckObjectsToSent(DisplayFunctionNr+MODULE_FUNCTION_CONTROL_2);
            CheckObjectsToSent(DisplayFunctionNr+MODULE_FUNCTION_CONTROL_3);
            CheckObjectsToSent(DisplayFunctionNr+MODULE_FUNCTION_CONTROL_4);
            CheckObjectsToSent(DisplayFunctionNr+MODULE_FUNCTION_SOURCE_GAIN_LEVEL);
          }
        }
      }
    }
    break;
    case MODULE_CONTROL_MODE_GAIN:
    {   //Gain
      AxumData.ModuleData[ModuleNr].Gain += (float)TempData/10;
      if (AxumData.ModuleData[ModuleNr].Gain < -20)
      {
        AxumData.ModuleData[ModuleNr].Gain = -20;
      }
      else if (AxumData.ModuleData[ModuleNr].Gain > 20)
      {
        AxumData.ModuleData[ModuleNr].Gain = 20;
      }
      SetAxum_ModuleProcessing(ModuleNr);
      unsigned int FunctionNrToSent = 0x00000000 | (ModuleNr<<12);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

      unsigned int DisplayFunctionNr = (ModuleNr<<12) | MODULE_FUNCTION_GAIN_LEVEL;
      CheckObjectsToSent(DisplayFunctionNr);
    }
    break;
    case MODULE_CONTROL_MODE_PHASE:
    {   //Phase reverse
      if (TempData>=0)
      {
        AxumData.ModuleData[ModuleNr].PhaseReverse = 1;
      }
      else
      {
        AxumData.ModuleData[ModuleNr].PhaseReverse = 0;
      }
      SetAxum_ModuleProcessing(ModuleNr);
      unsigned int FunctionNrToSent = 0x00000000 | (ModuleNr<<12);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

      unsigned int DisplayFunctionNr = (ModuleNr<<12) | MODULE_FUNCTION_PHASE;
      CheckObjectsToSent(DisplayFunctionNr);
    }
    break;
    case MODULE_CONTROL_MODE_LOW_CUT:
    {   //LowCut
      if (TempData>=0)
      {
        AxumData.ModuleData[ModuleNr].Filter.Frequency *= 1+((float)TempData/100);
      }
      else
      {
        AxumData.ModuleData[ModuleNr].Filter.Frequency /= 1+((float)-TempData/100);
      }
      AxumData.ModuleData[ModuleNr].Filter.On = 1;

      if (AxumData.ModuleData[ModuleNr].Filter.Frequency <= 20)
      {
        AxumData.ModuleData[ModuleNr].Filter.On = 0;
        AxumData.ModuleData[ModuleNr].Filter.Frequency = 20;
      }
      else if (AxumData.ModuleData[ModuleNr].Filter.Frequency > 15000)
      {
        AxumData.ModuleData[ModuleNr].Filter.Frequency = 15000;
      }

      SetAxum_ModuleProcessing(ModuleNr);
      unsigned int FunctionNrToSent = 0x00000000 | (ModuleNr<<12);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

      unsigned int DisplayFunctionNr = (ModuleNr<<12) | MODULE_FUNCTION_LOW_CUT_FREQUENCY;
      CheckObjectsToSent(DisplayFunctionNr);
    }
    break;
    case MODULE_CONTROL_MODE_EQ_BAND_1_LEVEL:
    case MODULE_CONTROL_MODE_EQ_BAND_2_LEVEL:
    case MODULE_CONTROL_MODE_EQ_BAND_3_LEVEL:
    case MODULE_CONTROL_MODE_EQ_BAND_4_LEVEL:
    case MODULE_CONTROL_MODE_EQ_BAND_5_LEVEL:
    case MODULE_CONTROL_MODE_EQ_BAND_6_LEVEL:
    {   //EQ level
      int BandNr = (ControlMode-MODULE_CONTROL_MODE_EQ_BAND_1_LEVEL)/(MODULE_CONTROL_MODE_EQ_BAND_2_LEVEL-MODULE_CONTROL_MODE_EQ_BAND_1_LEVEL);
      AxumData.ModuleData[ModuleNr].EQBand[BandNr].Level += (float)TempData/10;
      if (AxumData.ModuleData[ModuleNr].EQBand[BandNr].Level<-AxumData.ModuleData[ModuleNr].EQBand[BandNr].Range)
      {
        AxumData.ModuleData[ModuleNr].EQBand[BandNr].Level = -AxumData.ModuleData[ModuleNr].EQBand[BandNr].Range;
      }
      else if (AxumData.ModuleData[ModuleNr].EQBand[BandNr].Level>AxumData.ModuleData[ModuleNr].EQBand[BandNr].Range)
      {
        AxumData.ModuleData[ModuleNr].EQBand[BandNr].Level = AxumData.ModuleData[ModuleNr].EQBand[BandNr].Range;
      }
      SetAxum_EQ(ModuleNr, BandNr);
      unsigned int FunctionNrToSent = 0x00000000 | (ModuleNr<<12);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

      unsigned int DisplayFunctionNr = (ModuleNr<<12) | (MODULE_FUNCTION_EQ_BAND_1_LEVEL+(BandNr*(MODULE_FUNCTION_EQ_BAND_2_LEVEL-MODULE_FUNCTION_EQ_BAND_1_LEVEL)));
      CheckObjectsToSent(DisplayFunctionNr);
    }
    break;
    case MODULE_CONTROL_MODE_EQ_BAND_1_FREQUENCY:
    case MODULE_CONTROL_MODE_EQ_BAND_2_FREQUENCY:
    case MODULE_CONTROL_MODE_EQ_BAND_3_FREQUENCY:
    case MODULE_CONTROL_MODE_EQ_BAND_4_FREQUENCY:
    case MODULE_CONTROL_MODE_EQ_BAND_5_FREQUENCY:
    case MODULE_CONTROL_MODE_EQ_BAND_6_FREQUENCY:
    {   //EQ frequency
      int BandNr = (ControlMode-MODULE_CONTROL_MODE_EQ_BAND_1_FREQUENCY)/(MODULE_CONTROL_MODE_EQ_BAND_2_FREQUENCY-MODULE_CONTROL_MODE_EQ_BAND_1_FREQUENCY);
      if (TempData>=0)
      {
        AxumData.ModuleData[ModuleNr].EQBand[BandNr].Frequency *= 1+((float)TempData/100);
      }
      else
      {
        AxumData.ModuleData[ModuleNr].EQBand[BandNr].Frequency /= 1+((float)-TempData/100);
      }

      if (AxumData.ModuleData[ModuleNr].EQBand[BandNr].Frequency<20)
      {
        AxumData.ModuleData[ModuleNr].EQBand[BandNr].Frequency = 20;
      }
      else if (AxumData.ModuleData[ModuleNr].EQBand[BandNr].Frequency>15000)
      {
        AxumData.ModuleData[ModuleNr].EQBand[BandNr].Frequency = 15000;
      }
      SetAxum_EQ(ModuleNr, BandNr);
      unsigned int FunctionNrToSent = 0x00000000 | (ModuleNr<<12);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

      unsigned int DisplayFunctionNr = (ModuleNr<<12) | (MODULE_FUNCTION_EQ_BAND_1_FREQUENCY+(BandNr*(MODULE_FUNCTION_EQ_BAND_2_FREQUENCY-MODULE_FUNCTION_EQ_BAND_1_FREQUENCY)));
      CheckObjectsToSent(DisplayFunctionNr);
    }
    break;
    case MODULE_CONTROL_MODE_EQ_BAND_1_BANDWIDTH:
    case MODULE_CONTROL_MODE_EQ_BAND_2_BANDWIDTH:
    case MODULE_CONTROL_MODE_EQ_BAND_3_BANDWIDTH:
    case MODULE_CONTROL_MODE_EQ_BAND_4_BANDWIDTH:
    case MODULE_CONTROL_MODE_EQ_BAND_5_BANDWIDTH:
    case MODULE_CONTROL_MODE_EQ_BAND_6_BANDWIDTH:
    {   //EQ Bandwidth
      int BandNr = (ControlMode-MODULE_CONTROL_MODE_EQ_BAND_1_BANDWIDTH)/(MODULE_CONTROL_MODE_EQ_BAND_2_BANDWIDTH-MODULE_CONTROL_MODE_EQ_BAND_1_BANDWIDTH);

      AxumData.ModuleData[ModuleNr].EQBand[BandNr].Bandwidth += (float)TempData/10;

      if (AxumData.ModuleData[ModuleNr].EQBand[BandNr].Bandwidth<0.1)
      {
        AxumData.ModuleData[ModuleNr].EQBand[BandNr].Bandwidth = 0.1;
      }
      else if (AxumData.ModuleData[ModuleNr].EQBand[BandNr].Bandwidth>10)
      {
        AxumData.ModuleData[ModuleNr].EQBand[BandNr].Bandwidth = 10;
      }
      SetAxum_EQ(ModuleNr, BandNr);
      unsigned int FunctionNrToSent = 0x00000000 | (ModuleNr<<12);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

      unsigned int DisplayFunctionNr = (ModuleNr<<12) | (MODULE_FUNCTION_EQ_BAND_1_BANDWIDTH+(BandNr*(MODULE_FUNCTION_EQ_BAND_2_BANDWIDTH-MODULE_FUNCTION_EQ_BAND_1_BANDWIDTH)));
      CheckObjectsToSent(DisplayFunctionNr);
    }
    break;
    case MODULE_CONTROL_MODE_EQ_BAND_1_TYPE:
    case MODULE_CONTROL_MODE_EQ_BAND_2_TYPE:
    case MODULE_CONTROL_MODE_EQ_BAND_3_TYPE:
    case MODULE_CONTROL_MODE_EQ_BAND_4_TYPE:
    case MODULE_CONTROL_MODE_EQ_BAND_5_TYPE:
    case MODULE_CONTROL_MODE_EQ_BAND_6_TYPE:
    {   //EQ type
      int BandNr = (ControlMode-MODULE_CONTROL_MODE_EQ_BAND_1_TYPE)/(MODULE_CONTROL_MODE_EQ_BAND_2_TYPE-MODULE_CONTROL_MODE_EQ_BAND_1_TYPE);
      int Type = AxumData.ModuleData[ModuleNr].EQBand[BandNr].Type;

      Type += TempData;

      if (Type<OFF)
      {
        Type = OFF;
      }
      else if (Type>NOTCH)
      {
        Type = NOTCH;
      }
      AxumData.ModuleData[ModuleNr].EQBand[BandNr].Type = (FilterType)Type;
      SetAxum_EQ(ModuleNr, BandNr);
      unsigned int FunctionNrToSent = 0x00000000 | (ModuleNr<<12);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

      unsigned int DisplayFunctionNr = (ModuleNr<<12) | (MODULE_FUNCTION_EQ_BAND_1_TYPE+(BandNr*(MODULE_FUNCTION_EQ_BAND_2_TYPE-MODULE_FUNCTION_EQ_BAND_1_TYPE)));
      CheckObjectsToSent(DisplayFunctionNr);
    }
    break;
    case MODULE_CONTROL_MODE_DYNAMICS:
    {   //Dynamics
      AxumData.ModuleData[ModuleNr].Dynamics += TempData;
      if (AxumData.ModuleData[ModuleNr].Dynamics < 0)
      {
        AxumData.ModuleData[ModuleNr].Dynamics = 0;
      }
      else if (AxumData.ModuleData[ModuleNr].Dynamics > 100)
      {
        AxumData.ModuleData[ModuleNr].Dynamics = 100;
      }
      SetAxum_ModuleProcessing(ModuleNr);
      unsigned int FunctionNrToSent = 0x00000000 | (ModuleNr<<12);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

      unsigned int DisplayFunctionNr = (ModuleNr<<12) | MODULE_FUNCTION_DYNAMICS_AMOUNT;
      CheckObjectsToSent(DisplayFunctionNr);
    }
    break;
    case MODULE_CONTROL_MODE_MONO:
    { //Mono
      if (TempData>=0)
      {
        AxumData.ModuleData[ModuleNr].Mono = 1;
      }
      else
      {
        AxumData.ModuleData[ModuleNr].Mono = 0;
      }
      SetAxum_BussLevels(ModuleNr);

      unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_MONO);
    }
    break;
    case MODULE_CONTROL_MODE_PAN:
    {   //Panorama
      AxumData.ModuleData[ModuleNr].Panorama += TempData;
      if (AxumData.ModuleData[ModuleNr].Panorama< 0)
      {
        AxumData.ModuleData[ModuleNr].Panorama = 0;
      }
      else if (AxumData.ModuleData[ModuleNr].Panorama > 1023)
      {
        AxumData.ModuleData[ModuleNr].Panorama = 1023;
      }
      SetAxum_BussLevels(ModuleNr);
      unsigned int FunctionNrToSent = 0x00000000 | (ModuleNr<<12);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

      unsigned int DisplayFunctionNr = (ModuleNr<<12) | MODULE_FUNCTION_PAN;
      CheckObjectsToSent(DisplayFunctionNr);
    }
    break;
    case MODULE_CONTROL_MODE_MODULE_LEVEL:
    {   //Module level
      float CurrentLevel = AxumData.ModuleData[ModuleNr].FaderLevel;

      AxumData.ModuleData[ModuleNr].FaderLevel += TempData;
      if (AxumData.ModuleData[ModuleNr].FaderLevel < -140)
      {
        AxumData.ModuleData[ModuleNr].FaderLevel = -140;
      }
      else
      {
        if (AxumData.ModuleData[ModuleNr].FaderLevel > (10-AxumData.LevelReserve))
        {
          AxumData.ModuleData[ModuleNr].FaderLevel = (10-AxumData.LevelReserve);
        }
      }
      float NewLevel = AxumData.ModuleData[ModuleNr].FaderLevel;

      SetAxum_BussLevels(ModuleNr);

      unsigned int FunctionNrToSent = (ModuleNr<<12);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_MODULE_LEVEL);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

      if (((CurrentLevel<=-80) && (NewLevel>-80)) ||
          ((CurrentLevel>-80) && (NewLevel<=-80)))
      { //fader on changed
        DoAxum_ModuleStatusChanged(ModuleNr);

        if ((AxumData.ModuleData[ModuleNr].Source>=matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].Source<=matrix_sources.src_offset.max.source))
        {
          unsigned int SourceNr = AxumData.ModuleData[ModuleNr].Source-matrix_sources.src_offset.min.source;
          FunctionNrToSent = 0x05000000 | (SourceNr<<12);
          CheckObjectsToSent(FunctionNrToSent | SOURCE_FUNCTION_MODULE_FADER_ON);
          CheckObjectsToSent(FunctionNrToSent | SOURCE_FUNCTION_MODULE_FADER_OFF);
          CheckObjectsToSent(FunctionNrToSent | SOURCE_FUNCTION_MODULE_FADER_ON_OFF);
          CheckObjectsToSent(FunctionNrToSent | SOURCE_FUNCTION_MODULE_FADER_AND_ON_ACTIVE);
          CheckObjectsToSent(FunctionNrToSent | SOURCE_FUNCTION_MODULE_FADER_AND_ON_INACTIVE);
        }
      }
    }
    break;
    case MODULE_CONTROL_MODE_BUSS_1_2:
    case MODULE_CONTROL_MODE_BUSS_3_4:
    case MODULE_CONTROL_MODE_BUSS_5_6:
    case MODULE_CONTROL_MODE_BUSS_7_8:
    case MODULE_CONTROL_MODE_BUSS_9_10:
    case MODULE_CONTROL_MODE_BUSS_11_12:
    case MODULE_CONTROL_MODE_BUSS_13_14:
    case MODULE_CONTROL_MODE_BUSS_15_16:
    case MODULE_CONTROL_MODE_BUSS_17_18:
    case MODULE_CONTROL_MODE_BUSS_19_20:
    case MODULE_CONTROL_MODE_BUSS_21_22:
    case MODULE_CONTROL_MODE_BUSS_23_24:
    case MODULE_CONTROL_MODE_BUSS_25_26:
    case MODULE_CONTROL_MODE_BUSS_27_28:
    case MODULE_CONTROL_MODE_BUSS_29_30:
    case MODULE_CONTROL_MODE_BUSS_31_32:
    {   //Aux
      int BussNr = (ControlMode-MODULE_CONTROL_MODE_BUSS_1_2)/(MODULE_CONTROL_MODE_BUSS_3_4-MODULE_CONTROL_MODE_BUSS_1_2);
      AxumData.ModuleData[ModuleNr].Buss[BussNr].Level += TempData;
      if (AxumData.ModuleData[ModuleNr].Buss[BussNr].Level < -140)
      {
        AxumData.ModuleData[ModuleNr].Buss[BussNr].Level = -140;
      }
      else
      {
        if (AxumData.ModuleData[ModuleNr].Buss[BussNr].Level > (10-AxumData.LevelReserve))
        {
          AxumData.ModuleData[ModuleNr].Buss[BussNr].Level = (10-AxumData.LevelReserve);
        }
      }
      SetAxum_BussLevels(ModuleNr);
      unsigned int FunctionNrToSent = 0x00000000 | (ModuleNr<<12);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

      FunctionNrToSent = (ModuleNr<<12) | (MODULE_FUNCTION_BUSS_1_2_LEVEL+(BussNr*(MODULE_FUNCTION_BUSS_3_4_LEVEL-MODULE_FUNCTION_BUSS_1_2_LEVEL)));
      CheckObjectsToSent(FunctionNrToSent);
    }
    break;
    case MODULE_CONTROL_MODE_BUSS_1_2_BALANCE:
    case MODULE_CONTROL_MODE_BUSS_3_4_BALANCE:
    case MODULE_CONTROL_MODE_BUSS_5_6_BALANCE:
    case MODULE_CONTROL_MODE_BUSS_7_8_BALANCE:
    case MODULE_CONTROL_MODE_BUSS_9_10_BALANCE:
    case MODULE_CONTROL_MODE_BUSS_11_12_BALANCE:
    case MODULE_CONTROL_MODE_BUSS_13_14_BALANCE:
    case MODULE_CONTROL_MODE_BUSS_15_16_BALANCE:
    case MODULE_CONTROL_MODE_BUSS_17_18_BALANCE:
    case MODULE_CONTROL_MODE_BUSS_19_20_BALANCE:
    case MODULE_CONTROL_MODE_BUSS_21_22_BALANCE:
    case MODULE_CONTROL_MODE_BUSS_23_24_BALANCE:
    case MODULE_CONTROL_MODE_BUSS_25_26_BALANCE:
    case MODULE_CONTROL_MODE_BUSS_27_28_BALANCE:
    case MODULE_CONTROL_MODE_BUSS_29_30_BALANCE:
    case MODULE_CONTROL_MODE_BUSS_31_32_BALANCE:
    {
      int BussNr = (ControlMode-MODULE_CONTROL_MODE_BUSS_1_2_BALANCE)/(MODULE_CONTROL_MODE_BUSS_3_4_BALANCE-MODULE_CONTROL_MODE_BUSS_1_2_BALANCE);


      AxumData.ModuleData[ModuleNr].Buss[BussNr].Balance += TempData;
      if (AxumData.ModuleData[ModuleNr].Buss[BussNr].Balance< 0)
      {
        AxumData.ModuleData[ModuleNr].Buss[BussNr].Balance = 0;
      }
      else if (AxumData.ModuleData[ModuleNr].Buss[BussNr].Balance > 1023)
      {
        AxumData.ModuleData[ModuleNr].Buss[BussNr].Balance = 1023;
      }

      SetAxum_BussLevels(ModuleNr);
      unsigned int FunctionNrToSent = 0x00000000 | (ModuleNr<<12);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

      CheckObjectsToSent(FunctionNrToSent | (MODULE_FUNCTION_BUSS_1_2_BALANCE+(BussNr*(MODULE_FUNCTION_BUSS_3_4_BALANCE-MODULE_FUNCTION_BUSS_1_2_BALANCE))));
    }
    break;
    }
  }
  DataType = 0;
  DataSize = 0;
  DataMinimal = 0;
  DataMaximal = 0;
}

void ModeControllerResetSensorChange(unsigned int SensorReceiveFunctionNr, unsigned char *Data, unsigned char DataType, unsigned char DataSize, float DataMinimal, float DataMaximal)
{
  unsigned int ModuleNr = (SensorReceiveFunctionNr>>12)&0xFFF;
  unsigned int FunctionNr = SensorReceiveFunctionNr&0xFFF;
  char ControlMode = -1;

  printf("ModeControllerResetSensorChange\n");

  switch (FunctionNr)
  {
  case MODULE_FUNCTION_CONTROL_1_RESET:
  {
    ControlMode = AxumData.Control1Mode;
  }
  break;
  case MODULE_FUNCTION_CONTROL_2_RESET:
  {
    ControlMode = AxumData.Control2Mode;
  }
  break;
  case MODULE_FUNCTION_CONTROL_3_RESET:
  {
    ControlMode = AxumData.Control3Mode;
  }
  break;
  case MODULE_FUNCTION_CONTROL_4_RESET:
  {
    ControlMode = AxumData.Control4Mode;
  }
  break;
  }

  if (Data[3] == STATE_DATATYPE)
  {
    unsigned long TempData = 0;

    for (int cntByte=0; cntByte<Data[4]; cntByte++)
    {
      TempData <<= 8;
      TempData |= Data[5+cntByte];
    }

    if (TempData)
    {
      switch (ControlMode)
      {
      case MODULE_CONTROL_MODE_SOURCE:
      {
        unsigned int OldSource = AxumData.ModuleData[ModuleNr].Source;
        AxumData.ModuleData[ModuleNr].Source = 0;

        if (OldSource != 0)
        {
          int OldSourceActive = 0;
          if (AxumData.ModuleData[ModuleNr].On)
          {
            if (AxumData.ModuleData[ModuleNr].FaderLevel>-80)
            {
              OldSourceActive = 1;
            }
          }

          if (!OldSourceActive)
          {
            unsigned int FunctionNrToSent = (ModuleNr<<12);
            CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
            CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
            CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
            CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

            CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_SOURCE_A);
            CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_SOURCE_B);

            SetAxum_ModuleSource(ModuleNr);
            SetAxum_ModuleMixMinus(ModuleNr, OldSource);

            if ((OldSource>=matrix_sources.src_offset.min.source) && (OldSource<=matrix_sources.src_offset.max.source))
            {
              unsigned int SourceNr = OldSource-matrix_sources.src_offset.min.source;
              FunctionNrToSent = 0x05000000 | (SourceNr<<12);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_ON);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_ON_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_FADER_ON);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_FADER_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_FADER_ON_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_FADER_AND_ON_ACTIVE);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_FADER_AND_ON_INACTIVE);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_1_2_ON);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_1_2_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_1_2_ON_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_3_4_ON);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_3_4_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_3_4_ON_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_5_6_ON);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_5_6_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_5_6_ON_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_7_8_ON);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_7_8_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_7_8_ON_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_9_10_ON);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_9_10_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_9_10_ON_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_11_12_ON);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_11_12_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_11_12_ON_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_13_14_ON);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_13_14_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_13_14_ON_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_15_16_ON);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_15_16_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_15_16_ON_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_17_18_ON);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_17_18_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_17_18_ON_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_19_20_ON);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_19_20_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_19_20_ON_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_21_22_ON);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_21_22_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_21_22_ON_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_23_24_ON);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_23_24_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_23_24_ON_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_25_26_ON);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_25_26_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_25_26_ON_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_27_28_ON);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_27_28_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_27_28_ON_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_29_30_ON);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_29_30_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_29_30_ON_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_31_32_ON);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_31_32_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_31_32_ON_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_COUGH_ON_OFF);
              CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_COUGH_ON_OFF);

              for (int cntModule=0; cntModule<128; cntModule++)
              {
                if (AxumData.ModuleData[cntModule].Source == AxumData.ModuleData[ModuleNr].Source)
                {
                  FunctionNrToSent = (cntModule<<12);
                  CheckObjectsToSent(FunctionNrToSent+MODULE_FUNCTION_SOURCE_PHANTOM);
                  CheckObjectsToSent(FunctionNrToSent+MODULE_FUNCTION_SOURCE_PAD);
                  CheckObjectsToSent(FunctionNrToSent+MODULE_FUNCTION_SOURCE_GAIN_LEVEL);
                  CheckObjectsToSent(FunctionNrToSent+MODULE_FUNCTION_SOURCE_GAIN_LEVEL_RESET);
                  CheckObjectsToSent(FunctionNrToSent+MODULE_FUNCTION_SOURCE_START);
                  CheckObjectsToSent(FunctionNrToSent+MODULE_FUNCTION_SOURCE_STOP);
                  CheckObjectsToSent(FunctionNrToSent+MODULE_FUNCTION_SOURCE_START_STOP);
                  CheckObjectsToSent(FunctionNrToSent+MODULE_FUNCTION_SOURCE_ALERT);
                }
              }
            }
          }
        }
      }
      break;
      case MODULE_CONTROL_MODE_SOURCE_GAIN:
      {   //Source gain
        if ((AxumData.ModuleData[ModuleNr].Source>=matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].Source<=matrix_sources.src_offset.max.source))
        {
          unsigned int SourceNr = AxumData.ModuleData[ModuleNr].Source-matrix_sources.src_offset.min.source;

          AxumData.SourceData[SourceNr].Gain = 30;

          unsigned int FunctionNrToSent = (ModuleNr<<12);
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

          FunctionNrToSent = 0x05000000 | (SourceNr<<12);
          CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_GAIN);

          for (int cntModule=0; cntModule<128; cntModule++)
          {
            if (AxumData.ModuleData[cntModule].Source == (SourceNr+1))
            {
              FunctionNrToSent = (cntModule<<12);
              CheckObjectsToSent(FunctionNrToSent+MODULE_FUNCTION_CONTROL_1);
              CheckObjectsToSent(FunctionNrToSent+MODULE_FUNCTION_CONTROL_2);
              CheckObjectsToSent(FunctionNrToSent+MODULE_FUNCTION_CONTROL_3);
              CheckObjectsToSent(FunctionNrToSent+MODULE_FUNCTION_CONTROL_4);

              CheckObjectsToSent(FunctionNrToSent+MODULE_FUNCTION_SOURCE_GAIN_LEVEL);
            }
          }
        }
      }
      break;
      case MODULE_CONTROL_MODE_GAIN:
      {   //Gain
        AxumData.ModuleData[ModuleNr].Gain = 0;
        SetAxum_ModuleProcessing(ModuleNr);

        unsigned int FunctionNrToSent = (ModuleNr<<12);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

        FunctionNrToSent = (ModuleNr<<12) | MODULE_FUNCTION_GAIN_LEVEL;
        CheckObjectsToSent(FunctionNrToSent);
      }
      break;
      case MODULE_CONTROL_MODE_PHASE:
      {   //Phase reverse
        if (AxumData.ModuleData[ModuleNr].PhaseReverse)
        {
          AxumData.ModuleData[ModuleNr].PhaseReverse = 0;
        }
        else
        {
          AxumData.ModuleData[ModuleNr].PhaseReverse = 1;
        }
        SetAxum_ModuleProcessing(ModuleNr);

        unsigned int FunctionNrToSent = (ModuleNr<<12);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

        FunctionNrToSent = (ModuleNr<<12) | MODULE_FUNCTION_PHASE;
        CheckObjectsToSent(FunctionNrToSent);
      }
      break;
      case MODULE_CONTROL_MODE_LOW_CUT:
      {   //LowCut filter
        AxumData.ModuleData[ModuleNr].Filter.Frequency = 20;
        AxumData.ModuleData[ModuleNr].Filter.On = 0;

        SetAxum_ModuleProcessing(ModuleNr);

        unsigned int FunctionNrToSent = (ModuleNr<<12);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

        FunctionNrToSent = (ModuleNr<<12) | MODULE_FUNCTION_LOW_CUT_FREQUENCY;
        CheckObjectsToSent(FunctionNrToSent);
      }
      break;
      case MODULE_CONTROL_MODE_EQ_BAND_1_LEVEL:
      case MODULE_CONTROL_MODE_EQ_BAND_2_LEVEL:
      case MODULE_CONTROL_MODE_EQ_BAND_3_LEVEL:
      case MODULE_CONTROL_MODE_EQ_BAND_4_LEVEL:
      case MODULE_CONTROL_MODE_EQ_BAND_5_LEVEL:
      case MODULE_CONTROL_MODE_EQ_BAND_6_LEVEL:
      {   //EQ Level
        int BandNr = (ControlMode-MODULE_CONTROL_MODE_EQ_BAND_1_LEVEL)/(MODULE_CONTROL_MODE_EQ_BAND_2_LEVEL-MODULE_CONTROL_MODE_EQ_BAND_1_LEVEL);
        AxumData.ModuleData[ModuleNr].EQBand[BandNr].Level = 0;
        SetAxum_EQ(ModuleNr, BandNr);

        unsigned int FunctionNrToSent = (ModuleNr<<12);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

        FunctionNrToSent = (ModuleNr<<12) | (MODULE_FUNCTION_EQ_BAND_1_LEVEL+(BandNr*(MODULE_FUNCTION_EQ_BAND_2_LEVEL-MODULE_FUNCTION_EQ_BAND_1_LEVEL)));
        CheckObjectsToSent(FunctionNrToSent);
      }
      break;
      case MODULE_CONTROL_MODE_EQ_BAND_1_FREQUENCY:
      case MODULE_CONTROL_MODE_EQ_BAND_2_FREQUENCY:
      case MODULE_CONTROL_MODE_EQ_BAND_3_FREQUENCY:
      case MODULE_CONTROL_MODE_EQ_BAND_4_FREQUENCY:
      case MODULE_CONTROL_MODE_EQ_BAND_5_FREQUENCY:
      case MODULE_CONTROL_MODE_EQ_BAND_6_FREQUENCY:
      {   //EQ frequency
        int BandNr = (ControlMode-MODULE_CONTROL_MODE_EQ_BAND_1_FREQUENCY)/(MODULE_CONTROL_MODE_EQ_BAND_2_FREQUENCY-MODULE_CONTROL_MODE_EQ_BAND_1_FREQUENCY);
        AxumData.ModuleData[ModuleNr].EQBand[BandNr].Frequency = AxumData.ModuleData[ModuleNr].EQBand[BandNr].DefaultFrequency;

        SetAxum_EQ(ModuleNr, BandNr);

        unsigned int FunctionNrToSent = (ModuleNr<<12);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

        FunctionNrToSent = (ModuleNr<<12) | (MODULE_FUNCTION_EQ_BAND_1_FREQUENCY+(BandNr*(MODULE_FUNCTION_EQ_BAND_2_FREQUENCY-MODULE_FUNCTION_EQ_BAND_1_FREQUENCY)));
        CheckObjectsToSent(FunctionNrToSent);
      }
      break;
      case MODULE_CONTROL_MODE_EQ_BAND_1_BANDWIDTH:
      case MODULE_CONTROL_MODE_EQ_BAND_2_BANDWIDTH:
      case MODULE_CONTROL_MODE_EQ_BAND_3_BANDWIDTH:
      case MODULE_CONTROL_MODE_EQ_BAND_4_BANDWIDTH:
      case MODULE_CONTROL_MODE_EQ_BAND_5_BANDWIDTH:
      case MODULE_CONTROL_MODE_EQ_BAND_6_BANDWIDTH:
      {   //EQ bandwidth
        int BandNr = (ControlMode-MODULE_CONTROL_MODE_EQ_BAND_1_BANDWIDTH)/(MODULE_CONTROL_MODE_EQ_BAND_2_BANDWIDTH-MODULE_CONTROL_MODE_EQ_BAND_1_BANDWIDTH);
        AxumData.ModuleData[ModuleNr].EQBand[BandNr].Bandwidth = AxumData.ModuleData[ModuleNr].EQBand[BandNr].DefaultBandwidth;
        SetAxum_EQ(ModuleNr, BandNr);

        unsigned int FunctionNrToSent = (ModuleNr<<12);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

        FunctionNrToSent = (ModuleNr<<12) | (MODULE_FUNCTION_EQ_BAND_1_BANDWIDTH+(BandNr*(MODULE_FUNCTION_EQ_BAND_2_BANDWIDTH-MODULE_FUNCTION_EQ_BAND_1_BANDWIDTH)));
        CheckObjectsToSent(FunctionNrToSent);
      }
      break;
      case MODULE_CONTROL_MODE_EQ_BAND_1_TYPE:
      case MODULE_CONTROL_MODE_EQ_BAND_2_TYPE:
      case MODULE_CONTROL_MODE_EQ_BAND_3_TYPE:
      case MODULE_CONTROL_MODE_EQ_BAND_4_TYPE:
      case MODULE_CONTROL_MODE_EQ_BAND_5_TYPE:
      case MODULE_CONTROL_MODE_EQ_BAND_6_TYPE:
      {   //EQ Type
        int BandNr = (ControlMode-MODULE_CONTROL_MODE_EQ_BAND_1_TYPE)/(MODULE_CONTROL_MODE_EQ_BAND_2_TYPE-MODULE_CONTROL_MODE_EQ_BAND_1_TYPE);
        AxumData.ModuleData[ModuleNr].EQBand[BandNr].Type = AxumData.ModuleData[ModuleNr].EQBand[BandNr].DefaultType;
        SetAxum_EQ(ModuleNr, BandNr);

        unsigned int FunctionNrToSent = (ModuleNr<<12);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

        FunctionNrToSent = (ModuleNr<<12) | (MODULE_FUNCTION_EQ_BAND_1_TYPE+(BandNr*(MODULE_FUNCTION_EQ_BAND_2_TYPE-MODULE_FUNCTION_EQ_BAND_1_TYPE)));
        CheckObjectsToSent(FunctionNrToSent);
      }
      break;
      case MODULE_CONTROL_MODE_DYNAMICS:
      {   //Dynamics
        AxumData.ModuleData[ModuleNr].DynamicsOn = !AxumData.ModuleData[ModuleNr].DynamicsOn;
        SetAxum_ModuleProcessing(ModuleNr);

        unsigned int FunctionNrToSent = (ModuleNr<<12);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

        FunctionNrToSent = (ModuleNr<<12) | MODULE_FUNCTION_DYNAMICS_ON_OFF;
        CheckObjectsToSent(FunctionNrToSent);
      }
      break;
      case MODULE_CONTROL_MODE_MONO:
      {   //Mono
        AxumData.ModuleData[ModuleNr].Mono = !AxumData.ModuleData[ModuleNr].Mono;
        SetAxum_BussLevels(ModuleNr);

        unsigned int FunctionNrToSent = (ModuleNr<<12);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

        FunctionNrToSent = (ModuleNr<<12) | MODULE_FUNCTION_MONO;
        CheckObjectsToSent(FunctionNrToSent);
      }
      break;
      case MODULE_CONTROL_MODE_PAN:
      {   //Panorama
        AxumData.ModuleData[ModuleNr].Panorama = 512;
        SetAxum_BussLevels(ModuleNr);

        unsigned int FunctionNrToSent = (ModuleNr<<12);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

        FunctionNrToSent = (ModuleNr<<12) | MODULE_FUNCTION_PAN;
        CheckObjectsToSent(FunctionNrToSent);
      }
      break;
      case MODULE_CONTROL_MODE_MODULE_LEVEL:
      {   //Module level
        float CurrentLevel = AxumData.ModuleData[ModuleNr].FaderLevel;
        AxumData.ModuleData[ModuleNr].FaderLevel = 0;
        float NewLevel = AxumData.ModuleData[ModuleNr].FaderLevel;

        SetAxum_BussLevels(ModuleNr);

        unsigned int FunctionNrToSent = (ModuleNr<<12);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_MODULE_LEVEL);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

        if (((CurrentLevel<=-80) && (NewLevel>-80)) ||
            ((CurrentLevel>-80) && (NewLevel<=-80)))
        { //fader on changed
          DoAxum_ModuleStatusChanged(ModuleNr);

          if ((AxumData.ModuleData[ModuleNr].Source>=matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].Source<=matrix_sources.src_offset.max.source))
          {
            unsigned int SourceNr = AxumData.ModuleData[ModuleNr].Source-matrix_sources.src_offset.min.source;
            FunctionNrToSent = 0x05000000 | (SourceNr<<12);
            CheckObjectsToSent(FunctionNrToSent | SOURCE_FUNCTION_MODULE_FADER_ON);
            CheckObjectsToSent(FunctionNrToSent | SOURCE_FUNCTION_MODULE_FADER_OFF);
            CheckObjectsToSent(FunctionNrToSent | SOURCE_FUNCTION_MODULE_FADER_ON_OFF);
            CheckObjectsToSent(FunctionNrToSent | SOURCE_FUNCTION_MODULE_FADER_AND_ON_ACTIVE);
            CheckObjectsToSent(FunctionNrToSent | SOURCE_FUNCTION_MODULE_FADER_AND_ON_INACTIVE);
          }
        }
      }
      break;
      case MODULE_CONTROL_MODE_BUSS_1_2:
      case MODULE_CONTROL_MODE_BUSS_3_4:
      case MODULE_CONTROL_MODE_BUSS_5_6:
      case MODULE_CONTROL_MODE_BUSS_7_8:
      case MODULE_CONTROL_MODE_BUSS_9_10:
      case MODULE_CONTROL_MODE_BUSS_11_12:
      case MODULE_CONTROL_MODE_BUSS_13_14:
      case MODULE_CONTROL_MODE_BUSS_15_16:
      case MODULE_CONTROL_MODE_BUSS_17_18:
      case MODULE_CONTROL_MODE_BUSS_19_20:
      case MODULE_CONTROL_MODE_BUSS_21_22:
      case MODULE_CONTROL_MODE_BUSS_23_24:
      case MODULE_CONTROL_MODE_BUSS_25_26:
      case MODULE_CONTROL_MODE_BUSS_27_28:
      case MODULE_CONTROL_MODE_BUSS_29_30:
      case MODULE_CONTROL_MODE_BUSS_31_32:
      {   //Buss
        int BussNr = (ControlMode-MODULE_CONTROL_MODE_BUSS_1_2)/(MODULE_CONTROL_MODE_BUSS_3_4-MODULE_CONTROL_MODE_BUSS_1_2);
        AxumData.ModuleData[ModuleNr].Buss[BussNr].On = !AxumData.ModuleData[ModuleNr].Buss[BussNr].On;
        SetAxum_BussLevels(ModuleNr);
        SetAxum_ModuleMixMinus(ModuleNr, 0);


        unsigned int FunctionNrToSent = (ModuleNr<<12);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

        FunctionNrToSent = (ModuleNr<<12) | (MODULE_FUNCTION_BUSS_1_2_ON_OFF+(BussNr*(MODULE_FUNCTION_BUSS_3_4_ON_OFF-MODULE_FUNCTION_BUSS_1_2_ON_OFF)));
        CheckObjectsToSent(FunctionNrToSent);

        FunctionNrToSent = 0x04000000;
        CheckObjectsToSent(FunctionNrToSent | (GLOBAL_FUNCTION_BUSS_1_2_RESET+(BussNr*(GLOBAL_FUNCTION_BUSS_3_4_RESET-GLOBAL_FUNCTION_BUSS_1_2_RESET))));

        if (  (AxumData.BussMasterData[BussNr].PreModuleOn) &&
              (AxumData.BussMasterData[BussNr].PreModuleLevel))
        {
          printf("Have to check monitor routing and muting\n");
          DoAxum_ModulePreStatusChanged(BussNr);
        }
      }
      break;
      case MODULE_CONTROL_MODE_BUSS_1_2_BALANCE:
      case MODULE_CONTROL_MODE_BUSS_3_4_BALANCE:
      case MODULE_CONTROL_MODE_BUSS_5_6_BALANCE:
      case MODULE_CONTROL_MODE_BUSS_7_8_BALANCE:
      case MODULE_CONTROL_MODE_BUSS_9_10_BALANCE:
      case MODULE_CONTROL_MODE_BUSS_11_12_BALANCE:
      case MODULE_CONTROL_MODE_BUSS_13_14_BALANCE:
      case MODULE_CONTROL_MODE_BUSS_15_16_BALANCE:
      case MODULE_CONTROL_MODE_BUSS_17_18_BALANCE:
      case MODULE_CONTROL_MODE_BUSS_19_20_BALANCE:
      case MODULE_CONTROL_MODE_BUSS_21_22_BALANCE:
      case MODULE_CONTROL_MODE_BUSS_23_24_BALANCE:
      case MODULE_CONTROL_MODE_BUSS_25_26_BALANCE:
      case MODULE_CONTROL_MODE_BUSS_27_28_BALANCE:
      case MODULE_CONTROL_MODE_BUSS_29_30_BALANCE:
      case MODULE_CONTROL_MODE_BUSS_31_32_BALANCE:
      {   //Buss
        int BussNr = (ControlMode-MODULE_CONTROL_MODE_BUSS_1_2)/(MODULE_CONTROL_MODE_BUSS_3_4-MODULE_CONTROL_MODE_BUSS_1_2);

        AxumData.ModuleData[ModuleNr].Buss[BussNr].Balance = 512;
        SetAxum_BussLevels(ModuleNr);

        unsigned int FunctionNrToSent = (ModuleNr<<12);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
        CheckObjectsToSent(FunctionNrToSent | (MODULE_FUNCTION_BUSS_1_2_BALANCE+(BussNr*(MODULE_FUNCTION_BUSS_3_4_BALANCE-MODULE_FUNCTION_BUSS_1_2_BALANCE))));
      }
      break;
      }
    }
  }
  DataType = 0;
  DataSize = 0;
  DataMinimal = 0;
  DataMaximal = 0;
}

void ModeControllerSetData(unsigned int SensorReceiveFunctionNr, unsigned int MambaNetAddress, unsigned int ObjectNr, unsigned char DataType, unsigned char DataSize, float DataMinimal, float DataMaximal)
{
  unsigned int ModuleNr = (SensorReceiveFunctionNr>>12)&0xFFF;
  unsigned int FunctionNr = SensorReceiveFunctionNr&0xFFF;
  char LCDText[9];
  unsigned char TransmitData[32];
  char ControlMode = -1;

  switch (FunctionNr)
  {
  case MODULE_FUNCTION_CONTROL_1:
  {
    ControlMode = AxumData.Control1Mode;
  }
  break;
  case MODULE_FUNCTION_CONTROL_2:
  {
    ControlMode = AxumData.Control2Mode;
  }
  break;
  case MODULE_FUNCTION_CONTROL_3:
  {
    ControlMode = AxumData.Control3Mode;
  }
  break;
  case MODULE_FUNCTION_CONTROL_4:
  {
    ControlMode = AxumData.Control4Mode;
  }
  break;
  }

  switch (ControlMode)
  {
  case MODULE_CONTROL_MODE_NONE:
  {
    GetSourceLabel(AxumData.ModuleData[ModuleNr].Source, LCDText, 8);
  }
  break;
  case MODULE_CONTROL_MODE_SOURCE:
  {
    GetSourceLabel(AxumData.ModuleData[ModuleNr].Source, LCDText, 8);
  }
  break;
  case MODULE_CONTROL_MODE_SOURCE_GAIN:
  {
    if ((AxumData.ModuleData[ModuleNr].Source>=matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].Source<=matrix_sources.src_offset.max.source))
    {
      unsigned int SourceNr = AxumData.ModuleData[ModuleNr].Source-matrix_sources.src_offset.min.source;
      sprintf(LCDText, "%5.1fdB", AxumData.SourceData[SourceNr].Gain);
    }
    else
    {
      sprintf(LCDText, "  - dB  ");
    }
  }
  break;
  case MODULE_CONTROL_MODE_GAIN:
  {
    sprintf(LCDText,     "%5.1fdB", AxumData.ModuleData[ModuleNr].Gain);
  }
  break;
  case MODULE_CONTROL_MODE_PHASE:
  {
    if (AxumData.ModuleData[ModuleNr].PhaseReverse)
    {
      sprintf(LCDText, "Reverse ");
    }
    else
    {
      sprintf(LCDText, " Normal ");
    }
  }
  break;
  case MODULE_CONTROL_MODE_LOW_CUT:
  {
    if (!AxumData.ModuleData[ModuleNr].Filter.On)
    {
      sprintf(LCDText, "  Off  ");
    }
    else
    {
      sprintf(LCDText, "%5dHz", AxumData.ModuleData[ModuleNr].Filter.Frequency);
    }
  }
  break;
  case MODULE_CONTROL_MODE_EQ_BAND_1_LEVEL:
  case MODULE_CONTROL_MODE_EQ_BAND_2_LEVEL:
  case MODULE_CONTROL_MODE_EQ_BAND_3_LEVEL:
  case MODULE_CONTROL_MODE_EQ_BAND_4_LEVEL:
  case MODULE_CONTROL_MODE_EQ_BAND_5_LEVEL:
  case MODULE_CONTROL_MODE_EQ_BAND_6_LEVEL:
  {
    int BandNr = (ControlMode-MODULE_CONTROL_MODE_EQ_BAND_1_LEVEL)/(MODULE_CONTROL_MODE_EQ_BAND_2_LEVEL-MODULE_CONTROL_MODE_EQ_BAND_1_LEVEL);
    sprintf(LCDText, "%5.1fdB", AxumData.ModuleData[ModuleNr].EQBand[BandNr].Level);
  }
  break;
  case MODULE_CONTROL_MODE_EQ_BAND_1_FREQUENCY:
  case MODULE_CONTROL_MODE_EQ_BAND_2_FREQUENCY:
  case MODULE_CONTROL_MODE_EQ_BAND_3_FREQUENCY:
  case MODULE_CONTROL_MODE_EQ_BAND_4_FREQUENCY:
  case MODULE_CONTROL_MODE_EQ_BAND_5_FREQUENCY:
  case MODULE_CONTROL_MODE_EQ_BAND_6_FREQUENCY:
  {
    int BandNr = (ControlMode-MODULE_CONTROL_MODE_EQ_BAND_1_FREQUENCY)/(MODULE_CONTROL_MODE_EQ_BAND_2_FREQUENCY-MODULE_CONTROL_MODE_EQ_BAND_1_FREQUENCY);
    sprintf(LCDText, "%5dHz", AxumData.ModuleData[ModuleNr].EQBand[BandNr].Frequency);
  }
  break;
  case MODULE_CONTROL_MODE_EQ_BAND_1_BANDWIDTH:
  case MODULE_CONTROL_MODE_EQ_BAND_2_BANDWIDTH:
  case MODULE_CONTROL_MODE_EQ_BAND_3_BANDWIDTH:
  case MODULE_CONTROL_MODE_EQ_BAND_4_BANDWIDTH:
  case MODULE_CONTROL_MODE_EQ_BAND_5_BANDWIDTH:
  case MODULE_CONTROL_MODE_EQ_BAND_6_BANDWIDTH:
  {
    int BandNr = (ControlMode-MODULE_CONTROL_MODE_EQ_BAND_1_BANDWIDTH)/(MODULE_CONTROL_MODE_EQ_BAND_2_BANDWIDTH-MODULE_CONTROL_MODE_EQ_BAND_1_BANDWIDTH);
    sprintf(LCDText, "%5.1f Q", AxumData.ModuleData[ModuleNr].EQBand[BandNr].Bandwidth);
  }
  break;
  case MODULE_CONTROL_MODE_EQ_BAND_1_TYPE:
  case MODULE_CONTROL_MODE_EQ_BAND_2_TYPE:
  case MODULE_CONTROL_MODE_EQ_BAND_3_TYPE:
  case MODULE_CONTROL_MODE_EQ_BAND_4_TYPE:
  case MODULE_CONTROL_MODE_EQ_BAND_5_TYPE:
  case MODULE_CONTROL_MODE_EQ_BAND_6_TYPE:
  {
    int BandNr = (ControlMode-MODULE_CONTROL_MODE_EQ_BAND_1_TYPE)/(MODULE_CONTROL_MODE_EQ_BAND_2_TYPE-MODULE_CONTROL_MODE_EQ_BAND_1_TYPE);
    switch (AxumData.ModuleData[ModuleNr].EQBand[BandNr].Type)
    {
    case OFF:
    {
      sprintf(LCDText, "  Off  ");
    }
    break;
    case HPF:
    {
      sprintf(LCDText, "HighPass");
    }
    break;
    case LOWSHELF:
    {
      sprintf(LCDText, "LowShelf");
    }
    break;
    case PEAKINGEQ:
    {
      sprintf(LCDText, "Peaking ");
    }
    break;
    case HIGHSHELF:
    {
      sprintf(LCDText, "Hi Shelf");
    }
    break;
    case LPF:
    {
      sprintf(LCDText, "Low Pass");
    }
    break;
    case BPF:
    {
      sprintf(LCDText, "BandPass");
    }
    break;
    case NOTCH:
    {
      sprintf(LCDText, "  Notch ");
    }
    break;
    }
  }
  break;
  case MODULE_CONTROL_MODE_DYNAMICS:
  {
    sprintf(LCDText, "  %3d%%  ", AxumData.ModuleData[ModuleNr].Dynamics);
  }
  break;
  case MODULE_CONTROL_MODE_MONO:
  {
    if (AxumData.ModuleData[ModuleNr].Mono)
    {
      sprintf(LCDText, "   On   ");
    }
    else
    {
      sprintf(LCDText, "   Off  ");
    }
  }
  break;
  case MODULE_CONTROL_MODE_PAN:
  {
    unsigned char Types[4] = {'[','|','|',']'};
    unsigned char Pos = AxumData.ModuleData[ModuleNr].Panorama/128;
    unsigned char Type = (AxumData.ModuleData[ModuleNr].Panorama%128)/32;

    sprintf(LCDText, "        ");
    if (AxumData.ModuleData[ModuleNr].Panorama == 0)
    {
      sprintf(LCDText, "Left    ");
    }
    else if ((AxumData.ModuleData[ModuleNr].Panorama == 511) || (AxumData.ModuleData[ModuleNr].Panorama == 512))
    {
      sprintf(LCDText, " Center ");
    }
    else if (AxumData.ModuleData[ModuleNr].Panorama == 1023)
    {
      sprintf(LCDText, "   Right");
    }
    else
    {
      LCDText[Pos] = Types[Type];
    }
  }
  break;
  case MODULE_CONTROL_MODE_MODULE_LEVEL:
  {
    sprintf(LCDText, " %4.0f dB", AxumData.ModuleData[ModuleNr].FaderLevel);
  }
  break;
  case MODULE_CONTROL_MODE_BUSS_1_2:
  case MODULE_CONTROL_MODE_BUSS_3_4:
  case MODULE_CONTROL_MODE_BUSS_5_6:
  case MODULE_CONTROL_MODE_BUSS_7_8:
  case MODULE_CONTROL_MODE_BUSS_9_10:
  case MODULE_CONTROL_MODE_BUSS_11_12:
  case MODULE_CONTROL_MODE_BUSS_13_14:
  case MODULE_CONTROL_MODE_BUSS_15_16:
  case MODULE_CONTROL_MODE_BUSS_17_18:
  case MODULE_CONTROL_MODE_BUSS_19_20:
  case MODULE_CONTROL_MODE_BUSS_21_22:
  case MODULE_CONTROL_MODE_BUSS_23_24:
  case MODULE_CONTROL_MODE_BUSS_25_26:
  case MODULE_CONTROL_MODE_BUSS_27_28:
  case MODULE_CONTROL_MODE_BUSS_29_30:
  case MODULE_CONTROL_MODE_BUSS_31_32:
  {
    int BussNr = (ControlMode-MODULE_CONTROL_MODE_BUSS_1_2)/(MODULE_CONTROL_MODE_BUSS_3_4-MODULE_CONTROL_MODE_BUSS_1_2);
    if (AxumData.ModuleData[ModuleNr].Buss[BussNr].On)
    {
      sprintf(LCDText, " %4.0f dB", AxumData.ModuleData[ModuleNr].Buss[BussNr].Level);
    }
    else
    {
      sprintf(LCDText, "  Off   ");
    }
  }
  break;
  case MODULE_CONTROL_MODE_BUSS_1_2_BALANCE:
  case MODULE_CONTROL_MODE_BUSS_3_4_BALANCE:
  case MODULE_CONTROL_MODE_BUSS_5_6_BALANCE:
  case MODULE_CONTROL_MODE_BUSS_7_8_BALANCE:
  case MODULE_CONTROL_MODE_BUSS_9_10_BALANCE:
  case MODULE_CONTROL_MODE_BUSS_11_12_BALANCE:
  case MODULE_CONTROL_MODE_BUSS_13_14_BALANCE:
  case MODULE_CONTROL_MODE_BUSS_15_16_BALANCE:
  case MODULE_CONTROL_MODE_BUSS_17_18_BALANCE:
  case MODULE_CONTROL_MODE_BUSS_19_20_BALANCE:
  case MODULE_CONTROL_MODE_BUSS_21_22_BALANCE:
  case MODULE_CONTROL_MODE_BUSS_23_24_BALANCE:
  case MODULE_CONTROL_MODE_BUSS_25_26_BALANCE:
  case MODULE_CONTROL_MODE_BUSS_27_28_BALANCE:
  case MODULE_CONTROL_MODE_BUSS_29_30_BALANCE:
  case MODULE_CONTROL_MODE_BUSS_31_32_BALANCE:
  {
    int BussNr = (ControlMode-MODULE_CONTROL_MODE_BUSS_1_2_BALANCE)/(MODULE_CONTROL_MODE_BUSS_3_4_BALANCE-MODULE_CONTROL_MODE_BUSS_1_2_BALANCE);

    unsigned char Types[4] = {'[','|','|',']'};
    unsigned char Pos = AxumData.ModuleData[ModuleNr].Buss[BussNr].Balance/128;
    unsigned char Type = (AxumData.ModuleData[ModuleNr].Buss[BussNr].Balance%128)/32;

    sprintf(LCDText, "        ");
    if (AxumData.ModuleData[ModuleNr].Buss[BussNr].Balance == 0)
    {
      sprintf(LCDText, "Left    ");
    }
    else if ((AxumData.ModuleData[ModuleNr].Buss[BussNr].Balance == 511) || (AxumData.ModuleData[ModuleNr].Buss[BussNr].Balance == 512))
    {
      sprintf(LCDText, " Center ");
    }
    else if (AxumData.ModuleData[ModuleNr].Buss[BussNr].Balance == 1023)
    {
      sprintf(LCDText, "   Right");
    }
    else
    {
      LCDText[Pos] = Types[Type];
    }
  }
  break;
  default:
  {
    sprintf(LCDText, "UNKNOWN ");
  }
  break;
  }

  TransmitData[0] = (ObjectNr>>8)&0xFF;
  TransmitData[1] = ObjectNr&0xFF;
  TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
  TransmitData[3] = OCTET_STRING_DATATYPE;
  TransmitData[4] = 8;
  TransmitData[5] = LCDText[0];
  TransmitData[6] = LCDText[1];
  TransmitData[7] = LCDText[2];
  TransmitData[8] = LCDText[3];
  TransmitData[9] = LCDText[4];
  TransmitData[10] = LCDText[5];
  TransmitData[11] = LCDText[6];
  TransmitData[12] = LCDText[7];

  SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 13);

  DataType = 0;
  DataSize = 0;
  DataMinimal = 0;
  DataMaximal = 0;
}

void ModeControllerSetLabel(unsigned int SensorReceiveFunctionNr, unsigned int MambaNetAddress, unsigned int ObjectNr, unsigned char DataType, unsigned char DataSize, float DataMinimal, float DataMaximal)
{
  unsigned int ModuleNr = (SensorReceiveFunctionNr>>12)&0xFFF;
  unsigned int FunctionNr = SensorReceiveFunctionNr&0xFFF;
  char LCDText[9];
  unsigned char TransmitData[32];
  char ControlMode = -1;

  switch (FunctionNr)
  {
  case MODULE_FUNCTION_CONTROL_1_LABEL:
  {
    ControlMode = AxumData.Control1Mode;
  }
  break;
  case MODULE_FUNCTION_CONTROL_2_LABEL:
  {
    ControlMode = AxumData.Control2Mode;
  }
  break;
  case MODULE_FUNCTION_CONTROL_3_LABEL:
  {
    ControlMode = AxumData.Control3Mode;
  }
  break;
  case MODULE_FUNCTION_CONTROL_4_LABEL:
  {
    ControlMode = AxumData.Control4Mode;
  }
  break;
  }

  switch (ControlMode)
  {
  case MODULE_CONTROL_MODE_NONE:
  {
    if (ModuleNr<9)
    {
      sprintf(LCDText, " Mod %d  ", ModuleNr+1);
    }
    else if (ModuleNr<99)
    {
      sprintf(LCDText, " Mod %d ", ModuleNr+1);
    }
    else
    {
      sprintf(LCDText, "Mod %d ", ModuleNr+1);
    }
  }
  break;
  case MODULE_CONTROL_MODE_SOURCE:
  {
    sprintf(LCDText," Source ");
  }
  break;
  case MODULE_CONTROL_MODE_SOURCE_GAIN:
  {
    sprintf(LCDText,"Src gain");
  }
  break;
  case MODULE_CONTROL_MODE_GAIN:
  {
    sprintf(LCDText,"  Gain  ");
  }
  break;
  case MODULE_CONTROL_MODE_PHASE:
  {
    sprintf(LCDText," Phase  ");
  }
  break;
  case MODULE_CONTROL_MODE_LOW_CUT:
  {
    sprintf(LCDText,"Low cut ");
  }
  break;
  case MODULE_CONTROL_MODE_EQ_BAND_1_LEVEL:
  case MODULE_CONTROL_MODE_EQ_BAND_2_LEVEL:
  case MODULE_CONTROL_MODE_EQ_BAND_3_LEVEL:
  case MODULE_CONTROL_MODE_EQ_BAND_4_LEVEL:
  case MODULE_CONTROL_MODE_EQ_BAND_5_LEVEL:
  case MODULE_CONTROL_MODE_EQ_BAND_6_LEVEL:
  {
    int BandNr = (ControlMode-MODULE_CONTROL_MODE_EQ_BAND_1_LEVEL)/(MODULE_CONTROL_MODE_EQ_BAND_2_LEVEL-MODULE_CONTROL_MODE_EQ_BAND_1_LEVEL);
    sprintf(LCDText,"EQ%d lvl ", BandNr+1);
  }
  break;
  case MODULE_CONTROL_MODE_EQ_BAND_1_FREQUENCY:
  case MODULE_CONTROL_MODE_EQ_BAND_2_FREQUENCY:
  case MODULE_CONTROL_MODE_EQ_BAND_3_FREQUENCY:
  case MODULE_CONTROL_MODE_EQ_BAND_4_FREQUENCY:
  case MODULE_CONTROL_MODE_EQ_BAND_5_FREQUENCY:
  case MODULE_CONTROL_MODE_EQ_BAND_6_FREQUENCY:
  {
    int BandNr = (ControlMode-MODULE_CONTROL_MODE_EQ_BAND_1_FREQUENCY)/(MODULE_CONTROL_MODE_EQ_BAND_2_FREQUENCY-MODULE_CONTROL_MODE_EQ_BAND_1_FREQUENCY);
    sprintf(LCDText,"EQ%d freq", BandNr+1);
  }
  break;
  case MODULE_CONTROL_MODE_EQ_BAND_1_BANDWIDTH:
  case MODULE_CONTROL_MODE_EQ_BAND_2_BANDWIDTH:
  case MODULE_CONTROL_MODE_EQ_BAND_3_BANDWIDTH:
  case MODULE_CONTROL_MODE_EQ_BAND_4_BANDWIDTH:
  case MODULE_CONTROL_MODE_EQ_BAND_5_BANDWIDTH:
  case MODULE_CONTROL_MODE_EQ_BAND_6_BANDWIDTH:
  {
    int BandNr = (ControlMode-MODULE_CONTROL_MODE_EQ_BAND_1_BANDWIDTH)/(MODULE_CONTROL_MODE_EQ_BAND_2_BANDWIDTH-MODULE_CONTROL_MODE_EQ_BAND_1_BANDWIDTH);
    sprintf(LCDText," EQ%d Q  ", BandNr+1);
  }
  break;
  case MODULE_CONTROL_MODE_EQ_BAND_1_TYPE:
  case MODULE_CONTROL_MODE_EQ_BAND_2_TYPE:
  case MODULE_CONTROL_MODE_EQ_BAND_3_TYPE:
  case MODULE_CONTROL_MODE_EQ_BAND_4_TYPE:
  case MODULE_CONTROL_MODE_EQ_BAND_5_TYPE:
  case MODULE_CONTROL_MODE_EQ_BAND_6_TYPE:
  {
    int BandNr = (ControlMode-MODULE_CONTROL_MODE_EQ_BAND_1_TYPE)/(MODULE_CONTROL_MODE_EQ_BAND_2_TYPE-MODULE_CONTROL_MODE_EQ_BAND_1_TYPE);
    sprintf(LCDText,"EQ%d type", BandNr+1);
  }
  break;
  case MODULE_CONTROL_MODE_DYNAMICS:
  {
    sprintf(LCDText,"  Dyn   ");
  }
  break;
  case MODULE_CONTROL_MODE_MONO:
  {
    sprintf(LCDText,"  Mono  ");
  }
  break;
  case MODULE_CONTROL_MODE_PAN:
  {
    sprintf(LCDText,"  Pan   ");
  }
  break;
  case MODULE_CONTROL_MODE_BUSS_1_2:
  case MODULE_CONTROL_MODE_BUSS_3_4:
  case MODULE_CONTROL_MODE_BUSS_5_6:
  case MODULE_CONTROL_MODE_BUSS_7_8:
  case MODULE_CONTROL_MODE_BUSS_9_10:
  case MODULE_CONTROL_MODE_BUSS_11_12:
  case MODULE_CONTROL_MODE_BUSS_13_14:
  case MODULE_CONTROL_MODE_BUSS_15_16:
  case MODULE_CONTROL_MODE_BUSS_17_18:
  case MODULE_CONTROL_MODE_BUSS_19_20:
  case MODULE_CONTROL_MODE_BUSS_21_22:
  case MODULE_CONTROL_MODE_BUSS_23_24:
  case MODULE_CONTROL_MODE_BUSS_25_26:
  case MODULE_CONTROL_MODE_BUSS_27_28:
  case MODULE_CONTROL_MODE_BUSS_29_30:
  case MODULE_CONTROL_MODE_BUSS_31_32:
  {
    int BussNr = (ControlMode-MODULE_CONTROL_MODE_BUSS_1_2)/(MODULE_CONTROL_MODE_BUSS_3_4-MODULE_CONTROL_MODE_BUSS_1_2);
    strncpy(LCDText, AxumData.BussMasterData[BussNr].Label, 8);
  }
  break;
  case MODULE_CONTROL_MODE_BUSS_1_2_BALANCE:
  case MODULE_CONTROL_MODE_BUSS_3_4_BALANCE:
  case MODULE_CONTROL_MODE_BUSS_5_6_BALANCE:
  case MODULE_CONTROL_MODE_BUSS_7_8_BALANCE:
  case MODULE_CONTROL_MODE_BUSS_9_10_BALANCE:
  case MODULE_CONTROL_MODE_BUSS_11_12_BALANCE:
  case MODULE_CONTROL_MODE_BUSS_13_14_BALANCE:
  case MODULE_CONTROL_MODE_BUSS_15_16_BALANCE:
  case MODULE_CONTROL_MODE_BUSS_17_18_BALANCE:
  case MODULE_CONTROL_MODE_BUSS_19_20_BALANCE:
  case MODULE_CONTROL_MODE_BUSS_21_22_BALANCE:
  case MODULE_CONTROL_MODE_BUSS_23_24_BALANCE:
  case MODULE_CONTROL_MODE_BUSS_25_26_BALANCE:
  case MODULE_CONTROL_MODE_BUSS_27_28_BALANCE:
  case MODULE_CONTROL_MODE_BUSS_29_30_BALANCE:
  case MODULE_CONTROL_MODE_BUSS_31_32_BALANCE:
  {
    int BussNr = (ControlMode-MODULE_CONTROL_MODE_BUSS_1_2_BALANCE)/(MODULE_CONTROL_MODE_BUSS_3_4_BALANCE-MODULE_CONTROL_MODE_BUSS_1_2_BALANCE);
    strncpy(LCDText, AxumData.BussMasterData[BussNr].Label, 8);
  }
  break;
  case MODULE_CONTROL_MODE_MODULE_LEVEL:
  {
    sprintf(LCDText," Level  ");
  }
  break;
  }

  TransmitData[0] = (ObjectNr>>8)&0xFF;
  TransmitData[1] = ObjectNr&0xFF;
  TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
  TransmitData[3] = OCTET_STRING_DATATYPE;
  TransmitData[4] = 8;
  TransmitData[5] = LCDText[0];
  TransmitData[6] = LCDText[1];
  TransmitData[7] = LCDText[2];
  TransmitData[8] = LCDText[3];
  TransmitData[9] = LCDText[4];
  TransmitData[10] = LCDText[5];
  TransmitData[11] = LCDText[6];
  TransmitData[12] = LCDText[7];

  SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 13);
  DataType = 0;
  DataSize = 0;
  DataMinimal = 0;
  DataMaximal = 0;
}


void MasterModeControllerSensorChange(unsigned int SensorReceiveFunctionNr, unsigned char *Data, unsigned char DataType, unsigned char DataSize, float DataMinimal, float DataMaximal)
{
  int FunctionNr = SensorReceiveFunctionNr&0xFFF;
  int MasterControlMode = -1;

  switch (FunctionNr)
  {
  case GLOBAL_FUNCTION_MASTER_CONTROL_1:
  {
    MasterControlMode = AxumData.MasterControl1Mode;
  }
  break;
  case GLOBAL_FUNCTION_MASTER_CONTROL_2:
  {
    MasterControlMode = AxumData.MasterControl2Mode;
  }
  break;
  case GLOBAL_FUNCTION_MASTER_CONTROL_3:
  {
    MasterControlMode = AxumData.MasterControl3Mode;
  }
  break;
  case GLOBAL_FUNCTION_MASTER_CONTROL_4:
  {
    MasterControlMode = AxumData.MasterControl4Mode;
  }
  break;
  }

  if (Data[3] == UNSIGNED_INTEGER_DATATYPE)
  {
    unsigned long TempData = 0;

    for (int cntByte=0; cntByte<Data[4]; cntByte++)
    {
      TempData <<= 8;
      TempData |= Data[5+cntByte];
    }

    int Position = (TempData*1023)/(DataMaximal-DataMinimal);
    switch (MasterControlMode)
    {
    case MASTER_CONTROL_MODE_BUSS_1_2:
    case MASTER_CONTROL_MODE_BUSS_3_4:
    case MASTER_CONTROL_MODE_BUSS_5_6:
    case MASTER_CONTROL_MODE_BUSS_7_8:
    case MASTER_CONTROL_MODE_BUSS_9_10:
    case MASTER_CONTROL_MODE_BUSS_11_12:
    case MASTER_CONTROL_MODE_BUSS_13_14:
    case MASTER_CONTROL_MODE_BUSS_15_16:
    case MASTER_CONTROL_MODE_BUSS_17_18:
    case MASTER_CONTROL_MODE_BUSS_19_20:
    case MASTER_CONTROL_MODE_BUSS_21_22:
    case MASTER_CONTROL_MODE_BUSS_23_24:
    case MASTER_CONTROL_MODE_BUSS_25_26:
    case MASTER_CONTROL_MODE_BUSS_27_28:
    case MASTER_CONTROL_MODE_BUSS_29_30:
    case MASTER_CONTROL_MODE_BUSS_31_32:
    { //master level
      int BussNr = MasterControlMode-MASTER_CONTROL_MODE_BUSS_1_2;
      float dB = Position2dB[Position];
      //dB -= AxumData.LevelReserve;
      dB -= 10;

      AxumData.BussMasterData[BussNr].Level = dB;

      SetAxum_BussMasterLevels();
      CheckObjectsToSent(0x04000000 | GLOBAL_FUNCTION_MASTER_CONTROL_1);
      CheckObjectsToSent(0x04000000 | GLOBAL_FUNCTION_MASTER_CONTROL_2);
      CheckObjectsToSent(0x04000000 | GLOBAL_FUNCTION_MASTER_CONTROL_3);
      CheckObjectsToSent(0x04000000 | GLOBAL_FUNCTION_MASTER_CONTROL_4);

      unsigned int DisplayFunctionNr = 0x01000000 | (BussNr<<12);
      CheckObjectsToSent(DisplayFunctionNr | BUSS_FUNCTION_MASTER_LEVEL);
    }
    break;
    }
  }
  else if (Data[3] == SIGNED_INTEGER_DATATYPE)
  {
    long TempData = 0;
    if (Data[5]&0x80)
    {   //signed
      TempData = (unsigned long)0xFFFFFFFF;
    }

    for (int cntByte=0; cntByte<Data[4]; cntByte++)
    {
      TempData <<= 8;
      TempData |= Data[5+cntByte];
    }

    switch (MasterControlMode)
    {
    case MASTER_CONTROL_MODE_BUSS_1_2:
    case MASTER_CONTROL_MODE_BUSS_3_4:
    case MASTER_CONTROL_MODE_BUSS_5_6:
    case MASTER_CONTROL_MODE_BUSS_7_8:
    case MASTER_CONTROL_MODE_BUSS_9_10:
    case MASTER_CONTROL_MODE_BUSS_11_12:
    case MASTER_CONTROL_MODE_BUSS_13_14:
    case MASTER_CONTROL_MODE_BUSS_15_16:
    case MASTER_CONTROL_MODE_BUSS_17_18:
    case MASTER_CONTROL_MODE_BUSS_19_20:
    case MASTER_CONTROL_MODE_BUSS_21_22:
    case MASTER_CONTROL_MODE_BUSS_23_24:
    case MASTER_CONTROL_MODE_BUSS_25_26:
    case MASTER_CONTROL_MODE_BUSS_27_28:
    case MASTER_CONTROL_MODE_BUSS_29_30:
    case MASTER_CONTROL_MODE_BUSS_31_32:
    { //master level
      int BussNr = MasterControlMode-MASTER_CONTROL_MODE_BUSS_1_2;
      AxumData.BussMasterData[BussNr].Level += TempData;
      if (AxumData.BussMasterData[BussNr].Level<-140)
      {
        AxumData.BussMasterData[BussNr].Level = -140;
      }
      else
      {
        if (AxumData.BussMasterData[BussNr].Level>0)
        {
          AxumData.BussMasterData[BussNr].Level = 0;
        }
      }

      SetAxum_BussMasterLevels();
      CheckObjectsToSent(0x04000000 | GLOBAL_FUNCTION_MASTER_CONTROL_1);
      CheckObjectsToSent(0x04000000 | GLOBAL_FUNCTION_MASTER_CONTROL_2);
      CheckObjectsToSent(0x04000000 | GLOBAL_FUNCTION_MASTER_CONTROL_3);
      CheckObjectsToSent(0x04000000 | GLOBAL_FUNCTION_MASTER_CONTROL_4);

      unsigned int DisplayFunctionNr = 0x01000000 | (BussNr<<12);
      CheckObjectsToSent(DisplayFunctionNr | BUSS_FUNCTION_MASTER_LEVEL);
    }
    break;
    }
  }
  DataType = 0;
  DataSize = 0;
}

void MasterModeControllerResetSensorChange(unsigned int SensorReceiveFunctionNr, unsigned char *Data, unsigned char DataType, unsigned char DataSize, float DataMinimal, float DataMaximal)
{
  int FunctionNr = SensorReceiveFunctionNr&0xFFF;
  int MasterControlMode = -1;

  switch (FunctionNr)
  {
  case GLOBAL_FUNCTION_MASTER_CONTROL_1_RESET:
  {
    MasterControlMode = MasterControlMode;
  }
  break;
  case GLOBAL_FUNCTION_MASTER_CONTROL_2_RESET:
  {
    MasterControlMode = MasterControlMode;
  }
  break;
  case GLOBAL_FUNCTION_MASTER_CONTROL_3_RESET:
  {
    MasterControlMode = MasterControlMode;
  }
  break;
  case GLOBAL_FUNCTION_MASTER_CONTROL_4_RESET:
  {
    MasterControlMode = MasterControlMode;
  }
  break;
  }

  if (Data[3] == STATE_DATATYPE)
  {
    unsigned long TempData = 0;

    for (int cntByte=0; cntByte<Data[4]; cntByte++)
    {
      TempData <<= 8;
      TempData |= Data[5+cntByte];
    }

    if (TempData)
    {
      switch (MasterControlMode)
      {
      case MASTER_CONTROL_MODE_BUSS_1_2:
      case MASTER_CONTROL_MODE_BUSS_3_4:
      case MASTER_CONTROL_MODE_BUSS_5_6:
      case MASTER_CONTROL_MODE_BUSS_7_8:
      case MASTER_CONTROL_MODE_BUSS_9_10:
      case MASTER_CONTROL_MODE_BUSS_11_12:
      case MASTER_CONTROL_MODE_BUSS_13_14:
      case MASTER_CONTROL_MODE_BUSS_15_16:
      case MASTER_CONTROL_MODE_BUSS_17_18:
      case MASTER_CONTROL_MODE_BUSS_19_20:
      case MASTER_CONTROL_MODE_BUSS_21_22:
      case MASTER_CONTROL_MODE_BUSS_23_24:
      case MASTER_CONTROL_MODE_BUSS_25_26:
      case MASTER_CONTROL_MODE_BUSS_27_28:
      case MASTER_CONTROL_MODE_BUSS_29_30:
      case MASTER_CONTROL_MODE_BUSS_31_32:
      { //master level
        int BussNr = MasterControlMode - MASTER_CONTROL_MODE_BUSS_1_2;
        AxumData.BussMasterData[BussNr].Level = 0;

        SetAxum_BussMasterLevels();
        CheckObjectsToSent(0x04000000 | GLOBAL_FUNCTION_MASTER_CONTROL_1);
        CheckObjectsToSent(0x04000000 | GLOBAL_FUNCTION_MASTER_CONTROL_2);
        CheckObjectsToSent(0x04000000 | GLOBAL_FUNCTION_MASTER_CONTROL_3);
        CheckObjectsToSent(0x04000000 | GLOBAL_FUNCTION_MASTER_CONTROL_4);

        unsigned int DisplayFunctionNr = 0x01000000 | (BussNr<<12);
        CheckObjectsToSent(DisplayFunctionNr | BUSS_FUNCTION_MASTER_LEVEL);
      }
      break;
      }
    }
  }
  DataType = 0;
  DataSize = 0;
  DataMinimal = 0;
  DataMaximal = 0;
}


void MasterModeControllerSetData(unsigned int SensorReceiveFunctionNr, unsigned int MambaNetAddress, unsigned int ObjectNr, unsigned char DataType, unsigned char DataSize, float DataMinimal, float DataMaximal)
{
  int FunctionNr = SensorReceiveFunctionNr&0xFFF;
  int MasterControlMode = -1;
  unsigned char TransmitData[32];
  float MasterLevel = -140;
  unsigned char Mask = 0x00;

  switch (FunctionNr)
  {
  case GLOBAL_FUNCTION_MASTER_CONTROL_1:
  {
    MasterControlMode = AxumData.MasterControl1Mode;
  }
  break;
  case GLOBAL_FUNCTION_MASTER_CONTROL_2:
  {
    MasterControlMode = AxumData.MasterControl2Mode;
  }
  break;
  case GLOBAL_FUNCTION_MASTER_CONTROL_3:
  {
    MasterControlMode = AxumData.MasterControl3Mode;
  }
  break;
  case GLOBAL_FUNCTION_MASTER_CONTROL_4:
  {
    MasterControlMode = AxumData.MasterControl4Mode;
  }
  break;
  }

  switch (MasterControlMode)
  {
  case MASTER_CONTROL_MODE_BUSS_1_2:
  case MASTER_CONTROL_MODE_BUSS_3_4:
  case MASTER_CONTROL_MODE_BUSS_5_6:
  case MASTER_CONTROL_MODE_BUSS_7_8:
  case MASTER_CONTROL_MODE_BUSS_9_10:
  case MASTER_CONTROL_MODE_BUSS_11_12:
  case MASTER_CONTROL_MODE_BUSS_13_14:
  case MASTER_CONTROL_MODE_BUSS_15_16:
  case MASTER_CONTROL_MODE_BUSS_17_18:
  case MASTER_CONTROL_MODE_BUSS_19_20:
  case MASTER_CONTROL_MODE_BUSS_21_22:
  case MASTER_CONTROL_MODE_BUSS_23_24:
  case MASTER_CONTROL_MODE_BUSS_25_26:
  case MASTER_CONTROL_MODE_BUSS_27_28:
  case MASTER_CONTROL_MODE_BUSS_29_30:
  case MASTER_CONTROL_MODE_BUSS_31_32:
  { //busses
    int BussNr = MasterControlMode-MASTER_CONTROL_MODE_BUSS_1_2;
    MasterLevel = AxumData.BussMasterData[BussNr].Level;
  }
  break;
  }

  switch (DataType)
  {
  case UNSIGNED_INTEGER_DATATYPE:
  {
    int dB = (MasterLevel*10)+1400;
    if (dB<0)
    {
      dB = 0;
    }
    else if (dB>=1500)
    {
      dB = 1499;
    }
    int Position = ((dB2Position[dB]*(DataMaximal-DataMinimal))/1023)+DataMinimal;

    TransmitData[0] = (ObjectNr>>8)&0xFF;
    TransmitData[1] = ObjectNr&0xFF;
    TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
    TransmitData[3] = UNSIGNED_INTEGER_DATATYPE;
    TransmitData[4] = 2;
    TransmitData[5] = (Position>>8)&0xFF;
    TransmitData[6] = Position&0xFF;

    SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 7);
  }
  break;
  case BIT_STRING_DATATYPE:
  {
    int NrOfLEDs = (MasterLevel+140)/(140/DataMaximal);
    for (char cntBit=0; cntBit<NrOfLEDs; cntBit++)
    {
      Mask |= 0x01<<cntBit;
    }

    TransmitData[0] = (ObjectNr>>8)&0xFF;
    TransmitData[1] = ObjectNr&0xFF;
    TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
    TransmitData[3] = BIT_STRING_DATATYPE;
    TransmitData[4] = 1;
    TransmitData[5] = Mask;

    SendMambaNetMessage(MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
  }
  break;
  }
  DataSize = 0;
}

void DoAxum_BussReset(int BussNr)
{
  int BussWasActive = 0;
  for (int cntModule=0; cntModule<128; cntModule++)
  {
    if (AxumData.ModuleData[cntModule].Buss[BussNr].On)
    {
      BussWasActive = 1;

      AxumData.ModuleData[cntModule].Buss[BussNr].On = 0;

      SetAxum_BussLevels(cntModule);
      SetAxum_ModuleMixMinus(cntModule, 0);

      unsigned int FunctionNrToSent = 0x00000000 | (cntModule<<12);
      CheckObjectsToSent(FunctionNrToSent | (MODULE_FUNCTION_BUSS_1_2_ON_OFF+(BussNr*(MODULE_FUNCTION_BUSS_3_4_ON_OFF-MODULE_FUNCTION_BUSS_1_2_ON_OFF))));

      FunctionNrToSent = ((cntModule<<12)&0xFFF000);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

      if ((AxumData.ModuleData[cntModule].Source>=matrix_sources.src_offset.min.source) && (AxumData.ModuleData[cntModule].Source<=matrix_sources.src_offset.max.source))
      {
        int SourceNr = AxumData.ModuleData[cntModule].Source-matrix_sources.src_offset.min.source;
        FunctionNrToSent = 0x05000000 | (SourceNr<<12);
        CheckObjectsToSent(FunctionNrToSent | (SOURCE_FUNCTION_MODULE_BUSS_1_2_ON+(BussNr*(SOURCE_FUNCTION_MODULE_BUSS_3_4_ON-SOURCE_FUNCTION_MODULE_BUSS_1_2_ON))));
        CheckObjectsToSent(FunctionNrToSent | (SOURCE_FUNCTION_MODULE_BUSS_1_2_OFF+(BussNr*(SOURCE_FUNCTION_MODULE_BUSS_3_4_OFF-SOURCE_FUNCTION_MODULE_BUSS_1_2_OFF))));
        CheckObjectsToSent(FunctionNrToSent | (SOURCE_FUNCTION_MODULE_BUSS_1_2_ON_OFF+(BussNr*(SOURCE_FUNCTION_MODULE_BUSS_3_4_ON_OFF-SOURCE_FUNCTION_MODULE_BUSS_1_2_ON_OFF))));
      }
    }
  }

  //Check buss reset
  unsigned int FunctionNrToSent = 0x04000000;
  CheckObjectsToSent(FunctionNrToSent | (GLOBAL_FUNCTION_BUSS_1_2_RESET+(BussNr*(GLOBAL_FUNCTION_BUSS_3_4_RESET-GLOBAL_FUNCTION_BUSS_1_2_RESET))));

  //Check active buss auto switching
  char BussActive = 0;
  for (int cntModule=0; cntModule<128; cntModule++)
  {
    if (AxumData.ModuleData[cntModule].Buss[BussNr].On)
    {
      BussActive = 1;
    }
  }

  for (int cntMonitorBuss=0; cntMonitorBuss<16; cntMonitorBuss++)
  {
    if (AxumData.Monitor[cntMonitorBuss].AutoSwitchingBuss[BussNr])
    {
      AxumData.Monitor[cntMonitorBuss].Buss[BussNr] = BussActive;
    }
  }

  for (int cntMonitorBuss=0; cntMonitorBuss<16; cntMonitorBuss++)
  {
    unsigned int MonitorBussActive = 0;
    int cntBuss;
    for (cntBuss=0; cntBuss<16; cntBuss++)
    {
      if (AxumData.Monitor[cntMonitorBuss].Buss[cntBuss])
      {
        MonitorBussActive = 1;
      }
    }
    int cntExt;
    for (cntExt=0; cntExt<8; cntExt++)
    {
      if (AxumData.Monitor[cntMonitorBuss].Ext[cntExt])
      {
        MonitorBussActive = 1;
      }
    }

    if (!MonitorBussActive)
    {
      int DefaultSelection = AxumData.Monitor[cntMonitorBuss].DefaultSelection;
      if (DefaultSelection<16)
      {
        int BussNr = DefaultSelection;
        AxumData.Monitor[cntMonitorBuss].Buss[BussNr] = 1;

        unsigned int FunctionNrToSent = 0x02000000 | (cntMonitorBuss<<12);
        CheckObjectsToSent(FunctionNrToSent | (MONITOR_BUSS_FUNCTION_BUSS_1_2_ON_OFF+BussNr));
      }
      else if (DefaultSelection<24)
      {
        int ExtNr = DefaultSelection-16;
        AxumData.Monitor[cntMonitorBuss].Ext[ExtNr] = 1;

        unsigned int FunctionNrToSent = 0x02000000 | (cntMonitorBuss<<12);
        CheckObjectsToSent(FunctionNrToSent | (MONITOR_BUSS_FUNCTION_EXT_1_ON_OFF+ExtNr));
      }
    }
  }

  if (BussWasActive)
  {
    if (  (AxumData.BussMasterData[BussNr].PreModuleOn) &&
          (AxumData.BussMasterData[BussNr].PreModuleLevel))
    {
      printf("Have to check monitor routing and muting\n");
      DoAxum_ModulePreStatusChanged(BussNr);
    }
  }

  for (int cntMonitorBuss=0; cntMonitorBuss<16; cntMonitorBuss++)
  {
    if (AxumData.Monitor[cntMonitorBuss].AutoSwitchingBuss[BussNr])
    {
      SetAxum_MonitorBuss(cntMonitorBuss);

      unsigned int FunctionNrToSent = 0x02000000 | (cntMonitorBuss<<12);
      CheckObjectsToSent(FunctionNrToSent | (MONITOR_BUSS_FUNCTION_BUSS_1_2_ON_OFF+BussNr));
    }
  }
}

void DoAxum_ModuleStatusChanged(int ModuleNr)
{
  unsigned char Redlight[8] = {0,0,0,0,0,0,0,0};
  unsigned char MonitorMute[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

  if ((AxumData.ModuleData[ModuleNr].Source>=matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].Source<=matrix_sources.src_offset.max.source))
  {
    unsigned int SourceNr = AxumData.ModuleData[ModuleNr].Source-matrix_sources.src_offset.min.source;
    unsigned int CurrentSourceActive = AxumData.SourceData[SourceNr].Active;
    unsigned int NewSourceActive = 0;
    if (AxumData.ModuleData[ModuleNr].FaderLevel>-80)
    {   //fader open
      if (AxumData.ModuleData[ModuleNr].On)
      { //module on
        NewSourceActive = 1;
      }
    }

    if (!NewSourceActive)
    {
      for (int cntModule=0; cntModule<128; cntModule++)
      {
        if (AxumData.ModuleData[cntModule].Source == (SourceNr+matrix_sources.src_offset.min.source))
        {
          if (AxumData.ModuleData[cntModule].FaderLevel>-80)
          {   //fader open
            if (AxumData.ModuleData[cntModule].On)
            { //module on
              NewSourceActive = 1;
            }
          }
        }
      }
    }
    if (CurrentSourceActive != NewSourceActive)
    {
      AxumData.SourceData[SourceNr].Active = NewSourceActive;

      //Check current state or redlights/mutes
      for (int cntModule=0; cntModule<128; cntModule++)
      {
        if (AxumData.ModuleData[cntModule].Source != 0)
        {
          int SourceToCheck = AxumData.ModuleData[cntModule].Source-matrix_sources.src_offset.min.source;
          if (AxumData.SourceData[SourceToCheck].Active)
          {
            for (int cntRedlight=0; cntRedlight<8; cntRedlight++)
            {
              if (AxumData.SourceData[SourceToCheck].Redlight[cntRedlight])
              {
                Redlight[cntRedlight] = 1;
              }
            }
            for (int cntMonitorBuss=0; cntMonitorBuss<16; cntMonitorBuss++)
            {
              if (AxumData.SourceData[SourceToCheck].MonitorMute[cntMonitorBuss])
              {
                MonitorMute[cntMonitorBuss] = 1;
              }
            }
          }
        }
      }

      //redlights
      for (int cntRedlight=0; cntRedlight<8; cntRedlight++)
      {
        if (Redlight[cntRedlight] != AxumData.Redlight[cntRedlight])
        {
          AxumData.Redlight[cntRedlight] = Redlight[cntRedlight];

          unsigned int FunctionNrToSent = 0x04000000 | (GLOBAL_FUNCTION_REDLIGHT_1+(cntRedlight*(GLOBAL_FUNCTION_REDLIGHT_2-GLOBAL_FUNCTION_REDLIGHT_1)));
          CheckObjectsToSent(FunctionNrToSent);
        }
      }

      //mutes
      for (int cntMonitorBuss=0; cntMonitorBuss<16; cntMonitorBuss++)
      {
        if (MonitorMute[cntMonitorBuss] != AxumData.Monitor[cntMonitorBuss].Mute)
        {
          AxumData.Monitor[cntMonitorBuss].Mute = MonitorMute[cntMonitorBuss];
          unsigned int FunctionNrToSent = 0x02000000 | (cntMonitorBuss<<12);

          CheckObjectsToSent(FunctionNrToSent | MONITOR_BUSS_FUNCTION_MUTE);
          for (int cntDestination=0; cntDestination<1280; cntDestination++)
          {
            if (AxumData.DestinationData[cntDestination].Source == (unsigned int)(17+cntMonitorBuss))
            {
              FunctionNrToSent = 0x06000000 | (cntDestination<<12);
              CheckObjectsToSent(FunctionNrToSent | DESTINATION_FUNCTION_MUTE_AND_MONITOR_MUTE);
            }
          }
        }
      }
    }
  }

  if (AxumData.ModuleData[ModuleNr].On == 1)
  { //module turned on
    if (AxumData.ModuleData[ModuleNr].FaderLevel>-80)
    {   //fader open
      //Module active, check global buss reset
      for (int cntBuss=0; cntBuss<16; cntBuss++)
      {
        if (AxumData.ModuleData[ModuleNr].Buss[cntBuss].Active)
        {
          if (AxumData.BussMasterData[cntBuss].GlobalBussReset)
          {
            DoAxum_BussReset(cntBuss);
          }
        }
      }
    }
  }
}

void DoAxum_ModulePreStatusChanged(int BussNr)
{
  unsigned char NewMonitorBussDim[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

  for (int cntModule=0; cntModule<128; cntModule++)
  {
    if (AxumData.ModuleData[cntModule].Buss[BussNr].On)
    {
      if ((AxumData.ModuleData[cntModule].Source>=matrix_sources.src_offset.min.source) && (AxumData.ModuleData[cntModule].Source<=matrix_sources.src_offset.max.source))
      {
        int SourceNr = AxumData.ModuleData[cntModule].Source-matrix_sources.src_offset.min.source;

        for (int cntMonitorBuss=0; cntMonitorBuss<16; cntMonitorBuss++)
        {
          if (AxumData.SourceData[SourceNr].MonitorMute[cntMonitorBuss])
          {
            printf("Check routing of buss %d to monitor buss %d\n", BussNr, cntMonitorBuss);
            if (AxumData.Monitor[cntMonitorBuss].Buss[BussNr])
            {
              printf("must be dim!\n");

              NewMonitorBussDim[cntMonitorBuss] = 1;
            }
            else
            {
              printf("must NOT be dim!\n");
              NewMonitorBussDim[cntMonitorBuss] = 0;
            }
          }
        }
      }
    }
  }

  for (int cntMonitorBuss=0; cntMonitorBuss<16; cntMonitorBuss++)
  {
    if (NewMonitorBussDim[cntMonitorBuss] != AxumData.Monitor[cntMonitorBuss].Dim)
    {
      AxumData.Monitor[cntMonitorBuss].Dim = NewMonitorBussDim[cntMonitorBuss];

      unsigned int FunctionNrToSent = 0x02000000 | (cntMonitorBuss<<12);
      CheckObjectsToSent(FunctionNrToSent | MONITOR_BUSS_FUNCTION_DIM);

      for (int cntDestination=0; cntDestination<1280; cntDestination++)
      {
        if (AxumData.DestinationData[cntDestination].Source == (matrix_sources.src_offset.min.monitor_buss+cntMonitorBuss))
        {
          FunctionNrToSent = 0x06000000 | (cntDestination<<12);
          CheckObjectsToSent(FunctionNrToSent | DESTINATION_FUNCTION_DIM_AND_MONITOR_DIM);
        }
      }
    }
  }
}

int Axum_MixMinusSourceUsed(unsigned int CurrentSource)
{
  int ModuleNr = -1;
  int cntModule = 0;
  while ((cntModule<128) && (ModuleNr == -1))
  {
    if (cntModule != ModuleNr)
    {
      if (AxumData.ModuleData[cntModule].Source == (CurrentSource+matrix_sources.src_offset.min.source))
      {
        int cntDestination = 0;
        while ((cntDestination<1280) && (ModuleNr == -1))
        {
          if (AxumData.DestinationData[cntDestination].MixMinusSource == (CurrentSource+matrix_sources.src_offset.min.source))
          {
            ModuleNr = cntModule;
          }
          cntDestination++;
        }
      }
    }
    cntModule++;
  }
  return ModuleNr;
}

void GetSourceLabel(unsigned int SourceNr, char *TextString, int MaxLength)
{
  if (SourceNr == 0)
  {
    strncpy(TextString, "None", MaxLength);
  }
  else if ((SourceNr >= matrix_sources.src_offset.min.buss) && (SourceNr <= matrix_sources.src_offset.max.buss))
  {
    int BussNr = SourceNr-matrix_sources.src_offset.min.buss;
    strncpy(TextString, AxumData.BussMasterData[BussNr].Label, MaxLength);
  }
  else if ((SourceNr >= matrix_sources.src_offset.min.insert_out) && (SourceNr <= matrix_sources.src_offset.max.insert_out))
  {
    int ModuleNr = SourceNr-matrix_sources.src_offset.min.insert_out;
    char InsertText[32];

    //eventual depending on MaxLength
    sprintf(InsertText, "Ins. %3d", ModuleNr+1);

    strncpy(TextString, InsertText, MaxLength);
  }
  else if ((SourceNr >= matrix_sources.src_offset.min.monitor_buss) && (SourceNr <= matrix_sources.src_offset.max.monitor_buss))
  {
    int BussNr = SourceNr-matrix_sources.src_offset.min.monitor_buss;
    strncpy(TextString, AxumData.Monitor[BussNr].Label, MaxLength);
  }
  else if ((SourceNr >= matrix_sources.src_offset.min.mixminus) && (SourceNr <= matrix_sources.src_offset.max.mixminus))
  {
    int ModuleNr = SourceNr-matrix_sources.src_offset.min.mixminus;
    char MixMinusText[32];

    //eventual depending on MaxLength
    sprintf(MixMinusText, "N-1 %3d", ModuleNr+1);

    strncpy(TextString, MixMinusText, MaxLength);
  }
  else if ((SourceNr >= matrix_sources.src_offset.min.source) && (SourceNr <= matrix_sources.src_offset.max.source))
  {
    int LabelSourceNr = SourceNr-matrix_sources.src_offset.min.source;
    strncpy(TextString, AxumData.SourceData[LabelSourceNr].SourceName, MaxLength);
  }
}

unsigned int AdjustModuleSource(unsigned int CurrentSource, int Offset)
{
  char check_for_next_pos;
  int cntPos;
  int CurrentPos;
  int PosBefore;
  int PosAfter;

  //Determin the current position
  CurrentPos = -1;
  PosBefore = -1;
  PosAfter = MAX_POS_LIST_SIZE;
  for (cntPos=0; cntPos<MAX_POS_LIST_SIZE; cntPos++)
  {
    if (matrix_sources.pos[cntPos].src != -1)
    {
      if (matrix_sources.pos[cntPos].src == (signed short int)CurrentSource)
      {
        CurrentPos = cntPos;
      }
      else if (matrix_sources.pos[cntPos].src < (signed short int)CurrentSource)
      {
        if (cntPos>PosBefore)
        {
          PosBefore = cntPos;
        }
      }
      else if (matrix_sources.pos[cntPos].src > (signed short int)CurrentSource)
      {
        if (cntPos<PosAfter)
        {
          PosAfter = cntPos;
        }
      }
    }
  }

  //If current position not found...
  if (CurrentPos == -1)
  {
    if (Offset<0)
    {
      CurrentPos = PosBefore;
    }
    else
    {
      CurrentPos = PosAfter;
    }
  }

  if (CurrentPos != -1)
  {
    while (Offset != 0)
    {
      if (Offset>0)
      {
        check_for_next_pos = 1;
        while (check_for_next_pos)
        {
          check_for_next_pos = 0;

          CurrentPos++;
          if (CurrentPos>MAX_POS_LIST_SIZE)
          {
            CurrentPos = 0;
          }

          CurrentSource = matrix_sources.pos[CurrentPos].src;

          //check if hybrid is used
          if (CurrentSource != 0)
          {
            if (Axum_MixMinusSourceUsed(CurrentSource) != -1)
            {
              check_for_next_pos = 1;
            }
          }

          //not active, go further.
          if (!matrix_sources.pos[CurrentPos].active)
          {
            check_for_next_pos = 1;
          }
        }
        Offset--;
      }
      else
      {
        check_for_next_pos = 1;
        while (check_for_next_pos)
        {
          check_for_next_pos = 0;

          CurrentPos--;
          if (CurrentPos<0)
          {
            CurrentPos = MAX_POS_LIST_SIZE;
          }

          CurrentSource = matrix_sources.pos[CurrentPos].src;

          //check if hybrid is used
          if (CurrentSource != 0)
          {
            if (Axum_MixMinusSourceUsed(CurrentSource) != -1)
            {
              check_for_next_pos = 1;
            }
          }

          //not active, go further.
          if (!matrix_sources.pos[CurrentPos].active)
          {
            check_for_next_pos = 1;
          }
        }
        Offset++;
      }
    }
  }

  return CurrentSource;
}

void SetNewSource(int ModuleNr, unsigned int NewSource, int Forced, int ApplyAorBSettings)
{
  unsigned int OldSource = AxumData.ModuleData[ModuleNr].Source;
  int OldSourceActive = 0;

  if (OldSource != NewSource)
  {
    if (AxumData.ModuleData[ModuleNr].On)
    {
      if (AxumData.ModuleData[ModuleNr].FaderLevel>-80)
      {
        OldSourceActive = 1;
      }
    }

    if ((!OldSourceActive) || (Forced))
    {
      AxumData.ModuleData[ModuleNr].Source = NewSource;

      //eventual 'reset' set a preset?

      SetAxum_ModuleSource(ModuleNr);
      SetAxum_ModuleMixMinus(ModuleNr, OldSource);

      unsigned int FunctionNrToSent = (ModuleNr<<12);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_SOURCE_A);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_SOURCE_B);

      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_INSERT_ON_OFF);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_LOW_CUT_FREQUENCY);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_EQ_ON_OFF);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_DYNAMICS_ON_OFF);

      if ((OldSource>=matrix_sources.src_offset.min.source) && (OldSource<=matrix_sources.src_offset.max.source))
      {
        unsigned int SourceNr = OldSource-matrix_sources.src_offset.min.source;
        FunctionNrToSent = 0x05000000 | (SourceNr<<12);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_FADER_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_FADER_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_FADER_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_FADER_AND_ON_ACTIVE);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_FADER_AND_ON_INACTIVE);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_1_2_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_1_2_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_1_2_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_3_4_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_3_4_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_3_4_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_5_6_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_5_6_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_5_6_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_7_8_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_7_8_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_7_8_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_9_10_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_9_10_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_9_10_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_11_12_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_11_12_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_11_12_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_13_14_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_13_14_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_13_14_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_15_16_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_15_16_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_15_16_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_17_18_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_17_18_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_17_18_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_19_20_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_19_20_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_19_20_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_21_22_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_21_22_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_21_22_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_23_24_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_23_24_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_23_24_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_25_26_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_25_26_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_25_26_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_27_28_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_27_28_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_27_28_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_29_30_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_29_30_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_29_30_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_31_32_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_31_32_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_31_32_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_COUGH_ON_OFF);
      }

      if ((NewSource>=matrix_sources.src_offset.min.source) && (NewSource<=matrix_sources.src_offset.max.source))
      {
        unsigned int SourceNr = NewSource-matrix_sources.src_offset.min.source;
        FunctionNrToSent = 0x05000000 | (SourceNr<<12);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_FADER_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_FADER_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_FADER_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_FADER_AND_ON_ACTIVE);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_FADER_AND_ON_INACTIVE);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_1_2_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_1_2_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_1_2_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_3_4_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_3_4_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_3_4_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_5_6_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_5_6_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_5_6_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_7_8_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_7_8_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_7_8_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_9_10_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_9_10_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_9_10_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_11_12_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_11_12_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_11_12_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_13_14_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_13_14_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_13_14_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_15_16_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_15_16_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_15_16_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_17_18_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_17_18_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_17_18_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_19_20_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_19_20_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_19_20_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_21_22_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_21_22_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_21_22_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_23_24_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_23_24_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_23_24_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_25_26_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_25_26_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_25_26_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_27_28_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_27_28_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_27_28_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_29_30_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_29_30_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_29_30_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_31_32_ON);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_31_32_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_31_32_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_COUGH_ON_OFF);
      }

      for (int cntModule=0; cntModule<128; cntModule++)
      {
        if (AxumData.ModuleData[cntModule].Source == AxumData.ModuleData[ModuleNr].Source)
        {
          FunctionNrToSent = (cntModule<<12);
          CheckObjectsToSent(FunctionNrToSent+MODULE_FUNCTION_SOURCE);
          CheckObjectsToSent(FunctionNrToSent+MODULE_FUNCTION_SOURCE_PHANTOM);
          CheckObjectsToSent(FunctionNrToSent+MODULE_FUNCTION_SOURCE_PAD);
          CheckObjectsToSent(FunctionNrToSent+MODULE_FUNCTION_SOURCE_GAIN_LEVEL);
          CheckObjectsToSent(FunctionNrToSent+MODULE_FUNCTION_SOURCE_GAIN_LEVEL_RESET);
          CheckObjectsToSent(FunctionNrToSent+MODULE_FUNCTION_SOURCE_START);
          CheckObjectsToSent(FunctionNrToSent+MODULE_FUNCTION_SOURCE_STOP);
          CheckObjectsToSent(FunctionNrToSent+MODULE_FUNCTION_SOURCE_START_STOP);
          CheckObjectsToSent(FunctionNrToSent+MODULE_FUNCTION_SOURCE_ALERT);
        }
      }
    }
  }
  ApplyAorBSettings = 0;
}

void SetBussOnOff(int ModuleNr, int BussNr, int UseInterlock)
{
  SetAxum_BussLevels(ModuleNr);
  SetAxum_ModuleMixMinus(ModuleNr, 0);

  unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
  CheckObjectsToSent(FunctionNrToSent | (MODULE_FUNCTION_BUSS_1_2_ON_OFF+(BussNr*(MODULE_FUNCTION_BUSS_3_4_ON_OFF-MODULE_FUNCTION_BUSS_1_2_ON_OFF))));

  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

  if ((AxumData.ModuleData[ModuleNr].Source>=matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].Source<=matrix_sources.src_offset.max.source))
  {
    int SourceNr = AxumData.ModuleData[ModuleNr].Source-matrix_sources.src_offset.min.source;
    FunctionNrToSent = 0x05000000 | (SourceNr<<12);
    CheckObjectsToSent(FunctionNrToSent | (SOURCE_FUNCTION_MODULE_BUSS_1_2_ON+(BussNr*(SOURCE_FUNCTION_MODULE_BUSS_3_4_ON-SOURCE_FUNCTION_MODULE_BUSS_1_2_ON))));
    CheckObjectsToSent(FunctionNrToSent | (SOURCE_FUNCTION_MODULE_BUSS_1_2_OFF+(BussNr*(SOURCE_FUNCTION_MODULE_BUSS_3_4_OFF-SOURCE_FUNCTION_MODULE_BUSS_1_2_OFF))));
    CheckObjectsToSent(FunctionNrToSent | (SOURCE_FUNCTION_MODULE_BUSS_1_2_ON_OFF+(BussNr*(SOURCE_FUNCTION_MODULE_BUSS_3_4_ON_OFF-SOURCE_FUNCTION_MODULE_BUSS_1_2_ON_OFF))));
  }

  //Do interlock
  if ((AxumData.BussMasterData[BussNr].Interlock) && (UseInterlock))
  {
    for (int cntModule=0; cntModule<128; cntModule++)
    {
      if (ModuleNr != cntModule)
      {
        if (AxumData.ModuleData[cntModule].Buss[BussNr].On)
        {
          AxumData.ModuleData[cntModule].Buss[BussNr].On = 0;

          SetAxum_BussLevels(cntModule);
          SetAxum_ModuleMixMinus(cntModule, 0);

          unsigned int FunctionNrToSent = ((cntModule<<12)&0xFFF000);
          CheckObjectsToSent(FunctionNrToSent | (MODULE_FUNCTION_BUSS_1_2_ON_OFF+(BussNr*(MODULE_FUNCTION_BUSS_3_4_ON_OFF-MODULE_FUNCTION_BUSS_1_2_ON_OFF))));

          FunctionNrToSent = ((cntModule<<12)&0xFFF000);
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

          if ((AxumData.ModuleData[cntModule].Source>=matrix_sources.src_offset.min.source) && (AxumData.ModuleData[cntModule].Source<=matrix_sources.src_offset.max.source))
          {
            int SourceNr = AxumData.ModuleData[cntModule].Source-matrix_sources.src_offset.min.source;
            FunctionNrToSent = 0x05000000 | (SourceNr<<12);
            CheckObjectsToSent(FunctionNrToSent | (SOURCE_FUNCTION_MODULE_BUSS_1_2_ON+(BussNr*(SOURCE_FUNCTION_MODULE_BUSS_3_4_ON-SOURCE_FUNCTION_MODULE_BUSS_1_2_ON))));
            CheckObjectsToSent(FunctionNrToSent | (SOURCE_FUNCTION_MODULE_BUSS_1_2_OFF+(BussNr*(SOURCE_FUNCTION_MODULE_BUSS_3_4_OFF-SOURCE_FUNCTION_MODULE_BUSS_1_2_OFF))));
            CheckObjectsToSent(FunctionNrToSent | (SOURCE_FUNCTION_MODULE_BUSS_1_2_ON_OFF+(BussNr*(SOURCE_FUNCTION_MODULE_BUSS_3_4_ON_OFF-SOURCE_FUNCTION_MODULE_BUSS_1_2_ON_OFF))));
          }
        }
      }
    }
  }

  //Check buss reset
  FunctionNrToSent = 0x04000000;
  CheckObjectsToSent(FunctionNrToSent | (GLOBAL_FUNCTION_BUSS_1_2_RESET+(BussNr*(GLOBAL_FUNCTION_BUSS_3_4_RESET-GLOBAL_FUNCTION_BUSS_1_2_RESET))));

  //Check active buss auto switching
  char BussActive = 0;
  for (int cntModule=0; cntModule<128; cntModule++)
  {
    if (AxumData.ModuleData[cntModule].Buss[BussNr].On)
    {
      BussActive = 1;
    }
  }

  //Set data, make functional after eventual monitor mute is set
  for (int cntMonitorBuss=0; cntMonitorBuss<16; cntMonitorBuss++)
  {
    if (AxumData.Monitor[cntMonitorBuss].AutoSwitchingBuss[BussNr])
    {
      AxumData.Monitor[cntMonitorBuss].Buss[BussNr] = BussActive;
    }
  }

  for (int cntMonitorBuss=0; cntMonitorBuss<16; cntMonitorBuss++)
  {
    unsigned int MonitorBussActive = 0;
    int cntBuss;
    for (cntBuss=0; cntBuss<16; cntBuss++)
    {
      if (AxumData.Monitor[cntMonitorBuss].Buss[cntBuss])
      {
        MonitorBussActive = 1;
      }
    }
    int cntExt;
    for (cntExt=0; cntExt<8; cntExt++)
    {
      if (AxumData.Monitor[cntMonitorBuss].Ext[cntExt])
      {
        MonitorBussActive = 1;
      }
    }

    if (!MonitorBussActive)
    {
      int DefaultSelection = AxumData.Monitor[cntMonitorBuss].DefaultSelection;
      if (DefaultSelection<16)
      {
        int BussNr = DefaultSelection;
        AxumData.Monitor[cntMonitorBuss].Buss[BussNr] = 1;

        unsigned int FunctionNrToSent = 0x02000000 | (cntMonitorBuss<<12);
        CheckObjectsToSent(FunctionNrToSent | (MONITOR_BUSS_FUNCTION_BUSS_1_2_ON_OFF+BussNr));
      }
      else if (DefaultSelection<24)
      {
        int ExtNr = DefaultSelection-16;
        AxumData.Monitor[cntMonitorBuss].Ext[ExtNr] = 1;

        unsigned int FunctionNrToSent = 0x02000000 | (cntMonitorBuss<<12);
        CheckObjectsToSent(FunctionNrToSent | (MONITOR_BUSS_FUNCTION_EXT_1_ON_OFF+ExtNr));
      }
    }
  }

  if (  (AxumData.BussMasterData[BussNr].PreModuleOn) &&
        (AxumData.BussMasterData[BussNr].PreModuleLevel))
  {
    printf("Have to check monitor routing and muting\n");
    DoAxum_ModulePreStatusChanged(BussNr);
  }

  //make functional because the eventual monitor mute is already set
  for (int cntMonitorBuss=0; cntMonitorBuss<16; cntMonitorBuss++)
  {
    if (AxumData.Monitor[cntMonitorBuss].AutoSwitchingBuss[BussNr])
    {
      SetAxum_MonitorBuss(cntMonitorBuss);

      unsigned int FunctionNrToSent = 0x02000000 | (cntMonitorBuss<<12);
      CheckObjectsToSent(FunctionNrToSent | (MONITOR_BUSS_FUNCTION_BUSS_1_2_ON_OFF+BussNr));
    }
  }
}

void initialize_axum_data_struct()
{
  for (int cntSource=0; cntSource<1280; cntSource++)
  {
    AxumData.SourceData[cntSource].SourceName[0] = 0x00;
    for (int cntInput=0; cntInput<8; cntInput++)
    {
      AxumData.SourceData[cntSource].InputData[cntInput].MambaNetAddress = 0x00000000;
      AxumData.SourceData[cntSource].InputData[cntInput].SubChannel = -1;
    }

    AxumData.SourceData[cntSource].Preset.UseGain = false;
    AxumData.SourceData[cntSource].Preset.Gain = 0;

    AxumData.SourceData[cntSource].Preset.UseFilter = false;
    AxumData.SourceData[cntSource].Preset.Filter.Level = 0;
    AxumData.SourceData[cntSource].Preset.Filter.Frequency = 80;
    AxumData.SourceData[cntSource].Preset.Filter.Bandwidth = 1;
    AxumData.SourceData[cntSource].Preset.Filter.Slope = 1;
    AxumData.SourceData[cntSource].Preset.Filter.Type = HPF;
    AxumData.SourceData[cntSource].Preset.FilterOnOff = false;

    AxumData.SourceData[cntSource].Preset.UseInsert = false;
    AxumData.SourceData[cntSource].Preset.InsertSource = 0;
    AxumData.SourceData[cntSource].Preset.InsertOnOff = false;

    AxumData.SourceData[cntSource].Preset.UseEQ = false;

    AxumData.SourceData[cntSource].Preset.EQBand[0].Level = 0;
    AxumData.SourceData[cntSource].Preset.EQBand[0].Frequency = 12000;
    AxumData.SourceData[cntSource].Preset.EQBand[0].Bandwidth = 1;
    AxumData.SourceData[cntSource].Preset.EQBand[0].Slope = 1;
    AxumData.SourceData[cntSource].Preset.EQBand[0].Type = PEAKINGEQ;

    AxumData.SourceData[cntSource].Preset.EQBand[1].Level = 0;
    AxumData.SourceData[cntSource].Preset.EQBand[1].Frequency = 4000;
    AxumData.SourceData[cntSource].Preset.EQBand[1].Bandwidth = 1;
    AxumData.SourceData[cntSource].Preset.EQBand[1].Slope = 1;
    AxumData.SourceData[cntSource].Preset.EQBand[1].Type = PEAKINGEQ;

    AxumData.SourceData[cntSource].Preset.EQBand[2].Level = 0;
    AxumData.SourceData[cntSource].Preset.EQBand[2].Frequency = 800;
    AxumData.SourceData[cntSource].Preset.EQBand[2].Bandwidth = 1;
    AxumData.SourceData[cntSource].Preset.EQBand[2].Slope = 1;
    AxumData.SourceData[cntSource].Preset.EQBand[2].Type = PEAKINGEQ;

    AxumData.SourceData[cntSource].Preset.EQBand[3].Level = 0;
    AxumData.SourceData[cntSource].Preset.EQBand[3].Frequency = 120;
    AxumData.SourceData[cntSource].Preset.EQBand[3].Bandwidth = 1;
    AxumData.SourceData[cntSource].Preset.EQBand[3].Slope = 1;
    AxumData.SourceData[cntSource].Preset.EQBand[3].Type = LOWSHELF;

    AxumData.SourceData[cntSource].Preset.EQBand[4].Level = 0;
    AxumData.SourceData[cntSource].Preset.EQBand[4].Frequency = 300;
    AxumData.SourceData[cntSource].Preset.EQBand[4].Bandwidth = 1;
    AxumData.SourceData[cntSource].Preset.EQBand[4].Slope = 1;
    AxumData.SourceData[cntSource].Preset.EQBand[4].Type = HPF;

    AxumData.SourceData[cntSource].Preset.EQBand[5].Level = 0;
    AxumData.SourceData[cntSource].Preset.EQBand[5].Frequency = 3000;
    AxumData.SourceData[cntSource].Preset.EQBand[5].Bandwidth = 1;
    AxumData.SourceData[cntSource].Preset.EQBand[5].Slope = 1;
    AxumData.SourceData[cntSource].Preset.EQBand[5].Type = LPF;
    AxumData.SourceData[cntSource].Preset.EQOnOff = false;

    AxumData.SourceData[cntSource].Preset.UseDynamics = false;
    AxumData.SourceData[cntSource].Preset.Dynamics = 0;
    AxumData.SourceData[cntSource].Preset.DynamicsOnOff = false;

    AxumData.SourceData[cntSource].Preset.UseRouting = false;
    AxumData.SourceData[cntSource].Preset.RoutingPreset = 0;

    for (int cntRedlight=0; cntRedlight<8; cntRedlight++)
    {
      AxumData.SourceData[cntSource].Redlight[cntRedlight] = 0;
    }
    for (int cntMonitorMute=0; cntMonitorMute<16; cntMonitorMute++)
    {
      AxumData.SourceData[cntSource].MonitorMute[cntMonitorMute] = 0;
    }
    AxumData.SourceData[cntSource].Active = 0;
    AxumData.SourceData[cntSource].Start = 0;
    AxumData.SourceData[cntSource].Phantom = 0;
    AxumData.SourceData[cntSource].Pad = 0;
    AxumData.SourceData[cntSource].Gain = 30;
    AxumData.SourceData[cntSource].Alert = 0;
  }

  for (int cntDestination=0; cntDestination<1280; cntDestination++)
  {
    AxumData.DestinationData[cntDestination].DestinationName[0] = 0x00;
    for (int cntOutput=0; cntOutput<8; cntOutput++)
    {
      AxumData.DestinationData[cntDestination].OutputData[cntOutput].MambaNetAddress = 0x00000000;
      AxumData.DestinationData[cntDestination].OutputData[cntOutput].SubChannel = -1;
    }

    AxumData.DestinationData[cntDestination].Source = 0;
    AxumData.DestinationData[cntDestination].Level = -140;
    AxumData.DestinationData[cntDestination].Mute = 0;
    AxumData.DestinationData[cntDestination].Dim = 0;
    AxumData.DestinationData[cntDestination].Mono = 0;
    AxumData.DestinationData[cntDestination].Phase = 0;
    AxumData.DestinationData[cntDestination].Talkback[0] = 0;
    AxumData.DestinationData[cntDestination].Talkback[1] = 0;
    AxumData.DestinationData[cntDestination].Talkback[2] = 0;
    AxumData.DestinationData[cntDestination].Talkback[3] = 0;

    AxumData.DestinationData[cntDestination].MixMinusSource = 0;
    AxumData.DestinationData[cntDestination].MixMinusActive = 0;

  }

  for (int cntModule=0; cntModule<128; cntModule++)
  {
    AxumData.ModuleData[cntModule].Source = 0;
    AxumData.ModuleData[cntModule].SourceA = 0;
    AxumData.ModuleData[cntModule].SourceB = 0;
    AxumData.ModuleData[cntModule].SourceC = 0;
    AxumData.ModuleData[cntModule].SourceD = 0;
    AxumData.ModuleData[cntModule].Insert = 0;
    AxumData.ModuleData[cntModule].InsertOnOff = 0;
    AxumData.ModuleData[cntModule].Gain = 0;
    AxumData.ModuleData[cntModule].PhaseReverse = 0;

    AxumData.ModuleData[cntModule].Filter.On = 0;
    AxumData.ModuleData[cntModule].Filter.Level = 0;
    AxumData.ModuleData[cntModule].Filter.Frequency = 80;
    AxumData.ModuleData[cntModule].Filter.Bandwidth = 1;
    AxumData.ModuleData[cntModule].Filter.Slope = 1;
    AxumData.ModuleData[cntModule].Filter.Type = HPF;
    AxumData.ModuleData[cntModule].FilterOnOff = 0;

    AxumData.ModuleData[cntModule].EQBand[0].On = 1;
    AxumData.ModuleData[cntModule].EQBand[0].Level = 0;
    AxumData.ModuleData[cntModule].EQBand[0].Frequency = 12000;
    AxumData.ModuleData[cntModule].EQBand[0].Bandwidth = 1;
    AxumData.ModuleData[cntModule].EQBand[0].Slope = 1;
    AxumData.ModuleData[cntModule].EQBand[0].Type = PEAKINGEQ;

    AxumData.ModuleData[cntModule].EQBand[1].On = 1;
    AxumData.ModuleData[cntModule].EQBand[1].Level = 0;
    AxumData.ModuleData[cntModule].EQBand[1].Frequency = 4000;
    AxumData.ModuleData[cntModule].EQBand[1].Bandwidth = 1;
    AxumData.ModuleData[cntModule].EQBand[1].Slope = 1;
    AxumData.ModuleData[cntModule].EQBand[1].Type = PEAKINGEQ;

    AxumData.ModuleData[cntModule].EQBand[2].On = 1;
    AxumData.ModuleData[cntModule].EQBand[2].Level = 0;
    AxumData.ModuleData[cntModule].EQBand[2].Frequency = 800;
    AxumData.ModuleData[cntModule].EQBand[2].Bandwidth = 1;
    AxumData.ModuleData[cntModule].EQBand[2].Slope = 1;
    AxumData.ModuleData[cntModule].EQBand[2].Type = PEAKINGEQ;

    AxumData.ModuleData[cntModule].EQBand[3].On = 1;
    AxumData.ModuleData[cntModule].EQBand[3].Level = 0;
    AxumData.ModuleData[cntModule].EQBand[3].Frequency = 120;
    AxumData.ModuleData[cntModule].EQBand[3].Bandwidth = 1;
    AxumData.ModuleData[cntModule].EQBand[3].Slope = 1;
    AxumData.ModuleData[cntModule].EQBand[3].Type = LOWSHELF;

    AxumData.ModuleData[cntModule].EQBand[4].On = 0;
    AxumData.ModuleData[cntModule].EQBand[4].Level = 0;
    AxumData.ModuleData[cntModule].EQBand[4].Frequency = 300;
    AxumData.ModuleData[cntModule].EQBand[4].Bandwidth = 1;
    AxumData.ModuleData[cntModule].EQBand[4].Slope = 1;
    AxumData.ModuleData[cntModule].EQBand[4].Type = HPF;

    AxumData.ModuleData[cntModule].EQBand[5].On = 0;
    AxumData.ModuleData[cntModule].EQBand[5].Level = 0;
    AxumData.ModuleData[cntModule].EQBand[5].Frequency = 3000;
    AxumData.ModuleData[cntModule].EQBand[5].Bandwidth = 1;
    AxumData.ModuleData[cntModule].EQBand[5].Slope = 1;
    AxumData.ModuleData[cntModule].EQBand[5].Type = LPF;

    AxumData.ModuleData[cntModule].EQOn = 1;
    AxumData.ModuleData[cntModule].EQOnOff = 0;

    AxumData.ModuleData[cntModule].Dynamics = 0;
    AxumData.ModuleData[cntModule].DynamicsOn = 0;
    AxumData.ModuleData[cntModule].DynamicsOnOff = 0;

    AxumData.ModuleData[cntModule].Panorama = 512;
    AxumData.ModuleData[cntModule].Mono = 0;

    AxumData.ModuleData[cntModule].FaderLevel = -140;
    AxumData.ModuleData[cntModule].FaderTouch = 0;
    AxumData.ModuleData[cntModule].On = 0;
    AxumData.ModuleData[cntModule].Cough = 0;

    AxumData.ModuleData[cntModule].Signal = 0;
    AxumData.ModuleData[cntModule].Peak = 0;

    for (int cntBuss=0; cntBuss<16; cntBuss++)
    {
      AxumData.ModuleData[cntModule].Buss[cntBuss].Level = 0; //0dB
      AxumData.ModuleData[cntModule].Buss[cntBuss].On = 0;
      AxumData.ModuleData[cntModule].Buss[cntBuss].Balance = 512;
      AxumData.ModuleData[cntModule].Buss[cntBuss].PreModuleLevel = 0;
      AxumData.ModuleData[cntModule].Buss[cntBuss].Active = 1;

      for (int cntPreset=0; cntPreset<8; cntPreset++)
      {
        AxumData.ModuleData[cntModule].RoutingPreset[cntPreset][cntBuss].Level = 0; //0dB
        AxumData.ModuleData[cntModule].RoutingPreset[cntPreset][cntBuss].On = 0;
        AxumData.ModuleData[cntModule].RoutingPreset[cntPreset][cntBuss].Balance = 512;
        AxumData.ModuleData[cntModule].RoutingPreset[cntPreset][cntBuss].PreModuleLevel = 0;
      }
    }
  }

  AxumData.Control1Mode = MODULE_CONTROL_MODE_NONE;
  AxumData.Control2Mode = MODULE_CONTROL_MODE_NONE;
  AxumData.Control3Mode = MODULE_CONTROL_MODE_NONE;
  AxumData.Control4Mode = MODULE_CONTROL_MODE_NONE;
  AxumData.MasterControl1Mode = MASTER_CONTROL_MODE_BUSS_1_2;
  AxumData.MasterControl2Mode = MASTER_CONTROL_MODE_BUSS_1_2;
  AxumData.MasterControl3Mode = MASTER_CONTROL_MODE_BUSS_1_2;
  AxumData.MasterControl4Mode = MASTER_CONTROL_MODE_BUSS_1_2;

  for (int cntBuss=0; cntBuss<16; cntBuss++)
  {
    AxumData.BussMasterData[cntBuss].Label[0] = 0;
    AxumData.BussMasterData[cntBuss].Level = 0;
    AxumData.BussMasterData[cntBuss].On = 1;

    AxumData.BussMasterData[cntBuss].PreModuleOn = 0;
    AxumData.BussMasterData[cntBuss].PreModuleLevel = 0;
    AxumData.BussMasterData[cntBuss].PreModuleBalance = 0;

    AxumData.BussMasterData[cntBuss].Interlock = 0;
    AxumData.BussMasterData[cntBuss].GlobalBussReset = 0;
  }

  AxumData.Redlight[0] = 0;
  AxumData.Redlight[1] = 0;
  AxumData.Redlight[2] = 0;
  AxumData.Redlight[3] = 0;
  AxumData.Redlight[4] = 0;
  AxumData.Redlight[5] = 0;
  AxumData.Redlight[6] = 0;
  AxumData.Redlight[7] = 0;

  AxumData.Samplerate = 48000;
  AxumData.ExternClock = 0;
  AxumData.Headroom = -20;
  AxumData.LevelReserve = 0;
  for (int cntTalkback=0; cntTalkback<16; cntTalkback++)
  {
    AxumData.Talkback[cntTalkback].Source = 0;
  }

  for (int cntMonitor=0; cntMonitor<16; cntMonitor++)
  {
    AxumData.Monitor[cntMonitor].Label[0] = 0;
    AxumData.Monitor[cntMonitor].Interlock = 0;
    AxumData.Monitor[cntMonitor].DefaultSelection = 0;
    AxumData.Monitor[cntMonitor].AutoSwitchingBuss[0] = 0;
    AxumData.Monitor[cntMonitor].AutoSwitchingBuss[1] = 0;
    AxumData.Monitor[cntMonitor].AutoSwitchingBuss[2] = 0;
    AxumData.Monitor[cntMonitor].AutoSwitchingBuss[3] = 0;
    AxumData.Monitor[cntMonitor].AutoSwitchingBuss[4] = 0;
    AxumData.Monitor[cntMonitor].AutoSwitchingBuss[5] = 0;
    AxumData.Monitor[cntMonitor].AutoSwitchingBuss[6] = 0;
    AxumData.Monitor[cntMonitor].AutoSwitchingBuss[7] = 0;
    AxumData.Monitor[cntMonitor].AutoSwitchingBuss[8] = 0;
    AxumData.Monitor[cntMonitor].AutoSwitchingBuss[9] = 0;
    AxumData.Monitor[cntMonitor].AutoSwitchingBuss[10] = 0;
    AxumData.Monitor[cntMonitor].AutoSwitchingBuss[11] = 0;
    AxumData.Monitor[cntMonitor].AutoSwitchingBuss[12] = 0;
    AxumData.Monitor[cntMonitor].AutoSwitchingBuss[13] = 0;
    AxumData.Monitor[cntMonitor].AutoSwitchingBuss[14] = 0;
    AxumData.Monitor[cntMonitor].AutoSwitchingBuss[15] = 0;
    AxumData.Monitor[cntMonitor].SwitchingDimLevel = -140;
    AxumData.Monitor[cntMonitor].Buss[0] = 0;
    AxumData.Monitor[cntMonitor].Buss[1] = 0;
    AxumData.Monitor[cntMonitor].Buss[2] = 0;
    AxumData.Monitor[cntMonitor].Buss[3] = 0;
    AxumData.Monitor[cntMonitor].Buss[4] = 0;
    AxumData.Monitor[cntMonitor].Buss[5] = 0;
    AxumData.Monitor[cntMonitor].Buss[6] = 0;
    AxumData.Monitor[cntMonitor].Buss[7] = 0;
    AxumData.Monitor[cntMonitor].Buss[8] = 0;
    AxumData.Monitor[cntMonitor].Buss[9] = 0;
    AxumData.Monitor[cntMonitor].Buss[10] = 0;
    AxumData.Monitor[cntMonitor].Buss[11] = 0;
    AxumData.Monitor[cntMonitor].Buss[12] = 0;
    AxumData.Monitor[cntMonitor].Buss[13] = 0;
    AxumData.Monitor[cntMonitor].Buss[14] = 0;
    AxumData.Monitor[cntMonitor].Buss[15] = 0;
    AxumData.Monitor[cntMonitor].Ext[0] = 0;
    AxumData.Monitor[cntMonitor].Ext[1] = 0;
    AxumData.Monitor[cntMonitor].Ext[2] = 0;
    AxumData.Monitor[cntMonitor].Ext[3] = 0;
    AxumData.Monitor[cntMonitor].Ext[4] = 0;
    AxumData.Monitor[cntMonitor].Ext[5] = 0;
    AxumData.Monitor[cntMonitor].Ext[6] = 0;
    AxumData.Monitor[cntMonitor].Ext[7] = 0;
    AxumData.Monitor[cntMonitor].PhonesLevel = -140;
    AxumData.Monitor[cntMonitor].SpeakerLevel = -140;
    AxumData.Monitor[cntMonitor].Dim = 0;
    AxumData.Monitor[cntMonitor].Mute = 0;
  }

  for (int cntMonitor=0; cntMonitor<4; cntMonitor++)
  {
    AxumData.ExternSource[cntMonitor].Ext[0] = 0;
    AxumData.ExternSource[cntMonitor].Ext[1] = 0;
    AxumData.ExternSource[cntMonitor].Ext[2] = 0;
    AxumData.ExternSource[cntMonitor].Ext[3] = 0;
    AxumData.ExternSource[cntMonitor].Ext[4] = 0;
    AxumData.ExternSource[cntMonitor].Ext[5] = 0;
    AxumData.ExternSource[cntMonitor].Ext[6] = 0;
    AxumData.ExternSource[cntMonitor].Ext[7] = 0;
  }
}

ONLINE_NODE_INFORMATION_STRUCT *GetOnlineNodeInformation(unsigned long int addr)
{
  unsigned int Index;
  unsigned char cntIndex;
  unsigned char NodeFound;

  NodeFound = 0;
  for (cntIndex=0; cntIndex<AddressTableCount; cntIndex++)
  {
    if (OnlineNodeInformation[cntIndex].MambaNetAddress == addr)
    {
      Index = cntIndex;
      NodeFound = 1;
    }
  }

  if (NodeFound == 0)
  {
    return NULL;
  }
  return &OnlineNodeInformation[Index];
}
