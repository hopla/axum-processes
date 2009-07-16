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
#include "DNRPhaseMeter.h"

DNRPhaseMeter::DNRPhaseMeter(QWidget *parent)
    : QWidget(parent)
{
    FPosition = 0;
    FMinPosition = -1;
    FMaxPosition = +1;

    FPointerWidth = 1;
    FPointerMinColor = QColor(255,255,0,255);
    FPointerMaxColor = QColor(255,128,0,255);
    FBackgroundMonoColor = QColor(0,128,0,255);
    FBackgroundOutOfPhaseColor = QColor(128,0,0,255);
    FHorizontal = false;

    QString AxumSkinPath = QString(getenv(FSkinEnvironmentVariable.toAscii()));
    PhaseMeterBackgroundQImage = new QImage(AxumSkinPath + "/" + FPhaseMeterBackgroundFileName);

    setWindowTitle(tr("Phase Meter"));
    resize(16, 200);
}

void DNRPhaseMeter::paintEvent(QPaintEvent *)
{
   QPainter painter(this);
   painter.setRenderHint(QPainter::Antialiasing);
   painter.setPen(Qt::NoPen);

   double Range = FMaxPosition-FMinPosition;
   if (FPosition<FMinPosition)
   {
      FPosition = FMinPosition;
   }
   if (FPosition>FMaxPosition)
   {
      FPosition = FMaxPosition;
   }

   double Factor = (FPosition-FMinPosition)/Range;
   int Red = FPointerMinColor.red()*Factor + FPointerMaxColor.red()*(1-Factor);
   int Green = FPointerMinColor.green()*Factor + FPointerMaxColor.green()*(1-Factor);
   int Blue = FPointerMinColor.blue()*Factor + FPointerMaxColor.blue()*(1-Factor);
   int Alpha = FPointerMinColor.alpha()*Factor + FPointerMaxColor.alpha()*(1-Factor);

   QColor PointerColor(Red, Green, Blue, Alpha);

    if (PhaseMeterBackgroundQImage->isNull())
    {
    }
    else
    {
      painter.drawImage(0,0, *PhaseMeterBackgroundQImage);
    }

    if (FHorizontal)
    {
		QLinearGradient BackgroundGradient(width(), 0, 0, 0);
    	BackgroundGradient.setColorAt(0, FBackgroundMonoColor);
	    BackgroundGradient.setColorAt(1, FBackgroundOutOfPhaseColor);

		if (FGradientBackground)
		{
	    	painter.setBrush(QBrush(BackgroundGradient));
    		painter.drawRect(0,0,width(),height());
		}
		else
		{
		    painter.setBrush(FBackgroundOutOfPhaseColor);
    		painter.drawRect(0,0,width()/2,height());

		    painter.setBrush(FBackgroundMonoColor);
    		painter.drawRect(width()/2,0,width()/2, height());
		}

	 	painter.setPen(QPen(QBrush(PointerColor), FPointerWidth));
    	painter.setBrush(Qt::NoBrush);

	    painter.drawLine(width()-(width()*Factor), 0, width()-(width()*Factor), height());
	}
	else
	{
		QLinearGradient BackgroundGradient(0, 0, 0, height());
    	BackgroundGradient.setColorAt(0, FBackgroundMonoColor);
	    BackgroundGradient.setColorAt(1, FBackgroundOutOfPhaseColor);

		if (FGradientBackground)
		{
	    	painter.setBrush(QBrush(BackgroundGradient));
    		painter.drawRect(0,0,width(),height());
		}
		else
		{
		    painter.setBrush(FBackgroundMonoColor);
    		painter.drawRect(0,0,width(), height()/2);

		    painter.setBrush(FBackgroundOutOfPhaseColor);
    		painter.drawRect(0,height()/2,width(), height()/2);
		}

	 	painter.setPen(QPen(QBrush(PointerColor), FPointerWidth));
    	painter.setBrush(Qt::NoBrush);

	    painter.drawLine(0, height()*Factor , width(), height()*Factor);
	}
}

void DNRPhaseMeter::setPosition(double_phase NewPosition)
{
   if (FPosition != NewPosition)
   {
      FPosition = NewPosition;
      update();
   }
}

double DNRPhaseMeter::getPosition()
{
   return FPosition;
}

void DNRPhaseMeter::setMinPosition(double NewMinPosition)
{
   if (FMinPosition != NewMinPosition)
   {
      FMinPosition = NewMinPosition;
      update();
   }
}

double DNRPhaseMeter::getMinPosition()
{
   return FMinPosition;
}

void DNRPhaseMeter::setMaxPosition(double NewMaxPosition)
{
   if (FMaxPosition != NewMaxPosition)
   {
      FMaxPosition = NewMaxPosition;
      update();
   }
}

double DNRPhaseMeter::getMaxPosition()
{
   return FMaxPosition;
}

void DNRPhaseMeter::setPointerWidth(int NewPointerWidth)
{
   if (FPointerWidth != NewPointerWidth)
   {
      FPointerWidth = NewPointerWidth;
      update();
   }
}

int DNRPhaseMeter::getPointerWidth()
{
   return FPointerWidth;
}

const QColor & DNRPhaseMeter::getPointerMinColor() const
{
	return FPointerMinColor;
}

void DNRPhaseMeter::setPointerMinColor(const QColor & NewPointerMinColor)
{
	FPointerMinColor = NewPointerMinColor;
	update();
}

const QColor & DNRPhaseMeter::getPointerMaxColor() const
{
	return FPointerMaxColor;
}

void DNRPhaseMeter::setPointerMaxColor(const QColor & NewPointerMaxColor)
{
	FPointerMaxColor = NewPointerMaxColor;
	update();
}

const QColor & DNRPhaseMeter::getBackgroundMonoColor() const
{
	return FBackgroundMonoColor;
}

void DNRPhaseMeter::setBackgroundMonoColor(const QColor & NewBackgroundMonoColor)
{
	FBackgroundMonoColor = NewBackgroundMonoColor;
	update();
}

const QColor & DNRPhaseMeter::getBackgroundOutOfPhaseColor() const
{
	return FBackgroundOutOfPhaseColor;
}

void DNRPhaseMeter::setBackgroundOutOfPhaseColor(const QColor & NewBackgroundOutOfPhaseColor)
{
	FBackgroundOutOfPhaseColor = NewBackgroundOutOfPhaseColor;
	update();
}

void DNRPhaseMeter::setGradientBackground(bool NewGradientBackground)
{
   if (FGradientBackground != NewGradientBackground)
   {
      FGradientBackground = NewGradientBackground;
      update();
   }
}

bool DNRPhaseMeter::getGradientBackground()
{
   return FGradientBackground;
}

void DNRPhaseMeter::setHorizontal(bool NewHorizontal)
{
   if (FHorizontal != NewHorizontal)
   {
      FHorizontal = NewHorizontal;
      update();
   }
}

bool DNRPhaseMeter::getHorizontal()
{
   return FHorizontal;
}

void DNRPhaseMeter::setSkinEnvironmentVariable(const QString &NewSkinEnvironmentVariable)
{
   if (FSkinEnvironmentVariable != NewSkinEnvironmentVariable)
   {
      FSkinEnvironmentVariable = NewSkinEnvironmentVariable;

      QString AxumSkinPath = QString(getenv(FSkinEnvironmentVariable.toAscii()));
      if (!PhaseMeterBackgroundQImage->load(AxumSkinPath + "/" + FPhaseMeterBackgroundFileName))
      {
		  delete PhaseMeterBackgroundQImage;
		  PhaseMeterBackgroundQImage = new QImage();

	  }
      update();
   }
}

QString DNRPhaseMeter::getSkinEnvironmentVariable() const
{
   return FSkinEnvironmentVariable;
}

void DNRPhaseMeter::setPhaseMeterBackgroundFileName(const QString &NewPhaseMeterBackgroundFileName)
{
   if (FPhaseMeterBackgroundFileName != NewPhaseMeterBackgroundFileName)
   {
      FPhaseMeterBackgroundFileName = NewPhaseMeterBackgroundFileName;

      QString AxumSkinPath = QString(getenv(FSkinEnvironmentVariable.toAscii()));
      if (!PhaseMeterBackgroundQImage->load(AxumSkinPath + "/" + FPhaseMeterBackgroundFileName))
      {
		  delete PhaseMeterBackgroundQImage;
		  PhaseMeterBackgroundQImage = new QImage();

	  }
      update();
   }
}

QString DNRPhaseMeter::getPhaseMeterBackgroundFileName() const
{
   return FPhaseMeterBackgroundFileName;
}

