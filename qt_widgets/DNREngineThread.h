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

#ifndef DNRENGINETHREAD_H
#define DNRENGINETHREAD_H

#include "DNRDefines.h"
#include <QThread>

#define MAXNUMBEROFCHANNELS 1024

extern void registerDNRTypes();

class DNREngineThreadPrivate;

typedef struct
{
	unsigned char CUEActive:1;
	unsigned char CommActive:1;

	unsigned char Redlight1Active:1;
	unsigned char Redlight2Active:1;
	unsigned char Redlight3Active:1;
	unsigned char Redlight4Active:1;
	unsigned char Redlight5Active:1;
	unsigned char Redlight6Active:1;
	unsigned char Redlight7Active:1;
	unsigned char Redlight8Active:1;
   unsigned char CRMMuteActive:1;
   unsigned char CUEMuteActive:1;
   unsigned char Studio1MuteActive:1;
   unsigned char Studio2MuteActive:1;
   unsigned char Studio3MuteActive:1;
   unsigned char Studio4MuteActive:1;
   unsigned char Studio5MuteActive:1;
   unsigned char Studio6MuteActive:1;
   unsigned char Studio7MuteActive:1;
   unsigned char Studio8MuteActive:1;
   unsigned char RelatedOutput[9][3];
} SIGNALLING_STRUCT;

typedef struct EQBandStruct
{
	double Level;
	double Frequency;
	double Bandwith;
	int Type;
};

typedef struct ModuleSettingsStruct
{
	unsigned char Redlight1:1;
};

typedef struct ModuleDataStruct
{
	double FaderPosition;
	EQBandStruct EQBand1;
	EQBandStruct EQBand2;
	EQBandStruct EQBand3;
	EQBandStruct EQBand4;

	ModuleSettingsStruct Settings;
};

typedef struct AxumDataStruct
{
	 ModuleDataStruct ModuleData[MAXNUMBEROFCHANNELS];
};

class Q_DECL_EXPORT DNREngineThread : public QThread
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(DNREngineThread)

public:
    DNREngineThread(QObject *parent = 0);
    ~DNREngineThread();

    AxumDataStruct AxumData;
    AxumDataStruct PreviousAxumData;

    double LastElapsedTime;

	 QObjectList GetReceiverList(const char *signal);
	 QStringList GetReceivingSlotsList(const char *signal, const char *receiver);
    
    int ProcessID;
    int getProcessID();
    
    unsigned char ReceiveBuffer[4096];
    int cntReceiveBuffer;
    
    QString SerialPortDebugMessage;

public slots:
	void doBand1EQPositionChange(int_number ChannelNr, double_position Position);
	void doBand2EQPositionChange(int_number ChannelNr, double_position Position);
	void doBand3EQPositionChange(int_number ChannelNr, double_position Position);
	void doBand4EQPositionChange(int_number ChannelNr, double_position Position);
	void doFaderChange(int_number ChannelNr, double_position Position);

	void doRedlight1SettingChange(int_number ChannelNr, double_position Position);

signals:
	void FaderLevelChanged(int_number ChannelNr, double_position Position);
	void EQBand1LevelChanged(int_number ChannelNr, double_position Position);
	void EQBand2LevelChanged(int_number ChannelNr, double_position Position);
	void EQBand3LevelChanged(int_number ChannelNr, double_position Position);
	void EQBand4LevelChanged(int_number ChannelNr, double_position Position);
	void TimerTick(char_none unused);
	void MeterChange(double_db);
	void Redlight1Changed(double_position Position);

   
protected:
	void run();
	bool abort;
   
   

};

#endif
