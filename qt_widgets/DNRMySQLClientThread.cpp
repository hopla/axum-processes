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

#include "DNRMySQLClientThread.h"
#include "qstringlist.h"
#include <private/qthread_p.h>

#ifdef Q_OS_WIN32
#include <windows.h> // for QueryPerformanceCounter
#endif

class DNRMySQLClientThreadPrivate : public QThreadPrivate
{
    Q_DECLARE_PUBLIC(DNRMySQLClientThread)
public:
	DNRMySQLClientThreadPrivate()
	{}
};

DNRMySQLClientThread::DNRMySQLClientThread(QObject *parent): QThread(parent)
{
   registerDNRTypes();
   ProcessID = 0;
   LastElapsedTime = 0;
   abort = false;
}

DNRMySQLClientThread::~DNRMySQLClientThread()
{
   abort = true;
	wait();
}

QObjectList DNRMySQLClientThread::GetReceiverList(const char *signal)
{
	return d_func()->receiverList(signal);
}

QStringList DNRMySQLClientThread::GetReceivingSlotsList(const char *signal, const char *receiver)
{
	return d_func()->receivingSlotsList(signal, receiver);
}

void DNRMySQLClientThread::run()
{
	//counter for thread
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

   double previousNumberOfSeconds = newNumberOfSeconds;
   double previousNumberOfSeconds_ms = newNumberOfSeconds;
   double ElapsedTime;

#ifndef Q_OS_WIN32
   if (QSqlDatabase::drivers().isEmpty())
   {
      emit SQLErrorMessage(tr("No database drivers found\nThis demo requires at least one Qt database driver. "
            "Please check the documentation how to build the "
            "Qt SQL plugins."));
   }
   emit SQLErrorMessage(tr("Data base driver found"));

   QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL", objectName());
   db.setDatabaseName("hardware_status");
   db.setHostName("localhost");
   db.setPort(3306);
   if (!db.open("root", "root"))
   {
      //err = db.lastError();
      emit SQLErrorMessage(db.lastError().text());
      QSqlDatabase::removeDatabase(objectName());
      return;
   }
   emit SQLErrorMessage(tr("Starting thread-loop"));

   QSqlTableModel *configurationTable = new QSqlTableModel(0, db);
   QSqlTableModel *slot0Table = new QSqlTableModel(0, db);

   configurationTable->setTable("configuration_rack_axum");
   configurationTable->setEditStrategy(QSqlTableModel::OnFieldChange);
   configurationTable->select();

   slot0Table->setTable(configurationTable->record(0).value("TableNameCurrentConfiguration").toString());
   slot0Table->setEditStrategy(QSqlTableModel::OnFieldChange);
   slot0Table->select();
   
   ProcessID = getpid();

   forever
   {
		if (abort)
      {
         delete configurationTable;
         delete slot0Table;
         QSqlDatabase::removeDatabase(objectName());
			return;
      }

#ifdef Q_OS_WIN32
		QueryPerformanceCounter(&newTime);
		newNumberOfSeconds = (double)newTime.QuadPart/freq.QuadPart;
#else
	   clock_gettime(CLOCK_MONOTONIC, &newTime);
   	newNumberOfSeconds = newTime.tv_sec+((double)newTime.tv_nsec/1000000000);
#endif

      LastElapsedTime = newNumberOfSeconds-previousNumberOfSeconds;
      previousNumberOfSeconds = newNumberOfSeconds;

//Timers
		//Timers for reading database
      ElapsedTime = newNumberOfSeconds-previousNumberOfSeconds_ms;
      if (ElapsedTime >= 0.033)
      {
         //Do something
         configurationTable->select();
         int cntRow = 0;
         if (configurationTable->record(cntRow).value("HardwareSet").toInt() == 1)
         {
            slot0Table->select();
            double GPI1Value = slot0Table->record(cntRow).value("GPI1").toInt();
            if (GPI1Value != 0)
            {
               GPI1Value = POSITION_RESOLUTION;
            }
            emit HardwareStatusChange(0, GPI1Value);

            QSqlRecord record = slot0Table->record(cntRow);
            record.setValue("HardwareSet", (int)0);
            slot0Table->setRecord(cntRow, record);
         }
         previousNumberOfSeconds_ms = newNumberOfSeconds;
      }
      msleep(10);
   }
#endif
}


int DNRMySQLClientThread::getProcessID()
{
   return ProcessID;
}
