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

#include "DNRDigitalClock.h"
#include "DNRDigitalClockPlugin.h"

#include <QtPlugin>

DigitalClockPlugin::DigitalClockPlugin(QObject *parent)
    : QObject(parent)
{
    initialized = false;
}

void DigitalClockPlugin::initialize(QDesignerFormEditorInterface * /* core */)
{
    if (initialized)
        return;

    initialized = true;
}

bool DigitalClockPlugin::isInitialized() const
{
    return initialized;
}

QWidget *DigitalClockPlugin::createWidget(QWidget *parent)
{
    return new DNRDigitalClock(parent);
}

QString DigitalClockPlugin::name() const
{
    return "DNRDigitalClock";
}

QString DigitalClockPlugin::group() const
{
    return "DNR Widgets";
}

QIcon DigitalClockPlugin::icon() const
{
    return QIcon();
}

QString DigitalClockPlugin::toolTip() const
{
    return "";
}

QString DigitalClockPlugin::whatsThis() const
{
    return "";
}

bool DigitalClockPlugin::isContainer() const
{
    return false;
}

QString DigitalClockPlugin::domXml() const
{
    return "<widget class=\"DNRDigitalClock\" name=\"NewDNRDigitalClock\">\n"
           " <property name=\"geometry\">\n"
           "  <rect>\n"
           "   <x>0</x>\n"
           "   <y>0</y>\n"
           "   <width>100</width>\n"
           "   <height>30</height>\n"
           "  </rect>\n"
           " </property>\n"
           " <property name=\"toolTip\" >\n"
           "  <string>The current time</string>\n"
           " </property>\n"
           " <property name=\"whatsThis\" >\n"
           "  <string>The digital clock widget displays "
           "the current time.</string>\n"
           " </property>\n"
           "</widget>\n";
}

QString DigitalClockPlugin::includeFile() const
{
    return "DNRDigitalClock.h";
}
