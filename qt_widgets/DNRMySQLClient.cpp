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
#include <QtSql>
#include <math.h>
#include "DNRMySQLClient.h"

DNRMySQLClient::DNRMySQLClient(QWidget *parent)
    : DNRWidget(parent)
{
	 DebugString = "";
    StatusMessage = "<EMPTY>";

    MySQLClientThread = new DNRMySQLClientThread(parent);
    MySQLClientThread->setObjectName("NewDNRMySQLClientThread");

    setWindowTitle(tr("MySQL Client"));
    resize(50, 50);

    MySQLClientThread->start(QThread::IdlePriority);

    connect(MySQLClientThread, SIGNAL(SQLErrorMessage(QString)), this, SLOT(doSQLErrorMessage(QString)));
}

QObject *DNRMySQLClient::connectionsObject()
{
	return MySQLClientThread;
}

QObjectList DNRMySQLClient::GetReceiverList(const char *signal)
{
	return MySQLClientThread->GetReceiverList(signal);
}

QStringList DNRMySQLClient::GetReceivingSlotsList(const char *signal, const char *receiver)
{
	return MySQLClientThread->GetReceivingSlotsList(signal, receiver);
}

void DNRMySQLClient::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

	 painter.setBrush(Qt::NoBrush);
    painter.setPen(QColor(0,0,0));
    painter.drawRect(0,0,width(),height());
    painter.drawText(2,12,objectName());

    DebugString.sprintf("Time: %2.3f mS = %d Hz (th-pid %d, priority %d)", MySQLClientThread->LastElapsedTime*1000, (int)(1/MySQLClientThread->LastElapsedTime), MySQLClientThread->getProcessID(), MySQLClientThread->priority());
	 painter.drawText(2,22, DebugString);

    painter.drawText(2,32, StatusMessage);
}

void DNRMySQLClient::mousePressEvent(QMouseEvent *event)
{
   DNRWidget::mousePressEvent(event);

   if (event->buttons() & Qt::LeftButton)
   {
      update();
   }
}

void DNRMySQLClient::doSQLErrorMessage(QString Message)
{
   StatusMessage = Message;
   update();
}
