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

#define FIRMWARE_MAJOR_VERSION   0
#define FIRMWARE_MINOR_VERSION   1

#define MANUFACTURER_ID          0x0001	//D&R
#define PRODUCT_ID               0x001A	//Axum linux meters

#define NR_OF_STATIC_OBJECTS    (1032-1023)
#define NR_OF_OBJECTS            NR_OF_STATIC_OBJECTS

#define DEFAULT_GTW_PATH  "/tmp/axum-gateway"
#define DEFAULT_ETH_DEV   "eth0"
#define DEFAULT_LOG_FILE  "/var/log/axum-meters.log"

Browser *browser = NULL;

struct mbn_interface *itf;
struct mbn_handler *mbn;
char error[MBN_ERRSIZE];

struct mbn_node_info this_node = {
  0, 0,                   //MambaNet address, Services
  "Axum PPM Meters (Linux)",
  "Axum-PPM-Meters",
  MANUFACTURER_ID, PRODUCT_ID, 0x0001,
  0, 0,                   //Hw revision
  2, 0,                   //Fw revision
  0, 0,                   //FPGA revision
  NR_OF_OBJECTS,          //Number of objects
  0,                      //Default engine address
  {0x0000,0x0000,0x000},  //Hardware parent
  0                       //Service request
};

void init(int argc, char *argv[]);
int SetActuatorData(struct mbn_handler *mbn, unsigned short object, union mbn_data in);
int delay_us(double sleep_time);

void init(int argc, char *argv[])
{
  char ethdev[50];
  struct mbn_object objects[NR_OF_STATIC_OBJECTS];
  int c;

  strcpy(ethdev, DEFAULT_ETH_DEV);
  strcpy(log_file, DEFAULT_LOG_FILE);
  strcpy(hwparent_path, DEFAULT_GTW_PATH);

  while((c =getopt(argc, argv, "e:g:l:i:qws")) != -1)
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
      case 'q':
      case 'w':
      case 's':
      {
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

  objects[0] = MBN_OBJ( (char *)"Meter 1 Left dB",
                        MBN_DATATYPE_NODATA,
                        MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[1] = MBN_OBJ( (char *)"Meter 1 Right dB",
                        MBN_DATATYPE_NODATA,
                        MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[2] = MBN_OBJ( (char *)"Meter 1 Label A",
                        MBN_DATATYPE_NODATA,
                        MBN_DATATYPE_OCTETS, 8, 0, 127, 20, "Mon. 1");
  objects[3] = MBN_OBJ( (char *)"Meter 1 Label B",
                        MBN_DATATYPE_NODATA,
                        MBN_DATATYPE_OCTETS, 8, 0, 127, 20, "-");
  objects[4] = MBN_OBJ( (char *)"Meter 2 Left dB",
                        MBN_DATATYPE_NODATA,
                        MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[5] = MBN_OBJ( (char *)"Meter 2 Right dB",
                        MBN_DATATYPE_NODATA,
                        MBN_DATATYPE_FLOAT, 2, -50.0, 5.0, -50.0, -50.0);
  objects[6] = MBN_OBJ( (char *)"Meter 2 Label A",
                        MBN_DATATYPE_NODATA,
                        MBN_DATATYPE_OCTETS, 8, 0, 127, 20, "Mon. 2");
  objects[7] = MBN_OBJ( (char *)"Meter 2 Label B",
                        MBN_DATATYPE_NODATA,
                        MBN_DATATYPE_OCTETS, 8, 0, 127, 20, "-");
  objects[8] = MBN_OBJ( (char *)"On Air",
                        MBN_DATATYPE_NODATA,
                        MBN_DATATYPE_STATE, 1, 0, 1, 0, 0);
  this_node.NumberOfObjects = 9;

  log_open();
  hwparent(&this_node);

  if ((itf=mbnEthernetOpen(ethdev, error)) == NULL)
  {
    fprintf(stderr, "Error opening ethernet device: %s", error);
    log_close();
    exit(1);
  }

  if ((mbn=mbnInit(&this_node, objects, itf, error)) == NULL)
  {
    fprintf(stderr, "Error initializing MambaNet node: %s", error);
    log_close();
    exit(1);
  }

  mbnSetSetActuatorDataCallback(mbn, SetActuatorData);

  log_write("-----------------------");
  log_write("Axum meters initialized");
  log_write("Starting QApplication");
}

int SetActuatorData(struct mbn_handler *mbn, unsigned short object, union mbn_data in)
{
  if (((object<1024) || (object>=(1024+this_node.NumberOfObjects))) || (browser == NULL))
  {
    return 1;
  }

  switch (object)
  {
    case 1024:
    {
      float dB = in.Float+20;

      browser->MeterData[0] = dB;
      if (dB>browser->NewDNRPPMMeter->FdBPosition)
			{
			  browser->NewDNRPPMMeter->FdBPosition = dB;
				browser->NewDNRPPMMeter->update();
		  }
    }
    break;
    case 1025:
    {
      float dB = in.Float+20;

      browser->MeterData[1] = dB;
			if (dB>browser->NewDNRPPMMeter_2->FdBPosition)
			{
			  browser->NewDNRPPMMeter_2->FdBPosition = dB;
		    browser->NewDNRPPMMeter_2->update();
			}
    }
    break;
    case 1026:
    {
      strncpy(browser->Label[0], (char *)in.Octets, 8);
      browser->Label[0][8] = 0;
      browser->label_3->setText(QString(browser->Label[0]));
		}
    break;
    case 1027:
    {
      strncpy(browser->Label[1], (char *)in.Octets, 8);
      browser->Label[1][8] = 0;
      browser->label_5->setText(QString(browser->Label[1]));
    }
		break;
    case 1028:
    {
      float dB = in.Float+20;

      browser->MeterData[2] = dB;
      if (dB>browser->NewDNRPPMMeter_3->FdBPosition)
			{
			  browser->NewDNRPPMMeter_3->FdBPosition = dB;
				browser->NewDNRPPMMeter_3->update();
      }
    }
    break;
    case 1029:
    {
      float dB = in.Float+20;

      browser->MeterData[3] = dB;
      if (dB>browser->NewDNRPPMMeter_4->FdBPosition)
      {
        browser->NewDNRPPMMeter_4->FdBPosition = dB;
        browser->NewDNRPPMMeter_4->update();
      }
    }
    break;
		case 1030:
		{
      strncpy(browser->Label[2], (char *)in.Octets, 8);
      browser->Label[2][8] = 0;
      browser->label_4->setText(QString(browser->Label[2]));
		}
		break;
    case 1031:
		{
      strncpy(browser->Label[3], (char *)in.Octets, 8);
      browser->Label[3][8] = 0;
      browser->label_6->setText(QString(browser->Label[3]));
		}
    break;
		case 1032:
		{
      if (in.State)
      {
			  browser->label_7->setText("ON AIR");
			}
			else
			{
        browser->label_7->setText("");
      }
		}
  }
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

  app_return = app.exec();

  log_write("Closing meters");

  if (mbn)
    mbnFree(mbn);

  log_close();
  return app_return;
}
