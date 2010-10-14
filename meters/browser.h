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

#ifndef BROWSER_H
#define BROWSER_H

#include <QWidget>
#include "ui_browserwidget.h"

#include <QtGui>

#include <QObject>
#include <QTimer>
#include <QSocketNotifier>
#include <stdio.h>
#include <string.h>         //for memcpy/strncpy
#include <unistd.h>         //for STDIN_FILENO/close/write
#include <fcntl.h>          //for GET_FL/SET_FL/O_XXXXXX/FNDELAY
#include <arpa/inet.h>      //for AF_PACKET/SOCK_DGRAM/htons/ntohs/socket/bind/sendto
#include <linux/if_arp.h>   //for ETH_P_ALL/ifreq/sockaddr_ll/ETH_ALEN etc...
#include <sys/ioctl.h>          //for ioctl

class ConnectionWidget;
class QTableView;
class QPushButton;
class QTextEdit;
class QSqlError;
class ChaseWidget;
class ToolbarSearch;

class Browser: public QWidget, public Ui::Browser
{
    Q_OBJECT
public:
	 int cntSecond;

	 double MeterData[10];
   double PhaseMeterData[2];
   char Label[7][9];
   char CurrentLabel[7][9];
   bool OnAirState;
   bool CurrentOnAirState;
   bool RedlightState[8];
   bool CurrentRedlightState[8];
   bool CountDown;
   bool CurrentCountDown;
	 double previouNumberOfSeconds;
   bool LinkStatus;
   bool CurrentLinkStatus;

   bool ProgramEndTimeEnabled;
   bool CurrentProgramEndTimeEnabled;
   unsigned char ProgramEndMinute;
   unsigned char CurrentProgramEndMinute;
   unsigned char ProgramEndSecond;
   unsigned char CurrentProgramEndSecond;
   float CountDownSeconds;
   float CurrentCountDownSeconds;

   char ModuleLabel[9];
   char CurrentModuleLabel[9];
   char SourceLabel[9];
   char CurrentSourceLabel[9];
   unsigned char ModuleConsole;
   unsigned char CurrentModuleConsole;

   char DSPGain[9];
   char CurrentDSPGain[9];
   unsigned int LCFreq;
   unsigned int CurrentLCFreq;
   bool LCOn;
   bool CurrentLCOn;

   bool DynOn;
   bool CurrentDynOn;
   char DExpTh[9];
   char CurrentDExpTh[9];
   char AGCTh[9];
   char CurrentAGCTh[9];
   char AGCRatio[9];
   char CurrentAGCRatio[9];

   bool EQOn;
   bool CurrentEQOn;
   float EQLevel[6];
   float CurrentEQLevel[6];
   unsigned int EQFrequency[6];
   unsigned int CurrentEQFrequency[6];
   float EQBandwidth[6];
   float CurrentEQBandwidth[6];
   int EQType[6];
   int CurrentEQType[6];

   unsigned int Panorama;
   unsigned int CurrentPanorama;

   bool ShowModuleParameters;
   bool CurrentShowModuleParameters;

   Browser(QWidget *parent = 0);
   virtual ~Browser();

	 void timerEvent(QTimerEvent *Event);

	 QToolBar *m_NavigationBar;

	 QAction *m_Back;
	 QAction *m_Forward;
	 QAction *m_StopReload;
	 ChaseWidget *m_ChaseWidget;

public slots:
	 void MeterRelease();


private:
};

#endif
