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

#include "DNREQPanel.h"
#include "DNREQPanelPlugin.h"

#include <QtPlugin>

EQPanelPlugin::EQPanelPlugin(QObject *parent)
    : QObject(parent)
{
    initialized = false;
}

void EQPanelPlugin::initialize(QDesignerFormEditorInterface * /* core */)
{
    if (initialized)
        return;

    initialized = true;
}

bool EQPanelPlugin::isInitialized() const
{
    return initialized;
}

QWidget *EQPanelPlugin::createWidget(QWidget *parent)
{
    return new DNREQPanel(parent);
}

QString EQPanelPlugin::name() const
{
    return "DNREQPanel";
}

QString EQPanelPlugin::group() const
{
    return "DNR Widgets";
}

QIcon EQPanelPlugin::icon() const
{
    return QIcon();
}

QString EQPanelPlugin::toolTip() const
{
    return "";
}

QString EQPanelPlugin::whatsThis() const
{
    return "";
}

bool EQPanelPlugin::isContainer() const
{
    return false;
}

QString EQPanelPlugin::domXml() const
{
    return "<widget class=\"DNREQPanel\" name=\"NewDNREQPanel\">\n"
           " <property name=\"geometry\">\n"
           "  <rect>\n"
           "   <x>0</x>\n"
           "   <y>0</y>\n"
           "   <width>200</width>\n"
           "   <height>125</height>\n"
           "  </rect>\n"
           " </property>\n"
           " <property name=\"toolTip\" >\n"
           "  <string>The EQ Panel</string>\n"
           " </property>\n"
           " <property name=\"whatsThis\" >\n"
           "  <string>The EQ Panel widget displays </string>\n"
           " </property>\n"
           "</widget>\n";
}

QString EQPanelPlugin::includeFile() const
{
    return "DNREQPanel.h";
}
