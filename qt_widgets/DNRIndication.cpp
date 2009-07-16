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
#include "DNRIndication.h"

DNRIndication::DNRIndication(QWidget *parent)
    : QWidget(parent)
{
    FNumber = 0;
    FState = 0;

    QString AxumSkinPath = QString(getenv(FSkinEnvironmentVariable.toAscii()));
    State1QImage = new QImage(AxumSkinPath + "/" + FState1ImageFileName);
    State2QImage = new QImage(AxumSkinPath + "/" + FState2ImageFileName);
    State3QImage = new QImage(AxumSkinPath + "/" + FState3ImageFileName);
    State4QImage = new QImage(AxumSkinPath + "/" + FState4ImageFileName);

    setWindowTitle(tr("Indication"));
    resize(16, 16);
}

void DNRIndication::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    switch (FState)
	{
		case 0:
		{
		    if (State1QImage->isNull())
    		{
				painter.setBrush(Qt::NoBrush);
				painter.setPen(QColor(0,0,0));

				painter.drawRect(0,0,width(),height());
      			painter.drawText( 0, 0, width(), height(), Qt::AlignCenter, "1");
    		}
		    else
    		{
			    painter.drawImage(0, 0, *State1QImage);
		    }
		}
		break;
		case 1:
		{
		    if (State2QImage->isNull())
    		{
				painter.setBrush(Qt::NoBrush);
				painter.setPen(QColor(0,0,0));

				painter.drawRect(0,0,width(),height());
      			painter.drawText( 0, 0, width(), height(), Qt::AlignCenter, "2");
    		}
		    else
    		{
			    painter.drawImage(0, 0, *State2QImage);
		    }
		}
		break;
		case 2:
		{
		    if (State3QImage->isNull())
    		{
				painter.setBrush(Qt::NoBrush);
				painter.setPen(QColor(0,0,0));

				painter.drawRect(0,0,width(),height());
      			painter.drawText( 0, 0, width(), height(), Qt::AlignCenter, "3");
    		}
		    else
    		{
			    painter.drawImage(0, 0, *State3QImage);
		    }
		}
		break;
		case 3:
		{
		    if (State4QImage->isNull())
    		{
				painter.setBrush(Qt::NoBrush);
				painter.setPen(QColor(0,0,0));

				painter.drawRect(0,0,width(),height());
      			painter.drawText( 0, 0, width(), height(), Qt::AlignCenter, "4");
    		}
		    else
    		{
			    painter.drawImage(0, 0, *State4QImage);
		    }
		}
		break;
		default:
		{
		}
		break;
	}
}

void DNRIndication::setNumber(int NewNumber)
{
   if (FNumber != NewNumber)
   {
      FNumber = NewNumber;
   }
}

int DNRIndication::getNumber()
{
	return FNumber;
}


void DNRIndication::setState(int NewState)
{
   if (FState != NewState)
   {
      FState = NewState;
      update();
   }
}

int DNRIndication::getState()
{
   return FState;
}

void DNRIndication::setState(int_number Number, double_position NewState)
{
	if (FNumber == Number)
	{
		if (NewState < (POSITION_RESOLUTION/4))
		{
			setState(0);
		}
		else if (NewState < (POSITION_RESOLUTION/2))
		{
			setState(1);
		}
		else if (NewState < ((POSITION_RESOLUTION*3)/4))
		{
			setState(2);
		}
		else
		{
			setState(3);
		}
	}
}

void DNRIndication::setSkinEnvironmentVariable(const QString &NewSkinEnvironmentVariable)
{
   if (FSkinEnvironmentVariable != NewSkinEnvironmentVariable)
   {
      FSkinEnvironmentVariable = NewSkinEnvironmentVariable;

      QString AxumSkinPath = QString(getenv(FSkinEnvironmentVariable.toAscii()));

      if (!State1QImage->load(AxumSkinPath + "/" + FState1ImageFileName))
      {
		  delete State1QImage;
		  State1QImage = new QImage();

	  }
      if (!State2QImage->load(AxumSkinPath + "/" + FState2ImageFileName))
      {
		  delete State2QImage;
		  State2QImage = new QImage();

	  }
      if (!State3QImage->load(AxumSkinPath + "/" + FState3ImageFileName))
      {
		  delete State3QImage;
		  State3QImage = new QImage();

	  }
      if (!State4QImage->load(AxumSkinPath + "/" + FState4ImageFileName))
      {
		  delete State4QImage;
		  State4QImage = new QImage();

	  }

      update();
   }
}

QString DNRIndication::getSkinEnvironmentVariable() const
{
   return FSkinEnvironmentVariable;
}

void DNRIndication::setState1ImageFileName(const QString &NewState1ImageFileName)
{
   if (FState1ImageFileName != NewState1ImageFileName)
   {
      FState1ImageFileName = NewState1ImageFileName;

      QString AxumSkinPath = QString(getenv(FSkinEnvironmentVariable.toAscii()));
      if (!State1QImage->load(AxumSkinPath + "/" + FState1ImageFileName))
      {
		  delete State1QImage;
		  State1QImage = new QImage();

	  }
      update();
   }
}

QString DNRIndication::getState1ImageFileName() const
{
   return FState1ImageFileName;
}

void DNRIndication::setState2ImageFileName(const QString &NewState2ImageFileName)
{
   if (FState2ImageFileName != NewState2ImageFileName)
   {
      FState2ImageFileName = NewState2ImageFileName;

      QString AxumSkinPath = QString(getenv(FSkinEnvironmentVariable.toAscii()));
      if (!State2QImage->load(AxumSkinPath + "/" + FState2ImageFileName))
      {
		  delete State2QImage;
		  State2QImage = new QImage();

	  }
      update();
   }
}

QString DNRIndication::getState2ImageFileName() const
{
   return FState2ImageFileName;
}

void DNRIndication::setState3ImageFileName(const QString &NewState3ImageFileName)
{
   if (FState3ImageFileName != NewState3ImageFileName)
   {
      FState3ImageFileName = NewState3ImageFileName;

      QString AxumSkinPath = QString(getenv(FSkinEnvironmentVariable.toAscii()));
      if (!State3QImage->load(AxumSkinPath + "/" + FState3ImageFileName))
      {
		  delete State3QImage;
		  State3QImage = new QImage();

	  }
      update();
   }
}

QString DNRIndication::getState3ImageFileName() const
{
   return FState3ImageFileName;
}

void DNRIndication::setState4ImageFileName(const QString &NewState4ImageFileName)
{
   if (FState4ImageFileName != NewState4ImageFileName)
   {
      FState4ImageFileName = NewState4ImageFileName;

      QString AxumSkinPath = QString(getenv(FSkinEnvironmentVariable.toAscii()));
      if (!State4QImage->load(AxumSkinPath + "/" + FState4ImageFileName))
      {
		  delete State4QImage;
		  State4QImage = new QImage();

	  }
      update();
   }
}

QString DNRIndication::getState4ImageFileName() const
{
   return FState4ImageFileName;
}

