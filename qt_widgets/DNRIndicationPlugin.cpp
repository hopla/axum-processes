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

#include "DNRIndication.h"
#include "DNRIndicationPlugin.h"

#include <QtPlugin>

IndicationPlugin::IndicationPlugin(QObject *parent)
    : QObject(parent)
{
    initialized = false;
}

void IndicationPlugin::initialize(QDesignerFormEditorInterface * /* core */)
{
    if (initialized)
        return;

    initialized = true;
}

bool IndicationPlugin::isInitialized() const
{
    return initialized;
}

QWidget *IndicationPlugin::createWidget(QWidget *parent)
{
    return new DNRIndication(parent);
}

QString IndicationPlugin::name() const
{
    return "DNRIndication";
}

QString IndicationPlugin::group() const
{
    return "DNR Widgets";
}

QIcon IndicationPlugin::icon() const
{
    return QIcon();
}

QString IndicationPlugin::toolTip() const
{
    return "";
}

QString IndicationPlugin::whatsThis() const
{
    return "";
}

bool IndicationPlugin::isContainer() const
{
    return false;
}

QString IndicationPlugin::domXml() const
{
    return "<widget class=\"DNRIndication\" name=\"NewDNRIndication\">\n"
           " <property name=\"geometry\">\n"
           "  <rect>\n"
           "   <x>0</x>\n"
           "   <y>0</y>\n"
           "   <width>16</width>\n"
           "   <height>16</height>\n"
           "  </rect>\n"
           " </property>\n"
           " <property name=\"toolTip\" >\n"
           "  <string>The indication</string>\n"
           " </property>\n"
           " <property name=\"whatsThis\" >\n"
           "  <string>The indication widget displays </string>\n"
           " </property>\n"
           "</widget>\n";
}

QString IndicationPlugin::includeFile() const
{
    return "DNRIndication.h";
}
