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

#include "DNRPPMMeter.h"
#include "DNRPPMMeterPlugin.h"

#include <QtPlugin>

PPMMeterPlugin::PPMMeterPlugin(QObject *parent)
    : QObject(parent)
{
    initialized = false;
}

void PPMMeterPlugin::initialize(QDesignerFormEditorInterface * /* core */)
{
    if (initialized)
        return;

    initialized = true;
}

bool PPMMeterPlugin::isInitialized() const
{
    return initialized;
}

QWidget *PPMMeterPlugin::createWidget(QWidget *parent)
{
    return new DNRPPMMeter(parent);
}

QString PPMMeterPlugin::name() const
{
    return "DNRPPMMeter";
}

QString PPMMeterPlugin::group() const
{
    return "DNR Widgets";
}

QIcon PPMMeterPlugin::icon() const
{
    return QIcon();
}

QString PPMMeterPlugin::toolTip() const
{
    return "";
}

QString PPMMeterPlugin::whatsThis() const
{
    return "";
}

bool PPMMeterPlugin::isContainer() const
{
    return false;
}

QString PPMMeterPlugin::domXml() const
{
    return "<widget class=\"DNRPPMMeter\" name=\"NewDNRPPMMeter\">\n"
           " <property name=\"geometry\">\n"
           "  <rect>\n"
           "   <x>0</x>\n"
           "   <y>0</y>\n"
           "   <width>16</width>\n"
           "   <height>200</height>\n"
           "  </rect>\n"
           " </property>\n"
           " <property name=\"toolTip\" >\n"
           "  <string>The PPM Meter</string>\n"
           " </property>\n"
           " <property name=\"whatsThis\" >\n"
           "  <string>The PPM Meter widget displays </string>\n"
           " </property>\n"
           "</widget>\n";
}

QString PPMMeterPlugin::includeFile() const
{
    return "DNRPPMMeter.h";
}
