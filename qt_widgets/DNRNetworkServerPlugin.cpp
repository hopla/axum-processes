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

#include "DNRNetworkServer.h"
#include "DNRNetworkServerPlugin.h"

#include <QtPlugin>

NetworkServerPlugin::NetworkServerPlugin(QObject *parent)
    : QObject(parent)
{
    initialized = false;
}

void NetworkServerPlugin::initialize(QDesignerFormEditorInterface * /* core */)
{
    if (initialized)
        return;

    initialized = true;
}

bool NetworkServerPlugin::isInitialized() const
{
    return initialized;
}

QWidget *NetworkServerPlugin::createWidget(QWidget *parent)
{
    return new DNRNetworkServer(parent);
}

QString NetworkServerPlugin::name() const
{
    return "DNRNetworkServer";
}

QString NetworkServerPlugin::group() const
{
    return "DNR Widgets";
}

QIcon NetworkServerPlugin::icon() const
{
    return QIcon();
}

QString NetworkServerPlugin::toolTip() const
{
    return "";
}

QString NetworkServerPlugin::whatsThis() const
{
    return "";
}

bool NetworkServerPlugin::isContainer() const
{
    return false;
}

QString NetworkServerPlugin::domXml() const
{
    return "<widget class=\"DNRNetworkServer\" name=\"NewDNRNetworkServer\">\n"
           " <property name=\"geometry\">\n"
           "  <rect>\n"
           "   <x>0</x>\n"
           "   <y>0</y>\n"
           "   <width>200</width>\n"
           "   <height>20</height>\n"
           "  </rect>\n"
           " </property>\n"
           " <property name=\"toolTip\" >\n"
           "  <string>The Network Server</string>\n"
           " </property>\n"
           " <property name=\"whatsThis\" >\n"
           "  <string>The NetworkServer widget displays </string>\n"
           " </property>\n"
           "</widget>\n";
}

QString NetworkServerPlugin::includeFile() const
{
    return "DNRNetworkServer.h";
}
