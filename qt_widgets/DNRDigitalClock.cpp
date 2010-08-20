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

#include <QtGui>

#include "DNRDigitalClock.h"

DNRDigitalClock::DNRDigitalClock(QWidget *parent)
    : QWidget(parent)
{
    FHour = 0;
    FMinute = 0;
    FSecond = 0;

    FTimeDisplay = true;
    FDateDisplay = true;

    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(checkTime()));
    timer->start(100);

    setWindowTitle(tr("Digital Clock"));
    resize(100, 30);
}

void DNRDigitalClock::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.setBrush(Qt::NoBrush);

    int PosY = 0;
    int HalfHeight = height()/2;
    
    if (FTimeDisplay) {
      painter.setPen(FTimeDisplayFontColor);
      painter.setFont(FTimeDisplayFont);
      painter.drawText( 0, PosY, width(), HalfHeight, Qt::AlignCenter, TimeString);
      
      PosY += painter.fontMetrics().height();
    }
    
    if (FDateDisplay) {
      painter.setPen(FDateDisplayFontColor);
      painter.setFont(FDateDisplayFont);
      painter.drawText( 0, PosY, width(), HalfHeight, Qt::AlignCenter, DateString);
      PosY += painter.fontMetrics().height();
    }

}

void DNRDigitalClock::setTimeDisplay(bool NewTimeDisplay)
{
  if (FTimeDisplay != NewTimeDisplay)
  {
    FTimeDisplay = NewTimeDisplay;
    update();
  }
}

bool DNRDigitalClock::getTimeDisplay()
{
  return FTimeDisplay;
}

void DNRDigitalClock::setTimeDisplayFont(QFont NewTimeDisplayFont)
{
  if (FTimeDisplayFont != NewTimeDisplayFont)
  {
    FTimeDisplayFont = NewTimeDisplayFont;
    update();
  }
}

QFont DNRDigitalClock::getTimeDisplayFont()
{
  return FTimeDisplayFont;
}
    
void DNRDigitalClock::setTimeDisplayFontColor(QColor NewTimeDisplayFontColor)
{
  if (FTimeDisplayFontColor != NewTimeDisplayFontColor)
  {
    FTimeDisplayFontColor = NewTimeDisplayFontColor;
    update();
  }
}

QColor DNRDigitalClock::getTimeDisplayFontColor()
{
  return FTimeDisplayFontColor;
}

void DNRDigitalClock::setDateDisplay(bool NewDateDisplay)
{
  if (FDateDisplay != NewDateDisplay)
  {
    FDateDisplay = NewDateDisplay;
    update();
  }
}

bool DNRDigitalClock::getDateDisplay()
{
  return FDateDisplay;
}

void DNRDigitalClock::setDateDisplayFont(QFont NewDateDisplayFont)
{
  if (FDateDisplayFont != NewDateDisplayFont)
  {
    FDateDisplayFont = NewDateDisplayFont;
    update();
  }
}

QFont DNRDigitalClock::getDateDisplayFont()
{
  return FDateDisplayFont;
}

void DNRDigitalClock::setDateDisplayFontColor(QColor NewDateDisplayFontColor)
{
  if (FDateDisplayFontColor != NewDateDisplayFontColor)
  {
    FDateDisplayFontColor = NewDateDisplayFontColor;
    update();
  }
}

QColor DNRDigitalClock::getDateDisplayFontColor()
{
  return FDateDisplayFontColor;
}

void DNRDigitalClock::checkTime()
{
  bool TimeChanged = false;
  QTime time = QTime::currentTime();
  QDate Date = QDate::currentDate();
  int NewHour = time.hour();
  int NewMinute = time.minute();
  int NewSecond = time.second();
  TimeString = time.toString("hh:mm:ss");
  DateString = Date.toString();

  if (FSecond != NewSecond)
  {
    FSecond = NewSecond;
    if (FMinute != NewMinute)
    {
      FMinute = NewMinute;
      if (FHour != NewHour)
      {
        FHour = NewHour;
      }
    }
    TimeChanged = true;
  }

  if (TimeChanged)
  {
    update();
  }
}
