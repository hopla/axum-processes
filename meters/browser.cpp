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

#include "browser.h"

#include <QtCore>
#include <QtGui>
#include <QtSql>
#include <qthread.h>

#include "chasewidget.h"
#include "common.h"

extern QMutex qt_mutex;

Browser::Browser(QWidget *parent)
    : QWidget(parent)
{
  int cnt;

  setupUi(this);

  NewDNRImageTopOffAir->setVisible(true);
  NewDNRImageBottomOffAir->setVisible(true);

  frame->setVisible(false);
  frame_init->setVisible(true);

/*	m_NavigationBar = new QToolBar(label_7);

	m_Back = new QAction(label_7);
	m_Back->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));

	m_Forward = new QAction(label_7);
	m_Forward->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));

	m_StopReload = new QAction(label_7);
	m_StopReload->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));

//	m_ToolbarSearch = new ToolbarSearch(m_NavigationBar);

	m_ChaseWidget = new ChaseWidget(label_7);

	m_NavigationBar->addAction(m_Back);
	m_NavigationBar->addAction(m_Forward);
	m_NavigationBar->addAction(m_StopReload);
//	m_NavigationBar->addWidget(m_ToolbarSearch);
	m_NavigationBar->addWidget(m_ChaseWidget);

//	connect(m_ToolbarSearch, SIGNAL(search(const QUrl&)), SLOT(loadUrl(const QUrl&)));
	connect(m_Back, SIGNAL(triggered()), webView, SLOT(back()));
	connect(m_Forward, SIGNAL(triggered()), webView, SLOT(forward()));
	connect(m_StopReload, SIGNAL(triggered()), webView, SLOT(reload()));*/

	cntSecond = 0;

  frame->setVisible(true);

	startTimer(30);

	for (cnt=0; cnt<75; cnt++)
  {
    MeterData[cnt] = -50;
  }

  sprintf(Label[0],"Meter 1 ");
  sprintf(Label[1],"  ----  ");
  sprintf(Label[2],"Meter 2 ");
  sprintf(Label[3],"  ----  ");
  sprintf(Label[4],"Meter 3 ");
  sprintf(Label[5],"Meter 4 ");
  sprintf(Label[6],"  ----  ");
  sprintf(CurrentLabel[0],"Meter 1 ");
  sprintf(CurrentLabel[1],"  ----  ");
  sprintf(CurrentLabel[2],"Meter 2 ");
  sprintf(CurrentLabel[3],"  ----  ");
  sprintf(CurrentLabel[4],"Meter 3 ");
  sprintf(CurrentLabel[5],"Meter 4 ");
  sprintf(CurrentLabel[6],"  ----  ");

  sprintf(ModuleLabel, "Mod 1");
  sprintf(CurrentModuleLabel, "Mod 1");
  sprintf(SourceLabel, "None");
  sprintf(CurrentSourceLabel, "None");
  ModuleConsole = 0;
  CurrentModuleConsole = 0;

  sprintf(DExpTh, "-20 dB");
  sprintf(CurrentDExpTh, "-20 dB");
  sprintf(AGCTh, "-10 dB");
  sprintf(CurrentAGCTh, "-20 dB");
  sprintf(AGCRatio, "100%%");
  sprintf(CurrentAGCRatio, "100%%");

  for (cnt=0; cnt<8; cnt++)
  {
    RedlightState[cnt] = 0;
    CurrentRedlightState[cnt] = 0;
  }

  CountDown = 0;
  CurrentCountDown = 0;

  LinkStatus = 0;
  CurrentLinkStatus = 0;
  EngineStatus = 0;
  CurrentEngineStatus = 0;

  MICActiveTimerEnabled = 0;
  CurrentMICActiveTimerEnabled = 0;
  CurrentElapsedTime = 0;

  ProgramEndTimeEnabled = false;
  CurrentProgramEndTimeEnabled = false;
  ProgramEndHour = 0;
  CurrentProgramEndHour = 0;
  ProgramEndMinute = 0;
  CurrentProgramEndMinute = 0;
  ProgramEndSecond = 0;
  CurrentProgramEndSecond = 0;

  CountDownSeconds = 0;
  CurrentCountDownSeconds = 0;

  InitProgress = 0;
  CurrentInitProgress = 0;
  Initializing = 1;
  ProgressReceived = 0;

  sprintf(DSPGain, "0 dB");
  sprintf(CurrentDSPGain, "0 dB");

  LCFreq = 80;
  CurrentLCFreq = 80;

  LCOn = false;
  CurrentLCOn = false;

  DynOn = false;
  CurrentDynOn = false;

  Panorama = 512;
  CurrentPanorama = 512;

  ShowModuleParameters = true;
  CurrentShowModuleParameters = true;

  EQOn = false;
  CurrentEQOn = false;

  for (cnt=0; cnt<6; cnt++)
  {
    EQLevel[cnt] = 0;
    CurrentEQLevel[cnt] = 0;
    EQFrequency[cnt] = 1000;
    CurrentEQFrequency[cnt] = 1000;
    EQBandwidth[cnt] = 1;
    CurrentEQBandwidth[cnt] = 1;
    EQType[cnt] = 3;
    CurrentEQType[cnt] = 3;
  }
  TimerLabel->setVisible(false);
}

Browser::~Browser()
{
}

extern char CheckLinkStatus();

void IntToTimerString(char *timer_str, int elapsed_time)
{
  char TempStr[16];
  int Days = elapsed_time/86400;
  int time_left = elapsed_time%86400;
  int Hours = time_left/3600;
  time_left = time_left%3600;
  int Minutes = time_left/60;
  time_left = time_left%60;
  int Seconds = time_left;

  if (Days)
  {
    sprintf(TempStr, "%dd ", Days);
    strcat(timer_str, TempStr);
  }
  if ((Days) || (Hours))
  {
    sprintf(TempStr, "%02d:", Hours);
    strcat(timer_str, TempStr);
  }
  sprintf(TempStr, "%02d:%02d", Minutes, Seconds);
  strcat(timer_str, TempStr);
}

void Browser::timerEvent(QTimerEvent *Event)
{
  char timer_str[16] = "";

	cntSecond++;

  qt_mutex.lock();

  if (CurrentInitProgress != InitProgress)
  {
    if (InitProgress < 100)
    {
      frame_init->setVisible(true);

      progressBar->setValue(InitProgress);

      Initializing = 1;
    }
    else
    {
      frame_init->setVisible(false);

      Initializing = 0;
    }
    CurrentInitProgress = InitProgress;
    ProgressReceived = 1;
  }
  else if ((Initializing) && (!ProgressReceived))
  {
    if (InitProgress < 99)
    {
      if ((cntSecond%33) == 0)
      {
        progressBar->setValue(++InitProgress);
        CurrentInitProgress = InitProgress;
      }
    }
  }

  if ((LinkStatus = CheckLinkStatus()) != -1)
  {
    if (CurrentLinkStatus != LinkStatus)
    {
      NewDNRImageNoLink->setVisible(!LinkStatus);
      CurrentLinkStatus = LinkStatus;
      log_write("Link status change: %s", LinkStatus ? ("Up") : ("Down"));
    }
  }

  if (CurrentEngineStatus != EngineStatus)
  {
    log_write("EngineStatus %d", EngineStatus);
    NewDNRImageNoEngine->setVisible(!EngineStatus);
    CurrentEngineStatus = EngineStatus;
  }

  //if (!Initializing)
  {
    MeterRelease();

    if (CurrentMICActiveTimerEnabled != MICActiveTimerEnabled)
    {
      CurrentMICActiveTimerEnabled = MICActiveTimerEnabled;
      if (MICActiveTimerEnabled)
      {
        timespec newTime;
        clock_gettime(CLOCK_MONOTONIC, &newTime);
        PreviousNumberOfSeconds = newTime.tv_sec+((double)newTime.tv_nsec/1000000000);
        CurrentElapsedTime = 0;

        IntToTimerString(timer_str, 0);
        QString rich_str = tr("<font color='%1'>%2</font>");
        TimerLabel->setText(rich_str.arg("#FF5555", timer_str));
        TimerLabel->setVisible(true);
      }
    }
    else if (CurrentMICActiveTimerEnabled)
    {
      timespec newTime;
      clock_gettime(CLOCK_MONOTONIC, &newTime);
      int ElapsedTime = (newTime.tv_sec+((double)newTime.tv_nsec/1000000000)) - PreviousNumberOfSeconds;

      if (CurrentElapsedTime != ElapsedTime)
      {
        IntToTimerString(timer_str, ElapsedTime);
        QString rich_str = tr("<font color='%1'>%2</font>");
        TimerLabel->setText(rich_str.arg("#FF5555", timer_str));
        CurrentElapsedTime = ElapsedTime;
      }
    }
    else if (TimerLabel->isVisible())
    {
      timespec newTime;
      clock_gettime(CLOCK_MONOTONIC, &newTime);
      int ElapsedTime = (newTime.tv_sec+((double)newTime.tv_nsec/1000000000)) - PreviousNumberOfSeconds;

      if (CurrentElapsedTime != ElapsedTime)
      {
        int Difference = ElapsedTime-CurrentElapsedTime;
        if (Difference == 60)
        {
          IntToTimerString(timer_str, CurrentElapsedTime);
          QString rich_str = tr("<font color='%1'>%2</font>");
          TimerLabel->setText(rich_str.arg("#551B1B", timer_str));
        }
        else if (Difference > 60*10)
        {
          TimerLabel->setVisible(false);
        }
      }
    }
  }
  qt_mutex.unlock();

  return;
  Event = NULL;
}

#define RELEASE_STEP 0.45
void CalculatePPMRelease(DNRPPMMeter *PPMMeter, double *CurrentMeterData)
{
  float Difference;

  if ((PPMMeter->FdBPosition>-50) || (*CurrentMeterData>-50))
  {
    Difference = *CurrentMeterData-PPMMeter->FdBPosition;

    if (Difference < -RELEASE_STEP)
    {
      Difference = -RELEASE_STEP;
		}

    if (Difference != 0)
    {
      PPMMeter->FdBPosition += Difference;
      if (PPMMeter->CalculateMeter())
      {
        PPMMeter->update();
      }
    }
	}
}

void Browser::MeterRelease()
{
  int cnt;
  char ConsoleString[8];
  char ColorString[8];
/*
#ifdef Q_OS_WIN32
	LARGE_INTEGER freq, newTime;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&newTime);
	double newNumberOfSeconds = (double)newTime.QuadPart/freq.QuadPart;
#else
   timespec newTime;
   clock_gettime(CLOCK_MONOTONIC, &newTime);
   double newNumberOfSeconds = newTime.tv_sec+((double)newTime.tv_nsec/1000000000);
#endif
   double elapsedTime = newNumberOfSeconds - previousNumberOfSeconds;

	previousNumberOfSeconds = newNumberOfSeconds;
//	printf("time:%g - delta: %g\n", newNumberOfSeconds, elapsedTime);
*/
//  #define RELEASE_STEP 0.15
  #define PHASE_STEPSIZE 0.0075*4

  if (PhaseMeterData[0] > (NewDNRPhaseMeter->FPosition+PHASE_STEPSIZE))
    NewDNRPhaseMeter->setPosition(NewDNRPhaseMeter->FPosition+PHASE_STEPSIZE);
  else if (PhaseMeterData[0] < (NewDNRPhaseMeter->FPosition-PHASE_STEPSIZE))
    NewDNRPhaseMeter->setPosition(NewDNRPhaseMeter->FPosition-PHASE_STEPSIZE);
  else
    NewDNRPhaseMeter->setPosition(PhaseMeterData[0]);

  if (PhaseMeterData[1] > (NewDNRPhaseMeter_2->FPosition+PHASE_STEPSIZE))
    NewDNRPhaseMeter_2->setPosition(NewDNRPhaseMeter_2->FPosition+PHASE_STEPSIZE);
  else if (PhaseMeterData[1] < (NewDNRPhaseMeter_2->FPosition-PHASE_STEPSIZE))
    NewDNRPhaseMeter_2->setPosition(NewDNRPhaseMeter_2->FPosition-PHASE_STEPSIZE);
  else
    NewDNRPhaseMeter_2->setPosition(PhaseMeterData[1]);

  CalculatePPMRelease(NewDNRPPMMeter, &MeterData[0]);
  CalculatePPMRelease(NewDNRPPMMeter_2, &MeterData[1]);
  CalculatePPMRelease(NewDNRPPMMeter_3, &MeterData[2]);
  CalculatePPMRelease(NewDNRPPMMeter_4, &MeterData[3]);
  CalculatePPMRelease(NewDNRPPMMeter_5, &MeterData[4]);
  CalculatePPMRelease(NewDNRPPMMeter_6, &MeterData[5]);
  CalculatePPMRelease(NewDNRPPMMeter_7, &MeterData[6]);
  CalculatePPMRelease(NewDNRPPMMeter_8, &MeterData[7]);
  CalculatePPMRelease(NewDNRPPMMeter_9, &MeterData[8]);
  CalculatePPMRelease(NewDNRPPMMeter_10, &MeterData[9]);

  CalculatePPMRelease(NewSmallDNRPPMMeter_1, &MeterData[10]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_2, &MeterData[11]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_3, &MeterData[12]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_4, &MeterData[13]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_5, &MeterData[14]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_6, &MeterData[15]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_7, &MeterData[16]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_8, &MeterData[17]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_9, &MeterData[18]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_10, &MeterData[19]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_11, &MeterData[20]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_12, &MeterData[22]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_13, &MeterData[23]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_14, &MeterData[24]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_15, &MeterData[25]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_16, &MeterData[26]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_17, &MeterData[27]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_18, &MeterData[28]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_19, &MeterData[29]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_20, &MeterData[30]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_21, &MeterData[31]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_22, &MeterData[32]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_23, &MeterData[33]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_24, &MeterData[34]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_25, &MeterData[35]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_26, &MeterData[36]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_27, &MeterData[37]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_28, &MeterData[38]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_29, &MeterData[39]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_30, &MeterData[40]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_31, &MeterData[41]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_32, &MeterData[42]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_33, &MeterData[43]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_34, &MeterData[44]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_35, &MeterData[45]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_36, &MeterData[46]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_37, &MeterData[47]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_38, &MeterData[48]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_39, &MeterData[49]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_40, &MeterData[50]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_41, &MeterData[51]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_42, &MeterData[52]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_43, &MeterData[53]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_44, &MeterData[54]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_45, &MeterData[55]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_46, &MeterData[56]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_47, &MeterData[57]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_48, &MeterData[58]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_49, &MeterData[59]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_50, &MeterData[60]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_51, &MeterData[61]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_52, &MeterData[62]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_53, &MeterData[63]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_54, &MeterData[64]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_55, &MeterData[65]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_56, &MeterData[66]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_57, &MeterData[67]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_58, &MeterData[68]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_59, &MeterData[69]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_60, &MeterData[70]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_61, &MeterData[71]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_62, &MeterData[72]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_63, &MeterData[73]);
  CalculatePPMRelease(NewSmallDNRPPMMeter_64, &MeterData[74]);

  if (strcmp(Label[0], CurrentLabel[0]) != 0)
  {
    label_3->setText(QString(Label[0]));
    strcpy(Label[0], CurrentLabel[0]);
  }
  if (strcmp(Label[1], CurrentLabel[1]) != 0)
  {
    label_5->setText(QString(Label[1]));
    strcpy(Label[1], CurrentLabel[1]);
  }
  if (strcmp(Label[2], CurrentLabel[2]) != 0)
  {
    label_4->setText(QString(Label[2]));
    strcpy(Label[2], CurrentLabel[2]);
  }
  if (strcmp(Label[3], CurrentLabel[3]) != 0)
  {
    label_6->setText(QString(Label[3]));
    strcpy(Label[3], CurrentLabel[3]);
  }
  if (strcmp(Label[4], CurrentLabel[4]) != 0)
  {
    label_7->setText(QString(Label[4]));
    strcpy(Label[4], CurrentLabel[4]);
  }
  if (strcmp(Label[5], CurrentLabel[5]) != 0)
  {
    label_8->setText(QString(Label[5]));
    strcpy(Label[5], CurrentLabel[5]);
  }
  if (strcmp(Label[6], CurrentLabel[6]) != 0)
  {
    label_9->setText(QString(Label[6]));
    strcpy(Label[6], CurrentLabel[6]);
  }

  for (cnt=0; cnt<8; cnt++)
  {
    switch (cnt)
    {
      case 0:
      {
        Redlight1OffDNRImage->setVisible(!RedlightState[cnt]);
        Redlight1OnDNRImage->setVisible(RedlightState[cnt]);
      }
      break;
      case 1:
      {
        Redlight2OffDNRImage->setVisible(!RedlightState[cnt]);
        Redlight2OnDNRImage->setVisible(RedlightState[cnt]);
      }
      break;
      case 2:
      {
        Redlight3OffDNRImage->setVisible(!RedlightState[cnt]);
        Redlight3OnDNRImage->setVisible(RedlightState[cnt]);
      }
      break;
      case 3:
      {
        Redlight4OffDNRImage->setVisible(!RedlightState[cnt]);
        Redlight4OnDNRImage->setVisible(RedlightState[cnt]);
      }
      break;
      case 4:
      {
        Redlight5OffDNRImage->setVisible(!RedlightState[cnt]);
        Redlight5OnDNRImage->setVisible(RedlightState[cnt]);
      }
      break;
      case 5:
      {
        Redlight6OffDNRImage->setVisible(!RedlightState[cnt]);
        Redlight6OnDNRImage->setVisible(RedlightState[cnt]);
      }
      break;
      case 6:
      {
        Redlight7OffDNRImage->setVisible(!RedlightState[cnt]);
        Redlight7OnDNRImage->setVisible(RedlightState[cnt]);
      }
      break;
      case 7:
      {
        Redlight8OffDNRImage->setVisible(!RedlightState[cnt]);
        Redlight8OnDNRImage->setVisible(RedlightState[cnt]);
      }
      break;
    }
    CurrentRedlightState[cnt] = RedlightState[cnt];
  }
  if (CountDown != CurrentCountDown)
  {
    NewDNRAnalogClock->setSecondDotsCountDown(CountDown);
    CurrentCountDown = CountDown;
  }

  if (ProgramEndHour != CurrentProgramEndHour)
  {
    NewDNRAnalogClock->setEndTimeHour(ProgramEndHour);
    CurrentProgramEndHour = ProgramEndHour;
  }

  if (ProgramEndMinute != CurrentProgramEndMinute)
  {
    NewDNRAnalogClock->setEndTimeMinute(ProgramEndMinute);
    CurrentProgramEndMinute = ProgramEndMinute;
  }

  if (ProgramEndTimeEnabled != CurrentProgramEndTimeEnabled)
  {
    NewDNRAnalogClock->setEndTime(ProgramEndTimeEnabled);
    CurrentProgramEndTimeEnabled = ProgramEndTimeEnabled;
  }

  if (ProgramEndSecond != CurrentProgramEndSecond)
  {
    NewDNRAnalogClock->setEndTimeSecond(ProgramEndSecond);
    CurrentProgramEndSecond = ProgramEndSecond;
  }

  if (CountDownSeconds != CurrentCountDownSeconds)
  {
    NewDNRAnalogClock->setCountDownTime(CountDownSeconds);
    CurrentCountDownSeconds = CountDownSeconds;
  }

  if (strcmp(ModuleLabel, CurrentModuleLabel) != 0)
  {
    sprintf(ConsoleString, "%d", ModuleConsole+1);
    SelectedModuleLabel->setText(QString(ModuleLabel).trimmed()+" - "+QString(CurrentSourceLabel)+" - "+QString(ConsoleString));
    strcpy(CurrentModuleLabel, ModuleLabel);
  }
  if (strcmp(SourceLabel, CurrentSourceLabel) != 0)
  {
    sprintf(ConsoleString, "%d", ModuleConsole+1);
    SelectedModuleLabel->setText(QString(CurrentModuleLabel).trimmed()+" - "+QString(SourceLabel)+" - "+QString(ConsoleString));
    strcpy(CurrentSourceLabel, SourceLabel);
  }
  if (ModuleConsole != CurrentModuleConsole)
  {
    sprintf(ConsoleString, "%d", ModuleConsole+1);
    SelectedModuleLabel->setText(QString(CurrentModuleLabel).trimmed()+" - "+QString(CurrentSourceLabel)+" - "+QString(ConsoleString));
    CurrentModuleConsole = ModuleConsole;
  }

  if (strcmp(DSPGain, CurrentDSPGain) != 0)
  {
    DSPGainLabel->setText(QString("DSP Gain<BR>")+QString(DSPGain));
    strcpy(CurrentDSPGain, DSPGain);
  }

  if (LCFreq != CurrentLCFreq)
  {
    NewDNREQPanel->setFrequencyLowCut(LCFreq);
    CurrentLCFreq = LCFreq;
  }

  if (LCOn != CurrentLCOn)
  {
    NewDNREQPanel->setLCOn(LCOn);
    CurrentLCOn = LCOn;
  }

  if (DynOn != CurrentDynOn)
  {
    if (DynOn)
    {
      sprintf(ColorString, "#C0C0C0");
    }
    else
    {
      sprintf(ColorString, "#000000");
    }

    DExpThLabel->setText(QString("<font color='%1'>D-Exp Th<BR>%2").arg(ColorString, QString(CurrentDExpTh)));
    AGCThLabel->setText(QString("<font color='%1'>AGC Th<BR>%2").arg(ColorString, QString(CurrentAGCTh)));
    AGCRatioLabel->setText(QString("<font color='%1'>AGC Ratio<BR>%2").arg(ColorString, QString(CurrentAGCRatio)));

    CurrentDynOn = DynOn;
  }

  if (strcmp(DExpTh, CurrentDExpTh) != 0)
  {
    if (CurrentDynOn)
    {
      sprintf(ColorString, "#C0C0C0");
    }
    else
    {
      sprintf(ColorString, "#000000");
    }
    DExpThLabel->setText(QString("<font color='%1'>D-Exp Th<BR>%2").arg(ColorString, QString(DExpTh)));
    strcpy(CurrentDExpTh, DExpTh);
  }
  if (strcmp(AGCTh, CurrentAGCTh) != 0)
  {
    if (CurrentDynOn)
    {
      sprintf(ColorString, "#C0C0C0");
    }
    else
    {
      sprintf(ColorString, "#000000");
    }
    AGCThLabel->setText(QString("<font color='%1'>AGC Th<BR>%2").arg(ColorString, QString(CurrentAGCTh)));
    strcpy(CurrentAGCTh, AGCTh);
  }
  if (strcmp(AGCRatio, CurrentAGCRatio) != 0)
  {
    if (CurrentDynOn)
    {
      sprintf(ColorString, "#C0C0C0");
    }
    else
    {
      sprintf(ColorString, "#000000");
    }
    AGCRatioLabel->setText(QString("<font color='%1'>AGC Ratio<BR>%2").arg(ColorString, QString(CurrentAGCRatio)));
    strcpy(CurrentAGCRatio, AGCRatio);
  }

  if (EQOn != CurrentEQOn)
  {
    NewDNREQPanel->setEQOn(EQOn);
    CurrentEQOn = EQOn;
  }

  for (cnt=0; cnt<6; cnt++)
  {
    if (EQLevel[cnt] != CurrentEQLevel[cnt])
    {
      switch (cnt)
      {
        case 0: NewDNREQPanel->setGainBand1(EQLevel[0]); break;
        case 1: NewDNREQPanel->setGainBand2(EQLevel[1]); break;
        case 2: NewDNREQPanel->setGainBand3(EQLevel[2]); break;
        case 3: NewDNREQPanel->setGainBand4(EQLevel[3]); break;
        case 4: NewDNREQPanel->setGainBand5(EQLevel[4]); break;
        case 5: NewDNREQPanel->setGainBand6(EQLevel[5]); break;
      }
      CurrentEQLevel[cnt] = EQLevel[cnt];
    }

    if (EQFrequency[cnt] != CurrentEQFrequency[cnt])
    {
      switch (cnt)
      {
        case 0: NewDNREQPanel->setFrequencyBand1(EQFrequency[0]); break;
        case 1: NewDNREQPanel->setFrequencyBand2(EQFrequency[1]); break;
        case 2: NewDNREQPanel->setFrequencyBand3(EQFrequency[2]); break;
        case 3: NewDNREQPanel->setFrequencyBand4(EQFrequency[3]); break;
        case 4: NewDNREQPanel->setFrequencyBand5(EQFrequency[4]); break;
        case 5: NewDNREQPanel->setFrequencyBand6(EQFrequency[5]); break;
      }
      CurrentEQFrequency[cnt] = EQFrequency[cnt];
    }

    if (EQBandwidth[cnt] != CurrentEQBandwidth[cnt])
    {
      switch (cnt)
      {
        case 0: NewDNREQPanel->setBandwidthBand1(EQBandwidth[0]); break;
        case 1: NewDNREQPanel->setBandwidthBand2(EQBandwidth[1]); break;
        case 2: NewDNREQPanel->setBandwidthBand3(EQBandwidth[2]); break;
        case 3: NewDNREQPanel->setBandwidthBand4(EQBandwidth[3]); break;
        case 4: NewDNREQPanel->setBandwidthBand5(EQBandwidth[4]); break;
        case 5: NewDNREQPanel->setBandwidthBand6(EQBandwidth[5]); break;
      }
      CurrentEQBandwidth[cnt] = EQBandwidth[cnt];
    }

    if (EQType[cnt] != CurrentEQType[cnt])
    {
      switch (cnt)
      {
        case 0: NewDNREQPanel->setTypeBand1(EQType[0]); break;
        case 1: NewDNREQPanel->setTypeBand2(EQType[1]); break;
        case 2: NewDNREQPanel->setTypeBand3(EQType[2]); break;
        case 3: NewDNREQPanel->setTypeBand4(EQType[3]); break;
        case 4: NewDNREQPanel->setTypeBand5(EQType[4]); break;
        case 5: NewDNREQPanel->setTypeBand6(EQType[5]); break;
      }
      CurrentEQType[cnt] = EQType[cnt];
    }

    if (Panorama != CurrentPanorama)
    {
      NewDNRPanorama->setPosition(1023-Panorama);
      CurrentPanorama = Panorama;
    }

    if (ShowModuleParameters != CurrentShowModuleParameters)
    {
      widget_2->setVisible(ShowModuleParameters);
      CurrentShowModuleParameters = ShowModuleParameters;
    }
  }
}

/* for webbrowsing */
//void Browser::slotLoadProgress(int progress)
//{
//	if (progress < 100 && progress > 0)
//	{
//		m_chaseWidget->setAnimated(true);

/*		disconnect(m_stopReload, SIGNAL(triggered()), m_reload, SLOT(trigger()));
      if (m_stopIcon.isNull())
      	m_stopIcon = style()->standardIcon(QStyle::SP_BrowserStop);
      m_stopReload->setIcon(m_stopIcon);
     	connect(m_stopReload, SIGNAL(triggered()), m_stop, SLOT(trigger()));
		m_stopReload->setToolTip(tr("Stop loading the current page"));*/
//   }
//	else
//	{
//		m_chaseWidget->setAnimated(false);

/*		disconnect(m_stopReload, SIGNAL(triggered()), m_stop, SLOT(trigger()));
		m_stopReload->setIcon(m_reloadIcon);
		connect(m_stopReload, SIGNAL(triggered()), m_reload, SLOT(trigger()));
		m_stopReload->setToolTip(tr("Reload the current page"));*/
//	}
//}
