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
#include <QtNetwork>
#include <math.h>
#include "DNRNetworkServer.h"

DNRNetworkServer::DNRNetworkServer(QWidget *parent)
    : QWidget(parent)
{
	cntSlot = 0;
	cntByteInSlot = 0;


	tcpServer = new QTcpServer(this);
	tcpServer->setMaxPendingConnections(MAX_NUMBER_OF_CONNECTIONS_TO_SERVER);
   if (!tcpServer->listen())
   {
    	QMessageBox::critical(this, tr("DNR Network Server"), tr("Unable to start the server: %1.").arg(tcpServer->errorString()));
   }

    DebugMessage = tr("The server is running on port %1.").arg(tcpServer->serverPort());

    connect(tcpServer, SIGNAL(newConnection()), this, SLOT(doConnected()));

    setWindowTitle(tr("Network Server"));
    resize(200, 20);
}

void DNRNetworkServer::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

	 painter.setBrush(Qt::NoBrush);
    painter.setPen(QColor(0,0,0));
    painter.drawRect(0,0,width(),height());
    painter.drawText(2,12,objectName());

    painter.drawText(2,22, DebugMessage);
}


void DNRNetworkServer::doReadTCPSocket()
{
	QTcpSocket *clientConnection = (QTcpSocket *)sender();
	QByteArray Temp = clientConnection->readAll();
	unsigned char *ByteData = (unsigned char *)Temp.data();
	int count = Temp.count();

	for (int cntByte=0; cntByte<count; cntByte++)
	{
		if ((ByteData[cntByte]&0x81) == 0x80)
		{
			cntByteInSlot = 0;
		}

		if (cntByteInSlot<16)
		{
			MessageSlot[cntByteInSlot++] = ByteData[cntByte];
		}

		if ((ByteData[cntByte]&0x81) == 0x81)
		{	//End of a message
			if (MessageSlot[0] == 0x80)
			{
				int ChannelNr = (MessageSlot[2]<<7) | MessageSlot[1];
				int PositionData = (MessageSlot[4]<<7) | MessageSlot[3];
            emit FaderPositionChanged(ChannelNr,PositionData);
       	}
		}
	}
}

void DNRNetworkServer::doFaderPositionChange(int_number ChannelNr, double_position Position)
{
	unsigned char MessageData[16];
	int int_position = Position;

	MessageData[0] = 0x80;
	MessageData[1] = ChannelNr&0x7F;
	MessageData[2] = (ChannelNr>>7)&0x7F;
	MessageData[3] = int_position&0x7F;
	MessageData[4] = (int_position>>7)&0x7F;
	MessageData[5] = MessageData[0] | 0x01;

	for (int cntSocket=0; cntSocket<clientConnectionList.count(); cntSocket++)
	{
		QTcpSocket *clientConnection = clientConnectionList.at(cntSocket);
		if (clientConnection->state() == QAbstractSocket::ConnectedState)
		{
			clientConnection->write((char *)MessageData, 6);
		}
	}
}


void DNRNetworkServer::doConnected()
{
	QTcpSocket *clientConnection = tcpServer->nextPendingConnection();
	clientConnectionList.append(clientConnection);
	connect(clientConnection, SIGNAL(disconnected()), clientConnection, SLOT(deleteLater()));
	connect(clientConnection, SIGNAL(disconnected()), this, SLOT(RemoveFromClientConnectionList()));
	connect(clientConnection, SIGNAL(readyRead()), this, SLOT(doReadTCPSocket()));
}

void DNRNetworkServer::RemoveFromClientConnectionList()
{
	int ClientIndex = clientConnectionList.indexOf((QTcpSocket *)sender());
	if (ClientIndex != -1)
	{
		clientConnectionList.removeAt(ClientIndex);
	}
}
