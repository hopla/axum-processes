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
#include "DNRImage.h"

DNRImage::DNRImage(QWidget *parent)
    : QWidget(parent)
{
    QString AxumSkinPath = QString(getenv(FSkinEnvironmentVariable.toAscii()));	 
    ImageQImage = new QImage(AxumSkinPath + "/" + FImageFileName);
    FScaleImage = true;

    setWindowTitle(tr("Image"));
    resize(50, 50);
}

void DNRImage::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);

    if (ImageQImage->isNull())
    {
		painter.setBrush(Qt::NoBrush);
		painter.setPen(QColor(0,0,0));

		painter.drawRect(0,0,width(),height());
    }
    else
    {
       if (FScaleImage)
       {
         double RatioX = (double)width()/ImageQImage->width();
         double RatioY = (double)height()/ImageQImage->height();
         double Ratio = RatioX;
         if (Ratio > RatioY)
         {
			 Ratio = RatioY;
		 }

         painter.scale(Ratio, Ratio);
       }
      painter.drawImage(0,0, *ImageQImage);
    }
}

void DNRImage::setSkinEnvironmentVariable(const QString &NewSkinEnvironmentVariable)
{
   if (FSkinEnvironmentVariable != NewSkinEnvironmentVariable)
   {
      FSkinEnvironmentVariable = NewSkinEnvironmentVariable;

      QString AxumSkinPath = QString(getenv(FSkinEnvironmentVariable.toAscii()));
      if (!ImageQImage->load(AxumSkinPath + "/" + FImageFileName))
      {
		  delete ImageQImage;
		  ImageQImage = new QImage();

	  }
      update();
   }
}

QString DNRImage::getSkinEnvironmentVariable() const
{
   return FSkinEnvironmentVariable;
}

void DNRImage::setImageFileName(const QString &NewImageFileName)
{
   if (FImageFileName != NewImageFileName)
   {
      FImageFileName = NewImageFileName;

      QString AxumSkinPath = QString(getenv(FSkinEnvironmentVariable.toAscii()));
      if (!ImageQImage->load(AxumSkinPath + "/" + FImageFileName))
      {
		  delete ImageQImage;
		  ImageQImage = new QImage();

	  }
      update();
   }
}

QString DNRImage::getImageFileName() const
{
   return FImageFileName;
}

void DNRImage::setScaleImage(bool NewScaleImage)
{
	if (FScaleImage != NewScaleImage)
	{
		FScaleImage = NewScaleImage;
		update();
	}
}

bool DNRImage::getScaleImage()
{
	return FScaleImage;
}
