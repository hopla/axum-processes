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

#ifndef DNRTERMINAL_H
#define DNRTERMINAL_H

#include "DNRDefines.h"
#include "DNRWidget.h"
#include "DNRTerminalThread.h"
#include <QMenu>
#include <QtDesigner/QDesignerExportWidget>

class QDESIGNER_WIDGET_EXPORT DNRTerminal : public DNRWidget
{
    Q_OBJECT

public:
    DNRTerminal(QWidget *parent = 0);

	 QObject *connectionsObject();
	 QObjectList GetReceiverList(const char *signal);
	 QStringList GetReceivingSlotsList(const char *signal, const char *receiver);

	 QString DebugString;

protected:
    void paintEvent(QPaintEvent *event);
    void mousePressEvent(QMouseEvent *event);

    DNRTerminalThread *SignallingThread;

};

#endif
