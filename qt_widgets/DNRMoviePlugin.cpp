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

#include "DNRMovie.h"
#include "DNRMoviePlugin.h"

#include <QtPlugin>

MoviePlugin::MoviePlugin(QObject *parent)
    : QObject(parent)
{
    initialized = false;
}

void MoviePlugin::initialize(QDesignerFormEditorInterface * /* core */)
{
    if (initialized)
        return;

    initialized = true;
}

bool MoviePlugin::isInitialized() const
{
    return initialized;
}

QWidget *MoviePlugin::createWidget(QWidget *parent)
{
    return new DNRMovie(parent);
}

QString MoviePlugin::name() const
{
    return "DNRMovie";
}

QString MoviePlugin::group() const
{
    return "DNR Widgets";
}

QIcon MoviePlugin::icon() const
{
    return QIcon();
}

QString MoviePlugin::toolTip() const
{
    return "";
}

QString MoviePlugin::whatsThis() const
{
    return "";
}

bool MoviePlugin::isContainer() const
{
    return false;
}

QString MoviePlugin::domXml() const
{
    return "<widget class=\"DNRMovie\" name=\"NewDNRMovie\">\n"
           " <property name=\"geometry\">\n"
           "  <rect>\n"
           "   <x>0</x>\n"
           "   <y>0</y>\n"
           "   <width>50</width>\n"
           "   <height>50</height>\n"
           "  </rect>\n"
           " </property>\n"
           " <property name=\"toolTip\" >\n"
           "  <string>The Movie</string>\n"
           " </property>\n"
           " <property name=\"whatsThis\" >\n"
           "  <string>The Movie widget displays </string>\n"
           " </property>\n"
           "</widget>\n";
}

QString MoviePlugin::includeFile() const
{
    return "DNRMovie.h";
}
