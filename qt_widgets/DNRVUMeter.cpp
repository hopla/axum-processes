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

#include "DNRVUMeter.h"

#ifdef Q_OS_WIN32
#include <windows.h> // for QueryPerformanceCounter
#endif

DNRVUMeter::DNRVUMeter(QWidget *parent)
    : QWidget(parent)
{
//    QTimer *timer = new QTimer(this);
//    connect(timer, SIGNAL(timeout()), this, SLOT(update()));
//    timer->start(1000);

    FdBPosition = 0;
    FMindBPosition = -20;
    FMaxdBPosition = +3;
    FReleasePerSecond = 60;
    FVUCurve = true;
    FPointerWidth = 1;
    FPointerColor = QColor(255,0,0,255);
    FPointerLength = 120;
    FPointerStartY = 18;

	 double LiniearMaxPositive = pow(10,((float)90+MINCURVE_POSITIVE)/DIVCURVE_POSITIVE);
	 LiniearMinPositive = pow(10,((float)0+MINCURVE_POSITIVE)/DIVCURVE_POSITIVE);
  	 LiniearRangePositive = LiniearMaxPositive-LiniearMinPositive;

	 double LiniearMaxNegative = pow(10,((float)90+MINCURVE_NEGATIVE)/DIVCURVE_NEGATIVE);
	 LiniearMinNegative = pow(10,((float)0+MINCURVE_NEGATIVE)/DIVCURVE_NEGATIVE);
  	 LiniearRangeNegative = LiniearMaxNegative-LiniearMinNegative;

  	 dBRange = FMaxdBPosition-FMindBPosition;

    QString AxumSkinPath = QString(getenv(FSkinEnvironmentVariable.toAscii()));
    VUMeterBackgroundQImage = new QImage(AxumSkinPath + "/" + FVUMeterBackgroundFileName);

    setWindowTitle(tr("VU Meter"));
    resize(260, 136);
}

void DNRVUMeter::paintEvent(QPaintEvent *)
{
   int Radius = FPointerLength;
   int MinDegree = 223;
   int MaxDegree = 314;
   int OffsetDegree = MaxDegree-MinDegree;
   float LiniairPointerDegree = (OffsetDegree*(FdBPosition-FMindBPosition))/(FMaxdBPosition-FMindBPosition);
   if (FdBPosition<FMindBPosition)
   {
      LiniairPointerDegree = 0;
   }
   if (FdBPosition>FMaxdBPosition)
   {
      LiniairPointerDegree = OffsetDegree;
   }

   float PointerDegree = LiniairPointerDegree;

   if (FVUCurve)
   {
      if (FdBPosition < 0)
      {
			float Pos = pow(10,((float)LiniairPointerDegree)/DIVCURVE_NEGATIVE);
      	PointerDegree = ((Pos-LiniearMinNegative)*90)/LiniearRangeNegative;
      }
      else
      {
			float Pos = pow(10,((float)LiniairPointerDegree)/DIVCURVE_POSITIVE);
      	PointerDegree = ((Pos-LiniearMinPositive)*90)/LiniearRangePositive;
		}
   }

   float RadPosition = ((float)(PointerDegree+MinDegree)*2*M_PI)/360;

   float StartRadius = -FPointerStartY/sin(RadPosition);
   int StartX = cos(RadPosition)*StartRadius;
   int StartY = -FPointerStartY;
   int X = cos(RadPosition)*Radius;
   int Y = sin(RadPosition)*Radius;

	QColor PointerColor(0, 0, 255, 255);

	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	if (VUMeterBackgroundQImage->isNull())
	{
	}
	else
	{
		painter.drawImage(0,0, *VUMeterBackgroundQImage);
	}

	painter.setPen(QPen(QBrush(FPointerColor), FPointerWidth));
	painter.setBrush(Qt::NoBrush);

	painter.drawLine((width()/2)+StartX, height()+StartY, (width()/2)+X, height()+Y);
}

void DNRVUMeter::setdBPosition(double_db NewdBPosition)
{
   if (FdBPosition != NewdBPosition)
   {
      FdBPosition = NewdBPosition;
      update();
   }
}

double DNRVUMeter::getdBPosition()
{
   return FdBPosition;
}

void DNRVUMeter::setMindBPosition(double NewMindBPosition)
{
   if (FMindBPosition != NewMindBPosition)
   {
      FMindBPosition = NewMindBPosition;

		double LiniearMaxPositive = pow(10,((float)90+MINCURVE_POSITIVE)/DIVCURVE_POSITIVE);
	 	LiniearMinPositive = pow(10,((float)0+MINCURVE_POSITIVE)/DIVCURVE_POSITIVE);
  	 	LiniearRangePositive = LiniearMaxPositive-LiniearMinPositive;

	 	double LiniearMaxNegative = pow(10,((float)90+MINCURVE_NEGATIVE)/DIVCURVE_NEGATIVE);
	 	LiniearMinNegative = pow(10,((float)0+MINCURVE_NEGATIVE)/DIVCURVE_NEGATIVE);
  	 	LiniearRangeNegative = LiniearMaxNegative-LiniearMinNegative;

  	 	dBRange = FMaxdBPosition-FMindBPosition;

      update();
   }
}

double DNRVUMeter::getMindBPosition()
{
   return FMindBPosition;
}

void DNRVUMeter::setMaxdBPosition(double NewMaxdBPosition)
{
   if (FMaxdBPosition != NewMaxdBPosition)
   {
      FMaxdBPosition = NewMaxdBPosition;

	 	double LiniearMaxPositive = pow(10,((float)90+MINCURVE_POSITIVE)/DIVCURVE_POSITIVE);
	 	LiniearMinPositive = pow(10,((float)0+MINCURVE_POSITIVE)/DIVCURVE_POSITIVE);
  	 	LiniearRangePositive = LiniearMaxPositive-LiniearMinPositive;

	 	double LiniearMaxNegative = pow(10,((float)90+MINCURVE_NEGATIVE)/DIVCURVE_NEGATIVE);
	 	LiniearMinNegative = pow(10,((float)0+MINCURVE_NEGATIVE)/DIVCURVE_NEGATIVE);
  	 	LiniearRangeNegative = LiniearMaxNegative-LiniearMinNegative;

  	 	dBRange = FMaxdBPosition-FMindBPosition;

      update();
   }
}

double DNRVUMeter::getMaxdBPosition()
{
   return FMaxdBPosition;
}

void DNRVUMeter::setReleasePerSecond(double NewReleasePerSecond)
{
   if (FReleasePerSecond != NewReleasePerSecond)
   {
      FReleasePerSecond = NewReleasePerSecond;
   }
}

double DNRVUMeter::getReleasePerSecond()
{
   return FReleasePerSecond;
}

void DNRVUMeter::setVUCurve(bool NewVUCurve)
{
   if (FVUCurve != NewVUCurve)
   {
      FVUCurve = NewVUCurve;
      update();
   }
}

bool DNRVUMeter::getVUCurve()
{
   return FVUCurve;
}

void DNRVUMeter::setPointerLength(int NewPointerLength)
{
   if (FPointerLength != NewPointerLength)
   {
      FPointerLength = NewPointerLength;
      update();
   }
}

int DNRVUMeter::getPointerLength()
{
   return FPointerLength;
}

void DNRVUMeter::setPointerStartY(int NewPointerStartY)
{
   if (FPointerStartY != NewPointerStartY)
   {
      FPointerStartY = NewPointerStartY;
      update();
   }
}

int DNRVUMeter::getPointerStartY()
{
   return FPointerStartY;
}

void DNRVUMeter::setPointerWidth(int NewPointerWidth)
{
   if (FPointerWidth != NewPointerWidth)
   {
      FPointerWidth = NewPointerWidth;
      update();
   }
}

int DNRVUMeter::getPointerWidth()
{
   return FPointerWidth;
}

const QColor & DNRVUMeter::getPointerColor() const
{
	return FPointerColor;
}

void DNRVUMeter::setPointerColor(const QColor & NewPointerColor)
{
	FPointerColor = NewPointerColor;
	update();
}

void DNRVUMeter::setSkinEnvironmentVariable(const QString &NewSkinEnvironmentVariable)
{
   if (FSkinEnvironmentVariable != NewSkinEnvironmentVariable)
   {
      FSkinEnvironmentVariable = NewSkinEnvironmentVariable;

      QString AxumSkinPath = QString(getenv(FSkinEnvironmentVariable.toAscii()));
      if (!VUMeterBackgroundQImage->load(AxumSkinPath + "/" + FVUMeterBackgroundFileName))
      {
		  delete VUMeterBackgroundQImage;
		  VUMeterBackgroundQImage = new QImage();

	  }
      update();
   }
}

QString DNRVUMeter::getSkinEnvironmentVariable() const
{
   return FSkinEnvironmentVariable;
}

void DNRVUMeter::setVUMeterBackgroundFileName(const QString &NewVUMeterBackgroundFileName)
{
   if (FVUMeterBackgroundFileName != NewVUMeterBackgroundFileName)
   {
      FVUMeterBackgroundFileName = NewVUMeterBackgroundFileName;

      QString AxumSkinPath = QString(getenv(FSkinEnvironmentVariable.toAscii()));
      if (!VUMeterBackgroundQImage->load(AxumSkinPath + "/" + FVUMeterBackgroundFileName))
      {
		  delete VUMeterBackgroundQImage;
		  VUMeterBackgroundQImage = new QImage();

	  }
      update();
   }
}

QString DNRVUMeter::getVUMeterBackgroundFileName() const
{
   return FVUMeterBackgroundFileName;
}

void DNRVUMeter::doRelease(char_none unused)
{
   if (FdBPosition > FMindBPosition)
   {
#ifdef Q_OS_WIN32
		LARGE_INTEGER freq, newTime;
		QueryPerformanceFrequency(&freq);
		QueryPerformanceCounter(&newTime);
		double newNumberOfSeconds = (double)newTime.QuadPart/freq.QuadPart;
#else
      timespec newTime;
      clock_gettime(CLOCK_MONOTONIC, &newTime);
      double newNumberOfSeconds = newTime.tv_sec+((double)newTime.tv_nsec/1000000000);
#endif
      double elapsedTime = newNumberOfSeconds - previousNumberOfSeconds;
      if (elapsedTime>0)
      {
         setdBPosition(FdBPosition-((double)FReleasePerSecond*elapsedTime));
         previousNumberOfSeconds = newNumberOfSeconds;
      }
   }
}
