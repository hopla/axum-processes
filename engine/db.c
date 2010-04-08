#include "common.h"
#include "engine.h"
#include "dsp.h"
#include "db.h"
#include "mbn.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

//#define LOG_DEBUG_ENABLED

#ifdef LOG_DEBUG_ENABLED
  #define LOG_DEBUG(...) log_write(__VA_ARGS__)
#else
  #define LOG_DEBUG(...)
#endif

extern AXUM_DATA_STRUCT AxumData;
extern unsigned short int dB2Position[1500];
extern float Position2dB[1024];
extern struct mbn_handler *mbn;
extern int AxumApplicationAndDSPInitialized;
extern DSP_HANDLER_STRUCT *dsp_handler;

extern matrix_sources_struct matrix_sources;
extern preset_pos_struct presets;

extern PGconn *sql_conn;

struct sql_notify notifies[] = {
  { (char *)"templates_changed",                      db_event_templates_changed},
  { (char *)"address_removed",                        db_event_address_removed},
  { (char *)"slot_config_changed",                    db_event_slot_config_changed},
  { (char *)"src_config_changed",                     db_event_src_config_changed},
  { (char *)"module_config_changed",                  db_event_module_config_changed},
  { (char *)"buss_config_changed",                    db_event_buss_config_changed},
  { (char *)"monitor_buss_config_changed",            db_event_monitor_buss_config_changed},
  { (char *)"extern_src_config_changed",              db_event_extern_src_config_changed},
  { (char *)"talkback_config_changed",                db_event_talkback_config_changed},
  { (char *)"global_config_changed",                  db_event_global_config_changed},
  { (char *)"dest_config_changed",                    db_event_dest_config_changed},
  { (char *)"node_config_changed",                    db_event_node_config_changed},
  { (char *)"defaults_changed",                       db_event_defaults_changed},
  { (char *)"src_preset_changed",                     db_event_src_preset_changed},
  { (char *)"routing_preset_changed",                 db_event_routing_preset_changed},
  { (char *)"buss_preset_changed",                    db_event_buss_preset_changed},
  { (char *)"buss_preset_rows_changed",               db_event_buss_preset_rows_changed},
  { (char *)"monitor_buss_preset_rows_changed",       db_event_monitor_buss_preset_rows_changed},
  { (char *)"console_preset_changed",                 db_event_console_preset_changed},
};

double read_minmax(char *mambanet_minmax)
{
  int value_int;
  float value_float;

  if (sscanf(mambanet_minmax, "(%d,)", &value_int) == 1)
  {
    return (double)value_int;
  }
  else if (sscanf(mambanet_minmax, "(,%f)", &value_float) == 1)
  {
    return (double)value_float;
  }
  return 0;
}

void db_open(char *dbstr)
{
  LOG_DEBUG("[%s] enter", __func__);
  sql_open(dbstr, 19, notifies);
  LOG_DEBUG("[%s] leave", __func__);
}

int db_get_fd()
{
  LOG_DEBUG("[%s] enter", __func__);

  int fd = PQsocket(sql_conn);
  if (fd < 0)
  {
    LOG_DEBUG("db_get_fd/PQsocket error, no server connection is currently open");
  }

  LOG_DEBUG("[%s] leave", __func__);

  return fd;
}

int db_get_matrix_sources()
{
  int cntRow;
  LOG_DEBUG("[%s] enter", __func__);

  PGresult *qres = sql_exec("SELECT type, MIN(number), MAX(number) FROM matrix_sources GROUP BY type", 1, 0, NULL);
  if (qres == NULL)
  {
    LOG_DEBUG("[%s] leave with error", __func__);
    return 0;
  }

  for (cntRow=0; cntRow<PQntuples(qres); cntRow++)
  {
    int cntField=0;
    char src_type[32];

    strcpy(src_type, PQgetvalue(qres, cntRow, cntField++));

    if (strcmp(src_type, "buss") == 0)
    {
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &matrix_sources.src_offset.min.buss);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &matrix_sources.src_offset.max.buss);
    }
    else if (strcmp(src_type, "insert out") == 0)
    {
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &matrix_sources.src_offset.min.insert_out);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &matrix_sources.src_offset.max.insert_out);
    }
    else if (strcmp(src_type, "monitor buss") == 0)
    {
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &matrix_sources.src_offset.min.monitor_buss);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &matrix_sources.src_offset.max.monitor_buss);
    }
    else if (strcmp(src_type, "n-1") == 0)
    {
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &matrix_sources.src_offset.min.mixminus);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &matrix_sources.src_offset.max.mixminus);
    }
    else if (strcmp(src_type, "source") == 0)
    {
      cntField++; //skip this minimal value, will be determined in next query.
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &matrix_sources.src_offset.max.source);
    }
  }
  PQclear(qres);

  //find the first source number
  qres = sql_exec("SELECT MAX(number)+1 FROM matrix_sources WHERE type != 'source'", 1, 0, NULL);
  if (qres == NULL)
  {
    LOG_DEBUG("[%s] leave with error", __func__);
    return 0;
  }
  sscanf(PQgetvalue(qres,0,0), "%d", &matrix_sources.src_offset.min.source);
  PQclear(qres);

  //load the complete matrix_sources list
  qres = sql_exec("SELECT pos, number, active, type FROM matrix_sources", 1, 0, NULL);
  if (qres == NULL)
  {
    LOG_DEBUG("[%s] leave with error", __func__);
    return 0;
  }

  for (cntRow=0; cntRow<MAX_POS_LIST_SIZE; cntRow++)
  {
    matrix_sources.pos[cntRow].src = INT_MIN;
    matrix_sources.pos[cntRow].active = 0;
    matrix_sources.pos[cntRow].type = none;
    matrix_sources.pos[cntRow].pool[0] = 0;
    matrix_sources.pos[cntRow].pool[1] = 0;
    matrix_sources.pos[cntRow].pool[2] = 0;
    matrix_sources.pos[cntRow].pool[3] = 0;
    matrix_sources.pos[cntRow].pool[4] = 0;
    matrix_sources.pos[cntRow].pool[5] = 0;
    matrix_sources.pos[cntRow].pool[6] = 0;
    matrix_sources.pos[cntRow].pool[7] = 0;
  }

  for (cntRow=0; cntRow<PQntuples(qres); cntRow++)
  {
    int cntField=0;
    char src_type[32];
    unsigned short int pos;

    sscanf(PQgetvalue(qres, cntRow, cntField++), "%hd", &pos);
    if ((pos>=1) && (pos<=MAX_POS_LIST_SIZE))
    {
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &matrix_sources.pos[pos-1].src);
      matrix_sources.pos[pos-1].active = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");

      strcpy(src_type, PQgetvalue(qres, cntRow, cntField++));

      if (strcmp(src_type, "none") == 0)
      {
        matrix_sources.pos[pos-1].type = none;
      }
      else if (strcmp(src_type, "buss") == 0)
      {
        matrix_sources.pos[pos-1].type = buss;
      }
      else if (strcmp(src_type, "insert out") == 0)
      {
        matrix_sources.pos[pos-1].type = insert_out;
      }
      else if (strcmp(src_type, "monitor buss") == 0)
      {
        matrix_sources.pos[pos-1].type = monitor_buss;
      }
      else if (strcmp(src_type, "n-1") == 0)
      {
        matrix_sources.pos[pos-1].type = mixminus;
      }
      else if (strcmp(src_type, "source") == 0)
      {
        matrix_sources.pos[pos-1].type = source;
      }
    }
  }
  PQclear(qres);

  LOG_DEBUG("[%s] leave", __func__);

  return 1;
}

int db_read_slot_config()
{
  int cntRow;

  LOG_DEBUG("[%s] enter", __func__);

  PGresult *qres = sql_exec("SELECT slot_nr, addr FROM slot_config", 1, 0, NULL);
  if (qres == NULL)
  {
    LOG_DEBUG("[%s] leave with error", __func__);
    return 0;
  }
  for (cntRow=0; cntRow<42; cntRow++)
  {
    AxumData.RackOrganization[cntRow] = 0;
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

  LOG_DEBUG("[%s] leave", __func__);

  return 1;
}

int db_read_src_preset(unsigned short int first_preset, unsigned short int last_preset)
{
  char str[2][32];
  const char *params[2];
  int cntParams;
  int cntRow;

  LOG_DEBUG("[%s] enter", __func__);

  for (cntParams=0; cntParams<2; cntParams++)
  {
    params[cntParams] = (const char *)str[cntParams];
  }

  sprintf(str[0], "%hd", first_preset);
  sprintf(str[1], "%hd", last_preset);

  PGresult *qres = sql_exec("SELECT number,               \
                                    label,                \
                                    use_gain_preset,      \
                                    gain,                 \
                                    use_lc_preset,        \
                                    lc_frequency,         \
                                    lc_on_off,            \
                                    use_insert_preset,    \
                                    insert_on_off,        \
                                    use_phase_preset,     \
                                    phase,                \
                                    phase_on_off,         \
                                    use_mono_preset,      \
                                    mono,                 \
                                    mono_on_off,          \
                                    use_eq_preset,        \
                                    eq_band_1_range,      \
                                    eq_band_1_level,      \
                                    eq_band_1_freq,       \
                                    eq_band_1_bw,         \
                                    eq_band_1_slope,      \
                                    eq_band_1_type,       \
                                    eq_band_2_range,      \
                                    eq_band_2_level,      \
                                    eq_band_2_freq,       \
                                    eq_band_2_bw,         \
                                    eq_band_2_slope,      \
                                    eq_band_2_type,       \
                                    eq_band_3_range,      \
                                    eq_band_3_level,      \
                                    eq_band_3_freq,       \
                                    eq_band_3_bw,         \
                                    eq_band_3_slope,      \
                                    eq_band_3_type,       \
                                    eq_band_4_range,      \
                                    eq_band_4_level,      \
                                    eq_band_4_freq,       \
                                    eq_band_4_bw,         \
                                    eq_band_4_slope,      \
                                    eq_band_4_type,       \
                                    eq_band_5_range,      \
                                    eq_band_5_level,      \
                                    eq_band_5_freq,       \
                                    eq_band_5_bw,         \
                                    eq_band_5_slope,      \
                                    eq_band_5_type,       \
                                    eq_band_6_range,      \
                                    eq_band_6_level,      \
                                    eq_band_6_freq,       \
                                    eq_band_6_bw,         \
                                    eq_band_6_slope,      \
                                    eq_band_6_type,       \
                                    eq_on_off,            \
                                    use_dyn_preset,       \
                                    d_exp_threshold,      \
                                    agc_threshold,        \
                                    agc_amount,           \
                                    dyn_on_off,           \
                                    use_mod_preset,       \
                                    mod_pan,              \
                                    mod_lvl,              \
                                    mod_on_off            \
                                    FROM src_preset       \
                                    WHERE number>=$1 AND number<=$2", 1, 2, params);
  if (qres == NULL)
  {
    LOG_DEBUG("[%s] leave with error", __func__);
    return 0;
  }
  for (cntRow=0; cntRow<PQntuples(qres); cntRow++)
  {
    unsigned int number;
    int cntField;
    int cntEQ;

    cntField = 0;
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &number);

    AXUM_PRESET_DATA_STRUCT *PresetData = &AxumData.PresetData[number-1];

    strncpy(PresetData->PresetName, PQgetvalue(qres, cntRow, cntField++), 32);

    PresetData->UseGain = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &PresetData->Gain);

    PresetData->UseFilter = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &PresetData->Filter.Frequency);
    PresetData->FilterOnOff = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");

    PresetData->UseInsert = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    PresetData->InsertOnOff = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");

    PresetData->UsePhase = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%hhd", &PresetData->Phase);
    PresetData->PhaseOnOff = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");

    PresetData->UseMono = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%hhd", &PresetData->Mono);
    PresetData->MonoOnOff = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");

    PresetData->UseEQ = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    for (cntEQ=0; cntEQ<6; cntEQ++)
    {
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &PresetData->EQBand[cntEQ].Range);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &PresetData->EQBand[cntEQ].Level);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &PresetData->EQBand[cntEQ].Frequency);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &PresetData->EQBand[cntEQ].Bandwidth);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &PresetData->EQBand[cntEQ].Slope);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%hhd", (char *)&PresetData->EQBand[cntEQ].Type);
    }
    PresetData->EQOnOff = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");

    PresetData->UseDynamics = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");

    sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &PresetData->DownwardExpanderThreshold);
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &PresetData->AGCThreshold);
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%hhd", &PresetData->AGCAmount);
    PresetData->DynamicsOnOff = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");

    PresetData->UseModule = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &PresetData->Panorama);
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &PresetData->FaderLevel);
    PresetData->ModuleState = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
  }
  PQclear(qres);

  //get position/type list for presets
  qres = sql_exec("SELECT number FROM src_preset ORDER BY pos", 1, 0, NULL);
  if (qres == NULL)
  {
    LOG_DEBUG("[%s] leave with error", __func__);
    return 0;
  }
  for (int cntPreset=0; cntPreset<MAX_NR_OF_PRESETS; cntPreset++)
  {
    presets.pos[cntPreset].filled = 0;
  }
  presets.pos[0].number = 0;
  presets.pos[0].filled = 1;
  for (cntRow=0; cntRow<PQntuples(qres); cntRow++)
  {
    unsigned int number;
    int cntField;

    cntField = 0;
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &number);
    presets.pos[cntRow+1].number = number;
    presets.pos[cntRow+1].filled = 1;
  }
  PQclear(qres);

  LOG_DEBUG("[%s] leave", __func__);
  return 1;
}

int db_read_src_config(unsigned short int first_src, unsigned short int last_src)
{
  char str[2][32];
  const char *params[2];
  int cntParams;
  int cntRow;

  LOG_DEBUG("[%s] enter", __func__);

  for (cntParams=0; cntParams<2; cntParams++)
  {
    params[cntParams] = (const char *)str[cntParams];
  }

  sprintf(str[0], "%hd", first_src);
  sprintf(str[1], "%hd", last_src);

  PGresult *qres = sql_exec("SELECT number,               \
                                    label,                \
                                    input1_addr,          \
                                    input1_sub_ch,        \
                                    input2_addr,          \
                                    input2_sub_ch,        \
                                    input_phantom,        \
                                    input_pad,            \
                                    input_gain,           \
                                    default_src_preset,   \
                                    redlight1,            \
                                    redlight2,            \
                                    redlight3,            \
                                    redlight4,            \
                                    redlight5,            \
                                    redlight6,            \
                                    redlight7,            \
                                    redlight8,            \
                                    monitormute1,         \
                                    monitormute2,         \
                                    monitormute3,         \
                                    monitormute4,         \
                                    monitormute5,         \
                                    monitormute6,         \
                                    monitormute7,         \
                                    monitormute8,         \
                                    monitormute9,         \
                                    monitormute10,        \
                                    monitormute11,        \
                                    monitormute12,        \
                                    monitormute13,        \
                                    monitormute14,        \
                                    monitormute15,        \
                                    monitormute16         \
                                    FROM src_config       \
                                    WHERE number>=$1 AND number<=$2", 1, 2, params);
  if (qres == NULL)
  {
    LOG_DEBUG("[%s] leave with error", __func__);
    return 0;
  }
  for (cntRow=0; cntRow<PQntuples(qres); cntRow++)
  {
    int number;
    unsigned char cntModule;
    int cntField;

    cntField = 0;
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &number);

    AXUM_SOURCE_DATA_STRUCT *SourceData = &AxumData.SourceData[number-1];

    strncpy(SourceData->SourceName, PQgetvalue(qres, cntRow, cntField++), 32);
    if (sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &SourceData->InputData[0].MambaNetAddress) <= 0)
    {
      SourceData->InputData[0].MambaNetAddress = 0;
    }
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%hhd", &SourceData->InputData[0].SubChannel);
    SourceData->InputData[0].SubChannel--;
    if (sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &SourceData->InputData[1].MambaNetAddress) <= 0)
    {
      SourceData->InputData[1].MambaNetAddress = 0;
    }
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%hhd", &SourceData->InputData[1].SubChannel);
    SourceData->InputData[1].SubChannel--;
    SourceData->Phantom = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    SourceData->Pad = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &SourceData->Gain);
    if (sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &SourceData->DefaultProcessingPreset) <= 0)
    {
      SourceData->DefaultProcessingPreset = 0;
    }

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

    unsigned int FunctionNrToSent = 0x05000000 | ((number-1)<<12);
    CheckObjectsToSent(FunctionNrToSent | SOURCE_FUNCTION_PHANTOM);
    CheckObjectsToSent(FunctionNrToSent | SOURCE_FUNCTION_PAD);
    CheckObjectsToSent(FunctionNrToSent | SOURCE_FUNCTION_GAIN);

    for (cntModule=0; cntModule<128; cntModule++)
    {
      if (AxumData.ModuleData[cntModule].SelectedSource == ((number-1)+matrix_sources.src_offset.min.source))
      {
        SetAxum_ModuleSource(cntModule);
        SetAxum_ModuleMixMinus(cntModule, 0);

        unsigned int FunctionNrToSent = ((cntModule<<12)&0xFFF000);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_1);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_2);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_3);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_CONTROL_4);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_SOURCE_GAIN_LEVEL);
      }
    }
  }
  PQclear(qres);

  db_get_matrix_sources();

  LOG_DEBUG("[%s] leave", __func__);

  return 1;
}

int db_read_module_config(unsigned char first_mod, unsigned char last_mod, unsigned char console, unsigned char take_source_a)
{
  char str[4][32];
  const char *params[4];
  int cntParams;
  int cntRow;
  char first_console = 1;
  char last_console = 4;

  LOG_DEBUG("[%s] enter", __func__);

  for (cntParams=0; cntParams<4; cntParams++)
  {
    params[cntParams] = (const char *)str[cntParams];
  }
  if (console<4)
  {
    first_console = console+1;
    last_console = console+1;
  }

  sprintf(str[0], "%hd", first_mod);
  sprintf(str[1], "%hd", last_mod);
  sprintf(str[2], "%hd", first_console);
  sprintf(str[3], "%hd", last_console);

  PGresult *qres = sql_exec("SELECT number,               \
                                    console,              \
                                    source_a,             \
                                    source_b,             \
                                    source_c,             \
                                    source_d,             \
                                    source_e,             \
                                    source_f,             \
                                    source_g,             \
                                    source_h,             \
                                    source_a_preset,      \
                                    source_b_preset,      \
                                    source_c_preset,      \
                                    source_d_preset,      \
                                    source_e_preset,      \
                                    source_f_preset,      \
                                    source_g_preset,      \
                                    source_h_preset,      \
                                    overrule_active,      \
                                    use_insert_preset,    \
                                    insert_source,        \
                                    insert_on_off,        \
                                    use_gain_preset,      \
                                    gain,                 \
                                    use_lc_preset,        \
                                    lc_frequency,         \
                                    lc_on_off,            \
                                    use_phase_preset,     \
                                    phase,                \
                                    phase_on_off,         \
                                    use_mono_preset,      \
                                    mono,                 \
                                    mono_on_off,          \
                                    use_eq_preset,        \
                                    eq_band_1_range,      \
                                    eq_band_1_level,      \
                                    eq_band_1_freq,       \
                                    eq_band_1_bw,         \
                                    eq_band_1_slope,      \
                                    eq_band_1_type,       \
                                    eq_band_2_range,      \
                                    eq_band_2_level,      \
                                    eq_band_2_freq,       \
                                    eq_band_2_bw,         \
                                    eq_band_2_slope,      \
                                    eq_band_2_type,       \
                                    eq_band_3_range,      \
                                    eq_band_3_level,      \
                                    eq_band_3_freq,       \
                                    eq_band_3_bw,         \
                                    eq_band_3_slope,      \
                                    eq_band_3_type,       \
                                    eq_band_4_range,      \
                                    eq_band_4_level,      \
                                    eq_band_4_freq,       \
                                    eq_band_4_bw,         \
                                    eq_band_4_slope,      \
                                    eq_band_4_type,       \
                                    eq_band_5_range,      \
                                    eq_band_5_level,      \
                                    eq_band_5_freq,       \
                                    eq_band_5_bw,         \
                                    eq_band_5_slope,      \
                                    eq_band_5_type,       \
                                    eq_band_6_range,      \
                                    eq_band_6_level,      \
                                    eq_band_6_freq,       \
                                    eq_band_6_bw,         \
                                    eq_band_6_slope,      \
                                    eq_band_6_type,       \
                                    eq_on_off,            \
                                    use_dyn_preset,       \
                                    d_exp_threshold,      \
                                    agc_threshold,        \
                                    agc_amount,           \
                                    dyn_on_off,           \
                                    use_mod_preset,       \
                                    mod_level,            \
                                    mod_on_off,           \
                                    buss_1_2_use_preset,  \
                                    buss_1_2_level,       \
                                    buss_1_2_on_off,      \
                                    buss_1_2_pre_post,    \
                                    buss_1_2_balance,     \
                                    buss_1_2_assignment,  \
                                    buss_3_4_use_preset,  \
                                    buss_3_4_level,       \
                                    buss_3_4_on_off,      \
                                    buss_3_4_pre_post,    \
                                    buss_3_4_balance,     \
                                    buss_3_4_assignment,  \
                                    buss_5_6_use_preset,  \
                                    buss_5_6_level,       \
                                    buss_5_6_on_off,      \
                                    buss_5_6_pre_post,    \
                                    buss_5_6_balance,     \
                                    buss_5_6_assignment,  \
                                    buss_7_8_use_preset,  \
                                    buss_7_8_level,       \
                                    buss_7_8_on_off,      \
                                    buss_7_8_pre_post,    \
                                    buss_7_8_balance,     \
                                    buss_7_8_assignment,  \
                                    buss_9_10_use_preset,  \
                                    buss_9_10_level,       \
                                    buss_9_10_on_off,      \
                                    buss_9_10_pre_post,    \
                                    buss_9_10_balance,     \
                                    buss_9_10_assignment,  \
                                    buss_11_12_use_preset,  \
                                    buss_11_12_level,       \
                                    buss_11_12_on_off,      \
                                    buss_11_12_pre_post,    \
                                    buss_11_12_balance,     \
                                    buss_11_12_assignment,  \
                                    buss_13_14_use_preset,  \
                                    buss_13_14_level,       \
                                    buss_13_14_on_off,      \
                                    buss_13_14_pre_post,    \
                                    buss_13_14_balance,     \
                                    buss_13_14_assignment,  \
                                    buss_15_16_use_preset,  \
                                    buss_15_16_level,       \
                                    buss_15_16_on_off,      \
                                    buss_15_16_pre_post,    \
                                    buss_15_16_balance,     \
                                    buss_15_16_assignment,  \
                                    buss_17_18_use_preset,  \
                                    buss_17_18_level,       \
                                    buss_17_18_on_off,      \
                                    buss_17_18_pre_post,    \
                                    buss_17_18_balance,     \
                                    buss_17_18_assignment,  \
                                    buss_19_20_use_preset,  \
                                    buss_19_20_level,       \
                                    buss_19_20_on_off,      \
                                    buss_19_20_pre_post,    \
                                    buss_19_20_balance,     \
                                    buss_19_20_assignment,  \
                                    buss_21_22_use_preset,  \
                                    buss_21_22_level,       \
                                    buss_21_22_on_off,      \
                                    buss_21_22_pre_post,    \
                                    buss_21_22_balance,     \
                                    buss_21_22_assignment,  \
                                    buss_23_24_use_preset,  \
                                    buss_23_24_level,       \
                                    buss_23_24_on_off,      \
                                    buss_23_24_pre_post,    \
                                    buss_23_24_balance,     \
                                    buss_23_24_assignment,  \
                                    buss_25_26_use_preset,  \
                                    buss_25_26_level,       \
                                    buss_25_26_on_off,      \
                                    buss_25_26_pre_post,    \
                                    buss_25_26_balance,     \
                                    buss_25_26_assignment,  \
                                    buss_27_28_use_preset,  \
                                    buss_27_28_level,       \
                                    buss_27_28_on_off,      \
                                    buss_27_28_pre_post,    \
                                    buss_27_28_balance,     \
                                    buss_27_28_assignment,  \
                                    buss_29_30_use_preset,  \
                                    buss_29_30_level,       \
                                    buss_29_30_on_off,      \
                                    buss_29_30_pre_post,    \
                                    buss_29_30_balance,     \
                                    buss_29_30_assignment,  \
                                    buss_31_32_use_preset,  \
                                    buss_31_32_level,       \
                                    buss_31_32_on_off,      \
                                    buss_31_32_pre_post,    \
                                    buss_31_32_balance,     \
                                    buss_31_32_assignment   \
                                    FROM module_config      \
                                    WHERE number>=$1 AND number<=$2 AND console>=$3 AND console<=$4 ", 1, 4, params);
  if (qres == NULL)
  {
    LOG_DEBUG("[%s] leave with error", __func__);
    return 0;
  }
  for (cntRow=0; cntRow<PQntuples(qres); cntRow++)
  {
    short int number;
    unsigned int cntField;
    unsigned char cntEQ;
    unsigned char cntBuss;
//    float OldLevel;
//    unsigned char OldOn;

    cntField = 0;
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%hd", &number);

    if (number>0)
    {
      int Console = 0;
      int ModuleNr = number-1;

      AXUM_MODULE_DATA_STRUCT *ModuleData = &AxumData.ModuleData[ModuleNr];
      AXUM_DEFAULT_MODULE_DATA_STRUCT *DefaultModuleData = &ModuleData->Defaults;

      AXUM_MODULE_DATA_STRUCT CurrentModuleData = *ModuleData;

      sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &Console);
      ModuleData->Console = Console-1;
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &ModuleData->Source1A);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &ModuleData->Source1B);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &ModuleData->Source2A);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &ModuleData->Source2B);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &ModuleData->Source3A);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &ModuleData->Source3B);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &ModuleData->Source4A);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &ModuleData->Source4B);
      if (sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &ModuleData->ProcessingPreset1A) < 0)
      {
        ModuleData->ProcessingPreset1A = 0;
      }
      if (sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &ModuleData->ProcessingPreset1B) < 0)
      {
        ModuleData->ProcessingPreset1B = 0;
      }
      if (sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &ModuleData->ProcessingPreset2A) < 0)
      {
        ModuleData->ProcessingPreset2A = 0;
      }
      if (sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &ModuleData->ProcessingPreset2B) < 0)
      {
        ModuleData->ProcessingPreset2B = 0;
      }
      if (sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &ModuleData->ProcessingPreset3A) < 0)
      {
        ModuleData->ProcessingPreset3A = 0;
      }
      if (sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &ModuleData->ProcessingPreset3B) < 0)
      {
        ModuleData->ProcessingPreset3B = 0;
      }
      if (sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &ModuleData->ProcessingPreset4A) < 0)
      {
        ModuleData->ProcessingPreset4A = 0;
      }
      if (sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &ModuleData->ProcessingPreset4B) < 0)
      {
        ModuleData->ProcessingPreset4B = 0;
      }
      ModuleData->OverruleActive = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
      DefaultModuleData->InsertUsePreset = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &DefaultModuleData->InsertSource);
      DefaultModuleData->InsertOnOff = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
      DefaultModuleData->GainUsePreset = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &DefaultModuleData->Gain);
      DefaultModuleData->FilterUsePreset = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &DefaultModuleData->Filter.Frequency);
      DefaultModuleData->FilterOnOff = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
      DefaultModuleData->PhaseUsePreset = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%hhd", &DefaultModuleData->Phase);
      DefaultModuleData->PhaseOnOff = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
      DefaultModuleData->MonoUsePreset = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%hhd", &DefaultModuleData->Mono);
      DefaultModuleData->MonoOnOff = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
      DefaultModuleData->EQUsePreset = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
      for (cntEQ=0; cntEQ<6; cntEQ++)
      {
        sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &DefaultModuleData->EQBand[cntEQ].Range);
        sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &DefaultModuleData->EQBand[cntEQ].Level);
        sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &DefaultModuleData->EQBand[cntEQ].Frequency);
        sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &DefaultModuleData->EQBand[cntEQ].Bandwidth);
        sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &DefaultModuleData->EQBand[cntEQ].Slope);
        sscanf(PQgetvalue(qres, cntRow, cntField++), "%hhd", (char *)&DefaultModuleData->EQBand[cntEQ].Type);
      }
      DefaultModuleData->EQOnOff = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
      DefaultModuleData->DynamicsUsePreset = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &DefaultModuleData->DownwardExpanderThreshold);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &DefaultModuleData->AGCThreshold);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%hhd", &DefaultModuleData->AGCAmount);
      DefaultModuleData->DynamicsOnOff = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
      DefaultModuleData->ModuleUsePreset = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &DefaultModuleData->FaderLevel);
      DefaultModuleData->On = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");

      //Next are routing presets and assignment of modules to busses.
      for (cntBuss=0; cntBuss<16; cntBuss++)
      {
        DefaultModuleData->Buss[cntBuss].Use = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");

        sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &DefaultModuleData->Buss[cntBuss].Level);
        DefaultModuleData->Buss[cntBuss].On = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
        DefaultModuleData->Buss[cntBuss].PreModuleLevel = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
        sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &DefaultModuleData->Buss[cntBuss].Balance);
        ModuleData->Buss[cntBuss].Assigned = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");

        //for now initialize with first settings
      }

      //Check if module preset source/preset/routing changeda
      unsigned int FunctionNrToSent = ModuleNr<<12;
      if ((CurrentModuleData.Source1A != ModuleData->Source1A) ||
          (CurrentModuleData.ProcessingPreset1A != ModuleData->ProcessingPreset1A))
      {
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_1A);
        if (ModuleData->ModulePreset == 0)
        {
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_A);
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_A_B);
        }
      }
      if ((CurrentModuleData.Source1B != ModuleData->Source1B) ||
          (CurrentModuleData.ProcessingPreset1B != ModuleData->ProcessingPreset1B))
      {
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_1B);
        if (ModuleData->ModulePreset == 0)
        {
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_B);
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_A_B);
        }
      }
      if ((CurrentModuleData.Source2A != ModuleData->Source2A) ||
          (CurrentModuleData.ProcessingPreset2A != ModuleData->ProcessingPreset2A))
      {
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_2A);
        if (ModuleData->ModulePreset == 1)
        {
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_A);
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_A_B);
        }
      }
      if ((CurrentModuleData.Source2B != ModuleData->Source2B) ||
          (CurrentModuleData.ProcessingPreset2B != ModuleData->ProcessingPreset2B))
      {
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_2B);
        if (ModuleData->ModulePreset == 1)
        {
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_B);
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_A_B);
        }
      }
      if ((CurrentModuleData.Source3A != ModuleData->Source3A) ||
          (CurrentModuleData.ProcessingPreset3A != ModuleData->ProcessingPreset3A))
      {
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_3A);
        if (ModuleData->ModulePreset == 2)
        {
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_A);
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_A_B);
        }
      }
      if ((CurrentModuleData.Source3B != ModuleData->Source3B) ||
          (CurrentModuleData.ProcessingPreset3B != ModuleData->ProcessingPreset3B))
      {
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_3B);
        if (ModuleData->ModulePreset == 2)
        {
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_B);
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_A_B);
        }
      }
      if ((CurrentModuleData.Source4A != ModuleData->Source4A) ||
          (CurrentModuleData.ProcessingPreset4A != ModuleData->ProcessingPreset4A))
      {
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_4A);
        if (ModuleData->ModulePreset == 3)
        {
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_A);
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_A_B);
        }
      }
      if ((CurrentModuleData.Source4B != ModuleData->Source4B) ||
          (CurrentModuleData.ProcessingPreset4B != ModuleData->ProcessingPreset4B))
      {
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_4B);
        if (ModuleData->ModulePreset == 3)
        {
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_B);
          CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_A_B);
        }
      }

      //Take source A
      if (take_source_a)
      {
        DoAxum_SetNewSource(ModuleNr, ModuleData->Source1A, 1);
        DoAxum_LoadProcessingPreset(ModuleNr, ModuleData->ProcessingPreset1A, 0, 1, 1);
        DoAxum_LoadRoutingPreset(ModuleNr, 1, 0, 1, 1);
      }

/*    OldLevel = ModuleData->FaderLevel;
      OldOn = ModuleData->On;

      //Place data in module data, but may be overide by a preset later on...
      ModuleData->InsertSource = DefaultModuleData->InsertSource;
      ModuleData->InsertOnOff = DefaultModuleData->InsertOnOff;
      ModuleData->Gain = DefaultModuleData->Gain;
      ModuleData->Filter = DefaultModuleData->Filter;
      ModuleData->FilterOnOff = DefaultModuleData->FilterOnOff;
      ModuleData->Phase = DefaultModuleData->Phase;
      ModuleData->PhaseOnOff = DefaultModuleData->PhaseOnOff;
      ModuleData->Mono = DefaultModuleData->Mono;
      ModuleData->MonoOnOff = DefaultModuleData->MonoOnOff;
      ModuleData->EQBand[0] = DefaultModuleData->EQBand[0];
      ModuleData->EQBand[1] = DefaultModuleData->EQBand[1];
      ModuleData->EQBand[2] = DefaultModuleData->EQBand[2];
      ModuleData->EQBand[3] = DefaultModuleData->EQBand[3];
      ModuleData->EQBand[4] = DefaultModuleData->EQBand[4];
      ModuleData->EQBand[5] = DefaultModuleData->EQBand[5];
      ModuleData->EQOnOff = DefaultModuleData->EQOnOff;
      ModuleData->DownwardExpanderThreshold = DefaultModuleData->DownwardExpanderThreshold;
      ModuleData->AGCThreshold = DefaultModuleData->AGCThreshold;
      ModuleData->AGCAmount = DefaultModuleData->AGCAmount;
      ModuleData->DynamicsOnOff = DefaultModuleData->DynamicsOnOff;
      ModuleData->Panorama = DefaultModuleData->Panorama;
      ModuleData->FaderLevel = DefaultModuleData->FaderLevel;
      ModuleData->On = DefaultModuleData->On;
      for (cntBuss=0; cntBuss<16; cntBuss++)
      {
        ModuleData->Buss[cntBuss].Level = DefaultModuleData->Buss[cntBuss].Level;
        ModuleData->Buss[cntBuss].On = DefaultModuleData->Buss[cntBuss].On;
        ModuleData->Buss[cntBuss].PreModuleLevel = DefaultModuleData->Buss[cntBuss].PreModuleLevel;
        ModuleData->Buss[cntBuss].Balance = DefaultModuleData->Buss[cntBuss].Balance;
      }

      int ModuleNr = number-1;
      if (AxumApplicationAndDSPInitialized)
      {
        if (take_source_a)
        {
          DoAxum_SetNewSource(ModuleNr, ModuleData->SourceA, 1);
        }
        else
        {
          DoAxum_SetNewSource(ModuleNr, ModuleData->SelectedSource, 1);
        }
        SetAxum_ModuleInsertSource(ModuleNr);
        SetAxum_ModuleProcessing(ModuleNr);

        if (take_source_a)
        {
          DoAxum_LoadProcessingPreset(ModuleNr, ModuleData->SourceAPreset, 1, 1);
          DoAxum_LoadRoutingPreset(ModuleNr, 1, 1, 1);
        }
        else
        {
          int NewRoutingPreset = 0;
          DoAxum_LoadProcessingPreset(ModuleNr, ModuleData->SelectedPreset, 1, 1);
          if ((ModuleData->SelectedSource == ModuleData->SourceA) &&
             (ModuleData->SelectedPreset == ModuleData->SourceAPreset))
          {
            NewRoutingPreset = 1;
          }
          else if ((ModuleData->SelectedSource == ModuleData->SourceB) &&
             (ModuleData->SelectedPreset == ModuleData->SourceBPreset))
          {
            NewRoutingPreset = 2;
          }
          else if ((ModuleData->SelectedSource == ModuleData->SourceC) &&
             (ModuleData->SelectedPreset == ModuleData->SourceCPreset))
          {
            NewRoutingPreset = 3;
          }
          else if ((ModuleData->SelectedSource == ModuleData->SourceD) &&
             (ModuleData->SelectedPreset == ModuleData->SourceDPreset))
          {
            NewRoutingPreset = 4;
          }
          else if ((ModuleData->SelectedSource == ModuleData->SourceE) &&
             (ModuleData->SelectedPreset == ModuleData->SourceEPreset))
          {
            NewRoutingPreset = 5;
          }
          else if ((ModuleData->SelectedSource == ModuleData->SourceF) &&
             (ModuleData->SelectedPreset == ModuleData->SourceFPreset))
          {
            NewRoutingPreset = 6;
          }
          else if ((ModuleData->SelectedSource == ModuleData->SourceG) &&
             (ModuleData->SelectedPreset == ModuleData->SourceGPreset))
          {
            NewRoutingPreset = 7;
          }
          else if ((ModuleData->SelectedSource == ModuleData->SourceH) &&
             (ModuleData->SelectedPreset == ModuleData->SourceHPreset))
          {
            NewRoutingPreset = 8;
          }
          DoAxum_LoadRoutingPreset(ModuleNr, NewRoutingPreset, 1, 1);
        }

        //Set fader level and On;
        float NewLevel = AxumData.ModuleData[ModuleNr].FaderLevel;
        int NewOn = AxumData.ModuleData[ModuleNr].On;

        SetAxum_BussLevels(ModuleNr);

        unsigned int FunctionNrToSent = ((ModuleNr<<12)&0xFFF000);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_MODULE_LEVEL);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_MODULE_ON);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_MODULE_OFF);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_MODULE_ON_OFF);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_FADER_ON);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_FADER_OFF);
        CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_FADER_ON_OFF);

        if ((AxumData.ModuleData[ModuleNr].SelectedSource >= matrix_sources.src_offset.min.source) && (AxumData.ModuleData[ModuleNr].SelectedSource<=matrix_sources.src_offset.max.source))
        {
          unsigned int SourceNr = AxumData.ModuleData[ModuleNr].SelectedSource-matrix_sources.src_offset.min.source;
          FunctionNrToSent = 0x05000000 | (SourceNr<<12);
          CheckObjectsToSent(FunctionNrToSent | SOURCE_FUNCTION_MODULE_FADER_ON);
          CheckObjectsToSent(FunctionNrToSent | SOURCE_FUNCTION_MODULE_FADER_OFF);
          CheckObjectsToSent(FunctionNrToSent | SOURCE_FUNCTION_MODULE_FADER_ON_OFF);
          CheckObjectsToSent(FunctionNrToSent | SOURCE_FUNCTION_MODULE_FADER_AND_ON_ACTIVE);
          CheckObjectsToSent(FunctionNrToSent | SOURCE_FUNCTION_MODULE_FADER_AND_ON_INACTIVE);
        }

        if (((OldLevel<=-80) && (NewLevel>-80)) ||
            ((OldLevel>-80) && (NewLevel<=-80)) ||
            (OldOn != NewOn))
        { //fader on changed
          DoAxum_ModuleStatusChanged(ModuleNr, 1);
        }
      }*/
    }
  }
  PQclear(qres);

  LOG_DEBUG("[%s] leave", __func__);

  return 1;
}

int db_read_buss_config(unsigned char first_buss, unsigned char last_buss, unsigned char console)
{
  char str[4][32];
  const char *params[4];
  int cntParams;
  int cntRow;
  char first_console = 1;
  char last_console = 4;

  LOG_DEBUG("[%s] enter", __func__);

  for (cntParams=0; cntParams<4; cntParams++)
  {
    params[cntParams] = (const char *)str[cntParams];
  }

  if (console<4)
  {
    first_console = console+1;
    last_console = console+1;
  }
  sprintf(str[0], "%hd", first_buss);
  sprintf(str[1], "%hd", last_buss);
  sprintf(str[2], "%hd", first_console);
  sprintf(str[3], "%hd", last_console);

  PGresult *qres = sql_exec("SELECT number, label, console, mono, pre_on, pre_balance, level, on_off, interlock, exclusive, global_reset FROM buss_config WHERE number>=$1 AND number<=$2 AND console>=$3 AND console<=$4", 1, 4, params);
  if (qres == NULL)
  {
    LOG_DEBUG("[%s] leave with error", __func__);
    return 0;
  }
  for (cntRow=0; cntRow<PQntuples(qres); cntRow++)
  {
    short int number;
    int cntField;

    cntField=0;
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%hd", &number);

    AXUM_BUSS_MASTER_DATA_STRUCT *BussMasterData = &AxumData.BussMasterData[number-1];
    strncpy(BussMasterData->Label, PQgetvalue(qres, cntRow, cntField++), 32);
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%hhd", &BussMasterData->Console);
    BussMasterData->Console--;
    BussMasterData->Mono = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    BussMasterData->PreModuleOn = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    BussMasterData->PreModuleBalance = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &BussMasterData->Level);
    BussMasterData->On = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    BussMasterData->Interlock = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    BussMasterData->Exclusive = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    BussMasterData->GlobalBussReset = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");

    if (AxumApplicationAndDSPInitialized)
    {
      unsigned int FunctionNrToSent = 0x01000000 | (((number-1)<<12)&0xFFF000);
      CheckObjectsToSent(FunctionNrToSent | BUSS_FUNCTION_MASTER_PRE);
      CheckObjectsToSent(FunctionNrToSent | BUSS_FUNCTION_MASTER_LEVEL);
      CheckObjectsToSent(FunctionNrToSent | BUSS_FUNCTION_MASTER_ON_OFF);
      CheckObjectsToSent(FunctionNrToSent | BUSS_FUNCTION_LABEL);

      SetAxum_BussMasterLevels();

      FunctionNrToSent = 0x04000000;
      CheckObjectsToSent(FunctionNrToSent | GLOBAL_FUNCTION_MASTER_CONTROL_1);
      CheckObjectsToSent(FunctionNrToSent | GLOBAL_FUNCTION_MASTER_CONTROL_2);
      CheckObjectsToSent(FunctionNrToSent | GLOBAL_FUNCTION_MASTER_CONTROL_3);
      CheckObjectsToSent(FunctionNrToSent | GLOBAL_FUNCTION_MASTER_CONTROL_4);

      for (int cntModule=0; cntModule<128; cntModule++)
      {
        SetAxum_BussLevels(cntModule);
      }

      for (int cntDestination=0; cntDestination<=1280; cntDestination++)
      {
        if (AxumData.DestinationData[cntDestination].Source == (number+matrix_sources.src_offset.min.buss))
        {
          FunctionNrToSent = 0x06000000 | (cntDestination<<12);
          CheckObjectsToSent(FunctionNrToSent | DESTINATION_FUNCTION_SOURCE);
        }
      }
    }
  }
  PQclear(qres);

  LOG_DEBUG("[%s] leave", __func__);

  return 1;
}

int db_read_monitor_buss_config(unsigned char first_mon_buss, unsigned char last_mon_buss, unsigned char console)
{
  char str[4][32];
  const char *params[4];
  int cntParams;
  int cntRow;
  char first_console = 1;
  char last_console = 4;

  LOG_DEBUG("[%s] enter", __func__);

  for (cntParams=0; cntParams<4; cntParams++)
  {
    params[cntParams] = (const char *)str[cntParams];
  }
  if (console<4)
  {
    first_console = console+1;
    last_console = console+1;
  }

  sprintf(str[0], "%hd", first_mon_buss);
  sprintf(str[1], "%hd", last_mon_buss);
  sprintf(str[2], "%hd", first_console);
  sprintf(str[3], "%hd", last_console);

  PGresult *qres = sql_exec("SELECT number, label, console, interlock, default_selection, buss_1_2, buss_3_4, buss_5_6, buss_7_8, buss_9_10, buss_11_12, buss_13_14, buss_15_16, buss_17_18, buss_19_20, buss_21_22, buss_23_24, buss_25_26, buss_27_28, buss_29_30, buss_31_32, dim_level FROM monitor_buss_config WHERE number>=$1 AND number<=$2 AND console>=$3 AND console<=$4", 1, 4, params);
  if (qres == NULL)
  {
    LOG_DEBUG("[%s] leave with error", __func__);
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
    strncpy(MonitorData->Label, PQgetvalue(qres, cntRow, cntField++), 32);

    sscanf(PQgetvalue(qres, cntRow, cntField++), "%hhd", &MonitorData->Console);
    MonitorData->Console--;

    MonitorData->Interlock = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%hhd", &MonitorData->DefaultSelection);
    for (cntMonitorBuss=0; cntMonitorBuss<16; cntMonitorBuss++)
    {
      MonitorData->AutoSwitchingBuss[cntMonitorBuss] = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    }
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &MonitorData->SwitchingDimLevel);

    //turn on default selection
    int MonitorBussNr = number-1;
    for (int cntBuss=0; cntBuss<24; cntBuss++)
    {
      int DefaultSelection = AxumData.Monitor[MonitorBussNr].DefaultSelection;
      unsigned int FunctionNrToSent = 0x02000000 | ((MonitorBussNr<<12)&0xFFF000);
      if (cntBuss<16)
      {
        if (cntBuss == DefaultSelection)
        {
          AxumData.Monitor[MonitorBussNr].Buss[cntBuss] = 1;
        }
        else
        {
          AxumData.Monitor[MonitorBussNr].Buss[cntBuss] = 0;
        }
        if (AxumApplicationAndDSPInitialized)
        {
          CheckObjectsToSent(FunctionNrToSent | (MONITOR_BUSS_FUNCTION_BUSS_1_2_ON_OFF+cntBuss));
        }
      }
      else if (cntBuss<24)
      {
        if (cntBuss == DefaultSelection)
        {
          AxumData.Monitor[MonitorBussNr].Ext[cntBuss-16] = 1;
        }
        else
        {
          AxumData.Monitor[MonitorBussNr].Ext[cntBuss-16] = 0;
        }
        if (AxumApplicationAndDSPInitialized)
        {
          CheckObjectsToSent(FunctionNrToSent | (MONITOR_BUSS_FUNCTION_EXT_1_ON_OFF+(cntBuss-16)));
        }
      }
    }

    if (AxumApplicationAndDSPInitialized)
    {
      SetAxum_MonitorBuss(MonitorBussNr);

      unsigned int FunctionNrToSent = 0x02000000 | ((MonitorBussNr<<12)&0xFFF000);
      CheckObjectsToSent(FunctionNrToSent | MONITOR_BUSS_FUNCTION_LABEL);
    }
  }
  PQclear(qres);

  LOG_DEBUG("[%s] leave", __func__);

  return 1;
}

int db_read_extern_src_config(unsigned char first_dsp_card, unsigned char last_dsp_card)
{
  char str[2][32];
  const char *params[2];
  int cntParams;
  int cntRow;

  LOG_DEBUG("[%s] enter", __func__);

  for (cntParams=0; cntParams<2; cntParams++)
  {
    params[cntParams] = (const char *)str[cntParams];
  }

  sprintf(str[0], "%hd", first_dsp_card);
  sprintf(str[1], "%hd", last_dsp_card);

  PGresult *qres = sql_exec("SELECT number, ext1, ext2, ext3, ext4, ext5, ext6, ext7, ext8, safe1, safe2, safe3, safe4, safe5, safe6, safe7, safe8 FROM extern_src_config WHERE number>=$1 AND number<=$2", 1, 2, params);
  if (qres == NULL)
  {
    LOG_DEBUG("[%s] leave with error", __func__);
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
    for (cntExternSource=0; cntExternSource<8; cntExternSource++)
    {
      ExternSource->InterlockSafe[cntExternSource] = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    }

    if (AxumApplicationAndDSPInitialized)
    {
      SetAxum_ExternSources(number-1);
    }
  }
  PQclear(qres);

  LOG_DEBUG("[%s] leave", __func__);

  return 1;
}

int db_read_talkback_config(unsigned char first_tb, unsigned char last_tb)
{
  char str[2][32];
  const char *params[2];
  int cntParams;
  int cntRow;

  LOG_DEBUG("[%s] enter", __func__);

  for (cntParams=0; cntParams<2; cntParams++)
  {
    params[cntParams] = (const char *)str[cntParams];
  }

  sprintf(str[0], "%hd", first_tb);
  sprintf(str[1], "%hd", last_tb);

  PGresult *qres = sql_exec("SELECT number, source FROM talkback_config WHERE number>=$1 AND number<=$2", 1, 2, params);
  if (qres == NULL)
  {
    LOG_DEBUG("[%s] leave with error", __func__);
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

  LOG_DEBUG("[%s] leave", __func__);

  return 1;
}

int db_read_global_config()
{
  int cntRow;

  LOG_DEBUG("[%s] enter", __func__);

  PGresult *qres = sql_exec("SELECT samplerate, ext_clock, headroom, level_reserve, auto_momentary, startup_state FROM global_config", 1, 0, NULL);

  if (qres == NULL)
  {
    LOG_DEBUG("[%s] leave with error", __func__);
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
    AxumData.AutoMomentary = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    AxumData.StartupState = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");

    if (AxumApplicationAndDSPInitialized)
    {
      dsp_set_interpolation(dsp_handler, AxumData.Samplerate);
    }
    SetBackplaneClock();

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
  PQclear(qres);

  LOG_DEBUG("[%s] leave", __func__);

  return 1;
}

int db_read_dest_config(unsigned short int first_dest, unsigned short int last_dest)
{
  char str[2][32];
  const char *params[2];
  int cntParams;
  int cntRow;
  unsigned char *DestinationFound;

  LOG_DEBUG("[%s] enter", __func__);

  for (cntParams=0; cntParams<2; cntParams++)
  {
    params[cntParams] = (const char *)str[cntParams];
  }

  sprintf(str[0], "%hd", first_dest);
  sprintf(str[1], "%hd", last_dest);

  if (first_dest>last_dest)
  {
    unsigned short int dummy = first_dest;
    first_dest = last_dest;
    last_dest = dummy;
  }
  //to make sure we can do 'if <'
  last_dest++;

  DestinationFound = new unsigned char[last_dest-first_dest];
  if (DestinationFound == NULL)
  {
    log_write("[%s] Error no memory available for array DestinationNotFound", __func__);
    LOG_DEBUG("[%s] leave with error", __func__);
    return 0;
  }
  for (cntRow=0; cntRow<(last_dest-first_dest); cntRow++)
  {
    DestinationFound[cntRow] = 0;
  }

  PGresult *qres = sql_exec("SELECT number, label, output1_addr, output1_sub_ch, output2_addr, output2_sub_ch, level, source, routing, mix_minus_source FROM dest_config WHERE number>=$1 AND number<=$2", 1, 2, params);
  if (qres == NULL)
  {
    LOG_DEBUG("[%s] leave with error", __func__);
    return 0;
  }

  for (cntRow=0; cntRow<PQntuples(qres); cntRow++)
  {
    short int number;
    int cntField;

    cntField = 0;
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%hd", &number);

    DestinationFound[number-first_dest] = 1;

    AXUM_DESTINATION_DATA_STRUCT *DestinationData = &AxumData.DestinationData[number-1];

    unsigned int OldOutput1Address = DestinationData->OutputData[0].MambaNetAddress;
    unsigned char OldOutput1SubChannel = DestinationData->OutputData[0].SubChannel;
    unsigned int OldOutput2Address = DestinationData->OutputData[1].MambaNetAddress;
    unsigned char OldOutput2SubChannel = DestinationData->OutputData[1].SubChannel;
    strncpy(DestinationData->DestinationName, PQgetvalue(qres, cntRow, cntField++), 32);
    if (sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &DestinationData->OutputData[0].MambaNetAddress) <= 0)
    {
      DestinationData->OutputData[0].MambaNetAddress = 0;
    }
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%hhd", &DestinationData->OutputData[0].SubChannel);
    DestinationData->OutputData[0].SubChannel--;
    if (sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &DestinationData->OutputData[1].MambaNetAddress) <= 0)
    {
      DestinationData->OutputData[1].MambaNetAddress = 0;
    }
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%hhd", &DestinationData->OutputData[1].SubChannel);
    DestinationData->OutputData[1].SubChannel--;
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &DestinationData->Level);
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &DestinationData->Source);
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%hhd", &DestinationData->Routing);
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &DestinationData->MixMinusSource);

    if ((DestinationData->OutputData[0].MambaNetAddress == 0) && (OldOutput1Address > 0))
    {
      SetAxum_RemoveOutputRouting(OldOutput1Address, OldOutput1SubChannel);
    }
    if ((DestinationData->OutputData[1].MambaNetAddress == 0) && (OldOutput2Address > 0))
    {
      SetAxum_RemoveOutputRouting(OldOutput2Address, OldOutput2SubChannel);
    }
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

  //Check if destination is deleted
  for (cntRow=0; cntRow<(last_dest-first_dest); cntRow++)
  {
    AXUM_DESTINATION_DATA_STRUCT *DestinationData = &AxumData.DestinationData[first_dest+cntRow-1];

    if (!DestinationFound[cntRow])
    {
      SetAxum_RemoveOutputRouting(DestinationData->OutputData[0].MambaNetAddress, DestinationData->OutputData[0].SubChannel);
      SetAxum_RemoveOutputRouting(DestinationData->OutputData[1].MambaNetAddress, DestinationData->OutputData[1].SubChannel);
      DestinationData->OutputData[0].MambaNetAddress = 0;
      DestinationData->OutputData[1].MambaNetAddress = 0;

      //Check destinations
      unsigned int DisplayFunctionNr = 0x06000000 | ((first_dest+cntRow-1)<<12);
      CheckObjectsToSent(DisplayFunctionNr | DESTINATION_FUNCTION_LABEL);
      CheckObjectsToSent(DisplayFunctionNr | DESTINATION_FUNCTION_LEVEL);
      CheckObjectsToSent(DisplayFunctionNr | DESTINATION_FUNCTION_SOURCE);
    }
  }

  PQclear(qres);

  LOG_DEBUG("[%s] leave", __func__);

  return 1;
}

int db_read_db_to_position()
{
  int cntRow;
  unsigned short int cntPosition;
  unsigned int db_array_pointer;
  float dB;

  LOG_DEBUG("[%s] enter", __func__);

  PGresult *qres = sql_exec("SELECT db, position FROM db_to_position", 1, 0, NULL);
  if (qres == NULL)
  {
    LOG_DEBUG("[%s] leave with error", __func__);
    return 0;
  }
  for (cntRow=0; cntRow<PQntuples(qres); cntRow++)
  {
    sscanf(PQgetvalue(qres, cntRow, 0), "%f", &dB);
    db_array_pointer = floor((dB*10)+0.5)+1400;

    sscanf(PQgetvalue(qres, cntRow, 1), "%hd", &dB2Position[db_array_pointer]);
  }

  dB = -140;
  for (cntPosition=0; cntPosition<1024; cntPosition++)
  {
    for (db_array_pointer=0; db_array_pointer<1500; db_array_pointer++)
    {
      if (dB2Position[db_array_pointer] == cntPosition)
      {
        dB = ((float)((int)db_array_pointer-1400))/10;
      }
    }
    Position2dB[cntPosition] = dB;
  }

  PQclear(qres);

  LOG_DEBUG("[%s] leave", __func__);

  return 1;
}

int db_read_template_info(ONLINE_NODE_INFORMATION_STRUCT *node_info, unsigned char in_powerup_state)
{
  char str[3][32];
  const char *params[3];
  int cntParams;
  int cntRow;
  int maxobjnr;

  LOG_DEBUG("[%s] enter", __func__);

  for (cntParams=0; cntParams<3; cntParams++)
  {
    params[cntParams] = (const char *)str[cntParams];
  }

  //Determine number of objects for memory reservation
  sprintf(str[0], "%hd", node_info->ManufacturerID);
  sprintf(str[1], "%hd", node_info->ProductID);
  sprintf(str[2], "%hd", node_info->FirmwareMajorRevision);

  PGresult *qres = sql_exec("(                                                                                  \
                               SELECT number+1 FROM templates WHERE man_id=$1 AND prod_id=$2 AND firm_major=$3  \
                               EXCEPT                                                                           \
                               SELECT number FROM templates WHERE man_id=$1 AND prod_id=$2 AND firm_major=$3    \
                             )                                                                                  \
                             EXCEPT                                                                             \
                             SELECT MAX(number)+1 FROM templates WHERE man_id=$1 AND prod_id=$2 AND firm_major=$3", 1, 3, params);
  if (qres == NULL)
  {
    LOG_DEBUG("[%s] leave with error", __func__);
    return 0;
  }
  for (cntRow=0; cntRow<PQntuples(qres); cntRow++)
  {
    log_write("No template info found for obj %s at man_id=%d, prod_id=%d, firm_major=%d", PQgetvalue(qres, cntRow, 0), node_info->ManufacturerID, node_info->ProductID, node_info->FirmwareMajorRevision);
  }
  PQclear(qres);

  qres = sql_exec("SELECT MAX(number) FROM templates WHERE man_id=$1 AND prod_id=$2 AND firm_major=$3", 1, 3, params);
  if (qres == NULL)
  {
    LOG_DEBUG("[%s] leave with error", __func__);
    return 0;
  }

  node_info->NumberOfCustomObjects = 0;
  if (PQntuples(qres))
  {
    if (sscanf(PQgetvalue(qres, 0, 0), "%d", &maxobjnr) == 1)
    {
      node_info->NumberOfCustomObjects = maxobjnr-1023;
    }
  }

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
    }
  }
  PQclear(qres);

  //Load all object information (e.g. for range-convertion).
  qres = sql_exec("SELECT number, description, services, sensor_type, sensor_size, sensor_min, sensor_max, actuator_type, actuator_size, actuator_min, actuator_max, actuator_def FROM templates WHERE man_id=$1 AND prod_id=$2 AND firm_major=$3", 1, 3, params);
  if (qres == NULL)
  {
    LOG_DEBUG("[%s] leave with error", __func__);
    return 0;
  }
  for (cntRow=0; cntRow<PQntuples(qres); cntRow++)
  {
    unsigned short int ObjectNr;
    int cntField;

    cntField=0;
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%hd", &ObjectNr);

    if ((ObjectNr >= 1024) && (ObjectNr < (1024+node_info->NumberOfCustomObjects)))
    {
      OBJECT_INFORMATION_STRUCT *obj_info = &node_info->ObjectInformation[ObjectNr-1024];

      strncpy(&obj_info->Description[0], PQgetvalue(qres, cntRow, cntField++), 32);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%hhd", &obj_info->Services);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%hhd", &obj_info->SensorDataType);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%hhd", &obj_info->SensorDataSize);
      obj_info->SensorDataMinimal = read_minmax(PQgetvalue(qres, cntRow, cntField++));
      obj_info->SensorDataMaximal = read_minmax(PQgetvalue(qres, cntRow, cntField++));
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%hhd", &obj_info->ActuatorDataType);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%hhd", &obj_info->ActuatorDataSize);
      obj_info->ActuatorDataMinimal = read_minmax(PQgetvalue(qres, cntRow, cntField++));
      obj_info->ActuatorDataMaximal = read_minmax(PQgetvalue(qres, cntRow, cntField++));
      obj_info->ActuatorDataDefault = read_minmax(PQgetvalue(qres, cntRow, cntField++));


/*      if (ObjectNr == 1104)
      {
        fprintf(stderr, "min:%f\n", obj_info->ActuatorDataMinimal);
        fprintf(stderr, "max:%f\n", obj_info->ActuatorDataMaximal);
        fprintf(stderr, "def:%f\n", obj_info->ActuatorDataDefault);
      }*/

      if (in_powerup_state)
      {
        obj_info->CurrentActuatorDataDefault = obj_info->ActuatorDataDefault;
      }

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
    else
    {
      if (ObjectNr >= (1024+node_info->NumberOfCustomObjects))
      {
        log_write("[template error] ObjectNr %d to high for 'row count' (man_id:%04X, prod_id:%04X)", ObjectNr, node_info->ManufacturerID, node_info->ProductID);
      }
    }
  }

  PQclear(qres);

  LOG_DEBUG("[%s] leave", __func__);

  return 1;
}

int db_read_node_defaults(ONLINE_NODE_INFORMATION_STRUCT *node_info, unsigned short int first_obj, unsigned short int last_obj, bool DoNotCheckCurrentDefault, bool SetFirmwareDefaults)
{
  char str[3][32];
  const char *params[3];
  int cntParams;
  int cntRow;
  unsigned char *DefaultSet;

  LOG_DEBUG("[%s] enter", __func__);

  for (cntParams=0; cntParams<3; cntParams++)
  {
    params[cntParams] = (const char *)str[cntParams];
  }

  if (first_obj>last_obj)
  {
    unsigned short int dummy = first_obj;
    first_obj = last_obj;
    last_obj = dummy;
  }
  //to make sure we can do 'if <'
  last_obj++;

  DefaultSet = new unsigned char[last_obj-first_obj];
  if (DefaultSet == NULL)
  {
    log_write("[%s] Error no memory available for array DefaultSet", __func__);
    LOG_DEBUG("[%s] leave with error", __func__);
    return 0;
  }
  for (cntRow=0; cntRow<(last_obj-first_obj); cntRow++)
  {
    DefaultSet[cntRow] = 0;
  }

  sprintf(str[0], "%d", node_info->MambaNetAddress);
  sprintf(str[1], "%d", first_obj);
  sprintf(str[2], "%d", last_obj);

  PGresult *qres = sql_exec("SELECT d.object, d.data FROM defaults d                            \
                             WHERE d.addr=$1 AND d.object>=$2 AND d.object<=$3 AND NOT EXISTS   \
                             (SELECT c.object FROM node_config c WHERE c.object=d.object AND c.addr=d.addr)", 1, 3, params);
  if (qres == NULL)
  {
    delete[] DefaultSet;
    LOG_DEBUG("[%s] leave with error", __func__);
    return 0;
  }

  for (cntRow=0; cntRow<PQntuples(qres); cntRow++)
  {
    unsigned char send_default_data = 0;
    unsigned short int ObjectNr = -1;
    unsigned char data_size = 0;
    char OctetString[256];

    mbn_data data;
    sscanf(PQgetvalue(qres, cntRow, 0), "%hd", &ObjectNr);

    if ((ObjectNr>=1024) && ((ObjectNr-1024)<node_info->NumberOfCustomObjects))
    {
      OBJECT_INFORMATION_STRUCT *obj_info = &node_info->ObjectInformation[ObjectNr-1024];
      if (obj_info->ActuatorDataType != MBN_DATATYPE_NODATA)
      {
        switch (obj_info->ActuatorDataType)
        {
          case MBN_DATATYPE_UINT:
          case MBN_DATATYPE_STATE:
          {
            unsigned long int DataValue;
            sscanf(PQgetvalue(qres, cntRow, 1), "(%ld,,,)", &DataValue);

            if ((DataValue != obj_info->CurrentActuatorDataDefault) || (DoNotCheckCurrentDefault))
            {
              data_size = obj_info->ActuatorDataSize;
              if (obj_info->ActuatorDataType == MBN_DATATYPE_UINT)
              {
                data.UInt = DataValue;
              }
              else
              {
                data.State = DataValue;
              }
              send_default_data = 1;
            }
          }
          break;
          case MBN_DATATYPE_SINT:
          {
            long int DataValue;
            sscanf(PQgetvalue(qres, cntRow, 1), "(%ld,,,)", &DataValue);

            if ((DataValue != obj_info->CurrentActuatorDataDefault) || (DoNotCheckCurrentDefault))
            {
              data_size = obj_info->ActuatorDataSize;
              data.SInt = DataValue;
              send_default_data = 1;
              obj_info->CurrentActuatorDataDefault = DataValue;
            }
          }
          break;
          case MBN_DATATYPE_OCTETS:
          {
            sscanf(PQgetvalue(qres, cntRow, 1), "(,,,%s)", OctetString);

            int StringLength = strlen(OctetString);
            if (StringLength>obj_info->ActuatorDataSize)
            {
              StringLength = obj_info->ActuatorDataSize;
            }

            data_size = StringLength;
            data.Octets = (unsigned char *)OctetString;
            send_default_data = 1;
          }
          break;
          case MBN_DATATYPE_FLOAT:
          {
            float DataValue;
            sscanf(PQgetvalue(qres, cntRow, 1), "(,%f,,)", &DataValue);

            if ((DataValue != obj_info->CurrentActuatorDataDefault) || (DoNotCheckCurrentDefault))
            {
              data_size = obj_info->ActuatorDataSize;
              data.Float = DataValue;
              send_default_data = 1;
              obj_info->CurrentActuatorDataDefault = DataValue;
            }
            break;
            case MBN_DATATYPE_BITS:
            {
              unsigned char cntBit;
              unsigned long DataValue;
              char BitString[256];
              sscanf(PQgetvalue(qres, cntRow, 1), "(,,%s,)", BitString);

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

              data_size = obj_info->ActuatorDataSize;
              for (int cntByte=0; cntByte<obj_info->ActuatorDataSize; cntByte++)
              {
                data.Bits[cntByte] = (DataValue>>(((obj_info->ActuatorDataSize-1)-cntByte)*8))&0xFF;
              }
              send_default_data = 1;
              obj_info->CurrentActuatorDataDefault = DataValue;
            }
            break;
          }
        }

        if (send_default_data)
        {
          mbnSetActuatorData(mbn, node_info->MambaNetAddress, ObjectNr, obj_info->ActuatorDataType, data_size, data ,1);
          DefaultSet[ObjectNr-first_obj] = 1;
        }
      }
    }
  }
  PQclear(qres);

  //not set defaults are set to the firmware defaults
  if (SetFirmwareDefaults)
  {
    for (cntRow=0; cntRow<(last_obj-first_obj); cntRow++)
    {
      unsigned char send_default_data = 0;
      unsigned short int ObjectNr = -1;
      unsigned char data_size = 0;

      mbn_data data;
      if (DefaultSet[cntRow] == 0)
      {//need to set template default
        ObjectNr = first_obj+cntRow;

        if ((ObjectNr>=1024) && ((ObjectNr-1024)<node_info->NumberOfCustomObjects))
        {
          OBJECT_INFORMATION_STRUCT *obj_info = &node_info->ObjectInformation[ObjectNr-1024];
          if (obj_info->ActuatorDataType != MBN_DATATYPE_NODATA)
          {
            switch (obj_info->ActuatorDataType)
            {
              case MBN_DATATYPE_UINT:
              case MBN_DATATYPE_STATE:
              {
                data_size = obj_info->ActuatorDataSize;
                if (obj_info->ActuatorDataType == MBN_DATATYPE_UINT)
                {
                  data.UInt = obj_info->CurrentActuatorDataDefault;
                }
                else
                {
                  data.State = obj_info->CurrentActuatorDataDefault;
                }
                send_default_data = 1;
              }
              break;
              case MBN_DATATYPE_SINT:
              {
                data_size = obj_info->ActuatorDataSize;
                data.SInt = obj_info->CurrentActuatorDataDefault;
                send_default_data = 1;
              }
              break;
              case MBN_DATATYPE_FLOAT:
              {
                data_size = obj_info->ActuatorDataSize;
                data.Float = obj_info->CurrentActuatorDataDefault;
                send_default_data = 1;
              }
              break;
            }

            if (send_default_data)
            {
              mbnSetActuatorData(mbn, node_info->MambaNetAddress, ObjectNr, obj_info->ActuatorDataType, data_size, data ,1);
            }
          }
        }
      }
    }
  }
  delete[] DefaultSet;
  LOG_DEBUG("[%s] leave", __func__);

  return 1;
}

int db_read_node_config(ONLINE_NODE_INFORMATION_STRUCT *node_info, unsigned short int first_obj, unsigned short int last_obj)
{
  char str[3][32];
  const char *params[3];
  int cntParams;
  int cntRow;
  unsigned int cntObject;
  unsigned int *OldFunctions;

  LOG_DEBUG("[%s] enter", __func__);

  if (first_obj>last_obj)
  {
    unsigned short int dummy = first_obj;
    first_obj = last_obj;
    last_obj = dummy;
  }
  //to make sure we can do 'if <'
  last_obj++;

  OldFunctions = new unsigned int[last_obj-first_obj];
  if (OldFunctions == NULL)
  {
    log_write("[%s] Error no memory available for array OldFunctions", __func__);
    LOG_DEBUG("[%s] leave with error", __func__);
    return 0;
  }

  for (cntParams=0; cntParams<3; cntParams++)
  {
    params[cntParams] = (const char *)str[cntParams];
  }

  sprintf(str[0], "%d", node_info->MambaNetAddress);
  sprintf(str[1], "%hd", first_obj);
  sprintf(str[2], "%hd", last_obj);

  PGresult *qres = sql_exec("SELECT object, func FROM node_config WHERE addr=$1 AND object>=$2 AND object<=$3", 1, 3, params);
  if (qres == NULL)
  {
    delete[] OldFunctions;
    LOG_DEBUG("[%s] leave with error", __func__);
    return 0;
  }

  for (cntObject=first_obj; cntObject<last_obj; cntObject++)
  {
    OldFunctions[cntObject-first_obj] = node_info->SensorReceiveFunction[cntObject-1024].FunctionNr;
    node_info->SensorReceiveFunction[cntObject-1024].FunctionNr = -1;
  }

  for (cntRow=0; cntRow<PQntuples(qres); cntRow++)
  {
    unsigned short int ObjectNr = -1;
    unsigned char type;
    unsigned short int seq_nr;
    unsigned short int func_nr;
    unsigned int TotalFunctionNr = -1;

    sscanf(PQgetvalue(qres, cntRow, 0), "%hd", &ObjectNr);
    sscanf(PQgetvalue(qres, cntRow, 1), "(%hhd,%hd,%hd)", &type, &seq_nr, &func_nr);
    TotalFunctionNr = (((unsigned int)type)<<24)|(((unsigned int)seq_nr)<<12)|func_nr;

    if ((ObjectNr>=1024) && ((ObjectNr-1024)<node_info->NumberOfCustomObjects))
    {
      if (node_info->SensorReceiveFunction != NULL)
      {
        SENSOR_RECEIVE_FUNCTION_STRUCT *sensor_rcv_func = &node_info->SensorReceiveFunction[ObjectNr-1024];

        sensor_rcv_func->FunctionNr = TotalFunctionNr;
        if (OldFunctions[ObjectNr-first_obj] != sensor_rcv_func->FunctionNr)
        {
          sensor_rcv_func->LastChangedTime = 0;
          sensor_rcv_func->PreviousLastChangedTime = 0;
          sensor_rcv_func->TimeBeforeMomentary = DEFAULT_TIME_BEFORE_MOMENTARY;
          MakeObjectListPerFunction(sensor_rcv_func->FunctionNr);
          CheckObjectsToSent(sensor_rcv_func->FunctionNr, node_info->MambaNetAddress);

          if (((sensor_rcv_func->FunctionNr&0xFF000FFF) == (0x02000000 | MONITOR_BUSS_FUNCTION_SPEAKER_LEVEL)) ||
              ((sensor_rcv_func->FunctionNr&0xFF000FFF) == (0x02000000 | MONITOR_BUSS_FUNCTION_PHONES_LEVEL)))
          {
            if (node_info->ObjectInformation[ObjectNr-1024].SensorDataType != MBN_DATATYPE_NODATA)
            {
              mbnGetSensorData(mbn, node_info->MambaNetAddress, ObjectNr, 1);
            }
          }
        }
      }
    }
  }
  for (cntObject=first_obj; cntObject<last_obj; cntObject++)
  {
    if (node_info->SensorReceiveFunction != NULL)
    {
      SENSOR_RECEIVE_FUNCTION_STRUCT *sensor_rcv_func = &node_info->SensorReceiveFunction[cntObject-1024];

      if (OldFunctions[cntObject-first_obj] != sensor_rcv_func->FunctionNr)
      {
        if (OldFunctions[cntObject-first_obj] != (unsigned int)-1)
        {
          sensor_rcv_func->LastChangedTime = 0;
          sensor_rcv_func->PreviousLastChangedTime = 0;
          sensor_rcv_func->TimeBeforeMomentary = DEFAULT_TIME_BEFORE_MOMENTARY;
          MakeObjectListPerFunction(OldFunctions[cntObject-first_obj]);
        }
      }
    }
  }

  PQclear(qres);

  delete[] OldFunctions;

  LOG_DEBUG("[%s] leave", __func__);

  return 1;
}

int db_read_routing_preset(unsigned char first_mod, unsigned char last_mod)
{
  char str[2][32];
  const char *params[2];
  int cntParams;
  int cntRow;

  LOG_DEBUG("[%s] enter", __func__);

  for (cntParams=0; cntParams<2; cntParams++)
  {
    params[cntParams] = (const char *)str[cntParams];
  }

  sprintf(str[0], "%hd", first_mod);
  sprintf(str[1], "%hd", last_mod);

  PGresult *qres = sql_exec("SELECT mod_number,           \
                                    mod_preset,           \
                                    buss_1_2_use_preset,  \
                                    buss_1_2_level,       \
                                    buss_1_2_on_off,      \
                                    buss_1_2_pre_post,    \
                                    buss_1_2_balance,     \
                                    buss_3_4_use_preset,  \
                                    buss_3_4_level,       \
                                    buss_3_4_on_off,      \
                                    buss_3_4_pre_post,    \
                                    buss_3_4_balance,     \
                                    buss_5_6_use_preset,  \
                                    buss_5_6_level,       \
                                    buss_5_6_on_off,      \
                                    buss_5_6_pre_post,    \
                                    buss_5_6_balance,     \
                                    buss_7_8_use_preset,  \
                                    buss_7_8_level,       \
                                    buss_7_8_on_off,      \
                                    buss_7_8_pre_post,    \
                                    buss_7_8_balance,     \
                                    buss_9_10_use_preset,  \
                                    buss_9_10_level,       \
                                    buss_9_10_on_off,      \
                                    buss_9_10_pre_post,    \
                                    buss_9_10_balance,     \
                                    buss_11_12_use_preset,  \
                                    buss_11_12_level,       \
                                    buss_11_12_on_off,      \
                                    buss_11_12_pre_post,    \
                                    buss_11_12_balance,     \
                                    buss_13_14_use_preset,  \
                                    buss_13_14_level,       \
                                    buss_13_14_on_off,      \
                                    buss_13_14_pre_post,    \
                                    buss_13_14_balance,     \
                                    buss_15_16_use_preset,  \
                                    buss_15_16_level,       \
                                    buss_15_16_on_off,      \
                                    buss_15_16_pre_post,    \
                                    buss_15_16_balance,     \
                                    buss_17_18_use_preset,  \
                                    buss_17_18_level,       \
                                    buss_17_18_on_off,      \
                                    buss_17_18_pre_post,    \
                                    buss_17_18_balance,     \
                                    buss_19_20_use_preset,  \
                                    buss_19_20_level,       \
                                    buss_19_20_on_off,      \
                                    buss_19_20_pre_post,    \
                                    buss_19_20_balance,     \
                                    buss_21_22_use_preset,  \
                                    buss_21_22_level,       \
                                    buss_21_22_on_off,      \
                                    buss_21_22_pre_post,    \
                                    buss_21_22_balance,     \
                                    buss_23_24_use_preset,  \
                                    buss_23_24_level,       \
                                    buss_23_24_on_off,      \
                                    buss_23_24_pre_post,    \
                                    buss_23_24_balance,     \
                                    buss_25_26_use_preset,  \
                                    buss_25_26_level,       \
                                    buss_25_26_on_off,      \
                                    buss_25_26_pre_post,    \
                                    buss_25_26_balance,     \
                                    buss_27_28_use_preset,  \
                                    buss_27_28_level,       \
                                    buss_27_28_on_off,      \
                                    buss_27_28_pre_post,    \
                                    buss_27_28_balance,     \
                                    buss_29_30_use_preset,  \
                                    buss_29_30_level,       \
                                    buss_29_30_on_off,      \
                                    buss_29_30_pre_post,    \
                                    buss_29_30_balance,     \
                                    buss_31_32_use_preset,  \
                                    buss_31_32_level,       \
                                    buss_31_32_on_off,      \
                                    buss_31_32_pre_post,    \
                                    buss_31_32_balance      \
                                    FROM routing_preset     \
                                    WHERE mod_number>=$1 AND mod_number<=$2 \
                                    ORDER BY mod_preset, mod_number", 1, 2, params);
  if (qres == NULL)
  {
    LOG_DEBUG("[%s] leave with error", __func__);
    return 0;
  }
  for (cntRow=0; cntRow<PQntuples(qres); cntRow++)
  {
    short int number;
    unsigned char preset;
    unsigned int cntField;
    unsigned char cntBuss;

    cntField = 0;
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%hd", &number);
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%c", &preset);
    if ((number>=1) && (number<=128) &&
        (preset>='A') && (preset<='H'))
    {
      int ModuleNr = number-1;
      unsigned char PresetNr = preset-'A';

      //Next are routing presets and assignment of modules to busses.
      for (cntBuss=0; cntBuss<16; cntBuss++)
      {
        AXUM_ROUTING_PRESET_DATA_STRUCT CurrentRoutingPresetData = AxumData.ModuleData[ModuleNr].RoutingPreset[PresetNr][cntBuss];
        AXUM_ROUTING_PRESET_DATA_STRUCT *RoutingPresetData = &AxumData.ModuleData[ModuleNr].RoutingPreset[PresetNr][cntBuss];
        unsigned char RoutingPresetChanged = 0;

        RoutingPresetData->Use = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
        sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &RoutingPresetData->Level);
        RoutingPresetData->On = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
        RoutingPresetData->PreModuleLevel = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
        sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &RoutingPresetData->Balance);

        if (AxumData.BussMasterData[cntBuss].Console == AxumData.ModuleData[ModuleNr].Console)
        {
          if (RoutingPresetData->Use)
          {
            if ((RoutingPresetData->Use != CurrentRoutingPresetData.Use) ||
                (RoutingPresetData->On != CurrentRoutingPresetData.On))
            {
              RoutingPresetChanged = 1;
            }
          }
          else if (RoutingPresetData->Use != CurrentRoutingPresetData.Use)
          {
            RoutingPresetChanged = 1;
          }

          if (RoutingPresetChanged)
          {
            unsigned int FunctionNrToSent = ModuleNr<<12;
            switch (PresetNr)
            {
              case 0:
              {
                CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_1A);
                if (AxumData.ModuleData[ModuleNr].ModulePreset == 0)
                {
                  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_A);
                  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_A_B);
                }
              }
              break;
              case 1:
              {
                CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_1B);
                if (AxumData.ModuleData[ModuleNr].ModulePreset == 0)
                {
                  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_B);
                  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_A_B);
                }
              }
              break;
              case 2:
              {
                CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_2A);
                if (AxumData.ModuleData[ModuleNr].ModulePreset == 1)
                {
                  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_A);
                  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_A_B);
                }
              }
              break;
              case 3:
              {
                CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_2B);
                if (AxumData.ModuleData[ModuleNr].ModulePreset == 1)
                {
                  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_B);
                  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_A_B);
                }
              }
              break;
              case 4:
              {
                CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_3A);
                if (AxumData.ModuleData[ModuleNr].ModulePreset == 2)
                {
                  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_A);
                  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_A_B);
                }
              }
              break;
              case 5:
              {
                CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_3B);
                if (AxumData.ModuleData[ModuleNr].ModulePreset == 2)
                {
                  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_B);
                  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_A_B);
                }
              }
              break;
              case 6:
              {
                CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_4A);
                if (AxumData.ModuleData[ModuleNr].ModulePreset == 3)
                {
                  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_A);
                  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_A_B);
                }
              }
              break;
              case 7:
              {
                CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_4B);
                if (AxumData.ModuleData[ModuleNr].ModulePreset == 3)
                {
                  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_B);
                  CheckObjectsToSent(FunctionNrToSent | MODULE_FUNCTION_PRESET_A_B);
                }
              }
              break;
            }
          }
        }
      }
    }
  }
  PQclear(qres);

  LOG_DEBUG("[%s] leave", __func__);

  return 1;
}

int db_read_buss_preset(unsigned short int first_preset, unsigned short int last_preset)
{
  LOG_DEBUG("[%s] enter", __func__);
  LOG_DEBUG("[%s] leave", __func__);
  return 0;
  first_preset = 0;
  last_preset = 0;
}

int db_read_buss_preset_rows(unsigned short int first_preset, unsigned short int last_preset)
{
  char str[2][32];
  const char *params[2];
  int cntParams;
  int cntRow;

  LOG_DEBUG("[%s] enter", __func__);

  for (cntParams=0; cntParams<2; cntParams++)
  {
    params[cntParams] = (const char *)str[cntParams];
  }

  sprintf(str[0], "%hd", first_preset);
  sprintf(str[1], "%hd", last_preset);

  PGresult *qres = sql_exec("SELECT number,               \
                                    buss,                 \
                                    use_preset,           \
                                    level,                \
                                    on_off                \
                                    FROM buss_preset_rows \
                                    WHERE number>=$1 AND number<=$2 \
                                    ORDER BY number", 1, 2, params);
  if (qres == NULL)
  {
    LOG_DEBUG("[%s] leave with error", __func__);
    return 0;
  }
  for (cntRow=0; cntRow<PQntuples(qres); cntRow++)
  {
    short int number;
    short int buss;
    unsigned int cntField;

    cntField = 0;
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%hd", &number);
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%hd", &buss);
    AXUM_BUSS_PRESET_DATA_STRUCT *BussPresetData = &AxumData.BussPresetData[number-1][buss-1];
    BussPresetData->Use = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &BussPresetData->Level);
    BussPresetData->On = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
  }
  PQclear(qres);

  LOG_DEBUG("[%s] leave", __func__);

  return 1;
}

int db_read_monitor_buss_preset_rows(unsigned short int first_preset, unsigned short int last_preset)
{
  char str[2][32];
  const char *params[2];
  int cntParams;
  int cntRow;

  LOG_DEBUG("[%s] enter", __func__);

  for (cntParams=0; cntParams<2; cntParams++)
  {
    params[cntParams] = (const char *)str[cntParams];
  }

  sprintf(str[0], "%hd", first_preset);
  sprintf(str[1], "%hd", last_preset);

  PGresult *qres = sql_exec("SELECT number,                         \
                                    monitor_buss,                   \
                                    use_preset,                     \
                                    on_off                          \
                                    FROM monitor_buss_preset_rows   \
                                    WHERE number>=$1 AND number<=$2 \
                                    ORDER BY number", 1, 2, params);
  if (qres == NULL)
  {
    LOG_DEBUG("[%s] leave with error", __func__);
    return 0;
  }
  for (cntRow=0; cntRow<PQntuples(qres); cntRow++)
  {
    short int number;
    short int monitor_buss;
    unsigned int cntField;
    char BoolChar[24];

    cntField = 0;
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%hd", &number);
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%hd", &monitor_buss);
    AXUM_MONITOR_BUSS_PRESET_DATA_STRUCT *MonitorBussPresetData = &AxumData.MonitorBussPresetData[number-1][monitor_buss-1];

    sscanf(PQgetvalue(qres, cntRow, cntField++), "{%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c}",
                                                   &BoolChar[0], &BoolChar[1], &BoolChar[2], &BoolChar[3], &BoolChar[4], &BoolChar[5], &BoolChar[6], &BoolChar[7],
                                                   &BoolChar[8], &BoolChar[9], &BoolChar[10], &BoolChar[11], &BoolChar[12], &BoolChar[13], &BoolChar[14], &BoolChar[15],
                                                   &BoolChar[16], &BoolChar[17], &BoolChar[18], &BoolChar[19], &BoolChar[20], &BoolChar[21], &BoolChar[22], &BoolChar[23]);
    for (int cntBuss=0; cntBuss<24; cntBuss++)
    {
      MonitorBussPresetData->Use[cntBuss] = (BoolChar[cntBuss] == 't');
    }
    sscanf(PQgetvalue(qres, cntRow, cntField++), "{%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c}",
                                                   &BoolChar[0], &BoolChar[1], &BoolChar[2], &BoolChar[3], &BoolChar[4], &BoolChar[5], &BoolChar[6], &BoolChar[7],
                                                   &BoolChar[8], &BoolChar[9], &BoolChar[10], &BoolChar[11], &BoolChar[12], &BoolChar[13], &BoolChar[14], &BoolChar[15],
                                                   &BoolChar[16], &BoolChar[17], &BoolChar[18], &BoolChar[19], &BoolChar[20], &BoolChar[21], &BoolChar[22], &BoolChar[23]);
    for (int cntBuss=0; cntBuss<24; cntBuss++)
    {
      MonitorBussPresetData->On[cntBuss] = (BoolChar[cntBuss] == 't');
    }
  }
  PQclear(qres);

  LOG_DEBUG("[%s] leave", __func__);

  return 1;
}

int db_read_console_preset(unsigned short int first_preset, unsigned short int last_preset)
{
  char str[2][32];
  const char *params[2];
  int cntParams;
  int cntRow;

  LOG_DEBUG("[%s] enter", __func__);

  for (cntParams=0; cntParams<2; cntParams++)
  {
    params[cntParams] = (const char *)str[cntParams];
  }

  sprintf(str[0], "%hd", first_preset);
  sprintf(str[1], "%hd", last_preset);

  PGresult *qres = sql_exec("SELECT number,               \
                                    label,                \
                                    console1,             \
                                    console2,             \
                                    console3,             \
                                    console4,             \
                                    mod_preset,           \
                                    buss_preset           \
                                    FROM console_preset   \
                                    WHERE number>=$1 AND number<=$2 \
                                    ORDER BY number", 1, 2, params);
  if (qres == NULL)
  {
    LOG_DEBUG("[%s] leave with error", __func__);
    return 0;
  }
  for (cntRow=0; cntRow<PQntuples(qres); cntRow++)
  {
    short int number;
    unsigned int cntField;

    cntField = 0;
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%hd", &number);
    AXUM_CONSOLE_PRESET_DATA_STRUCT *ConsolePresetData = &AxumData.ConsolePresetData[number-1];

    strncpy(ConsolePresetData->Label, PQgetvalue(qres, cntRow, cntField++), 32);
    ConsolePresetData->Console[0] = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    ConsolePresetData->Console[1] = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    ConsolePresetData->Console[2] = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    ConsolePresetData->Console[3] = strcmp(PQgetvalue(qres, cntRow, cntField++), "f");
    if (sscanf(PQgetvalue(qres, cntRow, cntField++), "%c", &ConsolePresetData->ModulePreset) <= 0)
    {
      ConsolePresetData->ModulePreset = -1;
    }
    else
    {
      ConsolePresetData->ModulePreset -= 'A';
    }

    if (sscanf(PQgetvalue(qres, cntRow, cntField++), "%hd", &ConsolePresetData->MixMonitorPreset) <= 0)
    {
      ConsolePresetData->MixMonitorPreset = -1;
    }
    else
    {
      ConsolePresetData->MixMonitorPreset--;
    }
  }
  PQclear(qres);

  LOG_DEBUG("[%s] leave", __func__);

  return 1;
}

int db_insert_slot_config(unsigned char slot_nr, unsigned long int addr, unsigned char input_ch_cnt, unsigned char output_ch_cnt)
{
  char str[4][32];
  const char *params[4];
  int cntParams;

  LOG_DEBUG("[%s] enter", __func__);

  for (cntParams=0; cntParams<4; cntParams++)
  {
    params[cntParams] = (const char *)str[cntParams];
  }

  sprintf(str[0], "%d", slot_nr+1);
  sprintf(str[1], "%ld", addr);
  sprintf(str[2], "%d", input_ch_cnt);
  sprintf(str[3], "%d", output_ch_cnt);

  PGresult *qres = sql_exec("INSERT INTO slot_config (slot_nr, addr, input_ch_cnt, output_ch_cnt) VALUES ($1, $2, $3, $4)", 0, 4, params);
  if (qres == NULL)
  {
    LOG_DEBUG("[%s] leave with error", __func__);
    return 0;
  }
  PQclear(qres);

  //reread sources list if I/O and DSP cards changes
  db_get_matrix_sources();

  LOG_DEBUG("[%s] leave", __func__);

  return 1;
}

int db_delete_slot_config(unsigned char slot_nr)
{
  char str[1][32];
  const char *params[1];
  int cntParams;

  LOG_DEBUG("[%s] enter", __func__);

  for (cntParams=0; cntParams<1; cntParams++)
  {
    params[cntParams] = (const char *)str[cntParams];
  }

  sprintf(str[0], "%d", slot_nr+1);

  PGresult *qres = sql_exec("DELETE FROM slot_config WHERE slot_nr=$1", 0, 1, params);
  if (qres == NULL)
  {
    LOG_DEBUG("[%s] leave with error", __func__);
    return 0;
  }

  PQclear(qres);

  //reread sources list if I/O and DSP cards changes
  db_get_matrix_sources();

  LOG_DEBUG("[%s] leave", __func__);

  return 1;
}

int db_update_slot_config_input_ch_cnt(unsigned long int addr, unsigned char cnt)
{
  char str[2][32];
  const char *params[2];
  int cntParams;

  LOG_DEBUG("[%s] enter", __func__);

  for (cntParams=0; cntParams<2; cntParams++)
  {
    params[cntParams] = (const char *)str[cntParams];
  }

  sprintf(str[0], "%d", cnt);
  sprintf(str[1], "%ld", addr);

  PGresult *qres = sql_exec("UPDATE slot_config SET input_ch_cnt=$1 WHERE addr=$2", 0, 2, params);
  if (qres == NULL)
  {
    LOG_DEBUG("[%s] leave with error", __func__);
    return 0;
  }
  PQclear(qres);

  LOG_DEBUG("[%s] leave", __func__);

  return 1;
}

int db_update_slot_config_output_ch_cnt(unsigned long int addr, unsigned char cnt)
{
  char str[2][32];
  const char *params[2];
  int cntParams;

  LOG_DEBUG("[%s] enter", __func__);

  for (cntParams=0; cntParams<2; cntParams++)
  {
    params[cntParams] = (const char *)str[cntParams];
  }

  sprintf(str[0], "%d", cnt);
  sprintf(str[1], "%ld", addr);

  PGresult *qres = sql_exec("UPDATE slot_config SET output_ch_cnt=$1 WHERE addr=$2", 0, 2, params);
  if (qres == NULL)
  {
    LOG_DEBUG("[%s] leave with error", __func__);
    return 0;
  }
  PQclear(qres);

  LOG_DEBUG("[%s] leave", __func__);

  return 1;
}

int db_empty_slot_config()
{
  LOG_DEBUG("[%s] enter", __func__);
  PGresult *qres = sql_exec("TRUNCATE slot_config", 0, 0, NULL);
  if (qres == NULL)
  {
    LOG_DEBUG("[%s] leave with error", __func__);
    return 0;
  }
  PQclear(qres);

  LOG_DEBUG("[%s] leave", __func__);

  return 1;
}

void db_processnotifies()
{
  LOG_DEBUG("[%s] enter", __func__);
  PQconsumeInput(sql_conn);
  sql_processnotifies();
  LOG_DEBUG("[%s] leave", __func__);
}

void db_close()
{
  LOG_DEBUG("[%s] enter", __func__);
  sql_close();
  LOG_DEBUG("[%s] leave", __func__);
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
  LOG_DEBUG("[%s] enter", __func__);
  //No implementation
  arg = NULL;
  myself=0;
  LOG_DEBUG("[%s] leave", __func__);
}

void db_event_address_removed(char myself, char *arg)
{
  LOG_DEBUG("[%s] enter", __func__);
  //No implementation
  arg = NULL;
  myself=0;
  LOG_DEBUG("[%s] leave", __func__);
}

void db_event_slot_config_changed(char myself, char *arg)
{
  LOG_DEBUG("[%s] enter", __func__);
  db_read_slot_config();
  arg = NULL;
  myself=0;
  LOG_DEBUG("[%s] leave", __func__);
}

void db_event_src_config_changed(char myself, char *arg)
{
  LOG_DEBUG("[%s] enter", __func__);
  unsigned short int number;

  sscanf(arg, "%hd", &number);
  db_read_src_config(number, number);

  myself=0;
  LOG_DEBUG("[%s] leave", __func__);
}

void db_event_module_config_changed(char myself, char *arg)
{
  LOG_DEBUG("[%s] enter", __func__);
  unsigned char number;

  sscanf(arg, "%hhd", &number);
  db_read_module_config(number, number, 0xFF, 0);

  myself=0;
  LOG_DEBUG("[%s] leave", __func__);
}

void db_event_buss_config_changed(char myself, char *arg)
{
  LOG_DEBUG("[%s] enter", __func__);
  unsigned char number;

  sscanf(arg, "%hhd", &number);
  db_read_buss_config(number, number, 0xFF);

  myself=0;
  LOG_DEBUG("[%s] leave", __func__);
}

void db_event_monitor_buss_config_changed(char myself, char *arg)
{
  LOG_DEBUG("[%s] enter", __func__);
  unsigned char number;

  sscanf(arg, "%hhd", &number);
  db_read_monitor_buss_config(number, number, 0xFF);

  myself=0;
  LOG_DEBUG("[%s] leave", __func__);
}

void db_event_extern_src_config_changed(char myself, char *arg)
{
  LOG_DEBUG("[%s] enter", __func__);
  unsigned char number;

  sscanf(arg, "%hhd", &number);
  db_read_extern_src_config(number, number);

  myself=0;
  LOG_DEBUG("[%s] leave", __func__);
}

void db_event_talkback_config_changed(char myself, char *arg)
{
  LOG_DEBUG("[%s] enter", __func__);
  unsigned char number;

  sscanf(arg, "%hhd", &number);
  db_read_talkback_config(number, number);

  myself=0;
  LOG_DEBUG("[%s] leave", __func__);
}

void db_event_global_config_changed(char myself, char *arg)
{
  LOG_DEBUG("[%s] enter", __func__);
  db_read_global_config();

  arg = NULL;
  myself=0;
  LOG_DEBUG("[%s] leave", __func__);
}

void db_event_dest_config_changed(char myself, char *arg)
{
  LOG_DEBUG("[%s] enter", __func__);
  unsigned short int number;

  sscanf(arg, "%hd", &number);
  db_read_dest_config(number, number);

  myself=0;
  LOG_DEBUG("[%s] leave", __func__);
}

void db_event_node_config_changed(char myself, char *arg)
{
  LOG_DEBUG("[%s] enter", __func__);
  unsigned long int addr;

  sscanf(arg, "%ld", &addr);

  ONLINE_NODE_INFORMATION_STRUCT *node_info = GetOnlineNodeInformation(addr);
  if (node_info == NULL)
  {
    log_write("No node information for address: %08lX", addr);
    LOG_DEBUG("[%s] leave with error", __func__);
    return;
  }
  db_read_node_config(node_info, 1024, node_info->NumberOfCustomObjects+1023);

  myself=0;
  LOG_DEBUG("[%s] leave", __func__);
}

void db_event_defaults_changed(char myself, char *arg)
{
  LOG_DEBUG("[%s] enter", __func__);
  unsigned long int addr;
  unsigned int obj;

  if (sscanf(arg, "%ld %d", &addr, &obj) != 2)
  {
    log_write("defaults_changed notify has to less arguments");
    LOG_DEBUG("[%s] leave with error", __func__);
  }

  ONLINE_NODE_INFORMATION_STRUCT *node_info = GetOnlineNodeInformation(addr);
  if (node_info == NULL)
  {
    log_write("No node information for address: %08lX", addr);
    LOG_DEBUG("[%s] leave with error", __func__);
    return;
  }
  db_read_node_defaults(node_info, obj, obj, 1, 1);

  myself=0;
  LOG_DEBUG("[%s] leave", __func__);
}

void db_event_src_preset_changed(char myself, char *arg)
{
  LOG_DEBUG("[%s] enter", __func__);
  unsigned short int number;

  sscanf(arg, "%hd", &number);
  db_read_src_preset(number, number);

  myself=0;
  LOG_DEBUG("[%s] leave", __func__);
}

void db_event_routing_preset_changed(char myself, char *arg)
{
  LOG_DEBUG("[%s] enter", __func__);
  unsigned short int number;

  sscanf(arg, "%hd", &number);
  db_read_routing_preset(number, number);

  myself=0;
  LOG_DEBUG("[%s] leave", __func__);
}

void db_event_buss_preset_changed(char myself, char *arg)
{
  LOG_DEBUG("[%s] enter", __func__);
  unsigned short int number;

  sscanf(arg, "%hd", &number);
  db_read_buss_preset(number, number);

  myself=0;
  LOG_DEBUG("[%s] leave", __func__);
}

void db_event_buss_preset_rows_changed(char myself, char *arg)
{
  LOG_DEBUG("[%s] enter", __func__);
  unsigned short int number;

  sscanf(arg, "%hd", &number);
  db_read_buss_preset_rows(number, number);

  myself=0;
  LOG_DEBUG("[%s] leave", __func__);
}

void db_event_monitor_buss_preset_rows_changed(char myself, char *arg)
{
  LOG_DEBUG("[%s] enter", __func__);
  unsigned short int number;

  sscanf(arg, "%hd", &number);
  db_read_monitor_buss_preset_rows(number, number);

  myself=0;
  LOG_DEBUG("[%s] leave", __func__);
}

void db_event_console_preset_changed(char myself, char *arg)
{
  LOG_DEBUG("[%s] enter", __func__);
  unsigned short int number;

  sscanf(arg, "%hd", &number);
  db_read_console_preset(number, number);

  myself=0;
  LOG_DEBUG("[%s] leave", __func__);
}

void db_lock(int lock)
{
  LOG_DEBUG("[%s] enter", __func__);
  sql_lock(lock);
  LOG_DEBUG("[%s] leave", __func__);
}
