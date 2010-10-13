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

  NewDNRImageTopOnAir->setVisible(false);
  NewDNRImageBottomOnAir->setVisible(false);
  NewDNRImageTopOffAir->setVisible(true);
  NewDNRImageBottomOffAir->setVisible(true);

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

	startTimer(30);

	for (cnt=0; cnt<8; cnt++)
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

  OnAirState = 0;
  CurrentOnAirState = 0;
  for (cnt=0; cnt<8; cnt++)
  {
    RedlightState[cnt] = 0;
    CurrentRedlightState[cnt] = 0;
  }

  CountDown = 0;
  CurrentCountDown = 0;

  LinkStatus = 0;
  CurrentLinkStatus = 0;

  ProgramEndTimeEnabled = false;
  CurrentProgramEndTimeEnabled = false;
  ProgramEndMinute = 0;
  CurrentProgramEndMinute = 0;
  ProgramEndSecond = 0;
  CurrentProgramEndSecond = 0;

  CountDownSeconds = 0;
  CurrentCountDownSeconds = 0;

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
}

Browser::~Browser()
{
}

extern char CheckLinkStatus();

void Browser::timerEvent(QTimerEvent *Event)
{

	cntSecond++;
	MeterRelease();

  if ((LinkStatus = CheckLinkStatus()) != -1)
  {
    if (CurrentLinkStatus != LinkStatus)
    {
      NewDNRImageNoLink->setVisible(!LinkStatus);
      CurrentLinkStatus = LinkStatus;

      log_write("Link status change: %s", LinkStatus ? ("Up") : ("Down"));
    }
  }

  return;
  Event = NULL;
}

void Browser::MeterRelease()
{
  float Difference;
  int cnt;
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
  #define RELEASE_STEP 0.45
  #define PHASE_STEPSIZE 0.0075*4
  qt_mutex.lock();

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

  if ((NewDNRPPMMeter->FdBPosition>-50) || (MeterData[0]>-50))
  {
    Difference = MeterData[0]-NewDNRPPMMeter->FdBPosition;

    if (Difference < -RELEASE_STEP)
    {
      Difference = -RELEASE_STEP;
		}

    if (Difference != 0)
    {
      NewDNRPPMMeter->FdBPosition += Difference;
			NewDNRPPMMeter->update();
    }
	}
  if ((NewDNRPPMMeter_2->FdBPosition>-50) || (MeterData[1]>-50))
  {
    Difference = MeterData[1]-NewDNRPPMMeter_2->FdBPosition;

    if (Difference < -RELEASE_STEP)
    {
      Difference = -RELEASE_STEP;
		}

    if (Difference != 0)
    {
      NewDNRPPMMeter_2->FdBPosition += Difference;
			NewDNRPPMMeter_2->update();
    }
	}

  if ((NewDNRPPMMeter_3->FdBPosition>-50) || (MeterData[2]>-50))
  {
    Difference = MeterData[2]-NewDNRPPMMeter_3->FdBPosition;

    if (Difference < -RELEASE_STEP)
    {
      Difference = -RELEASE_STEP;
		}

    if (Difference != 0)
    {
      NewDNRPPMMeter_3->FdBPosition += Difference;
			NewDNRPPMMeter_3->update();
    }
	}

  if ((NewDNRPPMMeter_4->FdBPosition>-50) || (MeterData[3]>-50))
  {
    Difference = MeterData[3]-NewDNRPPMMeter_4->FdBPosition;

    if (Difference < -RELEASE_STEP)
    {
      Difference = -RELEASE_STEP;
		}

    if (Difference != 0)
    {
      NewDNRPPMMeter_4->FdBPosition += Difference;
			NewDNRPPMMeter_4->update();
    }
	}

  if ((NewDNRPPMMeter_5->FdBPosition>-50) || (MeterData[4]>-50))
  {
    Difference = MeterData[4]-NewDNRPPMMeter_5->FdBPosition;

    if (Difference < -RELEASE_STEP)
    {
      Difference = -RELEASE_STEP;
		}

    if (Difference != 0)
    {
      NewDNRPPMMeter_5->FdBPosition += Difference;
			NewDNRPPMMeter_5->update();
    }
	}
  if ((NewDNRPPMMeter_6->FdBPosition>-50) || (MeterData[5]>-50))
  {
    Difference = MeterData[5]-NewDNRPPMMeter_6->FdBPosition;

    if (Difference < -RELEASE_STEP)
    {
      Difference = -RELEASE_STEP;
		}

    if (Difference != 0)
    {
      NewDNRPPMMeter_6->FdBPosition += Difference;
			NewDNRPPMMeter_6->update();
    }
	}
  if ((NewDNRPPMMeter_7->FdBPosition>-50) || (MeterData[6]>-50))
  {
    Difference = MeterData[6]-NewDNRPPMMeter_7->FdBPosition;

    if (Difference < -RELEASE_STEP)
    {
      Difference = -RELEASE_STEP;
		}

    if (Difference != 0)
    {
      NewDNRPPMMeter_7->FdBPosition += Difference;
			NewDNRPPMMeter_7->update();
    }
	}
  if ((NewDNRPPMMeter_8->FdBPosition>-50) || (MeterData[7]>-50))
  {
    Difference = MeterData[7]-NewDNRPPMMeter_8->FdBPosition;

    if (Difference < -RELEASE_STEP)
    {
      Difference = -RELEASE_STEP;
		}

    if (Difference != 0)
    {
      NewDNRPPMMeter_8->FdBPosition += Difference;
			NewDNRPPMMeter_8->update();
    }
	}

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

  OnAirState = false;
  for (cnt=0; cnt<8; cnt++)
  {
    if (RedlightState[cnt])
    {
      OnAirState = true;
    }
  }

  if (OnAirState != CurrentOnAirState)
  {
    NewDNRImageTopOnAir->setVisible(OnAirState);
    NewDNRImageBottomOnAir->setVisible(OnAirState);
    NewDNRImageTopOffAir->setVisible(!OnAirState);
    NewDNRImageBottomOffAir->setVisible(!OnAirState);

    CurrentOnAirState = OnAirState;
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
    }
  }

  qt_mutex.unlock();
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
