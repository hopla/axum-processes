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

#include "DNRWidget.h"
#include "DNRWidgetPlugin.h"

#include <QtPlugin>

WidgetPlugin::WidgetPlugin(QObject *parent)
    : QObject(parent)
{
    initialized = false;
}

void WidgetPlugin::initialize(QDesignerFormEditorInterface * /* core */)
{
    if (initialized)
        return;

    initialized = true;
}

bool WidgetPlugin::isInitialized() const
{
    return initialized;
}

QWidget *WidgetPlugin::createWidget(QWidget *parent)
{
    return new DNRWidget(parent);
}

QString WidgetPlugin::name() const
{
    return "DNRWidget";
}

QString WidgetPlugin::group() const
{
    return "DNR Widgets";
}

QIcon WidgetPlugin::icon() const
{
    return QIcon();
}

QString WidgetPlugin::toolTip() const
{
    return "";
}

QString WidgetPlugin::whatsThis() const
{
    return "";
}

bool WidgetPlugin::isContainer() const
{
    return true;
}

QString WidgetPlugin::domXml() const
{
    return "<widget class=\"DNRWidget\" name=\"NewDNRWidget\">\n"
           " <property name=\"geometry\">\n"
           "  <rect>\n"
           "   <x>0</x>\n"
           "   <y>0</y>\n"
           "   <width>50</width>\n"
           "   <height>50</height>\n"
           "  </rect>\n"
           " </property>\n"
           " <property name=\"toolTip\" >\n"
           "  <string>The DNR Widget</string>\n"
           " </property>\n"
           " <property name=\"whatsThis\" >\n"
           "  <string>The DNR widget displays </string>\n"
           " </property>\n"
           "</widget>\n";
}

QString WidgetPlugin::includeFile() const
{
    return "DNRWidget.h";
}
