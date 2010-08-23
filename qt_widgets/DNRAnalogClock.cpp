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

#include "DNRAnalogClock.h"

DNRAnalogClock::DNRAnalogClock(QWidget *parent)
    : QWidget(parent)
{
    FHour = 0;
    FMinute = 0;
    FSecond = 0;

    FHourLines = false;
    FHourLinesLength = 8;
    FHourLinesColor = QColor(0,0,0);
    
    FMinuteLines = false;
    FMinuteLinesLength = 4;
    FMinuteLinesColor = QColor(61,61,61,191);

    FSecondDots = true;
    FSecondDotsCountDown = false;
    FDotSize = 1;
    FDotColor = QColor(0,0,0);
  
    FHands = true;
    FHourHandColor = QColor(0,0,0);
    FHourHandLength = 12;
    FHourHandWidth = 12;
    FMinuteHandColor = QColor(61,61,61,191);
    FMinuteHandLength = 8;
    FMinuteHandWidth = 8;

    setWindowTitle(tr("Analog Clock"));
    resize(200, 200);

    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(checkTime()));
    timer->start(100);
}

void DNRAnalogClock::paintEvent(QPaintEvent *)
{
  int XOffset = (int)(((float)FDotSize/2)+0.5);
  int side = qMin(width(), height());
  int minuteBorderSize = qMax(FDotSize, FMinuteLinesLength);
  int hourBorderSize = FHourLinesLength;
  
  int hourYOffset = (int)((((float)FHourHandWidth)/2)+0.5);
  int minuteYOffset = (int)((((float)FMinuteHandWidth)/2)+0.5);

  QPoint hourHand[3] = {
    QPoint(hourYOffset, -(96-hourBorderSize)+FHourHandLength),
    QPoint(-hourYOffset, -(96-hourBorderSize)+FHourHandLength),
    QPoint(0, -(96-hourBorderSize))
  };
  QPoint minuteHand[3] = {
    QPoint(minuteYOffset, -(96-minuteBorderSize)+FMinuteHandLength),
    QPoint(-minuteYOffset, -(96-minuteBorderSize)+FMinuteHandLength),
    QPoint(0, -(96-minuteBorderSize))
  };
  
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.translate(width() / 2, height() / 2);
  painter.scale(side / 200.0, side / 200.0);

  if (FHands)
  {
    painter.setPen(Qt::NoPen);
    painter.setBrush(FHourHandColor);

    painter.save();
    painter.rotate(30.0 * ((FHour + FMinute / 60.0)));
    painter.drawConvexPolygon(hourHand, 3);
    painter.restore();

    painter.setBrush(FMinuteHandColor);

    painter.save();
    painter.rotate(6.0 * (FMinute + FSecond / 60.0));
    painter.drawConvexPolygon(minuteHand, 3);
    painter.restore();
  }

  for (int j = 0; j < 60; ++j)
  {
    if ((j % 5) != 0)
    {
      if (FMinuteLines)
      {
        painter.setPen(FMinuteLinesColor);
        painter.drawLine(0, -(96-FMinuteLinesLength), 0, -96);
      }
    }
    else
    {
      if (FHourLines)
      {
        painter.setPen(FHourLinesColor);
        painter.drawLine(0, -(96-FHourLinesLength), 0, -96);
      }
    }

    if (FSecondDots)
    {
      painter.setPen(FDotColor);
      painter.setBrush(FDotColor);
      
      if (FSecondDotsCountDown)
      {
        if (j>=FSecond)
        {
          painter.drawEllipse(-XOffset, -(100-FDotSize), FDotSize, FDotSize);
        }
      }
      else
      {
        if (j<=FSecond)
        {
          painter.drawEllipse(-XOffset, -(100-FDotSize), FDotSize, FDotSize);
        }
      }
    }
    painter.rotate(6.0);
  }
}

void DNRAnalogClock::setHourLines(bool NewHourLines)
{
   if (FHourLines != NewHourLines)
   {
      FHourLines = NewHourLines;
      update();
   }
}

bool DNRAnalogClock::getHourLines()
{
   return FHourLines;
}
  
void DNRAnalogClock::setHourLinesLength(int NewHourLinesLength)
{
  if (FHourLinesLength != NewHourLinesLength)
  {
    FHourLinesLength = NewHourLinesLength;
    update();
  }
}

int DNRAnalogClock::getHourLinesLength()
{
  return FHourLinesLength;
}
    
void DNRAnalogClock::setHourLinesColor(QColor NewHourLinesColor)
{
  if (FHourLinesColor != NewHourLinesColor)
  {
    FHourLinesColor = NewHourLinesColor;
    update();
  }
}

QColor DNRAnalogClock::getHourLinesColor()
{
  return FHourLinesColor;
}
    
void DNRAnalogClock::setMinuteLines(bool NewMinuteLines)
{
   if (FMinuteLines != NewMinuteLines)
   {
      FMinuteLines = NewMinuteLines;
      update();
   }
}

bool DNRAnalogClock::getMinuteLines()
{
   return FMinuteLines;
}

void DNRAnalogClock::setMinuteLinesLength(int NewMinuteLinesLength)
{
  if (FMinuteLinesLength != NewMinuteLinesLength)
  {
    FMinuteLinesLength = NewMinuteLinesLength;
    update();
  }
}

int DNRAnalogClock::getMinuteLinesLength()
{
  return FMinuteLinesLength;
}
    
void DNRAnalogClock::setMinuteLinesColor(QColor NewMinuteLinesColor)
{
  if (FMinuteLinesColor != NewMinuteLinesColor)
  {
    FMinuteLinesColor = NewMinuteLinesColor;
    update();
  }
}

QColor DNRAnalogClock::getMinuteLinesColor()
{
  return FMinuteLinesColor;
}

void DNRAnalogClock::setSecondDots(bool NewSecondDots)
{
   if (FSecondDots != NewSecondDots)
   {
      FSecondDots = NewSecondDots;
      update();
   }
}

bool DNRAnalogClock::getSecondDots()
{
   return FSecondDots;
}

void DNRAnalogClock::setSecondDotsCountDown(bool NewSecondDotsCountDown)
{
   if (FSecondDotsCountDown != NewSecondDotsCountDown)
   {
      FSecondDotsCountDown = NewSecondDotsCountDown;
      update();
   }
}

bool DNRAnalogClock::getSecondDotsCountDown()
{
   return FSecondDotsCountDown;
}

void DNRAnalogClock::setDotSize(int NewDotSize)
{
  if (FDotSize != NewDotSize)
  {
    FDotSize = NewDotSize;
    update();
  }
}

int DNRAnalogClock::getDotSize()
{
  return FDotSize;
}

void DNRAnalogClock::setDotColor(QColor NewDotColor)
{
  if (FDotColor != NewDotColor)
  {
    FDotColor = NewDotColor;
    update();
  }
}

QColor DNRAnalogClock::getDotColor()
{
  return FDotColor;
}

void DNRAnalogClock::setHands(bool NewHands)
{
  if (FHands != NewHands)
  {
    FHands = NewHands;
    update();
  }
}

bool DNRAnalogClock::getHands()
{
  return FHands;
}
      
void DNRAnalogClock::setHourHandColor(QColor NewHourHandColor)
{
  if (FHourHandColor != NewHourHandColor)
  {
    FHourHandColor = NewHourHandColor;
    update();
  }
}

QColor DNRAnalogClock::getHourHandColor()
{
  return FHourHandColor;
}
    
void DNRAnalogClock::setHourHandLength(int NewHourHandLength)
{
  if (FHourHandLength != NewHourHandLength)
  {
    FHourHandLength = NewHourHandLength;
    update();
  }
}

int DNRAnalogClock::getHourHandLength()
{
  return FHourHandLength;
}

void DNRAnalogClock::setHourHandWidth(int NewHourHandWidth)
{
  if (FHourHandWidth != NewHourHandWidth)
  {
    FHourHandWidth = NewHourHandWidth;
    update();
  }
}

int DNRAnalogClock::getHourHandWidth()
{
  return FHourHandWidth;
}

void DNRAnalogClock::setMinuteHandColor(QColor NewMinuteHandColor)
{
  if (FMinuteHandColor != NewMinuteHandColor)
  {
    FMinuteHandColor = NewMinuteHandColor;
    update();
  }
}

QColor DNRAnalogClock::getMinuteHandColor()
{
  return FMinuteHandColor;
}

void DNRAnalogClock::setMinuteHandLength(int NewMinuteHandLength)
{
  if (FMinuteHandLength != NewMinuteHandLength)
  {
    FMinuteHandLength = NewMinuteHandLength;
    update();
  }
}

int DNRAnalogClock::getMinuteHandLength()
{
  return FMinuteHandLength;
}

void DNRAnalogClock::setMinuteHandWidth(int NewMinuteHandWidth)
{
  if (FMinuteHandWidth != NewMinuteHandWidth)
  {
    FMinuteHandWidth = NewMinuteHandWidth;
    update();
  }
}

int DNRAnalogClock::getMinuteHandWidth()
{
  return FMinuteHandWidth;
}

void DNRAnalogClock::checkTime()
{
  bool TimeChanged = false;
  QTime time = QTime::currentTime();
  int NewHour = time.hour();
  int NewMinute = time.minute();
  int NewSecond = time.second();
  
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
