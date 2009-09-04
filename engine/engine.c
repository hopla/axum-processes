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
#include "mbn.h"
#include "backup.h"

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

#include <pthread.h>
#include <time.h>
#include <sys/errno.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/msg.h>

/* deadlock warning: don't use sql_lock() within this mutex */
pthread_mutex_t get_node_info_mutex;

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
#define DEFAULT_BACKUP_FILE "/var/lib/axum/.backup"

#define PCB_MAJOR_VERSION        1
#define PCB_MINOR_VERSION        0

#define FIRMWARE_MAJOR_VERSION   2
#define FIRMWARE_MINOR_VERSION   0

#define MANUFACTURER_ID          0x0001 //D&R
#define PRODUCT_ID               0x000E //Axum Engine

#define NR_OF_STATIC_OBJECTS    (1023-1023)
#define NR_OF_OBJECTS            NR_OF_STATIC_OBJECTS

/********************************/
/* global declarations          */
/********************************/
struct mbn_interface *itf;
struct mbn_handler *mbn;
char error[MBN_ERRSIZE];

struct mbn_node_info this_node =
{
  0, MBN_ADDR_SERVICES_ENGINE,          //MambaNet address, Services
  "Axum Engine (Linux)",                //Description
  "Axum-Engine",                        //Name
  MANUFACTURER_ID, PRODUCT_ID, 0x0001,
  0, 0,                                 //Hw revision
  2, 0,                                 //Fw revision
  0, 0,                                 //FPGA revision
  NR_OF_OBJECTS,                        //Number of objects
  0,                                    //Default engine address
  {0x0000, 0x0000, 0x0000},             //Hardware parent
  0                                     //Service request
};

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
bool dump_packages;                 //To debug the incoming (unsigned char *)data

//int NetworkFileDescriptor;          //identifies the used network device
int DB_fd;

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

ONLINE_NODE_INFORMATION_STRUCT *OnlineNodeInformationList = NULL;
//#define ADDRESS_TABLE_SIZE 65536
//ONLINE_NODE_INFORMATION_STRUCT OnlineNodeInformation[ADDRESS_TABLE_SIZE];

//sqlite3 *axum_engine_db;
//sqlite3 *node_templates_db;

int CallbackNodeIndex = -1;

void lock_node_info(const char *Caller)
{
  printf("lock node_info (%s)\n", Caller);
  pthread_mutex_lock(&get_node_info_mutex);
}
void unlock_node_info(const char *Caller)
{
  pthread_mutex_unlock(&get_node_info_mutex);
  printf("unlock node_info (%s)\n", Caller);
}

void init(int argc, char **argv)
{
  //struct mbn_interface *itf;
  //char err[MBN_ERRSIZE];
  char ethdev[50];
  char dbstr[256];
  pthread_mutexattr_t mattr;
  int c;

  pthread_mutexattr_init(&mattr);
  //pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&get_node_info_mutex, &mattr);

  strcpy(ethdev, DEFAULT_ETH_DEV);
  strcpy(dbstr, DEFAULT_DB_STR);
  strcpy(log_file, DEFAULT_LOG_FILE);
  strcpy(hwparent_path, DEFAULT_GTW_PATH);
  strcpy(backup_file, DEFAULT_BACKUP_FILE);

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
        if(sscanf(optarg, "%hd", &(this_node.UniqueIDPerProduct)) != 1) {
          fprintf(stderr, "Invalid UniqueIDPerProduct");
          exit(1);
        }
        if (this_node.UniqueIDPerProduct < 1)
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

  hwparent(&this_node);

  log_write("hwparent %04X:%04X:%04X", this_node.HardwareParent[0], this_node.HardwareParent[1], this_node.HardwareParent[2]);

  db_open(dbstr);

  DB_fd = db_get_fd();
  if(DB_fd < 0)
  {
    printf("Invalid PostgreSQL socket\n");
    log_close();
    exit(1);
  }

  db_lock(1);
  db_get_matrix_sources();
  db_lock(0);

  dsp_handler = dsp_open();
  if (dsp_handler == NULL)
  {
    db_close();
    log_close();
    exit(1);
  }

  if ((itf=mbnEthernetOpen(ethdev, error)) == NULL)
  {
    fprintf(stderr, "Error opening ethernet device: %s", error);
    dsp_close(dsp_handler);
    db_close();
    log_close();
    exit(1);
  }


  if (!verbose)
    daemonize_finish();

  log_write("Axum Engine Initialized");
}

int main(int argc, char *argv[])
{
  fd_set readfs;

  initialize_axum_data_struct();

  init(argc, argv);

  AxumApplicationAndDSPInitialized = 1;
  log_write("Parameters in DSPs initialized");

  //Slot configuration, former rack organization
  db_lock(1);
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
  db_lock(0);

  //Update default values of EQ to the current values
  for (int cntModule=0; cntModule<128; cntModule++)
  {
    for (int cntEQBand=0; cntEQBand<6; cntEQBand++)
    {
      AxumData.ModuleData[cntModule].EQBand[cntEQBand].Level = AxumData.ModuleData[cntModule].Defaults.EQBand[cntEQBand].Level;
      AxumData.ModuleData[cntModule].EQBand[cntEQBand].Frequency = AxumData.ModuleData[cntModule].Defaults.EQBand[cntEQBand].Frequency;
      AxumData.ModuleData[cntModule].EQBand[cntEQBand].Bandwidth = AxumData.ModuleData[cntModule].Defaults.EQBand[cntEQBand].Bandwidth;
      AxumData.ModuleData[cntModule].EQBand[cntEQBand].Slope = AxumData.ModuleData[cntModule].Defaults.EQBand[cntEQBand].Slope;
      AxumData.ModuleData[cntModule].EQBand[cntEQBand].Type = AxumData.ModuleData[cntModule].Defaults.EQBand[cntEQBand].Type;
    }
  }

  //Check for backup
  if (backup_open((void *)&AxumData, sizeof(AxumData)))
  { //Backup loaded, clear rack-config and set processing data
    for (unsigned char cntSlot=0; cntSlot<42; cntSlot++)
    {
      AxumData.RackOrganization[cntSlot] = 0x00000000;
    }

    for (int cntModule=0; cntModule<128; cntModule++)
    {
      for (int cntBand=0; cntBand<6; cntBand++)
      {
        SetAxum_EQ(cntModule, cntBand);
      }
      SetAxum_ModuleProcessing(cntModule);
      SetAxum_ModuleMixMinus(cntModule, 0);
      SetAxum_BussLevels(cntModule);
    }
    SetAxum_BussMasterLevels();
    for (int cntBuss=0; cntBuss<16; cntBuss++)
    {
      SetAxum_MonitorBuss(cntBuss);
    }
  }

  if((mbn = mbnInit(&this_node, NULL, itf, error)) == NULL) {
    fprintf(stderr, "mbnInit: %s\n", error);
    dsp_close(dsp_handler);
    db_close();
    log_close();
    exit(1);
  }

  //Set required callbacks
  mbnSetAddressTableChangeCallback(mbn, mAddressTableChange);
  mbnSetSensorDataResponseCallback(mbn, mSensorDataResponse);
  mbnSetSensorDataChangedCallback(mbn, mSensorDataChanged);
  mbnSetErrorCallback(mbn, mError);
  mbnSetAcknowledgeTimeoutCallback(mbn, mAcknowledgeTimeout);

  //start interface for the mbn-handler
  mbnStartInterface(itf, error);
  log_write("Axum engine process started, version %d.%d", FIRMWARE_MAJOR_VERSION, FIRMWARE_MINOR_VERSION);

  InitalizeAllObjectListPerFunction();

//  log_write("using network interface device: %s [%02X:%02X:%02X:%02X:%02X:%02X]", NetworkInterface, LocalMACAddress[0], LocalMACAddress[1], LocalMACAddress[2], LocalMACAddress[3], LocalMACAddress[4], LocalMACAddress[5]);

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

  while (!main_quit)
  {
    //Set the sources which wakes the idle-wait process 'select'
    FD_ZERO(&readfs);
    FD_SET(DB_fd, &readfs);

    // block (process is in idle mode) until input becomes available
    int ReturnValue = select(DB_fd+1, &readfs, NULL, NULL, NULL);
    if ((ReturnValue == 0) || ((ReturnValue<0) && (errno == EINTR)))
    {//upon SIGALARM this happens :(
    }
    else if (ReturnValue<0)
    { //error
      log_write("select() failed: %s\n", strerror(errno));
    }
    else
    {//no error or non-blocked signal)
      //Test if the database notifier generated an event.
      db_lock(1);
      db_processnotifies();
      db_lock(0);
    }
  }
  log_write("Closing Engine");

  backup_close();

  DeleteAllObjectListPerFunction();

  dsp_close(dsp_handler);

  if (mbn)
  {
    mbnFree(mbn);
  }
  db_close();

  ONLINE_NODE_INFORMATION_STRUCT *OnlineNodeInformationElement = OnlineNodeInformationList;
  while (OnlineNodeInformationElement != NULL)
  {
    ONLINE_NODE_INFORMATION_STRUCT *DeleteOnlineNodeInformationElement = OnlineNodeInformationElement;;
    OnlineNodeInformationElement = OnlineNodeInformationElement->Next;
    if (DeleteOnlineNodeInformationElement->SensorReceiveFunction != NULL)
    {
      delete[] DeleteOnlineNodeInformationElement->SensorReceiveFunction;
    }
    if (DeleteOnlineNodeInformationElement->ObjectInformation != NULL)
    {
      delete[] DeleteOnlineNodeInformationElement->ObjectInformation;
    }
    delete DeleteOnlineNodeInformationElement;
  }
  OnlineNodeInformationList = NULL;

  log_close();

  return 0;
}

//normally response on GetSensorData
int mSensorDataChanged(struct mbn_handler *mbn, struct mbn_message *message, short unsigned int object, unsigned char type, union mbn_data data)
{
  printf("mSensorDataChanged addr: %08lx, obj: %d\n", message->AddressFrom, object);

  ONLINE_NODE_INFORMATION_STRUCT *OnlineNodeInformationElement = GetOnlineNodeInformation(message->AddressFrom);

  if (OnlineNodeInformationElement == NULL)
  {
    printf("OnlineNodeInformationElement == NULL\n");
    return 1;
  }

  if (OnlineNodeInformationElement->SensorReceiveFunction != NULL)
  {
    SENSOR_RECEIVE_FUNCTION_STRUCT *SensorReceiveFunction = &OnlineNodeInformationElement->SensorReceiveFunction[object-1024];
    SensorReceiveFunction->LastChangedTime = cntMillisecondTimer;
    if (!AxumData.AutoMomentary)
    {
      SensorReceiveFunction->PreviousLastChangedTime = SensorReceiveFunction->LastChangedTime;
    }

    int SensorReceiveFunctionNumber = SensorReceiveFunction->FunctionNr;
    int DataType = OnlineNodeInformationElement->ObjectInformation[object-1024].SensorDataType;
    int DataSize = OnlineNodeInformationElement->ObjectInformation[object-1024].SensorDataSize;
    float DataMinimal = OnlineNodeInformationElement->ObjectInformation[object-1024].SensorDataMinimal;
    float DataMaximal = OnlineNodeInformationElement->ObjectInformation[object-1024].SensorDataMaximal;

    printf("func %08x", SensorReceiveFunctionNumber);

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
          case MODULE_FUNCTION_SOURCE_C:
          case MODULE_FUNCTION_SOURCE_D:
          {   //Source
            printf("Source\n");
            int CurrentSource = AxumData.ModuleData[ModuleNr].TemporySource;

            if (type == MBN_DATATYPE_SINT)
            {
              CurrentSource = AdjustModuleSource(CurrentSource, data.SInt);
            }
            else if (type == MBN_DATATYPE_STATE)
            {
              CurrentSource = AxumData.ModuleData[ModuleNr].SelectedSource;

              if (data.State)
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
                  CurrentSource = AxumData.ModuleData[ModuleNr].SelectedSource;
                }
              }
            }

            if (FunctionNr != MODULE_FUNCTION_SOURCE)
            {
              SetNewSource(ModuleNr, CurrentSource, 0, 1);
            }
          }
          break;
          case MODULE_FUNCTION_SOURCE_PHANTOM:
          {
            printf("Source phantom\n");
            if (type == MBN_DATATYPE_STATE)
            {
              if ((AxumData.ModuleData[ModuleNr].SelectedSource >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].SelectedSource <= matrix_sources.src_offset.max.source))
              {
                unsigned int SourceNr = AxumData.ModuleData[ModuleNr].SelectedSource-matrix_sources.src_offset.min.source;
                if (data.State)
                {
                  AxumData.SourceData[SourceNr].Phantom = !AxumData.SourceData[SourceNr].Phantom;
                }
                else
                {
                  int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                  if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                  {
                    if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
                    {
                      AxumData.SourceData[SourceNr].Phantom = !AxumData.SourceData[SourceNr].Phantom;
                    }
                  }
                }

                unsigned int DisplayFunctionNr = 0x05000000 | (SourceNr<<12) | SOURCE_FUNCTION_PHANTOM;

                CheckObjectsToSent(DisplayFunctionNr);

                for (int cntModule=0; cntModule<128; cntModule++)
                {
                  if (AxumData.ModuleData[cntModule].SelectedSource == AxumData.ModuleData[ModuleNr].SelectedSource)
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
            if (type == MBN_DATATYPE_STATE)
            {
              if ((AxumData.ModuleData[ModuleNr].SelectedSource >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].SelectedSource <= matrix_sources.src_offset.max.source))
              {
                unsigned int SourceNr = AxumData.ModuleData[ModuleNr].SelectedSource-matrix_sources.src_offset.min.source;
                if (data.State)
                {
                  AxumData.SourceData[SourceNr].Pad = !AxumData.SourceData[SourceNr].Pad;
                }
                else
                {
                  int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                  if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                  {
                    if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
                    {
                      AxumData.SourceData[SourceNr].Pad = !AxumData.SourceData[SourceNr].Pad;
                    }
                  }
                }

                unsigned int DisplayFunctionNr = 0x05000000 | (SourceNr<<12) | SOURCE_FUNCTION_PAD;
                CheckObjectsToSent(DisplayFunctionNr);

                for (int cntModule=0; cntModule<128; cntModule++)
                {
                  if (AxumData.ModuleData[cntModule].SelectedSource == AxumData.ModuleData[ModuleNr].SelectedSource)
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
            if (type == MBN_DATATYPE_SINT)
            {
              if ((AxumData.ModuleData[ModuleNr].SelectedSource >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].SelectedSource <= matrix_sources.src_offset.max.source))
              {
                unsigned int SourceNr = AxumData.ModuleData[ModuleNr].SelectedSource-matrix_sources.src_offset.min.source;
                AxumData.SourceData[SourceNr].Gain += (float)data.SInt/10;
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
                  if (AxumData.ModuleData[cntModule].SelectedSource == AxumData.ModuleData[ModuleNr].SelectedSource)
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
            if (type == MBN_DATATYPE_STATE)
            {
              if ((AxumData.ModuleData[ModuleNr].SelectedSource >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].SelectedSource <= matrix_sources.src_offset.max.source))
              {
                unsigned int SourceNr = AxumData.ModuleData[ModuleNr].SelectedSource-matrix_sources.src_offset.min.source;
                if (data.State)
                {
                  AxumData.SourceData[SourceNr].Gain = 0;

                  unsigned int DisplayFunctionNr = 0x05000000 | (SourceNr<<12) | SOURCE_FUNCTION_GAIN;
                  CheckObjectsToSent(DisplayFunctionNr);

                  for (int cntModule=0; cntModule<128; cntModule++)
                  {
                    if (AxumData.ModuleData[cntModule].SelectedSource == AxumData.ModuleData[ModuleNr].SelectedSource)
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
            if (type == MBN_DATATYPE_STATE)
            {
              if (data.State)
              {
                AxumData.ModuleData[ModuleNr].InsertOnOff = !AxumData.ModuleData[ModuleNr].InsertOnOff;
              }
              else
              {
                int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                {
                  if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
                  {
                    AxumData.ModuleData[ModuleNr].InsertOnOff = !AxumData.ModuleData[ModuleNr].InsertOnOff;
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
            if (type == MBN_DATATYPE_STATE)
            {
              if (data.State)
              {
                AxumData.ModuleData[ModuleNr].PhaseReverse = !AxumData.ModuleData[ModuleNr].PhaseReverse;
              }
              else
              {
                int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                {
                  if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
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
            if (type == MBN_DATATYPE_SINT)
            {
              AxumData.ModuleData[ModuleNr].Gain += (float)data.SInt/10;
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
            else if (type == MBN_DATATYPE_UINT)
            {
              printf("Min:%f, Max:%f, Temp:%ld\n", DataMinimal, DataMaximal, data.UInt);

              AxumData.ModuleData[ModuleNr].Gain = (((float)40*(data.UInt-DataMinimal))/(DataMaximal-DataMinimal))-20;

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
            if (type == MBN_DATATYPE_STATE)
            {
              if (data.State)
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
            if (type == MBN_DATATYPE_SINT)
            {
              if (data.SInt>=0)
              {
                AxumData.ModuleData[ModuleNr].Filter.Frequency *= 1+((float)data.SInt/100);
              }
              else
              {
                AxumData.ModuleData[ModuleNr].Filter.Frequency /= 1+((float)-data.SInt/100);
              }

              if (AxumData.ModuleData[ModuleNr].Filter.Frequency <= 20)
              {
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
            if (type == MBN_DATATYPE_STATE)
            {
              if (data.State)
              {
                AxumData.ModuleData[ModuleNr].FilterOnOff = !AxumData.ModuleData[ModuleNr].FilterOnOff;
              }
              else
              {
                int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                {
                  if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
                  {
                    AxumData.ModuleData[ModuleNr].FilterOnOff = !AxumData.ModuleData[ModuleNr].FilterOnOff;
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
            if (type == MBN_DATATYPE_SINT)
            {
              AxumData.ModuleData[ModuleNr].EQBand[BandNr].Level += (float)data.SInt/10;
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
            if (type == MBN_DATATYPE_SINT)
            {
              if (data.SInt>=0)
              {
                AxumData.ModuleData[ModuleNr].EQBand[BandNr].Frequency *= 1+((float)data.SInt/100);
              }
              else
              {
                AxumData.ModuleData[ModuleNr].EQBand[BandNr].Frequency /= 1+((float)-data.SInt/100);
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
            if (type == MBN_DATATYPE_SINT)
            {
              AxumData.ModuleData[ModuleNr].EQBand[BandNr].Bandwidth += (float)data.SInt/10;

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
            if (type == MBN_DATATYPE_STATE)
            {
              if (data.State)
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
            if (type == MBN_DATATYPE_STATE)
            {
              if (data.State)
              {
                AxumData.ModuleData[ModuleNr].EQBand[BandNr].Frequency = AxumData.ModuleData[ModuleNr].Defaults.EQBand[BandNr].Frequency;

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
            if (type == MBN_DATATYPE_STATE)
            {
              if (data.State)
              {
                AxumData.ModuleData[ModuleNr].EQBand[BandNr].Bandwidth = AxumData.ModuleData[ModuleNr].Defaults.EQBand[BandNr].Bandwidth;

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
            if (type == MBN_DATATYPE_SINT)
            {
              int Type = AxumData.ModuleData[ModuleNr].EQBand[BandNr].Type;
              Type += data.SInt;

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
            if (type == MBN_DATATYPE_STATE)
            {
              if (data.State)
              {
                AxumData.ModuleData[ModuleNr].EQOnOff = !AxumData.ModuleData[ModuleNr].EQOnOff;
              }
              else
              {
                int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                {
                  if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
                  {
                    AxumData.ModuleData[ModuleNr].EQOnOff = !AxumData.ModuleData[ModuleNr].EQOnOff;
                  }
                }
              }

              for (int cntBand=0; cntBand<6; cntBand++)
              {
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
            if (type == MBN_DATATYPE_SINT)
            {
              AxumData.ModuleData[ModuleNr].Dynamics += data.SInt;
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
            if (type == MBN_DATATYPE_STATE)
            {
              if (data.State)
              {
                AxumData.ModuleData[ModuleNr].DynamicsOnOff = !AxumData.ModuleData[ModuleNr].DynamicsOnOff;
              }
              else
              {
                int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                {
                  if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
                  {
                    AxumData.ModuleData[ModuleNr].DynamicsOnOff = !AxumData.ModuleData[ModuleNr].DynamicsOnOff;
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
            if (type == MBN_DATATYPE_STATE)
            {
              if (data.State)
              {
                AxumData.ModuleData[ModuleNr].Mono = !AxumData.ModuleData[ModuleNr].Mono;
              }
              else
              {
                int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                {
                  if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
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
            if (type == MBN_DATATYPE_SINT)
            {
              AxumData.ModuleData[ModuleNr].Panorama += data.SInt;
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
            if (type == MBN_DATATYPE_STATE)
            {
              if (data.State)
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

            float CurrentLevel = AxumData.ModuleData[ModuleNr].FaderLevel;

            if (type == MBN_DATATYPE_UINT)
            {
              int Position = (data.SInt*1023)/(DataMaximal-DataMinimal);
              float dB = Position2dB[Position];
              dB -= AxumData.LevelReserve;

              AxumData.ModuleData[ModuleNr].FaderLevel = dB;
            }
            else if (type == MBN_DATATYPE_SINT)
            {
              AxumData.ModuleData[ModuleNr].FaderLevel += data.SInt;
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
            else if (type == MBN_DATATYPE_FLOAT)
            {
              AxumData.ModuleData[ModuleNr].FaderLevel += data.Float;
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

              if ((AxumData.ModuleData[ModuleNr].SelectedSource >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].SelectedSource <= matrix_sources.src_offset.max.source))
              {
                unsigned int SourceNr = AxumData.ModuleData[ModuleNr].SelectedSource - matrix_sources.src_offset.min.source;
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

            if (type == MBN_DATATYPE_STATE)
            {
              int CurrentOn = AxumData.ModuleData[ModuleNr].On;
              if (data.State)
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
                int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                {
                  if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
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

                if ((AxumData.ModuleData[ModuleNr].SelectedSource >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].SelectedSource <= matrix_sources.src_offset.max.source))
                {
                  unsigned int SourceNr = AxumData.ModuleData[ModuleNr].SelectedSource-matrix_sources.src_offset.min.source;
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
            if (type == MBN_DATATYPE_UINT)
            {
              int Position = ((data.UInt-DataMinimal)*1023)/(DataMaximal-DataMinimal);
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
            else if (type == MBN_DATATYPE_SINT)
            {
              AxumData.ModuleData[ModuleNr].Buss[BussNr].Level += data.SInt;
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
            else if (type == MBN_DATATYPE_FLOAT)
            {
              AxumData.ModuleData[ModuleNr].Buss[BussNr].Level = data.Float;
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
            if (type == MBN_DATATYPE_STATE)
            {
              if (data.State)
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
            unsigned char CallSetBussOnOff = 0;

            printf("Buss %d/%d on/off\n", (BussNr*2)+1, (BussNr*2)+2);
            if (type == MBN_DATATYPE_STATE)
            {
              if (data.State)
              {
                AxumData.ModuleData[ModuleNr].Buss[BussNr].On = !AxumData.ModuleData[ModuleNr].Buss[BussNr].On;
                CallSetBussOnOff = 1;
              }
              else
              {
                int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                {
                  if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
                  {
                    AxumData.ModuleData[ModuleNr].Buss[BussNr].On = !AxumData.ModuleData[ModuleNr].Buss[BussNr].On;
                    CallSetBussOnOff = 1;
                  }
                }
              }
              if (CallSetBussOnOff)
              {
                SetBussOnOff(ModuleNr, BussNr, 0);
              }
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
            if (type == MBN_DATATYPE_STATE)
            {
              if (data.State)
              {
                AxumData.ModuleData[ModuleNr].Buss[BussNr].PreModuleLevel = !AxumData.ModuleData[ModuleNr].Buss[BussNr].PreModuleLevel;
              }
              else
              {
                int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                {
                  if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
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

            if (type == MBN_DATATYPE_SINT)
            {
              AxumData.ModuleData[ModuleNr].Buss[BussNr].Balance += data.SInt;
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

            if (type == MBN_DATATYPE_STATE)
            {
              if (data.State)
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
            if (type == MBN_DATATYPE_STATE)
            {
              if ((AxumData.ModuleData[ModuleNr].SelectedSource >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].SelectedSource <= matrix_sources.src_offset.max.source))
              {
                unsigned int SourceNr = AxumData.ModuleData[ModuleNr].SelectedSource-matrix_sources.src_offset.min.source;
                char UpdateObjects = 0;

                if (data.State)
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
                  int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                  if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                  {
                    if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
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
                    if (AxumData.ModuleData[cntModule].SelectedSource == (SourceNr+matrix_sources.src_offset.min.source))
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
            if (type == MBN_DATATYPE_STATE)
            {
              AxumData.ModuleData[ModuleNr].Cough = data.State;
              SetAxum_BussLevels(ModuleNr);

              CheckObjectsToSent(SensorReceiveFunctionNumber);
              if ((AxumData.ModuleData[ModuleNr].SelectedSource >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].SelectedSource <= matrix_sources.src_offset.max.source))
              {
                unsigned int SourceNr = AxumData.ModuleData[ModuleNr].SelectedSource-matrix_sources.src_offset.min.source;
                SensorReceiveFunctionNumber = 0x05000000 | (SourceNr<<12);
                CheckObjectsToSent(SensorReceiveFunctionNumber+SOURCE_FUNCTION_MODULE_COUGH_ON_OFF);
              }
            }
          }
          break;
          case MODULE_FUNCTION_SOURCE_ALERT:
          { //Alert
            printf("Source alert\n");
            if (type == MBN_DATATYPE_STATE)
            {
              if ((AxumData.ModuleData[ModuleNr].SelectedSource >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].SelectedSource <= matrix_sources.src_offset.max.source))
              {
                unsigned int SourceNr = AxumData.ModuleData[ModuleNr].SelectedSource-matrix_sources.src_offset.min.source;

                if (data.State)
                {
                  AxumData.SourceData[SourceNr].Alert = !AxumData.SourceData[SourceNr].Alert;
                }
                else
                {
                  int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                  if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                  {
                    if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
                    {
                      AxumData.SourceData[SourceNr].Alert = !AxumData.SourceData[SourceNr].Alert;
                    }
                  }
                }

                unsigned int DisplayFunctionNr = 0x05000000 | (SourceNr<<12) | SOURCE_FUNCTION_ALERT;
                CheckObjectsToSent(DisplayFunctionNr);

                for (int cntModule=0; cntModule<128; cntModule++)
                {
                  if (AxumData.ModuleData[cntModule].SelectedSource == (SourceNr+matrix_sources.src_offset.min.source))
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
            ModeControllerSensorChange(SensorReceiveFunctionNumber, type, data, DataType, DataSize, DataMinimal, DataMaximal);
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
            ModeControllerResetSensorChange(SensorReceiveFunctionNumber, type, data, DataType, DataSize, DataMinimal, DataMaximal);
          }
          break;
          case MODULE_FUNCTION_ROUTING_PRESET_1:
          case MODULE_FUNCTION_ROUTING_PRESET_2:
          case MODULE_FUNCTION_ROUTING_PRESET_3:
          case MODULE_FUNCTION_ROUTING_PRESET_4:
          case MODULE_FUNCTION_ROUTING_PRESET_5:
          case MODULE_FUNCTION_ROUTING_PRESET_6:
          case MODULE_FUNCTION_ROUTING_PRESET_7:
          case MODULE_FUNCTION_ROUTING_PRESET_8:
          {
            unsigned char PresetNr = FunctionNr - MODULE_FUNCTION_ROUTING_PRESET_1;
            bool SourceActive = false;

            if (AxumData.ModuleData[ModuleNr].On)
            {
              if (AxumData.ModuleData[ModuleNr].FaderLevel>-80)
              {
                SourceActive = 1;
              }
            }
            if (!SourceActive)
            {
              if (type == MBN_DATATYPE_STATE)
              {
                if (data.State)
                {
                  //only set differences
                  LoadRoutingPreset(ModuleNr, PresetNr, 0);
                }
              }
            }
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
            if (type == MBN_DATATYPE_UINT)
            {
              int Position = ((data.UInt-DataMinimal)*1023)/(DataMaximal-DataMinimal);
              AxumData.BussMasterData[BussNr].Level = Position2dB[Position];
              AxumData.BussMasterData[BussNr].Level -= 10;//AxumData.LevelReserve;
            }
            else if (type == MBN_DATATYPE_SINT)
            {
              AxumData.BussMasterData[BussNr].Level += data.SInt;
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
            else if (type == MBN_DATATYPE_FLOAT)
            {
              AxumData.BussMasterData[BussNr].Level = data.Float;
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
            if (type == MBN_DATATYPE_STATE)
            {
              if (data.State)
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
            if (type == MBN_DATATYPE_STATE)
            {
              if (data.State)
              {
                AxumData.BussMasterData[BussNr].On = !AxumData.BussMasterData[BussNr].On;
              }
              else
              {
                int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                {
                  if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
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
            if (type == MBN_DATATYPE_STATE)
            {
              if (data.State)
              {
                AxumData.BussMasterData[BussNr].PreModuleLevel = !AxumData.BussMasterData[BussNr].PreModuleLevel;
              }
              else
              {
                int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                {
                  if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
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
            if (type == MBN_DATATYPE_STATE)
            {
              bool *MonitorSwitchState;

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
                if (data.State)
                {
                  *MonitorSwitchState = !*MonitorSwitchState;
                }
                else
                {
                  int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                  if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                  {
                    if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
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
              if (type == MBN_DATATYPE_STATE)
              {
                if (data.State)
                {
                  AxumData.Monitor[MonitorBussNr].Mute = !AxumData.Monitor[MonitorBussNr].Mute;
                }
                else
                {
                  int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                  if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                  {
                    if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
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
              if (type == MBN_DATATYPE_STATE)
              {
                if (data.State)
                {
                  AxumData.Monitor[MonitorBussNr].Dim = !AxumData.Monitor[MonitorBussNr].Dim;
                }
                else
                {
                  int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                  if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                  {
                    if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
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
              if (type == MBN_DATATYPE_UINT)
              {
                int Position = (data.UInt*1023)/(DataMaximal-DataMinimal);
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
              else if (type == MBN_DATATYPE_SINT)
              {
                AxumData.Monitor[MonitorBussNr].PhonesLevel += (float)data.SInt/10;
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
              if (type == MBN_DATATYPE_STATE)
              {
                if (data.State)
                {
                  AxumData.Monitor[MonitorBussNr].Mono = !AxumData.Monitor[MonitorBussNr].Mono;
                }
                else
                {
                  int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                  if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                  {
                    if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
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
              if (type == MBN_DATATYPE_STATE)
              {
                if (data.State)
                {
                  AxumData.Monitor[MonitorBussNr].Phase = !AxumData.Monitor[MonitorBussNr].Phase;
                }
                else
                {
                  int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                  if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                  {
                    if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
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
              if (type == MBN_DATATYPE_UINT)
              {
                int Position = (data.UInt*1023)/(DataMaximal-DataMinimal);
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
              else if (type == MBN_DATATYPE_SINT)
              {
                AxumData.Monitor[MonitorBussNr].SpeakerLevel += (float)data.SInt/10;
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
              if (type == MBN_DATATYPE_STATE)
              {
                if (data.State)
                {
                  AxumData.Monitor[MonitorBussNr].Talkback[TalkbackNr] = !AxumData.Monitor[MonitorBussNr].Talkback[TalkbackNr];
                }
                else
                {
                  int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                  if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                  {
                    if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
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
              if (type == MBN_DATATYPE_STATE)
              {
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

                    if (data.State)
                    {
                      AxumData.Redlight[FunctionNr-GLOBAL_FUNCTION_REDLIGHT_1] = !AxumData.Redlight[FunctionNr-GLOBAL_FUNCTION_REDLIGHT_1];
                    }
                    else
                    {
                      int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                      if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                      {
                        if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
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

                    if (data.State)
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
              if (type == MBN_DATATYPE_STATE)
              {
                int NewControl1Mode = -2;
                if (data.State)
                {
                  NewControl1Mode = FunctionNr-GLOBAL_FUNCTION_CONTROL_1_MODE_SOURCE;
                }
                else
                {
                  int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                  if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                  {
                    if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
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
              if (type == MBN_DATATYPE_STATE)
              {
                int NewControl2Mode = -2;
                if (data.State)
                {
                  NewControl2Mode = FunctionNr-GLOBAL_FUNCTION_CONTROL_2_MODE_SOURCE;
                }
                else
                {
                  int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                  if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                  {
                    if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
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
              if (type == MBN_DATATYPE_STATE)
              {
                int NewControl3Mode = -2;
                if (data.State)
                {
                  NewControl3Mode = FunctionNr-GLOBAL_FUNCTION_CONTROL_3_MODE_SOURCE;
                }
                else
                {
                  int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                  if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                  {
                    if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
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
              if (type == MBN_DATATYPE_STATE)
              {
                int NewControl4Mode = -2;
                if (data.State)
                {
                  NewControl4Mode = FunctionNr-GLOBAL_FUNCTION_CONTROL_4_MODE_SOURCE;
                }
                else
                {
                  int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                  if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                  {
                    if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
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
              if (type == MBN_DATATYPE_STATE)
              {
                if (data.State)
                {
                  char NewMasterControl1Mode = FunctionNr-GLOBAL_FUNCTION_MASTER_CONTROL_1_MODE_BUSS_1_2;
                  unsigned int OldFunctionNumber = 0x00000000;

                  if (AxumData.MasterControl1Mode != MASTER_CONTROL_MODE_NONE)
                  {
                    OldFunctionNumber = 0x04000000 | (AxumData.MasterControl1Mode+GLOBAL_FUNCTION_MASTER_CONTROL_1_MODE_BUSS_1_2);
                  }

                  if (AxumData.MasterControl1Mode != NewMasterControl1Mode)
                  {
                    AxumData.MasterControl1Mode = NewMasterControl1Mode;
                  }
                  else
                  {
                    AxumData.MasterControl1Mode = MASTER_CONTROL_MODE_NONE;
                  }
                  if (OldFunctionNumber != 0x00000000)
                  {
                    CheckObjectsToSent(OldFunctionNumber);
                  }
                  CheckObjectsToSent(SensorReceiveFunctionNumber);
                  CheckObjectsToSent(0x04000000 | GLOBAL_FUNCTION_MASTER_CONTROL_1);
                }
              }
            }
            else if ((FunctionNr>=GLOBAL_FUNCTION_MASTER_CONTROL_2_MODE_BUSS_1_2) && (FunctionNr<GLOBAL_FUNCTION_MASTER_CONTROL_3_MODE_BUSS_1_2))
            { //Master control 2 mode
              printf("Master control 2 mode\n");
              if (type == MBN_DATATYPE_STATE)
              {
                if (data.State)
                {
                  char NewMasterControl2Mode = FunctionNr-GLOBAL_FUNCTION_MASTER_CONTROL_2_MODE_BUSS_1_2;
                  unsigned int OldFunctionNumber = 0x00000000;

                  if (AxumData.MasterControl2Mode != MASTER_CONTROL_MODE_NONE)
                  {
                    OldFunctionNumber = 0x04000000 | (AxumData.MasterControl2Mode+GLOBAL_FUNCTION_MASTER_CONTROL_2_MODE_BUSS_1_2);
                  }

                  if (AxumData.MasterControl2Mode != NewMasterControl2Mode)
                  {
                    AxumData.MasterControl2Mode = NewMasterControl2Mode;
                  }
                  else
                  {
                    AxumData.MasterControl2Mode = MASTER_CONTROL_MODE_NONE;
                  }
                  if (OldFunctionNumber != 0x00000000)
                  {
                    CheckObjectsToSent(OldFunctionNumber);
                  }
                  CheckObjectsToSent(SensorReceiveFunctionNumber);
                  CheckObjectsToSent(0x04000000 | GLOBAL_FUNCTION_MASTER_CONTROL_2);
                }
              }
            }
            else if ((FunctionNr>=GLOBAL_FUNCTION_MASTER_CONTROL_3_MODE_BUSS_1_2) && (FunctionNr<GLOBAL_FUNCTION_MASTER_CONTROL_4_MODE_BUSS_1_2))
            { //Master control 3 mode
              printf("Master control 3 mode\n");
              if (type == MBN_DATATYPE_STATE)
              {
                if (data.State)
                {
                  char NewMasterControl3Mode = FunctionNr-GLOBAL_FUNCTION_MASTER_CONTROL_3_MODE_BUSS_1_2;
                  unsigned int OldFunctionNumber = 0x00000000;

                  if (AxumData.MasterControl3Mode != MASTER_CONTROL_MODE_NONE)
                  {
                    OldFunctionNumber = 0x04000000 | (AxumData.MasterControl3Mode+GLOBAL_FUNCTION_MASTER_CONTROL_3_MODE_BUSS_1_2);
                  }

                  if (AxumData.MasterControl3Mode != NewMasterControl3Mode)
                  {
                    AxumData.MasterControl3Mode = NewMasterControl3Mode;
                  }
                  else
                  {
                    AxumData.MasterControl3Mode = MASTER_CONTROL_MODE_NONE;
                  }
                  if (OldFunctionNumber != 0x00000000)
                  {
                    CheckObjectsToSent(OldFunctionNumber);
                  }
                  CheckObjectsToSent(SensorReceiveFunctionNumber);
                  CheckObjectsToSent(0x04000000 | GLOBAL_FUNCTION_MASTER_CONTROL_3);
                }
              }
            }
            else if ((FunctionNr>=GLOBAL_FUNCTION_MASTER_CONTROL_4_MODE_BUSS_1_2) && (FunctionNr<GLOBAL_FUNCTION_MASTER_CONTROL_1))
            { //Master control 4 mode
              printf("Master control 4 mode\n");
              if (type == MBN_DATATYPE_STATE)
              {
                if (data.State)
                {
                  char NewMasterControl4Mode = FunctionNr-GLOBAL_FUNCTION_MASTER_CONTROL_4_MODE_BUSS_1_2;
                  unsigned int OldFunctionNumber = 0x00000000;

                  if (AxumData.MasterControl4Mode != MASTER_CONTROL_MODE_NONE)
                  {
                    OldFunctionNumber = 0x04000000 | (AxumData.MasterControl4Mode+GLOBAL_FUNCTION_MASTER_CONTROL_4_MODE_BUSS_1_2);
                  }

                  if (AxumData.MasterControl4Mode != NewMasterControl4Mode)
                  {
                    AxumData.MasterControl4Mode = NewMasterControl4Mode;
                  }
                  else
                  {
                    AxumData.MasterControl4Mode = MASTER_CONTROL_MODE_NONE;
                  }
                  if (OldFunctionNumber != 0x00000000)
                  {
                    CheckObjectsToSent(OldFunctionNumber);
                  }
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
                MasterModeControllerSensorChange(SensorReceiveFunctionNumber, type, data, DataType, DataSize, DataMinimal, DataMaximal);
              }
              break;
              case GLOBAL_FUNCTION_MASTER_CONTROL_1_RESET:
              case GLOBAL_FUNCTION_MASTER_CONTROL_2_RESET:
              case GLOBAL_FUNCTION_MASTER_CONTROL_3_RESET:
              case GLOBAL_FUNCTION_MASTER_CONTROL_4_RESET:
              {
                MasterModeControllerResetSensorChange(SensorReceiveFunctionNumber, type, data, DataType, DataSize, DataMinimal, DataMaximal);
              }
              break;
              case GLOBAL_FUNCTION_MODULE_TO_DEFAULTS:
              {
                for (int cntModule=0; cntModule<128; cntModule++)
                {
                  //Only set differences
                  LoadSourcePreset(cntModule, false);
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

              if (type == MBN_DATATYPE_STATE)
              {
                for (int cntModule=0; cntModule<128; cntModule++)
                {
                  if (AxumData.ModuleData[cntModule].SelectedSource == (SourceNr+matrix_sources.src_offset.min.source))
                  {
                    int CurrentOn = AxumData.ModuleData[cntModule].On;

                    if (data.State)
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
                      int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                      if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                      {
                        if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
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

              if (type == MBN_DATATYPE_STATE)
              {
                for (int cntModule=0; cntModule<128; cntModule++)
                {
                  if (AxumData.ModuleData[cntModule].SelectedSource == (SourceNr+matrix_sources.src_offset.min.source))
                  {
                    float CurrentLevel = AxumData.ModuleData[cntModule].FaderLevel;
                    if (data.State)
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
                      int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                      if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                      {
                        if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
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

              if (type == MBN_DATATYPE_STATE)
              {
                for (int cntModule=0; cntModule<128; cntModule++)
                {
                  if (AxumData.ModuleData[cntModule].SelectedSource == (SourceNr+matrix_sources.src_offset.min.source))
                  {
                    float CurrentLevel = AxumData.ModuleData[cntModule].FaderLevel;
                    float CurrentOn = AxumData.ModuleData[cntModule].On;
                    if (data.State)
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
                      int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                      if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                      {
                        if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
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

              if (type == MBN_DATATYPE_STATE)
              {
                if (data.State)
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

                        if ((AxumData.ModuleData[cntModule].SelectedSource >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[cntModule].SelectedSource <= matrix_sources.src_offset.max.source))
                        {
                          int SourceNr = AxumData.ModuleData[cntModule].SelectedSource-matrix_sources.src_offset.min.source;
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
                    if (AxumData.ModuleData[cntModule].SelectedSource == (SourceNr+matrix_sources.src_offset.min.source))
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

              if (type == MBN_DATATYPE_STATE)
              {
                if (data.State)
                {
                  unsigned int FunctionNrToSent;

                  for (int cntModule=0; cntModule<128; cntModule++)
                  {
                    if (AxumData.ModuleData[cntModule].SelectedSource == (SourceNr+matrix_sources.src_offset.min.source))
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

              if (type == MBN_DATATYPE_STATE)
              {
                unsigned int FunctionNrToSent;

                for (int cntModule=0; cntModule<128; cntModule++)
                {
                  unsigned char CallSetBussOnOff = 0;
                  if (AxumData.ModuleData[cntModule].SelectedSource == (SourceNr+matrix_sources.src_offset.min.source))
                  {
                    if (data.State)
                    {
                      AxumData.ModuleData[cntModule].Buss[BussNr].On = !AxumData.ModuleData[cntModule].Buss[BussNr].On;
                      CallSetBussOnOff = 1;
                    }
                    else
                    {
                      int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                      if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                      {
                        if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
                        {
                          AxumData.ModuleData[cntModule].Buss[BussNr].On = !AxumData.ModuleData[cntModule].Buss[BussNr].On;
                          CallSetBussOnOff = 1;
                        }
                      }
                    }

                    if (CallSetBussOnOff)
                    {
                      SetBussOnOff(cntModule, BussNr, 0);
                    }
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

              if (type == MBN_DATATYPE_STATE)
              {
                for (int cntModule=0; cntModule<128; cntModule++)
                {
                  if (AxumData.ModuleData[cntModule].SelectedSource == (SourceNr+matrix_sources.src_offset.min.source))
                  {
                    AxumData.ModuleData[cntModule].Cough = data.State;

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
              if (type == MBN_DATATYPE_STATE)
              {
                char UpdateObjects = 0;

                if (data.State)
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
                  int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                  if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                  {
                    if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
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
                    if (AxumData.ModuleData[cntModule].SelectedSource == (SourceNr+matrix_sources.src_offset.min.source))
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
              if (type == MBN_DATATYPE_STATE)
              {
                if (data.State)
                {
                  AxumData.SourceData[SourceNr].Phantom = !AxumData.SourceData[SourceNr].Phantom;
                }
                else
                {
                  int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                  if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                  {
                    if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
                    {
                      AxumData.SourceData[SourceNr].Phantom = !AxumData.SourceData[SourceNr].Phantom;
                    }
                  }
                }

                unsigned int DisplayFunctionNr = 0x05000000 | (SourceNr<<12) | SOURCE_FUNCTION_PHANTOM;
                CheckObjectsToSent(DisplayFunctionNr);

                for (int cntModule=0; cntModule<128; cntModule++)
                {
                  if (AxumData.ModuleData[cntModule].SelectedSource == (SourceNr+matrix_sources.src_offset.min.source))
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
              if (type == MBN_DATATYPE_STATE)
              {
                if (data.State)
                {
                  AxumData.SourceData[SourceNr].Pad = !AxumData.SourceData[SourceNr].Pad;
                }
                else
                {
                  int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                  if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                  {
                    if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
                    {
                      AxumData.SourceData[SourceNr].Pad = !AxumData.SourceData[SourceNr].Pad;
                    }
                  }
                }

                unsigned int DisplayFunctionNr = 0x05000000 | (SourceNr<<12) | SOURCE_FUNCTION_PAD;
                CheckObjectsToSent(DisplayFunctionNr);

                for (int cntModule=0; cntModule<128; cntModule++)
                {
                  if (AxumData.ModuleData[cntModule].SelectedSource == (SourceNr+matrix_sources.src_offset.min.source))
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
              if (type == MBN_DATATYPE_SINT)
              {
                AxumData.SourceData[SourceNr].Gain += (float)data.SInt/10;
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
                  if (AxumData.ModuleData[cntModule].SelectedSource == (SourceNr+matrix_sources.src_offset.min.source))
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
              if (type == MBN_DATATYPE_STATE)
              {
                if (data.State)
                {
                  AxumData.SourceData[SourceNr].Alert = !AxumData.SourceData[SourceNr].Alert;
                }
                else
                {
                  int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                  if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                  {
                    if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
                    {
                      AxumData.SourceData[SourceNr].Alert = !AxumData.SourceData[SourceNr].Alert;
                    }
                  }
                }

                unsigned int DisplayFunctionNr = 0x05000000 | (SourceNr<<12) | SOURCE_FUNCTION_ALERT;
                CheckObjectsToSent(DisplayFunctionNr);

                for (int cntModule=0; cntModule<128; cntModule++)
                {
                  if (AxumData.ModuleData[cntModule].SelectedSource == (SourceNr+matrix_sources.src_offset.min.source))
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
              if (type == MBN_DATATYPE_SINT)
              {
                AxumData.DestinationData[DestinationNr].Source = (int)AdjustDestinationSource(AxumData.DestinationData[DestinationNr].Source, data.SInt);

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
              if (type == MBN_DATATYPE_UINT)
              {
                int Position = (data.UInt*1023)/(DataMaximal-DataMinimal);
                AxumData.DestinationData[DestinationNr].Level = Position2dB[Position];
                //AxumData.DestinationData[DestinationNr].Level -= AxumData.LevelReserve;

                CheckObjectsToSent(SensorReceiveFunctionNumber);
              }
              else if (type == MBN_DATATYPE_SINT)
              {
                AxumData.DestinationData[DestinationNr].Level += (float)data.SInt/10;
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
              else if (type == MBN_DATATYPE_FLOAT)
              {
                AxumData.DestinationData[DestinationNr].Level = data.Float;
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

              if (type == MBN_DATATYPE_STATE)
              {
                if (data.State)
                {
                  AxumData.DestinationData[DestinationNr].Mute = !AxumData.DestinationData[DestinationNr].Mute;
                }
                else
                {
                  int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                  if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                  {
                    if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
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

              if (type == MBN_DATATYPE_STATE)
              {
                if (data.State)
                {
                  AxumData.DestinationData[DestinationNr].Dim = !AxumData.DestinationData[DestinationNr].Dim;
                }
                else
                {
                  int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                  if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                  {
                    if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
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

              if (type == MBN_DATATYPE_STATE)
              {
                if (data.State)
                {
                  AxumData.DestinationData[DestinationNr].Mono = !AxumData.DestinationData[DestinationNr].Mono;
                }
                else
                {
                  int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                  if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                  {
                    if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
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

              if (type == MBN_DATATYPE_STATE)
              {
                if (data.State)
                {
                  AxumData.DestinationData[DestinationNr].Phase = !AxumData.DestinationData[DestinationNr].Phase;
                }
                else
                {
                  int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                  if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                  {
                    if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
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

              if (type == MBN_DATATYPE_STATE)
              {
                if (data.State)
                {
                  AxumData.DestinationData[DestinationNr].Talkback[TalkbackNr] = !AxumData.DestinationData[DestinationNr].Talkback[TalkbackNr];
                }
                else
                {
                  int delay_time = (SensorReceiveFunction->LastChangedTime-SensorReceiveFunction->PreviousLastChangedTime)*10;
                  if (SensorReceiveFunction->TimeBeforeMomentary>=0)
                  {
                    if (delay_time>=SensorReceiveFunction->TimeBeforeMomentary)
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

    switch (type)
    {
      case MBN_DATATYPE_NODATA:
      {
        printf(" - No data\n");
      }
      break;
      case MBN_DATATYPE_UINT:
      {
        printf(" - unsigned int: %ld\n", data.UInt);
      }
      break;
      case MBN_DATATYPE_SINT:
      {
        printf(" - signed int: %ld\n", data.SInt);
      }
      break;
      case MBN_DATATYPE_STATE:
      {
        printf(" - State: %ld\n", data.State);
      }
      break;
      case MBN_DATATYPE_OCTETS:
      {
        printf("- Octet string: %s\n", data.Octets);
      }
      break;
      case MBN_DATATYPE_FLOAT:
      {
        printf("- float: %f\n", data.Float);
      }
      break;
      case MBN_DATATYPE_BITS:
      {
        printf("- Bit string:\n");
      }
      break;
    }
    SensorReceiveFunction->PreviousLastChangedTime = SensorReceiveFunction->LastChangedTime;
  }
  return 0;

  mbn=NULL;
  message=NULL;
  object = 0;
  type = 0;
  data.State = 0;
}

//normally response on GetSensorData
int mSensorDataResponse(struct mbn_handler *mbn, struct mbn_message *message, short unsigned int object, unsigned char type, union mbn_data data)
{
  printf("SensorDataResponse addr: %08lx, object: %d\n", message->AddressFrom, object);

  lock_node_info(__FUNCTION__);

  ONLINE_NODE_INFORMATION_STRUCT *OnlineNodeInformationElement = GetOnlineNodeInformation(message->AddressFrom);

  if (OnlineNodeInformationElement == NULL)
  {
    unlock_node_info(__FUNCTION__);
    return 1;
  }

  switch (type)
  {
    case MBN_DATATYPE_UINT:
    {
      printf(" - unsigned int: %ld\n", data.UInt);

      if (object == 7)
      {   //Major firmware id
        if ((OnlineNodeInformationElement->ManufacturerID == 0x0001) &&
            (OnlineNodeInformationElement->ProductID == 0x000C) &&
            (data.UInt == 1))
        {   //Backplane must have major version 1
          if ((OnlineNodeInformationElement->ManufacturerID == this_node.HardwareParent[0]) &&
              (OnlineNodeInformationElement->ProductID == this_node.HardwareParent[1]) &&
              (OnlineNodeInformationElement->UniqueIDPerProduct == this_node.HardwareParent[2]))
          {
            printf("2 - Backplane %08x\n", OnlineNodeInformationElement->MambaNetAddress);

            if (BackplaneMambaNetAddress != OnlineNodeInformationElement->MambaNetAddress)
            { //Initialize all routing
              BackplaneMambaNetAddress = OnlineNodeInformationElement->MambaNetAddress;

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
                  unsigned int ObjectNumber = 1026+dsp_handler->dspcard[cntDSPCard].slot;
                  mbn_data data;

                  data.State = 1;//enabled
                  mbnSetActuatorData(mbn, BackplaneMambaNetAddress, ObjectNumber, MBN_DATATYPE_STATE, 1, data ,1);
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
        if (OnlineNodeInformationElement->FirmwareMajorRevision == -1)
        {
          OnlineNodeInformationElement->FirmwareMajorRevision = data.UInt;

          db_lock(1);
          db_read_template_info(OnlineNodeInformationElement, 1);

          if (OnlineNodeInformationElement->SlotNumberObjectNr != -1)
          {
            mbnGetSensorData(mbn, OnlineNodeInformationElement->MambaNetAddress, OnlineNodeInformationElement->SlotNumberObjectNr, 1);
          }

          if (OnlineNodeInformationElement->NumberOfCustomObjects>0)
          {
            db_read_node_defaults(OnlineNodeInformationElement, 1024, OnlineNodeInformationElement->NumberOfCustomObjects+1023, 0);
            db_read_node_config(OnlineNodeInformationElement, 1024, OnlineNodeInformationElement->NumberOfCustomObjects+1023);
          }
          db_lock(0);
        }
      }
      if ((object>=1024) && (((signed int)object) == OnlineNodeInformationElement->SlotNumberObjectNr))
      {
        db_lock(1);
        for (unsigned char cntSlot=0; cntSlot<42; cntSlot++)
        {
          if (cntSlot != data.UInt)
          { //other slot then current inserted
            if (AxumData.RackOrganization[cntSlot] == message->AddressFrom)
            {
              AxumData.RackOrganization[cntSlot] = 0;

              db_delete_slot_config(cntSlot);
            }
          }
        }
        if (AxumData.RackOrganization[data.UInt] != message->AddressFrom)
        {
          AxumData.RackOrganization[data.UInt] = message->AddressFrom;

          log_write("0x%08lX found at slot: %d", message->AddressFrom, data.UInt+1);
          db_insert_slot_config(data.UInt, message->AddressFrom, 0, 0);
        }
        db_lock(0);

        //if a slot number exists, check the number of input/output channels.
        if (OnlineNodeInformationElement->InputChannelCountObjectNr != -1)
        {
          printf("Get Input channel count @ ObjectNr: %d\n", OnlineNodeInformationElement->InputChannelCountObjectNr);
          mbnGetSensorData(mbn, OnlineNodeInformationElement->MambaNetAddress, OnlineNodeInformationElement->InputChannelCountObjectNr, 1);
        }
        if (OnlineNodeInformationElement->OutputChannelCountObjectNr != -1)
        {
          printf("Get Output channel count @ ObjectNr: %d\n", OnlineNodeInformationElement->OutputChannelCountObjectNr);
          mbnGetSensorData(mbn, OnlineNodeInformationElement->MambaNetAddress, OnlineNodeInformationElement->OutputChannelCountObjectNr, 1);
        }

        //Check for source if I/O card changed.
        for (unsigned int cntSource=0; cntSource<1280; cntSource++)
        {
          if (  (AxumData.SourceData[cntSource].InputData[0].MambaNetAddress == message->AddressFrom) ||
                (AxumData.SourceData[cntSource].InputData[1].MambaNetAddress == message->AddressFrom))
          { //this source is changed, update modules!
            printf("Found source: %d\n", cntSource);

            //Set Phantom, Pad and Gain
            unsigned int FunctionNrToSend = 0x05000000 | (cntSource<<12);
            CheckObjectsToSent(FunctionNrToSend | SOURCE_FUNCTION_PHANTOM);
            CheckObjectsToSent(FunctionNrToSend | SOURCE_FUNCTION_PAD);
            CheckObjectsToSent(FunctionNrToSend | SOURCE_FUNCTION_GAIN);

            for (int cntModule=0; cntModule<128; cntModule++)
            {
              if (AxumData.ModuleData[cntModule].SelectedSource == (cntSource+matrix_sources.src_offset.min.source))
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
          if (  (AxumData.DestinationData[cntDestination].OutputData[0].MambaNetAddress == message->AddressFrom) ||
                (AxumData.DestinationData[cntDestination].OutputData[1].MambaNetAddress == message->AddressFrom))
          { //this source is changed, update modules!
            printf("Found destination: %d\n", cntDestination);
            SetAxum_DestinationSource(cntDestination);

            unsigned int FunctionNrToSend = 0x06000000 | (cntDestination<<12);
            CheckObjectsToSent(FunctionNrToSend | DESTINATION_FUNCTION_LEVEL);
          }
        }
      }
      else if (OnlineNodeInformationElement->SlotNumberObjectNr != -1)
      {
        printf("Check for Channel Counts: %d, %d\n", OnlineNodeInformationElement->InputChannelCountObjectNr, OnlineNodeInformationElement->OutputChannelCountObjectNr);
        if (((signed int)object) == OnlineNodeInformationElement->InputChannelCountObjectNr)
        {
          db_lock(1);
          db_update_slot_config_input_ch_cnt(message->AddressFrom, data.UInt);
          db_lock(0);
        }
        else if (((signed int)object) == OnlineNodeInformationElement->OutputChannelCountObjectNr)
        {
          db_lock(1);
          db_update_slot_config_output_ch_cnt(message->AddressFrom, data.UInt);
          db_lock(0);
        }
      }
    }
    break;
  }
  unlock_node_info(__FUNCTION__);

  return 0;
}

void mAddressTableChange(struct mbn_handler *mbn, struct mbn_address_node *old_info, struct mbn_address_node *new_info)
{
  lock_node_info(__FUNCTION__);
  if (old_info == NULL)
  {
    log_write("Add node with MambaNet address: %08lX, Services: %02X", new_info->MambaNetAddr, new_info->Services);
    ONLINE_NODE_INFORMATION_STRUCT *NewOnlineNodeInformationElement = new ONLINE_NODE_INFORMATION_STRUCT;
    NewOnlineNodeInformationElement->Next = NULL;
    NewOnlineNodeInformationElement->MambaNetAddress = new_info->MambaNetAddr;
    NewOnlineNodeInformationElement->ManufacturerID = new_info->ManufacturerID;
    NewOnlineNodeInformationElement->ProductID = new_info->ProductID;
    NewOnlineNodeInformationElement->UniqueIDPerProduct = new_info->UniqueIDPerProduct;
    NewOnlineNodeInformationElement->FirmwareMajorRevision  = -1;
    NewOnlineNodeInformationElement->SlotNumberObjectNr = -1;
    NewOnlineNodeInformationElement->InputChannelCountObjectNr = -1;
    NewOnlineNodeInformationElement->OutputChannelCountObjectNr = -1;
    NewOnlineNodeInformationElement->Parent.ManufacturerID = 0;
    NewOnlineNodeInformationElement->Parent.ProductID = 0;
    NewOnlineNodeInformationElement->Parent.UniqueIDPerProduct = 0;
    NewOnlineNodeInformationElement->NumberOfCustomObjects = 0;
    NewOnlineNodeInformationElement->SensorReceiveFunction = NULL;
    NewOnlineNodeInformationElement->ObjectInformation = NULL;


    if (OnlineNodeInformationList == NULL)
    {
      OnlineNodeInformationList = NewOnlineNodeInformationElement;
    }
    else
    {
      ONLINE_NODE_INFORMATION_STRUCT *OnlineNodeInformationElement = OnlineNodeInformationList;
      while (OnlineNodeInformationElement->Next != NULL)
      {
        OnlineNodeInformationElement = OnlineNodeInformationElement->Next;
      }
      OnlineNodeInformationElement->Next = NewOnlineNodeInformationElement;
    }

    if (mbn->node.Services&0x80)
    {
      unsigned int ObjectNr = 7;//Firmware major revision;
      mbnGetSensorData(mbn, new_info->MambaNetAddr, ObjectNr, 1);
      log_write("Get firmware: %08lX", new_info->MambaNetAddr);
    }
  }
  else if (new_info == NULL)
  {
    if (OnlineNodeInformationList == NULL)
    {
      log_write("Error removing NodeInformationElement of address 0x%08lX", old_info->MambaNetAddr);
    }
    else
    {
      bool removed = false;
      ONLINE_NODE_INFORMATION_STRUCT *OnlineNodeInformationElement = OnlineNodeInformationList;
      ONLINE_NODE_INFORMATION_STRUCT *PreviousOnlineNodeInformationElement = NULL;

      while ((!removed) && (OnlineNodeInformationElement != NULL))
      {
        if (OnlineNodeInformationElement->MambaNetAddress == old_info->MambaNetAddr)
        {
          if (OnlineNodeInformationElement == OnlineNodeInformationList)
          {
            OnlineNodeInformationList = OnlineNodeInformationElement->Next;
          }
          else if (PreviousOnlineNodeInformationElement != NULL)
          {
            PreviousOnlineNodeInformationElement->Next = OnlineNodeInformationElement->Next;
          }

          if (OnlineNodeInformationElement->SensorReceiveFunction != NULL)
          {
            delete[] OnlineNodeInformationElement->SensorReceiveFunction;
          }
          if (OnlineNodeInformationElement->ObjectInformation != NULL)
          {
            delete[] OnlineNodeInformationElement->ObjectInformation;
          }

          delete OnlineNodeInformationElement;
          removed = true;
        }
        else
        {
          PreviousOnlineNodeInformationElement = OnlineNodeInformationElement;
          OnlineNodeInformationElement = OnlineNodeInformationElement->Next;
        }
      }
    }
    log_write("Removed node with MambaNet address: %08lX", old_info->MambaNetAddr);
  }
  else
  {
    bool Found = false;
    ONLINE_NODE_INFORMATION_STRUCT *OnlineNodeInformationElement = OnlineNodeInformationList;
    ONLINE_NODE_INFORMATION_STRUCT *FoundOnlineNodeInformationElement = NULL;

    while ((!Found) && (OnlineNodeInformationElement != NULL))
    {
      if (OnlineNodeInformationElement->MambaNetAddress == old_info->MambaNetAddr)
      {
        Found = true;
        FoundOnlineNodeInformationElement = OnlineNodeInformationElement;
      }
      OnlineNodeInformationElement = OnlineNodeInformationElement->Next;
    }

    if (FoundOnlineNodeInformationElement != NULL)
    {
      log_write("change OnlineNodeInformation");
    }
    log_write("Address changed: %08lX => %08lX", old_info->MambaNetAddr, new_info->MambaNetAddr);
  }
  unlock_node_info(__FUNCTION__);
}

void mError(struct mbn_handler *m, int code, char *str) {
  log_write("MambaNet Error: %s (%d)", str, code);
  m=NULL;
}

void mAcknowledgeTimeout(struct mbn_handler *m, struct mbn_message *msg) {
  log_write("Acknowledge timeout for message to %08lX, obj: %d", msg->AddressTo, msg->Message.Object.Number);
  m=NULL;
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

    //Check for firmware requests
    //lock_node_info(__FUNCTION__);
    ONLINE_NODE_INFORMATION_STRUCT *OnlineNodeInformationElement = OnlineNodeInformationList;
    while (OnlineNodeInformationElement != NULL)
    {
      if ((OnlineNodeInformationElement->MambaNetAddress != 0x00000000) &&
          (OnlineNodeInformationElement->FirmwareMajorRevision == -1))
      { //Read firmware...
        if (mbn->node.Services&0x80)
        {
          unsigned int ObjectNr = 7; //Firmware major revision
          printf("timer: Get firmware 0x%08X\n", OnlineNodeInformationElement->MambaNetAddress);
          mbnGetSensorData(mbn, OnlineNodeInformationElement->MambaNetAddress, ObjectNr, 0);
        }
      }
      OnlineNodeInformationElement = OnlineNodeInformationElement->Next;
    }
    //unlock_node_info(__FUNCTION__);
  }

  if (cntBroadcastPing)
  {
    if ((cntMillisecondTimer-PreviousCount_BroadcastPing)> 500)
    {
      mbnSendPingRequest(mbn, MBN_BROADCAST_ADDRESS);
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
//  printf("[SetBackplane_Source(%d,%d)]\n", FormInputNr, ChannelNr);

  int ObjectNr = 1032+ChannelNr;

  if (BackplaneMambaNetAddress != 0x00000000)
  {
    mbn_data data;
//    printf("SendMambaNetMessage(0x%08X, 0x%08X, ...\n", BackplaneMambaNetAddress, this_node.MambaNetAddr);

    data.UInt = FormInputNr;
    mbnSetActuatorData(mbn, BackplaneMambaNetAddress, ObjectNr, MBN_DATATYPE_UINT, 2, data, 1);
  }
}

void SetBackplane_Clock()
{
  mbn_data data;
  int ObjectNr = 1030;
  data.State = AxumData.ExternClock;

  if (BackplaneMambaNetAddress != 0x00000000)
  {
    mbnSetActuatorData(mbn, BackplaneMambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
  }

  ObjectNr = 1024;
  switch (AxumData.Samplerate)
  {
    case 32000:
    {
      data.State = 0;
    }
    break;
    case 44100:
    {
      data.State = 1;
    }
    break;
    default:
    {
      data.State = 2;
    }
    break;
  }

  if (BackplaneMambaNetAddress != 0x00000000)
  {
    mbnSetActuatorData(mbn, BackplaneMambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
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
    dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].EQBand[BandNr].On = AxumData.ModuleData[ModuleNr].EQOnOff;
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
    dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Insert = AxumData.ModuleData[ModuleNr].InsertOnOff;
    dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Gain = AxumData.ModuleData[ModuleNr].Gain;

    dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Filter.On = AxumData.ModuleData[ModuleNr].FilterOnOff;
    dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Filter.Level = AxumData.ModuleData[ModuleNr].Filter.Level;
    dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Filter.Frequency = AxumData.ModuleData[ModuleNr].Filter.Frequency;
    dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Filter.Bandwidth = AxumData.ModuleData[ModuleNr].Filter.Bandwidth;
    dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Filter.Slope = AxumData.ModuleData[ModuleNr].Filter.Slope;
    dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Filter.Type = AxumData.ModuleData[ModuleNr].Filter.Type;

    dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Dynamics.Percent = AxumData.ModuleData[ModuleNr].Dynamics;
    dspcard->data.ChannelData[DSPCardChannelNr+cntChannel].Dynamics.On = AxumData.ModuleData[ModuleNr].DynamicsOnOff;
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
  if ((src>=matrix_sources.src_offset.min.buss) && (src<=matrix_sources.src_offset.max.buss))
  {
    int BussNr = src-matrix_sources.src_offset.min.buss;
    *l_ch = 1793+(BussNr*2)+0;
    *r_ch = 1793+(BussNr*2)+1;
  }
  else if ((src>=matrix_sources.src_offset.min.insert_out) && (src<=matrix_sources.src_offset.max.insert_out))
  {
    int ModuleNr = src-matrix_sources.src_offset.min.insert_out;

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

    axum_get_mtrx_chs_from_src(AxumData.ModuleData[ModuleNr].SelectedSource, &Input1, &Input2);

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
    if (AxumData.DestinationData[cntDestination].MixMinusSource == AxumData.ModuleData[ModuleNr].SelectedSource)
    {
      DestinationNr = cntDestination;

      if ((AxumData.ModuleData[ModuleNr].SelectedSource>=matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].SelectedSource<=matrix_sources.src_offset.max.source))
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
        if ((AxumData.ModuleData[cntModule].SelectedSource == AxumData.DestinationData[DestinationNr].MixMinusSource))
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
      SetBackplane_Source(FromChannel1, Output1);
    }
    if (Output2>-1)
    {
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
    SetBackplane_Source(FromChannel1, Output1);
  }
  if (Output2>-1)
  {
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
  char LCDText[9];
  mbn_data data;

  switch (SensorReceiveFunctionType)
  {
    case MODULE_FUNCTIONS:
    {   //Module
      unsigned int ModuleNr = (SensorReceiveFunctionNumber>>12)&0xFFF;
      unsigned int FunctionNr = SensorReceiveFunctionNumber&0xFFF;
      //unsigned int Active = 0;

      switch (FunctionNr)
      {
        case MODULE_FUNCTION_LABEL:
        { //Label
          switch (DataType)
          {
            case MBN_DATATYPE_OCTETS:
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

              data.Octets = (unsigned char *)LCDText;
              printf("Set label: %s\n", LCDText);
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_OCTETS, 8, data, 1);
            }
            break;
          }
        }
        break;
        case MODULE_FUNCTION_SOURCE:
        { //Source
          switch (DataType)
          {
            case MBN_DATATYPE_OCTETS:
            {
              GetSourceLabel(AxumData.ModuleData[ModuleNr].TemporySource, LCDText, 8);

              data.Octets = (unsigned char *)LCDText;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_OCTETS, 8, data, 1);
            }
            break;
          }
        }
        break;
        case MODULE_FUNCTION_SOURCE_A:
        case MODULE_FUNCTION_SOURCE_B:
        case MODULE_FUNCTION_SOURCE_C:
        case MODULE_FUNCTION_SOURCE_D:
        { //Not implemented
          switch (DataType)
          {
            case MBN_DATATYPE_STATE:
            {
              int Active = 0;
              switch (FunctionNr)
              {
              case MODULE_FUNCTION_SOURCE_A: //Source A
              {
                if (AxumData.ModuleData[ModuleNr].SourceA != 0)
                {
                  if (AxumData.ModuleData[ModuleNr].SelectedSource == AxumData.ModuleData[ModuleNr].SourceA)
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
                  if (AxumData.ModuleData[ModuleNr].SelectedSource == AxumData.ModuleData[ModuleNr].SourceB)
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
                  if (AxumData.ModuleData[ModuleNr].SelectedSource == AxumData.ModuleData[ModuleNr].SourceC)
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
                  if (AxumData.ModuleData[ModuleNr].SelectedSource == AxumData.ModuleData[ModuleNr].SourceD)
                  {
                    Active = 1;
                  }
                }
              }
              break;
              }

              data.State = Active;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
            }
            break;
          }
        }
        break;
        case MODULE_FUNCTION_SOURCE_PHANTOM:
        {
          switch (DataType)
          {
            case MBN_DATATYPE_STATE:
            {
              if ((AxumData.ModuleData[ModuleNr].SelectedSource >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].SelectedSource <= matrix_sources.src_offset.max.source))
              {
                int SourceNr = AxumData.ModuleData[ModuleNr].SelectedSource-matrix_sources.src_offset.min.source;

                data.State = 0;
                if (AxumData.ModuleData[ModuleNr].SelectedSource != 0)
                {
                  data.State = AxumData.SourceData[SourceNr].Phantom;
                }
                mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
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
            case MBN_DATATYPE_STATE:
            {
              if ((AxumData.ModuleData[ModuleNr].SelectedSource >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].SelectedSource <= matrix_sources.src_offset.max.source))
              {
                int SourceNr = AxumData.ModuleData[ModuleNr].SelectedSource-matrix_sources.src_offset.min.source;

                data.State = 0;
                if (AxumData.ModuleData[ModuleNr].SelectedSource != 0)
                {
                  data.State = AxumData.SourceData[SourceNr].Pad;
                }
                mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
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
            case MBN_DATATYPE_OCTETS:
            {
              if ((AxumData.ModuleData[ModuleNr].SelectedSource >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].SelectedSource <= matrix_sources.src_offset.max.source))
              {
                int SourceNr = AxumData.ModuleData[ModuleNr].SelectedSource-matrix_sources.src_offset.min.source;
                sprintf(LCDText,     "%5.1fdB", AxumData.SourceData[SourceNr].Gain);
              }
              else
              {
                sprintf(LCDText, "  - dB  ");
              }
              data.Octets = (unsigned char *)LCDText;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_OCTETS, 8, data, 1);
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
            case MBN_DATATYPE_STATE:
            {
              data.State = AxumData.ModuleData[ModuleNr].InsertOnOff;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
            }
            break;
          }
        }
        break;
        case MODULE_FUNCTION_PHASE: //Phase
        {
          switch (DataType)
          {
            case MBN_DATATYPE_STATE:
            {
              data.State = AxumData.ModuleData[ModuleNr].PhaseReverse;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
            }
            break;
          }
        }
        break;
        case MODULE_FUNCTION_GAIN_LEVEL: //Gain level
        {
          switch (DataType)
          {
            case MBN_DATATYPE_UINT:
            {
              int Position = ((AxumData.ModuleData[ModuleNr].Gain+20)*1023)/40;

              data.UInt = Position;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_UINT, 2, data, 1);
            }
            break;
            case MBN_DATATYPE_OCTETS:
            {
              sprintf(LCDText,     "%5.1fdB", AxumData.ModuleData[ModuleNr].Gain);

              data.Octets = (unsigned char *)LCDText;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_OCTETS, 8, data, 1);
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
            case MBN_DATATYPE_OCTETS:
            {
              if (!AxumData.ModuleData[ModuleNr].FilterOnOff)
              {
                sprintf(LCDText, "  Off  ");
              }
              else
              {
                sprintf(LCDText, "%5dHz", AxumData.ModuleData[ModuleNr].Filter.Frequency);
              }

              data.Octets = (unsigned char *)LCDText;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_OCTETS, 8, data, 1);
            }
            break;
          }
        }
        break;
        case MODULE_FUNCTION_LOW_CUT_ON_OFF:  //Low cut on/off
        {
          switch (DataType)
          {
            case MBN_DATATYPE_STATE:
            {
              data.State = AxumData.ModuleData[ModuleNr].FilterOnOff;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
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
            case MBN_DATATYPE_OCTETS:
            {
              sprintf(LCDText, "%5.1fdB", AxumData.ModuleData[ModuleNr].EQBand[BandNr].Level);
              data.Octets = (unsigned char *)LCDText;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_OCTETS, 8, data, 1);
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
            case MBN_DATATYPE_OCTETS:
            {
              sprintf(LCDText, "%5dHz", AxumData.ModuleData[ModuleNr].EQBand[BandNr].Frequency);
              data.Octets = (unsigned char *)LCDText;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_OCTETS, 8, data, 1);
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
            case MBN_DATATYPE_OCTETS:
            {
              sprintf(LCDText, "%5.1f Q", AxumData.ModuleData[ModuleNr].EQBand[BandNr].Bandwidth);
              data.Octets = (unsigned char *)LCDText;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_OCTETS, 8, data, 1);
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
            case MBN_DATATYPE_OCTETS:
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

              data.Octets = (unsigned char *)LCDText;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_OCTETS, 8, data, 1);
            }
            break;
          }
        }
        break;
        case MODULE_FUNCTION_EQ_ON_OFF:
        { //EQ on/off
          switch (DataType)
          {
            case MBN_DATATYPE_STATE:
            {
              data.State = AxumData.ModuleData[ModuleNr].EQOnOff;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
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
            case MBN_DATATYPE_OCTETS:
            {
              sprintf(LCDText, "  %3d%%  ", AxumData.ModuleData[ModuleNr].Dynamics);
              data.Octets = (unsigned char *)LCDText;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_OCTETS, 8, data, 1);
            }
            break;
          }
        }
        break;
        case MODULE_FUNCTION_DYNAMICS_ON_OFF:
        { //Dynamics on/off
          switch (DataType)
          {
            case MBN_DATATYPE_STATE:
            {
              data.State = AxumData.ModuleData[ModuleNr].DynamicsOnOff;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
            }
            break;
          }
        }
        break;
        case MODULE_FUNCTION_MONO:
        { //Mono
          switch (DataType)
          {
            case MBN_DATATYPE_STATE:
            {
              data.State = AxumData.ModuleData[ModuleNr].Mono;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
            }
            break;
          }
        }
        break;
        case MODULE_FUNCTION_PAN:
        { //Pan
          switch (DataType)
          {
            case MBN_DATATYPE_OCTETS:
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

              data.Octets = (unsigned char *)LCDText;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_OCTETS, 8, data, 1);
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
            case MBN_DATATYPE_UINT:
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

              data.UInt = Position;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_UINT, DataSize, data, 0);
            }
            break;
            case MBN_DATATYPE_OCTETS:
            {
              sprintf(LCDText, " %4.0f dB", AxumData.ModuleData[ModuleNr].FaderLevel);
              data.Octets = (unsigned char *)LCDText;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_OCTETS, 8, data, 1);
            }
            break;
          }
        }
        break;
        case MODULE_FUNCTION_MODULE_ON:
        { //Module on
          switch (DataType)
          {
            case MBN_DATATYPE_STATE:
            {
              data.State = AxumData.ModuleData[ModuleNr].On;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
            }
            break;
          }
        }
        break;
        case MODULE_FUNCTION_MODULE_OFF:
        { //Module off
          switch (DataType)
          {
            case MBN_DATATYPE_STATE:
            {
              data.State = !AxumData.ModuleData[ModuleNr].On;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
            }
            break;
          }
        }
        break;
        case MODULE_FUNCTION_MODULE_ON_OFF:
        { //Module on/off
          switch (DataType)
          {
            case MBN_DATATYPE_STATE:
            {
              data.State = AxumData.ModuleData[ModuleNr].On;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
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
            case MBN_DATATYPE_UINT:
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

              data.UInt = Position;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_UINT, DataSize, data, 1);
            }
            break;
            case MBN_DATATYPE_OCTETS:
            {
              if (AxumData.ModuleData[ModuleNr].Buss[BussNr].On)
              {
                sprintf(LCDText, " %4.0f dB", AxumData.ModuleData[ModuleNr].Buss[BussNr].Level);
              }
              else
              {
                sprintf(LCDText, "  Off   ");
              }

              data.Octets = (unsigned char *)LCDText;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_OCTETS, 8, data, 1);
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
            case MBN_DATATYPE_STATE:
            {
              data.State = AxumData.ModuleData[ModuleNr].Buss[BussNr].On;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
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
            case MBN_DATATYPE_STATE:
            {
              data.State = AxumData.ModuleData[ModuleNr].Buss[BussNr].PreModuleLevel;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
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
            case MBN_DATATYPE_OCTETS:
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

              data.Octets = (unsigned char *)LCDText;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_OCTETS, 8, data, 1);
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
        bool Active;

        Active = 0;
        switch (FunctionNr)
        {
          case MODULE_FUNCTION_SOURCE_START:
          { //Start
            if ((AxumData.ModuleData[ModuleNr].SelectedSource >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].SelectedSource <= matrix_sources.src_offset.max.source))
            {
              int SourceNr = AxumData.ModuleData[ModuleNr].SelectedSource-matrix_sources.src_offset.min.source;
              Active = AxumData.SourceData[SourceNr].Start;
            }
          }
          break;
          case MODULE_FUNCTION_SOURCE_STOP:
          { //Stop
            if ((AxumData.ModuleData[ModuleNr].SelectedSource >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].SelectedSource <= matrix_sources.src_offset.max.source))
            {
              int SourceNr = AxumData.ModuleData[ModuleNr].SelectedSource-matrix_sources.src_offset.min.source;
              Active = !AxumData.SourceData[SourceNr].Start;
            }
          }
          break;
          case MODULE_FUNCTION_SOURCE_START_STOP:
          { //Start/Stop
            if ((AxumData.ModuleData[ModuleNr].SelectedSource >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].SelectedSource <= matrix_sources.src_offset.max.source))
            {
              int SourceNr = AxumData.ModuleData[ModuleNr].SelectedSource-matrix_sources.src_offset.min.source;
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
            if ((AxumData.ModuleData[ModuleNr].SelectedSource >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].SelectedSource <= matrix_sources.src_offset.max.source))
            {
              int SourceNr = AxumData.ModuleData[ModuleNr].SelectedSource-matrix_sources.src_offset.min.source;
              Active = AxumData.SourceData[SourceNr].Alert;
            }
          }
          break;
        }
        data.State = Active;
        mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
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
          switch (DataType)
          {
            case MBN_DATATYPE_STATE:
            {
              data.State = AxumData.ModuleData[ModuleNr].Peak;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
            }
            break;
          }
        }
        break;
        case MODULE_FUNCTION_SIGNAL:
        { //Signal
          switch (DataType)
          {
            case MBN_DATATYPE_STATE:
            {
              data.State = AxumData.ModuleData[ModuleNr].Signal;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
            }
            break;
          }
        }
        break;
      }
    }
    break;
    case BUSS_FUNCTIONS:
    {   //Busses
      unsigned int BussNr = (SensorReceiveFunctionNumber>>12)&0xFFF;
      unsigned int FunctionNr = SensorReceiveFunctionNumber&0xFFF;

      switch (FunctionNr)
      {
        case BUSS_FUNCTION_MASTER_LEVEL:
        {
          switch (DataType)
          {
            case MBN_DATATYPE_UINT:
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

              data.UInt = Position;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_UINT, DataSize, data, 1);
            }
            break;
            case MBN_DATATYPE_OCTETS:
            {
              sprintf(LCDText, " %4.0f dB", AxumData.BussMasterData[BussNr].Level);
              data.Octets = (unsigned char *)LCDText;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_OCTETS, 8, data, 1);
            }
            break;
            case MBN_DATATYPE_FLOAT:
            {
              float Level = AxumData.BussMasterData[BussNr].Level;
              if (Level<DataMinimal)
              {
                Level = DataMinimal;
              }
              else if (Level>DataMaximal)
              {
                Level = DataMaximal;
              }
              data.Float = Level;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_FLOAT, 2, data, 1);
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
            case MBN_DATATYPE_STATE:
            {
              data.State = AxumData.BussMasterData[BussNr].On;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
            }
            break;
          }
        }
        break;
        case BUSS_FUNCTION_MASTER_PRE:
        {
          switch (DataType)
          {
            case MBN_DATATYPE_STATE:
            {
              data.State = AxumData.BussMasterData[BussNr].PreModuleLevel;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
            }
            break;
          }
        }
        break;
        case BUSS_FUNCTION_LABEL:
        { //Buss label
          switch (DataType)
          {
            case MBN_DATATYPE_OCTETS:
            {
              data.Octets = (unsigned char *)AxumData.BussMasterData[BussNr].Label;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_OCTETS, 8, data, 1);
            }
            break;
          }
        }
        break;
        case BUSS_FUNCTION_AUDIO_LEVEL_LEFT:
        {
          switch (DataType)
          {
            case MBN_DATATYPE_FLOAT:
            {
              data.Float = SummingdBLevel[(BussNr*2)+0];
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_FLOAT, 2, data, 0);
            }
          }
        }
        break;
        case BUSS_FUNCTION_AUDIO_LEVEL_RIGHT:
        {
          data.Float = SummingdBLevel[(BussNr*2)+1];
          mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_FLOAT, 2, data, 0);
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

        data.State = Active;
        mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
      }
      else
      {
        switch (FunctionNr)
        {
          case MONITOR_BUSS_FUNCTION_MUTE:
          { //Mute
            switch (DataType)
            {
              case MBN_DATATYPE_STATE:
              {
                data.State = AxumData.Monitor[MonitorBussNr].Mute;
                mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
              }
              break;
            }
          }
          break;
          case MONITOR_BUSS_FUNCTION_DIM:
          { //Dim
            switch (DataType)
            {
              case MBN_DATATYPE_STATE:
              {
                data.State = AxumData.Monitor[MonitorBussNr].Dim;
                mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
              }
              break;
            }
          }
          break;
          case MONITOR_BUSS_FUNCTION_PHONES_LEVEL:
          { //Phones level
            switch (DataType)
            {
              case MBN_DATATYPE_UINT:
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

                data.UInt = Position;
                mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_UINT, DataSize, data, 1);
              }
              break;
              case MBN_DATATYPE_OCTETS:
              {
                sprintf(LCDText, " %4.0f dB", AxumData.Monitor[MonitorBussNr].PhonesLevel);
                data.Octets = (unsigned char *)LCDText;
                mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_OCTETS, 8, data, 1);
              }
              break;
              case MBN_DATATYPE_FLOAT:
              {
                float Level = AxumData.Monitor[MonitorBussNr].PhonesLevel;
                if (Level<DataMinimal)
                {
                  Level = DataMinimal;
                }
                else if (Level>DataMaximal)
                {
                  Level = DataMaximal;
                }
                data.Float = Level;
                mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_FLOAT, 2, data, 1);
              }
              break;
            }
          }
          break;
          case MONITOR_BUSS_FUNCTION_MONO:
          { //Mono
            switch (DataType)
            {
              case MBN_DATATYPE_STATE:
              {
                data.State = AxumData.Monitor[MonitorBussNr].Mono;
                mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
              }
              break;
            }
          }
          break;
          case MONITOR_BUSS_FUNCTION_PHASE:
          { //Phase
            switch (DataType)
            {
              case MBN_DATATYPE_STATE:
              {
                data.State = AxumData.Monitor[MonitorBussNr].Phase;
                mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
              }
              break;
            }
          }
          break;
          case MONITOR_BUSS_FUNCTION_SPEAKER_LEVEL:
          { //Speaker level
            switch (DataType)
            {
              case MBN_DATATYPE_UINT:
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

                data.UInt = Position;
                mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_UINT, DataSize, data, 1);
              }
              break;
              case MBN_DATATYPE_OCTETS:
              {
                sprintf(LCDText, " %4.0f dB", AxumData.Monitor[MonitorBussNr].SpeakerLevel);
                data.Octets = (unsigned char *)LCDText;
                mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_OCTETS, 8, data, 1);
              }
              break;
              case MBN_DATATYPE_FLOAT:
              {
                float Level = AxumData.Monitor[MonitorBussNr].SpeakerLevel;
                if (Level<DataMinimal)
                {
                  Level = DataMinimal;
                }
                else if (Level>DataMaximal)
                {
                  Level = DataMaximal;
                }
                data.Float = Level;
                mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_FLOAT, 2, data, 1);
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
              case MBN_DATATYPE_STATE:
              {
                data.State = AxumData.Monitor[MonitorBussNr].Talkback[TalkbackNr];
                mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
              }
              break;
            }
          }
          break;
          case MONITOR_BUSS_FUNCTION_AUDIO_LEVEL_LEFT:
          {
            switch (DataType)
            {
              case MBN_DATATYPE_FLOAT:
              {
                data.Float = SummingdBLevel[32+(MonitorBussNr*2)+0];
                mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_FLOAT, 2, data, 0);
              }
              break;
            }
          }
          break;
          case MONITOR_BUSS_FUNCTION_AUDIO_LEVEL_RIGHT:
          {
            switch (DataType)
            {
              case MBN_DATATYPE_FLOAT:
              {
                data.Float = SummingdBLevel[32+(MonitorBussNr*2)+1];
                mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_FLOAT, 2, data, 0);
              }
              break;
            }
          }
          break;
          case MONITOR_BUSS_FUNCTION_LABEL:
          { //Buss label
            switch (DataType)
            {
              case MBN_DATATYPE_OCTETS:
              {
                data.Octets = (unsigned char *)AxumData.Monitor[MonitorBussNr].Label;
                mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_OCTETS, 8, data, 1);
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
      unsigned char Active = 0;

      if (GlobalNr == 0)
      {
        if ((((signed int)FunctionNr)>=GLOBAL_FUNCTION_REDLIGHT_1) && (FunctionNr<GLOBAL_FUNCTION_CONTROL_1_MODE_SOURCE))
        { //all states
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

          data.State = Active;
          mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
        }
        else if ((FunctionNr>=GLOBAL_FUNCTION_CONTROL_1_MODE_SOURCE) && (FunctionNr<GLOBAL_FUNCTION_CONTROL_2_MODE_SOURCE))
        { //Control 1 mode
          unsigned char CorrespondingControlMode = FunctionNr-GLOBAL_FUNCTION_CONTROL_1_MODE_SOURCE;
          if (AxumData.Control1Mode == CorrespondingControlMode)
          {
            Active = 1;
          }

          data.State = Active;
          mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
        }
        else if ((FunctionNr>=GLOBAL_FUNCTION_CONTROL_2_MODE_SOURCE) && (FunctionNr<GLOBAL_FUNCTION_CONTROL_3_MODE_SOURCE))
        { //Control 2 mode
          unsigned char CorrespondingControlMode = FunctionNr-GLOBAL_FUNCTION_CONTROL_2_MODE_SOURCE;
          if (AxumData.Control2Mode == CorrespondingControlMode)
          {
            Active = 1;
          }

          data.State = Active;
          mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
        }
        else if ((FunctionNr>=GLOBAL_FUNCTION_CONTROL_3_MODE_SOURCE) && (FunctionNr<GLOBAL_FUNCTION_CONTROL_4_MODE_SOURCE))
        { //Control 3 mode
          unsigned char CorrespondingControlMode = FunctionNr-GLOBAL_FUNCTION_CONTROL_3_MODE_SOURCE;
          if (AxumData.Control3Mode == CorrespondingControlMode)
          {
            Active = 1;
          }

          data.State = Active;
          mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
        }
        else if ((FunctionNr>=GLOBAL_FUNCTION_CONTROL_4_MODE_SOURCE) && (FunctionNr<GLOBAL_FUNCTION_MASTER_CONTROL_1_MODE_BUSS_1_2))
        { //Control 4 mode
          unsigned char CorrespondingControlMode = FunctionNr-GLOBAL_FUNCTION_CONTROL_4_MODE_SOURCE;
          if (AxumData.Control4Mode == CorrespondingControlMode)
          {
            Active = 1;
          }

          data.State = Active;
          mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
        }
        else if ((FunctionNr>=GLOBAL_FUNCTION_MASTER_CONTROL_1_MODE_BUSS_1_2) && (FunctionNr<GLOBAL_FUNCTION_MASTER_CONTROL_2_MODE_BUSS_1_2))
        { //Master control 1 mode
          unsigned char CorrespondingControlMode = FunctionNr-GLOBAL_FUNCTION_MASTER_CONTROL_1_MODE_BUSS_1_2;
          if (AxumData.MasterControl1Mode == CorrespondingControlMode)
          {
            Active = 1;
          }

          data.State = Active;
          mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
        }
        else if ((FunctionNr>=GLOBAL_FUNCTION_MASTER_CONTROL_2_MODE_BUSS_1_2) && (FunctionNr<GLOBAL_FUNCTION_MASTER_CONTROL_3_MODE_BUSS_1_2))
        { //Master control 2 mode
          unsigned char CorrespondingControlMode = FunctionNr-GLOBAL_FUNCTION_MASTER_CONTROL_2_MODE_BUSS_1_2;
          if (AxumData.MasterControl2Mode == CorrespondingControlMode)
          {
            Active = 1;
          }

          data.State = Active;
          mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
        }
        else if ((FunctionNr>=GLOBAL_FUNCTION_MASTER_CONTROL_3_MODE_BUSS_1_2) && (FunctionNr<GLOBAL_FUNCTION_MASTER_CONTROL_4_MODE_BUSS_1_2))
        { //Master control 3 mode
          unsigned char CorrespondingControlMode = FunctionNr-GLOBAL_FUNCTION_MASTER_CONTROL_3_MODE_BUSS_1_2;
          if (AxumData.MasterControl3Mode == CorrespondingControlMode)
          {
            Active = 1;
          }

          data.State = Active;
          mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
        }
        else if ((FunctionNr>=GLOBAL_FUNCTION_MASTER_CONTROL_4_MODE_BUSS_1_2) && (FunctionNr<GLOBAL_FUNCTION_MASTER_CONTROL_1))
        { //Master control 1 mode
          unsigned char CorrespondingControlMode = FunctionNr-GLOBAL_FUNCTION_MASTER_CONTROL_4_MODE_BUSS_1_2;
          if (AxumData.MasterControl4Mode == CorrespondingControlMode)
          {
            Active = 1;
          }

          data.State = Active;
          mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
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
      unsigned char Active = 0;

      switch (FunctionNr)
      {
        case SOURCE_FUNCTION_MODULE_ON:
        case SOURCE_FUNCTION_MODULE_OFF:
        case SOURCE_FUNCTION_MODULE_ON_OFF:
        {
          Active = 0;
          for (int cntModule=0; cntModule<128; cntModule++)
          {
            if (AxumData.ModuleData[cntModule].SelectedSource == (SourceNr+matrix_sources.src_offset.min.source))
            {
              if (AxumData.ModuleData[cntModule].On)
              {
                Active = 1;
              }
            }
          }

          if (FunctionNr == SOURCE_FUNCTION_MODULE_OFF)
          {
            data.State = !Active;
          }
          else
          {
            data.State = Active;
          }
          mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
        }
        break;
        case SOURCE_FUNCTION_MODULE_FADER_ON:
        case SOURCE_FUNCTION_MODULE_FADER_OFF:
        case SOURCE_FUNCTION_MODULE_FADER_ON_OFF:
        {
          Active = 0;
          for (int cntModule=0; cntModule<128; cntModule++)
          {
            if (AxumData.ModuleData[cntModule].SelectedSource == (SourceNr+matrix_sources.src_offset.min.source))
            {
              if (AxumData.ModuleData[cntModule].FaderLevel>-80)
              {
                Active = 1;
              }
            }
          }

          if (FunctionNr == SOURCE_FUNCTION_MODULE_FADER_OFF)
          {
            data.State = !Active;
          }
          else
          {
            data.State = Active;
          }
          mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
        }
        break;
        case SOURCE_FUNCTION_MODULE_FADER_AND_ON_ACTIVE:
        case SOURCE_FUNCTION_MODULE_FADER_AND_ON_INACTIVE:
        {
          Active = 0;
          for (int cntModule=0; cntModule<128; cntModule++)
          {
            if (AxumData.ModuleData[cntModule].SelectedSource == (SourceNr+matrix_sources.src_offset.min.source))
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

          if (FunctionNr == SOURCE_FUNCTION_MODULE_FADER_AND_ON_INACTIVE)
          {
          data.State = !Active;
          }
          else
          {
          data.State = Active;
          }
          mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
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
            if (AxumData.ModuleData[cntModule].SelectedSource == (SourceNr+matrix_sources.src_offset.min.source))
            {
              if (AxumData.ModuleData[cntModule].Buss[BussNr].On)
              {
                Active = 1;
              }
            }
          }

          data.State = Active;
          mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
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
            if (AxumData.ModuleData[cntModule].SelectedSource == (SourceNr+matrix_sources.src_offset.min.source))
            {
              if (AxumData.ModuleData[cntModule].Buss[BussNr].On)
              {
                Active = 1;
              }
            }
          }

          data.State = !Active;
          mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
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
            if (AxumData.ModuleData[cntModule].SelectedSource == (SourceNr+matrix_sources.src_offset.min.source))
            {
              if (AxumData.ModuleData[cntModule].Buss[BussNr].On)
              {
                Active = 1;
              }
            }
          }
          data.State = Active;
          mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
        }
        break;
        case SOURCE_FUNCTION_MODULE_COUGH_ON_OFF:
        {
          Active = 0;
          for (int cntModule=0; cntModule<128; cntModule++)
          {
            if (AxumData.ModuleData[cntModule].SelectedSource == (SourceNr+matrix_sources.src_offset.min.source))
            {
              if (AxumData.ModuleData[cntModule].Cough)
              {
                Active = 1;
              }
            }
          }

          data.State = Active;
          mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
        }
        break;
        case SOURCE_FUNCTION_START:
        case SOURCE_FUNCTION_STOP:
        case SOURCE_FUNCTION_START_STOP:
        {
          if (FunctionNr == SOURCE_FUNCTION_STOP)
          {
            data.State = !AxumData.SourceData[SourceNr].Start;
          }
          else
          {
            data.State = AxumData.SourceData[SourceNr].Start;
          }
          mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
        }
        break;
        case SOURCE_FUNCTION_PHANTOM:
        {
          data.State = AxumData.SourceData[SourceNr].Phantom;
          mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
        }
        break;
        case SOURCE_FUNCTION_PAD:
        {
          switch (DataType)
          {
            case MBN_DATATYPE_STATE:
            {
              data.State = AxumData.SourceData[SourceNr].Pad;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
            }
            break;
          }
        }
        break;
        case SOURCE_FUNCTION_GAIN:
        {
          switch (DataType)
          {
            case MBN_DATATYPE_FLOAT:
            {
              float Level = AxumData.SourceData[SourceNr].Gain;
              if (Level<DataMinimal)
              {
                Level = DataMinimal;
              }
              else if (Level>DataMaximal)
              {
                Level = DataMaximal;
              }
              data.Float = Level;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_FLOAT, 2, data, 1);
            }
            break;
          }
        }
        break;
        case SOURCE_FUNCTION_ALERT:
        {
          switch (DataType)
          {
            case MBN_DATATYPE_STATE:
            {
              data.State = AxumData.SourceData[SourceNr].Alert;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
            }
            break;
          }
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

      switch (FunctionNr)
      {
      case DESTINATION_FUNCTION_LABEL:
      {
        switch (DataType)
        {
          case MBN_DATATYPE_OCTETS:
          {
            data.Octets = (unsigned char *)AxumData.DestinationData[DestinationNr].DestinationName;
            mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_OCTETS, 8, data, 1);
          }
          break;
        }
      }
      break;
      case DESTINATION_FUNCTION_SOURCE:
      {
        switch (DataType)
        {
          case MBN_DATATYPE_OCTETS:
          {
            GetSourceLabel(AxumData.DestinationData[DestinationNr].Source, LCDText, 8);

            data.Octets = (unsigned char *)LCDText;
            mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_OCTETS, 8, data, 1);
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
            case MBN_DATATYPE_UINT:
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

              data.UInt = Position;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_UINT, DataSize, data, 1);
            }
            break;
            case MBN_DATATYPE_OCTETS:
            {
              sprintf(LCDText, " %4.0f dB", AxumData.Monitor[MonitorBussNr].SpeakerLevel);
              data.Octets = (unsigned char *)LCDText;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_OCTETS, 8, data, 1);
            }
            break;
            case MBN_DATATYPE_FLOAT:
            {
              float Level = AxumData.Monitor[MonitorBussNr].SpeakerLevel;
              if (Level<DataMinimal)
              {
                Level = DataMinimal;
              }
              else if (Level>DataMaximal)
              {
                Level = DataMaximal;
              }
              data.Float = Level;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_FLOAT, 2, data, 1);
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
            case MBN_DATATYPE_UINT:
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

              data.UInt = Position;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_UINT, DataSize, data, 1);
            }
            break;
            case MBN_DATATYPE_OCTETS:
            {
              sprintf(LCDText, " %4.0f dB", AxumData.Monitor[MonitorBussNr].PhonesLevel);
              data.Octets = (unsigned char *)LCDText;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_OCTETS, 8, data, 1);
            }
            break;
            case MBN_DATATYPE_FLOAT:
            {
              float Level = AxumData.Monitor[MonitorBussNr].PhonesLevel;
              if (Level<DataMinimal)
              {
                Level = DataMinimal;
              }
              else if (Level>DataMaximal)
              {
                Level = DataMaximal;
              }
              data.Float = Level;
              mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_FLOAT, 2, data, 1);
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
          case MBN_DATATYPE_UINT:
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

            data.UInt = Position;
            mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_UINT, DataSize, data, 1);
          }
          break;
          case MBN_DATATYPE_OCTETS:
          {
            sprintf(LCDText, " %4.0f dB", AxumData.DestinationData[DestinationNr].Level);
            data.Octets = (unsigned char *)LCDText;
            mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_OCTETS, 8, data, 1);
          }
          break;
          case MBN_DATATYPE_FLOAT:
          {
            float Level = AxumData.DestinationData[DestinationNr].Level;
            if (Level<DataMinimal)
            {
              Level = DataMinimal;
            }
            else if (Level>DataMaximal)
            {
              Level = DataMaximal;
            }
            data.Float = Level;
            mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_FLOAT, 2, data, 1);
          }
          break;
        }
      }
      break;
      case DESTINATION_FUNCTION_MUTE:
      {
        switch (DataType)
        {
          case MBN_DATATYPE_STATE:
          {
            data.State = AxumData.DestinationData[DestinationNr].Mute;
            mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
          }
          break;
        }
      }
      break;
      case DESTINATION_FUNCTION_MUTE_AND_MONITOR_MUTE:
      {
        switch (DataType)
        {
          case MBN_DATATYPE_STATE:
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

            data.State = Active;
            mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
          }
          break;
        }
      }
      break;
      case DESTINATION_FUNCTION_DIM:
      {
        switch (DataType)
        {
          case MBN_DATATYPE_STATE:
          {
            data.State = AxumData.DestinationData[DestinationNr].Dim;
            mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
          }
          break;
        }
      }
      break;
      case DESTINATION_FUNCTION_DIM_AND_MONITOR_DIM:
      {
        switch (DataType)
        {
          case MBN_DATATYPE_STATE:
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

            data.State = Active;
            mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
          }
          break;
        }
      }
      break;
      case DESTINATION_FUNCTION_MONO:
      {
        switch (DataType)
        {
          case MBN_DATATYPE_STATE:
          {
            Active = AxumData.DestinationData[DestinationNr].Mono;

            data.State = Active;
            mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
          }
          break;
        }
      }
      break;
      case DESTINATION_FUNCTION_MONO_AND_MONITOR_MONO:
      {
        switch (DataType)
        {
          case MBN_DATATYPE_STATE:
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

            data.State = Active;
            mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
          }
          break;
        }
      }
      break;
      case DESTINATION_FUNCTION_PHASE:
      {
        switch (DataType)
        {
          case MBN_DATATYPE_STATE:
          {
            data.State = AxumData.DestinationData[DestinationNr].Phase;
            mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
          }
          break;
        }
      }
      break;
      case DESTINATION_FUNCTION_PHASE_AND_MONITOR_PHASE:
      {
        switch (DataType)
        {
          case MBN_DATATYPE_STATE:
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

            data.State = Active;
            mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
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
          case MBN_DATATYPE_STATE:
          {
            data.State = AxumData.DestinationData[DestinationNr].Talkback[TalkbackNr];
            mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
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
          case MBN_DATATYPE_STATE:
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

            data.State = Active;
            mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_STATE, 1, data, 1);
          }
          break;
        }
      }
      break;
      }
    }
    break;
  }
  DataSize = 0;
  DataMinimal = 0;
  DataMaximal = 0;
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

//  pthread_mutex_lock(&get_node_info_mutex);
  ONLINE_NODE_INFORMATION_STRUCT *OnlineNodeInformationElement = OnlineNodeInformationList;
  while (OnlineNodeInformationElement != NULL)
  {
    if (OnlineNodeInformationElement->MambaNetAddress != 0)
    {
      if (OnlineNodeInformationElement->SensorReceiveFunction != NULL)
      {
        for (int cntObject=0; cntObject<OnlineNodeInformationElement->NumberOfCustomObjects; cntObject++)
        {
          if (OnlineNodeInformationElement->SensorReceiveFunction[cntObject].FunctionNr == SensorReceiveFunctionNumber)
          {
            if (OnlineNodeInformationElement->ObjectInformation[cntObject].ActuatorDataType != MBN_DATATYPE_NODATA)
            {
              AXUM_FUNCTION_INFORMATION_STRUCT *AxumFunctionInformationStructToAdd = new AXUM_FUNCTION_INFORMATION_STRUCT;

              AxumFunctionInformationStructToAdd->MambaNetAddress = OnlineNodeInformationElement->MambaNetAddress;
              AxumFunctionInformationStructToAdd->ObjectNr = 1024+cntObject;
              AxumFunctionInformationStructToAdd->ActuatorDataType = OnlineNodeInformationElement->ObjectInformation[cntObject].ActuatorDataType;
              AxumFunctionInformationStructToAdd->ActuatorDataSize = OnlineNodeInformationElement->ObjectInformation[cntObject].ActuatorDataSize;
              AxumFunctionInformationStructToAdd->ActuatorDataMinimal = OnlineNodeInformationElement->ObjectInformation[cntObject].ActuatorDataMinimal;
              AxumFunctionInformationStructToAdd->ActuatorDataMaximal = OnlineNodeInformationElement->ObjectInformation[cntObject].ActuatorDataMaximal;
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
    OnlineNodeInformationElement = OnlineNodeInformationElement->Next;
  }
//  pthread_mutex_unlock(&get_node_info_mutex);

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

void ModeControllerSensorChange(unsigned int SensorReceiveFunctionNr, unsigned char type, mbn_data data, unsigned char DataType, unsigned char DataSize, float DataMinimal, float DataMaximal)
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

  if (type == MBN_DATATYPE_SINT)
  {
    switch (ControlMode)
    {
      case MODULE_CONTROL_MODE_SOURCE:
      {   //Source
        int CurrentSource = AxumData.ModuleData[ModuleNr].TemporySource;

        AxumData.ModuleData[ModuleNr].TemporySource = AdjustModuleSource(CurrentSource, data.SInt);

        unsigned int DisplayFunctionNr = (ModuleNr<<12);
        CheckObjectsToSent(DisplayFunctionNr+MODULE_FUNCTION_CONTROL_1);
        CheckObjectsToSent(DisplayFunctionNr+MODULE_FUNCTION_CONTROL_2);
        CheckObjectsToSent(DisplayFunctionNr+MODULE_FUNCTION_CONTROL_3);
        CheckObjectsToSent(DisplayFunctionNr+MODULE_FUNCTION_CONTROL_4);
        CheckObjectsToSent(DisplayFunctionNr+MODULE_FUNCTION_SOURCE);
        CheckObjectsToSent(DisplayFunctionNr+MODULE_FUNCTION_CONTROL_1_LABEL);
        CheckObjectsToSent(DisplayFunctionNr+MODULE_FUNCTION_CONTROL_2_LABEL);
        CheckObjectsToSent(DisplayFunctionNr+MODULE_FUNCTION_CONTROL_3_LABEL);
        CheckObjectsToSent(DisplayFunctionNr+MODULE_FUNCTION_CONTROL_4_LABEL);
      }
      break;
      case MODULE_CONTROL_MODE_SOURCE_GAIN:
      {   //Source gain
        if ((AxumData.ModuleData[ModuleNr].SelectedSource>=matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].SelectedSource<=matrix_sources.src_offset.max.source))
        {
          int SourceNr = AxumData.ModuleData[ModuleNr].SelectedSource-matrix_sources.src_offset.min.source;

          AxumData.SourceData[SourceNr].Gain += (float)data.SInt/10;
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
            if (AxumData.ModuleData[cntModule].SelectedSource == (SourceNr+matrix_sources.src_offset.min.source))
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
        AxumData.ModuleData[ModuleNr].Gain += (float)data.SInt/10;
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
        if (data.SInt>=0)
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
        if (data.SInt>=0)
        {
          AxumData.ModuleData[ModuleNr].Filter.Frequency *= 1+((float)data.SInt/100);
        }
        else
        {
          AxumData.ModuleData[ModuleNr].Filter.Frequency /= 1+((float)-data.SInt/100);
        }

        if (AxumData.ModuleData[ModuleNr].Filter.Frequency <= 20)
        {
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
        AxumData.ModuleData[ModuleNr].EQBand[BandNr].Level += (float)data.SInt/10;
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
        if (data.SInt>=0)
        {
          AxumData.ModuleData[ModuleNr].EQBand[BandNr].Frequency *= 1+((float)data.SInt/100);
        }
        else
        {
          AxumData.ModuleData[ModuleNr].EQBand[BandNr].Frequency /= 1+((float)-data.SInt/100);
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

        AxumData.ModuleData[ModuleNr].EQBand[BandNr].Bandwidth += (float)data.SInt/10;

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

        Type += data.SInt;

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
        AxumData.ModuleData[ModuleNr].Dynamics += data.SInt;
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
        if (data.SInt>=0)
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
        AxumData.ModuleData[ModuleNr].Panorama += data.SInt;
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

        AxumData.ModuleData[ModuleNr].FaderLevel += data.SInt;
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

          if ((AxumData.ModuleData[ModuleNr].SelectedSource>=matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].SelectedSource<=matrix_sources.src_offset.max.source))
          {
            unsigned int SourceNr = AxumData.ModuleData[ModuleNr].SelectedSource-matrix_sources.src_offset.min.source;
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
        AxumData.ModuleData[ModuleNr].Buss[BussNr].Level += data.SInt;
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

        AxumData.ModuleData[ModuleNr].Buss[BussNr].Balance += data.SInt;
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

  SensorReceiveFunctionNr = 0;
  type = 0;
  data.State = 0;
  DataType = 0;
  DataSize = 0;
  DataMinimal = 0;
  DataMaximal = 0;
}

void ModeControllerResetSensorChange(unsigned int SensorReceiveFunctionNr, unsigned char type, mbn_data data, unsigned char DataType, unsigned char DataSize, float DataMinimal, float DataMaximal)
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

  if (type == MBN_DATATYPE_STATE)
  {
    if (data.State)
    {
      switch (ControlMode)
      {
        case MODULE_CONTROL_MODE_SOURCE:
        {
          SetNewSource(ModuleNr, AxumData.ModuleData[ModuleNr].TemporySource, 0, 1);

          unsigned int DisplayFunctionNr = (ModuleNr<<12);
          CheckObjectsToSent(DisplayFunctionNr+MODULE_FUNCTION_CONTROL_1_LABEL);
          CheckObjectsToSent(DisplayFunctionNr+MODULE_FUNCTION_CONTROL_2_LABEL);
          CheckObjectsToSent(DisplayFunctionNr+MODULE_FUNCTION_CONTROL_3_LABEL);
          CheckObjectsToSent(DisplayFunctionNr+MODULE_FUNCTION_CONTROL_4_LABEL);
        }
        break;
        case MODULE_CONTROL_MODE_SOURCE_GAIN:
        {   //Source gain
          if ((AxumData.ModuleData[ModuleNr].SelectedSource>=matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].SelectedSource<=matrix_sources.src_offset.max.source))
          {
            unsigned int SourceNr = AxumData.ModuleData[ModuleNr].SelectedSource-matrix_sources.src_offset.min.source;

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
              if (AxumData.ModuleData[cntModule].SelectedSource == (SourceNr+1))
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
          AxumData.ModuleData[ModuleNr].FilterOnOff = !AxumData.ModuleData[ModuleNr].FilterOnOff;

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
          AxumData.ModuleData[ModuleNr].EQBand[BandNr].Frequency = AxumData.ModuleData[ModuleNr].Defaults.EQBand[BandNr].Frequency;

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
          AxumData.ModuleData[ModuleNr].EQBand[BandNr].Bandwidth = AxumData.ModuleData[ModuleNr].Defaults.EQBand[BandNr].Bandwidth;
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
          AxumData.ModuleData[ModuleNr].EQBand[BandNr].Type = AxumData.ModuleData[ModuleNr].Defaults.EQBand[BandNr].Type;
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
          AxumData.ModuleData[ModuleNr].DynamicsOnOff = !AxumData.ModuleData[ModuleNr].DynamicsOnOff;
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

            if ((AxumData.ModuleData[ModuleNr].SelectedSource>=matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].SelectedSource<=matrix_sources.src_offset.max.source))
            {
              unsigned int SourceNr = AxumData.ModuleData[ModuleNr].SelectedSource-matrix_sources.src_offset.min.source;
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

  SensorReceiveFunctionNr = 0;
  type = 0;
  data.State = NULL;
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
  char ControlMode = -1;
  mbn_data data;

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
      GetSourceLabel(AxumData.ModuleData[ModuleNr].SelectedSource, LCDText, 8);
    }
    break;
    case MODULE_CONTROL_MODE_SOURCE:
    {
      GetSourceLabel(AxumData.ModuleData[ModuleNr].TemporySource, LCDText, 8);
    }
    break;
    case MODULE_CONTROL_MODE_SOURCE_GAIN:
    {
      if ((AxumData.ModuleData[ModuleNr].SelectedSource>=matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].SelectedSource<=matrix_sources.src_offset.max.source))
      {
        unsigned int SourceNr = AxumData.ModuleData[ModuleNr].SelectedSource-matrix_sources.src_offset.min.source;
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
      if (!AxumData.ModuleData[ModuleNr].FilterOnOff)
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

  data.Octets = (unsigned char *)LCDText;
  mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_OCTETS, 8, data, 1);

  SensorReceiveFunctionNr = 0;
  MambaNetAddress = 0;
  ObjectNr = 0;
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
  char ControlMode = -1;
  mbn_data data;

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
    if (AxumData.ModuleData[ModuleNr].SelectedSource != AxumData.ModuleData[ModuleNr].TemporySource)
    {
      sprintf(LCDText," Source ");
    }
    else
    {
      sprintf(LCDText,">Source<");
    }
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

  data.Octets = (unsigned char *)LCDText;
  mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_OCTETS, 8, data, 1);

  DataType = 0;
  DataSize = 0;
  DataMinimal = 0;
  DataMaximal = 0;
}


void MasterModeControllerSensorChange(unsigned int SensorReceiveFunctionNr, unsigned char type, mbn_data data, unsigned char DataType, unsigned char DataSize, float DataMinimal, float DataMaximal)
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

  if (type == MBN_DATATYPE_UINT)
  {
    int Position = (data.UInt*1023)/(DataMaximal-DataMinimal);
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
  else if (type == MBN_DATATYPE_SINT)
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
        int BussNr = MasterControlMode-MASTER_CONTROL_MODE_BUSS_1_2;
        AxumData.BussMasterData[BussNr].Level += data.SInt;
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
  SensorReceiveFunctionNr = 0;
  type = 0;
  data.State = 0;
  DataType = 0;
  DataSize = 0;
  DataMinimal = 0;
  DataMaximal = 0;
}

void MasterModeControllerResetSensorChange(unsigned int SensorReceiveFunctionNr, unsigned char type, mbn_data data, unsigned char DataType, unsigned char DataSize, float DataMinimal, float DataMaximal)
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

  if (type == MBN_DATATYPE_STATE)
  {
    if (data.State)
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
  SensorReceiveFunctionNr = 0;
  type = 0;
  data.State = 0;
  DataType = 0;
  DataSize = 0;
  DataMinimal = 0;
  DataMaximal = 0;
}


void MasterModeControllerSetData(unsigned int SensorReceiveFunctionNr, unsigned int MambaNetAddress, unsigned int ObjectNr, unsigned char DataType, unsigned char DataSize, float DataMinimal, float DataMaximal)
{
  int FunctionNr = SensorReceiveFunctionNr&0xFFF;
  int MasterControlMode = -1;
  float MasterLevel = -140;
  unsigned char Mask = 0x00;
  mbn_data data;

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
    case MBN_DATATYPE_UINT:
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

      data.UInt = Position;
      mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_UINT, 2, data, 1);
    }
    break;
    case MBN_DATATYPE_BITS:
    {
      int NrOfLEDs = (MasterLevel+140)/(140/DataMaximal);
      for (char cntBit=0; cntBit<NrOfLEDs; cntBit++)
      {
        Mask |= 0x01<<cntBit;
      }

      data.Bits[0] = Mask;
      mbnSetActuatorData(mbn, MambaNetAddress, ObjectNr, MBN_DATATYPE_BITS, 1, data, 1);
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

      if ((AxumData.ModuleData[cntModule].SelectedSource>=matrix_sources.src_offset.min.source) && (AxumData.ModuleData[cntModule].SelectedSource<=matrix_sources.src_offset.max.source))
      {
        int SourceNr = AxumData.ModuleData[cntModule].SelectedSource-matrix_sources.src_offset.min.source;
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

  if ((AxumData.ModuleData[ModuleNr].SelectedSource>=matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].SelectedSource<=matrix_sources.src_offset.max.source))
  {
    unsigned int SourceNr = AxumData.ModuleData[ModuleNr].SelectedSource-matrix_sources.src_offset.min.source;
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
        if (AxumData.ModuleData[cntModule].SelectedSource == (SourceNr+matrix_sources.src_offset.min.source))
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
        if (AxumData.ModuleData[cntModule].SelectedSource != 0)
        {
          int SourceToCheck = AxumData.ModuleData[cntModule].SelectedSource-matrix_sources.src_offset.min.source;
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
      if ((AxumData.ModuleData[cntModule].SelectedSource>=matrix_sources.src_offset.min.source) && (AxumData.ModuleData[cntModule].SelectedSource<=matrix_sources.src_offset.max.source))
      {
        int SourceNr = AxumData.ModuleData[cntModule].SelectedSource-matrix_sources.src_offset.min.source;

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
      if (AxumData.ModuleData[cntModule].SelectedSource == (CurrentSource+matrix_sources.src_offset.min.source))
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
  unsigned int OldSource = AxumData.ModuleData[ModuleNr].SelectedSource;
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
      AxumData.ModuleData[ModuleNr].SelectedSource = NewSource;
      AxumData.ModuleData[ModuleNr].Cough = 0;

      //eventual 'reset' set a preset?
      LoadSourcePreset(ModuleNr, Forced);

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
      CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_COUGH_ON_OFF);

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
        if (AxumData.ModuleData[cntModule].SelectedSource == AxumData.ModuleData[ModuleNr].SelectedSource)
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

void SetBussOnOff(int ModuleNr, int BussNr, int LoadPreset)
{
  unsigned char Active = 0;

  if ((AxumData.BussMasterData[BussNr].Exclusive) && (!LoadPreset))
  {
    if (AxumData.ModuleData[ModuleNr].On)
    {
      if (AxumData.ModuleData[ModuleNr].FaderLevel>-80)
      {
        Active = 1;
      }
    }
    //TODO: Check if others already or still in exlusive mode...

    if (!Active)
    {
      if (!AxumData.ModuleData[ModuleNr].Buss[BussNr].On)
      {  //return to normal routing
        for (int cntBuss=0; cntBuss<16; cntBuss++)
        {
          if (cntBuss != BussNr)
          {
            if (AxumData.ModuleData[ModuleNr].Buss[cntBuss].On != AxumData.ModuleData[ModuleNr].Buss[cntBuss].PreviousOn)
            {
              AxumData.ModuleData[ModuleNr].Buss[cntBuss].On = AxumData.ModuleData[ModuleNr].Buss[cntBuss].PreviousOn;

              unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
              CheckObjectsToSent(FunctionNrToSent | (MODULE_FUNCTION_BUSS_1_2_ON_OFF+(cntBuss*(MODULE_FUNCTION_BUSS_3_4_ON_OFF-MODULE_FUNCTION_BUSS_1_2_ON_OFF))));

              if (AxumData.ModuleData[ModuleNr].SelectedSource != 0)
              {
                int SourceNr = AxumData.ModuleData[ModuleNr].SelectedSource-1;
                FunctionNrToSent = 0x05000000 | (SourceNr<<12);
                CheckObjectsToSent(FunctionNrToSent | (SOURCE_FUNCTION_MODULE_BUSS_1_2_ON+(cntBuss*(SOURCE_FUNCTION_MODULE_BUSS_3_4_ON-SOURCE_FUNCTION_MODULE_BUSS_1_2_ON))));
                CheckObjectsToSent(FunctionNrToSent | (SOURCE_FUNCTION_MODULE_BUSS_1_2_OFF+(cntBuss*(SOURCE_FUNCTION_MODULE_BUSS_3_4_OFF-SOURCE_FUNCTION_MODULE_BUSS_1_2_OFF))));
                CheckObjectsToSent(FunctionNrToSent | (SOURCE_FUNCTION_MODULE_BUSS_1_2_ON_OFF+(cntBuss*(SOURCE_FUNCTION_MODULE_BUSS_3_4_ON_OFF-SOURCE_FUNCTION_MODULE_BUSS_1_2_ON_OFF))));
              }
            }
          }
        }
      }
      else
      {  //turn off other routing
        for (int cntBuss=0; cntBuss<16; cntBuss++)
        {
          if (cntBuss != BussNr)
          {
            AxumData.ModuleData[ModuleNr].Buss[cntBuss].PreviousOn = AxumData.ModuleData[ModuleNr].Buss[cntBuss].On;
            if (AxumData.ModuleData[ModuleNr].Buss[cntBuss].On != 0)
            {
              AxumData.ModuleData[ModuleNr].Buss[cntBuss].On = 0;

              unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
              CheckObjectsToSent(FunctionNrToSent | (MODULE_FUNCTION_BUSS_1_2_ON_OFF+(cntBuss*(MODULE_FUNCTION_BUSS_3_4_ON_OFF-MODULE_FUNCTION_BUSS_1_2_ON_OFF))));

              if (AxumData.ModuleData[ModuleNr].SelectedSource != 0)
              {
                int SourceNr = AxumData.ModuleData[ModuleNr].SelectedSource-1;
                FunctionNrToSent = 0x05000000 | (SourceNr<<12);
                CheckObjectsToSent(FunctionNrToSent | (SOURCE_FUNCTION_MODULE_BUSS_1_2_ON+(cntBuss*(SOURCE_FUNCTION_MODULE_BUSS_3_4_ON-SOURCE_FUNCTION_MODULE_BUSS_1_2_ON))));
                CheckObjectsToSent(FunctionNrToSent | (SOURCE_FUNCTION_MODULE_BUSS_1_2_OFF+(cntBuss*(SOURCE_FUNCTION_MODULE_BUSS_3_4_OFF-SOURCE_FUNCTION_MODULE_BUSS_1_2_OFF))));
                CheckObjectsToSent(FunctionNrToSent | (SOURCE_FUNCTION_MODULE_BUSS_1_2_ON_OFF+(cntBuss*(SOURCE_FUNCTION_MODULE_BUSS_3_4_ON_OFF-SOURCE_FUNCTION_MODULE_BUSS_1_2_ON_OFF))));
              }
            }
          }
        }
      }
    }
    else
    {
      AxumData.ModuleData[ModuleNr].Buss[BussNr].On = !AxumData.ModuleData[ModuleNr].Buss[BussNr].On;
    }
  }
  SetAxum_BussLevels(ModuleNr);
  SetAxum_ModuleMixMinus(ModuleNr, 0);

  unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
  CheckObjectsToSent(FunctionNrToSent | (MODULE_FUNCTION_BUSS_1_2_ON_OFF+(BussNr*(MODULE_FUNCTION_BUSS_3_4_ON_OFF-MODULE_FUNCTION_BUSS_1_2_ON_OFF))));

  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);

  if ((AxumData.ModuleData[ModuleNr].SelectedSource>=matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].SelectedSource<=matrix_sources.src_offset.max.source))
  {
    int SourceNr = AxumData.ModuleData[ModuleNr].SelectedSource-matrix_sources.src_offset.min.source;
    FunctionNrToSent = 0x05000000 | (SourceNr<<12);
    CheckObjectsToSent(FunctionNrToSent | (SOURCE_FUNCTION_MODULE_BUSS_1_2_ON+(BussNr*(SOURCE_FUNCTION_MODULE_BUSS_3_4_ON-SOURCE_FUNCTION_MODULE_BUSS_1_2_ON))));
    CheckObjectsToSent(FunctionNrToSent | (SOURCE_FUNCTION_MODULE_BUSS_1_2_OFF+(BussNr*(SOURCE_FUNCTION_MODULE_BUSS_3_4_OFF-SOURCE_FUNCTION_MODULE_BUSS_1_2_OFF))));
    CheckObjectsToSent(FunctionNrToSent | (SOURCE_FUNCTION_MODULE_BUSS_1_2_ON_OFF+(BussNr*(SOURCE_FUNCTION_MODULE_BUSS_3_4_ON_OFF-SOURCE_FUNCTION_MODULE_BUSS_1_2_ON_OFF))));
  }

  //Do interlock
  if ((AxumData.BussMasterData[BussNr].Interlock) && (!LoadPreset))
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

          if ((AxumData.ModuleData[cntModule].SelectedSource>=matrix_sources.src_offset.min.source) && (AxumData.ModuleData[cntModule].SelectedSource<=matrix_sources.src_offset.max.source))
          {
            int SourceNr = AxumData.ModuleData[cntModule].SelectedSource-matrix_sources.src_offset.min.source;
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
    AxumData.ModuleData[cntModule].TemporySource = 0;
    AxumData.ModuleData[cntModule].SelectedSource = 0;
    AxumData.ModuleData[cntModule].SourceA = 0;
    AxumData.ModuleData[cntModule].SourceB = 0;
    AxumData.ModuleData[cntModule].SourceC = 0;
    AxumData.ModuleData[cntModule].SourceD = 0;
    AxumData.ModuleData[cntModule].InsertSource = 0;
    AxumData.ModuleData[cntModule].InsertOnOff = 0;
    AxumData.ModuleData[cntModule].Gain = 0;
    AxumData.ModuleData[cntModule].PhaseReverse = 0;

    AxumData.ModuleData[cntModule].Filter.Level = 0;
    AxumData.ModuleData[cntModule].Filter.Frequency = 80;
    AxumData.ModuleData[cntModule].Filter.Bandwidth = 1;
    AxumData.ModuleData[cntModule].Filter.Slope = 1;
    AxumData.ModuleData[cntModule].Filter.Type = HPF;
    AxumData.ModuleData[cntModule].FilterOnOff = 0;

    AxumData.ModuleData[cntModule].EQBand[0].Level = 0;
    AxumData.ModuleData[cntModule].EQBand[0].Frequency = 12000;
    AxumData.ModuleData[cntModule].EQBand[0].Bandwidth = 1;
    AxumData.ModuleData[cntModule].EQBand[0].Slope = 1;
    AxumData.ModuleData[cntModule].EQBand[0].Type = PEAKINGEQ;

    AxumData.ModuleData[cntModule].EQBand[1].Level = 0;
    AxumData.ModuleData[cntModule].EQBand[1].Frequency = 4000;
    AxumData.ModuleData[cntModule].EQBand[1].Bandwidth = 1;
    AxumData.ModuleData[cntModule].EQBand[1].Slope = 1;
    AxumData.ModuleData[cntModule].EQBand[1].Type = PEAKINGEQ;

    AxumData.ModuleData[cntModule].EQBand[2].Level = 0;
    AxumData.ModuleData[cntModule].EQBand[2].Frequency = 800;
    AxumData.ModuleData[cntModule].EQBand[2].Bandwidth = 1;
    AxumData.ModuleData[cntModule].EQBand[2].Slope = 1;
    AxumData.ModuleData[cntModule].EQBand[2].Type = PEAKINGEQ;

    AxumData.ModuleData[cntModule].EQBand[3].Level = 0;
    AxumData.ModuleData[cntModule].EQBand[3].Frequency = 120;
    AxumData.ModuleData[cntModule].EQBand[3].Bandwidth = 1;
    AxumData.ModuleData[cntModule].EQBand[3].Slope = 1;
    AxumData.ModuleData[cntModule].EQBand[3].Type = LOWSHELF;

    AxumData.ModuleData[cntModule].EQBand[4].Level = 0;
    AxumData.ModuleData[cntModule].EQBand[4].Frequency = 300;
    AxumData.ModuleData[cntModule].EQBand[4].Bandwidth = 1;
    AxumData.ModuleData[cntModule].EQBand[4].Slope = 1;
    AxumData.ModuleData[cntModule].EQBand[4].Type = HPF;

    AxumData.ModuleData[cntModule].EQBand[5].Level = 0;
    AxumData.ModuleData[cntModule].EQBand[5].Frequency = 3000;
    AxumData.ModuleData[cntModule].EQBand[5].Bandwidth = 1;
    AxumData.ModuleData[cntModule].EQBand[5].Slope = 1;
    AxumData.ModuleData[cntModule].EQBand[5].Type = LPF;

    AxumData.ModuleData[cntModule].EQOnOff = 1;

    AxumData.ModuleData[cntModule].Dynamics = 0;
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
      AxumData.ModuleData[cntModule].Buss[cntBuss].PreviousOn = 0;
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

    AxumData.ModuleData[cntModule].Defaults.SourceA = 0;
    AxumData.ModuleData[cntModule].Defaults.SourceB = 0;
    AxumData.ModuleData[cntModule].Defaults.SourceC = 0;
    AxumData.ModuleData[cntModule].Defaults.SourceD = 0;
    AxumData.ModuleData[cntModule].Defaults.InsertSource = 0;
    AxumData.ModuleData[cntModule].Defaults.InsertOnOff = 0;
    AxumData.ModuleData[cntModule].Defaults.Gain = 0;
    AxumData.ModuleData[cntModule].Defaults.PhaseReverse = 0;

    AxumData.ModuleData[cntModule].Defaults.Filter.Level = 0;
    AxumData.ModuleData[cntModule].Defaults.Filter.Frequency = 80;
    AxumData.ModuleData[cntModule].Defaults.Filter.Bandwidth = 1;
    AxumData.ModuleData[cntModule].Defaults.Filter.Slope = 1;
    AxumData.ModuleData[cntModule].Defaults.Filter.Type = HPF;
    AxumData.ModuleData[cntModule].Defaults.FilterOnOff = 0;

    AxumData.ModuleData[cntModule].Defaults.EQBand[0].Level = 0;
    AxumData.ModuleData[cntModule].Defaults.EQBand[0].Frequency = 12000;
    AxumData.ModuleData[cntModule].Defaults.EQBand[0].Bandwidth = 1;
    AxumData.ModuleData[cntModule].Defaults.EQBand[0].Slope = 1;
    AxumData.ModuleData[cntModule].Defaults.EQBand[0].Type = PEAKINGEQ;

    AxumData.ModuleData[cntModule].Defaults.EQBand[1].Level = 0;
    AxumData.ModuleData[cntModule].Defaults.EQBand[1].Frequency = 4000;
    AxumData.ModuleData[cntModule].Defaults.EQBand[1].Bandwidth = 1;
    AxumData.ModuleData[cntModule].Defaults.EQBand[1].Slope = 1;
    AxumData.ModuleData[cntModule].Defaults.EQBand[1].Type = PEAKINGEQ;

    AxumData.ModuleData[cntModule].Defaults.EQBand[2].Level = 0;
    AxumData.ModuleData[cntModule].Defaults.EQBand[2].Frequency = 800;
    AxumData.ModuleData[cntModule].Defaults.EQBand[2].Bandwidth = 1;
    AxumData.ModuleData[cntModule].Defaults.EQBand[2].Slope = 1;
    AxumData.ModuleData[cntModule].Defaults.EQBand[2].Type = PEAKINGEQ;

    AxumData.ModuleData[cntModule].Defaults.EQBand[3].Level = 0;
    AxumData.ModuleData[cntModule].Defaults.EQBand[3].Frequency = 120;
    AxumData.ModuleData[cntModule].Defaults.EQBand[3].Bandwidth = 1;
    AxumData.ModuleData[cntModule].Defaults.EQBand[3].Slope = 1;
    AxumData.ModuleData[cntModule].Defaults.EQBand[3].Type = LOWSHELF;

    AxumData.ModuleData[cntModule].Defaults.EQBand[4].Level = 0;
    AxumData.ModuleData[cntModule].Defaults.EQBand[4].Frequency = 300;
    AxumData.ModuleData[cntModule].Defaults.EQBand[4].Bandwidth = 1;
    AxumData.ModuleData[cntModule].Defaults.EQBand[4].Slope = 1;
    AxumData.ModuleData[cntModule].Defaults.EQBand[4].Type = HPF;

    AxumData.ModuleData[cntModule].Defaults.EQBand[5].Level = 0;
    AxumData.ModuleData[cntModule].Defaults.EQBand[5].Frequency = 3000;
    AxumData.ModuleData[cntModule].Defaults.EQBand[5].Bandwidth = 1;
    AxumData.ModuleData[cntModule].Defaults.EQBand[5].Slope = 1;
    AxumData.ModuleData[cntModule].Defaults.EQBand[5].Type = LPF;

    AxumData.ModuleData[cntModule].Defaults.EQOnOff = 1;

    AxumData.ModuleData[cntModule].Defaults.Dynamics = 0;
    AxumData.ModuleData[cntModule].Defaults.DynamicsOnOff = 0;

    AxumData.ModuleData[cntModule].Defaults.Panorama = 512;
    AxumData.ModuleData[cntModule].Defaults.Mono = 0;

    AxumData.ModuleData[cntModule].Defaults.FaderLevel = -140;
    AxumData.ModuleData[cntModule].Defaults.On = 0;
  }

  AxumData.Control1Mode = MODULE_CONTROL_MODE_NONE;
  AxumData.Control2Mode = MODULE_CONTROL_MODE_NONE;
  AxumData.Control3Mode = MODULE_CONTROL_MODE_NONE;
  AxumData.Control4Mode = MODULE_CONTROL_MODE_NONE;
  AxumData.MasterControl1Mode = MASTER_CONTROL_MODE_NONE;
  AxumData.MasterControl2Mode = MASTER_CONTROL_MODE_NONE;
  AxumData.MasterControl3Mode = MASTER_CONTROL_MODE_NONE;
  AxumData.MasterControl4Mode = MASTER_CONTROL_MODE_NONE;

  for (int cntBuss=0; cntBuss<16; cntBuss++)
  {
    AxumData.BussMasterData[cntBuss].Label[0] = 0;
    AxumData.BussMasterData[cntBuss].Level = 0;
    AxumData.BussMasterData[cntBuss].On = 1;

    AxumData.BussMasterData[cntBuss].PreModuleOn = 0;
    AxumData.BussMasterData[cntBuss].PreModuleLevel = 0;
    AxumData.BussMasterData[cntBuss].PreModuleBalance = 0;

    AxumData.BussMasterData[cntBuss].Interlock = 0;
    AxumData.BussMasterData[cntBuss].Exclusive = 0;
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
  AxumData.AutoMomentary = false;
  AxumData.UseModuleDefaults = true;

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
  bool Found = false;
//  pthread_mutex_lock(&get_node_info_mutex);
  ONLINE_NODE_INFORMATION_STRUCT *OnlineNodeInformationElement = OnlineNodeInformationList;
  ONLINE_NODE_INFORMATION_STRUCT *FoundOnlineNodeInformationElement = NULL;

  while ((!Found) && (OnlineNodeInformationElement != NULL))
  {
    if (OnlineNodeInformationElement->MambaNetAddress == addr)
    {
      Found = true;
      FoundOnlineNodeInformationElement = OnlineNodeInformationElement;
    }
    OnlineNodeInformationElement = OnlineNodeInformationElement->Next;
  }
//  pthread_mutex_unlock(&get_node_info_mutex);

  return FoundOnlineNodeInformationElement;
}

void LoadSourcePreset(unsigned char ModuleNr, unsigned char SetAllObjects)
{
  bool SetModuleProcessing = false;
  bool SetModuleControllers = false;
  unsigned char cntEQ;
  //parameters per module
  float Gain = 0;
  unsigned int Frequency = 80;
  bool FilterOnOff = 0;
  unsigned int InsertSource = 0;
  bool InsertOnOff = 0;
  bool EQOnOff;
  AXUM_EQ_BAND_PRESET_STRUCT EQBand[6];
  int Dynamics = 0;
  bool DynamicsOnOff = 0;

  if ((AxumData.ModuleData[ModuleNr].SelectedSource>=matrix_sources.src_offset.min.source) &&
      (AxumData.ModuleData[ModuleNr].SelectedSource<=matrix_sources.src_offset.max.source))
  {
    unsigned int SourceNr = AxumData.ModuleData[ModuleNr].SelectedSource-matrix_sources.src_offset.min.source;
    AXUM_SOURCE_DATA_STRUCT *SourceData = &AxumData.SourceData[SourceNr];

    if ((SourceData->Preset.UseGain) || (AxumData.UseModuleDefaults))
    {
      if (SourceData->Preset.UseGain)
      {
        Gain = SourceData->Preset.Gain;
      }
      else
      {
        Gain = AxumData.ModuleData[ModuleNr].Defaults.Gain;
      }
    }

    if ((SourceData->Preset.UseFilter) || (AxumData.UseModuleDefaults))
    {
      if (SourceData->Preset.UseFilter)
      {
        Frequency = SourceData->Preset.Filter.Frequency;
        FilterOnOff = SourceData->Preset.FilterOnOff;
      }
      else
      {
        Frequency = AxumData.ModuleData[ModuleNr].Defaults.Filter.Frequency;
        FilterOnOff = AxumData.ModuleData[ModuleNr].Defaults.FilterOnOff;
      }
    }

    if ((SourceData->Preset.UseInsert) || (AxumData.UseModuleDefaults))
    {
      if (SourceData->Preset.UseInsert)
      {
        InsertSource = SourceData->Preset.InsertSource;
        InsertOnOff = SourceData->Preset.InsertOnOff;
      }
      else
      {
        InsertSource = AxumData.ModuleData[ModuleNr].Defaults.InsertSource;
        InsertOnOff = AxumData.ModuleData[ModuleNr].Defaults.InsertOnOff;
      }
    }

    if ((SourceData->Preset.UseEQ) || (AxumData.UseModuleDefaults))
    {
      if (SourceData->Preset.UseEQ)
      {
        EQOnOff = SourceData->Preset.EQOnOff;
        for (cntEQ=0; cntEQ<6; cntEQ++)
        {
          EQBand[cntEQ].Range = SourceData->Preset.EQBand[cntEQ].Range;
          EQBand[cntEQ].Level = SourceData->Preset.EQBand[cntEQ].Level;
          EQBand[cntEQ].Frequency = SourceData->Preset.EQBand[cntEQ].Frequency;
          EQBand[cntEQ].Bandwidth = SourceData->Preset.EQBand[cntEQ].Bandwidth;
          EQBand[cntEQ].Slope = SourceData->Preset.EQBand[cntEQ].Slope;
          EQBand[cntEQ].Type = SourceData->Preset.EQBand[cntEQ].Type;
        }
      }
      else
      {
        EQOnOff = AxumData.ModuleData[ModuleNr].Defaults.EQOnOff;
        for (cntEQ=0; cntEQ<6; cntEQ++)
        {
          EQBand[cntEQ].Range = AxumData.ModuleData[ModuleNr].Defaults.EQBand[cntEQ].Range;
          EQBand[cntEQ].Level = AxumData.ModuleData[ModuleNr].Defaults.EQBand[cntEQ].Level;
          EQBand[cntEQ].Frequency = AxumData.ModuleData[ModuleNr].Defaults.EQBand[cntEQ].Frequency;
          EQBand[cntEQ].Bandwidth = AxumData.ModuleData[ModuleNr].Defaults.EQBand[cntEQ].Bandwidth;
          EQBand[cntEQ].Slope = AxumData.ModuleData[ModuleNr].Defaults.EQBand[cntEQ].Slope;
          EQBand[cntEQ].Type = AxumData.ModuleData[ModuleNr].Defaults.EQBand[cntEQ].Type;
        }
      }
    }

    if ((SourceData->Preset.UseDynamics) || (AxumData.UseModuleDefaults))
    {
      if (SourceData->Preset.UseDynamics)
      {
        Dynamics = SourceData->Preset.Dynamics;
        DynamicsOnOff = SourceData->Preset.DynamicsOnOff;
      }
      else
      {
        Dynamics = AxumData.ModuleData[ModuleNr].Defaults.Dynamics;
        DynamicsOnOff = AxumData.ModuleData[ModuleNr].Defaults.DynamicsOnOff;
      }
    }

    if ((SourceData->Preset.UseRouting) || (AxumData.UseModuleDefaults))
    {
      if (SourceData->Preset.UseRouting)
      {
        LoadRoutingPreset(ModuleNr, SourceData->Preset.RoutingPreset, SetAllObjects);
      }
      else
      {
        LoadRoutingPreset(ModuleNr, 0, SetAllObjects);
      }
    }
  }
  else if (AxumData.UseModuleDefaults)
  {
    Gain = AxumData.ModuleData[ModuleNr].Defaults.Gain;

    Frequency = AxumData.ModuleData[ModuleNr].Defaults.Filter.Frequency;
    FilterOnOff = AxumData.ModuleData[ModuleNr].Defaults.FilterOnOff;

    InsertSource = AxumData.ModuleData[ModuleNr].Defaults.InsertSource;
    InsertOnOff = AxumData.ModuleData[ModuleNr].Defaults.InsertOnOff;
    for (cntEQ=0; cntEQ<6; cntEQ++)
    {
      EQBand[cntEQ].Range = AxumData.ModuleData[ModuleNr].Defaults.EQBand[cntEQ].Range;
      EQBand[cntEQ].Level = AxumData.ModuleData[ModuleNr].Defaults.EQBand[cntEQ].Level;
      EQBand[cntEQ].Frequency = AxumData.ModuleData[ModuleNr].Defaults.EQBand[cntEQ].Frequency;
      EQBand[cntEQ].Bandwidth = AxumData.ModuleData[ModuleNr].Defaults.EQBand[cntEQ].Bandwidth;
      EQBand[cntEQ].Slope = AxumData.ModuleData[ModuleNr].Defaults.EQBand[cntEQ].Slope;
      EQBand[cntEQ].Type = AxumData.ModuleData[ModuleNr].Defaults.EQBand[cntEQ].Type;
    }
    EQOnOff = AxumData.ModuleData[ModuleNr].Defaults.EQOnOff;
    Dynamics = AxumData.ModuleData[ModuleNr].Defaults.Dynamics;
    DynamicsOnOff = AxumData.ModuleData[ModuleNr].Defaults.DynamicsOnOff;

    LoadRoutingPreset(ModuleNr, 0, SetAllObjects);
  }

  //Set if there is a difference
  if ((AxumData.ModuleData[ModuleNr].Gain != Gain) || (SetAllObjects))
  {
    unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);

    AxumData.ModuleData[ModuleNr].Gain = Gain;
    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_GAIN_LEVEL);

    SetModuleProcessing = true;
    SetModuleControllers = true;
  }
  if ((AxumData.ModuleData[ModuleNr].Filter.Frequency != Frequency) || (SetAllObjects))
  {
    unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
    AxumData.ModuleData[ModuleNr].Filter.Frequency = Frequency;

    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_LOW_CUT_FREQUENCY);
    SetModuleProcessing = true;
    SetModuleControllers = true;
  }

  if ((AxumData.ModuleData[ModuleNr].FilterOnOff != FilterOnOff) || (SetAllObjects))
  {
    unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
    AxumData.ModuleData[ModuleNr].FilterOnOff = FilterOnOff;

    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_LOW_CUT_ON_OFF);

    SetModuleProcessing = true;
    SetModuleControllers = true;
  }

  if ((AxumData.ModuleData[ModuleNr].InsertSource != InsertSource) || (SetAllObjects))
  {
    AxumData.ModuleData[ModuleNr].InsertSource = InsertSource;
    SetAxum_ModuleInsertSource(ModuleNr);
    SetModuleControllers = true;
  }

  if ((AxumData.ModuleData[ModuleNr].InsertOnOff != InsertOnOff) || (SetAllObjects))
  {
    unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
    AxumData.ModuleData[ModuleNr].InsertOnOff = InsertOnOff;
    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_INSERT_ON_OFF);

    SetModuleProcessing = true;
    SetModuleControllers = true;
  }

  if ((AxumData.ModuleData[ModuleNr].EQOnOff != EQOnOff) || (SetAllObjects))
  {
    unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);

    AxumData.ModuleData[ModuleNr].EQOnOff = EQOnOff;
    for (int cntBand=0; cntBand<6; cntBand++)
    {
      SetAxum_EQ(ModuleNr, cntBand);
    }

    FunctionNrToSent = (ModuleNr<<12) | MODULE_FUNCTION_EQ_ON_OFF;
    CheckObjectsToSent(FunctionNrToSent);

    SetModuleControllers = true;
  }

  for (cntEQ=0; cntEQ<6; cntEQ++)
  {
    unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
    bool EQBandChanged = false;

    if ((AxumData.ModuleData[ModuleNr].EQBand[cntEQ].Level != EQBand[cntEQ].Level) || (SetAllObjects))
    {
      AxumData.ModuleData[ModuleNr].EQBand[cntEQ].Level = EQBand[cntEQ].Level;
      CheckObjectsToSent(FunctionNrToSent | (MODULE_FUNCTION_EQ_BAND_1_LEVEL+(cntEQ*(MODULE_FUNCTION_EQ_BAND_2_LEVEL-MODULE_FUNCTION_EQ_BAND_1_LEVEL))));
      EQBandChanged = true;
    }
    if ((AxumData.ModuleData[ModuleNr].EQBand[cntEQ].Frequency != EQBand[cntEQ].Frequency) || (SetAllObjects))
    {
      AxumData.ModuleData[ModuleNr].EQBand[cntEQ].Frequency = EQBand[cntEQ].Frequency;
      CheckObjectsToSent(FunctionNrToSent | (MODULE_FUNCTION_EQ_BAND_1_FREQUENCY+(cntEQ*(MODULE_FUNCTION_EQ_BAND_2_FREQUENCY-MODULE_FUNCTION_EQ_BAND_1_FREQUENCY))));
      EQBandChanged = true;
    }
    if ((AxumData.ModuleData[ModuleNr].EQBand[cntEQ].Bandwidth != EQBand[cntEQ].Bandwidth) || (SetAllObjects))
    {
      AxumData.ModuleData[ModuleNr].EQBand[cntEQ].Bandwidth = EQBand[cntEQ].Bandwidth;
      CheckObjectsToSent(FunctionNrToSent | (MODULE_FUNCTION_EQ_BAND_1_BANDWIDTH+(cntEQ*(MODULE_FUNCTION_EQ_BAND_2_BANDWIDTH-MODULE_FUNCTION_EQ_BAND_1_BANDWIDTH))));
      EQBandChanged = true;
    }
    if ((AxumData.ModuleData[ModuleNr].EQBand[cntEQ].Type != EQBand[cntEQ].Type) || (SetAllObjects))
    {
      AxumData.ModuleData[ModuleNr].EQBand[cntEQ].Type = EQBand[cntEQ].Type;
      CheckObjectsToSent(FunctionNrToSent | (MODULE_FUNCTION_EQ_BAND_1_TYPE+(cntEQ*(MODULE_FUNCTION_EQ_BAND_2_TYPE-MODULE_FUNCTION_EQ_BAND_1_TYPE))));
      EQBandChanged = true;
    }

    if (EQBandChanged)
    {
      SetAxum_EQ(ModuleNr, cntEQ);
      SetModuleControllers = true;
    }
  }
  if ((AxumData.ModuleData[ModuleNr].Dynamics != Dynamics) || (SetAllObjects))
  {
    AxumData.ModuleData[ModuleNr].Dynamics = Dynamics;
    unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_DYNAMICS_AMOUNT);
    SetModuleProcessing = true;
    SetModuleControllers = true;
  }
  if ((AxumData.ModuleData[ModuleNr].DynamicsOnOff != DynamicsOnOff) || (SetAllObjects))
  {
    AxumData.ModuleData[ModuleNr].DynamicsOnOff = DynamicsOnOff;
    unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_DYNAMICS_ON_OFF);
    SetModuleProcessing = true;
    SetModuleControllers = true;
  }

  if (SetModuleProcessing)
  {
    SetAxum_ModuleProcessing(ModuleNr);
  }

  if (SetModuleControllers)
  {
    unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
  }
}

void LoadRoutingPreset(unsigned char ModuleNr, unsigned char PresetNr, unsigned char SetAllObjects)
{
  unsigned char cntBuss;
  AXUM_ROUTING_PRESET_STRUCT *RoutingPreset = AxumData.ModuleData[ModuleNr].RoutingPreset[PresetNr];
  bool BussChanged = false;
  bool SetModuleControllers = false;
  unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);

  for (cntBuss=0; cntBuss<16; cntBuss++)
  {
    float Level = -140;
    bool On = 0;
    signed int Balance = 512;
    bool PreModuleLevel = 0;

    Level = RoutingPreset[cntBuss].Level;
    On = RoutingPreset[cntBuss].On;
    Balance = RoutingPreset[cntBuss].Balance;
    PreModuleLevel = RoutingPreset[cntBuss].PreModuleLevel;

    if((AxumData.ModuleData[ModuleNr].Buss[cntBuss].Level != Level) || (SetAllObjects))
    {
      AxumData.ModuleData[ModuleNr].Buss[cntBuss].Level = Level;
      CheckObjectsToSent(FunctionNrToSent | (MODULE_FUNCTION_BUSS_1_2_LEVEL+(cntBuss*(MODULE_FUNCTION_BUSS_3_4_LEVEL-MODULE_FUNCTION_BUSS_1_2_LEVEL))));
      BussChanged = true;
      SetModuleControllers = true;
    }
    if((AxumData.ModuleData[ModuleNr].Buss[cntBuss].On != On) || (SetAllObjects))
    {
      AxumData.ModuleData[ModuleNr].Buss[cntBuss].On = On;
      SetBussOnOff(ModuleNr, cntBuss, 1);
      SetModuleControllers = true;
    }
    if((AxumData.ModuleData[ModuleNr].Buss[cntBuss].Balance != Balance) || (SetAllObjects))
    {
      AxumData.ModuleData[ModuleNr].Buss[cntBuss].Balance = Balance;
      CheckObjectsToSent(FunctionNrToSent | (MODULE_FUNCTION_BUSS_1_2_BALANCE+(cntBuss*(MODULE_FUNCTION_BUSS_3_4_BALANCE-MODULE_FUNCTION_BUSS_1_2_BALANCE))));
      BussChanged = true;
      SetModuleControllers = true;
    }
    if((AxumData.ModuleData[ModuleNr].Buss[cntBuss].PreModuleLevel != PreModuleLevel) || (SetAllObjects))
    {
      AxumData.ModuleData[ModuleNr].Buss[cntBuss].PreModuleLevel = PreModuleLevel;
      CheckObjectsToSent(FunctionNrToSent | (MODULE_FUNCTION_BUSS_1_2_PRE+(cntBuss*(MODULE_FUNCTION_BUSS_3_4_PRE-MODULE_FUNCTION_BUSS_1_2_PRE))));
      BussChanged = true;
      SetModuleControllers = true;
    }
  }

  if (BussChanged)
  {
    SetAxum_BussLevels(ModuleNr);
  }
  if (SetModuleControllers)
  {
    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
    CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
  }
}
