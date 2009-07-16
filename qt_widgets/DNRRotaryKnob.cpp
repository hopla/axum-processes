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
#include "DNRDefines.h"
#include "DNRRotaryKnob.h"

DNRRotaryKnob::DNRRotaryKnob(QWidget *parent)
    : QWidget(parent)
{
   FNumber = 0;
   FPosition = 0;
   FScaleKnobImage = 0;
   FBorderColor = QColor(0,64,128);
   FKnobColor = QColor(192,224,255);

   PreviousDegrees = 0;

   QString AxumSkinPath = QString(getenv(FSkinEnvironmentVariable.toAscii()));
   RotaryKnobQImage = new QImage(AxumSkinPath + "/" + FRotaryKnobFileName);

   setWindowTitle(tr("Rotary Knob"));
   resize(40, 40);
}

void DNRRotaryKnob::paintEvent(QPaintEvent *)
{
   double side = qMin(width(), height());
   double Degrees = ((FPosition*280)/POSITION_RESOLUTION)-140;

   //if (PreviousDegrees != Degrees)
   {
      QPainter painter(this);
      painter.setRenderHint(QPainter::Antialiasing);
      painter.translate(width() / 2, height() / 2);

      if (RotaryKnobQImage->isNull())
      {
         painter.scale(side / 40.0, side / 40.0);

         painter.save();
         painter.rotate(Degrees);

         painter.setPen(FBorderColor);
         painter.setBrush(FKnobColor);
         painter.drawEllipse(-15,-15,30,30);

         painter.setBrush(FBorderColor);
         painter.drawRect(-2, -18, 4, 20);

         //painter.setBrush(BlackColor);
         painter.drawRect(-1, -1, 2, 2);

         painter.restore();
      }
      else
      {
         if (FScaleKnobImage)
         {
            double ImageWidth = RotaryKnobQImage->width();
            double ImageHeight = RotaryKnobQImage->height();
            double MaxLength = sqrt((ImageWidth*ImageWidth)+(ImageHeight*ImageHeight));

            painter.scale(side / MaxLength, side / MaxLength);
         }

         painter.save();
         painter.rotate(Degrees);
         painter.drawImage(-RotaryKnobQImage->width()/2,-RotaryKnobQImage->height()/2, *RotaryKnobQImage);
         painter.restore();
      }
      painter.setPen(Qt::NoPen);
      painter.setBrush(FKnobColor);
      
      PreviousDegrees = Degrees;
   }
}

void DNRRotaryKnob::setNumber(int NewNumber)
{
	if (FNumber != NewNumber)
	{
		FNumber = NewNumber;
	}
}

int DNRRotaryKnob::getNumber()
{
	return FNumber;
}

void DNRRotaryKnob::setPosition(double NewPosition)
{
   if (NewPosition != FPosition)
   {
      FPosition = NewPosition;
      update();
   }
}

void DNRRotaryKnob::setPosition(int_number ChannelNr, double_position NewPosition)
{
	if (FNumber == ChannelNr)
	{
		setPosition(NewPosition);
	}
}

double DNRRotaryKnob::getPosition()
{
   return FPosition;
}

void DNRRotaryKnob::setBorderColor(const QColor & NewBorderColor)
{
	FBorderColor = NewBorderColor;
}

const QColor & DNRRotaryKnob::getBorderColor() const
{
	return FBorderColor;
}

void DNRRotaryKnob::setKnobColor(const QColor & NewKnobColor)
{
	FKnobColor = NewKnobColor;
}

const QColor & DNRRotaryKnob::getKnobColor() const
{
	return FKnobColor;
}

void DNRRotaryKnob::setSkinEnvironmentVariable(const QString &NewSkinEnvironmentVariable)
{
   if (FSkinEnvironmentVariable != NewSkinEnvironmentVariable)
   {
      FSkinEnvironmentVariable = NewSkinEnvironmentVariable;

      QString AxumSkinPath = QString(getenv(FSkinEnvironmentVariable.toAscii()));
      if (!RotaryKnobQImage->load(AxumSkinPath + "/" + FRotaryKnobFileName))
      {
		  delete RotaryKnobQImage;
		  RotaryKnobQImage = new QImage();

	  }
      update();
   }
}

QString DNRRotaryKnob::getSkinEnvironmentVariable() const
{
   return FSkinEnvironmentVariable;
}

void DNRRotaryKnob::setRotaryKnobFileName(const QString &NewRotaryKnobFileName)
{
   if (FRotaryKnobFileName != NewRotaryKnobFileName)
   {
      FRotaryKnobFileName = NewRotaryKnobFileName;

      QString AxumSkinPath = QString(getenv(FSkinEnvironmentVariable.toAscii()));
      if (!RotaryKnobQImage->load(AxumSkinPath + "/" + FRotaryKnobFileName))
      {
		  delete RotaryKnobQImage;
		  RotaryKnobQImage = new QImage();

	  }
      update();
   }
}

QString DNRRotaryKnob::getRotaryKnobFileName() const
{
   return FRotaryKnobFileName;
}

void DNRRotaryKnob::setScaleKnobImage(bool NewScaleKnobImage)
{
   if (FScaleKnobImage != NewScaleKnobImage)
   {
      FScaleKnobImage = NewScaleKnobImage;
      update();
   }
}

bool DNRRotaryKnob::getScaleKnobImage()
{
   return FScaleKnobImage;
}

void DNRRotaryKnob::mouseMoveEvent(QMouseEvent *ev)
{
   if (ev->buttons() & Qt::LeftButton)
   {
	  int NewPosition;

	  int x = ev->x()-(width()/2);
	  int y = ev->y()-(height()/2);
	  double Rad;
	  double Degree;


	  if (x!=0)
	  {
		  if (x<0)
		  {
			  Rad = atan((float)y/x)-M_PI;
	      }
		  else
		  {
              Rad = atan((float)y/x);
	      }
      }
      else
      {
      	  if (y<0)
          {
          	  Rad = 1.5*M_PI;
	      }
          else
          {
              Rad = M_PI_2;
	      }
      }

      if (Rad<0)
          Rad += M_PI*2;

      Degree = ((Rad*360)/(2*M_PI));

	  Degree += 280/2+90;
      if (Degree>=360)
      {
      	Degree -= 360;
  	  }

  	  if ((Degree>=0) && (Degree<=280))
  	  {
		  NewPosition = (Degree*POSITION_RESOLUTION)/280;
  	  }
  	  else if ((Degree>=280) && (Degree<=320))
  	  {
		  NewPosition = POSITION_RESOLUTION;
      }
      else
      {
		  NewPosition = 0;
  	  }

      if (FPosition != NewPosition)
      {
	      emit KnobMoved(FNumber, NewPosition);
  	  }
   }
}

