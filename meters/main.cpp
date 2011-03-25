/****************************************************************************
**
** Copyright (C) 2004-2006 Trolltech ASA. All rights reserved.
**
** This file is part of the demonstration applications of the Qt Toolkit.
**
** This file may be used under the terms of the GNU General Public
** License version 2.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of
** this file.  Please review the following information to ensure GNU
** General Public Licensing requirements will be met:
** http://www.trolltech.com/products/qt/opensource.html
**
** If you are unsure which license is appropriate for your use, please
** review the following information:
** http://www.trolltech.com/products/qt/licensing.html or contact the
** sales department at sales@trolltech.com.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

#include <unistd.h>/* for close */
#include <fcntl.h>/* for open */
#include <sys/ioctl.h>/* for ioctl */
#include <sys/mman.h>/* for mmap */
#include <stdio.h>

#include <QtCore>
#include <QtGui>
#include <QtSql>

#include <browser.h>
#include <errno.h>

#define MBN_VARARG
#include "mbn.h"
#include "common.h"

#define PCB_MAJOR_VERSION        1
#define PCB_MINOR_VERSION        0

#define FIRMWARE_MAJOR_VERSION   8
#define FIRMWARE_MINOR_VERSION   0

#define MANUFACTURER_ID          0x0001	//D&R
#define PRODUCT_ID               0x001A	//Axum linux meters

#define NR_OF_STATIC_OBJECTS    ((1094+64)-1023)
#define NR_OF_OBJECTS            NR_OF_STATIC_OBJECTS

#define DEFAULT_UNIX_HWPARENT_PATH "/tmp/hwparent.socket"
#define DEFAULT_UNIX_MAMBANET_PATH "/tmp/axum-gateway.socket"
#define DEFAULT_ETH_DEV            "eth0"
#define DEFAULT_LOG_FILE           "/var/log/axum-meters.log"

#ifndef UNIX_PATH_MAX
# define UNIX_PATH_MAX 108
#endif

Browser *browser = NULL;
QMutex qt_mutex(QMutex::Recursive);

struct mbn_interface *itf;
struct mbn_handler *mbn;
char error[MBN_ERRSIZE];
char use_eth = 0;

struct mbn_node_info this_node = {
  0, 0,                   //MambaNet address, Services
  "Axum PPM Meters (Linux)",
  "Axum-PPM-Meters",
  MANUFACTURER_ID, PRODUCT_ID, 0x0001,
  0, 0,                   //Hw revision
  8, 0,                   //Fw revision
  0, 0,                   //FPGA revision
  NR_OF_OBJECTS,          //Number of objects
  0,                      //Default engine address
  {0x0000,0x0000,0x000},  //Hardware parent
  0                       //Service request
};

void init(int argc, char *argv[]);
int SetActuatorData(struct mbn_handler *mbn, unsigned short object, union mbn_data in);
int delay_us(double sleep_time);

void AddressTableChange(struct mbn_handler *m, struct mbn_address_node *old_addr, struct mbn_address_node *new_addr) {
  char EngineStatus = -1;
  if ((old_addr == NULL) && (new_addr != NULL))
  {
    if (new_addr->Services & MBN_ADDR_SERVICES_ENGINE)
    {
      if (m->node.DefaultEngineAddr == new_addr->MambaNetAddr)
      {
        log_write("Our engine active (0x%08X)", new_addr->MambaNetAddr);
      }
      else
      {
        log_write("An engine active (0x%08X)", new_addr->MambaNetAddr);
      }
      EngineStatus = 1;
    }
  }
  if ((old_addr != NULL) && (new_addr == NULL))
  {
    if (old_addr->Services & MBN_ADDR_SERVICES_ENGINE)
    {
      if (m->node.DefaultEngineAddr == old_addr->MambaNetAddr)
      {
        log_write("Our engine inactive (0x%08X)", old_addr->MambaNetAddr);
      }
      else
      {
        log_write("An engine inactive (0x%08X)", old_addr->MambaNetAddr);
      }
      EngineStatus = 0;
    }
  }

  if (EngineStatus != -1)
  {
    qt_mutex.lock();
    browser->EngineStatus = EngineStatus;
    qt_mutex.unlock();
  }
}

void init(int argc, char *argv[])
{
  char ethdev[50];
  struct mbn_object objects[NR_OF_STATIC_OBJECTS];
  int c;
  char oem_name[32];
  int cntObject;
  char obj_desc[32];
  int cntBand;
  char cmdline[1024];
  char socket_path[UNIX_PATH_MAX];

  strcpy(ethdev, DEFAULT_ETH_DEV);
  strcpy(log_file, DEFAULT_LOG_FILE);
  strcpy(hwparent_path, DEFAULT_UNIX_HWPARENT_PATH);
  strcpy(socket_path, DEFAULT_UNIX_MAMBANET_PATH);

  while((c =getopt(argc, argv, "e:g:l:i:")) != -1)
  {
    switch(c)
    {
      case 'e':
      {
        if(strlen(optarg) > 50)
        {
          fprintf(stderr, "Too long device name.\n");
          exit(1);
        }
        strcpy(ethdev, optarg);
        use_eth = 1;
      }
      break;
      case 'g':
      {
        strcpy(hwparent_path, optarg);
      }
      break;
      case 'l':
      {
        strcpy(log_file, optarg);
      }
      break;
      case 'i':
      {
        if(sscanf(optarg, "%hd", &(this_node.UniqueIDPerProduct)) != 1)
        {
          fprintf(stderr, "Invalid UniqueIDPerProduct");
          exit(1);
        }
      }
      break;
      default:
      {
        fprintf(stderr, "Usage: %s [-e dev] [-g path] [-l path] [-i id]\n", argv[0]);
        fprintf(stderr, "  -e dev   Ethernet device for MambaNet communication.\n");
        fprintf(stderr, "  -g path  Hardware parent or path to gateway socket.\n");
        fprintf(stderr, "  -l path  Path to log file.\n");
        fprintf(stderr, "  -i id    UniqueIDPerProduct for the MambaNet node\n");
        exit(1);
      }
      break;
    }
  }

  cntObject = 0;
  objects[cntObject++] = MBN_OBJ( (char *)"Meter 1 Left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Meter 1 Right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Phase Meter 1",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, 0.0, 2.0, 0.0, 0.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Meter 1 Label A",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_OCTETS, 8, 0, 127, 20, "Mon. 1");
  objects[cntObject++] = MBN_OBJ( (char *)"Meter 1 Label B",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_OCTETS, 8, 0, 127, 20, "-");
  objects[cntObject++] = MBN_OBJ( (char *)"Meter 2 Left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Meter 2 Right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Phase Meter 2",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, 0.0, 2.0, 0.0, 0.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Meter 2 Label A",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_OCTETS, 8, 0, 127, 20, "Mon. 2");
  objects[cntObject++] = MBN_OBJ( (char *)"Meter 2 Label B",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_OCTETS, 8, 0, 127, 20, "-");
  objects[cntObject++] = MBN_OBJ( (char *)"Meter 3 Left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Meter 3 Right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Meter 3 Label",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_OCTETS, 8, 0, 127, 20, "Mon. 2");
  objects[cntObject++] = MBN_OBJ( (char *)"Meter 4 Left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Meter 4 Right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Meter 4 Label",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_OCTETS, 8, 0, 127, 20, "Mon. 2");
  objects[cntObject++] = MBN_OBJ( (char *)"Main/Clock Label",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_OCTETS, 32, 0, 127, 20, "Mon. 2");
  objects[cntObject++] = MBN_OBJ( (char *)"Redlight 1",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_STATE, 1, 0, 1, 0, 0);
  objects[cntObject++] = MBN_OBJ( (char *)"Redlight 2",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_STATE, 1, 0, 1, 0, 0);
  objects[cntObject++] = MBN_OBJ( (char *)"Redlight 3",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_STATE, 1, 0, 1, 0, 0);
  objects[cntObject++] = MBN_OBJ( (char *)"Redlight 4",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_STATE, 1, 0, 1, 0, 0);
  objects[cntObject++] = MBN_OBJ( (char *)"Redlight 5",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_STATE, 1, 0, 1, 0, 0);
  objects[cntObject++] = MBN_OBJ( (char *)"Redlight 6",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_STATE, 1, 0, 1, 0, 0);
  objects[cntObject++] = MBN_OBJ( (char *)"Redlight 7",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_STATE, 1, 0, 1, 0, 0);
  objects[cntObject++] = MBN_OBJ( (char *)"Redlight 8",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_STATE, 1, 0, 1, 0, 0);
  objects[cntObject++] = MBN_OBJ( (char *)"Second dot count down",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_STATE, 1, 0, 1, 0, 0);
  objects[cntObject++] = MBN_OBJ( (char *)"Program end time enable",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_STATE, 1, 0, 1, 0, 0);
  objects[cntObject++] = MBN_OBJ( (char *)"Program end hour",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_UINT, 1, 0, 99, 0, 0);
  objects[cntObject++] = MBN_OBJ( (char *)"Program end minute",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_UINT, 1, 0, 59, 0, 0);
  objects[cntObject++] = MBN_OBJ( (char *)"Program end second",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_UINT, 1, 0, 59, 0, 0);
  objects[cntObject++] = MBN_OBJ( (char *)"Count down seconds",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, 0.0, 60.0, 0.0, 0.0);
//Module Label
  objects[cntObject++] = MBN_OBJ( (char *)"Module label",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_OCTETS, 8, 0, 127, 20, "Mod 1");
//Module Source label
  objects[cntObject++] = MBN_OBJ( (char *)"Module source label",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_OCTETS, 8, 0, 127, 20, "None");
//Module Console
  objects[cntObject++] = MBN_OBJ( (char *)"Module console",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_UINT, 1, 0, 255, 0, 0);
//Module Meter Left
//Module Meter Right
  objects[cntObject++] = MBN_OBJ( (char *)"Module Meter Left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Module Meter Right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
//DSP Gain
  objects[cntObject++] = MBN_OBJ( (char *)"DSP gain",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_OCTETS, 8, 0, 127, 20, "0 dB");
//LC On/Off
  objects[cntObject++] = MBN_OBJ( (char *)"LC on/off",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_STATE, 1, 0, 1, 0, 0);
//LC Freq
  objects[cntObject++] = MBN_OBJ( (char *)"LC frequency",
                                   MBN_DATATYPE_NODATA,
                                   MBN_DATATYPE_UINT, 2, 10, 20000, 80, 80);
//Dyn On/Off
  objects[cntObject++] = MBN_OBJ( (char *)"Dyn on/off",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_STATE, 1, 0, 1, 0, 0);
//DExp Th
  objects[cntObject++] = MBN_OBJ( (char *)"D-Exp threshold",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_OCTETS, 8, 0, 127, 20, "-20 dB");
//AGC Th
  objects[cntObject++] = MBN_OBJ( (char *)"AGC threshold",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_OCTETS, 8, 0, 127, 20, "-10 dB");
//AGC Ratio
  objects[cntObject++] = MBN_OBJ( (char *)"AGC Ratio",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_OCTETS, 8, 0, 127, 20, "100%");


  objects[cntObject++] = MBN_OBJ( (char *)"EQ on/off",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_STATE, 1, 0, 1, 0, 0);
  for (cntBand=0; cntBand<6; cntBand++)
  {
    sprintf(obj_desc, "EQ%d level", cntBand+1);
    objects[cntObject++] = MBN_OBJ(obj_desc,
                                   MBN_DATATYPE_NODATA,
                                   MBN_DATATYPE_FLOAT, 2, -18.0, 18.0, 0.0, 0.0);
    sprintf(obj_desc, "EQ%d frequency", cntBand+1);
    objects[cntObject++] = MBN_OBJ(obj_desc,
                                   MBN_DATATYPE_NODATA,
                                   MBN_DATATYPE_UINT, 2, 10, 20000, 1000, 1000);
    sprintf(obj_desc, "EQ%d Q", cntBand+1);
    objects[cntObject++] = MBN_OBJ(obj_desc,
                                   MBN_DATATYPE_NODATA,
                                   MBN_DATATYPE_FLOAT, 2, 0.1, 10.0, 1.0, 1.0);
    sprintf(obj_desc, "EQ%d type", cntBand+1);
    objects[cntObject++] = MBN_OBJ(obj_desc,
                                   MBN_DATATYPE_NODATA,
                                   MBN_DATATYPE_STATE, 2, 0, 7, 3);
  }
  objects[cntObject++] = MBN_OBJ( (char *)"Panorama",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_UINT, 2, 0, 1023, 512, 512);
  objects[cntObject++] = MBN_OBJ( (char *)"Show module parameters",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_STATE, 1, 0, 1, 1, 1);
  objects[cntObject++] = MBN_OBJ( (char *)"MIC active timer",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_STATE, 1, 0, 1, 1, 1);
  objects[cntObject++] = MBN_OBJ( (char *)"Init progress",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_UINT, 1, 0, 100, 0, 0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 1 left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 1 right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 2 left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 2 right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 3 left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 3 right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 4 left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 4 right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 5 left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 5 right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 6 left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 6 right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 7 left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 7 right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 8 left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 8 right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 9 left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 9 right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 10 left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 10 right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 11 left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 11 right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 12 left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 12 right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 13 left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 13 right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 14 left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 14 right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 15 left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 15 right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 16 left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 16 right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 17 left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 17 right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 18 left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 18 right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 19 left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 19 right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 20 left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 20 right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 21 left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 21 right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 22 left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 22 right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 23 left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 23 right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 24 left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 24 right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 25 left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 25 right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 26 left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 26 right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 27 left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 27 right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 28 left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 28 right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 29 left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 29 right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 30 left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 30 right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 31 left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 31 right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 32 left dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[cntObject++] = MBN_OBJ( (char *)"Small meter 32 right dB",
                                  MBN_DATATYPE_NODATA,
                                  MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  this_node.NumberOfObjects = cntObject;

  log_open();

  if (oem_name_short(oem_name, 32))
  {
    strncpy(this_node.Name, oem_name, 32);
    strcat(this_node.Name, " Meters");

    strncpy(this_node.Description, oem_name, 32);
    strcat(this_node.Description, " Meters (Linux)");
  }

  hwparent(&this_node);

  if (!use_eth)
  {
    if ((itf=mbnUnixOpen(socket_path, NULL, error)) == NULL)
    {
      fprintf(stderr, "Error opening unix socket: %s ('%s')", error, socket_path);
      log_close();
      exit(1);
    }
  }
  else
  {
    if ((itf=mbnEthernetOpen(ethdev, error)) == NULL)
    {
      fprintf(stderr, "Error opening ethernet device: %s ('%s')", error, ethdev);
      log_close();
      exit(1);
    }

    log_write("start link check");

    //open ethernet device for link status.
    if (mbnEthernetMIILinkStatus(itf, error)) {
      log_write("Link up");
    } else {
      log_write("Link down");
    }
  }

  if ((mbn=mbnInit(&this_node, objects, itf, error)) == NULL)
  {
    fprintf(stderr, "Error initializing MambaNet node: %s", error);
    log_close();
    exit(1);
  }

  mbnSetSetActuatorDataCallback(mbn, SetActuatorData);
  mbnSetAddressTableChangeCallback(mbn, AddressTableChange);


  mbnStartInterface(itf, error);

  log_write("-----------------------");
  log_write("Axum meters initialized");
  log_write("Version %d.%d, compiled at %s (%s)", this_node.FirmwareMajorRevision, this_node.FirmwareMinorRevision, __DATE__, __TIME__);
  sprintf(cmdline, "command line:");
  for (int cnt=0; cnt<argc; cnt++)
  {
    strcat(cmdline, " ");
    strcat(cmdline, argv[cnt]);
  }
  log_write(cmdline);
  log_write(mbnVersion());
  log_write("Starting QApplication");
}

int SetActuatorData(struct mbn_handler *mbn, unsigned short object, union mbn_data in)
{
  qt_mutex.lock();
  if (((object<1024) || (object>=(1024+this_node.NumberOfObjects))) || (browser == NULL))
  {
    qt_mutex.unlock();
    return 1;
  }

  switch (object)
  {
    case 1024:
    {
      float dB = in.Float;

      browser->MeterData[0] = dB;
    }
    break;
    case 1025:
    {
      float dB = in.Float;

      browser->MeterData[1] = dB;
    }
    break;
    case 1026:
    {
      browser->PhaseMeterData[0] = in.Float;
    }
    break;
    case 1027:
    {
      strncpy(browser->Label[0], (char *)in.Octets, 8);
      browser->Label[0][8] = 0;
		}
    break;
    case 1028:
    {
      strncpy(browser->Label[1], (char *)in.Octets, 8);
      browser->Label[1][8] = 0;
    }
		break;
    case 1029:
    {
      float dB = in.Float;

      browser->MeterData[2] = dB;
    }
    break;
    case 1030:
    {
      float dB = in.Float;

      browser->MeterData[3] = dB;
    }
    break;
    case 1031:
    {
      browser->PhaseMeterData[1] = in.Float;
    }
    break;
		case 1032:
		{
      strncpy(browser->Label[2], (char *)in.Octets, 8);
      browser->Label[2][8] = 0;
		}
		break;
    case 1033:
		{
      strncpy(browser->Label[3], (char *)in.Octets, 8);
      browser->Label[3][8] = 0;
		}
    break;
    case 1034:
    {
      float dB = in.Float;

      browser->MeterData[4] = dB;
    }
    break;
    case 1035:
    {
      float dB = in.Float;

      browser->MeterData[5] = dB;
    }
    break;
		case 1036:
		{
      strncpy(browser->Label[4], (char *)in.Octets, 8);
      browser->Label[4][8] = 0;
		}
		break;
    case 1037:
    {
      float dB = in.Float;

      browser->MeterData[6] = dB;
    }
    break;
    case 1038:
    {
      float dB = in.Float;

      browser->MeterData[7] = dB;
    }
    break;
		case 1039:
		{
      strncpy(browser->Label[5], (char *)in.Octets, 8);
      browser->Label[5][8] = 0;
		}
		break;
		case 1040:
		{
      strncpy(browser->Label[6], (char *)in.Octets, 32);
      browser->Label[6][32] = 0;
		}
		break;
		case 1041:
		case 1042:
		case 1043:
		case 1044:
		case 1045:
		case 1046:
		case 1047:
		case 1048:
		{
      int RedlightNr = object-1041;
      browser->RedlightState[RedlightNr] = in.State;
		}
    break;
    case 1049:
    {
      browser->CountDown = in.State;
    }
    break;
    case 1050:
    {
      browser->ProgramEndTimeEnabled = in.State;
    }
    break;
    case 1051:
    {
      browser->ProgramEndHour = in.UInt;
    }
    break;
    case 1052:
    {
      browser->ProgramEndMinute = in.UInt;
    }
    break;
    case 1053:
    {
      browser->ProgramEndSecond = in.UInt;
    }
    break;
    case 1054:
    {
      browser->CountDownSeconds = in.Float;
    }
    break;
    case 1055:
    {
      strncpy(browser->ModuleLabel, (char *)in.Octets, 8);
      browser->ModuleLabel[8] = 0;
    }
    break;
    case 1056:
    {
      strncpy(browser->SourceLabel, (char *)in.Octets, 8);
      browser->SourceLabel[8] = 0;
    }
    break;
    case 1057:
    {
      browser->ModuleConsole = in.UInt;
    }
    break;
    case 1058:
    {
      float dB = in.Float;

      browser->MeterData[8] = dB;
    }
    break;
    case 1059:
    {
      float dB = in.Float;

      browser->MeterData[9] = dB;
    }
    break;
    case 1060:
    {
      strncpy(browser->DSPGain, (char *)in.Octets, 8);
      browser->DSPGain[8] = 0;
    }
    break;
    case 1061:
    {
      browser->LCOn = in.State;
    }
    break;
    case 1062:
    {
      browser->LCFreq = in.UInt;
    }
    break;
    case 1063:
    {
      browser->DynOn = in.State;
    }
    break;
    case 1064:
    {
      strncpy(browser->DExpTh, (char *)in.Octets, 8);
      browser->DExpTh[8] = 0;
    }
    break;
    case 1065:
    {
      strncpy(browser->AGCTh, (char *)in.Octets, 8);
      browser->AGCTh[8] = 0;
    }
    break;
    case 1066:
    {
      strncpy(browser->AGCRatio, (char *)in.Octets, 8);
      browser->AGCRatio[8] = 0;
    }
    break;
    case 1067:
    {
      browser->EQOn = in.State;
    }
    break;
    case 1068:
    case 1072:
    case 1076:
    case 1080:
    case 1084:
    case 1088:
    {
      int BandNr = (object-1068)/4;
      browser->EQLevel[BandNr] = in.Float;
    }
    break;
    case 1069:
    case 1073:
    case 1077:
    case 1081:
    case 1085:
    case 1089:
    {
      int BandNr = (object-1069)/4;
      browser->EQFrequency[BandNr] = in.UInt;
    }
    break;
    case 1070:
    case 1074:
    case 1078:
    case 1082:
    case 1086:
    case 1090:
    {
      int BandNr = (object-1070)/4;
      browser->EQBandwidth[BandNr] = in.Float;
    }
    break;
    case 1071:
    case 1075:
    case 1079:
    case 1083:
    case 1087:
    case 1091:
    {
      int BandNr = (object-1071)/4;
      browser->EQType[BandNr] = in.State;
    }
    break;
    case 1092:
    { //Panorama
      browser->Panorama = in.UInt;
    }
    break;
    case 1093:
    { //Show module parameters
      browser->ShowModuleParameters = in.State;
    }
    break;
    case 1094:
    {
      browser->MICActiveTimerEnabled = in.State;
    }
    break;
    case 1095:
    {
      log_write("Percent: %d", in.UInt);
      browser->InitProgress = in.UInt;
    }
    break;
  }
  if ((object >= 1096) && (object<1160))
  {
    float dB = in.Float;
    browser->MeterData[(object-1096)+10] = dB;
  }
  qt_mutex.unlock();
  mbnUpdateActuatorData(mbn, object, in);
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

int main(int argc, char *argv[])
{
  int app_return;

  QApplication app(argc, argv);

  QMainWindow mainWin;
  browser = new Browser(&mainWin);
  mainWin.setCentralWidget(browser);

  //init mambanet after browser is active
  init(argc, argv);

//	QWebView *test = new QWebView(browser->tabWidget);
//	test->load(QUrl("http://Service:Service@192.168.0.200/new/skin_table.html?file=main.php"));
//	test->show();

	mainWin.showFullScreen();
	mainWin.setGeometry(0,0, 1024,800);

  app.setOverrideCursor( QCursor( Qt::BlankCursor ) );

  app_return = app.exec();

  log_write("Closing meters");

  if (mbn)
    mbnFree(mbn);

  log_close();
  return app_return;
}

char CheckLinkStatus()
{
  char LinkStatus = -1;

  if (use_eth)
  {
    if ((LinkStatus = mbnEthernetMIILinkStatus(mbn->itf, error)) == -1)
    {
      log_write("mbnEthernetMIILinkStatus error: %d", error);
      return -1;
    }
  }
  return LinkStatus;

}
