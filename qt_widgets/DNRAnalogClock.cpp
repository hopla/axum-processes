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

    FCountDownTime = 0;
    FCountDownColor = QColor(255,0,255);
    FCountDownWidth = 2;
    FCountDownSpacing = 1;

    FEndTime = false;
    FEndTimeLength = 8;
    FEndTimeWidth = 2;
    FEndTimeMinute = 0;
    FEndTimeSecond = 0;
    FEndTimeColor = QColor(255,0,0);

    FHourLines = false;
    FHourLinesLength = 8;
    FHourLinesWidth = 2;
    FHourLinesColor = QColor(0,0,0);

    FMinuteLines = false;
    FMinuteLinesLength = 4;
    FMinuteLinesWidth = 1;
    FMinuteLinesColor = QColor(61,61,61,191);

    FSecondDots = true;
    FSecondDotsCountDown = false;
    FDotSize = 1;
    FDotColor = QColor(0,0,0);

    FHands = true;
    FHourHandColor = QColor(0,0,0);
    FHourHandBorder = 12;
    FHourHandLength = 12;
    FHourHandWidth = 12;
    FMinuteHandColor = QColor(61,61,61,191);
    FMinuteHandBorder = 4;
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

  int hourYOffset = (int)((((float)FHourHandWidth)/2)+0.5);
  int minuteYOffset = (int)((((float)FMinuteHandWidth)/2)+0.5);

  QPoint hourHand[3] = {
    QPoint(hourYOffset, -(100-FHourHandBorder)+FHourHandLength),
    QPoint(-hourYOffset, -(100-FHourHandBorder)+FHourHandLength),
    QPoint(0, -(100-FHourHandBorder))
  };
  QPoint minuteHand[3] = {
    QPoint(minuteYOffset, -(100-FMinuteHandBorder)+FMinuteHandLength),
    QPoint(-minuteYOffset, -(100-FMinuteHandBorder)+FMinuteHandLength),
    QPoint(0, -(100-FMinuteHandBorder))
  };

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.translate(width() / 2, height() / 2);
  painter.scale(side / 200.0, side / 200.0);

  for (int j = 0; j < 60; ++j)
  {
    if ((j % 5) != 0)
    {
      if (FMinuteLines)
      {
        painter.setPen(QPen(FMinuteLinesColor, FMinuteLinesWidth));
        painter.drawLine(0, -(96-FMinuteLinesLength), 0, -96);
      }
    }
    else
    {
      if (FHourLines)
      {
        painter.setPen(QPen(FHourLinesColor, FHourLinesWidth));
        painter.drawLine(0, -(96-FHourLinesLength), 0, -96);
      }
    }
    painter.rotate(6.0);
  }

  if (FEndTime)
  {
    int TimeInSeconds = FMinute*60+FSecond;
    int EndTimeInSeconds = FEndTimeMinute*60 + FEndTimeSecond;
    int TimeToEnd = EndTimeInSeconds-TimeInSeconds;
    if (TimeToEnd<=(15-3600))
    {
      TimeToEnd += 3600;
    }

    painter.save();
    painter.rotate(6.0 * (FEndTimeMinute + FEndTimeSecond / 60.0));
    painter.setPen(QPen(FEndTimeColor, FEndTimeWidth));

    painter.drawLine(0, -100+FEndTimeWidth/2, 0, -100+FEndTimeLength);

    if ((TimeToEnd>0) && (TimeToEnd<=15))
    {
      painter.drawArc(-100+FEndTimeWidth/2,-100+FEndTimeWidth/2, 200-FEndTimeWidth*2, 200-FEndTimeWidth*2, 90*16, (90*16*TimeToEnd)/15);
    }
    painter.restore();
  }

  if (FCountDownTime)
  {
    painter.setPen(QPen(FCountDownColor, FCountDownWidth));
    painter.drawArc(-96+FHourLinesLength+FCountDownSpacing,-96+FHourLinesLength+FCountDownSpacing, 192-(FHourLinesLength+FCountDownSpacing)*2, 192-(FHourLinesLength+FCountDownSpacing)*2, 90*16, (360*FCountDownTime)*16/60);
  }

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

void DNRAnalogClock::setHourLinesWidth(int NewHourLinesWidth)
{
  if (FHourLinesWidth != NewHourLinesWidth)
  {
    FHourLinesWidth = NewHourLinesWidth;
    update();
  }
}

int DNRAnalogClock::getHourLinesWidth()
{
  return FHourLinesWidth;
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

void DNRAnalogClock::setMinuteLinesWidth(int NewMinuteLinesWidth)
{
  if (FMinuteLinesWidth != NewMinuteLinesWidth)
  {
    FMinuteLinesWidth = NewMinuteLinesWidth;
    update();
  }
}

int DNRAnalogClock::getMinuteLinesWidth()
{
  return FMinuteLinesWidth;
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

void DNRAnalogClock::setHourHandBorder(int NewHourHandBorder)
{
  if (FHourHandBorder != NewHourHandBorder)
  {
    FHourHandBorder = NewHourHandBorder;
    update();
  }
}

int DNRAnalogClock::getHourHandBorder()
{
  return FHourHandBorder;
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

void DNRAnalogClock::setMinuteHandBorder(int NewMinuteHandBorder)
{
  if (FMinuteHandBorder != NewMinuteHandBorder)
  {
    FMinuteHandBorder = NewMinuteHandBorder;
    update();
  }
}

int DNRAnalogClock::getMinuteHandBorder()
{
  return FMinuteHandBorder;
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

void DNRAnalogClock::setEndTime(bool NewEndTime)
{
  if (FEndTime != NewEndTime)
  {
    FEndTime = NewEndTime;
    update();
  }
}

bool DNRAnalogClock::getEndTime()
{
  return FEndTime;
}

void DNRAnalogClock::setEndTimeLength(int NewEndTimeLength)
{
  if (FEndTimeLength != NewEndTimeLength)
  {
    FEndTimeLength = NewEndTimeLength;
    update();
  }
}

int DNRAnalogClock::getEndTimeLength()
{
  return FEndTimeLength;
}

void DNRAnalogClock::setEndTimeWidth(int NewEndTimeWidth)
{
  if (FEndTimeWidth != NewEndTimeWidth)
  {
    FEndTimeWidth = NewEndTimeWidth;
    update();
  }
}

int DNRAnalogClock::getEndTimeWidth()
{
  return FEndTimeWidth;
}

void DNRAnalogClock::setEndTimeMinute(int NewEndTimeMinute)
{
  if (FEndTimeMinute != NewEndTimeMinute)
  {
    FEndTimeMinute = NewEndTimeMinute;
    update();
  }
}

int DNRAnalogClock::getEndTimeMinute()
{
  return FEndTimeMinute;
}

void DNRAnalogClock::setEndTimeSecond(int NewEndTimeSecond)
{
  if (FEndTimeSecond != NewEndTimeSecond)
  {
    FEndTimeSecond = NewEndTimeSecond;
    update();
  }
}

int DNRAnalogClock::getEndTimeSecond()
{
  return FEndTimeSecond;
}

void DNRAnalogClock::setEndTimeColor(QColor NewEndTimeColor)
{
  if (FEndTimeColor != NewEndTimeColor)
  {
    FEndTimeColor = NewEndTimeColor;
    update();
  }

}

QColor DNRAnalogClock::getEndTimeColor()
{
  return FEndTimeColor;
}

void DNRAnalogClock::setCountDownTime(float NewCountDownTime)
{
  if (FCountDownTime != NewCountDownTime)
  {
    FCountDownTime = NewCountDownTime;
    update();
  }
}

float DNRAnalogClock::getCountDownTime()
{
  return FCountDownTime;
}

void DNRAnalogClock::setCountDownWidth(int NewCountDownWidth)
{
  if (FCountDownWidth != NewCountDownWidth)
  {
    FCountDownWidth = NewCountDownWidth;
    update();
  }
}

int DNRAnalogClock::getCountDownWidth()
{
  return FCountDownWidth;
}

void DNRAnalogClock::setCountDownSpacing(int NewCountDownSpacing)
{
  if (FCountDownSpacing != NewCountDownSpacing)
  {
    FCountDownSpacing = NewCountDownSpacing;
    update();
  }
}

int DNRAnalogClock::getCountDownSpacing()
{
  return FCountDownSpacing;
}

void DNRAnalogClock::setCountDownColor(QColor NewCountDownColor)
{
  if (FCountDownColor != NewCountDownColor)
  {
    FCountDownColor = NewCountDownColor;
    update();
  }
}

QColor DNRAnalogClock::getCountDownColor()
{
  return FCountDownColor;
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
