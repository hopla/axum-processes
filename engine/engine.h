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

#define DEFAULT_TIME_BEFORE_MOMENTARY 750

#define bool unsigned char

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
  float Range;
  float Level;
  unsigned int Frequency;
  float Bandwidth;
  float Slope;
  FilterType Type;
} AXUM_EQ_BAND_PRESET_STRUCT;

typedef struct {
  char PresetName[32];
  char Type;

  bool UseGain;
  float Gain;

  bool UseFilter;
  AXUM_EQ_BAND_PRESET_STRUCT Filter;
  bool FilterOnOff;

  bool UseInsert;
  bool InsertOnOff;

  bool UsePhase;
  unsigned char Phase;
  bool PhaseOnOff;

  bool UseMono;
  unsigned char Mono;
  bool MonoOnOff;

  bool UseEQ;
  AXUM_EQ_BAND_PRESET_STRUCT EQBand[6];
  bool EQOnOff;

  bool UseDynamics;
  char AGCAmount;
  float AGCThreshold;
  bool DynamicsOnOff;
  float DownwardExpanderThreshold;

  bool UseModule;
  int Panorama;
  float FaderLevel;
  bool ModuleState;
} AXUM_PRESET_DATA_STRUCT;

typedef struct
{
  bool Use;
  float Level;
  bool On;
} AXUM_BUSS_PRESET_DATA_STRUCT;

typedef struct
{
  bool Use[24];
  bool On[24];
} AXUM_MONITOR_BUSS_PRESET_DATA_STRUCT;

typedef struct
{
  char Label[32];
  char Console[4];
  char ModulePreset;
  short int MixMonitorPreset;
} AXUM_CONSOLE_PRESET_DATA_STRUCT;

typedef struct
{
  char SourceName[32];
  AXUM_INPUT_DATA_STRUCT InputData[8];

  unsigned int DefaultProcessingPreset;

  bool Redlight[8];
  bool MonitorMute[16];
  char Active;

  char Start;
  bool Phantom;
  bool Pad;
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

  unsigned int Source;
  float Level;
  unsigned char Mute;
  unsigned char Dim;
  unsigned char Mono;
  unsigned char Phase;
  unsigned char Talkback[16];
  unsigned char Routing;

  unsigned int MixMinusSource;
  unsigned char MixMinusActive;

} AXUM_DESTINATION_DATA_STRUCT;

typedef struct
{
  float Range;
  float Level;
  unsigned int Frequency;
  float Bandwidth;
  float Slope;
  FilterType Type;
} AXUM_EQ_BAND_DATA_STRUCT;

typedef struct
{
  float Level;
  unsigned char On;
  unsigned char PreviousOn;
  int Balance;
  unsigned char PreModuleLevel;

  unsigned char Assigned;
} AXUM_BUSS_DATA_STRUCT;

typedef struct
{
  unsigned char Use;
  float Level;
  unsigned char On;
  int Balance;
  unsigned char PreModuleLevel;
} AXUM_ROUTING_PRESET_DATA_STRUCT;

typedef struct
{
  bool InsertUsePreset;
  unsigned int InsertSource;
  bool InsertOnOff;

  bool GainUsePreset;
  float Gain;

  bool PhaseUsePreset;
  unsigned char Phase;
  bool PhaseOnOff;

  bool MonoUsePreset;
  unsigned char Mono;
  bool MonoOnOff;

  bool FilterUsePreset;
  AXUM_EQ_BAND_DATA_STRUCT Filter;
  bool FilterOnOff;

  bool EQUsePreset;
  AXUM_EQ_BAND_DATA_STRUCT EQBand[6];
  bool EQOnOff;

  bool DynamicsUsePreset;
  char AGCAmount;
  float AGCThreshold;
  bool DynamicsOnOff;
  float DownwardExpanderThreshold;

  bool ModuleUsePreset;
  int Panorama;
  float FaderLevel;
  bool On;

  AXUM_ROUTING_PRESET_DATA_STRUCT Buss[16];

} AXUM_DEFAULT_MODULE_DATA_STRUCT;

typedef struct
{
  unsigned int Console;
  unsigned int TemporySourceLocal;
  unsigned int TemporySourceControlMode[4];
  unsigned int SelectedSource;
  unsigned int TemporyPresetLocal;
  unsigned int TemporyPresetControlMode[4];
  unsigned int SelectedPreset;
  unsigned int SourceA;
  unsigned int SourceB;
  unsigned int SourceC;
  unsigned int SourceD;
  unsigned int SourceE;
  unsigned int SourceF;
  unsigned int SourceG;
  unsigned int SourceH;
  unsigned int SourceAPreset;
  unsigned int SourceBPreset;
  unsigned int SourceCPreset;
  unsigned int SourceDPreset;
  unsigned int SourceEPreset;
  unsigned int SourceFPreset;
  unsigned int SourceGPreset;
  unsigned int SourceHPreset;
  bool OverruleActive;
  int WaitingSource;
  int WaitingProcessingPreset;
  int WaitingRoutingPreset;
  unsigned int InsertSource;
  bool InsertOnOff;
  float Gain;
  unsigned char Phase;
  bool PhaseOnOff;
  AXUM_EQ_BAND_DATA_STRUCT Filter;
  bool FilterOnOff;
  AXUM_EQ_BAND_DATA_STRUCT EQBand[6];
  bool EQOnOff;

  char AGCAmount;
  float AGCThreshold;
  bool DynamicsOnOff;
  float DownwardExpanderThreshold;

  int Panorama;
  unsigned char Mono;
  bool MonoOnOff;
  float FaderLevel;
  bool FaderTouch;
  bool On;
  bool Cough;

  bool Signal;
  bool Peak;
  bool TalkbackToMixMinus[16];

  AXUM_BUSS_DATA_STRUCT Buss[16];

  AXUM_ROUTING_PRESET_DATA_STRUCT RoutingPreset[8][16];

  AXUM_DEFAULT_MODULE_DATA_STRUCT Defaults;

} AXUM_MODULE_DATA_STRUCT;

typedef struct
{
  char Label[32];
  unsigned char Console;
  bool Interlock;
  char DefaultSelection;
  bool AutoSwitchingBuss[16];
  float SwitchingDimLevel;

  bool Buss[16];
  bool Ext[8];

  float PhonesLevel;
  float SpeakerLevel;

  bool Dim;
  bool Mute;
  bool Mono;
  bool Phase;
  bool Talkback[16];
} AXUM_MONITOR_OUTPUT_DATA_STRUCT;

typedef struct
{
  unsigned int Ext[8];
  unsigned int InterlockSafe[8];
} AXUM_EXTERN_SOURCE_DATA_STRUCT;

typedef struct
{
  float Level;
  char Label[32];
  unsigned char Console;
  bool On;

  bool PreModuleOn;
  bool PreModuleBalance;

  bool Mono;

  bool Interlock;
  bool Exclusive;
  bool GlobalBussReset;
} AXUM_BUSS_MASTER_DATA_STRUCT;

typedef struct
{
  unsigned int Source;
} AXUM_TALKBACK_STRUCT;

typedef struct
{
  unsigned int RackOrganization[42];
  AXUM_SOURCE_DATA_STRUCT SourceData[1280];
  AXUM_DESTINATION_DATA_STRUCT DestinationData[1280];
  AXUM_MODULE_DATA_STRUCT ModuleData[128];
  AXUM_BUSS_MASTER_DATA_STRUCT BussMasterData[16];
  AXUM_PRESET_DATA_STRUCT PresetData[1280];
  AXUM_BUSS_PRESET_DATA_STRUCT BussPresetData[1280][16];
  AXUM_MONITOR_BUSS_PRESET_DATA_STRUCT MonitorBussPresetData[1280][16];
  AXUM_CONSOLE_PRESET_DATA_STRUCT ConsolePresetData[32];

  int ControlMode[4];
  int MasterControlMode[4];

  bool Redlight[8];
  unsigned int Samplerate;
  bool ExternClock;
  float Headroom;
  float LevelReserve;
  bool AutoMomentary;
  bool StartupState;

  unsigned int SelectedConsolePreset[4];

  AXUM_MONITOR_OUTPUT_DATA_STRUCT Monitor[16];
  AXUM_EXTERN_SOURCE_DATA_STRUCT ExternSource[4];
  AXUM_TALKBACK_STRUCT Talkback[16];
} AXUM_DATA_STRUCT;

//**************************************************************/
// Struct to determine offset numbers of sources
//**************************************************************/
typedef struct
{
  struct {
    unsigned int buss;
    unsigned int insert_out;
    unsigned int monitor_buss;
    unsigned int mixminus;
    unsigned int source;
  } min, max;
} src_offset_struct;

#define MAX_POS_LIST_SIZE  1568 //16+128+16+128+1280 => buss+insert_out+monitor_buss+mixminus+source
enum src_type {none=0, buss=1, insert_out=2, monitor_buss=3, mixminus=4, source=5};

typedef struct
{
  short int src;
  unsigned char active;
  src_type type;
  unsigned char pool[8];
} src_list_struct;

typedef struct
{
  src_offset_struct src_offset;
  src_list_struct pos[MAX_POS_LIST_SIZE];
} matrix_sources_struct;

#define MAX_NR_OF_PRESETS 1280
typedef struct
{
  short int number;
  unsigned char filled;
} preset_list_struct;

typedef struct
{
  preset_list_struct pos[MAX_NR_OF_PRESETS];
} preset_pos_struct;

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
  float       CurrentActuatorDataDefault;
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

struct ONLINE_NODE_INFORMATION_STRUCT
{
  unsigned int MambaNetAddress;
  unsigned int ManufacturerID;
  unsigned int ProductID;
  unsigned int UniqueIDPerProduct;
  int FirmwareMajorRevision;

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

  SENSOR_RECEIVE_FUNCTION_STRUCT *SensorReceiveFunction;
  OBJECT_INFORMATION_STRUCT *ObjectInformation;

  ONLINE_NODE_INFORMATION_STRUCT *Next;
};

float CalculateEQ(float *Coefficients, float Gain, int Frequency, float Bandwidth, float Slope, FilterType Type);

//mbn-lib callbacks
void mAddressTableChange(struct mbn_handler *mbn, struct mbn_address_node *old_info, struct mbn_address_node *new_info);
int mSensorDataResponse(struct mbn_handler *mbn, struct mbn_message *message, short unsigned int object, unsigned char type, union mbn_data data);
int mSensorDataChanged(struct mbn_handler *mbn, struct mbn_message *message, short unsigned int object, unsigned char type, union mbn_data data);
void mError(struct mbn_handler *m, int code, char *str);
void mAcknowledgeTimeout(struct mbn_handler *m, struct mbn_message *msg);
void mAcknowledgeReply(struct mbn_handler *m, struct mbn_message *request, struct mbn_message *reply, int retries);

//thread function, used for timing related functionality
void Timer100HzDone(int Value);

//function to initialize AxumData struct
void initialize_axum_data_struct();

//misc utility functions
int delay_ms(double sleep_time);
int delay_us(double sleep_time);
void axum_get_mtrx_chs_from_src(unsigned int src, unsigned int *l_ch, unsigned int *r_ch);
void debug_mambanet_data(unsigned int object, unsigned char type, union mbn_data data);

//MambaNet object vs Engine function utilities
void CheckObjectsToSent(unsigned int SensorReceiveFunctionNumber, unsigned int MambaNetAddress=0x00000000);
void SentDataToObject(unsigned int SensorReceiveFunctionNumber, unsigned int MambaNetAddress, unsigned int ObjectNr, unsigned char DataType, unsigned char DataSize, float DataMinimal, float DataMaximal);
void InitalizeAllObjectListPerFunction();
void MakeObjectListPerFunction(unsigned int SensorReceiveFunctionNumber);
void DeleteAllObjectListPerFunction();
ONLINE_NODE_INFORMATION_STRUCT *GetOnlineNodeInformation(unsigned long int addr);
unsigned int NrOfObjectsAttachedToFunction(unsigned int FunctionNumberToCheck);

//Backplane functions, sending MambaNet to the backplane
void SetBackplaneRouting(unsigned int FormInputNr, unsigned int ChannelNr);
void SetBackplaneClock();

//Axum functions, only effictive internally (no MambaNet)
void SetAxum_EQ(unsigned char ModuleNr, unsigned char BandNr);
void SetAxum_ModuleProcessing(unsigned int ModuleNr);
void SetAxum_BussLevels(unsigned int ModuleNr);
void SetAxum_ModuleSource(unsigned int ModuleNr);
void SetAxum_ModuleMixMinus(unsigned int ModuleNr, unsigned int OldSource);
void SetAxum_ModuleInsertSource(unsigned int ModuleNr);
void SetAxum_RemoveOutputRouting(unsigned int OutputMambaNetAddress, unsigned char SubChannel);
void SetAxum_DestinationSource(unsigned int DestinationNr);
void SetAxum_ExternSources(unsigned int MonitorBussPerFourNr);
void SetAxum_TalkbackSource(unsigned int TalkbackNr);
void SetAxum_BussMasterLevels();
void SetAxum_MonitorBuss(unsigned int MonitorBussNr);
int MixMinusSourceUsed(unsigned int CurrentSource);
void GetSourceLabel(unsigned int SourceNr, char *TextString, int MaxLength);
#define AdjustDestinationSource AdjustModuleSource
unsigned int AdjustModuleSource(unsigned int CurrentSource, int Offset);
unsigned int AdjustModulePreset(unsigned int CurrentPreset, int Offset);
void GetPresetLabel(unsigned int PresetNr, char *TextString, int MaxLength);
void GetConsolePresetLabel(unsigned int ConsolePresetNr, char *TextString, int MaxLength);
unsigned int GetFunctionNrFromControlMode(int ControlNr);
int SourceActive(unsigned int InputSourceNr);
unsigned char ModulePresetActive(int ModuleNr, unsigned char PresetNr);
unsigned char GetPresetNrFromFunctionNr(unsigned int FunctionNr);
unsigned int GetModuleFunctionNrFromPresetNr(unsigned char PresetNr);

//Mode controller implementation functions
void ModeControllerSensorChange(unsigned int SensorReceiveFunctionNr, unsigned char type, mbn_data data, unsigned char DataType, unsigned char DataSize, float DataMinimal, float DataMaximal);
void ModeControllerResetSensorChange(unsigned int SensorReceiveFunctionNr, unsigned char type, mbn_data data, unsigned char DataType, unsigned char DataSize, float DataMinimal, float DataMaximal);
void ModeControllerSetData(unsigned int SensorReceiveFunctionNr, unsigned int MambaNetAddress, unsigned int ObjectNr, unsigned char DataType, unsigned char DataSize, float DataMinimal, float DataMaximal);
void ModeControllerSetLabel(unsigned int SensorReceiveFunctionNr, unsigned int MambaNetAddress, unsigned int ObjectNr, unsigned char DataType, unsigned char DataSize, float DataMinimal, float DataMaximal);
//Master mode controller implementtion functions
void MasterModeControllerSensorChange(unsigned int SensorReceiveFunctionNr, unsigned char type, mbn_data data, unsigned char DataType, unsigned char DataSize, float DataMinimal, float DataMaximal);
void MasterModeControllerResetSensorChange(unsigned int SensorReceiveFunctionNr, unsigned char type, mbn_data data, unsigned char DataType, unsigned char DataSize, float DataMinimal, float DataMaximal);
void MasterModeControllerSetData(unsigned int SensorReceiveFunctionNr, unsigned int MambaNetAddress, unsigned int ObjectNr, unsigned char DataType, unsigned char DataSize, float DataMinimal, float DataMaximal);

//The DoAxum functions also may sent MambaNet data
void DoAxum_BussReset(int BussNr);
void DoAxum_ModuleStatusChanged(int ModuleNr, int ByModule);
void DoAxum_ModulePreStatusChanged(int BussNr);
bool DoAxum_SetNewSource(int ModuleNr, unsigned int NewSource, int Forced);
void DoAxum_SetBussOnOff(int ModuleNr, int BussNr, unsigned char NewState, int UseInterlock);
void DoAxum_LoadProcessingPreset(unsigned char ModuleNr, unsigned int PresetNr, unsigned char UseModuleDefaults, unsigned char SetAllObjects);
void DoAxum_LoadRoutingPreset(unsigned char ModuleNr, unsigned char PresetNr, unsigned char UseModuleDefaults, unsigned char SetAllObjects);
void DoAxum_LoadBussMasterPreset(unsigned char PresetNr, char *Console, bool SetAllObjects);
void DoAxum_LoadMonitorBussPreset(unsigned char PresetNr, char *Console, bool SetAllObjects);
void DoAxum_LoadConsolePreset(unsigned char PresetNr, bool SetAllObjects, bool DisableActiveCheck);
void DoAxum_UpdateModuleControlModeLabel(unsigned char ModuleNr, int ControlMode);
void DoAxum_UpdateModuleControlMode(unsigned char ModuleNr, int ControlMode);
void DoAxum_UpdateMasterControlMode(int ControlMode);

#endif
