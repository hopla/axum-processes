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

#ifndef DNRDIGITALCLOCK_H
#define DNRDIGITALCLOCK_H

#include <QWidget>
#include <QtDesigner/QDesignerExportWidget>

class QDESIGNER_WIDGET_EXPORT DNRDigitalClock : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(bool TimeDisplay READ getTimeDisplay WRITE setTimeDisplay);
    Q_PROPERTY(QFont TimeDisplayFont READ getTimeDisplayFont WRITE setTimeDisplayFont);
    Q_PROPERTY(QColor TimeDisplayFontColor READ getTimeDisplayFontColor WRITE setTimeDisplayFontColor);
    Q_PROPERTY(bool DateDisplay READ getDateDisplay WRITE setDateDisplay);
    Q_PROPERTY(QFont DateDisplayFont READ getDateDisplayFont WRITE setDateDisplayFont);
    Q_PROPERTY(QColor DateDisplayFontColor READ getDateDisplayFontColor WRITE setDateDisplayFontColor);

public:
    DNRDigitalClock(QWidget *parent = 0);
    
    bool FTimeDisplay;
    QFont FTimeDisplayFont;
    QColor FTimeDisplayFontColor;
    
    bool FDateDisplay;
    QFont FDateDisplayFont;
    QColor FDateDisplayFontColor;
    
    void setTimeDisplay(bool NewTimeDisplay);
    bool getTimeDisplay();

    void setTimeDisplayFont(QFont NewTimeDisplayFont);
    QFont getTimeDisplayFont();
    
    void setTimeDisplayFontColor(QColor NewTimeDisplayFontColor);
    QColor getTimeDisplayFontColor();
    
    void setDateDisplay(bool NewDateDisplay);
    bool getDateDisplay();

    void setDateDisplayFont(QFont NewDateDisplayFont);
    QFont getDateDisplayFont();

    void setDateDisplayFontColor(QColor NewDateDisplayFontColor);
    QColor getDateDisplayFontColor();

protected:
    void paintEvent(QPaintEvent *event);
};

#endif
