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
#include "DNRMovie.h"

DNRMovie::DNRMovie(QWidget *parent)
    : QWidget(parent)
{
    QString AxumSkinPath = QString(getenv(FSkinEnvironmentVariable.toAscii()));	 
    MovieQMovie = new QMovie(AxumSkinPath + "/" + FMovieFileName);
    FScaleMovie = true;

    connect(MovieQMovie, SIGNAL( frameChanged(int)), this, SLOT( paint()));

    setWindowTitle(tr("Movie"));
    resize(50, 50);
}

void DNRMovie::paint()
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(Qt::NoPen);

  const QImage & ImageQMovie = MovieQMovie->currentImage(); 

  if (ImageQMovie.isNull())
  {
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QColor(0,0,0));

    painter.drawRect(0,0,width(),height());
  }
  else
  {
    if (FScaleMovie)
    {
      double RatioX = (double)width()/ImageQMovie.width();
      double RatioY = (double)height()/ImageQMovie.height();
      double Ratio = RatioX;
      if (Ratio > RatioY)
      {
			  Ratio = RatioY;
		  }

      painter.scale(Ratio, Ratio);
    }
    painter.drawImage(0,0, ImageQMovie);
  } 
}

/*void DNRMovie::paintEvent(QPaintEvent *)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(Qt::NoPen);

  if (MovieQMovie->isNull())
  {
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QColor(0,0,0));

    painter.drawRect(0,0,width(),height());
  }
  else
  {
    if (FScaleMovie)
    {
      double RatioX = (double)width()/MovieQMovie->width();
      double RatioY = (double)height()/MovieQMovie->height();
      double Ratio = RatioX;
      if (Ratio > RatioY)
      {
			  Ratio = RatioY;
		  }

      painter.scale(Ratio, Ratio);
    }
    painter.drawImage(0,0, *MovieQMovie);
  }
}*/

void DNRMovie::setSkinEnvironmentVariable(const QString &NewSkinEnvironmentVariable)
{
  if (FSkinEnvironmentVariable != NewSkinEnvironmentVariable)
  {
    FSkinEnvironmentVariable = NewSkinEnvironmentVariable;

    QString AxumSkinPath = QString(getenv(FSkinEnvironmentVariable.toAscii()));
    MovieQMovie->setFileName(AxumSkinPath + "/" + FMovieFileName);
    update();
  }
}

QString DNRMovie::getSkinEnvironmentVariable() const
{
   return FSkinEnvironmentVariable;
}

void DNRMovie::setMovieFileName(const QString &NewMovieFileName)
{
  if (FMovieFileName != NewMovieFileName)
  {
    FMovieFileName = NewMovieFileName;

    QString AxumSkinPath = QString(getenv(FSkinEnvironmentVariable.toAscii()));
    MovieQMovie->setFileName(AxumSkinPath + "/" + FMovieFileName);
    update();
  }
}

QString DNRMovie::getMovieFileName() const
{
   return FMovieFileName;
}

void DNRMovie::setScaleMovie(bool NewScaleMovie)
{
	if (FScaleMovie != NewScaleMovie)
	{
		FScaleMovie = NewScaleMovie;
		update();
	}
}

bool DNRMovie::getScaleMovie()
{
	return FScaleMovie;
}
