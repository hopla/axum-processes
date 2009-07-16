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

#include "DNRAnalogClock.h"
#include "DNRAnalogClockPlugin.h"

#include <QtPlugin>

AnalogClockPlugin::AnalogClockPlugin(QObject *parent)
    : QObject(parent)
{
    initialized = false;
}

void AnalogClockPlugin::initialize(QDesignerFormEditorInterface * /* core */)
{
    if (initialized)
        return;

    initialized = true;
}

bool AnalogClockPlugin::isInitialized() const
{
    return initialized;
}

QWidget *AnalogClockPlugin::createWidget(QWidget *parent)
{
    return new DNRAnalogClock(parent);
}

QString AnalogClockPlugin::name() const
{
    return "DNRAnalogClock";
}

QString AnalogClockPlugin::group() const
{
    return "DNR Widgets";
}

QIcon AnalogClockPlugin::icon() const
{
    return QIcon();
}

QString AnalogClockPlugin::toolTip() const
{
    return "";
}

QString AnalogClockPlugin::whatsThis() const
{
    return "";
}

bool AnalogClockPlugin::isContainer() const
{
    return false;
}

QString AnalogClockPlugin::domXml() const
{
    return "<widget class=\"DNRAnalogClock\" name=\"NewDNRAnalogClock\">\n"
           " <property name=\"geometry\">\n"
           "  <rect>\n"
           "   <x>0</x>\n"
           "   <y>0</y>\n"
           "   <width>100</width>\n"
           "   <height>100</height>\n"
           "  </rect>\n"
           " </property>\n"
           " <property name=\"toolTip\" >\n"
           "  <string>The current time</string>\n"
           " </property>\n"
           " <property name=\"whatsThis\" >\n"
           "  <string>The analog clock widget displays "
           "the current time.</string>\n"
           " </property>\n"
           "</widget>\n";
}

QString AnalogClockPlugin::includeFile() const
{
    return "DNRAnalogClock.h";
}

