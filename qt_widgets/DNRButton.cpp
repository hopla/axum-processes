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
#include "DNRDefines.h"
#include "DNRButton.h"

DNRButton::DNRButton(QWidget *parent)
    : QWidget(parent)
{
//    QTimer *timer = new QTimer(this);
//    connect(timer, SIGNAL(timeout()), this, SLOT(update()));
//    timer->start(1000);
	FNumber = 0;
    FPosition = false;
    FState = false;
	FBorderColor = QColor(0,64,128);
	FUpOffColor = QColor(255,255,255);
    FUpOnColor = QColor(192,224,255);
    FDownOffColor = QColor(255,255,255);
    FDownOnColor = QColor(192,224,255);

    QString AxumSkinPath = QString(getenv(FSkinEnvironmentVariable.toAscii()));
    UpOffImage = new QImage(AxumSkinPath + "/" + FUpOffImageFileName);
    UpOnImage = new QImage(AxumSkinPath + "/" + FUpOnImageFileName);
    DownOffImage = new QImage(AxumSkinPath + "/" + FDownOffImageFileName);
    DownOnImage = new QImage(AxumSkinPath + "/" + FDownOnImageFileName);

    setWindowTitle(tr("Button"));
    resize(64, 16);
}

void DNRButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing|QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
	double dx = 1600/width();
	double dy = 1600/height();

    if (FPosition == 0)
    {	//FPosition=Up
    	if (FState)
    	{ //FState=On
		    if (UpOnImage->isNull())
    		{
				painter.setPen(FBorderColor);
				painter.setBrush(FUpOnColor);

				painter.drawRoundRect( 1, 1, width()-2, height()-2, dx, dy);
    		}
		    else
    		{
			    painter.drawImage(0, 0, *UpOnImage);
		    }
		}
		else
		{ //FState=Off
		    if (UpOffImage->isNull())
    		{
				painter.setPen(FBorderColor);
				painter.setBrush(FUpOffColor);
				painter.drawRoundRect( 1, 1, width()-2, height()-2, dx, dy);
    		}
		    else
    		{
			    painter.drawImage(0, 0, *UpOffImage);
		    }
		}
	}
	else
	{	//FPosition=Down
    	if (FState)
    	{ //FState=On
		    if (DownOnImage->isNull())
    		{
				painter.setPen(FBorderColor);
				painter.setBrush(FDownOnColor);
				painter.drawRoundRect( 1, 1, width()-2, height()-2, dx, dy);
    		}
		    else
    		{
			    painter.drawImage(0, 0, *DownOnImage);
		    }
		}
		else
		{ //FState=Off
		    if (DownOffImage->isNull())
    		{
				painter.setPen(FBorderColor);
				painter.setBrush(FDownOffColor);
				painter.drawRoundRect( 1, 1, width()-2, height()-2, dx, dy);
    		}
		    else
    		{
			    painter.drawImage(0, 0, *DownOffImage);
		    }
		}
	}
    painter.drawText( 0, 0, width(), height(), Qt::AlignCenter, FButtonText);
}

void DNRButton::setNumber(int NewNumber)
{
	if (FNumber != NewNumber)
	{
		FNumber = NewNumber;
	}
}

int DNRButton::getNumber()
{
	return FNumber;
}

void DNRButton::setPosition(bool NewPosition)
{
   if (FPosition != NewPosition)
   {
      FPosition = NewPosition;
      update();

      double TempPosition = 0;
      if (FPosition)
      {
		  TempPosition = POSITION_RESOLUTION;
  	  }
      emit positionChanged(FNumber, TempPosition);
   }
}

bool DNRButton::getPosition()
{
   return FPosition;
}

void DNRButton::setState(bool NewState)
{
   if (FState != NewState)
   {
      FState = NewState;
      update();
   }
}

void DNRButton::setState(int_number Number, double_position NewState)
{
	if (FNumber == Number)
	{
		if (NewState > (POSITION_RESOLUTION/2))
		{
			setState(true);
		}
		else
		{
			setState(false);
		}
	}
}

bool DNRButton::getState()
{
   return FState;
}

void DNRButton::setButtonText(const QString &NewButtonText)
{
   if (FButtonText != NewButtonText)
   {
      FButtonText = NewButtonText;
      update();
   }
}

QString DNRButton::getButtonText() const
{
   return FButtonText;
}

void DNRButton::setSkinEnvironmentVariable(const QString &NewSkinEnvironmentVariable)
{
   if (FSkinEnvironmentVariable != NewSkinEnvironmentVariable)
   {
      FSkinEnvironmentVariable = NewSkinEnvironmentVariable;

      QString AxumSkinPath = QString(getenv(FSkinEnvironmentVariable.toAscii()));

      if (!UpOffImage->load(AxumSkinPath + "/" + FUpOffImageFileName))
      {
		  delete UpOffImage;
		  UpOffImage = new QImage();

	  }
      if (!UpOnImage->load(AxumSkinPath + "/" + FUpOnImageFileName))
      {
		  delete UpOnImage;
		  UpOnImage = new QImage();

	  }
      if (!DownOffImage->load(AxumSkinPath + "/" + FDownOffImageFileName))
      {
		  delete DownOffImage;
		  DownOffImage = new QImage();

	  }
      if (!DownOnImage->load(AxumSkinPath + "/" + FDownOnImageFileName))
      {
		  delete DownOnImage;
		  DownOnImage = new QImage();

	  }

      update();
   }
}

QString DNRButton::getSkinEnvironmentVariable() const
{
   return FSkinEnvironmentVariable;
}

void DNRButton::setUpOffImageFileName(const QString &NewUpOffImageFileName)
{
   if (FUpOffImageFileName != NewUpOffImageFileName)
   {
      FUpOffImageFileName = NewUpOffImageFileName;

      QString AxumSkinPath = QString(getenv(FSkinEnvironmentVariable.toAscii()));
      if (!UpOffImage->load(AxumSkinPath + "/" + FUpOffImageFileName))
      {
		  delete UpOffImage;
		  UpOffImage = new QImage();

	  }
      update();
   }
}

QString DNRButton::getUpOffImageFileName() const
{
   return FUpOffImageFileName;
}

void DNRButton::setUpOnImageFileName(const QString &NewUpOnImageFileName)
{
   if (FUpOnImageFileName != NewUpOnImageFileName)
   {
      FUpOnImageFileName = NewUpOnImageFileName;

      QString AxumSkinPath = QString(getenv(FSkinEnvironmentVariable.toAscii()));
      if (!UpOnImage->load(AxumSkinPath + "/" + FUpOnImageFileName))
      {
		  delete UpOnImage;
		  UpOnImage = new QImage();

	  }
      update();
   }
}

QString DNRButton::getUpOnImageFileName() const
{
   return FUpOnImageFileName;
}

void DNRButton::setDownOffImageFileName(const QString &NewDownOffImageFileName)
{
   if (FDownOffImageFileName != NewDownOffImageFileName)
   {
      FDownOffImageFileName = NewDownOffImageFileName;

      QString AxumSkinPath = QString(getenv(FSkinEnvironmentVariable.toAscii()));
      if (!DownOffImage->load(AxumSkinPath + "/" + FDownOffImageFileName))
      {
		  delete DownOffImage;
		  DownOffImage = new QImage();

	  }
      update();
   }
}

QString DNRButton::getDownOffImageFileName() const
{
   return FDownOffImageFileName;
}

void DNRButton::setDownOnImageFileName(const QString &NewDownOnImageFileName)
{
   if (FDownOnImageFileName != NewDownOnImageFileName)
   {
      FDownOnImageFileName = NewDownOnImageFileName;

      QString AxumSkinPath = QString(getenv(FSkinEnvironmentVariable.toAscii()));
      if (!DownOnImage->load(AxumSkinPath + "/" + FDownOnImageFileName))
      {
		  delete DownOnImage;
		  DownOnImage = new QImage();

	  }
      update();
   }
}

QString DNRButton::getDownOnImageFileName() const
{
   return FDownOnImageFileName;
}

void DNRButton::mousePressEvent(QMouseEvent *event)
{
	QWidget::mousePressEvent(event);

   if (event->buttons() & Qt::LeftButton)
   {
      setPosition(1);
   }
}

void DNRButton::mouseReleaseEvent(QMouseEvent *event)
{
   if (event->button() & Qt::LeftButton)
   {
      setPosition(0);
   }
}

void DNRButton::setBorderColor(const QColor & NewBorderColor)
{
	FBorderColor = NewBorderColor;
}

const QColor & DNRButton::getBorderColor() const
{
	return FBorderColor;
}

void DNRButton::setUpOffColor(const QColor & NewUpOffColor)
{
	FUpOffColor = NewUpOffColor;
}

const QColor & DNRButton::getUpOffColor() const
{
	return FUpOffColor;
}

void DNRButton::setUpOnColor(const QColor & NewUpOnColor)
{
	FUpOnColor = NewUpOnColor;
}

const QColor & DNRButton::getUpOnColor() const
{
	return FUpOnColor;
}

void DNRButton::setDownOffColor(const QColor & NewDownOffColor)
{
	FDownOffColor = NewDownOffColor;
}

const QColor & DNRButton::getDownOffColor() const
{
	return FDownOffColor;
}

void DNRButton::setDownOnColor(const QColor & NewDownOnColor)
{
	FDownOnColor = NewDownOnColor;
}

const QColor & DNRButton::getDownOnColor() const
{
	return FDownOnColor;
}
