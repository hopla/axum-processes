/*****************************************************
 *                                                   *
 * The dsp functions uses an internal mutex locking. *
 * This is the reason its not allowed to use these   *
 * functions in a 'signal handler'.                  *
 * You also may not use the dsp_lock functions/mutex *
 * outside these dsp_functions                       *
 *                                                   *
 *****************************************************/

#ifndef _dsp_h
#define _dsp_h


//**************************************************************/
//DSP Data definitions
//**************************************************************/
typedef struct
{
  bool On;
  float Level;
  unsigned int Frequency;
  float Bandwidth;
  float Slope;
  FilterType Type;
} DSPCARD_EQ_BAND_DATA_STRUCT;

typedef struct
{
  int Percent;
  float Threshold;
  bool On;
  float DownwardExpanderThreshold;
} DSPCARD_DYNAMICS_DATA_STRUCT;

typedef struct
{
  float Level;
  bool On;
} DSPCARD_BUSS_DATA_STRUCT;

typedef struct
{
  int Source;
  float Gain;
  bool PhaseReverse;
  bool Insert;
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
  bool On;
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

typedef struct
{
  volatile unsigned long *HPIC;
  volatile unsigned long *HPIA;
  volatile unsigned long *HPID_Inc;
  volatile unsigned long *HPID;
} DSP_REGS_STRUCT;

typedef struct
{
  int slot;
  DSP_REGS_STRUCT dsp_regs[4];
  DSPCARD_DATA_STRUCT data;
} DSPCARD_STRUCT;

typedef struct
{
  DSPCARD_STRUCT dspcard[4];
} DSP_HANDLER_STRUCT;

DSP_HANDLER_STRUCT *dsp_open();
void dsp_close(DSP_HANDLER_STRUCT *dsp_handler);

int dsp_force_eeprom_prg(char *devname);

int dsp_card_available(DSP_HANDLER_STRUCT *dsp_handler, unsigned char CardNr);
void dsp_set_eq(DSP_HANDLER_STRUCT *dsp_handler, unsigned int SystemChannelNr, unsigned char BandNr);
void dsp_set_ch(DSP_HANDLER_STRUCT *dsp_handler, unsigned int SystemChannelNr);
void dsp_set_buss_lvl(DSP_HANDLER_STRUCT *dsp_handler, unsigned int SystemChannelNr);
void dsp_set_mixmin(DSP_HANDLER_STRUCT *dsp_handler, unsigned int SystemChannelNr);
void dsp_set_buss_mstr_lvl(DSP_HANDLER_STRUCT *dsp_handler);
void dsp_set_interpolation(DSP_HANDLER_STRUCT *dsp_handler, int Samplerate);
void dsp_set_monitor_buss(DSP_HANDLER_STRUCT *dsp_handler, unsigned int MonitorChannelNr);
void dsp_read_buss_meters(DSP_HANDLER_STRUCT *dsp_handler, float *SummingdBLevel);
void dsp_read_module_meters(DSP_HANDLER_STRUCT *dsp_handler, float *dBLevel);

//debug function
float dsp_read_float(DSP_HANDLER_STRUCT *dsp_handler, unsigned char CardNr, unsigned char DSPNr, unsigned int Address);

#endif
