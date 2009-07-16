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
#include "DNRTerminal.h"
#include <time.h>

DNRTerminal::DNRTerminal(QWidget *parent)
    : DNRWidget(parent)
{
   DebugString = "";

   SignallingThread = new DNRTerminalThread(parent);
   SignallingThread->setObjectName("NewDNRTerminalThread");

   setWindowTitle(tr("DNR Terminal"));
   resize(50, 50);

   SignallingThread->start();
}

QObject *DNRTerminal::connectionsObject()
{
	return SignallingThread;
}

QObjectList DNRTerminal::GetReceiverList(const char *signal)
{
	return SignallingThread->GetReceiverList(signal);
}

QStringList DNRTerminal::GetReceivingSlotsList(const char *signal, const char *receiver)
{
	return SignallingThread->GetReceivingSlotsList(signal, receiver);
}


void DNRTerminal::paintEvent(QPaintEvent *)
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

void DNRTerminal::mousePressEvent(QMouseEvent *event)
{
   DNRWidget::mousePressEvent(event);

   if (event->buttons() & Qt::LeftButton)
   {
      update();
   }
}
