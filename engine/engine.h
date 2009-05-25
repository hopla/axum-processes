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

#ifndef _engine_h
#define _engine_h

#include <stdio.h>
#include "engine_functions.h"
#include "mambanet_stack_axum.h"


enum FilterType {OFF=0, HPF=1, LOWSHELF=2, PEAKINGEQ=3, HIGHSHELF=4, LPF=5, BPF=6, NOTCH=7};

//**************************************************************/
//Axum Data definitions
//**************************************************************/
typedef struct
{
  unsigned int MambaNetAddress;
  unsigned char SubChannel;
} AXUM_INPUT_DATA_STRUCT;

typedef struct
{
  char SourceName[32];
  AXUM_INPUT_DATA_STRUCT InputData[8];
  char Redlight[8];
  char MonitorMute[16];
  char Active;

  char Start;
  char Phantom;
  char Pad;
  float Gain;
  char Alert;
} AXUM_SOURCE_DATA_STRUCT;

typedef struct
{
  unsigned int MambaNetAddress;
  unsigned char SubChannel;
} AXUM_OUTPUT_DATA_STRUCT;

typedef struct
{
  char DestinationName[32];
  AXUM_OUTPUT_DATA_STRUCT OutputData[8];

  int Source;
  float Level;
  unsigned char Mute;
  unsigned char Dim;
  unsigned char Mono;
  unsigned char Phase;
  unsigned char Talkback[16];

  int MixMinusSource;
  unsigned char MixMinusActive;

} AXUM_DESTINATION_DATA_STRUCT;

typedef struct
{
  //realtime parameters
  unsigned char On;
  float Level;
  unsigned int Frequency;
  float Bandwidth;
  float Slope;
  FilterType Type;

  //configuration parameters
  float Range;
  unsigned int DefaultFrequency;
  float DefaultBandwidth;
  float DefaultSlope;
  FilterType DefaultType;
} AXUM_EQ_BAND_DATA_STRUCT;

typedef struct
{
  float Level;
  unsigned char On;
  int Balance;
  unsigned char PreModuleLevel;

  unsigned char Active;
} AXUM_BUSS_DATA_STRUCT;

typedef struct
{
  int Source;
  int SourceA;
  int SourceB;
  int InsertSource;
  unsigned char Insert;
  char InsertOnOffA;
  char InsertOnOffB;
  float Gain;
  unsigned char PhaseReverse;
  AXUM_EQ_BAND_DATA_STRUCT Filter;
  char FilterOnOffA;
  char FilterOnOffB;
  AXUM_EQ_BAND_DATA_STRUCT EQBand[6];
  char EQOn;
  char EQOnOffA;
  char EQOnOffB;

  char Dynamics;
  char DynamicsOn;
  char DynamicsOnOffA;
  char DynamicsOnOffB;

  int Panorama;
  char Mono;
  float FaderLevel;
  unsigned char FaderTouch;
  unsigned char On;
  unsigned char Cough;

  char Signal;
  char Peak;

  AXUM_BUSS_DATA_STRUCT Buss[16];

} AXUM_MODULE_DATA_STRUCT;

typedef struct
{
  char Label[32];
  char Interlock;
  char DefaultSelection;
  char AutoSwitchingBuss[16];
  float SwitchingDimLevel;

  unsigned char Buss[16];
  unsigned char Ext[8];

  float PhonesLevel;
  float SpeakerLevel;

  unsigned char Dim;
  unsigned char Mute;
  unsigned char Mono;
  unsigned char Phase;
  unsigned char Talkback[16];
} AXUM_MONITOR_OUTPUT_DATA_STRUCT;

typedef struct
{
  int Ext[8];
} AXUM_EXTERN_SOURCE_DATA_STRUCT;

typedef struct
{
  float Level;
  char Label[32];
  unsigned char On;

  unsigned char PreModuleOn;
  unsigned char PreModuleLevel;
  unsigned char PreModuleBalance;

  unsigned char Interlock;
  unsigned char GlobalBussReset;
} AXUM_BUSS_MASTER_DATA_STRUCT;

typedef struct
{
  int Source;
} AXUM_TALKBACK_STRUCT;

typedef struct
{
  unsigned int RackOrganization[42];
  AXUM_SOURCE_DATA_STRUCT SourceData[1280];
  AXUM_DESTINATION_DATA_STRUCT DestinationData[1280];
  AXUM_MODULE_DATA_STRUCT ModuleData[128];
  AXUM_BUSS_MASTER_DATA_STRUCT BussMasterData[16];

  char Control1Mode;
  char Control2Mode;
  char Control3Mode;
  char Control4Mode;
  char MasterControl1Mode;
  char MasterControl2Mode;
  char MasterControl3Mode;
  char MasterControl4Mode;

  unsigned char Redlight[8];
  unsigned int Samplerate;
  unsigned char ExternClock;
  float Headroom;
  float LevelReserve;

  AXUM_MONITOR_OUTPUT_DATA_STRUCT Monitor[16];
  AXUM_EXTERN_SOURCE_DATA_STRUCT ExternSource[4];
  AXUM_TALKBACK_STRUCT Talkback[16];
} AXUM_DATA_STRUCT;


//**************************************************************/
//DSP Data definitions
//**************************************************************/
typedef struct
{
  unsigned char On;
  float Level;
  unsigned int Frequency;
  float Bandwidth;
  float Slope;
  FilterType Type;
} DSPCARD_EQ_BAND_DATA_STRUCT;

typedef struct
{
  int Percent;
  unsigned char On;
} DSPCARD_DYNAMICS_DATA_STRUCT;

typedef struct
{
  float Level;
  unsigned char On;
} DSPCARD_BUSS_DATA_STRUCT;

typedef struct
{
  int Source;
  float Gain;
  unsigned char PhaseReverse;
  unsigned char Insert;
  DSPCARD_EQ_BAND_DATA_STRUCT Filter;
  DSPCARD_EQ_BAND_DATA_STRUCT EQBand[6];

  DSPCARD_DYNAMICS_DATA_STRUCT Dynamics;

  DSPCARD_BUSS_DATA_STRUCT Buss[32];

} DSPCARD_CHANNEL_DATA_STRUCT;

typedef struct
{
  float Level[48];
  float MasterLevel;
} DSPCARD_MONITOR_CHANNEL_DATA_STRUCT;

typedef struct
{
  float Level;
  unsigned char On;
} DSPCARD_BUSS_MASTER_DATA_STRUCT;

typedef struct
{
  int Buss;
} DSPCARD_MIXMINUS_DATA_STRUCT;

typedef struct
{
  DSPCARD_CHANNEL_DATA_STRUCT ChannelData[64];
  DSPCARD_BUSS_MASTER_DATA_STRUCT BussMasterData[32];
  DSPCARD_MONITOR_CHANNEL_DATA_STRUCT MonitorChannelData[8];
  DSPCARD_MIXMINUS_DATA_STRUCT MixMinusData[64];
} DSPCARD_DATA_STRUCT;


//**************************************************************/
//MambaNet Node information definitions
//**************************************************************/
typedef struct
{
  char        Description[32];
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

  unsigned int  AxumFunctionNr;
} OBJECT_INFORMATION_STRUCT;

typedef struct
{
  unsigned int MambaNetAddress;
  unsigned int ObjectNr;
  unsigned char ActuatorDataType;
  unsigned char ActuatorDataSize;
  float ActuatorDataMinimal;
  float ActuatorDataMaximal;
  void *Next;
} AXUM_FUNCTION_INFORMATION_STRUCT;

typedef struct
{
  unsigned int FunctionNr;
  unsigned long LastChangedTime;
  unsigned long PreviousLastChangedTime;
  int TimeBeforeMomentary;
} SENSOR_RECEIVE_FUNCTION_STRUCT;

typedef struct
{
  unsigned int MambaNetAddress;
  unsigned int ManufacturerID;
  unsigned int ProductID;
  unsigned int UniqueIDPerProduct;
  int FirmwareMajorRevision;
//  int FirmwareMinorRevision;

//Not sure if should be stored here...
  int SlotNumberObjectNr;
  int InputChannelCountObjectNr;
  int OutputChannelCountObjectNr;

  struct
  {
    unsigned int ManufacturerID;
    unsigned int ProductID;
    unsigned int UniqueIDPerProduct;
  } Parent;

  int NumberOfCustomObjects;

  //int *SensorReceiveFunction;
  SENSOR_RECEIVE_FUNCTION_STRUCT *SensorReceiveFunction;
  OBJECT_INFORMATION_STRUCT *ObjectInformation;

} ONLINE_NODE_INFORMATION_STRUCT;

float CalculateEQ(float *Coefficients, float Gain, int Frequency, float Bandwidth, float Slope, FilterType Type);

//function to read the command line in this applications format
void GetCommandLineArguments(int argc, char *argv[], char *TTYDevice, char *NetworkInterface, unsigned char *TraceValue);

//requiered function for the MambaNet stack.
void MambaNetMessageReceived(unsigned long int ToAddress, unsigned long int FromAddress, unsigned long int MessageID, unsigned int MessageType, unsigned char *Data, unsigned char DataLength, unsigned char *FromHardwareAddress=NULL);

//debug function
void dump_block(const unsigned char *block, unsigned int length);

void SetupSTDIN(struct termios *oldtio, int *oldflags);
int SetupNetwork(char *NetworkInterface, unsigned char *LocalMACAddress);

void CloseSTDIN(struct termios *oldtio, int oldflags);
void CloseNetwork(int NetworkFileDescriptor);

void EthernetMambaNetMessageTransmitCallback(unsigned char *buffer, unsigned char buffersize, unsigned char hardware_address[16]);
void EthernetMambaNetMessageReceiveCallback(unsigned long int ToAddress, unsigned long int FromAddress, unsigned char Ack, unsigned long int MessageID, unsigned int MessageType, unsigned char *Data, unsigned char DataLength, unsigned char *FromHardwareAddress);
void EthernetMambaNetAddressTableChangeCallback(MAMBANET_ADDRESS_STRUCT *AddressTable, MambaNetAddressTableStatus Status, int Index);

void Timer100HzDone(int Value);

int delay_ms(double sleep_time);
int delay_us(double sleep_time);
bool ProgramEEPROM(int fd);

void SetDSPCard_EQ(unsigned int DSPCardChannelNr, unsigned char BandNr);
void SetDSPCard_ChannelProcessing(unsigned int DSPCardChannelNr);
void SetDSPCard_BussLevels(unsigned int DSPCardChannelNr);
void SetDSPCard_MixMinus(unsigned int DSPCardChannelNr);

void SetBackplane_Source(unsigned int FormInputNr, unsigned int ChannelNr);
void SetBackplane_Clock();

void SetAxum_EQ(unsigned char ModuleNr, unsigned char BandNr);
void SetAxum_ModuleProcessing(unsigned int ModuleNr);
void SetAxum_BussLevels(unsigned int ChannelNr);
void SetAxum_ModuleSource(unsigned int ModuleNr);
void SetAxum_ModuleMixMinus(unsigned int ModuleNr);
void SetAxum_ModuleInsertSource(unsigned int ModuleNr);
void SetAxum_DestinationSource(unsigned int DestinationNr);
void SetAxum_ExternSources(unsigned int MonitorBussPerFourNr);
void SetAxum_TalkbackSource(unsigned int TalkbackNr);

void SetModule_Switch(unsigned int SwitchNr, unsigned int ModuleNr, unsigned char State);

//void SetModule(unsigned int ModuleNr);
void SetCRM_Switch(unsigned int SwitchNr, unsigned char State);
void SetCRM_LEDBar(char NrOfLEDs);

void SetAxum_BussMasterLevels();
void SetDSP_BussMasterLevels();
//void SetCRM();

void SetDSPCard_Interpolation();

void SetDSPCard_MonitorChannel(unsigned int DSPCardMonitorChannelNr);
void SetAxum_MonitorBuss(unsigned int MonitorBussNr);

void SetModule_LEDs(unsigned int LEDNr, unsigned int ModuleNr, unsigned char State);
void SetModule_Fader(unsigned int ModuleNr, unsigned int Position);

void CheckObjectsToSent(unsigned int SensorReceiveFunctionNumber, unsigned int MambaNetAddress=0x00000000);
void SentDataToObject(unsigned int SensorReceiveFunctionNumber, unsigned int MambaNetAddress, unsigned int ObjectNr, unsigned char DataType, unsigned char DataSize, float DataMinimal, float DataMaximal);

void InitalizeAllObjectListPerFunction();
void MakeObjectListPerFunction(unsigned int SensorReceiveFunctionNumber);
void DeleteAllObjectListPerFunction();

void ModeControllerSensorChange(unsigned int SensorReceiveFunctionNr, unsigned char *Data, unsigned char DataType, unsigned char DataSize, float DataMinimal, float DataMaximal);
void ModeControllerResetSensorChange(unsigned int SensorReceiveFunctionNr, unsigned char *Data, unsigned char DataType, unsigned char DataSize, float DataMinimal, float DataMaximal);
void ModeControllerSetData(unsigned int SensorReceiveFunctionNr, unsigned int MambaNetAddress, unsigned int ObjectNr, unsigned char DataType, unsigned char DataSize, float DataMinimal, float DataMaximal);
void ModeControllerSetLabel(unsigned int SensorReceiveFunctionNr, unsigned int MambaNetAddress, unsigned int ObjectNr, unsigned char DataType, unsigned char DataSize, float DataMinimal, float DataMaximal);

void MasterModeControllerSensorChange(unsigned int SensorReceiveFunctionNr, unsigned char *Data, unsigned char DataType, unsigned char DataSize, float DataMinimal, float DataMaximal);
void MasterModeControllerResetSensorChange(unsigned int SensorReceiveFunctionNr, unsigned char *Data, unsigned char DataType, unsigned char DataSize, float DataMinimal, float DataMaximal);
void MasterModeControllerSetData(unsigned int SensorReceiveFunctionNr, unsigned int MambaNetAddress, unsigned int ObjectNr, unsigned char DataType, unsigned char DataSize, float DataMinimal, float DataMaximal);

void DoAxum_BussReset(int BussNr);
void DoAxum_ModuleStatusChanged(int ModuleNr);
void DoAxum_ModulePreStatusChanged(int BussNr);

int Axum_MixMinusSourceUsed(int CurrentSource);

//functions to set all objects connected to 'these' functions
void SetNewSource(int ModuleNr, int NewSource, int Forced, int ApplyAorBSettings);
void SetBussOnOff(int ModuleNr, int BussNr, int UseInterlock);

void initialize_axum_data_struct();

#endif
