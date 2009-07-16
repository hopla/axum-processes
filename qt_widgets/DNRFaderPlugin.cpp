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

#include "DNRFader.h"
#include "DNRFaderPlugin.h"

#include <QtPlugin>

FaderPlugin::FaderPlugin(QObject *parent)
    : QObject(parent)
{
    initialized = false;
}

void FaderPlugin::initialize(QDesignerFormEditorInterface * /* core */)
{
    if (initialized)
        return;

    initialized = true;
}

bool FaderPlugin::isInitialized() const
{
    return initialized;
}

QWidget *FaderPlugin::createWidget(QWidget *parent)
{
    return new DNRFader(parent);
}

QString FaderPlugin::name() const
{
    return "DNRFader";
}

QString FaderPlugin::group() const
{
    return "DNR Widgets";
}

QIcon FaderPlugin::icon() const
{
    return QIcon();
}

QString FaderPlugin::toolTip() const
{
    return "";
}

QString FaderPlugin::whatsThis() const
{
    return "";
}

bool FaderPlugin::isContainer() const
{
    return false;
}

QString FaderPlugin::domXml() const
{
    return "<widget class=\"DNRFader\" name=\"NewDNRFader\">\n"
           " <property name=\"geometry\">\n"
           "  <rect>\n"
           "   <x>0</x>\n"
           "   <y>0</y>\n"
           "   <width>27</width>\n"
           "   <height>257</height>\n"
           "  </rect>\n"
           " </property>\n"
           " <property name=\"toolTip\" >\n"
           "  <string>The fader</string>\n"
           " </property>\n"
           " <property name=\"whatsThis\" >\n"
           "  <string>The fader widget displays </string>\n"
           " </property>\n"
           "</widget>\n";
}

QString FaderPlugin::includeFile() const
{
    return "DNRFader.h";
}