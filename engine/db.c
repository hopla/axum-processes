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

int insert_engine_function(int type, int function_number, const char *name, int rcv_type, int xmt_type)
{
  char q[1024];
  PGresult *qres;

  sprintf(q, "INSERT INTO functions (func.type, func.seq, func.func, name, rcv_type, xmt_type) VALUES (%d, NULL, %d, '%s' , %d, %d)", type, function_number, name, rcv_type, xmt_type);
  qres = sql_exec(q, 0, 0, NULL);
  if(qres == NULL) 
  {
    return 0;
  }

  PQclear(qres);
  return 1;    
}

int insert_module_functions()
{
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_SOURCE              , "Source"              , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_SOURCE_A              , "Source A"            , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_SOURCE_B              , "Source B"            , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_SOURCE_PHANTOM          , "Source phantom"        , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_SOURCE_PAD            , "Source pad"            , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_SOURCE_GAIN_LEVEL       , "Source gain level"     , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_SOURCE_GAIN_LEVEL_RESET   , "Source gain level reset" , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_INSERT_ON_OFF         , "Insert on/off"         , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_PHASE               , "Phase"             , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_GAIN_LEVEL            , "Gain level"            , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_GAIN_LEVEL            , "Gain level"            , NO_DATA_DATATYPE        , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_GAIN_LEVEL_RESET        , "Gain level reset"        , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_LOW_CUT_FREQUENCY       , "Low cut frequency"     , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_LOW_CUT_ON_OFF          , "Low cut on/off"        , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_1_LEVEL       , "EQ Band 1 level"       , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_1_FREQUENCY     , "EQ Band 1 frequency"     , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_1_BANDWIDTH     , "EQ Band 1 bandwidth"     , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_1_LEVEL_RESET   , "EQ Band 1 level reset"   , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_1_FREQUENCY_RESET , "EQ Band 1 frequency reset" , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_1_BANDWIDTH_RESET , "EQ Band 1 bandwidth reset" , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_1_TYPE          , "EQ Band 1 type"        , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_2_LEVEL       , "EQ Band 2 level"       , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_2_FREQUENCY     , "EQ Band 2 frequency"     , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_2_BANDWIDTH     , "EQ Band 2 bandwidth"     , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_2_LEVEL_RESET   , "EQ Band 2 level reset"   , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_2_FREQUENCY_RESET , "EQ Band 2 frequency reset" , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_2_BANDWIDTH_RESET , "EQ Band 2 bandwidth reset" , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_2_TYPE          , "EQ Band 2 type"        , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_3_LEVEL       , "EQ Band 3 level"       , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_3_FREQUENCY     , "EQ Band 3 frequency"     , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_3_BANDWIDTH     , "EQ Band 3 bandwidth"     , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_3_LEVEL_RESET   , "EQ Band 3 level reset"   , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_3_FREQUENCY_RESET , "EQ Band 3 frequency reset" , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_3_BANDWIDTH_RESET , "EQ Band 3 bandwidth reset" , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_3_TYPE          , "EQ Band 3 type"        , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_4_LEVEL       , "EQ Band 4 level"       , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_4_FREQUENCY     , "EQ Band 4 frequency"     , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_4_BANDWIDTH     , "EQ Band 4 bandwidth"     , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_4_LEVEL_RESET   , "EQ Band 4 level reset"   , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_4_FREQUENCY_RESET , "EQ Band 4 frequency reset" , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_4_BANDWIDTH_RESET , "EQ Band 4 bandwidth reset" , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_4_TYPE          , "EQ Band 4 type"        , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_5_LEVEL       , "EQ Band 5 level"       , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_5_FREQUENCY     , "EQ Band 5 frequency"     , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_5_BANDWIDTH     , "EQ Band 5 bandwidth"     , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_5_LEVEL_RESET   , "EQ Band 5 level reset"   , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_5_FREQUENCY_RESET , "EQ Band 5 frequency reset" , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_5_BANDWIDTH_RESET , "EQ Band 5 bandwidth reset" , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_5_TYPE          , "EQ Band 5 type"        , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_6_LEVEL       , "EQ Band 6 level"       , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_6_FREQUENCY     , "EQ Band 6 frequency"     , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_6_BANDWIDTH     , "EQ Band 6 bandwidth"     , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_6_LEVEL_RESET   , "EQ Band 6 level reset"   , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_6_FREQUENCY_RESET , "EQ Band 6 frequency reset" , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_6_BANDWIDTH_RESET , "EQ Band 6 bandwidth reset" , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_BAND_6_TYPE          , "EQ Band 6 type"        , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_ON_OFF           , "EQ on/off"           , STATE_DATATYPE          , STATE_DATATYPE          );
//insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_TYPE_A           , "EQ type A"           , STATE_DATATYPE          , NO_DATA_DATATYPE        );
//insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_EQ_TYPE_B           , "EQ type B"           , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_DYNAMICS_AMOUNT       , "Dynamics amount"       , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_DYNAMICS_ON_OFF       , "Dynamics on/off"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_MONO                , "Mono"                , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_PAN               , "Pan"               , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_PAN_RESET           , "Pan reset"           , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_MODULE_LEVEL          , "Module level"          , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_MODULE_LEVEL          , "Module level"          , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_MODULE_ON           , "Module on"           , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_MODULE_OFF            , "Module off"            , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_MODULE_ON_OFF         , "Module on/off"         , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_1_2_LEVEL          , "Buss 1/2 level"        , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_1_2_LEVEL          , "Buss 1/2 level"        , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_1_2_LEVEL_RESET      , "Buss 1/2 level reset"    , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_1_2_ON_OFF       , "Buss 1/2 on/off"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_1_2_PRE          , "Buss 1/2 pre"          , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_1_2_BALANCE        , "Buss 1/2 balance"        , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_1_2_BALANCE        , "Buss 1/2 balance"        , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_1_2_BALANCE_RESET    , "Buss 1/2 balance reset"    , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_3_4_LEVEL          , "Buss 3/4 level"        , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_3_4_LEVEL          , "Buss 3/4 level"        , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_3_4_LEVEL_RESET      , "Buss 3/4 level reset"    , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_3_4_ON_OFF       , "Buss 3/4 on/off"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_3_4_PRE          , "Buss 3/4 pre"          , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_3_4_BALANCE        , "Buss 3/4 balance"        , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_3_4_BALANCE        , "Buss 3/4 balance"        , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_3_4_BALANCE_RESET    , "Buss 3/4 balance reset"    , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_5_6_LEVEL          , "Buss 5/6 level"        , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_5_6_LEVEL          , "Buss 5/6 level"        , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_5_6_LEVEL_RESET      , "Buss 5/6 level reset"    , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_5_6_ON_OFF       , "Buss 5/6 on/off"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_5_6_PRE          , "Buss 5/6 pre"          , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_5_6_BALANCE        , "Buss 5/6 balance"        , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_5_6_BALANCE        , "Buss 5/6 balance"        , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_5_6_BALANCE_RESET    , "Buss 5/6 balance reset"    , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_7_8_LEVEL          , "Buss 7/8 level"        , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_7_8_LEVEL          , "Buss 7/8 level"        , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_7_8_LEVEL_RESET      , "Buss 7/8 level reset"    , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_7_8_ON_OFF       , "Buss 7/8 on/off"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_7_8_PRE          , "Buss 7/8 pre"          , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_7_8_BALANCE        , "Buss 7/8 balance"        , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_7_8_BALANCE        , "Buss 7/8 balance"        , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_7_8_BALANCE_RESET    , "Buss 7/8 balance reset"    , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_9_10_LEVEL       , "Buss 9/10 level"       , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_9_10_LEVEL       , "Buss 9/10 level"       , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_9_10_LEVEL_RESET   , "Buss 9/10 level reset"   , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_9_10_ON_OFF        , "Buss 9/10 on/off"        , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_9_10_PRE         , "Buss 9/10 pre"         , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_9_10_BALANCE       , "Buss 9/10 balance"     , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_9_10_BALANCE       , "Buss 9/10 balance"     , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_9_10_BALANCE_RESET   , "Buss 9/10 balance reset" , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_11_12_LEVEL        , "Buss 11/12 level"        , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_11_12_LEVEL        , "Buss 11/12 level"        , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_11_12_LEVEL_RESET    , "Buss 11/12 level reset"    , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_11_12_ON_OFF       , "Buss 11/12 on/off"     , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_11_12_PRE          , "Buss 11/12 pre"        , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_11_12_BALANCE      , "Buss 11/12 balance"      , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_11_12_BALANCE      , "Buss 11/12 balance"      , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_11_12_BALANCE_RESET  , "Buss 11/12 balance reset"  , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_13_14_LEVEL        , "Buss 13/14 level"        , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_13_14_LEVEL        , "Buss 13/14 level"        , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_13_14_LEVEL_RESET    , "Buss 13/14 level reset"    , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_13_14_ON_OFF       , "Buss 13/14 on/off"     , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_13_14_PRE          , "Buss 13/14 pre"        , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_13_14_BALANCE      , "Buss 13/14 balance"      , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_13_14_BALANCE      , "Buss 13/14 balance"      , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_13_14_BALANCE_RESET  , "Buss 13/14 balance reset"  , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_15_16_LEVEL        , "Buss 15/16 level"        , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_15_16_LEVEL        , "Buss 15/16 level"        , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_15_16_LEVEL_RESET    , "Buss 15/16 level reset"    , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_15_16_ON_OFF       , "Buss 15/16 on/off"     , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_15_16_PRE          , "Buss 15/16 pre"        , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_15_16_BALANCE      , "Buss 15/16 balance"      , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_15_16_BALANCE      , "Buss 15/16 balance"      , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_15_16_BALANCE_RESET  , "Buss 15/16 balance reset"  , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_17_18_LEVEL        , "Buss 17/18 level"        , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_17_18_LEVEL        , "Buss 17/18 level"        , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_17_18_LEVEL_RESET    , "Buss 17/18 level reset"    , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_17_18_ON_OFF       , "Buss 17/18 on/off"     , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_17_18_PRE          , "Buss 17/18 pre"        , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_17_18_BALANCE      , "Buss 17/18 balance"      , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_17_18_BALANCE      , "Buss 17/18 balance"      , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_17_18_BALANCE_RESET  , "Buss 17/18 balance reset"  , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_19_20_LEVEL        , "Buss 19/20 level"        , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_19_20_LEVEL        , "Buss 19/20 level"        , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_19_20_LEVEL_RESET    , "Buss 19/20 level reset"    , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_19_20_ON_OFF       , "Buss 19/20 on/off"     , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_19_20_PRE          , "Buss 19/20 pre"        , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_19_20_BALANCE      , "Buss 19/20 balance"      , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_19_20_BALANCE      , "Buss 19/20 balance"      , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_19_20_BALANCE_RESET  , "Buss 19/20 balance reset"  , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_21_22_LEVEL        , "Buss 21/22 level"        , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_21_22_LEVEL        , "Buss 21/22 level"        , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_21_22_LEVEL_RESET    , "Buss 21/22 level reset"    , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_21_22_ON_OFF       , "Buss 21/22 on/off"     , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_21_22_PRE          , "Buss 21/22 pre"        , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_21_22_BALANCE      , "Buss 21/22 balance"      , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_21_22_BALANCE      , "Buss 21/22 balance"      , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_21_22_BALANCE_RESET  , "Buss 21/22 balance reset"  , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_23_24_LEVEL        , "Buss 23/24 level"        , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_23_24_LEVEL        , "Buss 23/24 level"        , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_23_24_LEVEL_RESET    , "Buss 23/24 level reset"    , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_23_24_ON_OFF       , "Buss 23/24 on/off"     , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_23_24_PRE          , "Buss 23/24 pre"        , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_23_24_BALANCE      , "Buss 23/24 balance"      , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_23_24_BALANCE      , "Buss 23/24 balance"      , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_23_24_BALANCE_RESET  , "Buss 23/24 balance reset"  , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_25_26_LEVEL        , "Buss 25/26 level"        , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_25_26_LEVEL        , "Buss 25/26 level"        , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_25_26_LEVEL_RESET    , "Buss 25/26 level reset"    , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_25_26_ON_OFF       , "Buss 25/26 on/off"     , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_25_26_PRE          , "Buss 25/26 pre"        , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_25_26_BALANCE      , "Buss 25/26 balance"      , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_25_26_BALANCE      , "Buss 25/26 balance"      , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_25_26_BALANCE_RESET  , "Buss 25/26 balance reset"  , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_27_28_LEVEL        , "Buss 27/28 level"        , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_27_28_LEVEL        , "Buss 27/28 level"        , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_27_28_LEVEL_RESET    , "Buss 27/28 level reset"    , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_27_28_ON_OFF       , "Buss 27/28 on/off"     , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_27_28_PRE          , "Buss 27/28 pre"        , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_27_28_BALANCE      , "Buss 27/28 balance"      , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_27_28_BALANCE      , "Buss 27/28 balance"      , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_27_28_BALANCE_RESET  , "Buss 27/28 balance reset"  , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_29_30_LEVEL        , "Buss 29/30 level"        , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_29_30_LEVEL        , "Buss 29/30 level"        , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_29_30_LEVEL_RESET    , "Buss 29/30 level reset"    , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_29_30_ON_OFF       , "Buss 29/30 on/off"     , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_29_30_PRE          , "Buss 29/30 pre"        , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_29_30_BALANCE      , "Buss 29/30 balance"      , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_29_30_BALANCE      , "Buss 29/30 balance"      , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_29_30_BALANCE_RESET  , "Buss 29/30 balance reset"  , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_31_32_LEVEL        , "Buss 31/32 level"        , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_31_32_LEVEL        , "Buss 31/32 level"        , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_31_32_LEVEL_RESET    , "Buss 31/32 level reset"    , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_31_32_ON_OFF       , "Buss 31/32 on/off"     , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_31_32_PRE          , "Buss 31/32 pre"        , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_31_32_BALANCE      , "Buss 31/32 balance"      , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_31_32_BALANCE      , "Buss 31/32 balance"      , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_BUSS_31_32_BALANCE_RESET  , "Buss 31/32 balance reset"  , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_SOURCE_START          , "Source start"          , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_SOURCE_STOP           , "Source stop"         , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_SOURCE_START_STOP       , "Source start/stop"     , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_COUGH_ON_OFF          , "Cough on/off"          , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_SOURCE_ALERT          , "Source alert"          , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_CONTROL_1           , "Control 1"           , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_CONTROL_1_LABEL       , "Control 1 label"       , NO_DATA_DATATYPE        , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_CONTROL_1_RESET       , "Control 1 reset"       , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_CONTROL_2           , "Control 2"           , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_CONTROL_2_LABEL       , "Control 2 label"       , NO_DATA_DATATYPE        , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_CONTROL_2_RESET       , "Control 2 reset"       , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_CONTROL_3           , "Control 3"           , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_CONTROL_3_LABEL       , "Control 3 label"       , NO_DATA_DATATYPE        , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_CONTROL_3_RESET       , "Control 3 reset"       , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_CONTROL_4           , "Control 4"           , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_CONTROL_4_LABEL       , "Control 4 label"       , NO_DATA_DATATYPE        , OCTET_STRING_DATATYPE     );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_CONTROL_4_RESET       , "Control 4 reset"       , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_PEAK                , "Peak"                , NO_DATA_DATATYPE        , STATE_DATATYPE          );
  insert_engine_function(MODULE_FUNCTIONS     , MODULE_FUNCTION_SIGNAL              , "Signal"              , NO_DATA_DATATYPE        , STATE_DATATYPE          );
  log_write("Module functions inserted");
  return 1;
}

int insert_buss_functions()
{
  insert_engine_function(BUSS_FUNCTIONS     , BUSS_FUNCTION_MASTER_LEVEL      , "Buss master level"     , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(BUSS_FUNCTIONS     , BUSS_FUNCTION_MASTER_LEVEL      , "Buss master level"     , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(BUSS_FUNCTIONS     , BUSS_FUNCTION_MASTER_LEVEL      , "Buss master level"     , NO_DATA_DATATYPE        , FLOAT_DATATYPE          );
  insert_engine_function(BUSS_FUNCTIONS     , BUSS_FUNCTION_MASTER_LEVEL_RESET  , "Buss master level reset" , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(BUSS_FUNCTIONS     , BUSS_FUNCTION_MASTER_ON_OFF     , "Buss master on/off"      , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(BUSS_FUNCTIONS     , BUSS_FUNCTION_MASTER_PRE        , "Buss master pre"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(BUSS_FUNCTIONS     , BUSS_FUNCTION_LABEL         , "Label"             , NO_DATA_DATATYPE        , OCTET_STRING_DATATYPE     );
  insert_engine_function(BUSS_FUNCTIONS     , BUSS_FUNCTION_AUDIO_LEVEL_LEFT    , "Audio level left"        , NO_DATA_DATATYPE        , FLOAT_DATATYPE          );
  insert_engine_function(BUSS_FUNCTIONS     , BUSS_FUNCTION_AUDIO_LEVEL_RIGHT , "Audio level right"     , NO_DATA_DATATYPE        , FLOAT_DATATYPE          );
  log_write("Buss functions inserted");
  return 1;
}

int insert_monitor_buss_functions()
{
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_BUSS_1_2_ON_OFF , "Buss 1/2 on/off"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_BUSS_3_4_ON_OFF , "Buss 3/4 on/off"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_BUSS_5_6_ON_OFF , "Buss 5/6 on/off"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_BUSS_7_8_ON_OFF , "Buss 7/8 on/off"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_BUSS_9_10_ON_OFF  , "Buss 9/10 on/off"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_BUSS_11_12_ON_OFF , "Buss 11/12 on/off" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_BUSS_13_14_ON_OFF , "Buss 13/14 on/off" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_BUSS_15_16_ON_OFF , "Buss 15/16 on/off" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_BUSS_17_18_ON_OFF , "Buss 17/18 on/off" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_BUSS_19_20_ON_OFF , "Buss 19/20 on/off" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_BUSS_21_22_ON_OFF , "Buss 21/22 on/off" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_BUSS_23_24_ON_OFF , "Buss 23/24 on/off" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_BUSS_25_26_ON_OFF , "Buss 25/26 on/off" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_BUSS_27_28_ON_OFF , "Buss 27/28 on/off" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_BUSS_29_30_ON_OFF , "Buss 29/30 on/off" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_BUSS_31_32_ON_OFF , "Buss 31/32 on/off" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_EXT_1_ON_OFF  , "Ext 1 on/off"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_EXT_2_ON_OFF  , "Ext 2 on/off"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_EXT_3_ON_OFF  , "Ext 3 on/off"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_EXT_4_ON_OFF  , "Ext 4 on/off"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_EXT_5_ON_OFF  , "Ext 5 on/off"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_EXT_6_ON_OFF  , "Ext 6 on/off"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_EXT_7_ON_OFF  , "Ext 7 on/off"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_EXT_8_ON_OFF  , "Ext 8 on/off"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_MUTE        , "Mute"        , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_DIM       , "Dim"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_PHONES_LEVEL  , "Phones level"  , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_PHONES_LEVEL  , "Phones level"  , SIGNED_INTEGER_DATATYPE    , OCTET_STRING_DATATYPE      );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_PHONES_LEVEL  , "Phones level"  , NO_DATA_DATATYPE        , FLOAT_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_MONO        , "Mono"        , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_PHASE       , "Phase"     , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_SPEAKER_LEVEL , "Speaker level" , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_SPEAKER_LEVEL , "Speaker level" , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_SPEAKER_LEVEL , "Speaker level" , NO_DATA_DATATYPE        , FLOAT_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_TALKBACK_1    , "Talkback 1"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_TALKBACK_2    , "Talkback 2"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_TALKBACK_3    , "Talkback 3"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_TALKBACK_4    , "Talkback 4"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_TALKBACK_5    , "Talkback 5"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_TALKBACK_6    , "Talkback 6"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_TALKBACK_7    , "Talkback 7"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_TALKBACK_8    , "Talkback 8"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_TALKBACK_9    , "Talkback 9"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_TALKBACK_10   , "Talkback 10" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_TALKBACK_11   , "Talkback 11" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_TALKBACK_12   , "Talkback 12" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_TALKBACK_13   , "Talkback 13" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_TALKBACK_14   , "Talkback 14" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_TALKBACK_15   , "Talkback 15" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_TALKBACK_16   , "Talkback 16" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_AUDIO_LEVEL_LEFT  , "Audio level left"  , NO_DATA_DATATYPE    , FLOAT_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_AUDIO_LEVEL_RIGHT , "Audio level right", NO_DATA_DATATYPE   , FLOAT_DATATYPE          );
  insert_engine_function(MONITOR_BUSS_FUNCTIONS , MONITOR_BUSS_FUNCTION_LABEL       , "Label"     , NO_DATA_DATATYPE        , OCTET_STRING_DATATYPE     );
  log_write("Monitor buss functions inserted");  
  return 1;
}

int insert_global_functions()
{
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_REDLIGHT_1                  , "Redlight 1"                  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_REDLIGHT_2                  , "Redlight 2"                  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_REDLIGHT_3                  , "Redlight 3"                  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_REDLIGHT_4                  , "Redlight 4"                  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_REDLIGHT_5                  , "Redlight 5"                  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_REDLIGHT_6                  , "Redlight 6"                  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_REDLIGHT_7                  , "Redlight 7"                  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_REDLIGHT_8                  , "Redlight 8"                  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_BUSS_1_2_RESET                , "Buss 1/2 reset"              , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_BUSS_3_4_RESET                , "Buss 3/4 reset"              , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_BUSS_5_6_RESET                , "Buss 5/6 reset"              , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_BUSS_7_8_RESET                , "Buss 7/8 reset"              , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_BUSS_9_10_RESET             , "Buss 9/10 reset"               , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_BUSS_11_12_RESET              , "Buss 11/12 reset"            , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_BUSS_13_14_RESET              , "Buss 13/14 reset"            , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_BUSS_15_16_RESET              , "Buss 15/16 reset"            , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_BUSS_17_18_RESET              , "Buss 17/18 reset"            , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_BUSS_19_20_RESET              , "Buss 19/20 reset"            , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_BUSS_21_22_RESET              , "Buss 21/22 reset"            , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_BUSS_23_24_RESET              , "Buss 23/24 reset"            , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_BUSS_25_26_RESET              , "Buss 25/26 reset"            , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_BUSS_27_28_RESET              , "Buss 27/28 reset"            , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_BUSS_29_30_RESET              , "Buss 29/30 reset"            , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_BUSS_31_32_RESET              , "Buss 31/32 reset"            , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_SOURCE         , "Control 1 mode source"         , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_SOURCE_GAIN        , "Control 1 mode source gain"      , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_GAIN           , "Control 1 mode gain"           , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_PHASE            , "Control 1 mode phase"          , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_LOW_CUT          , "Control 1 mode low cut"          , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_EQ_BAND_1_LEVEL    , "Control 1 mode EQ band 1 level"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_EQ_BAND_1_FREQUENCY  , "Control 1 mode EQ band 1 frequency"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_EQ_BAND_1_BANDWIDTH  , "Control 1 mode EQ band 1 bandwidth"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_EQ_BAND_1_TYPE     , "Control 1 mode EQ band 1 type"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_EQ_BAND_2_LEVEL    , "Control 1 mode EQ band 2 level"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_EQ_BAND_2_FREQUENCY  , "Control 1 mode EQ band 2 frequency"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_EQ_BAND_2_BANDWIDTH  , "Control 1 mode EQ band 2 bandwidth"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_EQ_BAND_2_TYPE     , "Control 1 mode EQ band 2 type"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_EQ_BAND_3_LEVEL    , "Control 1 mode EQ band 3 level"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_EQ_BAND_3_FREQUENCY  , "Control 1 mode EQ band 3 frequency"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_EQ_BAND_3_BANDWIDTH  , "Control 1 mode EQ band 3 bandwidth"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_EQ_BAND_3_TYPE     , "Control 1 mode EQ band 3 type"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_EQ_BAND_4_LEVEL    , "Control 1 mode EQ band 4 level"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_EQ_BAND_4_FREQUENCY  , "Control 1 mode EQ band 4 frequency"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_EQ_BAND_4_BANDWIDTH  , "Control 1 mode EQ band 4 bandwidth"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_EQ_BAND_4_TYPE     , "Control 1 mode EQ band 4 type"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_EQ_BAND_5_LEVEL    , "Control 1 mode EQ band 5 level"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_EQ_BAND_5_FREQUENCY  , "Control 1 mode EQ band 5 frequency"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_EQ_BAND_5_BANDWIDTH  , "Control 1 mode EQ band 5 bandwidth"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_EQ_BAND_5_TYPE     , "Control 1 mode EQ band 5 type"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_EQ_BAND_6_LEVEL    , "Control 1 mode EQ band 6 level"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_EQ_BAND_6_FREQUENCY  , "Control 1 mode EQ band 6 frequency"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_EQ_BAND_6_BANDWIDTH  , "Control 1 mode EQ band 6 bandwidth"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_EQ_BAND_6_TYPE     , "Control 1 mode EQ band 6 type"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_DYNAMICS         , "Control 1 mode dynamics"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_MONO           , "Control 1 mode mono"           , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_PAN            , "Control 1 mode pan"            , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_MODULE_LEVEL     , "Control 1 mode module level"     , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_BUSS_1_2         , "Control 1 mode buss 1/2"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_BUSS_1_2_BALANCE   , "Control 1 mode buss 1/2 balance"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_BUSS_3_4         , "Control 1 mode buss 3/4"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_BUSS_3_4_BALANCE   , "Control 1 mode buss 3/4 balance"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_BUSS_5_6         , "Control 1 mode buss 5/6"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_BUSS_5_6_BALANCE   , "Control 1 mode buss 5/6 balance"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_BUSS_7_8         , "Control 1 mode buss 7/8"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_BUSS_7_8_BALANCE   , "Control 1 mode buss 7/8 balance"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_BUSS_9_10        , "Control 1 mode buss 9/10"        , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_BUSS_9_10_BALANCE    , "Control 1 mode buss 9/10 balance"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_BUSS_11_12       , "Control 1 mode buss 11/12"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_BUSS_11_12_BALANCE , "Control 1 mode buss 11/12 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_BUSS_13_14       , "Control 1 mode buss 13/14"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_BUSS_13_14_BALANCE , "Control 1 mode buss 13/14 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_BUSS_15_16       , "Control 1 mode buss 15/16"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_BUSS_15_16_BALANCE , "Control 1 mode buss 15/16 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_BUSS_17_18       , "Control 1 mode buss 17/18"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_BUSS_17_18_BALANCE , "Control 1 mode buss 17/18 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_BUSS_19_20       , "Control 1 mode buss 19/20"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_BUSS_19_20_BALANCE , "Control 1 mode buss 19/20 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_BUSS_21_22       , "Control 1 mode buss 21/22"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_BUSS_21_22_BALANCE , "Control 1 mode buss 21/22 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_BUSS_23_24       , "Control 1 mode buss 23/24"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_BUSS_23_24_BALANCE , "Control 1 mode buss 23/24 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_BUSS_25_26       , "Control 1 mode buss 25/26"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_BUSS_25_26_BALANCE , "Control 1 mode buss 25/26 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_BUSS_27_28       , "Control 1 mode buss 27/28"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_BUSS_27_28_BALANCE , "Control 1 mode buss 27/28 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_BUSS_29_30       , "Control 1 mode buss 29/30"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_BUSS_29_30_BALANCE , "Control 1 mode buss 29/30 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_BUSS_31_32       , "Control 1 mode buss 31/32"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_1_MODE_BUSS_31_32_BALANCE , "Control 1 mode buss 31/32 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_SOURCE         , "Control 2 mode source"         , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_SOURCE_GAIN        , "Control 2 mode source gain"      , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_GAIN           , "Control 2 mode gain"           , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_PHASE            , "Control 2 mode phase"          , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_LOW_CUT          , "Control 2 mode low cut"          , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_EQ_BAND_1_LEVEL    , "Control 2 mode EQ band 1 level"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_EQ_BAND_1_FREQUENCY  , "Control 2 mode EQ band 1 frequency"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_EQ_BAND_1_BANDWIDTH  , "Control 2 mode EQ band 1 bandwidth"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_EQ_BAND_1_TYPE     , "Control 2 mode EQ band 1 type"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_EQ_BAND_2_LEVEL    , "Control 2 mode EQ band 2 level"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_EQ_BAND_2_FREQUENCY  , "Control 2 mode EQ band 2 frequency"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_EQ_BAND_2_BANDWIDTH  , "Control 2 mode EQ band 2 bandwidth"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_EQ_BAND_2_TYPE     , "Control 2 mode EQ band 2 type"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_EQ_BAND_3_LEVEL    , "Control 2 mode EQ band 3 level"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_EQ_BAND_3_FREQUENCY  , "Control 2 mode EQ band 3 frequency"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_EQ_BAND_3_BANDWIDTH  , "Control 2 mode EQ band 3 bandwidth"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_EQ_BAND_3_TYPE     , "Control 2 mode EQ band 3 type"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_EQ_BAND_4_LEVEL    , "Control 2 mode EQ band 4 level"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_EQ_BAND_4_FREQUENCY  , "Control 2 mode EQ band 4 frequency"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_EQ_BAND_4_BANDWIDTH  , "Control 2 mode EQ band 4 bandwidth"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_EQ_BAND_4_TYPE     , "Control 2 mode EQ band 4 type"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_EQ_BAND_5_LEVEL    , "Control 2 mode EQ band 5 level"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_EQ_BAND_5_FREQUENCY  , "Control 2 mode EQ band 5 frequency"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_EQ_BAND_5_BANDWIDTH  , "Control 2 mode EQ band 5 bandwidth"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_EQ_BAND_5_TYPE     , "Control 2 mode EQ band 5 type"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_EQ_BAND_6_LEVEL    , "Control 2 mode EQ band 6 level"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_EQ_BAND_6_FREQUENCY  , "Control 2 mode EQ band 6 frequency"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_EQ_BAND_6_BANDWIDTH  , "Control 2 mode EQ band 6 bandwidth"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_EQ_BAND_6_TYPE     , "Control 2 mode EQ band 6 type"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_DYNAMICS         , "Control 2 mode dynamics"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_MONO           , "Control 2 mode mono"           , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_PAN            , "Control 2 mode pan"            , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_MODULE_LEVEL     , "Control 2 mode module level"     , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_BUSS_1_2         , "Control 2 mode buss 1/2"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_BUSS_1_2_BALANCE   , "Control 2 mode buss 1/2 balance"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_BUSS_3_4         , "Control 2 mode buss 3/4"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_BUSS_3_4_BALANCE   , "Control 2 mode buss 3/4 balance"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_BUSS_5_6         , "Control 2 mode buss 5/6"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_BUSS_5_6_BALANCE   , "Control 2 mode buss 5/6 balance"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_BUSS_7_8         , "Control 2 mode buss 7/8"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_BUSS_7_8_BALANCE   , "Control 2 mode buss 7/8 balance"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_BUSS_9_10        , "Control 2 mode buss 9/10"        , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_BUSS_9_10_BALANCE    , "Control 2 mode buss 9/10 balance"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_BUSS_11_12       , "Control 2 mode buss 11/12"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_BUSS_11_12_BALANCE , "Control 2 mode buss 11/12 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_BUSS_13_14       , "Control 2 mode buss 13/14"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_BUSS_13_14_BALANCE , "Control 2 mode buss 13/14 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_BUSS_15_16       , "Control 2 mode buss 15/16"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_BUSS_15_16_BALANCE , "Control 2 mode buss 15/16 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_BUSS_17_18       , "Control 2 mode buss 17/18"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_BUSS_17_18_BALANCE , "Control 2 mode buss 17/18 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_BUSS_19_20       , "Control 2 mode buss 19/20"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_BUSS_19_20_BALANCE , "Control 2 mode buss 19/20 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_BUSS_21_22       , "Control 2 mode buss 21/22"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_BUSS_21_22_BALANCE , "Control 2 mode buss 21/22 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_BUSS_23_24       , "Control 2 mode buss 23/24"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_BUSS_23_24_BALANCE , "Control 2 mode buss 23/24 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_BUSS_25_26       , "Control 2 mode buss 25/26"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_BUSS_25_26_BALANCE , "Control 2 mode buss 25/26 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_BUSS_27_28       , "Control 2 mode buss 27/28"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_BUSS_27_28_BALANCE , "Control 2 mode buss 27/28 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_BUSS_29_30       , "Control 2 mode buss 29/30"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_BUSS_29_30_BALANCE , "Control 2 mode buss 29/30 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_BUSS_31_32       , "Control 2 mode buss 31/32"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_2_MODE_BUSS_31_32_BALANCE , "Control 2 mode buss 31/32 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_SOURCE         , "Control 3 mode source"         , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_SOURCE_GAIN        , "Control 3 mode source gain"      , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_GAIN           , "Control 3 mode gain"           , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_PHASE            , "Control 3 mode phase"          , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_LOW_CUT          , "Control 3 mode low cut"          , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_EQ_BAND_1_LEVEL    , "Control 3 mode EQ band 1 level"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_EQ_BAND_1_FREQUENCY  , "Control 3 mode EQ band 1 frequency"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_EQ_BAND_1_BANDWIDTH  , "Control 3 mode EQ band 1 bandwidth"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_EQ_BAND_1_TYPE     , "Control 3 mode EQ band 1 type"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_EQ_BAND_2_LEVEL    , "Control 3 mode EQ band 2 level"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_EQ_BAND_2_FREQUENCY  , "Control 3 mode EQ band 2 frequency"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_EQ_BAND_2_BANDWIDTH  , "Control 3 mode EQ band 2 bandwidth"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_EQ_BAND_2_TYPE     , "Control 3 mode EQ band 2 type"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_EQ_BAND_3_LEVEL    , "Control 3 mode EQ band 3 level"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_EQ_BAND_3_FREQUENCY  , "Control 3 mode EQ band 3 frequency"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_EQ_BAND_3_BANDWIDTH  , "Control 3 mode EQ band 3 bandwidth"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_EQ_BAND_3_TYPE     , "Control 3 mode EQ band 3 type"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_EQ_BAND_4_LEVEL    , "Control 3 mode EQ band 4 level"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_EQ_BAND_4_FREQUENCY  , "Control 3 mode EQ band 4 frequency"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_EQ_BAND_4_BANDWIDTH  , "Control 3 mode EQ band 4 bandwidth"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_EQ_BAND_4_TYPE     , "Control 3 mode EQ band 4 type"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_EQ_BAND_5_LEVEL    , "Control 3 mode EQ band 5 level"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_EQ_BAND_5_FREQUENCY  , "Control 3 mode EQ band 5 frequency"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_EQ_BAND_5_BANDWIDTH  , "Control 3 mode EQ band 5 bandwidth"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_EQ_BAND_5_TYPE     , "Control 3 mode EQ band 5 type"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_EQ_BAND_6_LEVEL    , "Control 3 mode EQ band 6 level"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_EQ_BAND_6_FREQUENCY  , "Control 3 mode EQ band 6 frequency"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_EQ_BAND_6_BANDWIDTH  , "Control 3 mode EQ band 6 bandwidth"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_EQ_BAND_6_TYPE     , "Control 3 mode EQ band 6 type"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_DYNAMICS         , "Control 3 mode dynamics"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_MONO           , "Control 3 mode mono"           , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_PAN            , "Control 3 mode pan"            , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_MODULE_LEVEL     , "Control 3 mode module level"     , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_BUSS_1_2         , "Control 3 mode buss 1/2"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_BUSS_1_2_BALANCE   , "Control 3 mode buss 1/2 balance"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_BUSS_3_4         , "Control 3 mode buss 3/4"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_BUSS_3_4_BALANCE   , "Control 3 mode buss 3/4 balance"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_BUSS_5_6         , "Control 3 mode buss 5/6"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_BUSS_5_6_BALANCE   , "Control 3 mode buss 5/6 balance"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_BUSS_7_8         , "Control 3 mode buss 7/8"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_BUSS_7_8_BALANCE   , "Control 3 mode buss 7/8 balance"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_BUSS_9_10        , "Control 3 mode buss 9/10"        , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_BUSS_9_10_BALANCE    , "Control 3 mode buss 9/10 balance"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_BUSS_11_12       , "Control 3 mode buss 11/12"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_BUSS_11_12_BALANCE , "Control 3 mode buss 11/12 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_BUSS_13_14       , "Control 3 mode buss 13/14"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_BUSS_13_14_BALANCE , "Control 3 mode buss 13/14 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_BUSS_15_16       , "Control 3 mode buss 15/16"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_BUSS_15_16_BALANCE , "Control 3 mode buss 15/16 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_BUSS_17_18       , "Control 3 mode buss 17/18"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_BUSS_17_18_BALANCE , "Control 3 mode buss 17/18 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_BUSS_19_20       , "Control 3 mode buss 19/20"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_BUSS_19_20_BALANCE , "Control 3 mode buss 19/20 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_BUSS_21_22       , "Control 3 mode buss 21/22"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_BUSS_21_22_BALANCE , "Control 3 mode buss 21/22 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_BUSS_23_24       , "Control 3 mode buss 23/24"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_BUSS_23_24_BALANCE , "Control 3 mode buss 23/24 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_BUSS_25_26       , "Control 3 mode buss 25/26"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_BUSS_25_26_BALANCE , "Control 3 mode buss 25/26 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_BUSS_27_28       , "Control 3 mode buss 27/28"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_BUSS_27_28_BALANCE , "Control 3 mode buss 27/28 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_BUSS_29_30       , "Control 3 mode buss 29/30"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_BUSS_29_30_BALANCE , "Control 3 mode buss 29/30 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_BUSS_31_32       , "Control 3 mode buss 31/32"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_3_MODE_BUSS_31_32_BALANCE , "Control 3 mode buss 31/32 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_SOURCE         , "Control 4 mode source"         , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_SOURCE_GAIN        , "Control 4 mode source gain"      , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_GAIN           , "Control 4 mode gain"           , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_PHASE            , "Control 4 mode phase"          , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_LOW_CUT          , "Control 4 mode low cut"          , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_EQ_BAND_1_LEVEL    , "Control 4 mode EQ band 1 level"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_EQ_BAND_1_FREQUENCY  , "Control 4 mode EQ band 1 frequency"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_EQ_BAND_1_BANDWIDTH  , "Control 4 mode EQ band 1 bandwidth"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_EQ_BAND_1_TYPE     , "Control 4 mode EQ band 1 type"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_EQ_BAND_2_LEVEL    , "Control 4 mode EQ band 2 level"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_EQ_BAND_2_FREQUENCY  , "Control 4 mode EQ band 2 frequency"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_EQ_BAND_2_BANDWIDTH  , "Control 4 mode EQ band 2 bandwidth"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_EQ_BAND_2_TYPE     , "Control 4 mode EQ band 2 type"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_EQ_BAND_3_LEVEL    , "Control 4 mode EQ band 3 level"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_EQ_BAND_3_FREQUENCY  , "Control 4 mode EQ band 3 frequency"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_EQ_BAND_3_BANDWIDTH  , "Control 4 mode EQ band 3 bandwidth"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_EQ_BAND_3_TYPE     , "Control 4 mode EQ band 3 type"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_EQ_BAND_4_LEVEL    , "Control 4 mode EQ band 4 level"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_EQ_BAND_4_FREQUENCY  , "Control 4 mode EQ band 4 frequency"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_EQ_BAND_4_BANDWIDTH  , "Control 4 mode EQ band 4 bandwidth"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_EQ_BAND_4_TYPE     , "Control 4 mode EQ band 4 type"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_EQ_BAND_5_LEVEL    , "Control 4 mode EQ band 5 level"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_EQ_BAND_5_FREQUENCY  , "Control 4 mode EQ band 5 frequency"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_EQ_BAND_5_BANDWIDTH  , "Control 4 mode EQ band 5 bandwidth"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_EQ_BAND_5_TYPE     , "Control 4 mode EQ band 5 type"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_EQ_BAND_6_LEVEL    , "Control 4 mode EQ band 6 level"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_EQ_BAND_6_FREQUENCY  , "Control 4 mode EQ band 6 frequency"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_EQ_BAND_6_BANDWIDTH  , "Control 4 mode EQ band 6 bandwidth"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_EQ_BAND_6_TYPE     , "Control 4 mode EQ band 6 type"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_DYNAMICS         , "Control 4 mode dynamics"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_MONO           , "Control 4 mode mono"           , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_PAN            , "Control 4 mode pan"            , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_MODULE_LEVEL     , "Control 4 mode module level"     , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_BUSS_1_2         , "Control 4 mode buss 1/2"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_BUSS_1_2_BALANCE   , "Control 4 mode buss 1/2 balance"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_BUSS_3_4         , "Control 4 mode buss 3/4"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_BUSS_3_4_BALANCE   , "Control 4 mode buss 3/4 balance"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_BUSS_5_6         , "Control 4 mode buss 5/6"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_BUSS_5_6_BALANCE   , "Control 4 mode buss 5/6 balance"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_BUSS_7_8         , "Control 4 mode buss 7/8"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_BUSS_7_8_BALANCE   , "Control 4 mode buss 7/8 balance"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_BUSS_9_10        , "Control 4 mode buss 9/10"        , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_BUSS_9_10_BALANCE    , "Control 4 mode buss 9/10 balance"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_BUSS_11_12       , "Control 4 mode buss 11/12"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_BUSS_11_12_BALANCE , "Control 4 mode buss 11/12 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_BUSS_13_14       , "Control 4 mode buss 13/14"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_BUSS_13_14_BALANCE , "Control 4 mode buss 13/14 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_BUSS_15_16       , "Control 4 mode buss 15/16"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_BUSS_15_16_BALANCE , "Control 4 mode buss 15/16 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_BUSS_17_18       , "Control 4 mode buss 17/18"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_BUSS_17_18_BALANCE , "Control 4 mode buss 17/18 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_BUSS_19_20       , "Control 4 mode buss 19/20"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_BUSS_19_20_BALANCE , "Control 4 mode buss 19/20 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_BUSS_21_22       , "Control 4 mode buss 21/22"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_BUSS_21_22_BALANCE , "Control 4 mode buss 21/22 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_BUSS_23_24       , "Control 4 mode buss 23/24"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_BUSS_23_24_BALANCE , "Control 4 mode buss 23/24 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_BUSS_25_26       , "Control 4 mode buss 25/26"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_BUSS_25_26_BALANCE , "Control 4 mode buss 25/26 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_BUSS_27_28       , "Control 4 mode buss 27/28"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_BUSS_27_28_BALANCE , "Control 4 mode buss 27/28 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_BUSS_29_30       , "Control 4 mode buss 29/30"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_BUSS_29_30_BALANCE , "Control 4 mode buss 29/30 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_BUSS_31_32       , "Control 4 mode buss 31/32"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_CONTROL_4_MODE_BUSS_31_32_BALANCE , "Control 4 mode buss 31/32 balance" , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_1_MODE_BUSS_1_2    , "Master control 1 mode buss 1/2"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_1_MODE_BUSS_3_4    , "Master control 1 mode buss 3/4"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_1_MODE_BUSS_5_6    , "Master control 1 mode buss 5/6"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_1_MODE_BUSS_7_8    , "Master control 1 mode buss 7/8"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_1_MODE_BUSS_9_10   , "Master control 1 mode buss 9/10"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_1_MODE_BUSS_11_12    , "Master control 1 mode buss 11/12"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_1_MODE_BUSS_13_14    , "Master control 1 mode buss 13/14"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_1_MODE_BUSS_15_16    , "Master control 1 mode buss 15/16"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_1_MODE_BUSS_17_18    , "Master control 1 mode buss 17/18"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_1_MODE_BUSS_19_20  , "Master control 1 mode buss 19/20"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_1_MODE_BUSS_21_22    , "Master control 1 mode buss 21/22"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_1_MODE_BUSS_23_24    , "Master control 1 mode buss 23/24"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_1_MODE_BUSS_25_26    , "Master control 1 mode buss 25/26"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_1_MODE_BUSS_27_28    , "Master control 1 mode buss 27/28"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_1_MODE_BUSS_29_30    , "Master control 1 mode buss 29/30"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_1_MODE_BUSS_31_32    , "Master control 1 mode buss 31/32"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_2_MODE_BUSS_1_2    , "Master control 2 mode buss 1/2"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_2_MODE_BUSS_3_4    , "Master control 2 mode buss 3/4"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_2_MODE_BUSS_5_6    , "Master control 2 mode buss 5/6"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_2_MODE_BUSS_7_8    , "Master control 2 mode buss 7/8"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_2_MODE_BUSS_9_10   , "Master control 2 mode buss 9/10"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_2_MODE_BUSS_11_12    , "Master control 2 mode buss 11/12"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_2_MODE_BUSS_13_14    , "Master control 2 mode buss 13/14"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_2_MODE_BUSS_15_16    , "Master control 2 mode buss 15/16"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_2_MODE_BUSS_17_18    , "Master control 2 mode buss 17/18"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_2_MODE_BUSS_19_20  , "Master control 2 mode buss 19/20"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_2_MODE_BUSS_21_22    , "Master control 2 mode buss 21/22"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_2_MODE_BUSS_23_24    , "Master control 2 mode buss 23/24"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_2_MODE_BUSS_25_26    , "Master control 2 mode buss 25/26"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_2_MODE_BUSS_27_28    , "Master control 2 mode buss 27/28"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_2_MODE_BUSS_29_30    , "Master control 2 mode buss 29/30"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_2_MODE_BUSS_31_32    , "Master control 2 mode buss 31/32"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_3_MODE_BUSS_1_2    , "Master control 3 mode buss 1/2"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_3_MODE_BUSS_3_4    , "Master control 3 mode buss 3/4"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_3_MODE_BUSS_5_6    , "Master control 3 mode buss 5/6"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_3_MODE_BUSS_7_8    , "Master control 3 mode buss 7/8"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_3_MODE_BUSS_9_10   , "Master control 3 mode buss 9/10"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_3_MODE_BUSS_11_12    , "Master control 3 mode buss 11/12"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_3_MODE_BUSS_13_14    , "Master control 3 mode buss 13/14"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_3_MODE_BUSS_15_16    , "Master control 3 mode buss 15/16"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_3_MODE_BUSS_17_18    , "Master control 3 mode buss 17/18"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_3_MODE_BUSS_19_20  , "Master control 3 mode buss 19/20"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_3_MODE_BUSS_21_22    , "Master control 3 mode buss 21/22"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_3_MODE_BUSS_23_24    , "Master control 3 mode buss 23/24"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_3_MODE_BUSS_25_26    , "Master control 3 mode buss 25/26"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_3_MODE_BUSS_27_28    , "Master control 3 mode buss 27/28"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_3_MODE_BUSS_29_30    , "Master control 3 mode buss 29/30"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_3_MODE_BUSS_31_32    , "Master control 3 mode buss 31/32"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_4_MODE_BUSS_1_2    , "Master control 4 mode buss 1/2"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_4_MODE_BUSS_3_4    , "Master control 4 mode buss 3/4"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_4_MODE_BUSS_5_6    , "Master control 4 mode buss 5/6"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_4_MODE_BUSS_7_8    , "Master control 4 mode buss 7/8"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_4_MODE_BUSS_9_10   , "Master control 4 mode buss 9/10"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_4_MODE_BUSS_11_12    , "Master control 4 mode buss 11/12"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_4_MODE_BUSS_13_14    , "Master control 4 mode buss 13/14"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_4_MODE_BUSS_15_16    , "Master control 4 mode buss 15/16"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_4_MODE_BUSS_17_18    , "Master control 4 mode buss 17/18"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_4_MODE_BUSS_19_20  , "Master control 4 mode buss 19/20"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_4_MODE_BUSS_21_22    , "Master control 4 mode buss 21/22"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_4_MODE_BUSS_23_24    , "Master control 4 mode buss 23/24"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_4_MODE_BUSS_25_26    , "Master control 4 mode buss 25/26"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_4_MODE_BUSS_27_28    , "Master control 4 mode buss 27/28"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_4_MODE_BUSS_29_30    , "Master control 4 mode buss 29/30"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_4_MODE_BUSS_31_32    , "Master control 4 mode buss 31/32"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_1              , "Master control 1"              , SIGNED_INTEGER_DATATYPE   , BIT_STRING_DATATYPE     );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_1              , "Master control 1"              , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_1_RESET          , "Master control 1 reset"          , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_2              , "Master control 2"              , SIGNED_INTEGER_DATATYPE   , BIT_STRING_DATATYPE     );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_2              , "Master control 2"              , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_2_RESET          , "Master control 2 reset"          , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_3              , "Master control 3"              , SIGNED_INTEGER_DATATYPE   , BIT_STRING_DATATYPE     );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_3              , "Master control 3"              , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_3_RESET          , "Master control 3 reset"          , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_4              , "Master control 4"              , SIGNED_INTEGER_DATATYPE   , BIT_STRING_DATATYPE     );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_4              , "Master control 4"              , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(GLOBAL_FUNCTIONS     , GLOBAL_FUNCTION_MASTER_CONTROL_4_RESET          , "Master control 4 reset"          , STATE_DATATYPE          , NO_DATA_DATATYPE        );
  log_write("Monitor buss functions inserted");  
  return 1;
}

int insert_source_functions()
{
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_ON             , "Module on"             , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_OFF              , "Module off"              , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_ON_OFF           , "Module on/off"           , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_FADER_ON         , "Module fader on"         , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_FADER_OFF          , "Module fader off"          , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_FADER_ON_OFF       , "Module fader on/off"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_FADER_AND_ON_ACTIVE    , "Module fader and on active"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_FADER_AND_ON_INACTIVE  , "Module fader and on inactive"  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_1_2_ON        , "Module buss 1/2 on"        , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_1_2_OFF       , "Module buss 1/2 off"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_1_2_ON_OFF      , "Module buss 1/2 on/off"      , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_3_4_ON        , "Module buss 3/4 on"        , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_3_4_OFF       , "Module buss 3/4 off"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_3_4_ON_OFF      , "Module buss 3/4 on/off"      , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_5_6_ON        , "Module buss 5/6 on"        , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_5_6_OFF       , "Module buss 5/6 off"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_5_6_ON_OFF      , "Module buss 5/6 on/off"      , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_7_8_ON        , "Module buss 7/8 on"        , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_7_8_OFF       , "Module buss 7/8 off"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_7_8_ON_OFF      , "Module buss 7/8 on/off"      , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_9_10_ON       , "Module buss 9/10 on"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_9_10_OFF        , "Module buss 9/10 off"      , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_9_10_ON_OFF     , "Module buss 9/10 on/off"   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_11_12_ON        , "Module buss 11/12 on"      , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_11_12_OFF     , "Module buss 11/12 off"     , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_11_12_ON_OFF    , "Module buss 11/12 on/off"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_13_14_ON        , "Module buss 13/14 on"      , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_13_14_OFF     , "Module buss 13/14 off"     , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_13_14_ON_OFF    , "Module buss 13/14 on/off"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_15_16_ON        , "Module buss 15/16 on"      , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_15_16_OFF     , "Module buss 15/16 off"     , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_15_16_ON_OFF    , "Module buss 15/16 on/off"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_17_18_ON        , "Module buss 17/18 on"      , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_17_18_OFF     , "Module buss 17/18 off"     , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_17_18_ON_OFF    , "Module buss 17/18 on/off"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_19_20_ON        , "Module buss 19/20 on"      , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_19_20_OFF     , "Module buss 19/20 off"     , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_19_20_ON_OFF    , "Module buss 19/20 on/off"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_21_22_ON        , "Module buss 21/22 on"      , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_21_22_OFF     , "Module buss 21/22 off"     , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_21_22_ON_OFF    , "Module buss 21/22 on/off"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_23_24_ON        , "Module buss 23/24 on"      , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_23_24_OFF     , "Module buss 23/24 off"     , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_23_24_ON_OFF    , "Module buss 23/24 on/off"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_25_26_ON        , "Module buss 25/26 on"      , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_25_26_OFF     , "Module buss 25/26 off"     , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_25_26_ON_OFF    , "Module buss 25/26 on/off"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_27_28_ON        , "Module buss 27/28 on"      , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_27_28_OFF     , "Module buss 27/28 off"     , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_27_28_ON_OFF    , "Module buss 27/28 on/off"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_29_30_ON        , "Module buss 29/30 on"      , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_29_30_OFF     , "Module buss 29/30 off"     , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_29_30_ON_OFF    , "Module buss 29/30 on/off"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_31_32_ON        , "Module buss 31/32 on"      , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_31_32_OFF     , "Module buss 31/32 off"     , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_BUSS_31_32_ON_OFF    , "Module buss 31/32 on/off"    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_MODULE_COUGH_ON_OFF       , "Module cough on/off"       , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_START                 , "Start"               , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_STOP                  , "Stop"                  , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_START_STOP              , "Start/stop"              , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_PHANTOM               , "Phantom"               , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_PAD                 , "Pad"                 , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_GAIN                  , "Input gain"              , SIGNED_INTEGER_DATATYPE   , FLOAT_DATATYPE          );
  insert_engine_function(SOURCE_FUNCTIONS , SOURCE_FUNCTION_ALERT                 , "Alert"               , STATE_DATATYPE          , STATE_DATATYPE          );
  log_write("Source functions inserted");  
  return 1;
}

int insert_destination_functions()
{
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_LABEL                    , "Label"                 , NO_DATA_DATATYPE        , OCTET_STRING_DATATYPE     );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_SOURCE                   , "Source"                  , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_MONITOR_SPEAKER_LEVEL          , "Monitor speaker level"       , NO_DATA_DATATYPE        , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_MONITOR_SPEAKER_LEVEL          , "Monitor speaker level"       , NO_DATA_DATATYPE        , OCTET_STRING_DATATYPE     );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_MONITOR_SPEAKER_LEVEL          , "Monitor speaker level"       , NO_DATA_DATATYPE        , FLOAT_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_MONITOR_PHONES_LEVEL         , "Monitor phones level"        , NO_DATA_DATATYPE        , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_MONITOR_PHONES_LEVEL         , "Monitor phones level"        , NO_DATA_DATATYPE        , OCTET_STRING_DATATYPE     );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_MONITOR_PHONES_LEVEL         , "Monitor phones level"        , NO_DATA_DATATYPE        , FLOAT_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_LEVEL                    , "Level"                 , UNSIGNED_INTEGER_DATATYPE , UNSIGNED_INTEGER_DATATYPE );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_LEVEL                    , "Level"                 , SIGNED_INTEGER_DATATYPE   , OCTET_STRING_DATATYPE     );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_LEVEL                    , "Level"                 , NO_DATA_DATATYPE        , FLOAT_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_MUTE                   , "Mute"                    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_MUTE_AND_MONITOR_MUTE          , "Mute & Monitor mute"         , NO_DATA_DATATYPE        , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_DIM                      , "Dim"                   , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_DIM_AND_MONITOR_DIM          , "Dim & Monitor dim"         , NO_DATA_DATATYPE        , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_MONO                   , "Mono"                    , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_MONO_AND_MONITOR_MONO          , "Mono & Monitor mono"         , NO_DATA_DATATYPE        , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_PHASE                    , "Phase"                 , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_PHASE_AND_MONITOR_PHASE        , "Phase & Monitor phase"       , NO_DATA_DATATYPE        , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_TALKBACK_1               , "Talkback 1"                , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_TALKBACK_1_AND_MONITOR_TALKBACK_1  , "Talkback 1 & Monitor talkback 1" , NO_DATA_DATATYPE        , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_TALKBACK_2               , "Talkback 2"                , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_TALKBACK_2_AND_MONITOR_TALKBACK_2  , "Talkback 2 & Monitor talkback 2" , NO_DATA_DATATYPE        , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_TALKBACK_3               , "Talkback 3"                , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_TALKBACK_3_AND_MONITOR_TALKBACK_3  , "Talkback 3 & Monitor talkback 3" , NO_DATA_DATATYPE        , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_TALKBACK_4               , "Talkback 4"                , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_TALKBACK_4_AND_MONITOR_TALKBACK_4  , "Talkback 4 & Monitor talkback 4" , NO_DATA_DATATYPE        , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_TALKBACK_5               , "Talkback 5"                , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_TALKBACK_5_AND_MONITOR_TALKBACK_5  , "Talkback 5 & Monitor talkback 5" , NO_DATA_DATATYPE        , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_TALKBACK_6               , "Talkback 6"                , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_TALKBACK_6_AND_MONITOR_TALKBACK_6  , "Talkback 6 & Monitor talkback 6" , NO_DATA_DATATYPE        , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_TALKBACK_7               , "Talkback 7"                , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_TALKBACK_7_AND_MONITOR_TALKBACK_7  , "Talkback 7 & Monitor talkback 7" , NO_DATA_DATATYPE        , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_TALKBACK_8               , "Talkback 8"                , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_TALKBACK_8_AND_MONITOR_TALKBACK_8  , "Talkback 8 & Monitor talkback 8" , NO_DATA_DATATYPE        , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_TALKBACK_9               , "Talkback 9"                , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_TALKBACK_9_AND_MONITOR_TALKBACK_9  , "Talkback 9 & Monitor talkback 9" , NO_DATA_DATATYPE        , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_TALKBACK_10                , "Talkback 10"               , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_TALKBACK_10_AND_MONITOR_TALKBACK_10, "Talkback 10 & Monitor talkback 10" , NO_DATA_DATATYPE        , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_TALKBACK_11                , "Talkback 11"               , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_TALKBACK_11_AND_MONITOR_TALKBACK_11, "Talkback 11 & Monitor talkback 11" , NO_DATA_DATATYPE        , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_TALKBACK_12                , "Talkback 12"               , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_TALKBACK_12_AND_MONITOR_TALKBACK_12, "Talkback 12 & Monitor talkback 12" , NO_DATA_DATATYPE        , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_TALKBACK_13                , "Talkback 13"               , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_TALKBACK_13_AND_MONITOR_TALKBACK_13, "Talkback 13 & Monitor talkback 13" , NO_DATA_DATATYPE        , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_TALKBACK_14                , "Talkback 14"               , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_TALKBACK_14_AND_MONITOR_TALKBACK_14, "Talkback 14 & Monitor talkback 14" , NO_DATA_DATATYPE        , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_TALKBACK_15                , "Talkback 15"               , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_TALKBACK_15_AND_MONITOR_TALKBACK_15, "Talkback 15 & Monitor talkback 15" , NO_DATA_DATATYPE        , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_TALKBACK_16                , "Talkback 16"               , STATE_DATATYPE          , STATE_DATATYPE          );
  insert_engine_function(DESTINATION_FUNCTIONS  , DESTINATION_FUNCTION_TALKBACK_16_AND_MONITOR_TALKBACK_16, "Talkback 16 & Monitor talkback 16" , NO_DATA_DATATYPE        , STATE_DATATYPE          );
  log_write("Destination functions inserted");
  return 1;
}

int db_check_engine_functions()
{
  int function_count;
  PGresult *qres = sql_exec("SELECT COUNT(*) FROM functions", 1, 0, NULL);
  if (qres == NULL)
  {
    return 0;
  }
  sscanf(PQgetvalue(qres, 0,0), "%d", &function_count);
  if (function_count == 0)
  {
    log_write("Insert engine functions into database");
    insert_module_functions();
    insert_buss_functions();
    insert_monitor_buss_functions();
    insert_global_functions();
    insert_source_functions();
    insert_destination_functions();
  }
  PQclear(qres);
  return 1;
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
    //short int input_ch_count, output_ch_count;

    sscanf(PQgetvalue(qres, cntRow, 0), "%hd", &slot_nr);
    sscanf(PQgetvalue(qres, cntRow, 1), "%ld", &addr);
    //sscanf(PQgetvalue(qres, cntRow, 2), "%hd", &input_ch_count);
    //sscanf(PQgetvalue(qres, cntRow, 4), "%hd", &output_ch_count);

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

    sscanf(PQgetvalue(qres, cntRow, 0), "%hd", &number);

    AXUM_SOURCE_DATA_STRUCT *SourceData = &AxumData.SourceData[number-1];
    
    sscanf(PQgetvalue(qres, cntRow, 1), "%s", SourceData->SourceName);
    sscanf(PQgetvalue(qres, cntRow, 2), "%d", &SourceData->InputData[0].MambaNetAddress); 
    sscanf(PQgetvalue(qres, cntRow, 3), "%c", &SourceData->InputData[0].SubChannel); 
    sscanf(PQgetvalue(qres, cntRow, 4), "%d", &SourceData->InputData[1].MambaNetAddress); 
    sscanf(PQgetvalue(qres, cntRow, 5), "%c", &SourceData->InputData[1].SubChannel); 
    sscanf(PQgetvalue(qres, cntRow, 6), "%c", &SourceData->Phantom); 
    sscanf(PQgetvalue(qres, cntRow, 7), "%c", &SourceData->Pad); 
    sscanf(PQgetvalue(qres, cntRow, 8), "%f", &SourceData->Gain); 
    sscanf(PQgetvalue(qres, cntRow, 9), "%c", &SourceData->Redlight[0]); 
    sscanf(PQgetvalue(qres, cntRow, 10), "%c", &SourceData->Redlight[1]); 
    sscanf(PQgetvalue(qres, cntRow, 11), "%c", &SourceData->Redlight[2]); 
    sscanf(PQgetvalue(qres, cntRow, 12), "%c", &SourceData->Redlight[3]); 
    sscanf(PQgetvalue(qres, cntRow, 13), "%c", &SourceData->Redlight[4]); 
    sscanf(PQgetvalue(qres, cntRow, 14), "%c", &SourceData->Redlight[5]); 
    sscanf(PQgetvalue(qres, cntRow, 15), "%c", &SourceData->Redlight[6]); 
    sscanf(PQgetvalue(qres, cntRow, 16), "%c", &SourceData->Redlight[7]); 
    sscanf(PQgetvalue(qres, cntRow, 17), "%c", &SourceData->MonitorMute[0]);
    sscanf(PQgetvalue(qres, cntRow, 18), "%c", &SourceData->MonitorMute[1]);
    sscanf(PQgetvalue(qres, cntRow, 19), "%c", &SourceData->MonitorMute[2]);
    sscanf(PQgetvalue(qres, cntRow, 20), "%c", &SourceData->MonitorMute[3]);
    sscanf(PQgetvalue(qres, cntRow, 2l), "%c", &SourceData->MonitorMute[4]);
    sscanf(PQgetvalue(qres, cntRow, 22), "%c", &SourceData->MonitorMute[5]);
    sscanf(PQgetvalue(qres, cntRow, 23), "%c", &SourceData->MonitorMute[6]);
    sscanf(PQgetvalue(qres, cntRow, 24), "%c", &SourceData->MonitorMute[7]);
    sscanf(PQgetvalue(qres, cntRow, 25), "%c", &SourceData->MonitorMute[8]);
    sscanf(PQgetvalue(qres, cntRow, 26), "%c", &SourceData->MonitorMute[9]);
    sscanf(PQgetvalue(qres, cntRow, 27), "%c", &SourceData->MonitorMute[10]);
    sscanf(PQgetvalue(qres, cntRow, 28), "%c", &SourceData->MonitorMute[11]);
    sscanf(PQgetvalue(qres, cntRow, 29), "%c", &SourceData->MonitorMute[12]);
    sscanf(PQgetvalue(qres, cntRow, 30), "%c", &SourceData->MonitorMute[13]);
    sscanf(PQgetvalue(qres, cntRow, 31), "%c", &SourceData->MonitorMute[14]);
    sscanf(PQgetvalue(qres, cntRow, 32), "%c", &SourceData->MonitorMute[15]);

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
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%c", &ModuleData->InsertOnOffA);
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%c", &ModuleData->InsertOnOffB);
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &ModuleData->Gain);
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &ModuleData->Filter.Frequency);
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%c", &ModuleData->FilterOnOffA);
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%c", &ModuleData->FilterOnOffB);
    for (cntEQ=0; cntEQ<6; cntEQ++)
    {
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &ModuleData->EQBand[cntEQ].Range);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &ModuleData->EQBand[cntEQ].Frequency);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &ModuleData->EQBand[cntEQ].Bandwidth);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &ModuleData->EQBand[cntEQ].Slope);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%c", (char *)&ModuleData->EQBand[cntEQ].Type);
    }
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%c", &ModuleData->EQOnOffA);
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%c", &ModuleData->EQOnOffB);
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%c", &ModuleData->Dynamics);
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%c", &ModuleData->DynamicsOnOffA);
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%c", &ModuleData->DynamicsOnOffB);
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &ModuleData->FaderLevel);
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%c", &ModuleData->On);
    for (cntBuss=0; cntBuss<16; cntBuss++)
    {
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%f", &ModuleData->Buss[cntBuss].Level);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%c", &ModuleData->Buss[cntBuss].On);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%c", &ModuleData->Buss[cntBuss].PreModuleLevel);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%d", &ModuleData->Buss[cntBuss].Balance);
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%c", &ModuleData->Buss[cntBuss].Active);
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
    sscanf(PQgetvalue(qres, cntRow, 0), "%hd", &number);

    AXUM_BUSS_MASTER_DATA_STRUCT *BussMasterData = &AxumData.BussMasterData[number-1];
    
    sscanf(PQgetvalue(qres, cntRow, 1), "%s", BussMasterData->Label);
    sscanf(PQgetvalue(qres, cntRow, 2), "%c", &BussMasterData->PreModuleOn); 
    sscanf(PQgetvalue(qres, cntRow, 3), "%c", &BussMasterData->PreModuleLevel); 
    sscanf(PQgetvalue(qres, cntRow, 3), "%c", &BussMasterData->PreModuleBalance); 
    sscanf(PQgetvalue(qres, cntRow, 3), "%f", &BussMasterData->Level); 
    sscanf(PQgetvalue(qres, cntRow, 3), "%c", &BussMasterData->On); 
    sscanf(PQgetvalue(qres, cntRow, 3), "%c", &BussMasterData->Interlock); 
    sscanf(PQgetvalue(qres, cntRow, 3), "%c", &BussMasterData->GlobalBussReset); 

    unsigned int FunctionNrToSent = 0x01000000 | (((number-1)<<12)&0xFFF000);
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

    for (int cntDestination=0; cntDestination<=1280; cntDestination++)
    {
      if (AxumData.DestinationData[cntDestination].Source == number)
      {
        FunctionNrToSent = 0x06000000 | (cntDestination<<12);
        CheckObjectsToSent(FunctionNrToSent | DESTINATION_FUNCTION_SOURCE);
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

  PGresult *qres = sql_exec("SELECT number, label, interlock, default_selection, buss_1_2, buss_3_4, buss_5_6, buss_7_8, buss_9_10, buss_11_12, buss_13_14, buss_15_!6, buss_17_18, buss_19_20, buss_21_22, buss_23_24, buss_25_26, buss_27_28, buss_29_30, buss_31_32, dim_level FROM monitor_buss_config WHERE number>=$1 AND number<=$2", 1, 2, params);
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
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%c", &MonitorData->Interlock); 
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%c", &MonitorData->DefaultSelection); 
    for (cntMonitorBuss=0; cntMonitorBuss<16; cntMonitorBuss++)
    { 
      sscanf(PQgetvalue(qres, cntRow, cntField++), "%c", &MonitorData->AutoSwitchingBuss[cntMonitorBuss]); 
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
    sscanf(PQgetvalue(qres, cntRow, cntField++), "%c", &AxumData.ExternClock);
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
    sscanf(PQgetvalue(qres, cntRow, 0), "%hd", &number);

    AXUM_DESTINATION_DATA_STRUCT *DestinationData = &AxumData.DestinationData[number-1];
    
    sscanf(PQgetvalue(qres, cntRow, 1), "%s", DestinationData->DestinationName);
    sscanf(PQgetvalue(qres, cntRow, 2), "%d", &DestinationData->OutputData[0].MambaNetAddress); 
    sscanf(PQgetvalue(qres, cntRow, 3), "%c", &DestinationData->OutputData[0].SubChannel); 
    sscanf(PQgetvalue(qres, cntRow, 4), "%d", &DestinationData->OutputData[1].MambaNetAddress); 
    sscanf(PQgetvalue(qres, cntRow, 5), "%c", &DestinationData->OutputData[1].SubChannel); 
    sscanf(PQgetvalue(qres, cntRow, 6), "%f", &DestinationData->Level); 
    sscanf(PQgetvalue(qres, cntRow, 7), "%d", &DestinationData->Source); 
    sscanf(PQgetvalue(qres, cntRow, 8), "%d", &DestinationData->MixMinusSource); 

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
  sprintf(str[2], "%c", node_info->FirmwareMajorRevision);
 
  PGresult *qres = sql_exec("SELECT COUNT(*) FROM templates WHERE man_id=$1, prod_id=$2, firm_major=$3", 0, 3, params);
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
  qres = sql_exec("SELECT number, desc, services, sensor_type, sensor_size, sensor_min, sensor_max, actuator_type, actuator_size, actuator_min, actuatar_max, actuator_def FROM templates WHERE man_id=$1, prod_id=$2, firm_major=$3", 0, 3, params);
  if (qres == NULL)
  {
    return 0;
  }
  for (cntRow=0; cntRow<PQntuples(qres); cntRow++)
  {
    unsigned short int ObjectNr;
    sscanf(PQgetvalue(qres, cntRow, 0), "%hd", &ObjectNr);
    if (ObjectNr >= 1024)
    {
      OBJECT_INFORMATION_STRUCT *obj_info = &node_info->ObjectInformation[ObjectNr-1024];

      sscanf(PQgetvalue(qres, cntRow, 1), "%s", &obj_info->Description[0]);
      sscanf(PQgetvalue(qres, cntRow, 2), "%c", &obj_info->Services);
      sscanf(PQgetvalue(qres, cntRow, 3), "%c", &obj_info->SensorDataType);
      sscanf(PQgetvalue(qres, cntRow, 4), "%c", &obj_info->SensorDataSize);
      sscanf(PQgetvalue(qres, cntRow, 5), "%f", &obj_info->SensorDataMinimal);
      sscanf(PQgetvalue(qres, cntRow, 6), "%f", &obj_info->SensorDataMaximal);
      sscanf(PQgetvalue(qres, cntRow, 7), "%c", &obj_info->ActuatorDataType);
      sscanf(PQgetvalue(qres, cntRow, 8), "%c", &obj_info->ActuatorDataSize);
      sscanf(PQgetvalue(qres, cntRow, 9), "%f", &obj_info->ActuatorDataMinimal);
      sscanf(PQgetvalue(qres, cntRow, 10), "%f", &obj_info->ActuatorDataMaximal);
      sscanf(PQgetvalue(qres, cntRow, 11), "%f", &obj_info->ActuatorDataDefault);
      
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
 
  PGresult *qres = sql_exec("SELECT object, data FROM defaults WHERE addr=$1 AND object>=$2 AND object<=$3", 0, 1, params);
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
 
  PGresult *qres = sql_exec("SELECT object, func FROM defaults WHERE addr=$1 AND object>=$2 AND object<=$3", 0, 1, params);
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
    sscanf(PQgetvalue(qres, cntRow, 0), "(%c,%hd,%hd)", &type, &seq_nr, &func_nr);
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

  PGresult *qres = sql_exec("UPDATE rack_organization SET InputChannelCount=$1, WHERE MambaNetAddress=$2", 1, 2, params);
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

  PGresult *qres = sql_exec("UPDATE rack_organization SET OutputChannelCount=$1, WHERE MambaNetAddress=$2", 1, 2, params);
  if (qres == NULL)
  {
    return 0;
  }
  PQclear(qres);
  return 1; 
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
