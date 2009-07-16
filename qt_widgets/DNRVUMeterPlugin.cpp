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

#include "DNRVUMeter.h"
#include "DNRVUMeterPlugin.h"

#include <QtPlugin>

VUMeterPlugin::VUMeterPlugin(QObject *parent)
    : QObject(parent)
{
    initialized = false;
}

void VUMeterPlugin::initialize(QDesignerFormEditorInterface * /* core */)
{
    if (initialized)
        return;

    initialized = true;
}

bool VUMeterPlugin::isInitialized() const
{
    return initialized;
}

QWidget *VUMeterPlugin::createWidget(QWidget *parent)
{
    return new DNRVUMeter(parent);
}

QString VUMeterPlugin::name() const
{
    return "DNRVUMeter";
}

QString VUMeterPlugin::group() const
{
    return "DNR Widgets";
}

QIcon VUMeterPlugin::icon() const
{
    return QIcon();
}

QString VUMeterPlugin::toolTip() const
{
    return "";
}

QString VUMeterPlugin::whatsThis() const
{
    return "";
}

bool VUMeterPlugin::isContainer() const
{
    return false;
}

QString VUMeterPlugin::domXml() const
{
    return "<widget class=\"DNRVUMeter\" name=\"NewDNRVUMeter\">\n"
           " <property name=\"geometry\">\n"
           "  <rect>\n"
           "   <x>0</x>\n"
           "   <y>0</y>\n"
           "   <width>260</width>\n"
           "   <height>136</height>\n"
           "  </rect>\n"
           " </property>\n"
           " <property name=\"toolTip\" >\n"
           "  <string>The VU Meter</string>\n"
           " </property>\n"
           " <property name=\"whatsThis\" >\n"
           "  <string>The VU Meter widget displays </string>\n"
           " </property>\n"
           "</widget>\n";
}

QString VUMeterPlugin::includeFile() const
{
    return "DNRVUMeter.h";
}
