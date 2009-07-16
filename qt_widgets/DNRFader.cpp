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

#include "DNRFader.h"

DNRFader::DNRFader(QWidget *parent)
    : QWidget(parent)
{
   FNumber = 0;
   FPosition = 0;
   FShaftWidth = 1;
   FBorderColor = QColor(0,64,128);
   FKnobColor = QColor(192,224,255);

   QString AxumSkinPath = QString(getenv(FSkinEnvironmentVariable.toAscii()));
   FaderKnobQImage = new QImage(AxumSkinPath + "/" + FFaderKnobFileName);

   setWindowTitle(tr("Fader"));
   resize(27, 256);
}

void DNRFader::paintEvent(QPaintEvent *)
{
    QColor BackgroundColor(0, 0, 0, 255);
    QColor ShaftColor(0, 0, 0);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    //painter.translate(width() / 2, height() / 2);

    double RatioY = height()/FADER_GRAPHICS_RESOLUTION_Y;
    double RatioX = width()/FADER_GRAPHICS_RESOLUTION_X;

    double Ratio = RatioY;
    if (RatioX<RatioY)
    {
      Ratio = RatioX;
    }
    painter.scale(Ratio, Ratio);

	if (FShaftWidth>0)
	{
    	painter.setPen(QPen(Qt::SolidPattern, FShaftWidth));
	    painter.drawLine(FADER_GRAPHICS_RESOLUTION_X/2, (FADER_GRAPHICS_RESOLUTION_Y-FADER_SHAFT_LENGTH)/2, FADER_GRAPHICS_RESOLUTION_X/2, FADER_GRAPHICS_RESOLUTION_Y-(FADER_GRAPHICS_RESOLUTION_Y-FADER_SHAFT_LENGTH)/2);
    }
    painter.setPen(FBorderColor);
    painter.setBrush(FKnobColor);

    if (FaderKnobQImage->isNull())
    {
		double dx = 1600/FADER_KNOB_WIDTH;
		double dy = 1600/FADER_KNOB_HEIGHT;

	    painter.drawRoundRect(1+(FADER_GRAPHICS_RESOLUTION_X-FADER_KNOB_WIDTH)/2,1+FADER_GRAPHICS_RESOLUTION_Y-(((FADER_GRAPHICS_RESOLUTION_Y-FADER_TRACK_LENGTH)/2)+(FADER_KNOB_HEIGHT/2)+((FPosition*FADER_TRACK_LENGTH)/POSITION_RESOLUTION)), FADER_KNOB_WIDTH-2, FADER_KNOB_HEIGHT-2, dx, dy);
	    painter.drawLine(1+(FADER_GRAPHICS_RESOLUTION_X-FADER_KNOB_WIDTH)/2,FADER_GRAPHICS_RESOLUTION_Y-(((FADER_GRAPHICS_RESOLUTION_Y-FADER_TRACK_LENGTH)/2)+((FPosition*FADER_TRACK_LENGTH)/POSITION_RESOLUTION)), FADER_KNOB_WIDTH-2, FADER_GRAPHICS_RESOLUTION_Y-(((FADER_GRAPHICS_RESOLUTION_Y-FADER_TRACK_LENGTH)/2)+((FPosition*FADER_TRACK_LENGTH)/POSITION_RESOLUTION)));
    }
    else
    {
      painter.drawImage((FADER_GRAPHICS_RESOLUTION_X-FADER_KNOB_WIDTH)/2,FADER_GRAPHICS_RESOLUTION_Y-(((FADER_GRAPHICS_RESOLUTION_Y-FADER_TRACK_LENGTH)/2)+(FADER_KNOB_HEIGHT/2)+((FPosition*FADER_TRACK_LENGTH)/POSITION_RESOLUTION)), *FaderKnobQImage);
    }

}

void DNRFader::setNumber(int NewNumber)
{
	if (FNumber != NewNumber)
	{
		FNumber = NewNumber;
	}
}

int DNRFader::getNumber()
{
	return FNumber;
}

void DNRFader::setPosition(double  NewPosition)
{
   if (NewPosition != FPosition)
   {
      FPosition = NewPosition;
      update();
   }
}

void DNRFader::setPosition(int_number Number, double_position NewPosition)
{
	if (FNumber == Number)
	{
		setPosition(NewPosition);
	}
}

double  DNRFader::getPosition()
{
   return FPosition;
}

void DNRFader::setShaftWidth(int NewShaftWidth)
{
   if (NewShaftWidth != FShaftWidth)
   {
      FShaftWidth = NewShaftWidth;
      update();
   }
}

int DNRFader::getShaftWidth()
{
   return FShaftWidth;
}

void DNRFader::setBorderColor(const QColor & NewBorderColor)
{
	FBorderColor = NewBorderColor;
}

const QColor & DNRFader::getBorderColor() const
{
	return FBorderColor;
}

void DNRFader::setKnobColor(const QColor & NewKnobColor)
{
	FKnobColor = NewKnobColor;
}

const QColor & DNRFader::getKnobColor() const
{
	return FKnobColor;
}

void DNRFader::setSkinEnvironmentVariable(const QString &NewSkinEnvironmentVariable)
{
   if (FSkinEnvironmentVariable != NewSkinEnvironmentVariable)
   {
      FSkinEnvironmentVariable = NewSkinEnvironmentVariable;

      QString AxumSkinPath = QString(getenv(FSkinEnvironmentVariable.toAscii()));
      if (!FaderKnobQImage->load(AxumSkinPath + "/" + FFaderKnobFileName))
      {
		  delete FaderKnobQImage;
		  FaderKnobQImage = new QImage();

	  }
      update();
   }
}

QString DNRFader::getSkinEnvironmentVariable() const
{
   return FSkinEnvironmentVariable;
}

void DNRFader::setFaderKnobFileName(const QString &NewFaderKnobFileName)
{
   if (FFaderKnobFileName != NewFaderKnobFileName)
   {
      FFaderKnobFileName = NewFaderKnobFileName;

      QString AxumSkinPath = QString(getenv(FSkinEnvironmentVariable.toAscii()));
      if (!FaderKnobQImage->load(AxumSkinPath + "/" + FFaderKnobFileName))
      {
		  delete FaderKnobQImage;
		  FaderKnobQImage = new QImage();

	  }
      update();
   }
}

QString DNRFader::getFaderKnobFileName() const
{
   return FFaderKnobFileName;
}

void DNRFader::mouseMoveEvent(QMouseEvent *ev)
{
   if (ev->buttons() & Qt::LeftButton)
   {
      int y;
      int NewPosition;
      double RatioY = height()/FADER_GRAPHICS_RESOLUTION_Y;
      double RatioX = width()/FADER_GRAPHICS_RESOLUTION_X;

      double Ratio = RatioY;
      if (RatioX<RatioY)
      {
         Ratio = RatioX;
      }

      double FaderGraphicsResolutionY = height()/Ratio;
      double DifferenceInResolutionY = FaderGraphicsResolutionY - FADER_GRAPHICS_RESOLUTION_Y;

      y = FaderGraphicsResolutionY-((ev->y()*FaderGraphicsResolutionY)/height());
      y -= DifferenceInResolutionY;

      if (y<((FADER_GRAPHICS_RESOLUTION_Y-FADER_TRACK_LENGTH)/2))
      {
         NewPosition = 0;
      }
      else if (y>(FADER_GRAPHICS_RESOLUTION_Y-((FADER_GRAPHICS_RESOLUTION_Y-FADER_TRACK_LENGTH)/2)))
      {
         NewPosition = POSITION_RESOLUTION;
      }
      else
      {
         y -= (FADER_GRAPHICS_RESOLUTION_Y-FADER_TRACK_LENGTH)/2;

         NewPosition = (y*POSITION_RESOLUTION)/FADER_TRACK_LENGTH;
      }

      if (FPosition != NewPosition)
      {
	      emit FaderMoved(FNumber, NewPosition);
  	  }
   }
}
