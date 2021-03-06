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
  float AGCRatio;
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
  unsigned int SafeRecallTime;
  unsigned int ForcedRecallTime;
} AXUM_CONSOLE_PRESET_DATA_STRUCT;

typedef struct
{
  char SourceName[32];
  AXUM_INPUT_DATA_STRUCT InputData[8];

  int DefaultProcessingPreset;

  unsigned char StartTrigger;
  unsigned char StopTrigger;

  int RelatedDest;

  bool Redlight[8];
  bool MonitorMute[16];
  char Active;

  char Start;
  bool Phantom;
  bool Pad;
  float Gain;
  float DefaultGain;
  char Alert;

  char CoughComm[2];

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
  unsigned char Routing;

  int MixMinusSource;
  unsigned char MixMinusActive;

  int CommBuss;
  unsigned char CommActive;

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
  unsigned char PreviousOn[16];
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
  float AGCRatio;
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
  int TemporySourceLocal;
  int TemporySourceControlMode[4];
  int SelectedSource;
  int TemporyPresetLocal;
  int TemporyPresetControlMode[4];
  int SelectedProcessingPreset;
  unsigned int ModulePreset;
  int Source1A;
  int Source1B;
  int Source2A;
  int Source2B;
  int Source3A;
  int Source3B;
  int Source4A;
  int Source4B;
  int ProcessingPreset1A;
  int ProcessingPreset1B;
  int ProcessingPreset2A;
  int ProcessingPreset2B;
  int ProcessingPreset3A;
  int ProcessingPreset3B;
  int ProcessingPreset4A;
  int ProcessingPreset4B;
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

  float AGCRatio;
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
  bool TalkbackToRelatedDestination[16];

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
  int Ext[8];
  unsigned char InterlockSafe[8];
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
  unsigned char Exclusive;
  bool GlobalBussReset;

  unsigned char Talkback[16];
  unsigned char Dim;
} AXUM_BUSS_MASTER_DATA_STRUCT;

typedef struct
{
  int Source;
} AXUM_TALKBACK_STRUCT;

typedef struct
{
  int ControlMode;
  unsigned int ControlModeTimerValue;
  int MasterControlMode;
  char Username[33];
  char Password[17];
  char ActiveUsername[33];
  char ActivePassword[17];
  char UsernameToWrite[33];
  char PasswordToWrite[17];
  unsigned char UserLevel;
  unsigned char SourcePool;
  unsigned char PresetPool;
  unsigned char LogoutToIdle;
  unsigned char ConsolePreset;

  unsigned char DotCountUpDown;
  unsigned char ProgramEndTimeEnable;
  unsigned char ProgramEndTimeHours;
  unsigned char ProgramEndTimeMinutes;
  unsigned char ProgramEndTimeSeconds;
  float CountDownTimer;

  unsigned int SelectedConsolePreset;

  unsigned int SelectedModule;
  int SelectedModuleTimeout;

  unsigned int SelectedBuss;
  int SelectedBussTimeout;

  unsigned int SelectedMonitorBuss;
  int SelectedMonitorBussTimeout;

  unsigned int SelectedSource;
  int SelectedSourceTimeout;

  unsigned int SelectedDestination;
  int SelectedDestinationTimeout;
} AXUM_CONSOLE_DATA;

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
  AXUM_MONITOR_OUTPUT_DATA_STRUCT Monitor[16];
  AXUM_EXTERN_SOURCE_DATA_STRUCT ExternSource[4];
  AXUM_TALKBACK_STRUCT Talkback[16];

  AXUM_CONSOLE_DATA ConsoleData[NUMBER_OF_CONSOLES];

  bool Redlight[8];
  unsigned int Samplerate;
  unsigned int ExternClock;
  float Headroom;
  float LevelReserve;
  bool AutoMomentary;
  bool StartupState;

  unsigned char PercentInitialized;
} AXUM_DATA_STRUCT;

//**************************************************************/
// Struct to determine offset numbers of sources
//**************************************************************/
typedef struct
{
  struct {
    int buss;
    int insert_out;
    int monitor_buss;
    int mixminus;
    int source;
  } min, max;
} src_offset_struct;

#define MAX_POS_LIST_SIZE  1568 //16+128+16+128+1280 => buss+insert_out+monitor_buss+mixminus+source
enum src_type {none=0, buss=1, insert_out=2, monitor_buss=3, mixminus=4, source=5};

typedef struct
{
  int src;
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
  unsigned char pool[8];
} preset_list_struct;

typedef struct
{
  preset_list_struct pos[2+MAX_NR_OF_PRESETS];
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
  float ActuatorDataDefault;
  float ActuatorData;
  void *Next;
} AXUM_FUNCTION_INFORMATION_STRUCT;

typedef struct
{
  unsigned int FunctionNr;
  unsigned long LastChangedTime;
  unsigned long PreviousLastChangedTime;
  int TimeBeforeMomentary;
  unsigned char ActiveInUserLevel[6];
  unsigned char ChangedWhileSensorNotAllowed;
} SENSOR_RECEIVE_FUNCTION_STRUCT;

struct ONLINE_NODE_INFORMATION_STRUCT
{
  unsigned int MambaNetAddress;
  unsigned int ManufacturerID;
  unsigned int ProductID;
  unsigned int UniqueIDPerProduct;
  int FirmwareMajorRevision;
  unsigned char UserLevelFromConsole;
  unsigned char TimerRequestDone;
  unsigned char InitializationFinished;

//Not sure if should be stored here...
  int SlotNumberObjectNr;
  int InputChannelCountObjectNr;
  int OutputChannelCountObjectNr;
  int EnableWCObjectNr;

  struct
  {
    unsigned int ManufacturerID;
    unsigned int ProductID;
    unsigned int UniqueIDPerProduct;
  } Parent;

  int UsedNumberOfCustomObjects;
  int OnlineNumberOfCustomObjects;
  int TemplateNumberOfCustomObjects;

  SENSOR_RECEIVE_FUNCTION_STRUCT *SensorReceiveFunction;
  OBJECT_INFORMATION_STRUCT *ObjectInformation;

  struct
  {
    unsigned char UsernameReceived;
    char Username[33];
    unsigned char PasswordReceived;
    char Password[17];
  } Account;

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
void axum_get_mtrx_chs_from_src(int src, unsigned int *l_ch, unsigned int *r_ch);
void debug_mambanet_data(unsigned int addr, unsigned int object, unsigned char type, union mbn_data data);

//MambaNet object vs Engine function utilities
void CheckObjectsToSent(unsigned int SensorReceiveFunctionNumber, unsigned int MambaNetAddress=0x00000000);
void CheckObjectRange(unsigned int SensorReceiveFunctionNumber, float *min, float *max, float *def, unsigned int MambaNetAddress=0x00000000);
void SentDataToObject(unsigned int SensorReceiveFunctionNumber, AXUM_FUNCTION_INFORMATION_STRUCT *InfoObjectToSend);
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
void SetAxum_ModuleMixMinus(unsigned int ModuleNr, int OldSource);
void SetAxum_ModuleInsertSource(unsigned int ModuleNr);
void SetAxum_RemoveOutputRouting(unsigned int OutputMambaNetAddress, unsigned char SubChannel);
void SetAxum_DestinationSource(unsigned int DestinationNr);
void SetAxum_ExternSources(unsigned int MonitorBussPerFourNr);
void SetAxum_TalkbackSource(unsigned int TalkbackNr);
void SetAxum_BussMasterLevels();
void SetAxum_MonitorBuss(unsigned int MonitorBussNr);
int MixMinusSourceUsed(int CurrentSource);
void GetSourceLabel(int SourceNr, char *TextString, int MaxLength);
#define AdjustDestinationSource AdjustModuleSource
int AdjustModuleSource(int CurrentSource, int Offset, unsigned char Pool);
int AdjustModulePreset(int CurrentPreset, int Offset, unsigned char Pool);
void GetPresetLabel(int PresetNr, char *TextString, int MaxLength);
void GetConsolePresetLabel(unsigned int ConsolePresetNr, char *TextString, int MaxLength);
int GetControlModeFromConsoleFunctionNr(unsigned int CheckFunctionNr);
unsigned int GetConsoleFunctionNrFromControlMode(unsigned int ConsoleNr);
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
void MasterModeControllerSetData(unsigned int ConsoleNr, unsigned int MambaNetAddress, unsigned int ObjectNr, unsigned char DataType, unsigned char DataSize, float DataMinimal, float DataMaximal);

//The DoAxum functions also may sent MambaNet data
void DoAxum_BussReset(int BussNr);
void DoAxum_ModuleStatusChanged(int ModuleNr, int ByModule);
bool DoAxum_SetNewSource(int ModuleNr, int NewSource, int Forced);
void DoAxum_SetBussOnOff(int ModuleNr, int BussNr, unsigned char NewState, int LoadPreset);
void DoAxum_SetCRMBussOnOff(int MonitorBussNr, int BussNr, unsigned char NewState, int PreventDoingInterlock);
void DoAxum_LoadProcessingPreset(unsigned char ModuleNr, int ProcessingPresetNr, unsigned char OverrideAtSourceSelect, unsigned char UseModuleDefaults, unsigned char SetAllObjects);
void DoAxum_LoadRoutingPreset(unsigned char ModuleNr, int PresetNr, unsigned char OverrideAtSourceSelect, unsigned char UseModuleDefaults, unsigned char SetAllObjects);
void DoAxum_LoadBussMasterPreset(unsigned char PresetNr, char *Console, bool SetAllObjects);
void DoAxum_LoadMonitorBussPreset(unsigned char PresetNr, char *Console, bool SetAllObjects);
void DoAxum_LoadConsolePreset(unsigned char PresetNr, bool SetAllObjects, bool DisableActiveCheck);
void DoAxum_UpdateModuleControlModeLabel(unsigned char ModuleNr, int ControlMode);
void DoAxum_UpdateModuleControlMode(unsigned char ModuleNr, int ControlMode);
void DoAxum_UpdateMasterControlMode(int ControlMode);
void DoAxum_StartStopTrigger(unsigned int ModuleNr, float CurrentLevel, float NewLevel, unsigned char CurrentOn, unsigned char NewOn);
void DoAxum_TalkbackToRelatedDestination(unsigned char ModuleNr, unsigned char TalkbackNr, unsigned char NewState, unsigned char Dimming);
void DoAxum_SetCough(int SourceNr, unsigned char NewState);
void DoAxum_SetComm(int SourceNr, unsigned char CommNr, unsigned char NewState);

//select functions
void SetSelectedModule(unsigned char SelectNr, unsigned int NewModuleNr);
void SetSelectedBuss(unsigned char SelectNr, unsigned int NewBussNr);
void SetSelectedMonitorBuss(unsigned char SelectNr, unsigned int NewMonitorBussNr);
void SetSelectedSource(unsigned char SelectNr, unsigned int NewSourceNr);
void SetSelectedDestination(unsigned char SelectNr, unsigned int NewDestinationNr);

#endif
