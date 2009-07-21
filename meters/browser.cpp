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

Browser::Browser(QWidget *parent)
    : QWidget(parent)
{
   setupUi(this);

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

	startTimer(10);

	MeterData[0] = -50;
	MeterData[1] = -50;
	MeterData[2] = -50;
	MeterData[3] = -50;
}

Browser::~Browser()
{
}

void Browser::timerEvent(QTimerEvent *Event)
{
	cntSecond++;
	MeterRelease();
  return;
  Event = NULL;
}

void Browser::MeterRelease()
{
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

   if (NewDNRPPMMeter->FdBPosition>-50)
   {     
		if (MeterData[0] < NewDNRPPMMeter->FdBPosition)
      {         
			NewDNRPPMMeter->FdBPosition-=0.15;      
//			NewDNRPPMMeter->repaint();
			NewDNRPPMMeter->update();
		}
	}

   if (NewDNRPPMMeter_2->FdBPosition>-50)
   {     
		if (MeterData[1] < NewDNRPPMMeter_2->FdBPosition)
      {         
			NewDNRPPMMeter_2->FdBPosition-=0.15;      
//			NewDNRPPMMeter_2->repaint();
			NewDNRPPMMeter_2->update();
		}
	}   

   if (NewDNRPPMMeter_3->FdBPosition>-50)
   {     
		if (MeterData[2] < NewDNRPPMMeter_3->FdBPosition)
      {         
			NewDNRPPMMeter_3->FdBPosition-=0.15;      
			NewDNRPPMMeter_3->update();
//			NewDNRPPMMeter_3->repaint();
		}   
	}

   if (NewDNRPPMMeter_4->FdBPosition>-50)
   {     
		if (MeterData[3] < NewDNRPPMMeter_4->FdBPosition)
      {         
			NewDNRPPMMeter_4->FdBPosition-=0.15;      
			NewDNRPPMMeter_4->update();
//			NewDNRPPMMeter_4->repaint();
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


