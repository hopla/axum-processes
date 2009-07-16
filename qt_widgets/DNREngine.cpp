/****************************************************************************
**
** Copyright (C) 2005-2006 Trolltech ASA. All rights reserved.
**
** This file is part of the example classes of the Qt Toolkit.
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

#include <QtGui>

//#include <math.h>
#include "DNREngine.h"
#include <time.h>

DNREngine::DNREngine(QWidget *parent)
    : DNRWidget(parent)
{
   FNumberOfChannels = 2;
   DebugString = "";

   SignallingThread = new DNREngineThread(parent);
   SignallingThread->setObjectName("NewDNREngineThread");

   setWindowTitle(tr("DNR Engine"));
   resize(50, 50);

   SignallingThread->start(QThread::TimeCriticalPriority);
   //SignallingThread->start(QThread::HighPriority);
   //SignallingThread->start(QThread::LowPriority);
}

QObject *DNREngine::connectionsObject()
{
	return SignallingThread;
}

QObjectList DNREngine::GetReceiverList(const char *signal)
{
	return SignallingThread->GetReceiverList(signal);
}

QStringList DNREngine::GetReceivingSlotsList(const char *signal, const char *receiver)
{
	return SignallingThread->GetReceivingSlotsList(signal, receiver);
}


void DNREngine::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

	 painter.setBrush(Qt::NoBrush);
    painter.setPen(QColor(0,0,0));
    painter.drawRect(0,0,width(),height());
    painter.drawText(2,12,objectName());


    DebugString.sprintf("Time: %2.3f mS = %d Hz (th-pid %d, priority %d)", SignallingThread->LastElapsedTime*1000, (int)(1/SignallingThread->LastElapsedTime), SignallingThread->getProcessID(), SignallingThread->priority());
	 painter.drawText(2,22, DebugString);

    //DebugString.sprintf("%d BytesInBuffer", SignallingThread->cntReceiveBufferTop-SignallingThread->cntReceiveBufferBottom);
    //painter.drawText(2,32, DebugString);
    
    painter.drawText(2,42, SignallingThread->SerialPortDebugMessage);
}


void DNREngine::setNumberOfChannels(int NewNumberOfChannels)
{
	if (FNumberOfChannels != NewNumberOfChannels)
	{
		if ((NewNumberOfChannels>=0) && (NewNumberOfChannels<MAXNUMBEROFCHANNELS))
		{
			FNumberOfChannels = NewNumberOfChannels;
			update();
		}
	}
}

int DNREngine::getNumberOfChannels()
{
	return FNumberOfChannels;
}

void DNREngine::mousePressEvent(QMouseEvent *event)
{
   DNRWidget::mousePressEvent(event);

   if (event->buttons() & Qt::LeftButton)
   {
      update();
   }
}
