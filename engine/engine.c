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

#include <sqlite3.h>

#include <pthread.h>
#include <time.h>
#include <sys/errno.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/msg.h>

#include <libpq-fe.h>       //postgres library

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

#define DEFAULT_TIME_BEFORE_MOMENTARY 750

int AxumApplicationAndDSPInitialized = 0;

int cntDebugObject=1024;
int cntDebugNodeObject=0;
float cntFloatDebug = 0;

unsigned char VUMeter = 0;
unsigned char MeterFrequency = 5; //20Hz
unsigned int ModuleDSPEntryPoint                = 0x00000000;
unsigned int ModuleDSPRoutingFrom               = 0x00000000;
unsigned int ModuleDSPUpdate_InputGainFactor    = 0x00000000;
unsigned int ModuleDSPUpdate_LevelFactor        = 0x00000000;
unsigned int ModuleDSPFilterCoefficients        = 0x00000000;
unsigned int ModuleDSPEQCoefficients            = 0x00000000;
unsigned int ModuleDSPDynamicsOriginalFactor    = 0x00000000;
unsigned int ModuleDSPDynamicsProcessedFactor   = 0x00000000;
unsigned int ModuleDSPMeterPPM                  = 0x00000000;
unsigned int ModuleDSPMeterVU                   = 0x00000000;
unsigned int ModuleDSPPhaseRMS                  = 0x00000000;
unsigned int ModuleDSPSmoothFactor              = 0x00000000;
unsigned int ModuleDSPPPMReleaseFactor          = 0x00000000;
unsigned int ModuleDSPVUReleaseFactor           = 0x00000000;
unsigned int ModuleDSPRMSReleaseFactor          = 0x00000000;

unsigned int SummingDSPEntryPoint               = 0x00000000;
unsigned int SummingDSPUpdate_MatrixFactor      = 0x00000000;
unsigned int SummingDSPBussMeterPPM             = 0x00000000;
unsigned int SummingDSPBussMeterVU              = 0x00000000;
unsigned int SummingDSPPhaseRMS                 = 0x00000000;
unsigned int SummingDSPSelectedMixMinusBuss     = 0x00000000;
unsigned int SummingDSPSmoothFactor             = 0x00000000;
unsigned int SummingDSPVUReleaseFactor          = 0x00000000;
unsigned int SummingDSPPhaseRelease             = 0x00000000;

unsigned int FXDSPEntryPoint                    = 0x00000000;

int FirstModuleOnSurface = 0;

AXUM_DATA_STRUCT AxumData;

float SummingdBLevel[48];
unsigned int BackplaneMambaNetAddress = 0x00000000;

DSPCARD_DATA_STRUCT DSPCardData[4];

AXUM_FUNCTION_INFORMATION_STRUCT *SourceFunctions[NUMBER_OF_SOURCES][NUMBER_OF_SOURCE_FUNCTIONS];
AXUM_FUNCTION_INFORMATION_STRUCT *ModuleFunctions[NUMBER_OF_MODULES][NUMBER_OF_MODULE_FUNCTIONS];
AXUM_FUNCTION_INFORMATION_STRUCT *BussFunctions[NUMBER_OF_BUSSES][NUMBER_OF_BUSS_FUNCTIONS];
AXUM_FUNCTION_INFORMATION_STRUCT *MonitorBussFunctions[NUMBER_OF_MONITOR_BUSSES][NUMBER_OF_MONITOR_BUSS_FUNCTIONS];
AXUM_FUNCTION_INFORMATION_STRUCT *DestinationFunctions[NUMBER_OF_DESTINATIONS][NUMBER_OF_DESTINATION_FUNCTIONS];
AXUM_FUNCTION_INFORMATION_STRUCT *GlobalFunctions[NUMBER_OF_GLOBAL_FUNCTIONS];

float Position2dB[1024];
int dB2Position[1500];
unsigned int PulseTime;

bool ExitApplication = 0;

unsigned char TraceValue;           //To set the MambaNet trace (0x01=packets, 0x02=address table)
bool dump_packages;                 //To debug the incoming data

int NetworkFileDescriptor;          //identifies the used network device
int DatabaseFileDescriptor;
extern PGconn *sqldb;

unsigned char EthernetReceiveBuffer[4096];
int cntEthernetReceiveBufferTop;
int cntEthernetReceiveBufferBottom;
unsigned char EthernetMambaNetDecodeBuffer[128];
unsigned char cntEthernetMambaNetDecodeBuffer;

char TTYDevice[256];                    //Buffer to store the serial device name
char NetworkInterface[256];     //Buffer to store the networ device name

unsigned char LocalMACAddress[6];  //Buffer to store local MAC Address

volatile unsigned long *PtrDSP_HPIC[4] = {NULL,NULL,NULL,NULL};
volatile unsigned long *PtrDSP_HPIA[4] = {NULL,NULL,NULL,NULL};
volatile unsigned long *PtrDSP_HPIDAutoIncrement[4] = {NULL,NULL,NULL,NULL};
volatile unsigned long *PtrDSP_HPID[4] = {NULL,NULL,NULL,NULL};

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

static int callback(void *NotUsed, int argc, char **argv, char **azColName)
{
  return 0;
  //make sure the variable are used
  NotUsed = NULL;
  argc = 0;
  argv = NULL;
  azColName = NULL;
}

#define ADDRESS_TABLE_SIZE 65536
ONLINE_NODE_INFORMATION_STRUCT OnlineNodeInformation[ADDRESS_TABLE_SIZE];

sqlite3 *axum_engine_db;
sqlite3 *node_templates_db;

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
  while((c = getopt(argc, argv, "e:d:l:g:i:")) != -1) {
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
      default:
        fprintf(stderr, "Usage: %s [-e dev] [-u path] [-g path] [-d str] [-l path] [-i id]\n", argv[0]);
        fprintf(stderr, "  -e dev   Ethernet device for MambaNet communication.\n");
        fprintf(stderr, "  -i id    UniqueIDPerProduct for the MambaNet node.\n");
        fprintf(stderr, "  -g path  Hardware parent or path to gateway socket.\n");
        fprintf(stderr, "  -l path  Path to log file.\n");
        fprintf(stderr, "  -d str   PostgreSQL database connection options.\n");
        exit(1);
    }
  }

  daemonize();
  log_open();
  //hwparent(&this_node);
  sql_open(dbstr, 0, NULL);

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
  daemonize_finish();
  log_write("-----------------------");
  log_write("Axum Engine Initialized");
}

static int SlotNumberObjectCallback(void *NotUsed, int argc, char **argv, char **azColName)
{
  if ((argc == 1) && (CallbackNodeIndex != -1) && (argv != NULL))
  {
    OnlineNodeInformation[CallbackNodeIndex].SlotNumberObjectNr    = atoi(argv[0]);
  }
  return 0;
  NotUsed = NULL;
  azColName = NULL;
}

static int InputChannelCountObjectCallback(void *NotUsed, int argc, char **argv, char **azColName)
{
  if ((argc == 1) && (CallbackNodeIndex != -1) && (argv != NULL))
  {
    OnlineNodeInformation[CallbackNodeIndex].InputChannelCountObjectNr    = atoi(argv[0]);
  }
  return 0;
  NotUsed = NULL;
  azColName = NULL;
}

static int OutputChannelCountObjectCallback(void *NotUsed, int argc, char **argv, char **azColName)
{
  if ((argc == 1) && (CallbackNodeIndex != -1) && (argv != NULL))
  {
    OnlineNodeInformation[CallbackNodeIndex].OutputChannelCountObjectNr    = atoi(argv[0]);
  }
  return 0;
  NotUsed = NULL;
  azColName = NULL;
}

static int NumberOfCustomObjectsCallback(void *NotUsed, int argc, char **argv, char **azColName)
{
  printf("[NumberOfCustomObjectsCallback] argc:%d, CallbackNodeIndex:%d\n", argc, CallbackNodeIndex);

  if ((argc == 1) && (CallbackNodeIndex != -1) && (argv != NULL))
  {
    if (argv[0] != NULL)
    {
      OnlineNodeInformation[CallbackNodeIndex].NumberOfCustomObjects  = atoi(argv[0]);
      if (OnlineNodeInformation[CallbackNodeIndex].NumberOfCustomObjects>0)
      {
        OnlineNodeInformation[CallbackNodeIndex].SensorReceiveFunction = new SENSOR_RECEIVE_FUNCTION_STRUCT[OnlineNodeInformation[CallbackNodeIndex].NumberOfCustomObjects];
        OnlineNodeInformation[CallbackNodeIndex].ObjectInformation = new OBJECT_INFORMATION_STRUCT[OnlineNodeInformation[CallbackNodeIndex].NumberOfCustomObjects];
        for (int cntObject=0; cntObject<OnlineNodeInformation[CallbackNodeIndex].NumberOfCustomObjects; cntObject++)
        {
          OnlineNodeInformation[CallbackNodeIndex].SensorReceiveFunction[cntObject].FunctionNr = -1;
          OnlineNodeInformation[CallbackNodeIndex].SensorReceiveFunction[cntObject].LastChangedTime = 0;
          OnlineNodeInformation[CallbackNodeIndex].SensorReceiveFunction[cntObject].PreviousLastChangedTime = 0;
          OnlineNodeInformation[CallbackNodeIndex].SensorReceiveFunction[cntObject].TimeBeforeMomentary = DEFAULT_TIME_BEFORE_MOMENTARY;
          OnlineNodeInformation[CallbackNodeIndex].ObjectInformation[cntObject].AxumFunctionNr = -1;
        }
      }

      printf("Number of custom objects: %d\n", OnlineNodeInformation[CallbackNodeIndex].NumberOfCustomObjects);
    }
  }
  return 0;
  NotUsed = NULL;
  azColName = NULL;
}

static int ObjectInformationCallback(void *NotUsed, int argc, char **argv, char **azColName)
{
  int i;
  int           ObjectNr = 0;
  char          Description[32]="";
  unsigned char   Services;
  unsigned char   SensorDataType;
  unsigned char   SensorDataSize;
  float       SensorDataMinimal;
  float       SensorDataMaximal;
  unsigned char   ActuatorDataType;
  unsigned char   ActuatorDataSize;
  float       ActuatorDataMinimal;
  float       ActuatorDataMaximal;
  float       ActuatorDataDefault;

  for (i=0; i<argc; i++)
  {
    if (argv[i])
    {
      if (strcmp(azColName[i],"Object Number") == 0)
      {
        ObjectNr = atoi(argv[i]);
      }
      else if (strcmp(azColName[i],"Object Description") == 0)
      {
        strncpy(Description, argv[i], 32);
      }
      else if (strcmp(azColName[i],"Object Services") == 0)
      {
        Services = atoi(argv[i]);
      }
      else if (strcmp(azColName[i],"Sensor Data Type") == 0)
      {
        SensorDataType = atoi(argv[i]);
      }
      else if (strcmp(azColName[i],"Sensor Data Size") == 0)
      {
        SensorDataSize = atoi(argv[i]);
      }
      else if (strcmp(azColName[i],"Sensor Data Minimal") == 0)
      {
        SensorDataMinimal = atof(argv[i]);
      }
      else if (strcmp(azColName[i],"Sensor Data Maximal") == 0)
      {
        SensorDataMaximal = atof(argv[i]);
      }
      else if (strcmp(azColName[i],"Actuator Data Type") == 0)
      {
        ActuatorDataType = atoi(argv[i]);
      }
      else if (strcmp(azColName[i],"Actuator Data Size") == 0)
      {
        ActuatorDataSize = atoi(argv[i]);
      }
      else if (strcmp(azColName[i],"Actuator Data Minimal") == 0)
      {
        ActuatorDataMinimal = atof(argv[i]);
      }
      else if (strcmp(azColName[i],"Actuator Data Maximal") == 0)
      {
        ActuatorDataMaximal = atof(argv[i]);
      }
      else if (strcmp(azColName[i],"Actuator Data Default") == 0)
      {
        ActuatorDataDefault = atof(argv[i]);
      }
    }
    else
    {  //no data in database
      if (strcmp(azColName[i],"Object Number") == 0)
      {
        ObjectNr = 0;
      }
      else if (strcmp(azColName[i],"Object Description") == 0)
      {
        Description[0] = 0;
      }
      else if (strcmp(azColName[i],"Object Services") == 0)
      {
        Services = 0;
      }
      else if (strcmp(azColName[i],"Sensor Data Type") == 0)
      {
        SensorDataType = NO_DATA_DATATYPE;
      }
      else if (strcmp(azColName[i],"Sensor Data Size") == 0)
      {
        SensorDataSize = 0;
      }
      else if (strcmp(azColName[i],"Sensor Data Minimal") == 0)
      {
        SensorDataMinimal = 0;
      }
      else if (strcmp(azColName[i],"Sensor Data Maximal") == 0)
      {
        SensorDataMaximal = 0;
      }
      else if (strcmp(azColName[i],"Actuator Data Type") == 0)
      {
        ActuatorDataType = NO_DATA_DATATYPE;
      }
      else if (strcmp(azColName[i],"Actuator Data Size") == 0)
      {
        ActuatorDataSize = 0;
      }
      else if (strcmp(azColName[i],"Actuator Data Minimal") == 0)
      {
        ActuatorDataMinimal = 0;
      }
      else if (strcmp(azColName[i],"Actuator Data Maximal") == 0)
      {
        ActuatorDataMaximal = 0;
      }
      else if (strcmp(azColName[i],"Actuator Data Default") == 0)
      {
        ActuatorDataDefault = 0;
      }
    }
  }

  if (OnlineNodeInformation[CallbackNodeIndex].ObjectInformation != NULL)
  {
    if (ObjectNr >= 1024)
    {
      strncpy(OnlineNodeInformation[CallbackNodeIndex].ObjectInformation[ObjectNr-1024].Description, Description, 32);

      OnlineNodeInformation[CallbackNodeIndex].ObjectInformation[ObjectNr-1024].Services = Services;
      OnlineNodeInformation[CallbackNodeIndex].ObjectInformation[ObjectNr-1024].SensorDataType = SensorDataType;
      OnlineNodeInformation[CallbackNodeIndex].ObjectInformation[ObjectNr-1024].SensorDataSize = SensorDataSize;
      OnlineNodeInformation[CallbackNodeIndex].ObjectInformation[ObjectNr-1024].SensorDataMinimal = SensorDataMinimal;
      OnlineNodeInformation[CallbackNodeIndex].ObjectInformation[ObjectNr-1024].SensorDataMaximal = SensorDataMaximal;
      OnlineNodeInformation[CallbackNodeIndex].ObjectInformation[ObjectNr-1024].ActuatorDataType = ActuatorDataType;
      OnlineNodeInformation[CallbackNodeIndex].ObjectInformation[ObjectNr-1024].ActuatorDataSize = ActuatorDataSize;
      OnlineNodeInformation[CallbackNodeIndex].ObjectInformation[ObjectNr-1024].ActuatorDataMinimal = ActuatorDataMinimal;
      OnlineNodeInformation[CallbackNodeIndex].ObjectInformation[ObjectNr-1024].ActuatorDataMaximal = ActuatorDataMaximal;
      OnlineNodeInformation[CallbackNodeIndex].ObjectInformation[ObjectNr-1024].ActuatorDataDefault = ActuatorDataDefault;
    }
  }

  return 0;
  NotUsed = NULL;
}

static int NodeConfigurationCallback(void *IndexOfSender, int argc, char **argv, char **azColName)
{
  int i;
  int ObjectNr = -1;
  int Function = -1;

  for (i=0; i<argc; i++)
  {
    if (argv[i])
    {
      if (strcmp(azColName[i],"Object Number") == 0)
      {
        ObjectNr = atoi(argv[i]);
      }
      if (strcmp(azColName[i],"Function") == 0)
      {
        Function = atoi(argv[i]);
      }
    }
  }
  if ((ObjectNr != -1) && (Function != -1) && ((ObjectNr-1024)<OnlineNodeInformation[*((int *)IndexOfSender)].NumberOfCustomObjects))
  {
    if (OnlineNodeInformation[*((int *)IndexOfSender)].SensorReceiveFunction != NULL)
    {
      OnlineNodeInformation[*((int *)IndexOfSender)].SensorReceiveFunction[ObjectNr-1024].FunctionNr = Function;
      OnlineNodeInformation[*((int *)IndexOfSender)].SensorReceiveFunction[ObjectNr-1024].LastChangedTime = 0;
      OnlineNodeInformation[*((int *)IndexOfSender)].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime = 0;
      OnlineNodeInformation[*((int *)IndexOfSender)].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary = DEFAULT_TIME_BEFORE_MOMENTARY;
    }
    MakeObjectListPerFunction(Function);

    //Set default value;
//    printf("4 - ObjectNr:%d Function:%08lx\n", ObjectNr, Function);

    CheckObjectsToSent(Function, OnlineNodeInformation[*((int *)IndexOfSender)].MambaNetAddress);
    delay_ms(1);
  }
  return 0;
}

static int SourceConfigurationCallback(void *NotUsed, int argc, char **argv, char **azColName)
{
  int i;
  int Number = 0;
  char  Label[33]="";
  AXUM_INPUT_DATA_STRUCT InputData[8] = {{0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}};
  int Phantom = 0;
  int Pad = 0;
  float Gain = 0;
  int Redlight[8] = {0,0,0,0,0,0,0,0};
  int MonitorMute[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

  for (i=0; i<argc; i++)
  {
    if (argv[i])
    {
      if (strcmp(azColName[i], "Number") == 0)
      {
        Number = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Label") == 0)
      {
        strncpy(Label, argv[i], 32);
      }
      else if (strcmp(azColName[i], "Input1MambaNetAddress") == 0)
      {
        InputData[0].MambaNetAddress = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Input1SubChannel") == 0)
      {
        InputData[0].SubChannel = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Input2MambaNetAddress") == 0)
      {
        InputData[1].MambaNetAddress = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Input2SubChannel") == 0)
      {
        InputData[1].SubChannel = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Input3MambaNetAddress") == 0)
      {
        InputData[2].MambaNetAddress = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Input3SubChannel") == 0)
      {
        InputData[2].SubChannel = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Input4MambaNetAddress") == 0)
      {
        InputData[3].MambaNetAddress = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Input4SubChannel") == 0)
      {
        InputData[3].SubChannel = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Input5MambaNetAddress") == 0)
      {
        InputData[4].MambaNetAddress = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Input5SubChannel") == 0)
      {
        InputData[4].SubChannel = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Input6MambaNetAddress") == 0)
      {
        InputData[5].MambaNetAddress = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Input6SubChannel") == 0)
      {
        InputData[5].SubChannel = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Input7MambaNetAddress") == 0)
      {
        InputData[6].MambaNetAddress = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Input7SubChannel") == 0)
      {
        InputData[6].SubChannel = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Input8MambaNetAddress") == 0)
      {
        InputData[7].MambaNetAddress = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Input8SubChannel") == 0)
      {
        InputData[7].SubChannel = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Phantom") == 0)
      {
        Phantom = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Pad") == 0)
      {
        Pad = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Gain") == 0)
      {
        Gain = atof(argv[i]);
      }
      else if (strcmp(azColName[i], "Redlight1") == 0)
      {
        Redlight[0] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Redlight2") == 0)
      {
        Redlight[1] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Redlight3") == 0)
      {
        Redlight[2] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Redlight4") == 0)
      {
        Redlight[3] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Redlight5") == 0)
      {
        Redlight[4] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Redlight6") == 0)
      {
        Redlight[5] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Redlight7") == 0)
      {
        Redlight[6] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Redlight8") == 0)
      {
        Redlight[7] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "MonitorMute1") == 0)
      {
        MonitorMute[0] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "MonitorMute2") == 0)
      {
        MonitorMute[1] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "MonitorMute3") == 0)
      {
        MonitorMute[2] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "MonitorMute4") == 0)
      {
        MonitorMute[3] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "MonitorMute5") == 0)
      {
        MonitorMute[4] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "MonitorMute6") == 0)
      {
        MonitorMute[5] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "MonitorMute7") == 0)
      {
        MonitorMute[6] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "MonitorMute8") == 0)
      {
        MonitorMute[7] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "MonitorMute9") == 0)
      {
        MonitorMute[8] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "MonitorMute10") == 0)
      {
        MonitorMute[9] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "MonitorMute11") == 0)
      {
        MonitorMute[10] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "MonitorMute12") == 0)
      {
        MonitorMute[11] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "MonitorMute13") == 0)
      {
        MonitorMute[12] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "MonitorMute14") == 0)
      {
        MonitorMute[13] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "MonitorMute15") == 0)
      {
        MonitorMute[14] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "MonitorMute16") == 0)
      {
        MonitorMute[15] = atoi(argv[i]);
      }

      //printf(azColName[i]);
      //printf(" - ");
      //printf(argv[i]);
      //printf("\n");
    }
  }

  //printf("Number:%d Label:%s Input1:%d, Input2:%d\n", Number, Label, Input[0], Input[1]);

  if (Number>0)
  {
    strncpy(AxumData.SourceData[Number-1].SourceName, Label, 32);
    for (unsigned char cntInput=0; cntInput<8; cntInput++)
    {
      AxumData.SourceData[Number-1].InputData[cntInput].MambaNetAddress = InputData[cntInput].MambaNetAddress;
      AxumData.SourceData[Number-1].InputData[cntInput].SubChannel = InputData[cntInput].SubChannel;
    }

    for (int cntRedlight=0; cntRedlight<8; cntRedlight++)
    {
      AxumData.SourceData[Number-1].Redlight[cntRedlight] = Redlight[cntRedlight];
    }
    for (int cntMonitorMute=0; cntMonitorMute<16; cntMonitorMute++)
    {
      AxumData.SourceData[Number-1].MonitorMute[cntMonitorMute] = MonitorMute[cntMonitorMute];
    }

    for (int cntModule=0; cntModule<128; cntModule++)
    {
      if (AxumData.ModuleData[cntModule].Source == Number)
      {
        SetAxum_ModuleSource(cntModule);
        SetAxum_ModuleMixMinus(cntModule);

        unsigned int FunctionNrToSent = ((cntModule<<12)&0xFFF000);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
      }
    }
  }

  return 0;
  NotUsed = NULL;
}

static int ModuleConfigurationCallback(void *UpdateType, int argc, char **argv, char **azColName)
{
  //UpdateType: 0=all, 1=input, 2=eq, 3=busses
  int i;
  int Number = 0;
  int SourceA = 0;
  int SourceB = 0;
  int InsertSource = 0;
  int InsertOnOffA = 0;
  int InsertOnOffB = 0;
  float Gain = 0;
  int LowCutFrequency = 120;
  int LowCutOnOffA = 0;
  int LowCutOnOffB = 0;

  struct
  {
    float Range;
    unsigned int DefaultFrequency;
    float DefaultBandwidth;
    float DefaultSlope;
    FilterType DefaultType;
  }  EQBand[6] = {  {18, 12000, 1, 1, HIGHSHELF},
    {18,  4000, 1, 1, PEAKINGEQ},
    {18,   800, 1, 1, LOWSHELF},
    {18,   120, 1, 1, HPF},
    {18, 12000, 1, 1, LPF},
    {18,   120, 1, 1, HPF}
  };
  int EQOnOffA = 0;
  int EQOnOffB = 0;

  int DynamicsAmount = 0;
  int DynamicsOnOffA = 0;
  int DynamicsOnOffB = 0;

  float ModuleLevel = -140;
  int ModuleState = 0;

  struct
  {
    float Level;
    int State;
    int PrePost;
    int Balance;
    int Assignment;
  } Buss[16] = { { 0, 0, 0 , 0, 1},
    { 0, 0, 0 , 0, 1},
    { 0, 0, 0 , 0, 1},
    { 0, 0, 0 , 0, 1},
    { 0, 0, 0 , 0, 1},
    { 0, 0, 0 , 0, 1},
    { 0, 0, 0 , 0, 1},
    { 0, 0, 0 , 0, 1},
    { 0, 0, 0 , 0, 1},
    { 0, 0, 0 , 0, 1},
    { 0, 0, 0 , 0, 1},
    { 0, 0, 0 , 0, 1},
    { 0, 0, 0 , 0, 1},
    { 0, 0, 0 , 0, 1},
    { 0, 0, 0 , 0, 1},
    { 0, 0, 0 , 0, 1}
  };

  for (i=0; i<argc; i++)
  {
    if (argv[i])
    {
      if (strcmp(azColName[i], "Number") == 0)
      {
        Number = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "SourceA") == 0)
      {
        SourceA = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "SourceB") == 0)
      {
        SourceB = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "InsertSource") == 0)
      {
        InsertSource = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "InsertOnOffA") == 0)
      {
        InsertOnOffA = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "InsertOnOffB") == 0)
      {
        InsertOnOffB = atoi(argv[i]);
      }
      else if ((strcmp(azColName[i], "EQBand1Range") == 0) ||
               (strcmp(azColName[i], "EQBand2Range") == 0) ||
               (strcmp(azColName[i], "EQBand3Range") == 0) ||
               (strcmp(azColName[i], "EQBand4Range") == 0) ||
               (strcmp(azColName[i], "EQBand5Range") == 0) ||
               (strcmp(azColName[i], "EQBand6Range") == 0))
      {
        int BandNr = azColName[i][6]-'1';
        EQBand[BandNr].Range = atof(argv[i]);
      }
      else if ((strcmp(azColName[i], "EQBand1DefaultFrequency") == 0) ||
               (strcmp(azColName[i], "EQBand2DefaultFrequency") == 0) ||
               (strcmp(azColName[i], "EQBand3DefaultFrequency") == 0) ||
               (strcmp(azColName[i], "EQBand4DefaultFrequency") == 0) ||
               (strcmp(azColName[i], "EQBand5DefaultFrequency") == 0) ||
               (strcmp(azColName[i], "EQBand6DefaultFrequency") == 0))
      {
        int BandNr = azColName[i][6]-'1';
        EQBand[BandNr].DefaultFrequency = atoi(argv[i]);
      }
      else if ((strcmp(azColName[i], "EQBand1DefaultBandwidth") == 0) ||
               (strcmp(azColName[i], "EQBand2DefaultBandwidth") == 0) ||
               (strcmp(azColName[i], "EQBand3DefaultBandwidth") == 0) ||
               (strcmp(azColName[i], "EQBand4DefaultBandwidth") == 0) ||
               (strcmp(azColName[i], "EQBand5DefaultBandwidth") == 0) ||
               (strcmp(azColName[i], "EQBand6DefaultBandwidth") == 0))
      {
        int BandNr = azColName[i][6]-'1';
        EQBand[BandNr].DefaultBandwidth = atof(argv[i]);
      }
      else if ((strcmp(azColName[i], "EQBand1DefaultSlope") == 0) ||
               (strcmp(azColName[i], "EQBand2DefaultSlope") == 0) ||
               (strcmp(azColName[i], "EQBand3DefaultSlope") == 0) ||
               (strcmp(azColName[i], "EQBand4DefaultSlope") == 0) ||
               (strcmp(azColName[i], "EQBand5DefaultSlope") == 0) ||
               (strcmp(azColName[i], "EQBand6DefaultSlope") == 0))
      {
        int BandNr = azColName[i][6]-'1';
        EQBand[BandNr].DefaultSlope = atof(argv[i]);
      }
      else if ((strcmp(azColName[i], "EQBand1DefaultType") == 0) ||
               (strcmp(azColName[i], "EQBand2DefaultType") == 0) ||
               (strcmp(azColName[i], "EQBand3DefaultType") == 0) ||
               (strcmp(azColName[i], "EQBand4DefaultType") == 0) ||
               (strcmp(azColName[i], "EQBand5DefaultType") == 0) ||
               (strcmp(azColName[i], "EQBand6DefaultType") == 0))
      {
        int BandNr = azColName[i][6]-'1';
        EQBand[BandNr].DefaultType = (FilterType)atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "EQOnOffA") == 0)
      {
        EQOnOffA = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "EQOnOffB") == 0)
      {
        EQOnOffB = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "DynamicsAmount") == 0)
      {
        DynamicsAmount = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "DynamicsOnOffA") == 0)
      {
        DynamicsOnOffA = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "DynamicsOnOffB") == 0)
      {
        DynamicsOnOffB = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "ModuleLevel") == 0)
      {
        ModuleLevel = atof(argv[i]);
      }
      else if (strcmp(azColName[i], "ModuleOnOff") == 0)
      {
        ModuleState = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss1and2Level") == 0)
      {
        Buss[0].Level = atof(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss1and2OnOff") == 0)
      {
        Buss[0].State = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss1and2PrePost") == 0)
      {
        Buss[0].PrePost = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss1and2Balance") == 0)
      {
        Buss[0].Balance = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss1and2Assignment") == 0)
      {
        Buss[0].Assignment = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss3and4Level") == 0)
      {
        Buss[1].Level = atof(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss3and4OnOff") == 0)
      {
        Buss[1].State = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss3and4PrePost") == 0)
      {
        Buss[1].PrePost = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss3and4Balance") == 0)
      {
        Buss[1].Balance = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss3and4Assignment") == 0)
      {
        Buss[1].Assignment = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss5and6Level") == 0)
      {
        Buss[2].Level = atof(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss5and6OnOff") == 0)
      {
        Buss[2].State = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss5and6PrePost") == 0)
      {
        Buss[2].PrePost = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss5and6Balance") == 0)
      {
        Buss[2].Balance = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss5and6Assignment") == 0)
      {
        Buss[2].Assignment = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss7and8Level") == 0)
      {
        Buss[3].Level = atof(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss7and8OnOff") == 0)
      {
        Buss[3].State = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss7and8PrePost") == 0)
      {
        Buss[3].PrePost = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss7and8Balance") == 0)
      {
        Buss[3].Balance = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss7and8Assignment") == 0)
      {
        Buss[3].Assignment = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss9and10Level") == 0)
      {
        Buss[4].Level = atof(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss9and10OnOff") == 0)
      {
        Buss[4].State = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss9and10PrePost") == 0)
      {
        Buss[4].PrePost = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss9and10Balance") == 0)
      {
        Buss[4].Balance = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss9and10Assignment") == 0)
      {
        Buss[4].Assignment = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss11and12Level") == 0)
      {
        Buss[5].Level = atof(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss11and12OnOff") == 0)
      {
        Buss[5].State = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss11and12PrePost") == 0)
      {
        Buss[5].PrePost = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss11and12Balance") == 0)
      {
        Buss[5].Balance = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss11and12Assignment") == 0)
      {
        Buss[5].Assignment = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss13and14Level") == 0)
      {
        Buss[6].Level = atof(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss13and14OnOff") == 0)
      {
        Buss[6].State = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss13and14PrePost") == 0)
      {
        Buss[6].PrePost = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss13and14Balance") == 0)
      {
        Buss[6].Balance = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss13and14Assignment") == 0)
      {
        Buss[6].Assignment = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss15and16Level") == 0)
      {
        Buss[7].Level = atof(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss15and16OnOff") == 0)
      {
        Buss[7].State = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss15and16PrePost") == 0)
      {
        Buss[7].PrePost = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss15and16Balance") == 0)
      {
        Buss[7].Balance = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss15and16Assignment") == 0)
      {
        Buss[7].Assignment = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss17and18Level") == 0)
      {
        Buss[8].Level = atof(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss17and18OnOff") == 0)
      {
        Buss[8].State = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss17and18PrePost") == 0)
      {
        Buss[8].PrePost = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss17and18Balance") == 0)
      {
        Buss[8].Balance = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss17and18Assignment") == 0)
      {
        Buss[8].Assignment = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss19and20Level") == 0)
      {
        Buss[9].Level = atof(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss19and20OnOff") == 0)
      {
        Buss[9].State = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss19and20PrePost") == 0)
      {
        Buss[9].PrePost = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss19and20Balance") == 0)
      {
        Buss[9].Balance = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss19and20Assignment") == 0)
      {
        Buss[9].Assignment = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss21and22Level") == 0)
      {
        Buss[10].Level = atof(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss21and22OnOff") == 0)
      {
        Buss[10].State = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss21and22PrePost") == 0)
      {
        Buss[10].PrePost = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss21and22Balance") == 0)
      {
        Buss[10].Balance = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss21and22Assignment") == 0)
      {
        Buss[10].Assignment = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss23and24Level") == 0)
      {
        Buss[11].Level = atof(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss23and24OnOff") == 0)
      {
        Buss[11].State = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss23and24PrePost") == 0)
      {
        Buss[11].PrePost = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss23and24Balance") == 0)
      {
        Buss[11].Balance = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss23and24Assignment") == 0)
      {
        Buss[11].Assignment = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss25and26Level") == 0)
      {
        Buss[12].Level = atof(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss25and26OnOff") == 0)
      {
        Buss[12].State = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss25and26PrePost") == 0)
      {
        Buss[12].PrePost = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss25and26Balance") == 0)
      {
        Buss[12].Balance = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss25and26Assignment") == 0)
      {
        Buss[12].Assignment = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss27and28Level") == 0)
      {
        Buss[13].Level = atof(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss27and28OnOff") == 0)
      {
        Buss[13].State = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss27and28PrePost") == 0)
      {
        Buss[13].PrePost = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss27and28Balance") == 0)
      {
        Buss[13].Balance = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss27and28Assignment") == 0)
      {
        Buss[13].Assignment = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss29and30Level") == 0)
      {
        Buss[14].Level = atof(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss29and30OnOff") == 0)
      {
        Buss[14].State = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss29and30PrePost") == 0)
      {
        Buss[14].PrePost = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss29and30Balance") == 0)
      {
        Buss[14].Balance = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss29and30Assignment") == 0)
      {
        Buss[14].Assignment = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss31and32Level") == 0)
      {
        Buss[15].Level = atof(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss31and32OnOff") == 0)
      {
        Buss[15].State = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss31and32PrePost") == 0)
      {
        Buss[15].PrePost = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss31and32Balance") == 0)
      {
        Buss[15].Balance = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss31and32Assignment") == 0)
      {
        Buss[15].Assignment = atoi(argv[i]);
      }

      //printf(azColName[i]);
      //printf(" - ");
      //printf(argv[i]);
      //printf("\n");
    }
  }

  //printf("UpdateType: %d", *((int *)UpdateType));
  //printf("Number:%d Label:%s Input1:%d, Input2:%d\n", Number, Label, Input[0], Input[1]);

  if (Number>0)
  {
    int ModuleNr = Number-1;

    if ((*((int *)UpdateType) == 0) || (*((int *)UpdateType) == 1))
    { //All or input
      AxumData.ModuleData[ModuleNr].SourceA = SourceA;
      AxumData.ModuleData[ModuleNr].SourceB = SourceB;
      AxumData.ModuleData[ModuleNr].Gain = Gain;
      AxumData.ModuleData[ModuleNr].InsertSource = InsertSource;
      AxumData.ModuleData[ModuleNr].InsertOnOffA = InsertOnOffA;
      AxumData.ModuleData[ModuleNr].InsertOnOffB = InsertOnOffB;
      AxumData.ModuleData[ModuleNr].Filter.Frequency = LowCutFrequency;
      AxumData.ModuleData[ModuleNr].FilterOnOffA = LowCutOnOffA;
      AxumData.ModuleData[ModuleNr].FilterOnOffB = LowCutOnOffB;

      AxumData.ModuleData[ModuleNr].EQOnOffA = EQOnOffA;
      AxumData.ModuleData[ModuleNr].EQOnOffB = EQOnOffB;

      AxumData.ModuleData[ModuleNr].Dynamics = DynamicsAmount;
      AxumData.ModuleData[ModuleNr].DynamicsOnOffA = DynamicsOnOffA;
      AxumData.ModuleData[ModuleNr].DynamicsOnOffB = DynamicsOnOffB;

      float OldLevel = AxumData.ModuleData[ModuleNr].FaderLevel;
      int OldOn = AxumData.ModuleData[ModuleNr].On;
      AxumData.ModuleData[ModuleNr].FaderLevel = ModuleLevel;
      AxumData.ModuleData[ModuleNr].On = ModuleState;

      if (AxumApplicationAndDSPInitialized)
      {
        SetNewSource(ModuleNr, SourceA, 1, 1);

        SetAxum_ModuleInsertSource(ModuleNr);

        //Set fader level and On;
        float NewLevel = AxumData.ModuleData[ModuleNr].FaderLevel;
        int NewOn = AxumData.ModuleData[ModuleNr].On;

        SetAxum_BussLevels(ModuleNr);

        unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_MODULE_LEVEL);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_MODULE_ON);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_MODULE_OFF);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_MODULE_ON_OFF);

        if (AxumData.ModuleData[ModuleNr].Source > 0)
        {
          FunctionNrToSent = 0x05000000 | ((AxumData.ModuleData[ModuleNr].Source-1)<<12);
          CheckObjectsToSent(FunctionNrToSent | SOURCE_FUNCTION_MODULE_FADER_ON);
          CheckObjectsToSent(FunctionNrToSent | SOURCE_FUNCTION_MODULE_FADER_OFF);
          CheckObjectsToSent(FunctionNrToSent | SOURCE_FUNCTION_MODULE_FADER_ON_OFF);
          CheckObjectsToSent(FunctionNrToSent | SOURCE_FUNCTION_MODULE_FADER_AND_ON_ACTIVE);
          CheckObjectsToSent(FunctionNrToSent | SOURCE_FUNCTION_MODULE_FADER_AND_ON_INACTIVE);
        }

        FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

        if (((OldLevel<=-80) && (NewLevel>-80)) ||
            ((OldLevel>-80) && (NewLevel<=-80)) ||
            (OldOn != NewOn))
        { //fader on changed
          DoAxum_ModuleStatusChanged(ModuleNr);
        }
      }
      else
      {
        AxumData.ModuleData[ModuleNr].Source = SourceA;
      }
    }

    if ((*((int *)UpdateType) == 0) || (*((int *)UpdateType) == 2))
    { //All or eq
      for (int cntBand=0; cntBand<6; cntBand++)
      {
        AxumData.ModuleData[ModuleNr].EQBand[cntBand].Range = EQBand[cntBand].Range;
        AxumData.ModuleData[ModuleNr].EQBand[cntBand].DefaultFrequency = EQBand[cntBand].DefaultFrequency;
        AxumData.ModuleData[ModuleNr].EQBand[cntBand].DefaultBandwidth = EQBand[cntBand].DefaultBandwidth;
        AxumData.ModuleData[ModuleNr].EQBand[cntBand].DefaultSlope = EQBand[cntBand].DefaultSlope;
        AxumData.ModuleData[ModuleNr].EQBand[cntBand].DefaultType = EQBand[cntBand].DefaultType;

        if (AxumData.ModuleData[ModuleNr].EQBand[cntBand].Level>AxumData.ModuleData[ModuleNr].EQBand[cntBand].Range)
        {
          AxumData.ModuleData[ModuleNr].EQBand[cntBand].Level = AxumData.ModuleData[ModuleNr].EQBand[cntBand].Range;
        }
        else if (AxumData.ModuleData[ModuleNr].EQBand[cntBand].Level < -AxumData.ModuleData[ModuleNr].EQBand[cntBand].Range)
        {
          AxumData.ModuleData[ModuleNr].EQBand[cntBand].Level = AxumData.ModuleData[ModuleNr].EQBand[cntBand].Range;
        }
        AxumData.ModuleData[ModuleNr].EQBand[cntBand].Frequency = AxumData.ModuleData[ModuleNr].EQBand[cntBand].DefaultFrequency;
        AxumData.ModuleData[ModuleNr].EQBand[cntBand].Bandwidth = AxumData.ModuleData[ModuleNr].EQBand[cntBand].DefaultBandwidth;
        AxumData.ModuleData[ModuleNr].EQBand[cntBand].Slope = AxumData.ModuleData[ModuleNr].EQBand[cntBand].DefaultSlope;
        AxumData.ModuleData[ModuleNr].EQBand[cntBand].Type = AxumData.ModuleData[ModuleNr].EQBand[cntBand].DefaultType;

        if (AxumApplicationAndDSPInitialized)
        {
          SetAxum_EQ(ModuleNr, cntBand);

          unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
          CheckObjectsToSent(FunctionNrToSent | (MODULE_FUNCTION_EQ_BAND_1_LEVEL+(cntBand*(MODULE_FUNCTION_EQ_BAND_2_LEVEL-MODULE_FUNCTION_EQ_BAND_1_LEVEL))));
          CheckObjectsToSent(FunctionNrToSent | (MODULE_FUNCTION_EQ_BAND_1_FREQUENCY+(cntBand*(MODULE_FUNCTION_EQ_BAND_2_FREQUENCY-MODULE_FUNCTION_EQ_BAND_1_FREQUENCY))));
          CheckObjectsToSent(FunctionNrToSent | (MODULE_FUNCTION_EQ_BAND_1_BANDWIDTH+(cntBand*(MODULE_FUNCTION_EQ_BAND_2_BANDWIDTH-MODULE_FUNCTION_EQ_BAND_1_BANDWIDTH))));
          //CheckObjectsToSent(FunctionNrToSent | (MODULE_FUNCTION_EQ_BAND_1_SLOPE+(cntBand*(MODULE_FUNCTION_EQ_BAND_2_SLOPE-MODULE_FUNCTION_EQ_BAND_1_SLOPE))));
          CheckObjectsToSent(FunctionNrToSent | (MODULE_FUNCTION_EQ_BAND_1_TYPE+(cntBand*(MODULE_FUNCTION_EQ_BAND_2_TYPE-MODULE_FUNCTION_EQ_BAND_1_TYPE))));
        }
      }

      unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
    }

    if ((*((int *)UpdateType) == 0) || (*((int *)UpdateType) == 3))
    { //All or busses
      for (int cntBuss=0; cntBuss<16; cntBuss++)
      {
        AxumData.ModuleData[ModuleNr].Buss[cntBuss].Level = Buss[cntBuss].Level;
        AxumData.ModuleData[ModuleNr].Buss[cntBuss].On = Buss[cntBuss].State;
        AxumData.ModuleData[ModuleNr].Buss[cntBuss].PreModuleLevel = Buss[cntBuss].PrePost;
        AxumData.ModuleData[ModuleNr].Buss[cntBuss].Balance = Buss[cntBuss].Balance;
        AxumData.ModuleData[ModuleNr].Buss[cntBuss].Active = Buss[cntBuss].Assignment;

        if (AxumApplicationAndDSPInitialized)
        {
          SetAxum_BussLevels(ModuleNr);

          SetBussOnOff(ModuleNr, cntBuss, 1);//use interlock

          unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_BUSS_1_2_LEVEL);
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_BUSS_1_2_PRE);
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_BUSS_1_2_BALANCE);

          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
        }
      }
    }
  }

  return 0;
}

static int BussConfigurationCallback(void *NotUsed, int argc, char **argv, char **azColName)
{
  int i;
  int Number = 0;
  char  Label[33]="";
  int PreModuleOn = 0;
  int PreModuleLevel = 0;
  int PreModuleBalance = 0;
  float Level;
  int OnOff;
  int Interlock = 0;
  int GlobalBussReset = 0;

  for (i=0; i<argc; i++)
  {
    if (argv[i])
    {
      if (strcmp(azColName[i], "Number") == 0)
      {
        Number = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Label") == 0)
      {
        strncpy(Label, argv[i], 32);
      }
      else if (strcmp(azColName[i], "PreModuleOn") == 0)
      {
        PreModuleOn = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "PreModuleLevel") == 0)
      {
        PreModuleLevel = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "PreModuleBalance") == 0)
      {
        PreModuleBalance = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Level") == 0)
      {
        Level = atof(argv[i]);
      }
      else if (strcmp(azColName[i], "OnOff") == 0)
      {
        OnOff = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Interlock") == 0)
      {
        Interlock = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "GlobalBussReset") == 0)
      {
        GlobalBussReset = atoi(argv[i]);
      }
    }
  }

  //printf("Number:%d Label:%s Input1:%d, Input2:%d\n", Number, Label, Input[0], Input[1]);

  if (Number>0)
  {
    int BussNr = Number-1;
    strncpy(AxumData.BussMasterData[BussNr].Label, Label, 32);
    AxumData.BussMasterData[BussNr].PreModuleOn = PreModuleOn;
    AxumData.BussMasterData[BussNr].PreModuleLevel = PreModuleLevel;
    AxumData.BussMasterData[BussNr].PreModuleBalance = PreModuleBalance;
    AxumData.BussMasterData[BussNr].Level = Level;
    AxumData.BussMasterData[BussNr].On = OnOff;
    AxumData.BussMasterData[BussNr].Interlock = Interlock;
    AxumData.BussMasterData[BussNr].GlobalBussReset = GlobalBussReset;

    unsigned int FunctionNrToSent = 0x01000000 | ((BussNr<<12)&0xFFF000);
    CheckObjectsToSent(FunctionNrToSent | BUSS_FUNCTION_MASTER_PRE);
    CheckObjectsToSent(FunctionNrToSent | BUSS_FUNCTION_MASTER_LEVEL);
    CheckObjectsToSent(FunctionNrToSent | BUSS_FUNCTION_MASTER_ON_OFF);
    CheckObjectsToSent(FunctionNrToSent | BUSS_FUNCTION_LABEL);

    for (int cntModule=0; cntModule<128; cntModule++)
    {
      if (AxumApplicationAndDSPInitialized)
      {
        SetAxum_BussLevels(cntModule);
      }
    }

    for (int cntDestination=0; cntDestination<1280; cntDestination++)
    {
      if (AxumData.DestinationData[cntDestination].Source == Number)
      {
        FunctionNrToSent = 0x06000000 | (cntDestination<<12);
        CheckObjectsToSent(FunctionNrToSent | DESTINATION_FUNCTION_SOURCE);
      }
    }
  }

  return 0;
  NotUsed = NULL;
}

static int MonitorBussConfigurationCallback(void *NotUsed, int argc, char **argv, char **azColName)
{
  int i;
  int Number = 0;
  char  Label[33]="";
  int Interlock = 0;
  int DefaultSelection = 0;
  int AutoSwitchingBuss[16];
  float SwitchingDimLevel = -140;

  for (i=0; i<argc; i++)
  {
    if (argv[i])
    {
      if (strcmp(azColName[i], "Number") == 0)
      {
        Number = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Label") == 0)
      {
        strncpy(Label, argv[i], 32);
      }
      else if (strcmp(azColName[i], "Interlock") == 0)
      {
        Interlock = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "DefaultSelection") == 0)
      {
        DefaultSelection = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss1and2") == 0)
      {
        AutoSwitchingBuss[0] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss3and4") == 0)
      {
        AutoSwitchingBuss[1] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss5and6") == 0)
      {
        AutoSwitchingBuss[2] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss7and8") == 0)
      {
        AutoSwitchingBuss[3] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss9and10") == 0)
      {
        AutoSwitchingBuss[4] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss11and12") == 0)
      {
        AutoSwitchingBuss[5] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss13and14") == 0)
      {
        AutoSwitchingBuss[6] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss15and16") == 0)
      {
        AutoSwitchingBuss[7] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss17and18") == 0)
      {
        AutoSwitchingBuss[8] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss19and20") == 0)
      {
        AutoSwitchingBuss[9] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss21and22") == 0)
      {
        AutoSwitchingBuss[10] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss23and24") == 0)
      {
        AutoSwitchingBuss[11] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss25and26") == 0)
      {
        AutoSwitchingBuss[12] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss27and28") == 0)
      {
        AutoSwitchingBuss[13] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss29and30") == 0)
      {
        AutoSwitchingBuss[14] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Buss31and32") == 0)
      {
        AutoSwitchingBuss[15] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "SwitchingDimLevel") == 0)
      {
        SwitchingDimLevel = atof(argv[i]);
      }
    }
  }

  //printf("Number:%d Label:%s Input1:%d, Input2:%d\n", Number, Label, Input[0], Input[1]);

  if (Number>0)
  {
    int MonitorBussNr = Number-1;
    strncpy(AxumData.Monitor[MonitorBussNr].Label, Label, 32);
    AxumData.Monitor[MonitorBussNr].Interlock = Interlock;
    AxumData.Monitor[MonitorBussNr].DefaultSelection = DefaultSelection;
    if (AxumData.Monitor[MonitorBussNr].DefaultSelection<16)
    {
      int MixingBussNr = AxumData.Monitor[MonitorBussNr].DefaultSelection;
      AxumData.Monitor[MonitorBussNr].Buss[MixingBussNr] = 1;
    }
    else if (AxumData.Monitor[MonitorBussNr].DefaultSelection<24)
    {
      int ExtNr = AxumData.Monitor[MonitorBussNr].DefaultSelection-16;
      AxumData.Monitor[MonitorBussNr].Buss[ExtNr] = 1;
    }
    AxumData.Monitor[MonitorBussNr].AutoSwitchingBuss[0] = AutoSwitchingBuss[0];
    AxumData.Monitor[MonitorBussNr].AutoSwitchingBuss[1] = AutoSwitchingBuss[1];
    AxumData.Monitor[MonitorBussNr].AutoSwitchingBuss[2] = AutoSwitchingBuss[2];
    AxumData.Monitor[MonitorBussNr].AutoSwitchingBuss[3] = AutoSwitchingBuss[3];
    AxumData.Monitor[MonitorBussNr].AutoSwitchingBuss[4] = AutoSwitchingBuss[4];
    AxumData.Monitor[MonitorBussNr].AutoSwitchingBuss[5] = AutoSwitchingBuss[5];
    AxumData.Monitor[MonitorBussNr].AutoSwitchingBuss[6] = AutoSwitchingBuss[6];
    AxumData.Monitor[MonitorBussNr].AutoSwitchingBuss[7] = AutoSwitchingBuss[7];
    AxumData.Monitor[MonitorBussNr].AutoSwitchingBuss[8] = AutoSwitchingBuss[8];
    AxumData.Monitor[MonitorBussNr].AutoSwitchingBuss[9] = AutoSwitchingBuss[9];
    AxumData.Monitor[MonitorBussNr].AutoSwitchingBuss[10] = AutoSwitchingBuss[10];
    AxumData.Monitor[MonitorBussNr].AutoSwitchingBuss[11] = AutoSwitchingBuss[11];
    AxumData.Monitor[MonitorBussNr].AutoSwitchingBuss[12] = AutoSwitchingBuss[12];
    AxumData.Monitor[MonitorBussNr].AutoSwitchingBuss[13] = AutoSwitchingBuss[13];
    AxumData.Monitor[MonitorBussNr].AutoSwitchingBuss[14] = AutoSwitchingBuss[14];
    AxumData.Monitor[MonitorBussNr].AutoSwitchingBuss[15] = AutoSwitchingBuss[15];
    AxumData.Monitor[MonitorBussNr].SwitchingDimLevel = SwitchingDimLevel;

    if (AxumApplicationAndDSPInitialized)
    {
      SetAxum_MonitorBuss(MonitorBussNr);

      unsigned int FunctionNrToSent = 0x02000000 | ((MonitorBussNr<<12)&0xFFF000);
      if (AxumData.Monitor[MonitorBussNr].DefaultSelection<16)
      {
        int MixingBussNr = AxumData.Monitor[MonitorBussNr].DefaultSelection;
        CheckObjectsToSent(FunctionNrToSent | (MONITOR_BUSS_FUNCTION_BUSS_1_2_ON_OFF+MixingBussNr));
      }
      else if (AxumData.Monitor[MonitorBussNr].DefaultSelection<24)
      {
        int ExtNr = AxumData.Monitor[MonitorBussNr].DefaultSelection-16;
        CheckObjectsToSent(FunctionNrToSent | (MONITOR_BUSS_FUNCTION_EXT_1_ON_OFF+ExtNr));
      }
      CheckObjectsToSent(FunctionNrToSent | MONITOR_BUSS_FUNCTION_LABEL);
    }
  }

  return 0;
  NotUsed = NULL;
}

static int ExternSourceConfigurationCallback(void *NotUsed, int argc, char **argv, char **azColName)
{
  int i;
  int Number = 0;
  int Ext[8] = {0,0,0,0,0,0,0,0};

  for (i=0; i<argc; i++)
  {
    if (argv[i])
    {
      if (strcmp(azColName[i], "Number") == 0)
      {
        Number = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Ext1") == 0)
      {
        Ext[0] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Ext2") == 0)
      {
        Ext[1] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Ext3") == 0)
      {
        Ext[2] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Ext4") == 0)
      {
        Ext[3] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Ext5") == 0)
      {
        Ext[4] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Ext6") == 0)
      {
        Ext[5] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Ext7") == 0)
      {
        Ext[6] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Ext8") == 0)
      {
        Ext[7] = atoi(argv[i]);
      }
    }
  }

  //printf("Number:%d Label:%s Input1:%d, Input2:%d\n", Number, Label, Input[0], Input[1]);

  if (Number>0)
  {
    int MonitorBussPerFourNr = Number-1;
    AxumData.ExternSource[MonitorBussPerFourNr].Ext[0] = Ext[0];
    AxumData.ExternSource[MonitorBussPerFourNr].Ext[1] = Ext[1];
    AxumData.ExternSource[MonitorBussPerFourNr].Ext[2] = Ext[2];
    AxumData.ExternSource[MonitorBussPerFourNr].Ext[3] = Ext[3];
    AxumData.ExternSource[MonitorBussPerFourNr].Ext[4] = Ext[4];
    AxumData.ExternSource[MonitorBussPerFourNr].Ext[5] = Ext[5];
    AxumData.ExternSource[MonitorBussPerFourNr].Ext[6] = Ext[6];
    AxumData.ExternSource[MonitorBussPerFourNr].Ext[7] = Ext[7];

    if (AxumApplicationAndDSPInitialized)
    {
      SetAxum_ExternSources(MonitorBussPerFourNr);
    }
  }

  return 0;
  NotUsed = NULL;
}

static int TalkbackConfigurationCallback(void *NotUsed, int argc, char **argv, char **azColName)
{
  int i;
  int Talkback[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  int cntTalkback;

  for (i=0; i<argc; i++)
  {
    if (argv[i])
    {
      if (strcmp(azColName[i], "Talkback1") == 0)
      {
        Talkback[0] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Talkback2") == 0)
      {
        Talkback[1] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Talkback3") == 0)
      {
        Talkback[2] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Talkback4") == 0)
      {
        Talkback[3] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Talkback5") == 0)
      {
        Talkback[4] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Talkback6") == 0)
      {
        Talkback[5] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Talkback7") == 0)
      {
        Talkback[6] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Talkback8") == 0)
      {
        Talkback[7] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Talkback9") == 0)
      {
        Talkback[8] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Talkback10") == 0)
      {
        Talkback[9] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Talkback11") == 0)
      {
        Talkback[10] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Talkback12") == 0)
      {
        Talkback[11] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Talkback13") == 0)
      {
        Talkback[12] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Talkback14") == 0)
      {
        Talkback[13] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Talkback15") == 0)
      {
        Talkback[14] = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Talkback16") == 0)
      {
        Talkback[15] = atoi(argv[i]);
      }
    }
  }

  for (cntTalkback=0; cntTalkback<16; cntTalkback++)
  {
    if (AxumData.Talkback[cntTalkback].Source != Talkback[cntTalkback])
    {
      AxumData.Talkback[cntTalkback].Source = Talkback[cntTalkback];
      SetAxum_TalkbackSource(cntTalkback);
    }
  }

  //set routing?

  return 0;
  NotUsed = NULL;
}

static int GlobalConfigurationCallback(void *NotUsed, int argc, char **argv, char **azColName)
{
  int i;
  unsigned int Samplerate = 48000;
  unsigned char ExternClock = 0;
  float Headroom = -20;
  float LevelReserve = 0;

  for (i=0; i<argc; i++)
  {
    if (argv[i])
    {
      if (strcmp(azColName[i], "Samplerate") == 0)
      {
        Samplerate = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "ExternClock") == 0)
      {
        ExternClock = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Headroom") == 0)
      {
        Headroom = atof(argv[i]);
      }
      else if (strcmp(azColName[i], "LevelReserve") == 0)
      {
        LevelReserve = atof(argv[i]);
      }
    }
  }

  AxumData.Samplerate = Samplerate;
  AxumData.ExternClock = ExternClock;
  AxumData.Headroom = Headroom;
  AxumData.LevelReserve = LevelReserve;

  if (AxumApplicationAndDSPInitialized)
  {
    SetDSPCard_Interpolation();
  }
  SetBackplane_Clock();

  int cntModule;
  for (cntModule=0; cntModule<128; cntModule++)
  {
    unsigned int FunctionNrToSent = ((cntModule<<12)&0xFFF000);

    for (int cntBand=0; cntBand<6; cntBand++)
    {
      if (AxumApplicationAndDSPInitialized)
      {
        SetAxum_EQ(cntModule, cntBand);
      }
    }

    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_MODULE_LEVEL);
    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_BUSS_1_2_LEVEL);
    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_BUSS_3_4_LEVEL);
    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_BUSS_5_6_LEVEL);
    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_BUSS_7_8_LEVEL);
    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_BUSS_9_10_LEVEL);
    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_BUSS_11_12_LEVEL);
    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_BUSS_13_14_LEVEL);
    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_BUSS_15_16_LEVEL);
    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_BUSS_17_18_LEVEL);
    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_BUSS_19_20_LEVEL);
    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_BUSS_21_22_LEVEL);
    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_BUSS_23_24_LEVEL);
    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_BUSS_25_26_LEVEL);
    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_BUSS_27_28_LEVEL);
    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_BUSS_29_30_LEVEL);
    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_BUSS_31_32_LEVEL);

    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
  }

  int cntBuss;
  for (cntBuss=0; cntModule<16; cntBuss++)
  {
    unsigned int FunctionNrToSent = 0x01000000 | ((cntBuss<<12)&0xFFF000);
    CheckObjectsToSent(FunctionNrToSent | BUSS_FUNCTION_MASTER_LEVEL);
  }

  for (cntBuss=0; cntModule<16; cntBuss++)
  {
    unsigned int FunctionNrToSent = 0x02000000 | ((cntBuss<<12)&0xFFF000);
    CheckObjectsToSent(FunctionNrToSent | MONITOR_BUSS_FUNCTION_PHONES_LEVEL);
    CheckObjectsToSent(FunctionNrToSent | MONITOR_BUSS_FUNCTION_SPEAKER_LEVEL);
  }

  int cntDestination;
  for (cntDestination=0; cntDestination<1280; cntDestination++)
  {
    unsigned int FunctionNrToSent = 0x06000000 | ((cntDestination<<12)&0xFFF000);
    CheckObjectsToSent(FunctionNrToSent | DESTINATION_FUNCTION_LEVEL);
  }

  unsigned int FunctionNrToSent = 0x04000000;
  CheckObjectsToSent(FunctionNrToSent | GLOBAL_FUNCTION_MASTER_CONTROL_1);
  CheckObjectsToSent(FunctionNrToSent | GLOBAL_FUNCTION_MASTER_CONTROL_2);
  CheckObjectsToSent(FunctionNrToSent | GLOBAL_FUNCTION_MASTER_CONTROL_3);
  CheckObjectsToSent(FunctionNrToSent | GLOBAL_FUNCTION_MASTER_CONTROL_4);

  return 0;
  NotUsed = NULL;
}

static int DestinationConfigurationCallback(void *NotUsed, int argc, char **argv, char **azColName)
{
  int i;
  int Number = 0;
  char  Label[33]="";
  AXUM_OUTPUT_DATA_STRUCT OutputData[8] = {{0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}};
  float Level;
  int Source = 0;
  int MixMinusSource = 0;

  for (i=0; i<argc; i++)
  {
    if (argv[i])
    {
      if (strcmp(azColName[i], "Number") == 0)
      {
        Number = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Label") == 0)
      {
        strncpy(Label, argv[i], 32);
      }
      else if (strcmp(azColName[i], "Output1MambaNetAddress") == 0)
      {
        OutputData[0].MambaNetAddress = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Output1SubChannel") == 0)
      {
        OutputData[0].SubChannel = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Output2MambaNetAddress") == 0)
      {
        OutputData[1].MambaNetAddress = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Output2SubChannel") == 0)
      {
        OutputData[1].SubChannel = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Output3MambaNetAddress") == 0)
      {
        OutputData[2].MambaNetAddress = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Output3SubChannel") == 0)
      {
        OutputData[2].SubChannel = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Output4MambaNetAddress") == 0)
      {
        OutputData[3].MambaNetAddress = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Output4SubChannel") == 0)
      {
        OutputData[3].SubChannel = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Output5MambaNetAddress") == 0)
      {
        OutputData[4].MambaNetAddress = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Output5SubChannel") == 0)
      {
        OutputData[4].SubChannel = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Output6MambaNetAddress") == 0)
      {
        OutputData[5].MambaNetAddress = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Output6SubChannel") == 0)
      {
        OutputData[5].SubChannel = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Output7MambaNetAddress") == 0)
      {
        OutputData[6].MambaNetAddress = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Output7SubChannel") == 0)
      {
        OutputData[6].SubChannel = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Output8MambaNetAddress") == 0)
      {
        OutputData[7].MambaNetAddress = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Output8SubChannel") == 0)
      {
        OutputData[7].SubChannel = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "Level") == 0)
      {
        Level = atof(argv[i]);
      }
      else if (strcmp(azColName[i], "Source") == 0)
      {
        Source = atoi(argv[i]);
      }
      else if (strcmp(azColName[i], "MixMinusSource") == 0)
      {
        MixMinusSource = atoi(argv[i]);
      }

      //printf(azColName[i]);
      //printf(" - ");
      //printf(argv[i]);
      //printf("\n");
    }
  }

  //printf("Number:%d Label:%s Input1:%d, Input2:%d\n", Number, Label, Input[0], Input[1]);

  if (Number>0)
  {
    strncpy(AxumData.DestinationData[Number-1].DestinationName, Label, 32);
    for (unsigned char cntOutput=0; cntOutput<8; cntOutput++)
    {
      AxumData.DestinationData[Number-1].OutputData[cntOutput].MambaNetAddress = OutputData[cntOutput].MambaNetAddress;
      AxumData.DestinationData[Number-1].OutputData[cntOutput].SubChannel = OutputData[cntOutput].SubChannel;
    }
    AxumData.DestinationData[Number-1].Level = Level;
    AxumData.DestinationData[Number-1].Source = Source;
    AxumData.DestinationData[Number-1].MixMinusSource = MixMinusSource;

    if (AxumApplicationAndDSPInitialized)
    {
      SetAxum_DestinationSource(Number-1);
    }

    //Check destinations
    unsigned int DisplayFunctionNr = 0x06000000 | ((Number-1)<<12);
    CheckObjectsToSent(DisplayFunctionNr | DESTINATION_FUNCTION_LABEL);
    CheckObjectsToSent(DisplayFunctionNr | DESTINATION_FUNCTION_LEVEL);
    CheckObjectsToSent(DisplayFunctionNr | DESTINATION_FUNCTION_SOURCE);
  }

  return 0;
  NotUsed = NULL;
}

static int PositionTodBCallback(void *NotUsed, int argc, char **argv, char **azColName)
{
  int i;
  int Position = -1;
  float dB = -140;

  for (i=0; i<argc; i++)
  {
    if (argv[i])
    {
      //printf(azColName[i]);
      //printf(" - ");
      //printf(argv[i]);
      //printf("\n");
      if (strcmp(azColName[i],"position") == 0)
      {
        Position = atoi(argv[i]);
      }
      else if (strcmp(azColName[i],"db") == 0)
      {
        dB = atof(argv[i]);
      }
    }
  }

  if (Position != -1)
  {
    Position2dB[Position] = dB;
  }

  return 0;
  NotUsed = NULL;
}

static int dBToPositionCallback(void *NotUsed, int argc, char **argv, char **azColName)
{
  int i;
  int Position = -1;
  int dB = -1;

  for (i=0; i<argc; i++)
  {
    if (argv[i])
    {
      //printf(azColName[i]);
      //printf(" - ");
      //printf(argv[i]);
      //printf("\n");

      if (strcmp(azColName[i],"db") == 0)
      {
        double Temp = atof(argv[i])*10;
        dB = Temp+1400;
      }
      else if (strcmp(azColName[i],"position") == 0)
      {
        Position = atoi(argv[i]);
      }
    }
  }

  if ((dB >= 0) && (dB<1500))
  {
    if ((Position>=0) && (Position<1024))
    {
      dB2Position[dB] = Position;
    }
  }

  return 0;
  NotUsed = NULL;
}

static int NodeDefaultsCallback(void *IndexOfSender, int argc, char **argv, char **azColName)
{
  int i;
  int ObjectNr = -1;

  for (i=0; i<argc; i++)
  {
    if (argv[i])
    {
      if (strcmp(azColName[i],"Object Number") == 0)
      {
        ObjectNr = atoi(argv[i]);
      }
    }
  }

  if ((ObjectNr != -1) && ((ObjectNr-1024)<OnlineNodeInformation[*((int *)IndexOfSender)].NumberOfCustomObjects))
  {
//    printf("ObjectNr:%d Default data:%s\n", ObjectNr, argv[1]);
    if (OnlineNodeInformation[*((int *)IndexOfSender)].ObjectInformation[ObjectNr-1024].ActuatorDataType != NO_DATA_DATATYPE)
    {
//      printf("ObjectNr:%d Default data:%s DataType:%d, Size %d\n", ObjectNr, argv[1], OnlineNodeInformation[*((int *)IndexOfSender)].ObjectInformation[ObjectNr-1024].ActuatorDataType, OnlineNodeInformation[*((int *)IndexOfSender)].ObjectInformation[ObjectNr-1024].ActuatorDataSize);
//      printf("template default: %g\n", OnlineNodeInformation[*((int *)IndexOfSender)].ObjectInformation[ObjectNr-1024].ActuatorDataDefault);

      if (atof(argv[1]) != OnlineNodeInformation[*((int *)IndexOfSender)].ObjectInformation[ObjectNr-1024].ActuatorDataDefault)
      {
        unsigned char TransmitData[128];
        unsigned char cntTransmitData = 0;
        TransmitData[cntTransmitData++] = (ObjectNr>>8)&0xFF;
        TransmitData[cntTransmitData++] = ObjectNr&0xFF;
        TransmitData[cntTransmitData++] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;//Set actuator
        TransmitData[cntTransmitData++] = OnlineNodeInformation[*((int *)IndexOfSender)].ObjectInformation[ObjectNr-1024].ActuatorDataType;
        switch (OnlineNodeInformation[*((int *)IndexOfSender)].ObjectInformation[ObjectNr-1024].ActuatorDataType)
        {
        case UNSIGNED_INTEGER_DATATYPE:
        case STATE_DATATYPE:
        {
          unsigned long DataValue = atoi(argv[1]);
          int DataSize = OnlineNodeInformation[*((int *)IndexOfSender)].ObjectInformation[ObjectNr-1024].ActuatorDataSize;
          TransmitData[cntTransmitData++] = DataSize;
          for (int cntByte=0; cntByte<DataSize; cntByte++)
          {
            TransmitData[cntTransmitData++] = (DataValue>>(((DataSize-1)-cntByte)*8))&0xFF;
          }
        }
        break;
        case SIGNED_INTEGER_DATATYPE:
        {
          long DataValue = atoi(argv[1]);
          int DataSize = OnlineNodeInformation[*((int *)IndexOfSender)].ObjectInformation[ObjectNr-1024].ActuatorDataSize;
          TransmitData[cntTransmitData++] = DataSize;
          for (int cntByte=0; cntByte<DataSize; cntByte++)
          {
            TransmitData[cntTransmitData++] = (DataValue>>(((DataSize-1)-cntByte)*8))&0xFF;
          }
        }
        break;
        case OCTET_STRING_DATATYPE:
        {
          int MaxSize = OnlineNodeInformation[*((int *)IndexOfSender)].ObjectInformation[ObjectNr-1024].ActuatorDataSize;
          int StringLength = strlen(argv[1]);
          if (StringLength>MaxSize)
          {
            StringLength = MaxSize;
          }

          TransmitData[cntTransmitData++] = StringLength;
          for (int cntChar=0; cntChar<StringLength; cntChar++)
          {
            TransmitData[cntTransmitData++] = argv[1][cntChar];
          }
        }
        break;
        case FLOAT_DATATYPE:
        {
          float DataValue = atof(argv[1]);
          int DataSize = OnlineNodeInformation[*((int *)IndexOfSender)].ObjectInformation[ObjectNr-1024].ActuatorDataSize;
          TransmitData[cntTransmitData++] = DataSize;

          if (Float2VariableFloat(DataValue, DataSize, &TransmitData[cntTransmitData]) == 0)
          {
            cntTransmitData += cntTransmitData;
          }
        }
        break;
        case BIT_STRING_DATATYPE:
        {
          unsigned long DataValue = atoi(argv[1]);
          int DataSize = OnlineNodeInformation[*((int *)IndexOfSender)].ObjectInformation[ObjectNr-1024].ActuatorDataSize;
          TransmitData[cntTransmitData++] = DataSize;
          for (int cntByte=0; cntByte<DataSize; cntByte++)
          {
            TransmitData[cntTransmitData++] = (DataValue>>(((DataSize-1)-cntByte)*8))&0xFF;
          }
        }
        break;
        }
        SendMambaNetMessage(OnlineNodeInformation[*((int *)IndexOfSender)].MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, cntTransmitData);
      }
    }
  }

  return 0;
}

int main(int argc, char *argv[])
{
  struct termios oldtio;
  int oldflags;

  int maxfd;
  fd_set readfs;

  init(argc, argv);

  char *zErrMsg;
  if (sqlite3_open("/var/lib/axum/axum-engine.sqlite3", &axum_engine_db))
  {
    printf("Can't open database: axum-engine.sqlite3");
    sqlite3_close(axum_engine_db);
    return 1;
  }
  if (sqlite3_open("/var/lib/axum/node-templates.sqlite3", &node_templates_db))
  {
    printf("Can't open database: node-templates.sqlite");
    sqlite3_close(node_templates_db);
    return 1;
  }

  db_check_engine_functions();
  
//**************************************************************/
//Initialize AXUM Data
//**************************************************************/
  initialize_axum_data_struct();

  if (!dsp_open())
  {
    exit(1);
  }
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
  db_read_slot_config();

  //Source configuration
  db_read_src_config();
  
  //module_configuration
  db_read_module_config();
  
  //buss_configuration
  db_read_buss_config();

  //monitor_buss_configuration
  db_read_monitor_buss_config();
  
  //extern_source_configuration
  db_read_extern_src_config();

  //talkback_configuration
  db_read_talkback_config();
  
  //global_configuration
  db_read_global_config();

  //destination_configuration
  db_read_dest_config();
  
  //position to db
  if (sqlite3_exec(axum_engine_db, "SELECT * FROM position_to_db;", PositionTodBCallback, 0, &zErrMsg) != SQLITE_OK)
  {
    printf("SQL error: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
  }

  //db to position
  if (sqlite3_exec(axum_engine_db, "SELECT * FROM db_to_position;", dBToPositionCallback, 0, &zErrMsg) != SQLITE_OK)
  {
    printf("SQL error: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
  }

  printf("Axum engine process started, version 1.0\r\n");

//Update default values...
  for (int cntModule=0; cntModule<128; cntModule++)
  {
    for (int cntEQBand=0; cntEQBand<6; cntEQBand++)
    {
      AxumData.ModuleData[cntModule].EQBand[cntEQBand].Frequency = AxumData.ModuleData[cntModule].EQBand[cntEQBand].DefaultFrequency;
      AxumData.ModuleData[cntModule].EQBand[cntEQBand].Bandwidth = AxumData.ModuleData[cntModule].EQBand[cntEQBand].DefaultBandwidth;
      AxumData.ModuleData[cntModule].EQBand[cntEQBand].Type = AxumData.ModuleData[cntModule].EQBand[cntEQBand].DefaultType;
    }
  }

  //initalize global variables
  TTYDevice[0]                    = 0x00;
  NetworkInterface[0]         = 0x00;
  NetworkFileDescriptor       = -1;
  dump_packages                   = 0;
  TraceValue                      = 0x00;
  InitializeMambaNetStack(&AxumEngineDefaultObjects, AxumEngineCustomObjectInformation, NR_OF_STATIC_OBJECTS);
//  AxumEngineDefaultObjects.MambaNetAddress = 0x202;

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

  //get command line arguments
  GetCommandLineArguments(argc, argv, TTYDevice, NetworkInterface, &TraceValue);
  ChangeMambaNetStackTrace(TraceValue);

  //Change stdin (keyboard) settings to be 'character-driven' instead of line.
  SetupSTDIN(&oldtio, &oldflags);

  //Setup Network if serial port name is given.
  if (NetworkInterface[0] == 0x00)
  {
    printf("No network interface found.\r\n");
  }
  else
  {
    NetworkFileDescriptor = SetupNetwork(NetworkInterface, LocalMACAddress);
    if (NetworkFileDescriptor >= 0)
    {
      printf("using network interface device: %s [%02X:%02X:%02X:%02X:%02X:%02X].\r\n", NetworkInterface, LocalMACAddress[0], LocalMACAddress[1], LocalMACAddress[2], LocalMACAddress[3], LocalMACAddress[4], LocalMACAddress[5]);
    }
  }

  //Only one interface may be open for a application
  if (NetworkFileDescriptor>=0)
  {

//**************************************************************/
//Initialize Timer
//**************************************************************/
    struct itimerval MillisecondTimeOut;
    struct sigaction signal_action;
    sigset_t old_sigmask;

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

    sigprocmask(SIG_UNBLOCK, &signal_action.sa_mask, &old_sigmask);

    setitimer(ITIMER_REAL, &MillisecondTimeOut, NULL);


    //initalize maxfd for the idle-wait process 'select'
    maxfd = STDIN_FILENO;
    if (NetworkFileDescriptor > maxfd)
      maxfd = NetworkFileDescriptor;
    maxfd++;

    pthread_t tid;
    pthread_create(&tid, NULL, thread, NULL);

    DatabaseFileDescriptor = PQsocket(sql_conn);
    if(DatabaseFileDescriptor < 0) {
      //log_write("Invalid PostgreSQL socket!");
      ExitApplication = 1;
    }

    while (!ExitApplication)
    {
      //Set the sources which wakes the idle-wait process 'select'
      //Standard input (keyboard) and network.
      FD_ZERO(&readfs);
      FD_SET(STDIN_FILENO, &readfs);
      FD_SET(NetworkFileDescriptor, &readfs);
      FD_SET(DatabaseFileDescriptor, &readfs);

      // block (process is in idle mode) until input becomes available
//            int ReturnValue = select(maxfd, &readfs, NULL, NULL, NULL);
      int ReturnValue = pselect(maxfd, &readfs, NULL, NULL, NULL, &old_sigmask);
      if (ReturnValue == -1)
      {//no error or non-blocked signal)
//          perror("pselect:");
      }
      if (ReturnValue != -1)
      {//no error or non-blocked signal)
        //Test if stdin (keyboard) generated an event.
        if (FD_ISSET(STDIN_FILENO, &readfs))
        {
          unsigned char ReadedChar = getchar();
          switch (ReadedChar)
          {
          case ' ':
          {
            VUMeter = !VUMeter;
            if (VUMeter)
            {
              printf("VU Meter\n");
            }
            else
            {
              printf("PPM Meter\n");
            }
          }
          break;
          case '1':
          case '2':
          case '3':
          case '4':
          case '5':
          case '6':
          case '7':
          case '8':
          case '9':
          {
            MeterFrequency = ReadedChar-'1';
            //AudioFrom = (ReadedChar-'0');
            //printf("Selected Audio from: %d\n", AudioFrom);
          }
          break;
          case '0':
          {
            MeterFrequency = 10;
            //AudioFrom = (ReadedChar-'0');
            //printf("Selected Audio from: %d\n", AudioFrom);
          }
          break;
          case '+':
          {
          }
          break;
          case '-':
          {
          }
          break;
//                      case '1':
//                      {
//                          ChangeMambaNetStackTrace(0x01);
//                      }
//                      break;
//                      case '2':
//                      {
//                          ChangeMambaNetStackTrace(0x02);
//                      }
//                      break;
          case 'D':
          case 'd':
          {   //turn on/off packet dumps
            dump_packages = !dump_packages;
          }
          break;
          case 'Q':
          case 'q':
          {   //exit the program
            ExitApplication = 1;
          }
          break;
          case 'f':
          case 'F':
          {
            unsigned char TransmitData[128];
            unsigned int ObjectNr=1112;

            TransmitData[0] = (ObjectNr>>8)&0xFF;
            TransmitData[1] = ObjectNr&0xFF;
            TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;//Set actuator
            TransmitData[3] = FLOAT_DATATYPE;
            TransmitData[4] = 1;
            TransmitData[5] = 0xE7;

            SendMambaNetMessage(0x00000002, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
          }
          break;
          case 'g':
          case 'G':
          {
            unsigned char TransmitData[128];
            unsigned int ObjectNr=1112;

            TransmitData[0] = (ObjectNr>>8)&0xFF;
            TransmitData[1] = ObjectNr&0xFF;
            TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;//Set actuator
            TransmitData[3] = FLOAT_DATATYPE;
            TransmitData[4] = 1;
            TransmitData[5] = 0x26;

            SendMambaNetMessage(0x00000002, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
          }
          break;
          case 'h':
          case 'H':
          case 'j':
          case 'J':
          {
            unsigned char TransmitData[128];
            unsigned int ObjectNr=1112;

#define FLOAT_SIZE 2

            TransmitData[0] = (ObjectNr>>8)&0xFF;
            TransmitData[1] = ObjectNr&0xFF;
            TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;//Set actuator
            TransmitData[3] = FLOAT_DATATYPE;
            TransmitData[4] = FLOAT_SIZE;

            if ((ReadedChar == 'j') || (ReadedChar == 'J'))
            {
              cntFloatDebug += 0.25;
            }
            else
            {
              cntFloatDebug -= 0.25;
            }
            if (cntFloatDebug > 22)
            {
              cntFloatDebug = 22;
            }
            else if (cntFloatDebug < -96)
            {
              cntFloatDebug = -96;
            }
            printf("float32: %f ", cntFloatDebug);
            printf("[0x%08X]\r\n", *((unsigned int *)&cntFloatDebug));

            if (Float2VariableFloat(cntFloatDebug, FLOAT_SIZE, &TransmitData[5]) == 0)
            {
              float TemporyFloat;

              VariableFloat2Float(&TransmitData[5], FLOAT_SIZE, &TemporyFloat);
              if (FLOAT_SIZE == 1)
              {
                printf("hex: 0x%02X => %f\r\n", TransmitData[5], TemporyFloat);
              }
              else if (FLOAT_SIZE == 2)
              {
                printf("hex: 0x%02X%02X => %f\r\n", TransmitData[5], TransmitData[6], TemporyFloat);
              }
              else if (FLOAT_SIZE == 4)
              {
                printf("hex: 0x%02X%02X%02X%02X => %f\r\n", TransmitData[5], TransmitData[6], TransmitData[7], TransmitData[8], TemporyFloat);
              }
              SendMambaNetMessage(0x00000002, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 5+FLOAT_SIZE);
            }
            else
            {
              printf("can't convert[0x%08X]\r\n", *((unsigned int *)&cntFloatDebug));
            }
          }
          break;
          case 'A':
          case 'a':
          {   //debug: fader open
            //unsigned char Data[2];
            //unsigned int Position = 1023;
            ////Data[0] = (Position>>8)&0xFF;
            //Data[1] = Position&0xFF;

            //SendMambaNetMessage(0x00000002, AxumEngineDefaultObjects.MambaNetAddress, 0, MAMBANET_OBJECT_MESSAGETYPE, Data, 2);

            unsigned char TransmitData[128];
            unsigned int ObjectNr=1024;

            TransmitData[0] = (ObjectNr>>8)&0xFF;
            TransmitData[1] = ObjectNr&0xFF;
            TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
            TransmitData[3] = OCTET_STRING_DATATYPE;//Octet string
            TransmitData[4] = 5;//Size
            if (ReadedChar == 'A')
            {
              TransmitData[5] = 'A';
              TransmitData[6] = 'N';
              TransmitData[7] = 'T';
              TransmitData[8] = 'O';
              TransmitData[9] = 'N';
            }
            else
            {
              TransmitData[5] = 'a';
              TransmitData[6] = 'n';
              TransmitData[7] = 't';
              TransmitData[8] = 'o';
              TransmitData[9] = 'n';
            }

            SendMambaNetMessage(0x00000002, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 10);
          }
          break;
          case 'Z':
          case 'z':
          {   //debug: fader close
//                      unsigned char Data[2];
//                      unsigned int Position = 0;
//                      Data[0] = (Position>>8)&0xFF;
//                      Data[1] = Position&0xFF;
//
//                      SendMambaNetMessage(0x00000002, AxumEngineDefaultObjects.MambaNetAddress, 0, MAMBANET_OBJECT_MESSAGETYPE, Data, 2);
            unsigned char TransmitData[128];
            unsigned int ObjectNr=1040;

            TransmitData[0] = (ObjectNr>>8)&0xFF;
            TransmitData[1] = ObjectNr&0xFF;
            TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;//Set actuator
            TransmitData[3] = STATE_DATATYPE;
            TransmitData[4] = 1;
            if (ReadedChar == 'Z')
            {
              TransmitData[5] = 1;
            }
            else
            {
              TransmitData[5] = 0;
            }

            SendMambaNetMessage(0x00000002, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
          }
          break;
          case 's':
          case 'S':
          {
            unsigned char TransmitData[128];
            unsigned int ObjectNr=1072;

            TransmitData[0] = (ObjectNr>>8)&0xFF;
            TransmitData[1] = ObjectNr&0xFF;
            TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
            TransmitData[3] = STATE_DATATYPE;
            TransmitData[4] = 1;

            if (ReadedChar == 'S')
            {
              TransmitData[5] = 1;
            }
            else
            {
              TransmitData[5] = 0;
            }

            SendMambaNetMessage(0x00000002, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
          }
          break;
          case 'P':
          case 'p':
          case 'L':
          case 'l':
          {
            unsigned char TransmitData[128];
            unsigned int ObjectNr=1104;

            TransmitData[0] = (ObjectNr>>8)&0xFF;
            TransmitData[1] = ObjectNr&0xFF;
            TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
            TransmitData[3] = UNSIGNED_INTEGER_DATATYPE;
            TransmitData[4] = 2;

            if ((ReadedChar == 'p') || (ReadedChar == 'P'))
            {
              TransmitData[5] = 1023>>8;
              TransmitData[6] = 1023&0xFF;
            }
            else
            {
              TransmitData[5] = 0;
              TransmitData[6] = 0;
            }

            SendMambaNetMessage(0x00000002, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 7);
          }
          break;
          case 'I':
          case 'i':
          {
            unsigned char TransmitData[128];
            unsigned int ObjectNr= cntDebugObject++;

            TransmitData[0] = (ObjectNr>>8)&0xFF;
            TransmitData[1] = ObjectNr&0xFF;
            TransmitData[2] = MAMBANET_OBJECT_ACTION_GET_INFORMATION;
            TransmitData[3] = NO_DATA_DATATYPE;

            SendMambaNetMessage(0x00000103, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 4);
          }
          break;
          case 'N':
          case 'n':
          {
            unsigned char TransmitData[128];
            unsigned int ObjectNr= cntDebugNodeObject++;
            if (cntDebugNodeObject>12)
            {
              cntDebugNodeObject=0;
            }

            TransmitData[0] = (ObjectNr>>8)&0xFF;
            TransmitData[1] = ObjectNr&0xFF;
            TransmitData[2] = MAMBANET_OBJECT_ACTION_GET_SENSOR_DATA;
            TransmitData[3] = NO_DATA_DATATYPE;

            SendMambaNetMessage(0x00000103, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 4);
          }
          break;
          default:
          {
          }
          break;
          }
        }

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
                }
              }
            }
            numRead = recvfrom(NetworkFileDescriptor, buffer, 2048, 0, (struct sockaddr *)&fromAddress, (socklen_t *)&fromlen);
          }

          /*                      unsigned int cnt;
                                  unsigned char buffer[2048];
                                 int numRead = recv(NetworkFileDescriptor, buffer, 2048, 0);
                                  while (numRead>0)
                                  {   // bytes received, check the ethernet protocol.
                                      if ((buffer[12] == 0x88) && (buffer[13] == 0x20))
                                      {
                                          char AllowedToProcess = 1;
                                          unsigned char SourceHardwareAddress[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
                                          SourceHardwareAddress[0] = buffer[6];
                                          SourceHardwareAddress[1] = buffer[7];
                                          SourceHardwareAddress[2] = buffer[8];
                                          SourceHardwareAddress[3] = buffer[9];
                                          SourceHardwareAddress[4] = buffer[10];
                                          SourceHardwareAddress[5] = buffer[11];
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
                                              for (cnt=14; cnt<numRead; cnt++)
                                              {
                                                  EthernetReceiveBuffer[cntEthernetReceiveBufferTop++] = buffer[cnt];
                                                  if (cntEthernetReceiveBufferTop>4095)
                                                  {
                                                      cntEthernetReceiveBufferTop = 0;
                                                  }
                                              }

                                              while (cntEthernetReceiveBufferTop != cntEthernetReceiveBufferBottom)
                                              {
                                                  unsigned char ReadedByte = EthernetReceiveBuffer[cntEthernetReceiveBufferBottom++];
                                                  if (cntEthernetReceiveBufferBottom>4095)
                                                  {
                                                      cntEthernetReceiveBufferBottom = 0;
                                                  }

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
                                                          DecodeRawMambaNetMessage(EthernetMambaNetDecodeBuffer, cntEthernetMambaNetDecodeBuffer, EthernetInterfaceIndex, SourceHardwareAddress);
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
                                     numRead = recv(NetworkFileDescriptor, buffer, 2048, 0);
                                  }*/
        }
        //Test if the database notifier generated an event.
        if (FD_ISSET(DatabaseFileDescriptor, &readfs))
        {
          sql_processnotifies(); 
        } 
      }
    }
  }
  printf("Closing Axum engine process...\r\n");

  DeleteAllObjectListPerFunction();

  dsp_close();

  sqlite3_close(node_templates_db);
  sqlite3_close(axum_engine_db);
  CloseNetwork(NetworkFileDescriptor);
  CloseSTDIN(&oldtio, oldflags);

  log_write("Closing Engine");
  sql_close();
  log_close();

  return 0;
}


//This functions checks the commandline arguments and stores
//them in the corresponding variables.
void GetCommandLineArguments(int argc, char *argv[], char *TTYDevice, char *NetworkInterface, unsigned char *TraceValue)
{
  int cnt;

  for (cnt=0; cnt<argc; cnt++)
  {
    if (argv[cnt][0] == '-')
    {
      switch (argv[cnt][1])
      {
      case '1':
      {
        *TraceValue |= 1;
      }
      break;
      case '2':
      {
        *TraceValue |= 2;
      }
      break;
      case '3':
      {
        *TraceValue |= 3;
      }
      break;
      case 'T':
      {
        if (argv[cnt][2] != 0x00)
        {
          strcpy(TTYDevice, &argv[cnt][2]);
        }
        else
        {
          if (cnt<(argc-1))
          {
            cnt++;
            strcpy(TTYDevice, argv[cnt]);
          }
        }
      }
      break;
      case 'N':
      {
        if (argv[cnt][2] != 0x00)
        {
          strcpy(NetworkInterface, &argv[cnt][2]);
        }
        else
        {
          if (cnt<(argc-1))
          {
            cnt++;
            strcpy(NetworkInterface, argv[cnt]);
          }
        }
      }
      break;
      }
    }
  }
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
          printf("--- Object information ---\r\n");
          printf("ObjectNr          : %d\r\n", ObjectNr);
          if (Data[3] == OBJECT_INFORMATION_DATATYPE)
          {
            char TempString[256] = "";
            strncpy(TempString, (char *)&Data[5], 32);
            TempString[32] = 0;
            printf("Description       : %s\r\n", TempString);
            int cntData=32;

            unsigned char Service = Data[5+cntData++];
            printf("Service           : %d\r\n", Service);

            unsigned char DataType = Data[5+cntData++];
            unsigned char DataSize = Data[5+cntData++];
            unsigned char *PtrData = &Data[5+cntData];
            cntData += DataSize;

            printf("Sensor data type  : ");
            switch (DataType)
            {
            case NO_DATA_DATATYPE:
            {
              printf("NO_DATA\r\n");
            }
            break;
            case UNSIGNED_INTEGER_DATATYPE:
            {
              printf("UNSIGNED_INTEGER\r\n");
            }
            break;
            case SIGNED_INTEGER_DATATYPE:
            {
              printf("SIGNED_INTEGER\r\n");
            }
            break;
            case STATE_DATATYPE:
            {
              printf("STATE\r\n");
            }
            break;
            case OCTET_STRING_DATATYPE:
            {
              printf("OCTET_STRING\r\n");
            }
            break;
            case FLOAT_DATATYPE:
            {
              printf("FLOAT\r\n");
            }
            break;
            case BIT_STRING_DATATYPE:
            {
              printf("BIT_STRING\r\n");
            }
            break;
            case OBJECT_INFORMATION_DATATYPE:
            {
              printf("OBJECT_INFORMATION\r\n");
            }
            break;
            }
            if (Data2ASCIIString(TempString, DataType, DataSize, PtrData) == 0)
            {
              printf("Sensor minimal    : %s\r\n", TempString);
            }
            PtrData = &Data[5+cntData];
            cntData += DataSize;
            if (Data2ASCIIString(TempString, DataType, DataSize, PtrData) == 0)
            {
              printf("Sensor maximal    : %s\r\n", TempString);
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
              printf("NO_DATA\r\n");
            }
            break;
            case UNSIGNED_INTEGER_DATATYPE:
            {
              printf("UNSIGNED_INTEGER\r\n");
            }
            break;
            case SIGNED_INTEGER_DATATYPE:
            {
              printf("SIGNED_INTEGER\r\n");
            }
            break;
            case STATE_DATATYPE:
            {
              printf("STATE\r\n");
            }
            break;
            case OCTET_STRING_DATATYPE:
            {
              printf("OCTET_STRING\r\n");
            }
            break;
            case FLOAT_DATATYPE:
            {
              printf("FLOAT\r\n");
            }
            break;
            case BIT_STRING_DATATYPE:
            {
              printf("BIT_STRING\r\n");
            }
            break;
            case OBJECT_INFORMATION_DATATYPE:
            {
              printf("OBJECT_INFORMATION\r\n");
            }
            break;
            }

            if (Data2ASCIIString(TempString, DataType, DataSize, PtrData) == 0)
            {
              printf("Actuator minimal  : %s\r\n", TempString);
            }
            PtrData = &Data[5+cntData];
            cntData += DataSize;
            if (Data2ASCIIString(TempString, DataType, DataSize, PtrData) == 0)
            {
              printf("Actuator maximal  : %s\r\n", TempString);
            }
            printf("\r\n");
          }
          else if (Data[3] == 0)
          {
            printf("No data\r\n");
            printf("\r\n");
          }
        }
        else if (Data[2] == MAMBANET_OBJECT_ACTION_SENSOR_DATA_RESPONSE)
        {   // sensor response
          char TempString[256] = "";
          printf("Sensor response, ObjectNr %d", ObjectNr);

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

            if (ObjectNr == 7)
            {   //Major firmware id
              char TableName[32];
              char SQLQuery[8192];

              printf("FirmwareMajorRevision: %d, Do check node information\n", Data[5]);

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
                      SetAxum_ModuleMixMinus(cntModule);
                      SetAxum_ModuleInsertSource(cntModule);
                      SetAxum_BussLevels(cntModule);
                    }

                    for (int cntDSPCard=0; cntDSPCard<4; cntDSPCard++)
                    {
                      SetAxum_ExternSources(cntDSPCard);
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

                CallbackNodeIndex = IndexOfSender;
                sprintf(TableName, "node_info_%04x_%04x_%02x_0100", OnlineNodeInformation[IndexOfSender].ManufacturerID, OnlineNodeInformation[IndexOfSender].ProductID, 0x01);

                //Get Number of objects
                printf("Get Number of objects\n");
                sprintf(SQLQuery,   "SELECT \"Sensor Data Value\" FROM %s WHERE \"Object Number\" = 11;", TableName);
                char *zErrMsg;
                if (sqlite3_exec(node_templates_db, SQLQuery, NumberOfCustomObjectsCallback, 0, &zErrMsg) != SQLITE_OK)
                {
                  printf("SQL error: %s\n", zErrMsg);
                  sqlite3_free(zErrMsg);
                }

                printf("Get all required object data: %s\n", TableName);
                //Get all required object data
                sprintf(SQLQuery,   "SELECT * FROM %s;", TableName);
                if (sqlite3_exec(node_templates_db, SQLQuery, ObjectInformationCallback, 0, &zErrMsg) != SQLITE_OK)
                {
                  printf("SQL error: %s\n", zErrMsg);
                  sqlite3_free(zErrMsg);
                }

                printf("Get slot number ObjectNr if available: %s\n", TableName);
                //Get slot number ObjectNr if available
                sprintf(SQLQuery,   "SELECT \"Object Number\" FROM %s WHERE \"Object description\" = \"Slot number\";", TableName);
                if (sqlite3_exec(node_templates_db, SQLQuery, SlotNumberObjectCallback, 0, &zErrMsg) != SQLITE_OK)
                {
                  printf("SQL error: %s\n", zErrMsg);
                  sqlite3_free(zErrMsg);
                }

                printf("Get input channel count ObjectNr if available: %s\n", TableName);
                //Get input channel count ObjectNr if available
                sprintf(SQLQuery,   "SELECT \"Object Number\" FROM %s WHERE \"Object description\" = \"Input channel count\";", TableName);
                if (sqlite3_exec(node_templates_db, SQLQuery, InputChannelCountObjectCallback, 0, &zErrMsg) != SQLITE_OK)
                {
                  printf("SQL error: %s\n", zErrMsg);
                  sqlite3_free(zErrMsg);
                }

                printf("Get output channel count ObjectNr if available: %s\n", TableName);
                //Get output channel count ObjectNr if available
                sprintf(SQLQuery,   "SELECT \"Object Number\" FROM %s WHERE \"Object description\" = \"Output channel count\";", TableName);
                if (sqlite3_exec(node_templates_db, SQLQuery, OutputChannelCountObjectCallback, 0, &zErrMsg) != SQLITE_OK)
                {
                  printf("SQL error: %s\n", zErrMsg);
                  sqlite3_free(zErrMsg);
                }

                if (OnlineNodeInformation[IndexOfSender].SlotNumberObjectNr != -1)
                {
                  unsigned char TransmitBuffer[64];
                  unsigned char cntTransmitBuffer;
                  unsigned int ObjectNumber = OnlineNodeInformation[IndexOfSender].SlotNumberObjectNr;

                  printf("Get Slot Number @ ObjectNr: %d\n", ObjectNumber);

                  cntTransmitBuffer = 0;
                  TransmitBuffer[cntTransmitBuffer++] = (ObjectNumber>>8)&0xFF;
                  TransmitBuffer[cntTransmitBuffer++] = ObjectNumber&0xFF;
                  TransmitBuffer[cntTransmitBuffer++] = MAMBANET_OBJECT_ACTION_GET_SENSOR_DATA;
                  TransmitBuffer[cntTransmitBuffer++] = NO_DATA_DATATYPE;
                  //TransmitBuffer[cntTransmitBuffer++] = 0;

                  SendMambaNetMessage(OnlineNodeInformation[IndexOfSender].MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, 1, TransmitBuffer, cntTransmitBuffer);
                }

                printf("Make or get node defaults\n");
                //Make or get node defaults
                sprintf(SQLQuery, "	CREATE TABLE IF NOT EXISTS defaults_node_%08lx	\
												(																\
													\"Object Number\" int PRIMARY KEY,				\
													\"Default Data\" VARCHAR(64)										\
												);\n", FromAddress);

                if (sqlite3_exec(axum_engine_db, SQLQuery, callback, 0, &zErrMsg) != SQLITE_OK)
                {
                  printf("SQL error: %s\n", zErrMsg);
                  sqlite3_free(zErrMsg);
                }

                sprintf(SQLQuery, "	SELECT * FROM defaults_node_%08lx;\n", FromAddress);
                if (sqlite3_exec(axum_engine_db, SQLQuery, NodeDefaultsCallback, (void *)&IndexOfSender, &zErrMsg) != SQLITE_OK)
                {
                  printf("SQL error: %s\n", zErrMsg);
                  sqlite3_free(zErrMsg);
                }

                printf("Make or get node configuration\n");
                //Make or get node configuration
                sprintf(SQLQuery, "	CREATE TABLE IF NOT EXISTS configuration_node_%08lx	\
												(																	\
													\"Object Number\" int PRIMARY KEY,						\
													Function int												\
												);\n", FromAddress);

                if (sqlite3_exec(axum_engine_db, SQLQuery, callback, 0, &zErrMsg) != SQLITE_OK)
                {
                  printf("SQL error: %s\n", zErrMsg);
                  sqlite3_free(zErrMsg);
                }


                sprintf(SQLQuery, "	SELECT * FROM configuration_node_%08lx;\n", FromAddress);
                if (sqlite3_exec(axum_engine_db, SQLQuery, NodeConfigurationCallback, (void *)&IndexOfSender, &zErrMsg) != SQLITE_OK)
                {
                  printf("SQL error: %s\n", zErrMsg);
                  sqlite3_free(zErrMsg);
                }
              }
            }
            if (((signed int)ObjectNr) == OnlineNodeInformation[IndexOfSender].SlotNumberObjectNr)
            {
              printf("0x%08lX @Slot: %d\n", FromAddress, Data[5]+1);
              char SQLQuery[8192];

              for (int cntSlot=0; cntSlot<42; cntSlot++)
              {
                if (cntSlot != Data[5])
                { //other slot then current inserted
                  if (AxumData.RackOrganization[cntSlot] == FromAddress)
                  {
                    AxumData.RackOrganization[cntSlot] = 0;

                    sprintf(SQLQuery,   "UPDATE rack_organization SET MambaNetAddress = 0, InputChannelCount = 0, OutputChannelCount = 0 WHERE SlotNr = %d;", cntSlot+1);
                    char *zErrMsg;
                    if (sqlite3_exec(axum_engine_db, SQLQuery, callback, 0, &zErrMsg) != SQLITE_OK)
                    {
                      printf("SQL error: %s\n", zErrMsg);
                      sqlite3_free(zErrMsg);
                    }
                  }
                }
              }
              AxumData.RackOrganization[Data[5]] = FromAddress;

              sprintf(SQLQuery,   "UPDATE rack_organization SET MambaNetAddress = %ld WHERE SlotNr = %d;", FromAddress, Data[5]+1);
              char *zErrMsg;
              if (sqlite3_exec(axum_engine_db, SQLQuery, callback, 0, &zErrMsg) != SQLITE_OK)
              {
                printf("SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
              }

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
              for (int cntSource=0; cntSource<1280; cntSource++)
              {
                if (  (AxumData.SourceData[cntSource].InputData[0].MambaNetAddress == FromAddress) ||
                      (AxumData.SourceData[cntSource].InputData[1].MambaNetAddress == FromAddress))
                { //this source is changed, update modules!
                  printf("Found source: %d\n", cntSource);
                  for (int cntModule=0; cntModule<128; cntModule++)
                  {
                    if (AxumData.ModuleData[cntModule].Source == (cntSource+1))
                    {
                      printf("Found module: %d\n", cntModule);
                      SetAxum_ModuleSource(cntModule);
                      SetAxum_ModuleMixMinus(cntModule);
                    }
                  }
                  for (int cntDSPCard=0; cntDSPCard<4; cntDSPCard++)
                  {
                    for (int cntExt=0; cntExt<8; cntExt++)
                    {
                      if (AxumData.ExternSource[cntDSPCard].Ext[cntExt] == (cntSource+1))
                      {
                        printf("Found extern input @ %d\n", cntDSPCard);
                        SetAxum_ExternSources(cntDSPCard);
                      }
                    }
                  }
                  for (int cntTalkback=0; cntTalkback<16; cntTalkback++)
                  {
                    if ((AxumData.Talkback[cntTalkback].Source-160) == (cntSource+1))
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
                }
              }
            }
            else if (OnlineNodeInformation[IndexOfSender].SlotNumberObjectNr != -1)
            {
              printf("Check for Channel Counts: %d, %d\n", OnlineNodeInformation[IndexOfSender].InputChannelCountObjectNr, OnlineNodeInformation[IndexOfSender].OutputChannelCountObjectNr);
              if (((signed int)ObjectNr) == OnlineNodeInformation[IndexOfSender].InputChannelCountObjectNr)
              {
                char SQLQuery[8192];
                sprintf(SQLQuery,   "UPDATE rack_organization SET InputChannelCount = %d WHERE MambaNetAddress = %ld;", Data[5], FromAddress);
                char *zErrMsg;
                printf(SQLQuery);
                printf("\n" );
                if (sqlite3_exec(axum_engine_db, SQLQuery, callback, 0, &zErrMsg) != SQLITE_OK)
                {
                  printf("SQL error: %s\n", zErrMsg);
                  sqlite3_free(zErrMsg);
                }
              }
              else if (((signed int)ObjectNr) == OnlineNodeInformation[IndexOfSender].OutputChannelCountObjectNr)
              {
                char SQLQuery[8192];
                sprintf(SQLQuery,   "UPDATE rack_organization SET OutputChannelCount = %d WHERE MambaNetAddress = %ld;", Data[5], FromAddress);
                char *zErrMsg;
                printf(SQLQuery);
                printf("\n" );
                if (sqlite3_exec(axum_engine_db, SQLQuery, callback, 0, &zErrMsg) != SQLITE_OK)
                {
                  printf("SQL error: %s\n", zErrMsg);
                  sqlite3_free(zErrMsg);
                }
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

                    while (TempData != 0)
                    {
                      if (TempData>0)
                      {
                        CurrentSource++;
                        if (CurrentSource > 1280)
                        {
                          CurrentSource = 0;
                        }

                        //skip used hybrids
                        while ((CurrentSource != 0) && (Axum_MixMinusSourceUsed(CurrentSource-1) != -1))
                        {
                          CurrentSource++;
                          if (CurrentSource > 1280)
                          {
                            CurrentSource = 0;
                          }
                        }

                        //skip empty sources
                        while ((CurrentSource != 0) && ((AxumData.SourceData[CurrentSource-1].InputData[0].MambaNetAddress == 0x00000000) && (AxumData.SourceData[CurrentSource-1].InputData[1].MambaNetAddress == 0x00000000)))
                        {
                          CurrentSource++;
                          if (CurrentSource > 1280)
                          {
                            CurrentSource = 0;
                          }
                        }
                        TempData--;
                      }
                      else
                      {
                        CurrentSource--;
                        if (CurrentSource < 0)
                        {
                          CurrentSource = 1280;
                        }

                        //skip empty sources
                        while ((CurrentSource != 0) && ((AxumData.SourceData[CurrentSource-1].InputData[0].MambaNetAddress == 0x00000000) && (AxumData.SourceData[CurrentSource-1].InputData[1].MambaNetAddress == 0x00000000)))
                        {
                          CurrentSource--;
                          if (CurrentSource < 0)
                          {
                            CurrentSource = 1280;
                          }
                        }

                        //skip used hybrids
                        while ((CurrentSource != 0) && (Axum_MixMinusSourceUsed(CurrentSource-1) != -1))
                        {
                          CurrentSource--;
                          if (CurrentSource < 0)
                          {
                            CurrentSource = 1280;
                          }
                        }
                        TempData++;
                      }
                    }

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
                  /*
                                                  int OldSource = AxumData.ModuleData[ModuleNr].Source;
                                                  if (OldSource != CurrentSource)
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
                                                      AxumData.ModuleData[ModuleNr].Source = CurrentSource;
                                                                      SetAxum_ModuleSource(ModuleNr);
                                                      SetAxum_ModuleMixMinus(ModuleNr);

                                                      unsigned int FunctionNrToSent = (ModuleNr<<12);
                                                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                                                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                                                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                                                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

                                                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_SOURCE_A);
                                                      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_SOURCE_B);

                                                      if (OldSource != 0)
                                                      {
                                                        FunctionNrToSent = 0x05000000 | ((OldSource-1)<<12);
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

                                                      if (CurrentSource != 0)
                                                      {
                                                        FunctionNrToSent = 0x05000000 | ((CurrentSource-1)<<12);
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
                                                  }*/
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

                    if (TempData)
                    {
                      if (AxumData.ModuleData[ModuleNr].Source != 0)
                      {
                        AxumData.SourceData[AxumData.ModuleData[ModuleNr].Source-1].Phantom = !AxumData.SourceData[AxumData.ModuleData[ModuleNr].Source-1].Phantom;
                      }
                      else
                      {
                        int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                        if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                        {
                          if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                          {
                            AxumData.SourceData[AxumData.ModuleData[ModuleNr].Source-1].Phantom = !AxumData.SourceData[AxumData.ModuleData[ModuleNr].Source-1].Phantom;
                          }
                        }
                      }

                      unsigned int DisplayFunctionNr = 0x05000000 | ((AxumData.ModuleData[ModuleNr].Source-1)<<12) | SOURCE_FUNCTION_PHANTOM;

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

                    if (TempData)
                    {
                      if (AxumData.ModuleData[ModuleNr].Source != 0)
                      {
                        AxumData.SourceData[AxumData.ModuleData[ModuleNr].Source-1].Pad = !AxumData.SourceData[AxumData.ModuleData[ModuleNr].Source-1].Pad;
                      }
                      else
                      {
                        int delay_time = (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime-OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime)*10;
                        if (OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary>=0)
                        {
                          if (delay_time>=OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary)
                          {
                            AxumData.SourceData[AxumData.ModuleData[ModuleNr].Source-1].Pad = !AxumData.SourceData[AxumData.ModuleData[ModuleNr].Source-1].Pad;
                          }
                        }
                      }

                      unsigned int DisplayFunctionNr = 0x05000000 | ((AxumData.ModuleData[ModuleNr].Source-1)<<12) | SOURCE_FUNCTION_PAD;
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

                    if (AxumData.ModuleData[ModuleNr].Source != 0)
                    {
                      int SourceNr = AxumData.ModuleData[ModuleNr].Source-1;
                      AxumData.SourceData[SourceNr].Gain += (float)TempData/10;
                      if (AxumData.SourceData[SourceNr].Gain<20)
                      {
                        AxumData.SourceData[SourceNr].Gain = 20;
                      }
                      else if (AxumData.SourceData[SourceNr].Gain>20)
                      {
                        AxumData.SourceData[SourceNr].Gain = 75;
                      }

                      unsigned int DisplayFunctionNr = 0x05000000 | ((AxumData.ModuleData[ModuleNr].Source-1)<<12) | SOURCE_FUNCTION_GAIN;
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

                    if (TempData)
                    {
                      if (AxumData.ModuleData[ModuleNr].Source != 0)
                      {
                        AxumData.SourceData[AxumData.ModuleData[ModuleNr].Source-1].Gain = 0;

                        unsigned int DisplayFunctionNr = 0x05000000 | ((AxumData.ModuleData[ModuleNr].Source-1)<<12) | SOURCE_FUNCTION_GAIN;
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

                    //AxumData.ModuleData[ModuleNr].FaderLevel = (((float)118*TempData)/(SensorDataMaximal-SensorDataMinimal))-96;

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

                    if (AxumData.ModuleData[ModuleNr].Source > 0)
                    {
                      SensorReceiveFunctionNumber = 0x05000000 | ((AxumData.ModuleData[ModuleNr].Source-1)<<12);
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

                      if (AxumData.ModuleData[ModuleNr].Source > 0)
                      {
                        SensorReceiveFunctionNumber = 0x05000000 | ((AxumData.ModuleData[ModuleNr].Source-1)<<12);
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
                    /*SetAxum_BussLevels(ModuleNr);
                    SetAxum_ModuleMixMinus(ModuleNr);

                    CheckObjectsToSent(SensorReceiveFunctionNumber);

                    unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

                    if (AxumData.ModuleData[ModuleNr].Source != 0)
                    {
                    int SourceNr = AxumData.ModuleData[ModuleNr].Source-1;
                    FunctionNrToSent = 0x05000000 | (SourceNr<<12);
                    CheckObjectsToSent(FunctionNrToSent | (SOURCE_FUNCTION_MODULE_BUSS_1_2_ON+(BussNr*(SOURCE_FUNCTION_MODULE_BUSS_3_4_ON-SOURCE_FUNCTION_MODULE_BUSS_1_2_ON))));
                    CheckObjectsToSent(FunctionNrToSent | (SOURCE_FUNCTION_MODULE_BUSS_1_2_OFF+(BussNr*(SOURCE_FUNCTION_MODULE_BUSS_3_4_OFF-SOURCE_FUNCTION_MODULE_BUSS_1_2_OFF))));
                    CheckObjectsToSent(FunctionNrToSent | (SOURCE_FUNCTION_MODULE_BUSS_1_2_ON_OFF+(BussNr*(SOURCE_FUNCTION_MODULE_BUSS_3_4_ON_OFF-SOURCE_FUNCTION_MODULE_BUSS_1_2_ON_OFF))));
                    }

                    //Do interlock
                    if ((AxumData.BussMasterData[BussNr].Interlock) && (TempData))
                    {
                    for (int cntModule=0; cntModule<128; cntModule++)
                    {
                    if (ModuleNr != cntModule)
                    {
                        if (AxumData.ModuleData[cntModule].Buss[BussNr].On)
                    {
                    AxumData.ModuleData[cntModule].Buss[BussNr].On = 0;

                            SetAxum_BussLevels(cntModule);
                    SetAxum_ModuleMixMinus(cntModule);

                        unsigned int FunctionNrToSent = ((cntModule<<12)&0xFFF000);
                    CheckObjectsToSent(FunctionNrToSent | FunctionNr);

                        FunctionNrToSent = ((cntModule<<12)&0xFFF000);
                        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

                    if (AxumData.ModuleData[cntModule].Source != 0)
                    {
                    int SourceNr = AxumData.ModuleData[cntModule].Source-1;
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
                    AxumData.Monitor[cntMonitorBuss].Buss[ExtNr] = 1;

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
                    }*/
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

                    if (AxumData.ModuleData[ModuleNr].Source != 0)
                    {
                      int SourceNr = AxumData.ModuleData[ModuleNr].Source-1;
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
                          if (AxumData.ModuleData[cntModule].Source == (SourceNr+1))
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
                    if (AxumData.ModuleData[ModuleNr].Source > 0)
                    {
                      SensorReceiveFunctionNumber = 0x05000000 | ((AxumData.ModuleData[ModuleNr].Source-1)<<12);
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

                    if (AxumData.ModuleData[ModuleNr].Source != 0)
                    {
                      int SourceNr = AxumData.ModuleData[ModuleNr].Source-1;


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
                        if (AxumData.ModuleData[cntModule].Source == (SourceNr+1))
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
                    unsigned char *MonitorSwitchState;
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
                        AxumData.Monitor[MonitorBussNr].Buss[ExtNr] = 1;

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
                        if (AxumData.DestinationData[cntDestination].Source == ((signed int)(17+MonitorBussNr)))
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
                        if (AxumData.DestinationData[cntDestination].Source == ((signed int)(17+MonitorBussNr)))
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

///                                                AxumData.Monitor[MonitorBussNr].PhonesLevel = (((float)118*TempData)/1023)-96;
                      float SensorDataMinimal = OnlineNodeInformation[IndexOfSender].ObjectInformation[ObjectNr-1024].SensorDataMinimal;
                      float SensorDataMaximal = OnlineNodeInformation[IndexOfSender].ObjectInformation[ObjectNr-1024].SensorDataMaximal;

                      int Position = (TempData*1023)/(SensorDataMaximal-SensorDataMinimal);
                      float dB = Position2dB[Position];
                      //dB -= AxumData.LevelReserve;
                      dB +=10; //20dB reserve

                      AxumData.Monitor[MonitorBussNr].PhonesLevel = dB;

                      CheckObjectsToSent(SensorReceiveFunctionNumber);

                      for (int cntDestination=0; cntDestination<1280; cntDestination++)
                      {
                        if (AxumData.DestinationData[cntDestination].Source == ((signed int)(17+MonitorBussNr)))
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
                        if (AxumData.DestinationData[cntDestination].Source == ((signed int)(17+MonitorBussNr)))
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
                        if (AxumData.DestinationData[cntDestination].Source == ((signed int)(17+MonitorBussNr)))
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
                        if (AxumData.DestinationData[cntDestination].Source == ((signed int)(17+MonitorBussNr)))
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

                      //AxumData.Monitor[MonitorBussNr].SpeakerLevel = (((float)118*TempData)/1023)-96;
                      float SensorDataMinimal = OnlineNodeInformation[IndexOfSender].ObjectInformation[ObjectNr-1024].SensorDataMinimal;
                      float SensorDataMaximal = OnlineNodeInformation[IndexOfSender].ObjectInformation[ObjectNr-1024].SensorDataMaximal;

                      int Position = (TempData*1023)/(SensorDataMaximal-SensorDataMinimal);
                      float dB = Position2dB[Position];
                      //dB -= AxumData.LevelReserve;
                      dB += 10;

                      AxumData.Monitor[MonitorBussNr].SpeakerLevel = dB;
                      CheckObjectsToSent(SensorReceiveFunctionNumber);

                      for (int cntDestination=0; cntDestination<1280; cntDestination++)
                      {
                        if (AxumData.DestinationData[cntDestination].Source == ((signed int)(17+MonitorBussNr)))
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
                        if (AxumData.DestinationData[cntDestination].Source == ((signed int)(17+MonitorBussNr)))
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
                        if (AxumData.DestinationData[cntDestination].Source == ((signed int)(17+MonitorBussNr)))
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
                      if (AxumData.ModuleData[cntModule].Source == ((signed int)(SourceNr+1)))
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
                      if (AxumData.ModuleData[cntModule].Source == ((signed int)(SourceNr+1)))
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
                      if (AxumData.ModuleData[cntModule].Source == ((signed int)(SourceNr+1)))
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
                            SetAxum_ModuleMixMinus(cntModule);

                            unsigned int FunctionNrToSent = ((cntModule<<12)&0xFFF000);
                            CheckObjectsToSent(FunctionNrToSent | FunctionNr);

                            FunctionNrToSent = ((cntModule<<12)&0xFFF000);
                            CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                            CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                            CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                            CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

                            if (AxumData.ModuleData[cntModule].Source != 0)
                            {
                              int SourceNr = AxumData.ModuleData[cntModule].Source-1;
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
                        if (AxumData.ModuleData[cntModule].Source == ((signed int)(SourceNr+1)))
                        {
                          AxumData.ModuleData[cntModule].Buss[BussNr].On = 1;
                          SetAxum_BussLevels(cntModule);
                          SetAxum_ModuleMixMinus(cntModule);

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
                            AxumData.Monitor[cntMonitorBuss].Buss[ExtNr] = 1;

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
                        if (AxumData.ModuleData[cntModule].Source == ((signed int)(SourceNr+1)))
                        {
                          AxumData.ModuleData[cntModule].Buss[BussNr].On = 0;
                          SetAxum_BussLevels(cntModule);
                          SetAxum_ModuleMixMinus(cntModule);

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
                            AxumData.Monitor[cntMonitorBuss].Buss[ExtNr] = 1;

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
                      if (AxumData.ModuleData[cntModule].Source == ((signed int)(SourceNr+1)))
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
                        SetAxum_ModuleMixMinus(cntModule);

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
                            SetAxum_ModuleMixMinus(cntModule);

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
                          AxumData.Monitor[cntMonitorBuss].Buss[ExtNr] = 1;

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
                      if (AxumData.ModuleData[cntModule].Source == ((signed int)(SourceNr+1)))
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
                        if (AxumData.ModuleData[cntModule].Source == ((signed int)(SourceNr+1)))
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
                      if (AxumData.ModuleData[cntModule].Source == ((signed int)(SourceNr+1)))
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
                      if (AxumData.ModuleData[cntModule].Source == ((signed int)(SourceNr+1)))
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
                      if (AxumData.ModuleData[cntModule].Source == ((signed int)(SourceNr+1)))
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
                      if (AxumData.ModuleData[cntModule].Source == ((signed int)(SourceNr+1)))
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

                    int NrOfSources = 0;
                    for (int cntSource=0; cntSource<1280; cntSource++)
                    {
                      if (!((AxumData.SourceData[cntSource].InputData[0].MambaNetAddress == 0x00000000) && (AxumData.SourceData[cntSource].InputData[1].MambaNetAddress == 0x00000000)))
                      {
                        NrOfSources++;
                      }
                    }

                    printf("NrOfSources: %d\n", NrOfSources);

                    AxumData.DestinationData[DestinationNr].Source += TempData;
                    while (AxumData.DestinationData[DestinationNr].Source > (32+128+NrOfSources))
                    {
                      AxumData.DestinationData[DestinationNr].Source -= (32+128+NrOfSources)+1;
                    }

                    if (AxumData.DestinationData[DestinationNr].Source<0)
                    {
                      AxumData.DestinationData[DestinationNr].Source = 32+128+NrOfSources;
                    }
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

//Function that initializes stdin
//Actually not a 'stack' functionality but usefull in the axum.
//
//  - termios oldtio must be declared before put in this fucntion.
//   when you are finished using stdin the struct must be parameter
//   of the CloseSTDIN(...) function.
//  - oldflags must be declared before put in this function.
//   when you are finished using stdin the struct must be parameter
//   of the CloseSTDIN(...) function.
void SetupSTDIN(struct termios *oldtio, int *oldflags)
{
  struct termios tio;

  //Get all attributes of the stdin
  if (tcgetattr(STDIN_FILENO, &tio) != 0)
  {
    perror("error in tcgetattr from stdin");
  }

  //store the current attributes to set back if we leave the process.
  memcpy(oldtio, &tio, sizeof(struct termios));

  //do not use line-interpetation and/or echo features of the tty.
  tio.c_lflag &= ~(ICANON | ECHO);

  // Set the new attributes for stdin
  if (tcsetattr(STDIN_FILENO, TCSANOW, &tio) != 0)
  {
    perror("error in tcsetattr from stdin.\r\n");
  }

  // Get the currently set flags for stdin, remember them to set back later.
  *oldflags = fcntl(STDIN_FILENO, F_GETFL);
  if (*oldflags < 0)
  {
    perror("error in fcntl(STDIN_FILENO, F_GETFL)");
  }

  // Set the O_NONBLOCK flag for stdin
  if (fcntl(STDIN_FILENO, F_SETFL, *oldflags | O_NONBLOCK)<0)
  {
    perror("error in fcntl(STDIN_FILENO, F_SETFL)");
  }
}

//Function that initializes stdin
//Actually not a 'stack' functionality but usefull in the axum.
//
//This functions sets back the oldtio and oldflags for stdin.
void CloseSTDIN(struct termios *oldtio, int oldflags)
{
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, oldtio) != 0)
  {
    perror("error in tcsetattr from stdin.");
  }
  fcntl(STDIN_FILENO, F_SETFL, oldflags);
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
    unsigned char cntRetry=0;

    while ((CharactersSend < buffersize) && (cntRetry<10))
    {
      CharactersSend = sendto(NetworkFileDescriptor, buffer, buffersize, 0, (struct sockaddr *) &socket_address, sizeof(socket_address));

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

/*
void EthernetMambaNetMessageTransmitCallback(unsigned char *buffer, unsigned char buffersize, unsigned char hardware_address[16])
{
    unsigned char TransmitBuffer[2048];

    TransmitBuffer[0] = hardware_address[0];
    TransmitBuffer[1] = hardware_address[1];
    TransmitBuffer[2] = hardware_address[2];
    TransmitBuffer[3] = hardware_address[3];
    TransmitBuffer[4] = hardware_address[4];
    TransmitBuffer[5] = hardware_address[5];
    TransmitBuffer[6] = LocalMACAddress[0];
    TransmitBuffer[7] = LocalMACAddress[1];
    TransmitBuffer[8] = LocalMACAddress[2];
    TransmitBuffer[9] = LocalMACAddress[3];
    TransmitBuffer[10] = LocalMACAddress[4];
    TransmitBuffer[11] = LocalMACAddress[5];
    TransmitBuffer[12] = 0x88;
    TransmitBuffer[13] = 0x20;
    for (int cntBuffer=0; cntBuffer<buffersize; cntBuffer++)
    {
        TransmitBuffer[14+cntBuffer] = buffer[cntBuffer];
    }

    if (NetworkFileDescriptor >= 0)
    {
        int CharactersSend = 0;
        unsigned char cntRetry=0;

        while ((CharactersSend < buffersize) && (cntRetry<10))
        {
            CharactersSend = send(NetworkFileDescriptor, TransmitBuffer, 14+buffersize, 0);

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
*/

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
      //TransmitBuffer[cntTransmitBuffer++] = 0;

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

    if (OnlineNodeInformation[Index].SlotNumberObjectNr != -1)
    {
      printf("remove 0x%08X @Slot: %d\n", OnlineNodeInformation[Index].MambaNetAddress, OnlineNodeInformation[Index].SlotNumberObjectNr);
      char SQLQuery[8192];

      sprintf(SQLQuery,   "UPDATE rack_organization SET MambaNetAddress = 0, InputChannelCount = 0, OutputChannelCount = 0 WHERE SlotNr = %d;", OnlineNodeInformation[Index].SlotNumberObjectNr);
      printf(SQLQuery);
      printf("\n");
      char *zErrMsg;
      if (sqlite3_exec(axum_engine_db, SQLQuery, callback, 0, &zErrMsg) != SQLITE_OK)
      {
        printf("SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
      }
    }

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

    if (PtrDSP_HPIA[2] != NULL)
    {
      //VU or PPM
      //float dBLevel[48];
      for (int cntChannel=0; cntChannel<48; cntChannel++)
      {
        unsigned int MeterAddress = SummingDSPBussMeterPPM+cntChannel*4;
        if (VUMeter)
        {
          MeterAddress = SummingDSPBussMeterVU+cntChannel*4;
        }

        *PtrDSP_HPIA[2] = MeterAddress;
        float LinearLevel = *((float *)PtrDSP_HPID[2]);
        if (!VUMeter)
        {
          *((float *)PtrDSP_HPID[2]) = 0;
        }
        if (LinearLevel != 0)
        {
          SummingdBLevel[cntChannel] = 20*log10(LinearLevel/2147483647);
        }
        else
        {
          SummingdBLevel[cntChannel] = -2000;
        }
      }

      //buss audio level
      for (int cntBuss=0; cntBuss<16; cntBuss++)
      {
        CheckObjectsToSent(0x01000000 | (cntBuss<<12) | BUSS_FUNCTION_AUDIO_LEVEL_LEFT);
        CheckObjectsToSent(0x01000000 | (cntBuss<<12) | BUSS_FUNCTION_AUDIO_LEVEL_RIGHT);
      }

      //monitor buss audio level
      for (int cntMonitorBuss=0; cntMonitorBuss<2; cntMonitorBuss++)
      {
        CheckObjectsToSent(0x02000000 | (cntMonitorBuss<<12) | MONITOR_BUSS_FUNCTION_AUDIO_LEVEL_LEFT);
        CheckObjectsToSent(0x02000000 | (cntMonitorBuss<<12) | MONITOR_BUSS_FUNCTION_AUDIO_LEVEL_RIGHT);
      }
    }
  }

  if ((cntMillisecondTimer-PreviousCount_SignalDetect)>10)
  {
    PreviousCount_SignalDetect = cntMillisecondTimer;

    if (PtrDSP_HPIA[0] != NULL)
    {
      float dBLevel[256];

      for (int cntChannel=0; cntChannel<32; cntChannel++)
      {
        *PtrDSP_HPIA[0] = ModuleDSPMeterPPM+cntChannel*4;
        float LinearLevel = *((float *)PtrDSP_HPID[0]);
        *((float *)PtrDSP_HPID[0]) = 0;
        if (LinearLevel != 0)
        {
          dBLevel[cntChannel] = 20*log10(LinearLevel/2147483647);
        }
        else
        {
          dBLevel[cntChannel] = -2000;
        }
      }
      for (int cntChannel=0; cntChannel<32; cntChannel++)
      {
        *PtrDSP_HPIA[1] = ModuleDSPMeterPPM+cntChannel*4;
        float LinearLevel = *((float *)PtrDSP_HPID[1]);
        *((float *)PtrDSP_HPID[1]) = 0;
        if (LinearLevel != 0)
        {
          dBLevel[32+cntChannel] = 20*log10(LinearLevel/2147483647);
        }
        else
        {
          dBLevel[32+cntChannel] = -2000;
        }
      }

      for (int cntModule=FirstModuleOnSurface; cntModule<128; cntModule++)
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
        //TransmitBuffer[cntTransmitBuffer++] = 0;

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

bool ProgramEEPROM(int fd)
{
  struct
  {
    unsigned char SubClass;
    unsigned char BaseClass;
    unsigned char SubSysByte0;
    unsigned char SubSysByte1;
    unsigned char SubSysByte2;
    unsigned char SubSysByte3;
    unsigned char GPIOSelect;
    unsigned char RSVD_0;
    unsigned char RSVD_1;
    unsigned char MiscCtrlByte0;
    unsigned char MiscCtrlByte1;
    unsigned char Diagnostic;
    unsigned char HPI_ImpByte0;
    unsigned char RSVD_2;
    unsigned char HPI_DWByte0;
    unsigned char RSVD_3;
    unsigned char RSVD[16];
  } EEpromRegisters;

  EEpromRegisters.SubClass = 0x80;
  EEpromRegisters.BaseClass = 0x06;
  EEpromRegisters.SubSysByte0 = 0x00;
  EEpromRegisters.SubSysByte1 = 0x00;
  EEpromRegisters.SubSysByte2 = 0x00;
  EEpromRegisters.SubSysByte3 = 0x00;
  EEpromRegisters.GPIOSelect = 0x3C;
  EEpromRegisters.RSVD_0 = 0x00;
  EEpromRegisters.RSVD_1 = 0x00;
  EEpromRegisters.MiscCtrlByte0 = 0x27;
  EEpromRegisters.MiscCtrlByte1 = 0x00;
  EEpromRegisters.Diagnostic = 0x00;
  EEpromRegisters.HPI_ImpByte0 = 0x0F; //(Axum-RACK-DSP is 0x0F)
  EEpromRegisters.RSVD_2 = 0x00;
  EEpromRegisters.HPI_DWByte0 = 0x0F;
  EEpromRegisters.RSVD_3 = 0x00;

  pci2040_ioctl_linux pci2040_ioctl_message;
  PCI2040_WRITE_REG  reg;

  reg.Regoff = ENUM_GPBOUTDATA_OFFSET;
  //Clock High & Data High
  reg.DataVal = 0x03;

  pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
  pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
  pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
  ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
  delay_us(5);

  ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);

  //ByteWrite to EEPROM
  for (unsigned char cntByte=0; cntByte<32; cntByte++)
  {
    //StartCondition (Data goes Low (bit 1) & Clock stays high (bit 0))
    reg.DataVal &= 0xFD;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(5);

    //I2C Address
    for (int cntBit=0; cntBit<8; cntBit++)
    {
      unsigned char Mask = 0x80>>cntBit;

      //Clock Low
      reg.DataVal &= 0xFE;
      pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
      pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
      pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
      ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
      delay_us(5);

      //Set I2C Address
      if (0xA0 & Mask)
        reg.DataVal |= 0x02;
      else
        reg.DataVal &= 0xFD;

      pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
      pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
      pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
      ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
      delay_us(5);

      //Clock High
      reg.DataVal |= 0x01;
      pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
      pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
      pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
      ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
      delay_us(5);
    }
    //Acknowledge
    //Clock Low & Data sould be an input
    reg.DataVal &= 0xFE;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(5);

    //Change data for input
    reg.Regoff = ENUM_GPBDATADIR_OFFSET;
    reg.DataVal = 0x01;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(5);

    reg.Regoff = ENUM_GPBOUTDATA_OFFSET;

    //Clock High
    reg.DataVal |= 0x01;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(5);

    //Clock Low
    reg.DataVal &= 0xFE;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(5);

    //Change Data for Output
    reg.Regoff = ENUM_GPBDATADIR_OFFSET;
    reg.DataVal = 0x03;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(5);

    reg.Regoff = ENUM_GPBOUTDATA_OFFSET;
    //EEProm Address
    for (int cntBit=0; cntBit<8; cntBit++)
    {
      unsigned char Mask = 0x80>>cntBit;

      //Clock Low
      reg.DataVal &= 0xFE;
      pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
      pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
      pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
      ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
      delay_us(5);

      //Set EEPROM Address
      if (cntByte & Mask)
        reg.DataVal |= 0x02;
      else
        reg.DataVal &= 0xFD;
      pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
      pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
      pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
      ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
      delay_us(5);

      //Clock High
      reg.DataVal |= 0x01;
      pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
      pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
      pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
      ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
      delay_us(5);
    }
    //Acknowledge
    //Clock Low & Data sould be an input
    reg.DataVal &= 0xFE;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(5);

    //Change data for input
    reg.Regoff = ENUM_GPBDATADIR_OFFSET;
    reg.DataVal = 0x01;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(5);

    reg.Regoff = ENUM_GPBOUTDATA_OFFSET;
    //Clock High
    reg.DataVal |= 0x01;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(5);

    //Clock Low
    reg.DataVal &= 0xFE;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(5);

    //Change Data for Output
    reg.Regoff = ENUM_GPBDATADIR_OFFSET;
    reg.DataVal = 0x03;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(5);

    reg.Regoff = ENUM_GPBOUTDATA_OFFSET;
    //EEProm Data
    for (int cntBit=0; cntBit<8; cntBit++)
    {
      unsigned char Mask = 0x80>>cntBit;

      //Clock Low
      reg.DataVal &= 0xFE;
      pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
      pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
      pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
      ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
      delay_us(5);

      //Set EEPROM Data
      if (((unsigned char *)&EEpromRegisters.SubClass)[cntByte] & Mask)
        reg.DataVal |= 0x02;
      else
        reg.DataVal &= 0xFD;
      pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
      pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
      pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
      ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
      delay_us(5);

      //Clock High
      reg.DataVal |= 0x01;
      pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
      pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
      pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
      ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
      delay_us(5);
    }
    //Acknowledge
    //Clock Low & Data sould be an input
    reg.DataVal &= 0xFE;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(5);

    //Change data for input
    reg.Regoff = ENUM_GPBDATADIR_OFFSET;
    reg.DataVal = 0x01;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(5);

    reg.Regoff = ENUM_GPBOUTDATA_OFFSET;
    //Clock High
    reg.DataVal |= 0x01;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(5);

    //Clock Low
    reg.DataVal &= 0xFE;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(5);

    //Change Data for Output
    reg.Regoff = ENUM_GPBDATADIR_OFFSET;
    reg.DataVal = 0x03;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(5);

    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(5);

    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(5);

    reg.Regoff = ENUM_GPBOUTDATA_OFFSET;
    //Stopbit

    //Data Low
    reg.DataVal &= 0xFD;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(5);

    //Data High
    reg.DataVal |= 0x02;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(5);


    //Clock High
    reg.DataVal |= 0x01;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(5);
  }
  return true;
}

float CalculateEQ(float *Coefficients, float Gain, int Frequency, float Bandwidth, float Slope, FilterType Type)
{
  //TODO: Add your source code here
  // [0] = CoefficientZero0;
  // [1] = CoefficientZero1;
  // [2] = CoefficientZero2;
  // [3] = CoefficientPole0;
  // [4] = CoefficientPole1;
  // [5] = CoefficientPole2;
  double a0=1, a1=0 ,a2=0;    //<- Zero coefficients
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

void SetDSPCard_Interpolation()
{
  float AdjustedOffset = (0.002*48000)/AxumData.Samplerate;
  float SmoothFactor = (1-AdjustedOffset);

  float Value;

  //DSP1
  *PtrDSP_HPIA[0] = ModuleDSPSmoothFactor;
  *((float *)PtrDSP_HPID[0]) = SmoothFactor;
  Value = *((float *)PtrDSP_HPID[0]);
  printf("SmoothFactor[0] %g\n", Value);

  AdjustedOffset = (0.1*48000)/AxumData.Samplerate;
  float PPMReleaseFactor = (1-AdjustedOffset);
  *PtrDSP_HPIA[0] = ModuleDSPPPMReleaseFactor;
  *((float *)PtrDSP_HPID[0]) = PPMReleaseFactor;
  Value = *((float *)PtrDSP_HPID[0]);
  printf("PPMReleaseFactor[0] %g\n", Value);

  AdjustedOffset = (0.00019186*48000)/AxumData.Samplerate;
  float VUReleaseFactor = (1-AdjustedOffset);
  *PtrDSP_HPIA[0] = ModuleDSPVUReleaseFactor;
  *((float *)PtrDSP_HPID[0]) = VUReleaseFactor;
  Value = *((float *)PtrDSP_HPID[0]);
  printf("VUReleaseFactor[0] %g\n", Value);

  AdjustedOffset = (0.00043891*48000)/AxumData.Samplerate;
  float RMSReleaseFactor = (1-AdjustedOffset);
  *PtrDSP_HPIA[0] = ModuleDSPRMSReleaseFactor;
  *((float *)PtrDSP_HPID[0]) = RMSReleaseFactor;
  Value = *((float *)PtrDSP_HPID[0]);
  printf("RMSReleaseFactor[0] %g\n", Value);

  //DSP2
  *PtrDSP_HPIA[1] = ModuleDSPSmoothFactor;
  *((float *)PtrDSP_HPID[1]) = SmoothFactor;
  Value = *((float *)PtrDSP_HPID[1]);
  printf("SmoothFactor[1] %g\n", Value);

  *PtrDSP_HPIA[1] = ModuleDSPPPMReleaseFactor;
  *((float *)PtrDSP_HPID[1]) = PPMReleaseFactor;
  Value = *((float *)PtrDSP_HPID[1]);
  printf("PPMReleaseFactor[1] %g\n", Value);

  *PtrDSP_HPIA[1] = ModuleDSPVUReleaseFactor;
  *((float *)PtrDSP_HPID[1]) = VUReleaseFactor;
  Value = *((float *)PtrDSP_HPID[1]);
  printf("VUReleaseFactor[1] %g\n", Value);

  *PtrDSP_HPIA[1] = ModuleDSPRMSReleaseFactor;
  *((float *)PtrDSP_HPID[1]) = RMSReleaseFactor;
  Value = *((float *)PtrDSP_HPID[1]);
  printf("RMSReleaseFactor[1] %g\n", Value);


  AdjustedOffset = (0.002*48000)/AxumData.Samplerate;
  SmoothFactor = (1-(AdjustedOffset*4)); //*4 for interpolation
  *PtrDSP_HPIA[2] = SummingDSPSmoothFactor;
  *((float *)PtrDSP_HPID[2]) = SmoothFactor;
  SmoothFactor = *((float *)PtrDSP_HPID[2]);
  printf("SmoothFactor[2] %g\n", SmoothFactor);

  AdjustedOffset = (0.001*48000)/AxumData.Samplerate;
  VUReleaseFactor = (1-AdjustedOffset);
  *PtrDSP_HPIA[2] = SummingDSPVUReleaseFactor;
  *((float *)PtrDSP_HPID[2]) = VUReleaseFactor;
  Value = *((float *)PtrDSP_HPID[2]);
  printf("VUReleaseFactor[2] %g\n", Value);

//  float PhaseRelease = ((0.0002*AxumData.Samplerate)/48000);
  float PhaseRelease = ((0.0002*48000)/AxumData.Samplerate);
  *PtrDSP_HPIA[2] = SummingDSPPhaseRelease;
  *((float *)PtrDSP_HPID[2]) = PhaseRelease;
  Value = *((float *)PtrDSP_HPID[2]);
  printf("PhaseRelease[2] %g\n", Value);
}

void SetDSPCard_EQ(unsigned int DSPCardChannelNr, unsigned char BandNr)
{
  float Coefs[6];
  float a0 = 1;
  float a1 = 0;
  float a2 = 0;
  float b1 = 0;
  float b2 = 0;
  unsigned char DSPCardNr = (DSPCardChannelNr/64);
  unsigned char DSPNr = (DSPCardChannelNr%64)/32;
  unsigned char DSPChannelNr = DSPCardChannelNr%32;

  if (DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr].EQBand[BandNr].On)
  {
    float           Level               = DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr].EQBand[BandNr].Level;
    unsigned int    Frequency           = DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr].EQBand[BandNr].Frequency;
    float           Bandwidth           = DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr].EQBand[BandNr].Bandwidth;
    float           Slope               = DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr].EQBand[BandNr].Slope;
    FilterType      Type                = DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr].EQBand[BandNr].Type;

    CalculateEQ(Coefs, Level, Frequency, Bandwidth, Slope, Type);

    a0 = Coefs[0]/Coefs[3];
    a1 = Coefs[1]/Coefs[3];
    a2 = Coefs[2]/Coefs[3];
    b1 = Coefs[4]/Coefs[3];
    b2 = Coefs[5]/Coefs[3];

    //printf("%f | %f | %f | %f | %f | %f\n", Coefs[0], Coefs[1], Coefs[2], Coefs[3], Coefs[4], Coefs[5]);
  }
//  printf("DSPCardNr: %d, DSPNr: %d, DSPChannelNr: %d\n", DSPCardNr, DSPNr, DSPChannelNr);

  //MuteX implementation?
  if (DSPCardNr == 0)
  {
    *PtrDSP_HPIA[DSPNr] = ModuleDSPEQCoefficients+(((DSPChannelNr*5)+(BandNr*32*5))*4);
    *((float *)PtrDSP_HPIDAutoIncrement[DSPNr]) = -b1;
    *((float *)PtrDSP_HPIDAutoIncrement[DSPNr]) = -b2;
    *((float *)PtrDSP_HPIDAutoIncrement[DSPNr]) = a0;
    *((float *)PtrDSP_HPIDAutoIncrement[DSPNr]) = a1;
    *((float *)PtrDSP_HPIDAutoIncrement[DSPNr]) = a2;
  }
}

void SetDSPCard_ChannelProcessing(unsigned int DSPCardChannelNr)
{
  unsigned char DSPCardNr = (DSPCardChannelNr/64);
  unsigned char DSPNr = (DSPCardChannelNr%64)/32;
  unsigned char DSPChannelNr = DSPCardChannelNr%32;

  if (DSPCardNr == 0)
  {
    //Routing from (0: Gain input is default '0'->McASPA)

    //Routing from (1: EQ input is '2'->Gain output or '1'->McASPB)
    *PtrDSP_HPIA[DSPNr] = ModuleDSPRoutingFrom+((1*32*4)+(DSPChannelNr*4));
    if (DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr].Insert)
    {
      *((int *)PtrDSP_HPID[DSPNr]) = 1;
    }
    else
    {
      *((int *)PtrDSP_HPID[DSPNr]) = 2;
    }

    //Routing from (3: McASPA input (insert out) is '5'->Level output)
    *PtrDSP_HPIA[DSPNr] = ModuleDSPRoutingFrom+((3*32*4)+(DSPChannelNr*4));
    *((int *)PtrDSP_HPID[DSPNr]) = 2;

    //Routing from (3: McASPA input (insert out) is '2'->Gain output)
//      *PtrDSP_HPIA[DSPNr] = ModuleDSPRoutingFrom+((3*32*4)+(DSPChannelNr*4));
//      *((int *)PtrDSP_HPID[DSPNr]) = 2;

    //Routing from (5: level meter input is '0'->McASPA  or '1'->McASPB)
    *PtrDSP_HPIA[DSPNr] = ModuleDSPRoutingFrom+((5*32*4)+(DSPChannelNr*4));
    if (DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr].Insert)
    {
      *((int *)PtrDSP_HPID[DSPNr]) = 1;
    }
    else
    {
      *((int *)PtrDSP_HPID[DSPNr]) = 0;
    }

    //Routing from (6: level input is '1'->McASPB (insert input)
    *PtrDSP_HPIA[DSPNr] = ModuleDSPRoutingFrom+((6*32*4)+(DSPChannelNr*4));
    *((int *)PtrDSP_HPID[DSPNr]) = 1;

    //Gain section
    float factor = pow10(DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr].Gain/20);
    if (DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr].PhaseReverse)
    {
      factor *= -1;
    }
    //MuteX implementation?
    *PtrDSP_HPIA[DSPNr] = ModuleDSPUpdate_InputGainFactor+(DSPChannelNr*4);
    *((float *)PtrDSP_HPID[DSPNr]) = factor;

    //Filter section
    float Coefs[6];
    float a0 = 1;
    float a1 = 0;
    float a2 = 0;
    float b1 = 0;
    float b2 = 0;
    if (DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr].Filter.On)
    {
      float           Level               = DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr].Filter.Level;
      unsigned int    Frequency           = DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr].Filter.Frequency;
      float           Bandwidth           = DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr].Filter.Bandwidth;
      float           Slope                   = DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr].Filter.Slope;
      FilterType      Type                    = DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr].Filter.Type;

      CalculateEQ(Coefs, Level, Frequency, Bandwidth, Slope, Type);

      a0 = Coefs[0]/Coefs[3];
      a1 = Coefs[1]/Coefs[3];
      a2 = Coefs[2]/Coefs[3];
      b1 = Coefs[4]/Coefs[3];
      b2 = Coefs[5]/Coefs[3];
    }
    //MuteX implementation?
    *PtrDSP_HPIA[DSPNr] = ModuleDSPFilterCoefficients+((DSPChannelNr*5)*4);
    *((float *)PtrDSP_HPIDAutoIncrement[DSPNr]) = -b1;
    *((float *)PtrDSP_HPIDAutoIncrement[DSPNr]) = -b2;
    *((float *)PtrDSP_HPIDAutoIncrement[DSPNr]) = a0;
    *((float *)PtrDSP_HPIDAutoIncrement[DSPNr]) = a1;
    *((float *)PtrDSP_HPIDAutoIncrement[DSPNr]) = a2;

    for (int cntBand=0; cntBand<6; cntBand++)
    {
      SetDSPCard_EQ(DSPCardChannelNr, cntBand);
    }


    //MuteX implementation?
    float DynamicsProcessedFactor = 0;
    float DynamicsOriginalFactor = 1;
    if (DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr].Dynamics.On)
    {
      DynamicsProcessedFactor = (float)DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr].Dynamics.Percent/100;
      DynamicsOriginalFactor = 1-DynamicsProcessedFactor;
    }

    *PtrDSP_HPIA[DSPNr] = ModuleDSPDynamicsOriginalFactor+(DSPChannelNr*4);
    *((float *)PtrDSP_HPIDAutoIncrement[DSPNr]) = DynamicsOriginalFactor;

    *PtrDSP_HPIA[DSPNr] = ModuleDSPDynamicsProcessedFactor+(DSPChannelNr*4);
    *((float *)PtrDSP_HPIDAutoIncrement[DSPNr]) = DynamicsProcessedFactor;
  }
}

void SetDSPCard_BussLevels(unsigned int DSPCardChannelNr)
{
  unsigned char DSPCardNr = (DSPCardChannelNr/64);
  unsigned char DSPChannelNr = DSPCardChannelNr%64;

  if (DSPCardNr == 0)
  {
    for (int cntBuss=0; cntBuss<32; cntBuss++)
    {
      float factor = 0;

      if (DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr].Buss[cntBuss].On)
      {
        if (DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr].Buss[cntBuss].Level<-120)
        {
          factor = 0;
        }
        else
        {
          factor = pow10(DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr].Buss[cntBuss].Level/20);
        }
      }

      //MuteX implementation?
      *PtrDSP_HPIA[2] = SummingDSPUpdate_MatrixFactor+((cntBuss+(DSPChannelNr*32))*4);
      *((float *)PtrDSP_HPID[2]) = factor;
    }
  }
}

void SetDSPCard_MixMinus(unsigned int DSPCardChannelNr)
{
  unsigned char DSPCardNr = (DSPCardChannelNr/64);
  unsigned char DSPChannelNr = DSPCardChannelNr%64;

  printf("CardNr: %d, ChannelNr: %d used buss: %d\n", DSPCardNr, DSPChannelNr, DSPCardData[DSPCardNr].MixMinusData[DSPCardChannelNr].Buss);

  if (DSPCardNr == 0)
  {
    //MuteX implementation?
    *PtrDSP_HPIA[2] = SummingDSPSelectedMixMinusBuss+(DSPChannelNr*4);
    *((int *)PtrDSP_HPID[2]) = DSPCardData[DSPCardNr].MixMinusData[DSPCardChannelNr].Buss;
  }
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
  unsigned int FirstDSPChannelNr = ModuleNr<<1;
  unsigned char DSPCardNr = (FirstDSPChannelNr/64);
  unsigned char DSPCardChannelNr = FirstDSPChannelNr%64;

  for (int cntChannel=0; cntChannel<2; cntChannel++)
  {
    DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].EQBand[BandNr].On = AxumData.ModuleData[ModuleNr].EQBand[BandNr].On;
    DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].EQBand[BandNr].Level = AxumData.ModuleData[ModuleNr].EQBand[BandNr].Level;
    DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].EQBand[BandNr].Frequency = AxumData.ModuleData[ModuleNr].EQBand[BandNr].Frequency;
    DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].EQBand[BandNr].Bandwidth = AxumData.ModuleData[ModuleNr].EQBand[BandNr].Bandwidth;
    DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].EQBand[BandNr].Slope  = AxumData.ModuleData[ModuleNr].EQBand[BandNr].Slope;
    DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].EQBand[BandNr].Type = AxumData.ModuleData[ModuleNr].EQBand[BandNr].Type;
  }

  for (int cntChannel=0; cntChannel<2; cntChannel++)
  {
    SetDSPCard_EQ(DSPCardChannelNr+cntChannel, BandNr);
  }
}

void SetAxum_ModuleProcessing(unsigned int ModuleNr)
{
  unsigned int FirstDSPChannelNr = ModuleNr<<1;
  unsigned char DSPCardNr = (FirstDSPChannelNr/64);
  unsigned char DSPCardChannelNr = FirstDSPChannelNr%64;

  for (int cntChannel=0; cntChannel<2; cntChannel++)
  {
    DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Insert = AxumData.ModuleData[ModuleNr].Insert;
    DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Gain = AxumData.ModuleData[ModuleNr].Gain;

    DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Filter.On = AxumData.ModuleData[ModuleNr].Filter.On;
    DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Filter.Level = AxumData.ModuleData[ModuleNr].Filter.Level;
    DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Filter.Frequency = AxumData.ModuleData[ModuleNr].Filter.Frequency;
    DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Filter.Bandwidth = AxumData.ModuleData[ModuleNr].Filter.Bandwidth;
    DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Filter.Slope = AxumData.ModuleData[ModuleNr].Filter.Slope;
    DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Filter.Type = AxumData.ModuleData[ModuleNr].Filter.Type;

    DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Dynamics.Percent = AxumData.ModuleData[ModuleNr].Dynamics;
    DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Dynamics.On = AxumData.ModuleData[ModuleNr].DynamicsOn;
  }
  DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+1].PhaseReverse = AxumData.ModuleData[ModuleNr].PhaseReverse;


  for (int cntChannel=0; cntChannel<2; cntChannel++)
  {
    SetDSPCard_ChannelProcessing(DSPCardChannelNr+cntChannel);
  }
}

void SetAxum_BussLevels(unsigned int ModuleNr)
{
  unsigned int FirstDSPChannelNr = ModuleNr<<1;
  unsigned char DSPCardNr = (FirstDSPChannelNr/64);
  unsigned char DSPCardChannelNr = FirstDSPChannelNr%64;
  float PanoramadB[2] = {-200, -200};

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
      DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+0].Level = -140; //0 dB?
      DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+1].Level = -140; //0 dB?
      DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+0].On = 0;
      DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+1].On = 0;

      if (AxumData.ModuleData[ModuleNr].Buss[cntBuss].Active)
      {
        if (AxumData.ModuleData[ModuleNr].Mono)
        {
          DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+0].On = AxumData.ModuleData[ModuleNr].Buss[cntBuss].On;
          DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+1].On = AxumData.ModuleData[ModuleNr].Buss[cntBuss].On;
          DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+0].Level = AxumData.ModuleData[ModuleNr].Buss[cntBuss].Level;
          DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+1].Level = AxumData.ModuleData[ModuleNr].Buss[cntBuss].Level;
          DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+0].Level += BussBalancedB[0];
          DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+1].Level += BussBalancedB[1];
          DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+0].Level += -6;
          DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+1].Level += -6;

          if ((!AxumData.ModuleData[ModuleNr].Buss[cntBuss].PreModuleLevel) && (!AxumData.BussMasterData[cntBuss].PreModuleLevel))
          {
            DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+0].Level += AxumData.ModuleData[ModuleNr].FaderLevel;
            DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+1].Level += AxumData.ModuleData[ModuleNr].FaderLevel;
          }
          if (!AxumData.BussMasterData[cntBuss].PreModuleBalance)
          {
            DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+0].Level += PanoramadB[0];
            DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+1].Level += PanoramadB[1];
          }
          if (!AxumData.BussMasterData[cntBuss].PreModuleOn)
          {
            if ((!AxumData.ModuleData[ModuleNr].On) || (AxumData.ModuleData[ModuleNr].Cough))
            {
              DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+0].On = 0;
              DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+1].On = 0;
            }
          }
        }
        else
        { //Stereo
          DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+cntChannel].On = AxumData.ModuleData[ModuleNr].Buss[cntBuss].On;
          DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+cntChannel].Level = AxumData.ModuleData[ModuleNr].Buss[cntBuss].Level;
          DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+cntChannel].Level += BussBalancedB[cntChannel];

          if ((!AxumData.ModuleData[ModuleNr].Buss[cntBuss].PreModuleLevel) && (!AxumData.BussMasterData[cntBuss].PreModuleLevel))
          {
            DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+cntChannel].Level += AxumData.ModuleData[ModuleNr].FaderLevel;
          }

          if (!AxumData.BussMasterData[cntBuss].PreModuleBalance)
          {
            DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+cntChannel].Level += PanoramadB[cntChannel];
          }

          if (!AxumData.BussMasterData[cntBuss].PreModuleOn)
          {
            if ((!AxumData.ModuleData[ModuleNr].On) || (AxumData.ModuleData[ModuleNr].Cough))
            {
              DSPCardData[DSPCardNr].ChannelData[DSPCardChannelNr+cntChannel].Buss[(cntBuss*2)+cntChannel].On = 0;
            }
          }
        }
      }
    }
  }

  for (int cntChannel=0; cntChannel<2; cntChannel++)
  {
    SetDSPCard_BussLevels(DSPCardChannelNr+cntChannel);
  }
}

void SetAxum_ModuleSource(unsigned int ModuleNr)
{
  unsigned int FirstDSPChannelNr = ModuleNr<<1;
//    unsigned char DSPCardNr = FirstDSPChannelNr/64;
  unsigned char DSPCardNr = (FirstDSPChannelNr/64);
  //unsigned char DSPNr = (FirstDSPChannelNr%64)/32;
  //unsigned char DSPChannelNr = FirstDSPChannelNr%32;

  unsigned int ToChannelNr = 480+(DSPCardNr*5*32)+FirstDSPChannelNr%64;

//    printf("Card:%d, DSPNr:%d, DSPChannelNr:%d ToCh:%d Source:%d,%d\n", DSPCardNr, DSPNr, DSPChannelNr, ToChannelNr, AxumData.SourceData[AxumData.ModuleData[ModuleNr].Source-1].Input[0], AxumData.SourceData[AxumData.ModuleData[ModuleNr].Source-1].Input[1]);

  if (AxumData.ModuleData[ModuleNr].Source == 0)
  {
    for (int cntChannel=0; cntChannel<2; cntChannel++)
    {   //Turn off
      SetBackplane_Source(0, ToChannelNr+cntChannel);
    }
  }
  else
  {
    int Input1 = 0;
    int Input2 = 0;
    //Get slot number from MambaNet Address
    for (int cntSlot=0; cntSlot<15; cntSlot++)
    {
      if (AxumData.RackOrganization[cntSlot] == AxumData.SourceData[AxumData.ModuleData[ModuleNr].Source-1].InputData[0].MambaNetAddress)
      {
        Input1 = cntSlot*32;
      }
      if (AxumData.RackOrganization[cntSlot] == AxumData.SourceData[AxumData.ModuleData[ModuleNr].Source-1].InputData[1].MambaNetAddress)
      {
        Input2 = cntSlot*32;
      }
    }
    for (int cntSlot=15; cntSlot<19; cntSlot++)
    {
      if (AxumData.RackOrganization[cntSlot] == AxumData.SourceData[AxumData.ModuleData[ModuleNr].Source-1].InputData[0].MambaNetAddress)
      {
        Input1 = 480+((cntSlot-15)*32*5);
      }
      if (AxumData.RackOrganization[cntSlot] == AxumData.SourceData[AxumData.ModuleData[ModuleNr].Source-1].InputData[1].MambaNetAddress)
      {
        Input2 = 480+((cntSlot-15)*32*5);
      }
    }
    for (int cntSlot=21; cntSlot<42; cntSlot++)
    {
      if (AxumData.RackOrganization[cntSlot] == AxumData.SourceData[AxumData.ModuleData[ModuleNr].Source-1].InputData[0].MambaNetAddress)
      {
        Input1 = 1120+((cntSlot-21)*32);
      }
      if (AxumData.RackOrganization[cntSlot] == AxumData.SourceData[AxumData.ModuleData[ModuleNr].Source-1].InputData[1].MambaNetAddress)
      {
        Input2 = 1120+((cntSlot-21)*32);
      }
    }
    Input1 += AxumData.SourceData[AxumData.ModuleData[ModuleNr].Source-1].InputData[0].SubChannel;
    Input2 += AxumData.SourceData[AxumData.ModuleData[ModuleNr].Source-1].InputData[1].SubChannel;

    printf("In1:%d, In2:%d\n", Input1, Input2);
    SetBackplane_Source(Input1+1, ToChannelNr+0);
    SetBackplane_Source(Input2+1, ToChannelNr+1);
  }
}

void SetAxum_ExternSources(unsigned int MonitorBussPerFourNr)
{
  unsigned char DSPCardNr = MonitorBussPerFourNr;
//    unsigned char DSPCardNr = 4-MonitorBussPerFourNr;

  //four stereo busses
  for (int cntExt=0; cntExt<4; cntExt++)
  {
    unsigned int ToChannelNr = 480+(DSPCardNr*5*32)+128+(cntExt*2);

    if (AxumData.ExternSource[MonitorBussPerFourNr].Ext[cntExt] == 0)
    {
      for (int cntChannel=0; cntChannel<2; cntChannel++)
      {   //Turn off
        SetBackplane_Source(0, ToChannelNr+cntChannel);
      }
    }
    else
    {
      int Input1 = 0;
      int Input2 = 0;

      //Get slot number from MambaNet Address
      for (int cntSlot=0; cntSlot<15; cntSlot++)
      {
        if (AxumData.RackOrganization[cntSlot] == AxumData.SourceData[AxumData.ExternSource[MonitorBussPerFourNr].Ext[cntExt]-1].InputData[0].MambaNetAddress)
        {
          Input1 = cntSlot*32;
        }
        if (AxumData.RackOrganization[cntSlot] == AxumData.SourceData[AxumData.ExternSource[MonitorBussPerFourNr].Ext[cntExt]-1].InputData[1].MambaNetAddress)
        {
          Input2 = cntSlot*32;
        }
      }
      for (int cntSlot=15; cntSlot<19; cntSlot++)
      {
        if (AxumData.RackOrganization[cntSlot] == AxumData.SourceData[AxumData.ExternSource[MonitorBussPerFourNr].Ext[cntExt]-1].InputData[0].MambaNetAddress)
        {
          Input1 = 480+((cntSlot-15)*32*5);
        }
        if (AxumData.RackOrganization[cntSlot] == AxumData.SourceData[AxumData.ExternSource[MonitorBussPerFourNr].Ext[cntExt]-1].InputData[1].MambaNetAddress)
        {
          Input2 = 480+((cntSlot-15)*32*5);
        }
      }
      for (int cntSlot=21; cntSlot<42; cntSlot++)
      {
        if (AxumData.RackOrganization[cntSlot] == AxumData.SourceData[AxumData.ExternSource[MonitorBussPerFourNr].Ext[cntExt]-1].InputData[0].MambaNetAddress)
        {
          Input1 = 1120+((cntSlot-21)*32);
        }
        if (AxumData.RackOrganization[cntSlot] == AxumData.SourceData[AxumData.ExternSource[MonitorBussPerFourNr].Ext[cntExt]-1].InputData[1].MambaNetAddress)
        {
          Input2 = 1120+((cntSlot-21)*32);
        }
      }
      Input1 += AxumData.SourceData[AxumData.ExternSource[MonitorBussPerFourNr].Ext[cntExt]-1].InputData[0].SubChannel;
      Input2 += AxumData.SourceData[AxumData.ExternSource[MonitorBussPerFourNr].Ext[cntExt]-1].InputData[1].SubChannel;

      printf("In1:%d, In2:%d -> %d, %d\n", Input1, Input2, ToChannelNr+0, ToChannelNr+1);
      SetBackplane_Source(Input1+1, ToChannelNr+0);
      SetBackplane_Source(Input2+1, ToChannelNr+1);
    }
  }
}

void SetAxum_ModuleMixMinus(unsigned int ModuleNr)
{
  unsigned int FirstDSPChannelNr = ModuleNr<<1;
//   unsigned char DSPCardNr = FirstDSPChannelNr/64;
  unsigned char DSPCardNr = (FirstDSPChannelNr/64);
  unsigned char DSPCardChannelNr = FirstDSPChannelNr%64;

  int cntDestination=0;
  int DestinationNr = -1;
  while (cntDestination<1280)
  {
    if (AxumData.DestinationData[cntDestination].MixMinusSource == AxumData.ModuleData[ModuleNr].Source)
    {
      DestinationNr = cntDestination;

      if (AxumData.ModuleData[ModuleNr].Source != 0)
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

          DSPCardData[DSPCardNr].MixMinusData[DSPCardChannelNr+0].Buss = (BussToUse<<1)+0;
          DSPCardData[DSPCardNr].MixMinusData[DSPCardChannelNr+1].Buss = (BussToUse<<1)+1;

          for (int cntChannel=0; cntChannel<2; cntChannel++)
          {
            SetDSPCard_MixMinus(DSPCardChannelNr+cntChannel);
          }

          AxumData.DestinationData[DestinationNr].MixMinusActive = 1;
          SetAxum_DestinationSource(DestinationNr);
        }
        else
        {
          printf("Use default src\n");

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
  unsigned int FirstDSPChannelNr = ModuleNr<<1;
//    unsigned char DSPCardNr = FirstDSPChannelNr/64;
  unsigned char DSPCardNr = (FirstDSPChannelNr/64);
  //unsigned char DSPNr = (FirstDSPChannelNr%64)/32;
  //unsigned char DSPChannelNr = FirstDSPChannelNr%32;

  unsigned int ToChannelNr = 480+64+(DSPCardNr*5*32)+FirstDSPChannelNr%64;

//    printf("Card:%d, DSPNr:%d, DSPChannelNr:%d ToCh:%d Source:%d,%d\n", DSPCardNr, DSPNr, DSPChannelNr, ToChannelNr, AxumData.SourceData[AxumData.ModuleData[ModuleNr].Source-1].Input[0], AxumData.SourceData[AxumData.ModuleData[ModuleNr].Source-1].Input[1]);

  if (AxumData.ModuleData[ModuleNr].InsertSource == 0)
  {
    for (int cntChannel=0; cntChannel<2; cntChannel++)
    {   //Turn off
      SetBackplane_Source(0, ToChannelNr+cntChannel);
    }
  }
  else
  {
    int Input1 = 0;
    int Input2 = 0;
    //Get slot number from MambaNet Address
    for (int cntSlot=0; cntSlot<42; cntSlot++)
    {
      if (AxumData.RackOrganization[cntSlot] == AxumData.SourceData[AxumData.ModuleData[ModuleNr].InsertSource-1].InputData[0].MambaNetAddress)
      {
        Input1 = cntSlot*32;
      }
      if (AxumData.RackOrganization[cntSlot] == AxumData.SourceData[AxumData.ModuleData[ModuleNr].InsertSource-1].InputData[1].MambaNetAddress)
      {
        Input2 = cntSlot*32;
      }
    }
    Input1 += AxumData.SourceData[AxumData.ModuleData[ModuleNr].InsertSource-1].InputData[0].SubChannel;
    Input2 += AxumData.SourceData[AxumData.ModuleData[ModuleNr].InsertSource-1].InputData[1].SubChannel;

    printf("Insert-In1:%d, Insert-In2:%d\n", Input1, Input2);
    SetBackplane_Source(Input1+1, ToChannelNr+0);
    SetBackplane_Source(Input2+1, ToChannelNr+1);
  }
}

void SetAxum_DestinationSource(unsigned int DestinationNr)
{
  int Output1 = -1;
  int Output2 = -1;
  int FromChannel1 = 0;
  int FromChannel2 = 0;

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
      unsigned int FirstDSPChannelNr = 545+(DSPCardNr*5*32);

      FromChannel1 = FirstDSPChannelNr+(MixMinusNr&0x1F);
      FromChannel2 = FirstDSPChannelNr+(MixMinusNr&0x1F);
    }
    else if (AxumData.DestinationData[DestinationNr].Source == 0)
    {
      printf("src: None\n");
      FromChannel1 = 0;
      FromChannel2 = 0;
    }
    else if (AxumData.DestinationData[DestinationNr].Source<17)
    {
      int BussNr = AxumData.DestinationData[DestinationNr].Source-1;
      printf("src: Total buss %d\n", BussNr+1);
      FromChannel1 = 1793+(BussNr*2)+0;
      FromChannel2 = 1793+(BussNr*2)+1;
    }
    else if (AxumData.DestinationData[DestinationNr].Source<33)
    {
      int BussNr = AxumData.DestinationData[DestinationNr].Source-17;
      printf("src: Monitor buss %d\n", BussNr+1);

      unsigned char DSPCardNr = (BussNr/4);

      unsigned int FirstDSPChannelNr = 577+(DSPCardNr*5*32);

      FromChannel1 = FirstDSPChannelNr+(BussNr*2)+0;
      FromChannel2 = FirstDSPChannelNr+(BussNr*2)+1;
    }
    else if (AxumData.DestinationData[DestinationNr].Source<161)
    {
      int ModuleNr = AxumData.DestinationData[DestinationNr].Source-33;
      printf("src: Module insert out %d\n", ModuleNr+1);

      unsigned char DSPCardNr = (ModuleNr/32);
      //unsigned char DSPNr = (ModuleNr%32)/16;

      unsigned int FirstDSPChannelNr = 481+(DSPCardNr*5*32);

      FromChannel1 = FirstDSPChannelNr+(ModuleNr*2)+0;
      FromChannel2 = FirstDSPChannelNr+(ModuleNr*2)+1;
    }
    else
    { //161
      int SourceNr = AxumData.DestinationData[DestinationNr].Source-161;
      printf("src: Source %d\n", SourceNr);

      //Get slot number from MambaNet Address
      FromChannel1 = 0;
      FromChannel2 = 0;
      for (int cntSlot=0; cntSlot<15; cntSlot++)
      {
        if (AxumData.RackOrganization[cntSlot] == AxumData.SourceData[SourceNr].InputData[0].MambaNetAddress)
        {
          FromChannel1 = cntSlot*32;
        }
        if (AxumData.RackOrganization[cntSlot] == AxumData.SourceData[SourceNr].InputData[1].MambaNetAddress)
        {
          FromChannel2 = cntSlot*32;
        }
      }
      for (int cntSlot=15; cntSlot<19; cntSlot++)
      {
        if (AxumData.RackOrganization[cntSlot] == AxumData.SourceData[SourceNr].InputData[0].MambaNetAddress)
        {
          FromChannel1 = 480+((cntSlot-15)*32*5);
        }
        if (AxumData.RackOrganization[cntSlot] == AxumData.SourceData[SourceNr].InputData[1].MambaNetAddress)
        {
          FromChannel2 = 480+((cntSlot-15)*32*5);
        }
      }
      for (int cntSlot=21; cntSlot<42; cntSlot++)
      {
        if (AxumData.RackOrganization[cntSlot] == AxumData.SourceData[SourceNr].InputData[0].MambaNetAddress)
        {
          FromChannel1 = 1120+((cntSlot-21)*32);
        }
        if (AxumData.RackOrganization[cntSlot] == AxumData.SourceData[SourceNr].InputData[0].MambaNetAddress)
        {
          FromChannel2 = 1120+((cntSlot-21)*32);
        }
      }
      FromChannel1 += AxumData.SourceData[SourceNr].InputData[0].SubChannel;
      FromChannel2 += AxumData.SourceData[SourceNr].InputData[1].SubChannel;

      //Because 0 = mute, add one per channel
      FromChannel1 += 1;
      FromChannel2 += 1;
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
  int FromChannel1 = 0;
  int FromChannel2 = 0;

  if (AxumData.Talkback[TalkbackNr].Source == 0)
  {
    printf("Talkback src: None\n");
    FromChannel1 = 0;
    FromChannel2 = 0;
  }
  else if (AxumData.Talkback[TalkbackNr].Source<17)
  {
    int BussNr = AxumData.Talkback[TalkbackNr].Source-1;
    printf("Talkback src: Total buss %d\n", BussNr+1);
    FromChannel1 = 1793+(BussNr*2)+0;
    FromChannel2 = 1793+(BussNr*2)+1;
  }
  else if (AxumData.Talkback[TalkbackNr].Source<33)
  {
    int BussNr = AxumData.Talkback[TalkbackNr].Source-17;
    printf("Talkback src: Monitor buss %d\n", BussNr+1);

    unsigned char DSPCardNr = (BussNr/4);

    unsigned int FirstDSPChannelNr = 577+(DSPCardNr*5*32);

    FromChannel1 = FirstDSPChannelNr+(BussNr*2)+0;
    FromChannel2 = FirstDSPChannelNr+(BussNr*2)+1;
  }
  else if (AxumData.Talkback[TalkbackNr].Source<161)
  {
    int ModuleNr = AxumData.Talkback[TalkbackNr].Source-33;
    printf("Talkback src: Module insert out %d\n", ModuleNr+1);

    unsigned char DSPCardNr = (ModuleNr/32);
    //unsigned char DSPNr = (ModuleNr%32)/16;

    unsigned int FirstDSPChannelNr = 481+(DSPCardNr*5*32);

    FromChannel1 = FirstDSPChannelNr+(ModuleNr*2)+0;
    FromChannel2 = FirstDSPChannelNr+(ModuleNr*2)+1;
  }
  else
  { //161
    int SourceNr = AxumData.Talkback[TalkbackNr].Source-161;
    printf("Talkback src: Source %d\n", SourceNr);

    //Get slot number from MambaNet Address
    //Get slot number from MambaNet Address
    FromChannel1 = 0;
    FromChannel2 = 0;
    for (int cntSlot=0; cntSlot<15; cntSlot++)
    {
      if (AxumData.RackOrganization[cntSlot] == AxumData.SourceData[SourceNr].InputData[0].MambaNetAddress)
      {
        FromChannel1 = cntSlot*32;
      }
      if (AxumData.RackOrganization[cntSlot] == AxumData.SourceData[SourceNr].InputData[1].MambaNetAddress)
      {
        FromChannel2 = cntSlot*32;
      }
    }
    for (int cntSlot=15; cntSlot<19; cntSlot++)
    {
      if (AxumData.RackOrganization[cntSlot] == AxumData.SourceData[SourceNr].InputData[0].MambaNetAddress)
      {
        FromChannel1 = 480+((cntSlot-15)*32*5);
      }
      if (AxumData.RackOrganization[cntSlot] == AxumData.SourceData[SourceNr].InputData[1].MambaNetAddress)
      {
        FromChannel2 = 480+((cntSlot-15)*32*5);
      }
    }
    for (int cntSlot=21; cntSlot<42; cntSlot++)
    {
      if (AxumData.RackOrganization[cntSlot] == AxumData.SourceData[SourceNr].InputData[0].MambaNetAddress)
      {
        FromChannel1 = 1120+((cntSlot-21)*32);
      }
      if (AxumData.RackOrganization[cntSlot] == AxumData.SourceData[SourceNr].InputData[0].MambaNetAddress)
      {
        FromChannel2 = 1120+((cntSlot-21)*32);
      }
    }
    FromChannel1 += AxumData.SourceData[SourceNr].InputData[0].SubChannel;
    FromChannel2 += AxumData.SourceData[SourceNr].InputData[1].SubChannel;

    //Because 0 = mute, add one per channel
    FromChannel1 += 1;
    FromChannel2 += 1;
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



/*void SetModule_Switch(unsigned int SwitchNr, unsigned int ModuleNr, unsigned char State)
{
    int ObjectNr = 1040+(SwitchNr*4)+ModuleNr-FirstModuleOnSurface;
    unsigned char TransmitData[32];

    TransmitData[0] = (ObjectNr>>8)&0xFF;
    TransmitData[1] = ObjectNr&0xFF;
    TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
    TransmitData[3] = STATE_DATATYPE;
    TransmitData[4] = 1;
    TransmitData[5] = State&0xFF;

    SendMambaNetMessage(0x00000002, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
}

void SetCRM_Switch(unsigned int SwitchNr, unsigned char State)
{
    int ObjectNr = 1024+SwitchNr;
    unsigned char TransmitData[32];

    TransmitData[0] = (ObjectNr>>8)&0xFF;
    TransmitData[1] = ObjectNr&0xFF;
    TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
    TransmitData[3] = STATE_DATATYPE;
    TransmitData[4] = 1;
    TransmitData[5] = State&0xFF;

    SendMambaNetMessage(0x00000034, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
}

void SetCRM_LEDBar(char NrOfLEDs)
{
    int ObjectNr = 1080;
    unsigned char TransmitData[32];
    unsigned char Mask = 0x00;

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

    SendMambaNetMessage(0x00000034, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
}*/

void SetAxum_BussMasterLevels()
{
  for (int cntDSPCard=0; cntDSPCard<4; cntDSPCard++)
  {
    //stereo buss 1-16
    for (int cntBuss=0; cntBuss<16; cntBuss++)
    {
      DSPCardData[cntDSPCard].BussMasterData[(cntBuss*2)+0].Level = AxumData.BussMasterData[cntBuss].Level;
      DSPCardData[cntDSPCard].BussMasterData[(cntBuss*2)+1].Level = AxumData.BussMasterData[cntBuss].Level;
      DSPCardData[cntDSPCard].BussMasterData[(cntBuss*2)+0].On = AxumData.BussMasterData[cntBuss].On;
      DSPCardData[cntDSPCard].BussMasterData[(cntBuss*2)+1].On = AxumData.BussMasterData[cntBuss].On;
    }
  }
  SetDSP_BussMasterLevels();
}

void SetDSP_BussMasterLevels()
{
  for (int cntDSPCard=0; cntDSPCard<4; cntDSPCard++)
  {
    for (int cntBuss=0; cntBuss<32; cntBuss++)
    {
      float factor = pow10(DSPCardData[cntDSPCard].BussMasterData[cntBuss].Level/20);
      if (!DSPCardData[cntDSPCard].BussMasterData[cntBuss].On)
      {
        factor = 0;
      }

      if (cntDSPCard == 3)
      {
        //MuteX implementation?
        *PtrDSP_HPIA[2] = SummingDSPUpdate_MatrixFactor+((64*32)*4)+(cntBuss*4);
        *((float *)PtrDSP_HPID[2]) = factor;
      }
    }
  }
}

/*
void SetCRM()
{
    for (int cntSwitch=0; cntSwitch<16; cntSwitch++)
    {
        if (cntSwitch == AxumData.Control1Mode)
        {
            SetCRM_Switch(cntSwitch, 1);
        }
        else
        {
            SetCRM_Switch(cntSwitch, 0);
        }
    }

    for (int cntSwitch=0; cntSwitch<6; cntSwitch++)
    {
//        if (cntSwitch == AxumData.MasterControl1Mode)
//        {
//            SetCRM_Switch(16+cntSwitch, 1);
//        }
//        else
        {
            SetCRM_Switch(16+cntSwitch, 0);
        }
    }

    float *BussMasterLevel = NULL;
    switch (AxumData.MasterControl1Mode)
    {
        case 0:
        { //Prog master level
            BussMasterLevel = &AxumData.ProgMasterLevel;
        }
        break;
        case 1:
        { //Sub master level
            BussMasterLevel = &AxumData.SubMasterLevel;
        }
        break;
        case 2:
        { //CUE master level
            BussMasterLevel = &AxumData.CUEMasterLevel;
        }
        break;
        case 3:
        { //Comm master level
            BussMasterLevel = &AxumData.CommMasterLevel;
        }
        break;
        case 4:
        { //Aux 1 master level
            BussMasterLevel = &AxumData.AuxMasterLevel[0];
        }
        break;
        case 5:
        { //Aux 2 master level
            BussMasterLevel = &AxumData.AuxMasterLevel[1];
        }
        break;
        case 6:
        { //Aux 3 master level
            BussMasterLevel = &AxumData.AuxMasterLevel[2];
        }
        break;
        case 7:
        { //Aux 4 master level
            BussMasterLevel = &AxumData.AuxMasterLevel[3];
        }
        break;
        case 8:
        { //Aux 5 master level
            BussMasterLevel = &AxumData.AuxMasterLevel[4];
        }
        break;
        case 9:
        { //Aux 6 master level
            BussMasterLevel = &AxumData.AuxMasterLevel[5];
        }
        break;
        case 10:
        { //Aux 7 master level
            BussMasterLevel = &AxumData.AuxMasterLevel[6];
        }
        break;
        case 11:
        { //Aux 8 master level
            BussMasterLevel = &AxumData.AuxMasterLevel[7];
        }
        break;
        case 12:
        { //Aux 9 master level
            BussMasterLevel = &AxumData.AuxMasterLevel[8];
        }
        break;
        case 13:
        { //Aux 10 master level
            BussMasterLevel = &AxumData.AuxMasterLevel[9];
        }
        break;
        case 14:
        { //Aux 11 master level
            BussMasterLevel = &AxumData.AuxMasterLevel[10];
        }
        break;
        case 15:
        { //Aux 12 master level
            BussMasterLevel = &AxumData.AuxMasterLevel[11];
        }
        break;
    }

    if (BussMasterLevel != NULL)
    {
        SetCRM_LEDBar((*BussMasterLevel+140)/(150/7));
    }

    SetCRM_Switch(22, AxumData.Monitor[0].Prog);
    SetCRM_Switch(23, AxumData.Monitor[0].Sub);
    SetCRM_Switch(24, AxumData.Monitor[0].Aux[0]);
    SetCRM_Switch(25, AxumData.Monitor[0].Aux[1]);
    SetCRM_Switch(26, AxumData.Monitor[0].Aux[2]);
    SetCRM_Switch(27, AxumData.Monitor[0].Aux[3]);
    SetCRM_Switch(28, AxumData.Monitor[0].Comm);
    SetCRM_Switch(29, AxumData.Monitor[0].CUE);
    SetCRM_Switch(30, AxumData.Monitor[0].Ext[0]);
    SetCRM_Switch(31, AxumData.Monitor[0].Ext[1]);
    SetCRM_Switch(32, AxumData.Monitor[0].Ext[2]);
    SetCRM_Switch(33, AxumData.Monitor[0].Ext[3]);
    SetCRM_Switch(34, AxumData.Monitor[0].Dim);
    SetCRM_Switch(35, AxumData.Monitor[0].Mute);

    SetCRM_Switch(36, AxumData.Monitor[1].Prog);
    SetCRM_Switch(37, AxumData.Monitor[1].Sub);
    SetCRM_Switch(38, AxumData.Monitor[1].Aux[0]);
    SetCRM_Switch(39, AxumData.Monitor[1].Aux[1]);
    SetCRM_Switch(40, AxumData.Monitor[1].Aux[2]);
    SetCRM_Switch(41, AxumData.Monitor[1].Aux[3]);
    SetCRM_Switch(42, AxumData.Monitor[1].Comm);
    SetCRM_Switch(43, AxumData.Monitor[1].CUE);
    SetCRM_Switch(44, AxumData.Monitor[1].Ext[0]);
    SetCRM_Switch(45, AxumData.Monitor[1].Ext[1]);
    SetCRM_Switch(46, AxumData.Monitor[1].Ext[2]);
    SetCRM_Switch(47, AxumData.Monitor[1].Ext[3]);
    SetCRM_Switch(48, AxumData.Monitor[1].Dim);
    SetCRM_Switch(49, AxumData.Monitor[1].Mute);
}
*/

void SetDSPCard_MonitorChannel(unsigned int MonitorChannelNr)
{
  unsigned char DSPCardNr = (MonitorChannelNr/8);
  unsigned char DSPCardMonitorChannelNr = MonitorChannelNr%8;
  float factor = 0;

  for (int cntMonitorInput=0; cntMonitorInput<48; cntMonitorInput++)
  {
    if (DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr].Level[cntMonitorInput]<=-140)
    {
      factor = 0;
    }
    else
    {
      factor = pow10(DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr].Level[cntMonitorInput]/20);
    }

    if (DSPCardNr == 0)
    {
      //MuteX implementation?
      *PtrDSP_HPIA[2] = SummingDSPUpdate_MatrixFactor+((64*32)*4)+(32*4)+(DSPCardMonitorChannelNr*4)+((cntMonitorInput*8)*4);
      *((float *)PtrDSP_HPID[2]) = factor;
    }
  }

  if (DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr].MasterLevel<=-140)
  {
    factor = 0;
  }
  else
  {
    factor = pow10(DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr].MasterLevel/20);
  }
  if (DSPCardNr == 0)
  {
    //MuteX implementation?
    *PtrDSP_HPIA[2] = SummingDSPUpdate_MatrixFactor+((64*32)*4)+(32*4)+(DSPCardMonitorChannelNr*4)+((48*8)*4);
    *((float *)PtrDSP_HPID[2]) = factor;
  }
}

void SetAxum_MonitorBuss(unsigned int MonitorBussNr)
{
  unsigned int FirstMonitorChannelNr = MonitorBussNr<<1;
  unsigned char DSPCardNr = (FirstMonitorChannelNr/8);
  unsigned char DSPCardMonitorChannelNr = FirstMonitorChannelNr%8;

  for (int cntMonitorInput=0; cntMonitorInput<48; cntMonitorInput++)
  {
    DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+0].Level[cntMonitorInput] = -140;
    DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+1].Level[cntMonitorInput] = -140;
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
      DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+0].Level[(cntBuss*2)+0] = 0;
      DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+1].Level[(cntBuss*2)+1] = 0;

      if (MonitorBussDimActive)
      {
        if (!AxumData.Monitor[MonitorBussNr].AutoSwitchingBuss[cntBuss])
        {
          DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+0].Level[(cntBuss*2)+0] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
          DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+1].Level[(cntBuss*2)+1] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
        }
      }
    }
  }

  if (AxumData.Monitor[MonitorBussNr].Ext[0])
  {
    DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+0].Level[32] = 0;
    DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+1].Level[33] = 0;
    if (MonitorBussDimActive)
    {
      DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+0].Level[32] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
      DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+1].Level[33] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
    }
  }
  if (AxumData.Monitor[MonitorBussNr].Ext[1])
  {
    DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+0].Level[34] = 0;
    DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+1].Level[35] = 0;
    if (MonitorBussDimActive)
    {
      DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+0].Level[34] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
      DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+1].Level[35] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
    }
  }
  if (AxumData.Monitor[MonitorBussNr].Ext[2])
  {
    DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+0].Level[36] = 0;
    DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+1].Level[37] = 0;
    if (MonitorBussDimActive)
    {
      DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+0].Level[36] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
      DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+1].Level[37] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
    }
  }
  if (AxumData.Monitor[MonitorBussNr].Ext[3])
  {
    DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+0].Level[38] = 0;
    DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+1].Level[39] = 0;
    if (MonitorBussDimActive)
    {
      DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+0].Level[38] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
      DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+1].Level[39] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
    }
  }
  if (AxumData.Monitor[MonitorBussNr].Ext[4])
  {
    DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+0].Level[40] = 0;
    DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+1].Level[41] = 0;
    if (MonitorBussDimActive)
    {
      DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+0].Level[40] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
      DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+1].Level[41] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
    }
  }
  if (AxumData.Monitor[MonitorBussNr].Ext[5])
  {
    DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+0].Level[42] = 0;
    DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+1].Level[43] = 0;
    if (MonitorBussDimActive)
    {
      DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+0].Level[42] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
      DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+1].Level[43] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
    }
  }
  if (AxumData.Monitor[MonitorBussNr].Ext[6])
  {
    DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+0].Level[44] = 0;
    DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+1].Level[45] = 0;
    if (MonitorBussDimActive)
    {
      DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+0].Level[44] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
      DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+1].Level[45] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
    }
  }
  if (AxumData.Monitor[MonitorBussNr].Ext[7])
  {
    DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+0].Level[46] = 0;
    DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+1].Level[47] = 0;
    if (MonitorBussDimActive)
    {
      DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+0].Level[46] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
      DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+1].Level[47] += AxumData.Monitor[MonitorBussNr].SwitchingDimLevel;
    }
  }

  DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+0].MasterLevel = 0;
  DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+1].MasterLevel = 0;
  /*    if (AxumData.Monitor[MonitorBussNr].Dim)
      {
          DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+0].MasterLevel -= 10;
          DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+1].MasterLevel -= 10;
      }

      if (AxumData.Monitor[MonitorBussNr].Mute)
      {
          DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+0].MasterLevel = -140;
          DSPCardData[DSPCardNr].MonitorChannelData[DSPCardMonitorChannelNr+1].MasterLevel = -140;
      }*/

  SetDSPCard_MonitorChannel(DSPCardMonitorChannelNr+0);
  SetDSPCard_MonitorChannel(DSPCardMonitorChannelNr+1);
}

/*void SetModule_LEDs(unsigned int LEDNr, unsigned int ModuleNr, unsigned char State)
{
    int ObjectNr = 1072+(LEDNr*4)+ModuleNr-FirstModuleOnSurface;
    unsigned char TransmitData[32];

    TransmitData[0] = (ObjectNr>>8)&0xFF;
    TransmitData[1] = ObjectNr&0xFF;
    TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
    TransmitData[3] = STATE_DATATYPE;
    TransmitData[4] = 1;
    TransmitData[5] = State&0xFF;

    SendMambaNetMessage(0x00000002, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 6);
}

void SetModule_Fader(unsigned int ModuleNr, unsigned int Position)
{
    int ObjectNr = 1104+ModuleNr-FirstModuleOnSurface;
    unsigned char TransmitData[32];

    TransmitData[0] = (ObjectNr>>8)&0xFF;
    TransmitData[1] = ObjectNr&0xFF;
    TransmitData[2] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;
    TransmitData[3] = UNSIGNED_INTEGER_DATATYPE;
    TransmitData[4] = 2;
    TransmitData[5] = (Position>>8)&0xFF;
    TransmitData[6] = Position&0xFF;

    SendMambaNetMessage(0x00000002, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, 7);
}*/

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
//    if (((SensorReceiveFunctionNumber&0xFF000FFF) != 0x0200001e) && ((SensorReceiveFunctionNumber&0xFF000FFF) != 0x0200001f))
//    {
//      printf("0x%08lx connected to 0x%08lx, ObjectNr: %d\n", SensorReceiveFunctionNumber, WalkAxumFunctionInformationStruct->MambaNetAddress, WalkAxumFunctionInformationStruct->ObjectNr);
//    }

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
        if (AxumData.ModuleData[ModuleNr].Source == 0)
        {
          sprintf(LCDText, "  None  ");
        }
        else
        {
          strncpy(LCDText, AxumData.SourceData[AxumData.ModuleData[ModuleNr].Source-1].SourceName, 8);
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
            if (  (AxumData.ModuleData[ModuleNr].Source == AxumData.ModuleData[ModuleNr].SourceA) &&
                  (AxumData.ModuleData[ModuleNr].Insert == AxumData.ModuleData[ModuleNr].InsertOnOffA) &&
                  (AxumData.ModuleData[ModuleNr].Filter.On == AxumData.ModuleData[ModuleNr].FilterOnOffA) &&
                  (AxumData.ModuleData[ModuleNr].EQOn == AxumData.ModuleData[ModuleNr].EQOnOffA) &&
                  (AxumData.ModuleData[ModuleNr].DynamicsOn == AxumData.ModuleData[ModuleNr].DynamicsOnOffA))
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
            if (  (AxumData.ModuleData[ModuleNr].Source == AxumData.ModuleData[ModuleNr].SourceB) &&
                  (AxumData.ModuleData[ModuleNr].Insert == AxumData.ModuleData[ModuleNr].InsertOnOffB) &&
                  (AxumData.ModuleData[ModuleNr].Filter.On == AxumData.ModuleData[ModuleNr].FilterOnOffB) &&
                  (AxumData.ModuleData[ModuleNr].EQOn == AxumData.ModuleData[ModuleNr].EQOnOffB) &&
                  (AxumData.ModuleData[ModuleNr].DynamicsOn == AxumData.ModuleData[ModuleNr].DynamicsOnOffB))
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
        int SourceNr = AxumData.ModuleData[ModuleNr].Source-1;

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
        int SourceNr = AxumData.ModuleData[ModuleNr].Source-1;

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
        if (AxumData.ModuleData[ModuleNr].Source == 0)
        {
          sprintf(LCDText, "  - dB  ");
        }
        else
        {
          sprintf(LCDText,     "%5.1fdB", AxumData.SourceData[AxumData.ModuleData[ModuleNr].Source-1].Gain);
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
        if (AxumData.ModuleData[ModuleNr].Source != 0)
        {
          int SourceNr = AxumData.ModuleData[ModuleNr].Source-1;
          Active = AxumData.SourceData[SourceNr].Start;
        }
      }
      break;
      case MODULE_FUNCTION_SOURCE_STOP:
      { //Stop
        Active = 0;
        if (AxumData.ModuleData[ModuleNr].Source != 0)
        {
          int SourceNr = AxumData.ModuleData[ModuleNr].Source-1;
          Active = !AxumData.SourceData[SourceNr].Start;
        }
      }
      break;
      case MODULE_FUNCTION_SOURCE_START_STOP:
      { //Start/Stop
        Active = 0;
        if (AxumData.ModuleData[ModuleNr].Source != 0)
        {
          int SourceNr = AxumData.ModuleData[ModuleNr].Source-1;
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
        if (AxumData.ModuleData[ModuleNr].Source != 0)
        {
          int SourceNr = AxumData.ModuleData[ModuleNr].Source-1;
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
      else if ((FunctionNr>=GLOBAL_FUNCTION_MASTER_CONTROL_4_MODE_BUSS_1_2) && (FunctionNr<GLOBAL_FUNCTION_MASTER_CONTROL_1))
      { //Master control 1 mode
        unsigned char Active = 0;
        unsigned char TransmitData[32];
        unsigned char CorrespondingControlMode = FunctionNr-GLOBAL_FUNCTION_MASTER_CONTROL_4_MODE_BUSS_1_2;
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
        if (AxumData.ModuleData[cntModule].Source == ((signed int)(SourceNr+1)))
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
        if (AxumData.ModuleData[cntModule].Source == ((signed int)(SourceNr+1)))
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
        if (AxumData.ModuleData[cntModule].Source == ((signed int)(SourceNr+1)))
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
        if (AxumData.ModuleData[cntModule].Source == ((signed int)(SourceNr+1)))
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
        if (AxumData.ModuleData[cntModule].Source == ((signed int)(SourceNr+1)))
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
        if (AxumData.ModuleData[cntModule].Source == ((signed int)(SourceNr+1)))
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
        if (AxumData.ModuleData[cntModule].Source == ((signed int)(SourceNr+1)))
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

        switch (AxumData.DestinationData[DestinationNr].Source)
        {
        case 0:
        {
          sprintf(LCDText, "  None  ");
        }
        break;
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
        {
          int BussNr = AxumData.DestinationData[DestinationNr].Source-1;
          strncpy(LCDText, AxumData.BussMasterData[BussNr].Label, 8);
        }
        break;
        case 17:
        {
          sprintf(LCDText, "  CRM   ");
        }
        break;
        case 18:
        case 19:
        case 20:
        {
          sprintf(LCDText, "Studio %d", AxumData.DestinationData[DestinationNr].Source-17);
        }
        break;
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
        case 26:
        case 27:
        case 28:
        case 29:
        case 30:
        case 31:
        case 32:
        {
          sprintf(LCDText, " Mon %2d ", AxumData.DestinationData[DestinationNr].Source-20);
        }
        break;
        }

        if (AxumData.DestinationData[DestinationNr].Source>32)
        {
          if (AxumData.DestinationData[DestinationNr].Source<161)
          {
            sprintf(LCDText, "Ins: %d", AxumData.DestinationData[DestinationNr].Source-32);
          }
          else
          {
            int SourceNr =AxumData.DestinationData[DestinationNr].Source-161;
            if ((SourceNr+1) == 0)
            {
              sprintf(LCDText, "  None  ");
            }
            else
            {
              strncpy(LCDText, AxumData.SourceData[SourceNr].SourceName, 8);
            }
          }
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
      }
    }
    break;
    case DESTINATION_FUNCTION_MONITOR_SPEAKER_LEVEL:
    {
      if ((AxumData.DestinationData[DestinationNr].Source>16) && (AxumData.DestinationData[DestinationNr].Source<33))
      {
        int MonitorBussNr = AxumData.DestinationData[DestinationNr].Source-17;

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
      if ((AxumData.DestinationData[DestinationNr].Source>16) && (AxumData.DestinationData[DestinationNr].Source<33))
      {
        int MonitorBussNr = AxumData.DestinationData[DestinationNr].Source-17;

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

        if ((AxumData.DestinationData[DestinationNr].Source>16) && (AxumData.DestinationData[DestinationNr].Source<33))
        {
          int MonitorBussNr = AxumData.DestinationData[DestinationNr].Source-17;
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

        if ((AxumData.DestinationData[DestinationNr].Source>16) && (AxumData.DestinationData[DestinationNr].Source<33))
        {
          int MonitorBussNr = AxumData.DestinationData[DestinationNr].Source-17;
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

        if ((AxumData.DestinationData[DestinationNr].Source>16) && (AxumData.DestinationData[DestinationNr].Source<33))
        {
          int MonitorBussNr = AxumData.DestinationData[DestinationNr].Source-17;
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

        if ((AxumData.DestinationData[DestinationNr].Source>16) && (AxumData.DestinationData[DestinationNr].Source<33))
        {
          int MonitorBussNr = AxumData.DestinationData[DestinationNr].Source-17;
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

        if ((AxumData.DestinationData[DestinationNr].Source>16) && (AxumData.DestinationData[DestinationNr].Source<33))
        {
          int MonitorBussNr = AxumData.DestinationData[DestinationNr].Source-17;
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
//              printf("0x%08lx connected to 0x%08lx, ObjectNr: %d - %s\n", SensorReceiveFunctionNumber, OnlineNodeInformation[cntNodes].MambaNetAddress, 1024+cntObject, OnlineNodeInformation[cntNodes].ObjectInformation[cntObject].Description);

              AXUM_FUNCTION_INFORMATION_STRUCT *AxumFunctionInformationStructToAdd = new AXUM_FUNCTION_INFORMATION_STRUCT;
//              printf("0x%08lx pointer\n", AxumFunctionInformationStructToAdd);

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
//    printf("0x%08lx connected to 0x%08lx, ObjectNr: %d\n", SensorReceiveFunctionNumber, WalkAxumFunctionInformationStruct->MambaNetAddress, WalkAxumFunctionInformationStruct->ObjectNr);
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

void *thread(void *vargp)
{
  int ID = msgget(5001, 0666 | IPC_CREAT);

  typedef struct
  {
    long mType;
    char Text[256];
  } MSG_STRUCT;

  MSG_STRUCT TempMessage;
  int msgsz = sizeof(MSG_STRUCT) - sizeof(long int);

  while (!ExitApplication)
  {
    int BytesReceived = msgrcv(ID, &TempMessage, msgsz, 0, 0);
    if (BytesReceived == -1)
    {
      perror("msgrcv");
    }
    else
    {
      char TextString[256];

      strncpy(TextString, TempMessage.Text, BytesReceived);
      TextString[BytesReceived] = 0;
      printf("Message Received: %s\n", TextString);

      int StartPos = 0;
      int Length = 0;
      for (int cntChar=0; cntChar<BytesReceived; cntChar++)
      {
        if (TextString[cntChar] == '"')
        {
          if (StartPos == 0)
          {
            StartPos = cntChar+1;
          }
          else if (Length == 0)
          {
            Length = cntChar-StartPos;
          }
        }
      }

      unsigned char NodeConfigurationChanged = 0;
      unsigned char SourceConfigurationChanged = 0;
      unsigned char DestinationConfigurationChanged = 0;
      int ModuleConfigurationChanged = 0;
      unsigned char BussConfigurationChanged = 0;
      unsigned char MonitorBussConfigurationChanged = 0;
      unsigned char ExternSourceConfigurationChanged = 0;
      unsigned char TalkbackConfigurationChanged = 0;
      unsigned char GlobalConfigurationChanged = 0;
      unsigned int MambaNetAddress = 0x00000000;
      unsigned int ObjectNr = 0;

      int ParameterNumber = 0;
      char TempBuffer[256];
      unsigned char cntTempBuffer = 0;
      for (int cntChar=0; cntChar<Length; cntChar++)
      {
        if (TextString[StartPos+cntChar]==',')
        {
          TempBuffer[cntTempBuffer]=0;
          switch (ParameterNumber)
          {
          case 0:
          { //message type
            if (strcmp("configuration_node", TempBuffer) == 0)
            {
              NodeConfigurationChanged = 1;
            }
            else if (strcmp("source_configuration", TempBuffer) == 0)
            {
              SourceConfigurationChanged = 1;
            }
            else if (strcmp("destination_configuration", TempBuffer) == 0)
            {
              DestinationConfigurationChanged = 1;
            }
            else if (strcmp("module_configuration", TempBuffer) == 0)
            {
              ModuleConfigurationChanged = 1;
            }
            else if (strcmp("module_configuration_eq", TempBuffer) == 0)
            {
              ModuleConfigurationChanged = 2;
            }
            else if (strcmp("module_configuration_busses", TempBuffer) == 0)
            {
              ModuleConfigurationChanged = 3;
            }
            else if (strcmp("buss_configuration", TempBuffer) == 0)
            {
              BussConfigurationChanged = 1;
            }
            else if (strcmp("monitor_buss_configuration", TempBuffer) == 0)
            {
              MonitorBussConfigurationChanged = 1;
            }
            else if (strcmp("extern_source_configuration", TempBuffer) == 0)
            {
              ExternSourceConfigurationChanged = 1;
            }
            else if (strcmp("talkback_configuration", TempBuffer) == 0)
            {
              TalkbackConfigurationChanged = 1;
            }
            else if (strcmp("global_configuration", TempBuffer) == 0)
            {
              GlobalConfigurationChanged = 1;
            }
          }
          break;
          case 1:
          {
            if (NodeConfigurationChanged)
            {
              //MambaNetAddres
              MambaNetAddress = strtol(TempBuffer, NULL, 16);
            }
          }
          break;
          }
          ParameterNumber++;
          cntTempBuffer=0;
        }
        else
        {
          TempBuffer[cntTempBuffer++] = TextString[StartPos+cntChar];
        }
      }
      TempBuffer[cntTempBuffer]=0;
      //Last parameter is ObjectNr
      ObjectNr = atoi(TempBuffer);

      if (NodeConfigurationChanged)
      {
        char SQLQuery[8192];
        char *zErrMsg;

        //Do refresh configuration table
        sprintf(SQLQuery, "	SELECT * FROM configuration_node_%08x WHERE \"Object Number\" = %d;\n", MambaNetAddress, ObjectNr);

        int IndexOfSender = -1;
        for (unsigned int cntIndex=0; cntIndex<AddressTableCount; cntIndex++)
        {
          if (OnlineNodeInformation[cntIndex].MambaNetAddress == MambaNetAddress)
          {
            IndexOfSender = cntIndex;
          }
        }

        if (IndexOfSender != -1)
        {
          long int OldFunction = OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].FunctionNr;
          OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].FunctionNr = -1;
          OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].LastChangedTime = 0;
          OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].PreviousLastChangedTime = 0;
          OnlineNodeInformation[IndexOfSender].SensorReceiveFunction[ObjectNr-1024].TimeBeforeMomentary = DEFAULT_TIME_BEFORE_MOMENTARY;
          if (sqlite3_exec(axum_engine_db, SQLQuery, NodeConfigurationCallback, (void *)&IndexOfSender, &zErrMsg) != SQLITE_OK)
          {
            printf("SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
          }

          if (OldFunction >= 0)
          {
            MakeObjectListPerFunction(OldFunction);
          }
        }
      }
      if (SourceConfigurationChanged)
      {
        char SQLQuery[8192];
        char *zErrMsg;

        //Do refresh configuration table
        sprintf(SQLQuery, "SELECT * FROM source_configuration WHERE Number = %d;\n", ObjectNr);
        printf(SQLQuery);
        if (sqlite3_exec(axum_engine_db, SQLQuery, SourceConfigurationCallback, 0, &zErrMsg) != SQLITE_OK)
        {
          printf("SQL error: %s\n", zErrMsg);
          sqlite3_free(zErrMsg);
        }
      }
      if (DestinationConfigurationChanged)
      {
        char SQLQuery[8192];
        char *zErrMsg;

        //Do refresh configuration table
        sprintf(SQLQuery, "SELECT * FROM destination_configuration WHERE Number = %d;\n", ObjectNr);
        printf(SQLQuery);
        if (sqlite3_exec(axum_engine_db, SQLQuery, DestinationConfigurationCallback, 0, &zErrMsg) != SQLITE_OK)
        {
          printf("SQL error: %s\n", zErrMsg);
          sqlite3_free(zErrMsg);
        }
      }
      if (ModuleConfigurationChanged)
      {
        char SQLQuery[8192];
        char *zErrMsg;

        //Do refresh configuration table
        sprintf(SQLQuery, "SELECT * FROM module_configuration WHERE Number = %d;\n", ObjectNr);
        printf(SQLQuery);
        if (sqlite3_exec(axum_engine_db, SQLQuery, ModuleConfigurationCallback, (void *)&ModuleConfigurationChanged, &zErrMsg) != SQLITE_OK)
        {
          printf("SQL error: %s\n", zErrMsg);
          sqlite3_free(zErrMsg);
        }
      }
      if (BussConfigurationChanged)
      {
        char SQLQuery[8192];
        char *zErrMsg;

        //Do refresh configuration table
        sprintf(SQLQuery, "SELECT * FROM buss_configuration WHERE Number = %d;\n", ObjectNr);
        printf(SQLQuery);
        if (sqlite3_exec(axum_engine_db, SQLQuery, BussConfigurationCallback, 0, &zErrMsg) != SQLITE_OK)
        {
          printf("SQL error: %s\n", zErrMsg);
          sqlite3_free(zErrMsg);
        }
      }
      if (MonitorBussConfigurationChanged)
      {
        char SQLQuery[8192];
        char *zErrMsg;

        //Do refresh configuration table
        sprintf(SQLQuery, "SELECT * FROM monitor_buss_configuration WHERE Number = %d;\n", ObjectNr);
        printf(SQLQuery);
        if (sqlite3_exec(axum_engine_db, SQLQuery, MonitorBussConfigurationCallback, 0, &zErrMsg) != SQLITE_OK)
        {
          printf("SQL error: %s\n", zErrMsg);
          sqlite3_free(zErrMsg);
        }
      }
      if (ExternSourceConfigurationChanged)
      {
        char SQLQuery[8192];
        char *zErrMsg;

        //Do refresh configuration table
        sprintf(SQLQuery, "SELECT * FROM extern_source_configuration WHERE Number = %d;\n", ObjectNr);
        printf(SQLQuery);
        if (sqlite3_exec(axum_engine_db, SQLQuery, ExternSourceConfigurationCallback, 0, &zErrMsg) != SQLITE_OK)
        {
          printf("SQL error: %s\n", zErrMsg);
          sqlite3_free(zErrMsg);
        }
      }
      if (TalkbackConfigurationChanged)
      {
        char SQLQuery[8192];
        char *zErrMsg;

        //Do refresh configuration table
        sprintf(SQLQuery, "SELECT * FROM talkback_configuration;\n");
        printf(SQLQuery);
        if (sqlite3_exec(axum_engine_db, SQLQuery, TalkbackConfigurationCallback, 0, &zErrMsg) != SQLITE_OK)
        {
          printf("SQL error: %s\n", zErrMsg);
          sqlite3_free(zErrMsg);
        }
      }
      if (GlobalConfigurationChanged)
      {
        char SQLQuery[8192];
        char *zErrMsg;

        //Do refresh configuration table
        sprintf(SQLQuery, "SELECT * FROM global_configuration;\n");
        printf(SQLQuery);
        if (sqlite3_exec(axum_engine_db, SQLQuery, GlobalConfigurationCallback, 0, &zErrMsg) != SQLITE_OK)
        {
          printf("SQL error: %s\n", zErrMsg);
          sqlite3_free(zErrMsg);
        }
      }
    }
  }

  return NULL;
  vargp = NULL;
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
      while (TempData != 0)
      {
        if (TempData>0)
        {
          CurrentSource++;
          if (CurrentSource > 1280)
          {
            CurrentSource = 0;
          }

          //skip used hybrids
          while ((CurrentSource != 0) && (Axum_MixMinusSourceUsed(CurrentSource-1) != -1))
          {
            CurrentSource++;
            if (CurrentSource > 1280)
            {
              CurrentSource = 0;
            }
          }

          //skip empty sources
          while ((CurrentSource != 0) && ((AxumData.SourceData[CurrentSource-1].InputData[0].MambaNetAddress == 0x00000000) && (AxumData.SourceData[CurrentSource-1].InputData[1].MambaNetAddress == 0x00000000)))
          {
            CurrentSource++;
            if (CurrentSource > 1280)
            {
              CurrentSource = 0;
            }
          }

          TempData--;
        }
        else
        {
          CurrentSource--;
          if (CurrentSource < 0)
          {
            CurrentSource = 1280;
          }

          //skip empty sources
          while ((CurrentSource != 0) && ((AxumData.SourceData[CurrentSource-1].InputData[0].MambaNetAddress == 0x00000000) && (AxumData.SourceData[CurrentSource-1].InputData[1].MambaNetAddress == 0x00000000)))
          {
            CurrentSource--;
            if (CurrentSource < 0)
            {
              CurrentSource = 1280;
            }
          }

          //skip used hybrids
          while ((CurrentSource != 0) && (Axum_MixMinusSourceUsed(CurrentSource-1) != -1))
          {
            CurrentSource--;
            if (CurrentSource < 0)
            {
              CurrentSource = 1280;
            }
          }
          TempData++;
        }
      }
      SetNewSource(ModuleNr, CurrentSource, 0, 0);
      /*
              int OldSource = AxumData.ModuleData[ModuleNr].Source;
              if (OldSource != CurrentSource)
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
                  AxumData.ModuleData[ModuleNr].Source = CurrentSource;

                      SetAxum_ModuleSource(ModuleNr);
                  SetAxum_ModuleMixMinus(ModuleNr);

                  unsigned int FunctionNrToSent = 0x00000000 | (ModuleNr<<12);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
                    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

                  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_SOURCE_A);
                  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_SOURCE_B);

                  if (OldSource != 0)
                  {
                    FunctionNrToSent = 0x05000000 | ((OldSource-1)<<12);
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
                      CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_31_32_ON_OFF);          CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_COUGH_ON_OFF);
                  }

                  if (CurrentSource != 0)
                  {
                    FunctionNrToSent = 0x05000000 | ((CurrentSource-1)<<12);
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
                      CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_BUSS_31_32_ON_OFF);          CheckObjectsToSent(FunctionNrToSent+SOURCE_FUNCTION_MODULE_COUGH_ON_OFF);
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
              }*/
    }
    break;
    case MODULE_CONTROL_MODE_SOURCE_GAIN:
    {   //Source gain
      if (AxumData.ModuleData[ModuleNr].Source != 0)
      {
        int SourceNr = AxumData.ModuleData[ModuleNr].Source-1;

        AxumData.SourceData[SourceNr].Gain += (float)TempData/10;
        if (AxumData.SourceData[SourceNr].Gain < 20)
        {
          AxumData.SourceData[SourceNr].Gain = 20;
        }
        else if (AxumData.SourceData[SourceNr].Gain > 75)
        {
          AxumData.SourceData[SourceNr].Gain = 75;
        }
        //CONTROL 1 sent later in loop...
        //CheckObjectsToSent(SensorReceiveFunctionNumber);


        unsigned int DisplayFunctionNr = 0x05000000 | (SourceNr<<12);
        CheckObjectsToSent(DisplayFunctionNr+SOURCE_FUNCTION_GAIN);

        for (int cntModule=0; cntModule<128; cntModule++)
        {
          if (AxumData.ModuleData[cntModule].Source == (SourceNr+1))
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
      int BandNr = (AxumData.Control1Mode-MODULE_CONTROL_MODE_EQ_BAND_1_LEVEL)/(MODULE_CONTROL_MODE_EQ_BAND_2_LEVEL-MODULE_CONTROL_MODE_EQ_BAND_1_LEVEL);
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
      int BandNr = (AxumData.Control1Mode-MODULE_CONTROL_MODE_EQ_BAND_1_FREQUENCY)/(MODULE_CONTROL_MODE_EQ_BAND_2_FREQUENCY-MODULE_CONTROL_MODE_EQ_BAND_1_FREQUENCY);
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
      int BandNr = (AxumData.Control1Mode-MODULE_CONTROL_MODE_EQ_BAND_1_BANDWIDTH)/(MODULE_CONTROL_MODE_EQ_BAND_2_BANDWIDTH-MODULE_CONTROL_MODE_EQ_BAND_1_BANDWIDTH);

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
      int BandNr = (AxumData.Control1Mode-MODULE_CONTROL_MODE_EQ_BAND_1_TYPE)/(MODULE_CONTROL_MODE_EQ_BAND_2_TYPE-MODULE_CONTROL_MODE_EQ_BAND_1_TYPE);
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

        if (AxumData.ModuleData[ModuleNr].Source > 0)
        {
          FunctionNrToSent = 0x05000000 | ((AxumData.ModuleData[ModuleNr].Source-1)<<12);
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
      int BussNr = (AxumData.Control1Mode-MODULE_CONTROL_MODE_BUSS_1_2)/(MODULE_CONTROL_MODE_BUSS_3_4-MODULE_CONTROL_MODE_BUSS_1_2);
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
      int BussNr = (AxumData.Control1Mode-MODULE_CONTROL_MODE_BUSS_1_2_BALANCE)/(MODULE_CONTROL_MODE_BUSS_3_4_BALANCE-MODULE_CONTROL_MODE_BUSS_1_2_BALANCE);


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
        int OldSource = AxumData.ModuleData[ModuleNr].Source;
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
            SetAxum_ModuleMixMinus(ModuleNr);

            FunctionNrToSent = 0x05000000 | ((OldSource-1)<<12);
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
      break;
      case MODULE_CONTROL_MODE_SOURCE_GAIN:
      {   //Source gain
        if (AxumData.ModuleData[ModuleNr].Source != 0)
        {
          int SourceNr = AxumData.ModuleData[ModuleNr].Source-1;

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
        int BandNr = (AxumData.Control1Mode-MODULE_CONTROL_MODE_EQ_BAND_1_LEVEL)/(MODULE_CONTROL_MODE_EQ_BAND_2_LEVEL-MODULE_CONTROL_MODE_EQ_BAND_1_LEVEL);
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
        int BandNr = (AxumData.Control1Mode-MODULE_CONTROL_MODE_EQ_BAND_1_FREQUENCY)/(MODULE_CONTROL_MODE_EQ_BAND_2_FREQUENCY-MODULE_CONTROL_MODE_EQ_BAND_1_FREQUENCY);
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
        int BandNr = (AxumData.Control1Mode-MODULE_CONTROL_MODE_EQ_BAND_1_BANDWIDTH)/(MODULE_CONTROL_MODE_EQ_BAND_2_BANDWIDTH-MODULE_CONTROL_MODE_EQ_BAND_1_BANDWIDTH);
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
        int BandNr = (AxumData.Control1Mode-MODULE_CONTROL_MODE_EQ_BAND_1_TYPE)/(MODULE_CONTROL_MODE_EQ_BAND_2_TYPE-MODULE_CONTROL_MODE_EQ_BAND_1_TYPE);
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

          if (AxumData.ModuleData[ModuleNr].Source > 0)
          {
            FunctionNrToSent = 0x05000000 | ((AxumData.ModuleData[ModuleNr].Source-1)<<12);
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
        int BussNr = (AxumData.Control1Mode-MODULE_CONTROL_MODE_BUSS_1_2)/(MODULE_CONTROL_MODE_BUSS_3_4-MODULE_CONTROL_MODE_BUSS_1_2);
        AxumData.ModuleData[ModuleNr].Buss[BussNr].On = !AxumData.ModuleData[ModuleNr].Buss[BussNr].On;
        SetAxum_BussLevels(ModuleNr);
        SetAxum_ModuleMixMinus(ModuleNr);


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
        int BussNr = (AxumData.Control1Mode-MODULE_CONTROL_MODE_BUSS_1_2)/(MODULE_CONTROL_MODE_BUSS_3_4-MODULE_CONTROL_MODE_BUSS_1_2);

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
    if (AxumData.ModuleData[ModuleNr].Source == 0)
    {
      sprintf(LCDText, "  None  ");
    }
    else
    {
      strncpy(LCDText, AxumData.SourceData[AxumData.ModuleData[ModuleNr].Source-1].SourceName, 8);
    }
  }
  break;
  case MODULE_CONTROL_MODE_SOURCE:
  {
    if (AxumData.ModuleData[ModuleNr].Source == 0)
    {
      sprintf(LCDText, "  None  ");
    }
    else
    {
      strncpy(LCDText, AxumData.SourceData[AxumData.ModuleData[ModuleNr].Source-1].SourceName, 8);
    }
  }
  break;
  case MODULE_CONTROL_MODE_SOURCE_GAIN:
  {
    if (AxumData.ModuleData[ModuleNr].Source == 0)
    {
      sprintf(LCDText, "  - dB  ");
    }
    else
    {
      sprintf(LCDText,     "%5.1fdB", AxumData.SourceData[AxumData.ModuleData[ModuleNr].Source-1].Gain);
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
    int BandNr = (AxumData.Control1Mode-MODULE_CONTROL_MODE_EQ_BAND_1_LEVEL)/(MODULE_CONTROL_MODE_EQ_BAND_2_LEVEL-MODULE_CONTROL_MODE_EQ_BAND_1_LEVEL);
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
    int BandNr = (AxumData.Control1Mode-MODULE_CONTROL_MODE_EQ_BAND_1_FREQUENCY)/(MODULE_CONTROL_MODE_EQ_BAND_2_FREQUENCY-MODULE_CONTROL_MODE_EQ_BAND_1_FREQUENCY);
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
    int BandNr = (AxumData.Control1Mode-MODULE_CONTROL_MODE_EQ_BAND_1_BANDWIDTH)/(MODULE_CONTROL_MODE_EQ_BAND_2_BANDWIDTH-MODULE_CONTROL_MODE_EQ_BAND_1_BANDWIDTH);
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
    int BandNr = (AxumData.Control1Mode-MODULE_CONTROL_MODE_EQ_BAND_1_TYPE)/(MODULE_CONTROL_MODE_EQ_BAND_2_TYPE-MODULE_CONTROL_MODE_EQ_BAND_1_TYPE);
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
    int BussNr = (AxumData.Control1Mode-MODULE_CONTROL_MODE_BUSS_1_2)/(MODULE_CONTROL_MODE_BUSS_3_4-MODULE_CONTROL_MODE_BUSS_1_2);
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
    int BussNr = (AxumData.Control1Mode-MODULE_CONTROL_MODE_BUSS_1_2_BALANCE)/(MODULE_CONTROL_MODE_BUSS_3_4_BALANCE-MODULE_CONTROL_MODE_BUSS_1_2_BALANCE);

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
    int BandNr = (AxumData.Control1Mode-MODULE_CONTROL_MODE_EQ_BAND_1_LEVEL)/(MODULE_CONTROL_MODE_EQ_BAND_2_LEVEL-MODULE_CONTROL_MODE_EQ_BAND_1_LEVEL);
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
    int BandNr = (AxumData.Control1Mode-MODULE_CONTROL_MODE_EQ_BAND_1_FREQUENCY)/(MODULE_CONTROL_MODE_EQ_BAND_2_FREQUENCY-MODULE_CONTROL_MODE_EQ_BAND_1_FREQUENCY);
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
    int BandNr = (AxumData.Control1Mode-MODULE_CONTROL_MODE_EQ_BAND_1_BANDWIDTH)/(MODULE_CONTROL_MODE_EQ_BAND_2_BANDWIDTH-MODULE_CONTROL_MODE_EQ_BAND_1_BANDWIDTH);
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
    int BandNr = (AxumData.Control1Mode-MODULE_CONTROL_MODE_EQ_BAND_1_TYPE)/(MODULE_CONTROL_MODE_EQ_BAND_2_TYPE-MODULE_CONTROL_MODE_EQ_BAND_1_TYPE);
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
    int BussNr = (AxumData.Control1Mode-MODULE_CONTROL_MODE_BUSS_1_2)/(MODULE_CONTROL_MODE_BUSS_3_4-MODULE_CONTROL_MODE_BUSS_1_2);
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
    int BussNr = (AxumData.Control1Mode-MODULE_CONTROL_MODE_BUSS_1_2_BALANCE)/(MODULE_CONTROL_MODE_BUSS_3_4_BALANCE-MODULE_CONTROL_MODE_BUSS_1_2_BALANCE);
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
      int BussNr = AxumData.MasterControl1Mode-MASTER_CONTROL_MODE_BUSS_1_2;
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
      int BussNr = AxumData.MasterControl1Mode-MASTER_CONTROL_MODE_BUSS_1_2;
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
    MasterControlMode = AxumData.MasterControl1Mode;
  }
  break;
  case GLOBAL_FUNCTION_MASTER_CONTROL_2_RESET:
  {
    MasterControlMode = AxumData.MasterControl2Mode;
  }
  break;
  case GLOBAL_FUNCTION_MASTER_CONTROL_3_RESET:
  {
    MasterControlMode = AxumData.MasterControl3Mode;
  }
  break;
  case GLOBAL_FUNCTION_MASTER_CONTROL_4_RESET:
  {
    MasterControlMode = AxumData.MasterControl4Mode;
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
        int BussNr = AxumData.MasterControl1Mode - MASTER_CONTROL_MODE_BUSS_1_2;
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

  switch (AxumData.MasterControl1Mode)
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
    int BussNr = AxumData.MasterControl1Mode-MASTER_CONTROL_MODE_BUSS_1_2;
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
      SetAxum_ModuleMixMinus(cntModule);

      unsigned int FunctionNrToSent = 0x00000000 | (cntModule<<12);
      CheckObjectsToSent(FunctionNrToSent | (MODULE_FUNCTION_BUSS_1_2_ON_OFF+(BussNr*(MODULE_FUNCTION_BUSS_3_4_ON_OFF-MODULE_FUNCTION_BUSS_1_2_ON_OFF))));

      FunctionNrToSent = ((cntModule<<12)&0xFFF000);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

      if (AxumData.ModuleData[cntModule].Source != 0)
      {
        int SourceNr = AxumData.ModuleData[cntModule].Source-1;
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
        AxumData.Monitor[cntMonitorBuss].Buss[ExtNr] = 1;

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
  if (AxumData.ModuleData[ModuleNr].Source != 0)
  {
    int SourceNr = AxumData.ModuleData[ModuleNr].Source-1;
    int CurrentSourceActive = AxumData.SourceData[SourceNr].Active;
    int NewSourceActive = 0;
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
        if (AxumData.ModuleData[cntModule].Source == (SourceNr+1))
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

      //redlights
      for (int cntRedlight=0; cntRedlight<8; cntRedlight++)
      {
        if (AxumData.SourceData[SourceNr].Redlight[cntRedlight])
        {
          AxumData.Redlight[cntRedlight] = AxumData.SourceData[SourceNr].Active;

          unsigned int FunctionNrToSent = 0x04000000 | (GLOBAL_FUNCTION_REDLIGHT_1+(cntRedlight*(GLOBAL_FUNCTION_REDLIGHT_2-GLOBAL_FUNCTION_REDLIGHT_1)));
          CheckObjectsToSent(FunctionNrToSent);
        }
      }
      //mutes
      for (int cntMonitorBuss=0; cntMonitorBuss<16; cntMonitorBuss++)
      {
        if (AxumData.SourceData[SourceNr].MonitorMute[cntMonitorBuss])
        {
          AxumData.Monitor[cntMonitorBuss].Mute = AxumData.SourceData[SourceNr].Active;

          unsigned int FunctionNrToSent = 0x02000000 | (cntMonitorBuss<<12);
          CheckObjectsToSent(FunctionNrToSent | MONITOR_BUSS_FUNCTION_MUTE);

          for (int cntDestination=0; cntDestination<1280; cntDestination++)
          {
            if (AxumData.DestinationData[cntDestination].Source == (17+cntMonitorBuss))
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
      if (AxumData.ModuleData[cntModule].Source != 0)
      {
        int SourceNr = AxumData.ModuleData[cntModule].Source-1;

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
        if (AxumData.DestinationData[cntDestination].Source == (17+cntMonitorBuss))
        {
          FunctionNrToSent = 0x06000000 | (cntDestination<<12);
          CheckObjectsToSent(FunctionNrToSent | DESTINATION_FUNCTION_DIM_AND_MONITOR_DIM);
        }
      }
    }
  }
}

int Axum_MixMinusSourceUsed(int CurrentSource)
{
  int ModuleNr = -1;
  int cntModule = 0;
  while ((cntModule<128) && (ModuleNr == -1))
  {
    if (cntModule != ModuleNr)
    {
      if (AxumData.ModuleData[cntModule].Source == (CurrentSource+1))
      {
        int cntDestination = 0;
        while ((cntDestination<1280) && (ModuleNr == -1))
        {
          if (AxumData.DestinationData[cntDestination].MixMinusSource == (CurrentSource+1))
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

void SetNewSource(int ModuleNr, int NewSource, int Forced, int ApplyAorBSettings)
{
  int OldSource = AxumData.ModuleData[ModuleNr].Source;
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

      //eventual 'reset' the A/B values
      if ((NewSource == AxumData.ModuleData[ModuleNr].SourceA) || (NewSource == AxumData.ModuleData[ModuleNr].SourceB))
      {
        if (ApplyAorBSettings)
        {
          if (NewSource == AxumData.ModuleData[ModuleNr].SourceA)
          {
            AxumData.ModuleData[ModuleNr].Insert = AxumData.ModuleData[ModuleNr].InsertOnOffA;
            AxumData.ModuleData[ModuleNr].Filter.On = AxumData.ModuleData[ModuleNr].FilterOnOffA;
            AxumData.ModuleData[ModuleNr].EQOn = AxumData.ModuleData[ModuleNr].EQOnOffA;
            AxumData.ModuleData[ModuleNr].DynamicsOn = AxumData.ModuleData[ModuleNr].DynamicsOnOffA;
          }
          else if (NewSource == AxumData.ModuleData[ModuleNr].SourceB)
          {
            AxumData.ModuleData[ModuleNr].Insert = AxumData.ModuleData[ModuleNr].InsertOnOffB;
            AxumData.ModuleData[ModuleNr].Filter.On = AxumData.ModuleData[ModuleNr].FilterOnOffB;
            AxumData.ModuleData[ModuleNr].EQOn = AxumData.ModuleData[ModuleNr].EQOnOffB;
            AxumData.ModuleData[ModuleNr].DynamicsOn = AxumData.ModuleData[ModuleNr].DynamicsOnOffB;
          }

          SetAxum_ModuleProcessing(ModuleNr);
          for (int cntBand=0; cntBand<6; cntBand++)
          {
            AxumData.ModuleData[ModuleNr].EQBand[cntBand].On = AxumData.ModuleData[ModuleNr].EQOn;
            SetAxum_EQ(ModuleNr, cntBand);
          }
        }
      }

      SetAxum_ModuleSource(ModuleNr);
      SetAxum_ModuleMixMinus(ModuleNr);

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

      if (OldSource != 0)
      {
        FunctionNrToSent = 0x05000000 | ((OldSource-1)<<12);
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

      if (NewSource != 0)
      {
        FunctionNrToSent = 0x05000000 | ((NewSource-1)<<12);
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
}

void SetBussOnOff(int ModuleNr, int BussNr, int UseInterlock)
{
  SetAxum_BussLevels(ModuleNr);
  SetAxum_ModuleMixMinus(ModuleNr);

  unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
  CheckObjectsToSent(FunctionNrToSent | (MODULE_FUNCTION_BUSS_1_2_ON_OFF+(BussNr*(MODULE_FUNCTION_BUSS_3_4_ON_OFF-MODULE_FUNCTION_BUSS_1_2_ON_OFF))));

  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

  if (AxumData.ModuleData[ModuleNr].Source != 0)
  {
    int SourceNr = AxumData.ModuleData[ModuleNr].Source-1;
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
          SetAxum_ModuleMixMinus(cntModule);

          unsigned int FunctionNrToSent = ((cntModule<<12)&0xFFF000);
          CheckObjectsToSent(FunctionNrToSent | (MODULE_FUNCTION_BUSS_1_2_ON_OFF+(BussNr*(MODULE_FUNCTION_BUSS_3_4_ON_OFF-MODULE_FUNCTION_BUSS_1_2_ON_OFF))));

          FunctionNrToSent = ((cntModule<<12)&0xFFF000);
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

          if (AxumData.ModuleData[cntModule].Source != 0)
          {
            int SourceNr = AxumData.ModuleData[cntModule].Source-1;
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
        AxumData.Monitor[cntMonitorBuss].Buss[ExtNr] = 1;

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
    AxumData.ModuleData[cntModule].Insert = 0;
    AxumData.ModuleData[cntModule].InsertOnOffA = 0;
    AxumData.ModuleData[cntModule].InsertOnOffB = 0;
    AxumData.ModuleData[cntModule].Gain = 0;
    AxumData.ModuleData[cntModule].PhaseReverse = 0;

    AxumData.ModuleData[cntModule].Filter.On = 0;
    AxumData.ModuleData[cntModule].Filter.Level = 0;
    AxumData.ModuleData[cntModule].Filter.Frequency = 80;
    AxumData.ModuleData[cntModule].Filter.Bandwidth = 1;
    AxumData.ModuleData[cntModule].Filter.Slope = 1;
    AxumData.ModuleData[cntModule].Filter.Type = HPF;
    AxumData.ModuleData[cntModule].FilterOnOffA = 0;
    AxumData.ModuleData[cntModule].FilterOnOffB = 0;

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
    AxumData.ModuleData[cntModule].EQOnOffA = 0;
    AxumData.ModuleData[cntModule].EQOnOffB = 0;

    AxumData.ModuleData[cntModule].Dynamics = 0;
    AxumData.ModuleData[cntModule].DynamicsOn = 0;
    AxumData.ModuleData[cntModule].DynamicsOnOffA = 0;
    AxumData.ModuleData[cntModule].DynamicsOnOffB = 0;

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
