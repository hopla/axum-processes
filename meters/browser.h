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

	 double MeterData[4];
   char Label[4][9];
	 double previousNumberOfSeconds;

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
