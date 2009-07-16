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

#include "DNRMySQLClient.h"
#include "DNRMySQLClientPlugin.h"

#include <QtPlugin>

MySQLClientPlugin::MySQLClientPlugin(QObject *parent)
    : QObject(parent)
{
    initialized = false;
}

void MySQLClientPlugin::initialize(QDesignerFormEditorInterface * /* core */)
{
    if (initialized)
        return;

    initialized = true;
}

bool MySQLClientPlugin::isInitialized() const
{
    return initialized;
}

QWidget *MySQLClientPlugin::createWidget(QWidget *parent)
{
    return new DNRMySQLClient(parent);
}

QString MySQLClientPlugin::name() const
{
    return "DNRMySQLClient";
}

QString MySQLClientPlugin::group() const
{
    return "DNR Widgets";
}

QIcon MySQLClientPlugin::icon() const
{
    return QIcon();
}

QString MySQLClientPlugin::toolTip() const
{
    return "";
}

QString MySQLClientPlugin::whatsThis() const
{
    return "";
}

bool MySQLClientPlugin::isContainer() const
{
    return false;
}

QString MySQLClientPlugin::domXml() const
{
    return "<widget class=\"DNRMySQLClient\" name=\"NewDNRMySQLClient\">\n"
           " <property name=\"geometry\">\n"
           "  <rect>\n"
           "   <x>0</x>\n"
           "   <y>0</y>\n"
           "   <width>50</width>\n"
           "   <height>50</height>\n"
           "  </rect>\n"
           " </property>\n"
           " <property name=\"toolTip\" >\n"
           "  <string>The MySQL Client </string>\n"
           " </property>\n"
           " <property name=\"whatsThis\" >\n"
           "  <string>The MySQLClient widget displays </string>\n"
           " </property>\n"
           "</widget>\n";
}

QString MySQLClientPlugin::includeFile() const
{
    return "DNRMySQLClient.h";
}
