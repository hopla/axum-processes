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

#include "DNRImage.h"
#include "DNRImagePlugin.h"

#include <QtPlugin>

ImagePlugin::ImagePlugin(QObject *parent)
    : QObject(parent)
{
    initialized = false;
}

void ImagePlugin::initialize(QDesignerFormEditorInterface * /* core */)
{
    if (initialized)
        return;

    initialized = true;
}

bool ImagePlugin::isInitialized() const
{
    return initialized;
}

QWidget *ImagePlugin::createWidget(QWidget *parent)
{
    return new DNRImage(parent);
}

QString ImagePlugin::name() const
{
    return "DNRImage";
}

QString ImagePlugin::group() const
{
    return "DNR Widgets";
}

QIcon ImagePlugin::icon() const
{
    return QIcon();
}

QString ImagePlugin::toolTip() const
{
    return "";
}

QString ImagePlugin::whatsThis() const
{
    return "";
}

bool ImagePlugin::isContainer() const
{
    return false;
}

QString ImagePlugin::domXml() const
{
    return "<widget class=\"DNRImage\" name=\"NewDNRImage\">\n"
           " <property name=\"geometry\">\n"
           "  <rect>\n"
           "   <x>0</x>\n"
           "   <y>0</y>\n"
           "   <width>50</width>\n"
           "   <height>50</height>\n"
           "  </rect>\n"
           " </property>\n"
           " <property name=\"toolTip\" >\n"
           "  <string>The Image</string>\n"
           " </property>\n"
           " <property name=\"whatsThis\" >\n"
           "  <string>The Image widget displays </string>\n"
           " </property>\n"
           "</widget>\n";
}

QString ImagePlugin::includeFile() const
{
    return "DNRImage.h";
}
