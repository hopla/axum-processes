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

#ifndef DNRNETWORKSERVER_H
#define DNRNETWORKSERVER_H

#include "DNRDefines.h"
#include <QWidget>
#include <QtDesigner/QDesignerExportWidget>

class QTcpServer;
class QTcpSocket;

class QDESIGNER_WIDGET_EXPORT DNRNetworkServer : public QWidget
{
    Q_OBJECT
//    Q_PROPERTY(int NumberOfChannels READ getNumberOfChannels WRITE setNumberOfChannels);
public:
    DNRNetworkServer(QWidget *parent = 0);

private slots:
	void doReadTCPSocket();
	void doConnected();
	void RemoveFromClientConnectionList();

signals:
	void FaderPositionChanged(int_number ChannelNr, double_position Position);

public slots:
	void doFaderPositionChange(int_number ChannelNr, double_position Position);

protected:
    void paintEvent(QPaintEvent *event);
    QTcpServer *tcpServer;
    QList<QTcpSocket *> clientConnectionList;

    unsigned char MessageSlot[16];
    char cntSlot;
    char cntByteInSlot;

    QString DebugMessage;

};

#endif
