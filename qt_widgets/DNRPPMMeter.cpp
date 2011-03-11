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
#include <math.h>

#include "DNRPPMMeter.h"

#ifdef Q_OS_WIN32
#include <windows.h> // for QueryPerformanceCounter
#endif

DNRPPMMeter::DNRPPMMeter(QWidget *parent)
    : QWidget(parent)
{
	 //QTimer *timer = new QTimer();
    //QObject::connect(timer, SIGNAL(timeout()), this, SLOT(doRelease()));
    //timer->start(10);

    FdBPosition = -50;
    FMindBPosition = -50;
    FMaxdBPosition = +5;
    FReleasePerSecond = 18;
    FDINCurve = true;
    FGradientBackground = true;
    FGradientForground = false;

    double LiniearMax = pow(10,((double)FMaxdBPosition+MINCURVE)/DIVCURVE);
    LiniearMin = pow(10,((double)FMindBPosition+MINCURVE)/DIVCURVE);
    LiniearRange = LiniearMax-LiniearMin;
    dBRange = FMaxdBPosition-FMindBPosition;
    CurrentMeterHeight = 0;
    MeterHeight = 0;
    ZerodBHeight = 0;

    FMaxColor = QColor(255,0,0,255);
    FMinColor = QColor(0,255,0,255);
    FMaxBackgroundColor = QColor(64,0,0,255);
    FMinBackgroundColor = QColor(0,64,0,255);

    QString AxumSkinPath = QString(getenv(FSkinEnvironmentVariable.toAscii()));
    PPMMeterBackgroundQImage = new QImage(AxumSkinPath + "/" + FPPMMeterBackgroundFileName);

    setAttribute(Qt::WA_OpaquePaintEvent);

    setWindowTitle(tr("PPM Meter"));
    resize(16, 200);

#ifdef Q_OS_WIN32
		LARGE_INTEGER freq, newTime;
		QueryPerformanceFrequency(&freq);
		QueryPerformanceCounter(&newTime);
		previousNumberOfSeconds = (double)newTime.QuadPart/freq.QuadPart;
#else
    timespec newTime;
    clock_gettime(CLOCK_MONOTONIC, &newTime);
    previousNumberOfSeconds = newTime.tv_sec+((double)newTime.tv_nsec/1000000000);
#endif
}

bool DNRPPMMeter::CalculateMeter()
{
   if (FdBPosition<FMindBPosition)
   {
      FdBPosition = FMindBPosition;
   }
   if (FdBPosition>FMaxdBPosition)
   {
      FdBPosition = FMaxdBPosition;
   }

   MeterHeight = 0;
   ZerodBHeight = 0;

   if (FDINCurve)
   {
      double Pos = pow(10,((double)FdBPosition+MINCURVE)/DIVCURVE);
      MeterHeight = (int)(((Pos-LiniearMin)*height())/LiniearRange);
      ZerodBHeight = (int)(((1-LiniearMin)*height())/LiniearRange);
   }
   else
   {
	   double AbsolutedBPosition = FdBPosition-FMindBPosition;
      MeterHeight = (int)(AbsolutedBPosition*height())/dBRange;
      ZerodBHeight = (int)(-FMindBPosition*height())/dBRange;
   }

  if (CurrentMeterHeight != MeterHeight)
  {
    CurrentMeterHeight = MeterHeight;
    return 1;
  }
  return 0;
}

void DNRPPMMeter::paintEvent(QPaintEvent *)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(Qt::NoPen);

  if (PPMMeterBackgroundQImage->isNull())
  {
  }
  else
  {
    painter.drawImage(0,0, *PPMMeterBackgroundQImage);
  }
  
  int HalfHeight = ((float)height()/2)+0.5;

  QLinearGradient BackgroundGradient(0, HalfHeight, 0, 0);
  BackgroundGradient.setColorAt(0, FMinBackgroundColor);
  BackgroundGradient.setColorAt(1, FMaxBackgroundColor);

  QLinearGradient ForgroundGradient(0, HalfHeight, 0, 0);
  ForgroundGradient.setColorAt(0, FMinColor);
  ForgroundGradient.setColorAt(1, FMaxColor);

  if (FGradientBackground)
  {
    painter.setBrush(FMinBackgroundColor);
    painter.drawRect(0, HalfHeight, width(), HalfHeight);

    painter.setBrush(QBrush(BackgroundGradient));
    painter.drawRect(0, 0, width(), HalfHeight);
  }
  else
  {
    painter.setBrush(FMinBackgroundColor);
    painter.drawRect(0, height()-ZerodBHeight, width(), ZerodBHeight);

    painter.setBrush(FMaxBackgroundColor);
    painter.drawRect(0, 0, width(), height()-ZerodBHeight);
  }

  if (FGradientForground)
  {
    painter.setBrush(FMinColor);
    int SingleColorHeight = HalfHeight+2;
    if (MeterHeight<SingleColorHeight)
    {
      SingleColorHeight = MeterHeight;
    }
    painter.drawRect(0, height()-SingleColorHeight, width(), SingleColorHeight);

    int GradientHeight = MeterHeight-HalfHeight;
    if (GradientHeight>0)
    {
      painter.setBrush(QBrush(ForgroundGradient));
      painter.drawRect(0, HalfHeight-GradientHeight, width(), GradientHeight);
    }
  }
  else
  {
    if (MeterHeight>ZerodBHeight)
    {
      painter.setBrush(FMinColor);
      painter.drawRect(0, height()-ZerodBHeight, width(), ZerodBHeight);

      painter.setBrush(FMaxColor);
      painter.drawRect(0, height()-MeterHeight, width(), MeterHeight-ZerodBHeight);
    }
    else
    {
      painter.setBrush(FMinColor);
      painter.drawRect(0, height()-MeterHeight, width(), MeterHeight);
    }
  }
}

/*void DNRPPMMeter::setdBPosition(double_db NewdBPosition)
{
}
*/

void DNRPPMMeter::setdBPosition(double_db NewdBPosition)
{
	FdBPosition = NewdBPosition;
   update();
}

double DNRPPMMeter::getdBPosition()
{
   return FdBPosition;
}

void DNRPPMMeter::setMindBPosition(double NewMindBPosition)
{
   if (FMindBPosition != NewMindBPosition)
   {
      FMindBPosition = NewMindBPosition;

	   double LiniearMax = pow(10,((double)FMaxdBPosition+MINCURVE)/DIVCURVE);
      LiniearMin = pow(10,((double)FMindBPosition+MINCURVE)/DIVCURVE);
      LiniearRange = LiniearMax-LiniearMin;
      dBRange = FMaxdBPosition-FMindBPosition;

      update();
   }
}

double DNRPPMMeter::getMindBPosition()
{
   return FMindBPosition;
}

void DNRPPMMeter::setMaxdBPosition(double NewMaxdBPosition)
{
   if (FMaxdBPosition != NewMaxdBPosition)
   {
      FMaxdBPosition = NewMaxdBPosition;

	   double LiniearMax = pow(10,((double)FMaxdBPosition+MINCURVE)/DIVCURVE);
      LiniearMin = pow(10,((double)FMindBPosition+MINCURVE)/DIVCURVE);
      LiniearRange = LiniearMax-LiniearMin;
      dBRange = FMaxdBPosition-FMindBPosition;

      update();
   }
}

double DNRPPMMeter::getMaxdBPosition()
{
   return FMaxdBPosition;
}

void DNRPPMMeter::setReleasePerSecond(double NewReleasePerSecond)
{
   if (FReleasePerSecond != NewReleasePerSecond)
   {
      FReleasePerSecond = NewReleasePerSecond;
   }
}

double DNRPPMMeter::getReleasePerSecond()
{
   return FReleasePerSecond;
}

void DNRPPMMeter::setDINCurve(bool NewDINCurve)
{
   if (FDINCurve != NewDINCurve)
   {
      FDINCurve = NewDINCurve;
      update();
   }
}

bool DNRPPMMeter::getDINCurve()
{
   return FDINCurve;
}

void DNRPPMMeter::setGradientBackground(bool NewGradientBackground)
{
   if (FGradientBackground != NewGradientBackground)
   {
      FGradientBackground = NewGradientBackground;
      update();
   }
}

bool DNRPPMMeter::getGradientBackground()
{
   return FGradientBackground;
}

void DNRPPMMeter::setGradientForground(bool NewGradientForground)
{
   if (FGradientForground != NewGradientForground)
   {
      FGradientForground = NewGradientForground;
      update();
   }
}

bool DNRPPMMeter::getGradientForground()
{
   return FGradientForground;
}

const QColor & DNRPPMMeter::getMaxColor() const
{
	return FMaxColor;
}

void DNRPPMMeter::setMaxColor(const QColor & NewMaxColor)
{
	FMaxColor = NewMaxColor;
	update();
}

const QColor & DNRPPMMeter::getMinColor() const
{
	return FMinColor;
}

void DNRPPMMeter::setMinColor(const QColor & NewMinColor)
{
	FMinColor = NewMinColor;
	update();
}

const QColor & DNRPPMMeter::getMaxBackgroundColor() const
{
	return FMaxBackgroundColor;
}

void DNRPPMMeter::setMaxBackgroundColor(const QColor & NewMaxBackgroundColor)
{
	FMaxBackgroundColor = NewMaxBackgroundColor;
	update();
}

const QColor & DNRPPMMeter::getMinBackgroundColor() const
{
	return FMinBackgroundColor;
}

void DNRPPMMeter::setMinBackgroundColor(const QColor & NewMinBackgroundColor)
{
	FMinBackgroundColor = NewMinBackgroundColor;
	update();
}

void DNRPPMMeter::setSkinEnvironmentVariable(const QString &NewSkinEnvironmentVariable)
{
   if (FSkinEnvironmentVariable != NewSkinEnvironmentVariable)
   {
      FSkinEnvironmentVariable = NewSkinEnvironmentVariable;

      QString AxumSkinPath = QString(getenv(FSkinEnvironmentVariable.toAscii()));
      if (!PPMMeterBackgroundQImage->load(AxumSkinPath + "/" + FPPMMeterBackgroundFileName))
      {
		  delete PPMMeterBackgroundQImage;
		  PPMMeterBackgroundQImage = new QImage();

	  }
      update();
   }
}

QString DNRPPMMeter::getSkinEnvironmentVariable() const
{
   return FSkinEnvironmentVariable;
}

void DNRPPMMeter::setPPMMeterBackgroundFileName(const QString &NewPPMMeterBackgroundFileName)
{
   if (FPPMMeterBackgroundFileName != NewPPMMeterBackgroundFileName)
   {
      FPPMMeterBackgroundFileName = NewPPMMeterBackgroundFileName;

      QString AxumSkinPath = QString(getenv(FSkinEnvironmentVariable.toAscii()));
      if (!PPMMeterBackgroundQImage->load(AxumSkinPath + "/" + FPPMMeterBackgroundFileName))
      {
		  delete PPMMeterBackgroundQImage;
		  PPMMeterBackgroundQImage = new QImage();

	  }
      update();
   }
}

QString DNRPPMMeter::getPPMMeterBackgroundFileName() const
{
   return FPPMMeterBackgroundFileName;
}
