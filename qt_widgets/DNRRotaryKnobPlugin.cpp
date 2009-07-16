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

#include "DNRRotaryKnob.h"
#include "DNRRotaryKnobPlugin.h"

#include <QtPlugin>

RotaryKnobPlugin::RotaryKnobPlugin(QObject *parent)
    : QObject(parent)
{
    initialized = false;
}

void RotaryKnobPlugin::initialize(QDesignerFormEditorInterface * /* core */)
{
    if (initialized)
        return;

    initialized = true;
}

bool RotaryKnobPlugin::isInitialized() const
{
    return initialized;
}

QWidget *RotaryKnobPlugin::createWidget(QWidget *parent)
{
    return new DNRRotaryKnob(parent);
}

QString RotaryKnobPlugin::name() const
{
    return "DNRRotaryKnob";
}

QString RotaryKnobPlugin::group() const
{
    return "DNR Widgets";
}

QIcon RotaryKnobPlugin::icon() const
{
    return QIcon();
}

QString RotaryKnobPlugin::toolTip() const
{
    return "";
}

QString RotaryKnobPlugin::whatsThis() const
{
    return "";
}

bool RotaryKnobPlugin::isContainer() const
{
    return false;
}

QString RotaryKnobPlugin::domXml() const
{
    return "<widget class=\"DNRRotaryKnob\" name=\"NewDNRRotaryKnob\">\n"
           " <property name=\"geometry\">\n"
           "  <rect>\n"
           "   <x>0</x>\n"
           "   <y>0</y>\n"
           "   <width>40</width>\n"
           "   <height>40</height>\n"
           "  </rect>\n"
           " </property>\n"
           " <property name=\"toolTip\" >\n"
           "  <string>The Rotary Knob</string>\n"
           " </property>\n"
           " <property name=\"whatsThis\" >\n"
           "  <string>The rotary knob widget displays </string>\n"
           " </property>\n"
           "</widget>\n";
}

QString RotaryKnobPlugin::includeFile() const
{
    return "DNRRotaryKnob.h";
}
