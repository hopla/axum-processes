#include "common.h"
#include "engine.h"
#include "db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern AXUM_DATA_STRUCT AxumData;
extern unsigned short int dB2Position[1500];
extern float Position2dB[1024];
extern DEFAULT_NODE_OBJECTS_STRUCT AxumEngineDefaultObjects;
extern int AxumApplicationAndDSPInitialized;

struct sql_notify notifies[] = {
  { (char *)"templates_changed",            db_event_templates_changed},
  { (char *)"addresses_changed",            db_event_addresses_changed},
  { (char *)"slot_config_changed",          db_event_slot_config_changed},
  { (char *)"src_config_changed",           db_event_src_config_changed},
  { (char *)"module_config_changed",        db_event_module_config_changed},
  { (char *)"buss_config_changed",          db_event_buss_config_changed},
  { (char *)"monitor_buss_config_changed",  db_event_monitor_buss_config_changed},
  { (char *)"extern_src_config_changed",    db_event_extern_src_config_changed},
  { (char *)"talkback_config_changed",      db_event_talkback_config_changed},
  { (char *)"global_config_changed",        db_event_global_config_changed},
  { (char *)"dest_config_changed",          db_event_dest_config_changed}
};

void db_open(char *dbstr)
{
  sql_open(dbstr, 11, notifies);
}

int db_get_fd()
{
  int fd = PQsocket(sql_conn);
  if (fd < 0)
  {
    log_write("db_get_fd/PQsocket error, no server connection is currently open");  
  }
  return fd;
}

int db_read_slot_config()
{
  int cntRow;

  PGresult *qres = sql_exec("SELECT slot_nr, addr FROM slot_config", 1, 0, NULL);
  if (qres == NULL)
  {
    return 0;
  }
  for (cntRow=0; cntRow<PQntuples(qres); cntRow++)
  {
    short int slot_nr;
    unsigned long int addr;
    int cntField;
    //short int input_ch_count, output_ch_count;

    cntField=0;
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%hd", &slot_nr);
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%ld", &addr);
    //sscanf(PQgetvalue(qres, cntRow, cntField++), "%hd", &input_ch_count);
    //sscanf(PQgetvalue(qres, cntRow, cntField++), "%hd", &output_ch_count);

    AxumData.RackOrganization[slot_nr-1] = addr;
  }
  PQclear(qres);
  return 1;
}

int db_read_src_config(unsigned short int first_src, unsigned short int last_src)
{
  char str[2][32];
  const char *params[2];
  int cntParams;
  int cntRow;

  for (cntParams=0; cntParams<2; cntParams++)
  {
    params[cntParams] = (const char *)str[cntParams];
  }

  sprintf(str[0], "%hd", first_src);
  sprintf(str[1], "%hd", last_src);

  PGresult *qres = sql_exec("SELECT number, label, input1_addr, input1_sub_ch, input2_addr, input2_sub_ch, phantom, pad, gain, redlight1, redlight2, redlight3, redlight4, redlight5, redlight6, redlight7, redlight8, monitormute1, monitormute2, monitormute3, monitormute4, monitormute5, monitormute6, monitormute7, monitormute8, monitormute9, monitormute10, monitormute11, monitormute12, monitormute13, monitormute14, monitormute15, monitormute16 FROM src_config WHERE number>=$1 AND number<=$2", 1, 2, params);
  if (qres == NULL)
  {
    return 0;
  }
  for (cntRow=0; cntRow<PQntuples(qres); cntRow++)
  {
    short int number;
    unsigned char cntModule;
    int cntField;

    cntField = 0;
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%hd", &number);

    AXUM_SOURCE_DATA_STRUCT *SourceData = &AxumData.SourceData[number-1];
    
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%s", SourceData->SourceName);
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &SourceData->InputData[0].MambaNetAddress); 
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%c", &SourceData->InputData[0].SubChannel); 
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &SourceData->InputData[1].MambaNetAddress); 
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%c", &SourceData->InputData[1].SubChannel); 
    SourceData->Phantom = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    SourceData->Pad = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    SourceData->Gain = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    SourceData->Redlight[0] = strcmp(PQgetvalue(qres, cntRow, cntField++), "f"); 
    SourceData->Redlight[1] = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    SourceData->Redlight[2] = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    SourceData->Redlight[3] = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    SourceData->Redlight[4] = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    SourceData->Redlight[5] = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    SourceData->Redlight[6] = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    SourceData->Redlight[7] = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    SourceData->MonitorMute[0] = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    SourceData->MonitorMute[1] = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    SourceData->MonitorMute[2] = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    SourceData->MonitorMute[3] = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    SourceData->MonitorMute[4] = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    SourceData->MonitorMute[5] = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    SourceData->MonitorMute[6] = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    SourceData->MonitorMute[7] = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    SourceData->MonitorMute[8] = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    SourceData->MonitorMute[9] = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    SourceData->MonitorMute[10] = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    SourceData->MonitorMute[11] = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    SourceData->MonitorMute[12] = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    SourceData->MonitorMute[13] = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    SourceData->MonitorMute[14] = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    SourceData->MonitorMute[15] = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");

    for (cntModule=0; cntModule<128; cntModule++)
    {
      if (AxumData.ModuleData[cntModule].Source == number)
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
  PQclear(qres);
  return 1;
}

int db_read_module_config(unsigned char first_mod, unsigned char last_mod)
{
  char str[2][32];
  const char *params[2];
  int cntParams;
  int cntRow;

  for (cntParams=0; cntParams<2; cntParams++)
  {
    params[cntParams] = (const char *)str[cntParams];
  }

  sprintf(str[0], "%hd", first_mod);
  sprintf(str[1], "%hd", last_mod);

  PGresult *qres = sql_exec("SELECT number,               \
                                    source_a,             \
                                    source_b,             \
                                    insert_source,        \
                                    insert_on_off_a,      \
                                    insert_on_off_b,      \
                                    gain,                 \
                                    lc_frequency,         \
                                    lc_on_off_a,          \
                                    lc_on_off_b,          \
                                    eq_band_1_range,      \
                                    eq_band_1_freq,       \
                                    eq_band_1_bw,         \
                                    eq_band_1_slope,      \
                                    eq_band_1_type,       \
                                    eq_band_2_range,      \
                                    eq_band_2_freq,       \
                                    eq_band_2_bw,         \
                                    eq_band_2_slope,      \
                                    eq_band_2_type,       \
                                    eq_band_3_range,      \
                                    eq_band_3_freq,       \
                                    eq_band_3_bw,         \
                                    eq_band_3_slope,      \
                                    eq_band_3_type,       \
                                    eq_band_4_range,      \
                                    eq_band_4_freq,       \
                                    eq_band_4_bw,         \
                                    eq_band_4_slope,      \
                                    eq_band_4_type,       \
                                    eq_band_5_range,      \
                                    eq_band_5_freq,       \
                                    eq_band_5_bw,         \
                                    eq_band_5_slope,      \
                                    eq_band_5_type,       \
                                    eq_band_6_range,      \
                                    eq_band_6_freq,       \
                                    eq_band_6_bw,         \
                                    eq_band_6_slope,      \
                                    eq_band_6_type,       \
                                    eq_on_off_a,          \
                                    eq_on_off_b,          \
                                    dyn_amount,           \
                                    dyn_on_off_a,         \
                                    dyn_on_off_b,         \
                                    mod_level,            \
                                    mod_on_off,           \
                                    buss_1_2_level,       \
                                    buss_1_2_on_off,      \
                                    buss_1_2_pre_post,    \
                                    buss_1_2_balance,     \
                                    buss_1_2_assignment,  \
                                    buss_3_4_level,       \
                                    buss_3_4_on_off,      \
                                    buss_3_4_pre_post,    \
                                    buss_3_4_balance,     \
                                    buss_3_4_assignment,  \
                                    buss_5_6_level,       \
                                    buss_5_6_on_off,      \
                                    buss_5_6_pre_post,    \
                                    buss_5_6_balance,     \
                                    buss_5_6_assignment,  \
                                    buss_7_8_level,       \
                                    buss_7_8_on_off,      \
                                    buss_7_8_pre_post,    \
                                    buss_7_8_balance,     \
                                    buss_7_8_assignment,  \
                                    buss_9_10_level,       \
                                    buss_9_10_on_off,      \
                                    buss_9_10_pre_post,    \
                                    buss_9_10_balance,     \
                                    buss_9_10_assignment,  \
                                    buss_11_12_level,       \
                                    buss_11_12_on_off,      \
                                    buss_11_12_pre_post,    \
                                    buss_11_12_balance,     \
                                    buss_11_12_assignment,  \
                                    buss_13_14_level,       \
                                    buss_13_14_on_off,      \
                                    buss_13_14_pre_post,    \
                                    buss_13_14_balance,     \
                                    buss_13_14_assignment,  \
                                    buss_15_16_level,       \
                                    buss_15_16_on_off,      \
                                    buss_15_16_pre_post,    \
                                    buss_15_16_balance,     \
                                    buss_15_16_assignment,  \
                                    buss_17_18_level,       \
                                    buss_17_18_on_off,      \
                                    buss_17_18_pre_post,    \
                                    buss_17_18_balance,     \
                                    buss_17_18_assignment,  \
                                    buss_19_20_level,       \
                                    buss_19_20_on_off,      \
                                    buss_19_20_pre_post,    \
                                    buss_19_20_balance,     \
                                    buss_19_20_assignment,  \
                                    buss_21_22_level,       \
                                    buss_21_22_on_off,      \
                                    buss_21_22_pre_post,    \
                                    buss_21_22_balance,     \
                                    buss_21_22_assignment,  \
                                    buss_23_24_level,       \
                                    buss_23_24_on_off,      \
                                    buss_23_24_pre_post,    \
                                    buss_23_24_balance,     \
                                    buss_23_24_assignment,  \
                                    buss_25_26_level,       \
                                    buss_25_26_on_off,      \
                                    buss_25_26_pre_post,    \
                                    buss_25_26_balance,     \
                                    buss_25_26_assignment,  \
                                    buss_27_28_level,       \
                                    buss_27_28_on_off,      \
                                    buss_27_28_pre_post,    \
                                    buss_27_28_balance,     \
                                    buss_27_28_assignment,  \
                                    buss_29_30_level,       \
                                    buss_29_30_on_off,      \
                                    buss_29_30_pre_post,    \
                                    buss_29_30_balance,     \
                                    buss_29_30_assignment,  \
                                    buss_31_32_level,       \
                                    buss_31_32_on_off,      \
                                    buss_31_32_pre_post,    \
                                    buss_31_32_balance,     \
                                    buss_31_32_assignment   \
                                    FROM module_config      \
                                    WHERE number>=$1 AND number<=$2", 1, 2, params);
  if (qres == NULL)
  {
    return 0;
  }
  for (cntRow=0; cntRow<PQntuples(qres); cntRow++)
  {
    short int number;
    unsigned int cntField;
    unsigned char cntEQ;
    unsigned char cntBuss;
    float OldLevel;
    unsigned char OldOn;

    cntField = 0;
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%hd", &number);

    AXUM_MODULE_DATA_STRUCT *ModuleData = &AxumData.ModuleData[number-1];
   
    OldLevel = ModuleData->FaderLevel;
    OldOn = ModuleData->On;

    sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &ModuleData->SourceA);
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &ModuleData->SourceB);
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &ModuleData->InsertSource);
    ModuleData->InsertOnOffA = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    ModuleData->InsertOnOffA = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    ModuleData->InsertOnOffB = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &ModuleData->Gain);
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &ModuleData->Filter.Frequency);
    ModuleData->FilterOnOffA = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    ModuleData->FilterOnOffB = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    for (cntEQ=0; cntEQ<6; cntEQ++)
    {
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &ModuleData->EQBand[cntEQ].Range);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &ModuleData->EQBand[cntEQ].Frequency);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &ModuleData->EQBand[cntEQ].Bandwidth);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &ModuleData->EQBand[cntEQ].Slope);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%c", (char *)&ModuleData->EQBand[cntEQ].Type);
    }
    ModuleData->EQOnOffA = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    ModuleData->EQOnOffB = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%c", &ModuleData->Dynamics);
    ModuleData->DynamicsOnOffA = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    ModuleData->DynamicsOnOffB = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &ModuleData->FaderLevel);
    ModuleData->On = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    for (cntBuss=0; cntBuss<16; cntBuss++)
    {
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &ModuleData->Buss[cntBuss].Level);
      ModuleData->Buss[cntBuss].On = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
      ModuleData->Buss[cntBuss].PreModuleLevel = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &ModuleData->Buss[cntBuss].Balance);
      ModuleData->Buss[cntBuss].Active = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    }

    if (number>0)
    {
      int ModuleNr = number-1;

      //if ((*((int *)UpdateType) == 0) || (*((int *)UpdateType) == 1))
      { //All or input
        if (AxumApplicationAndDSPInitialized)
        {
          SetNewSource(ModuleNr, ModuleData->SourceA, 1, 1);
  
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
      }

      //if ((*((int *)UpdateType) == 0) || (*((int *)UpdateType) == 2))
      { //All or eq
        for (int cntBand=0; cntBand<6; cntBand++)
        {
          if (AxumData.ModuleData[ModuleNr].EQBand[cntBand].Level>AxumData.ModuleData[ModuleNr].EQBand[cntBand].Range)
          {
            AxumData.ModuleData[ModuleNr].EQBand[cntBand].Level = AxumData.ModuleData[ModuleNr].EQBand[cntBand].Range;
          }
          else if (AxumData.ModuleData[ModuleNr].EQBand[cntBand].Level < -AxumData.ModuleData[ModuleNr].EQBand[cntBand].Range)
          {
            AxumData.ModuleData[ModuleNr].EQBand[cntBand].Level = AxumData.ModuleData[ModuleNr].EQBand[cntBand].Range;
          }
  
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

      //if ((*((int *)UpdateType) == 0) || (*((int *)UpdateType) == 3))
      { //All or busses
        for (int cntBuss=0; cntBuss<16; cntBuss++)
        {
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
  }
  PQclear(qres);
  return 1;
}

int db_read_buss_config(unsigned char first_buss, unsigned char last_buss)
{
  char str[2][32];
  const char *params[2];
  int cntParams;
  int cntRow;

  for (cntParams=0; cntParams<2; cntParams++)
  {
    params[cntParams] = (const char *)str[cntParams];
  }

  sprintf(str[0], "%hd", first_buss);
  sprintf(str[1], "%hd", last_buss);

  PGresult *qres = sql_exec("SELECT number, label, pre_on, pre_level, pre_balance, level, on_off, interlock, global_reset FROM buss_config WHERE number>=$1 AND number<=$2", 1, 2, params);
  if (qres == NULL)
  {
    return 0;
  }
  for (cntRow=0; cntRow<PQntuples(qres); cntRow++)
  {
    short int number;
    int cntField;

    cntField=0;
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%hd", &number);

    AXUM_BUSS_MASTER_DATA_STRUCT *BussMasterData = &AxumData.BussMasterData[number-1];
    
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%s", BussMasterData->Label);
    BussMasterData->PreModuleOn = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    BussMasterData->PreModuleLevel = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    BussMasterData->PreModuleBalance = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &BussMasterData->Level); 
    BussMasterData->On = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    BussMasterData->Interlock = strcmp(PQgetvalue(qres, cntRow, cntField++), "f"); 
    BussMasterData->GlobalBussReset = strcmp(PQgetvalue(qres, cntRow, cntField++), "f"); 

    if (AxumApplicationAndDSPInitialized)
    {
      unsigned int FunctionNrToSent = 0x01000000 | (((number-1)<<12)&0xFFF000);
      CheckObjectsToSent(FunctionNrToSent | BUSS_FUNCTION_MASTER_PRE);
      CheckObjectsToSent(FunctionNrToSent | BUSS_FUNCTION_MASTER_LEVEL);
      CheckObjectsToSent(FunctionNrToSent | BUSS_FUNCTION_MASTER_ON_OFF);
      CheckObjectsToSent(FunctionNrToSent | BUSS_FUNCTION_LABEL);

      for (int cntModule=0; cntModule<128; cntModule++)
      {
        SetAxum_BussLevels(cntModule);
      }

      for (int cntDestination=0; cntDestination<=1280; cntDestination++)
      {
        if (AxumData.DestinationData[cntDestination].Source == number)
        {
          FunctionNrToSent = 0x06000000 | (cntDestination<<12);
          CheckObjectsToSent(FunctionNrToSent | DESTINATION_FUNCTION_SOURCE);
        }
      }
    }
  }
  PQclear(qres);
  return 1;
}

int db_read_monitor_buss_config(unsigned char first_mon_buss, unsigned char last_mon_buss)
{
  char str[2][32];
  const char *params[2];
  int cntParams;
  int cntRow;

  for (cntParams=0; cntParams<2; cntParams++)
  {
    params[cntParams] = (const char *)str[cntParams];
  }

  sprintf(str[0], "%hd", first_mon_buss);
  sprintf(str[1], "%hd", last_mon_buss);

  PGresult *qres = sql_exec("SELECT number, label, interlock, default_selection, buss_1_2, buss_3_4, buss_5_6, buss_7_8, buss_9_10, buss_11_12, buss_13_14, buss_15_16, buss_17_18, buss_19_20, buss_21_22, buss_23_24, buss_25_26, buss_27_28, buss_29_30, buss_31_32, dim_level FROM monitor_buss_config WHERE number>=$1 AND number<=$2", 1, 2, params);
  if (qres == NULL)
  {
    return 0;
  }
  for (cntRow=0; cntRow<PQntuples(qres); cntRow++)
  {
    short int number;
    unsigned int cntField;
    unsigned int cntMonitorBuss;

    cntField = 0;
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%hd", &number);

    AXUM_MONITOR_OUTPUT_DATA_STRUCT *MonitorData = &AxumData.Monitor[number-1];
    
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%s", MonitorData->Label);
    MonitorData->Interlock = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%c", &MonitorData->DefaultSelection); 
    for (cntMonitorBuss=0; cntMonitorBuss<16; cntMonitorBuss++)
    { 
      MonitorData->AutoSwitchingBuss[cntMonitorBuss] = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    }
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &MonitorData->SwitchingDimLevel); 

    if (AxumApplicationAndDSPInitialized)
    {
      int MonitorBussNr = number-1;
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
  PQclear(qres);
  return 1;
}

int db_read_extern_src_config(unsigned char first_dsp_card, unsigned char last_dsp_card)
{
  char str[2][32];
  const char *params[2];
  int cntParams;
  int cntRow;

  for (cntParams=0; cntParams<2; cntParams++)
  {
    params[cntParams] = (const char *)str[cntParams];
  }

  sprintf(str[0], "%hd", first_dsp_card);
  sprintf(str[1], "%hd", last_dsp_card);
  
  PGresult *qres = sql_exec("SELECT number, ext1, ext2, ext3, ext4, ext5, ext6, ext7, ext8 FROM extern_src_config WHERE number>=$1 AND number<=$2", 1, 2, params);
  if (qres == NULL)
  {
    return 0;
  }
  for (cntRow=0; cntRow<PQntuples(qres); cntRow++)
  {
    short int number;
    unsigned int cntField;
    unsigned int cntExternSource;

    cntField = 0;
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%hd", &number);

    AXUM_EXTERN_SOURCE_DATA_STRUCT *ExternSource = &AxumData.ExternSource[number-1];
    
    for (cntExternSource=0; cntExternSource<8; cntExternSource++)
    { 
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &ExternSource->Ext[cntExternSource]); 
    }
    
    if (AxumApplicationAndDSPInitialized)
    {
      SetAxum_ExternSources(number-1);
    }
  }
  PQclear(qres);
  return 1;
}

int db_read_talkback_config(unsigned char first_tb, unsigned char last_tb)
{
  char str[2][32];
  const char *params[2];
  int cntParams;
  int cntRow;

  for (cntParams=0; cntParams<2; cntParams++)
  {
    params[cntParams] = (const char *)str[cntParams];
  }

  sprintf(str[0], "%hd", first_tb);
  sprintf(str[1], "%hd", last_tb);

  PGresult *qres = sql_exec("SELECT number, source FROM talkback_config WHERE number>=$1 AND number<=$2", 1, 2, params);
  if (qres == NULL)
  {
    return 0;
  }
  for (cntRow=0; cntRow<PQntuples(qres); cntRow++)
  {
    short int number;
    unsigned int cntField;

    cntField = 0;
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%hd", &number);
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &AxumData.Talkback[number-1].Source);
    SetAxum_TalkbackSource(number-1);
  }
  PQclear(qres);
  return 1;
}

int db_read_global_config()
{
  int cntRow;

  PGresult *qres = sql_exec("SELECT samplerate, ext_clock, headroom, level_reserve FROM global_config", 1, 0, NULL);

  if (qres == NULL)
  {
    return 0;
  }
  for (cntRow=0; cntRow<PQntuples(qres); cntRow++)
  {
    unsigned int cntField;

    cntField = 0;
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &AxumData.Samplerate);
    AxumData.ExternClock = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &AxumData.Headroom);
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &AxumData.LevelReserve);

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

  }
  return 1;
}

int db_read_dest_config(unsigned short int first_dest, unsigned short int last_dest)
{
  char str[2][32];
  const char *params[2];
  int cntParams;
  int cntRow;

  for (cntParams=0; cntParams<2; cntParams++)
  {
    params[cntParams] = (const char *)str[cntParams];
  }

  sprintf(str[0], "%hd", first_dest);
  sprintf(str[1], "%hd", last_dest);

  PGresult *qres = sql_exec("SELECT number, label, output1_addr, output1_sub_ch, output2_addr, output2_sub_ch, level, source, mix_minus_source FROM dest_config WHERE number>=$1 AND number<=$2", 1, 2, params);
  if (qres == NULL)
  {
    return 0;
  }
  for (cntRow=0; cntRow<PQntuples(qres); cntRow++)
  {
    short int number;
    int cntField;

    cntField = 0;
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%hd", &number);

    AXUM_DESTINATION_DATA_STRUCT *DestinationData = &AxumData.DestinationData[number-1];
    
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%s", DestinationData->DestinationName);
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &DestinationData->OutputData[0].MambaNetAddress); 
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%c", &DestinationData->OutputData[0].SubChannel); 
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &DestinationData->OutputData[1].MambaNetAddress); 
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%c", &DestinationData->OutputData[1].SubChannel); 
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &DestinationData->Level); 
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &DestinationData->Source); 
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &DestinationData->MixMinusSource); 

    if (AxumApplicationAndDSPInitialized)
    {
      SetAxum_DestinationSource(number-1);
    }

    //Check destinations
    unsigned int DisplayFunctionNr = 0x06000000 | ((number-1)<<12);
    CheckObjectsToSent(DisplayFunctionNr | DESTINATION_FUNCTION_LABEL);
    CheckObjectsToSent(DisplayFunctionNr | DESTINATION_FUNCTION_LEVEL);
    CheckObjectsToSent(DisplayFunctionNr | DESTINATION_FUNCTION_SOURCE);
  }
  return 1;
}

int db_read_db_to_position()
{
  int cntRow;
  unsigned short int cntPosition;
  unsigned int db_array_pointer;
  float dB;

  PGresult *qres = sql_exec("SELECT db, position FROM db_to_position", 1, 0, NULL);
  if (qres == NULL)
  {
    return 0;
  }
  for (cntRow=0; cntRow<PQntuples(qres); cntRow++)
  {
    sscanf(PQgetvalue(qres, cntRow, 0), "%f", &dB);
    db_array_pointer = (dB*10)+1400;

    sscanf(PQgetvalue(qres, cntRow, 1), "%hd", &dB2Position[db_array_pointer]);
  }

  dB = -140;
  for (cntPosition=0; cntPosition<1024; cntPosition++)
  {
    for (db_array_pointer=0; db_array_pointer<1500; db_array_pointer++)
    {
      if (dB2Position[db_array_pointer] == cntPosition)
      {
        dB = ((float)(db_array_pointer-1400))/10;
      }
      Position2dB[cntPosition] = dB;
    }
  }
  return 1;
}

int db_read_template_info(ONLINE_NODE_INFORMATION_STRUCT *node_info)
{
  char str[3][32];
  const char *params[3];
  int cntParams;
  int cntRow;

  for (cntParams=0; cntParams<3; cntParams++)
  {
    params[cntParams] = (const char *)str[cntParams];
  }

  //Determine number of objects for memory reservation
  sprintf(str[0], "%hd", node_info->ManufacturerID);
  sprintf(str[1], "%hd", node_info->ProductID);
  sprintf(str[2], "%hd", node_info->FirmwareMajorRevision);
  log_write("man_id:%s, prod_id:%s, firm_major:%s", str[0], str[1], str[2]); 
  PGresult *qres = sql_exec("SELECT COUNT(*) FROM templates WHERE man_id=$1 AND prod_id=$2 AND firm_major=$3", 1, 3, params);
  if (qres == NULL)
  {
    return 0;
  }
  sscanf(PQgetvalue(qres, 0, 0), "%d", &node_info->NumberOfCustomObjects);
  if (node_info->NumberOfCustomObjects>0)
  {
    node_info->SensorReceiveFunction = new SENSOR_RECEIVE_FUNCTION_STRUCT[node_info->NumberOfCustomObjects];
    node_info->ObjectInformation = new OBJECT_INFORMATION_STRUCT[node_info->NumberOfCustomObjects];
    for (int cntObject=0; cntObject<node_info->NumberOfCustomObjects; cntObject++)
    {
      node_info->SensorReceiveFunction[cntObject].FunctionNr = -1;
      node_info->SensorReceiveFunction[cntObject].LastChangedTime = 0;
      node_info->SensorReceiveFunction[cntObject].PreviousLastChangedTime = 0;
      node_info->SensorReceiveFunction[cntObject].TimeBeforeMomentary = DEFAULT_TIME_BEFORE_MOMENTARY;
      node_info->ObjectInformation[cntObject].AxumFunctionNr = -1;
    }
  }

  //Load all object information (e.g. for range-convertion).
  qres = sql_exec("SELECT number, description, services, sensor_type, sensor_size, sensor_min, sensor_max, actuator_type, actuator_size, actuator_min, actuator_max, actuator_def FROM templates WHERE man_id=$1 AND prod_id=$2 AND firm_major=$3", 1, 3, params);
  if (qres == NULL)
  {
    return 0;
  }
  for (cntRow=0; cntRow<PQntuples(qres); cntRow++)
  {
    unsigned short int ObjectNr;
    int cntField;

    cntField=0;
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%hd", &ObjectNr);
    if (ObjectNr >= 1024)
    {
      OBJECT_INFORMATION_STRUCT *obj_info = &node_info->ObjectInformation[ObjectNr-1024];

      sscanf(PQgetvalue(qres, cntRow, cntField++), "%s", &obj_info->Description[0]);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%c", &obj_info->Services);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%c", &obj_info->SensorDataType);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%c", &obj_info->SensorDataSize);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &obj_info->SensorDataMinimal);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &obj_info->SensorDataMaximal);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%c", &obj_info->ActuatorDataType);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%c", &obj_info->ActuatorDataSize);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &obj_info->ActuatorDataMinimal);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &obj_info->ActuatorDataMaximal);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &obj_info->ActuatorDataDefault);
      
      if (strcmp("Slot number", &obj_info->Description[0]) == 0)
      { 
        node_info->SlotNumberObjectNr = ObjectNr;
      }
      else if (strcmp("Input channel count", &obj_info->Description[0]) == 0)
      {
        node_info->InputChannelCountObjectNr = ObjectNr;
      }
      else if (strcmp("Output channel count", &obj_info->Description[0]) == 0)
      {
        node_info->OutputChannelCountObjectNr = ObjectNr;
      }
    }
  }
  return 0;
}

int db_read_node_defaults(ONLINE_NODE_INFORMATION_STRUCT *node_info, unsigned short int first_obj, unsigned short int last_obj)
{
  char str[3][32];
  const char *params[3];
  int cntParams;
  int cntRow;

  for (cntParams=0; cntParams<3; cntParams++)
  {
    params[cntParams] = (const char *)str[cntParams];
  }

  sprintf(str[0], "%d", node_info->MambaNetAddress);
  sprintf(str[1], "%d", first_obj);
  sprintf(str[2], "%d", last_obj);
 
  PGresult *qres = sql_exec("SELECT object, data FROM defaults WHERE addr=$1 AND object>=$2 AND object<=$3", 0, 3, params);
  if (qres == NULL)
  {
    return 0;
  }
  
  for (cntRow=0; cntRow<PQntuples(qres); cntRow++)
  {
    bool sent_default_data = false;
    unsigned short int ObjectNr = -1;
    sscanf(PQgetvalue(qres, cntRow, 0), "%hd", &ObjectNr);

    if ((ObjectNr>=1024) && ((ObjectNr-1024)<node_info->NumberOfCustomObjects))
    {
      OBJECT_INFORMATION_STRUCT *obj_info = &node_info->ObjectInformation[ObjectNr-1024];
      if (obj_info->ActuatorDataType != NO_DATA_DATATYPE)
      {
        unsigned char TransmitData[128];
        unsigned char cntTransmitData = 0;
        TransmitData[cntTransmitData++] = (ObjectNr>>8)&0xFF;
        TransmitData[cntTransmitData++] = ObjectNr&0xFF;
        TransmitData[cntTransmitData++] = MAMBANET_OBJECT_ACTION_SET_ACTUATOR_DATA;//Set actuator
        TransmitData[cntTransmitData++] = obj_info->ActuatorDataType;
        switch (obj_info->ActuatorDataType)
        {
          case UNSIGNED_INTEGER_DATATYPE:
          case STATE_DATATYPE:
          {
            unsigned long int DataValue;
            sscanf(PQgetvalue(qres, cntRow, 0), "(%ld,,,)", &DataValue);

            if (DataValue != obj_info->ActuatorDataDefault)
            {
              TransmitData[cntTransmitData++] = obj_info->ActuatorDataSize;
              for (int cntByte=0; cntByte<obj_info->ActuatorDataSize; cntByte++)
              {
                TransmitData[cntTransmitData++] = (DataValue>>(((obj_info->ActuatorDataSize-1)-cntByte)*8))&0xFF;
                sent_default_data = true;
              }
            }            
          }
          break;
          case SIGNED_INTEGER_DATATYPE:
          {
            long int DataValue;
            sscanf(PQgetvalue(qres, cntRow, 0), "(%ld,,,)", &DataValue);

            if (DataValue != obj_info->ActuatorDataDefault)
            {
              TransmitData[cntTransmitData++] = obj_info->ActuatorDataSize;
              for (int cntByte=0; cntByte<obj_info->ActuatorDataSize; cntByte++)
              {
                TransmitData[cntTransmitData++] = (DataValue>>(((obj_info->ActuatorDataSize-1)-cntByte)*8))&0xFF;
                sent_default_data = true;
              }
            }            
          }
          break;
          case OCTET_STRING_DATATYPE:
          {
            char OctetString[256];
            sscanf(PQgetvalue(qres, cntRow, 0), "(,,,%s)", OctetString);
            
            int StringLength = strlen(OctetString);
            if (StringLength>obj_info->ActuatorDataSize)
            {
              StringLength = obj_info->ActuatorDataSize;
            }

            TransmitData[cntTransmitData++] = StringLength;
            for (int cntChar=0; cntChar<StringLength; cntChar++)
            {
              TransmitData[cntTransmitData++] = OctetString[cntChar];
            }
            sent_default_data = true;
          }
          break;
          case FLOAT_DATATYPE:
          {
            float DataValue;
            sscanf(PQgetvalue(qres, cntRow, 0), "(,%f,,)", &DataValue);
            
            if (DataValue != obj_info->ActuatorDataDefault)
            {
              TransmitData[cntTransmitData++] = obj_info->ActuatorDataSize;

              if (Float2VariableFloat(DataValue, obj_info->ActuatorDataSize, &TransmitData[cntTransmitData]) == 0)
              {
                cntTransmitData += cntTransmitData;
              }
              sent_default_data = true;
            }
            break;
            case BIT_STRING_DATATYPE:
            {
              unsigned char cntBit;
              unsigned long DataValue;
              char BitString[256];
              sscanf(PQgetvalue(qres, cntRow, 0), "(,,%s,)", BitString);
             
              int StringLength = strlen(BitString);
              if (StringLength>obj_info->ActuatorDataSize)
              {
                StringLength = obj_info->ActuatorDataSize;
              }

              DataValue = 0;
              for (cntBit=0; cntBit<StringLength; cntBit++)
              {
                if ((BitString[cntBit] == '0') || (BitString[cntBit] == '1'))
                { 
                  DataValue |= (BitString[cntBit]-'0');
                }
              }

              TransmitData[cntTransmitData++] = obj_info->ActuatorDataSize;
              for (int cntByte=0; cntByte<obj_info->ActuatorDataSize; cntByte++)
              {
                TransmitData[cntTransmitData++] = (DataValue>>(((obj_info->ActuatorDataSize-1)-cntByte)*8))&0xFF;
              }
              sent_default_data = true;
            }
            break;
          }
          
          if (sent_default_data)
          {
            SendMambaNetMessage(node_info->MambaNetAddress, AxumEngineDefaultObjects.MambaNetAddress, 0, 0, MAMBANET_OBJECT_MESSAGETYPE, TransmitData, cntTransmitData);
          } 
        }
      }
    }
  }
  return 1;
}

int db_read_node_configuration(ONLINE_NODE_INFORMATION_STRUCT *node_info, unsigned short int first_obj, unsigned short int last_obj)
{
  char str[3][32];
  const char *params[3];
  int cntParams;
  int cntRow;

  for (cntParams=0; cntParams<3; cntParams++)
  {
    params[cntParams] = (const char *)str[cntParams];
  }

  sprintf(str[0], "%d", node_info->MambaNetAddress);
  sprintf(str[1], "%hd", first_obj);
  sprintf(str[2], "%hd", last_obj);
 
  PGresult *qres = sql_exec("SELECT object, func FROM node_config WHERE addr=$1 AND object>=$2 AND object<=$3", 0, 3, params);
  if (qres == NULL)
  {
    return 0;
  }
  
  for (cntRow=0; cntRow<PQntuples(qres); cntRow++)
  {
    unsigned short int ObjectNr = -1;
    unsigned char type;
    unsigned short int seq_nr;
    unsigned short int func_nr;
    unsigned int TotalFunctionNr = -1;

    sscanf(PQgetvalue(qres, cntRow, 0), "%hd", &ObjectNr);
    sscanf(PQgetvalue(qres, cntRow, 1), "(%c,%hd,%hd)", &type, &seq_nr, &func_nr);
    TotalFunctionNr = (((unsigned int)type)<<24)|(((unsigned int)seq_nr)<<12)|func_nr;
    

    if ((ObjectNr>=1024) && ((ObjectNr-1024)<node_info->NumberOfCustomObjects))
    {
      if (node_info->SensorReceiveFunction != NULL)
      { 
        SENSOR_RECEIVE_FUNCTION_STRUCT *sensor_rcv_func = &node_info->SensorReceiveFunction[ObjectNr-1024];
        
        sensor_rcv_func->FunctionNr = TotalFunctionNr;
        sensor_rcv_func->LastChangedTime = 0;
        sensor_rcv_func->PreviousLastChangedTime = 0;
        sensor_rcv_func->TimeBeforeMomentary = DEFAULT_TIME_BEFORE_MOMENTARY;
      }
      MakeObjectListPerFunction(TotalFunctionNr);

      CheckObjectsToSent(TotalFunctionNr, node_info->MambaNetAddress);
      delay_ms(1);
    }
  }
  PQclear(qres);
  return 1;
}

int db_update_rack_organization(unsigned char slot_nr, unsigned long int addr, unsigned char input_ch_cnt, unsigned char output_ch_cnt)
{
  char str[4][32];
  const char *params[4];
  int cntParams;

  for (cntParams=0; cntParams<4; cntParams++)
  {
    params[cntParams] = (const char *)str[cntParams];
  }

  sprintf(str[0], "%d", slot_nr+1);
  sprintf(str[1], "%ld", addr);
  sprintf(str[2], "%c", input_ch_cnt);
  sprintf(str[3], "%c", output_ch_cnt);
  
  PGresult *qres = sql_exec("UPDATE rack_organization SET MambaNetAddress = $2, InputChannelCount = $3, OutputChannelCount = $4 WHERE SlotNr = $1", 1, 4, params);
  if (qres == NULL)
  {
    return 0;
  }

  PQclear(qres);
  return 1; 
}

int db_update_rack_organization_input_ch_cnt(unsigned long int addr, unsigned char cnt)
{
  char str[2][32];
  const char *params[2];
  int cntParams;

  for (cntParams=0; cntParams<2; cntParams++)
  {
    params[cntParams] = (const char *)str[cntParams];
  }

  sprintf(str[0], "%c", cnt);
  sprintf(str[1], "%ld", addr);

  PGresult *qres = sql_exec("UPDATE rack_organization SET InputChannelCount=$1 WHERE MambaNetAddress=$2", 1, 2, params);
  if (qres == NULL)
  {
    return 0;
  }
  PQclear(qres);
  return 1; 
}

int db_update_rack_organization_output_ch_cnt(unsigned long int addr, unsigned char cnt)
{
  char str[2][32];
  const char *params[2];
  int cntParams;

  for (cntParams=0; cntParams<2; cntParams++)
  {
    params[cntParams] = (const char *)str[cntParams];
  }

  sprintf(str[0], "%c", cnt);
  sprintf(str[1], "%ld", addr);

  PGresult *qres = sql_exec("UPDATE rack_organization SET OutputChannelCount=$1 WHERE MambaNetAddress=$2", 1, 2, params);
  if (qres == NULL)
  {
    return 0;
  }
  PQclear(qres);
  return 1; 
}

void db_processnotifies()
{
  sql_processnotifies(); 
}

void db_close()
{
  sql_close();
}
 
 /*
int db_load_engine_functions() {
  PGresult *qres;
  char q[1024];
  int row_count, cnt_row;

  sprintf(q,"SELECT func, name, rcv_type, xmt_type FROM functions");
  qres = PQexec(sqldb, q); if(qres == NULL || PQresultStatus(qres) != PGRES_COMMAND_OK) {
    //writelog("SQL Error on %s:%d: %s", __FILE__, __LINE__, PQresultErrorMessage(qres));
  
    PQclear(qres);
    return 0;
  }

  //parse result of the query
  row_count = PQntuples(qres);   
  if (row_count && (PQnfields(qres)==4)) {
    for (cnt_row=0; cnt_row<row_countl cnt_row++) {
      if (sscanf(PQgetvalue(qres, cnt_row, 0, "(%d,%d,%d)", type, sequence_number, function_number)) != 3) {
        writelog("SQL Error on %s:%d %s", __FILE__, __LINE__, "sscanf failed");
        return 0;
      }
      if (sscanf(PQgetvalue(qres, cnt_row, 1, "%s", function_name)) != 1) {
        writelog("SQL Error on %s:%d %s", __FILE__, __LINE__, "sscanf failed");
        return 0;
      }
      if (sscanf(PQgetvalue(qres, cnt_row, 2, "%hd", function_rcv_type)) != 1) {
        writelog("SQL Error on %s:%d %s", __FILE__, __LINE__, "sscanf failed");
        return 0;
      }
      if (sscanf(PQgetvalue(qres, cnt_row, 3, "%hd", function_xmt_type)) != 1) {
        writelog("SQL Error on %s:%d %s", __FILE__, __LINE__, "sscanf failed");
        return 0;
      }
    }
  }

  PQclear(qs);
  return row_count;
}
*/


//*********************************************
// NOTIFY EVENTS
//*********************************************

void db_event_templates_changed(char myself, char *arg)
{
  //No implementation
  arg = NULL;
  myself=0;
}

void db_event_addresses_changed(char myself, char *arg)
{
  //No implementation
  arg = NULL;
  myself=0;
}

void db_event_slot_config_changed(char myself, char *arg)
{
  db_read_slot_config();
  arg = NULL;
  myself=0;
}

void db_event_src_config_changed(char myself, char *arg)
{
  unsigned short int number;

  sscanf(arg, "%hd", &number);
  db_read_src_config(number, number);

  myself=0;
}

void db_event_module_config_changed(char myself, char *arg)
{
  unsigned char number;

  sscanf(arg, "%c", &number);
  db_read_module_config(number, number);

  myself=0;
}

void db_event_buss_config_changed(char myself, char *arg)
{
  unsigned char number;

  sscanf(arg, "%c", &number);
  db_read_buss_config(number, number);

  myself=0;
}

void db_event_monitor_buss_config_changed(char myself, char *arg)
{
  unsigned char number;

  sscanf(arg, "%c", &number);
  db_read_monitor_buss_config(number, number);

  myself=0;
}

void db_event_extern_src_config_changed(char myself, char *arg)
{
  unsigned char number;

  sscanf(arg, "%c", &number);
  db_read_extern_src_config(number, number);

  myself=0;
}

void db_event_talkback_config_changed(char myself, char *arg)
{
  unsigned char number;

  sscanf(arg, "%c", &number);
  db_read_talkback_config(number, number);

  myself=0;
}

void db_event_global_config_changed(char myself, char *arg)
{
  db_read_global_config();

  arg = NULL;
  myself=0;
}

void db_event_dest_config_changed(char myself, char *arg)
{
  unsigned short int number;

  sscanf(arg, "%hd", &number);
  db_read_dest_config(number, number);

  myself=0;
}
