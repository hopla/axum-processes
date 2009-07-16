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

#ifndef DNRMYSQLCLIENTTHREAD_H
#define DNRMYSQLCLIENTTHREAD_H

#include "DNRDefines.h"
#include <QThread>
#include <QtSql>

extern void registerDNRTypes();

class DNRMySQLClientThreadPrivate;

class Q_DECL_EXPORT DNRMySQLClientThread : public QThread
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(DNRMySQLClientThread)

public:
    DNRMySQLClientThread(QObject *parent = 0);
    ~DNRMySQLClientThread();

    double LastElapsedTime;

	 QObjectList GetReceiverList(const char *signal);
	 QStringList GetReceivingSlotsList(const char *signal, const char *receiver);
    
    int ProcessID;
    int getProcessID();

public slots:

signals:
   void SQLErrorMessage(QString Message);
   void HardwareStatusChange(int_number ChannelNr, double_position Position);

protected:
	void run();
	bool abort;

};

#endif
