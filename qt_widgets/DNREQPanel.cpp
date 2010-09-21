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

#include "DNREQPanel.h"

#ifdef Q_OS_WIN32
#include <windows.h> // for QueryPerformanceCounter
#endif

DNREQPanel::DNREQPanel(QWidget *parent)
    : QWidget(parent)
{
}

void DNREQPanel::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setPen(Qt::NoPen);

}

QColor DNREQPanel::getBackgroundColor()
{
  return FBackgroundColor;
}

void DNREQPanel::setBackgroundColor(QColor NewBackgroundColor)
{
  if (FBackgroundColor != NewBackgroundColor)
  {
    FBackgroundColor = NewBackgroundColor;
    update();
  }
}

QColor DNREQPanel::getAxisColor()
{
  return FAxisColor;
}

void DNREQPanel::setAxisColor(QColor NewAxisColor)
{
  if (FAxisColor != NewAxisColor)
  {
    FAxisColor = NewAxisColor;
    update();
  }
}


QColor DNREQPanel::getGridColor()
{
  return FGridColor;
}

void DNREQPanel::setGridColor(QColor NewGridColor)
{
  if (FGridColor != NewGridColor)
  {
    FGridColor = NewGridColor;
  }
}

QColor DNREQPanel::getLogGridColor()
{
  return FLogGridColor;
}

void DNREQPanel::setLogGridColor(QColor NewLogGridColor)
{
  if (FLogGridColor != NewLogGridColor)
  {
    FLogGridColor = NewLogGridColor;
  }
}

int DNREQPanel::getVerticalGridAtdB()
{
  return FVerticalGridAtdB;
}

void DNREQPanel::setVerticalGridAtdB(int NewVerticalGridAtdB)
{
  if (FVerticalGridAtdB != NewVerticalGridAtdB)
  {
    FVerticalGridAtdB = NewVerticalGridAtdB;
    update();
  }
}

float DNREQPanel::getAnchorSize()
{
  return FAnchorSize;
}

void DNREQPanel::setAnchorSize(float NewAnchorSize)
{
  if (FAnchorSize != NewAnchorSize)
  {
    FAnchorSize = NewAnchorSize;
    update();
  }
}

float DNREQPanel::getGainBand1()
{
  return FGainBand1;
}

void DNREQPanel::setGainBand1(float NewGainBand1)
{
  if (FGainBand1 != NewGainBand1)
  {
    FGainBand1 = NewGainBand1;
    update();
  }
}

float DNREQPanel::getGainBand2()
{
  return FGainBand2;
}

void DNREQPanel::setGainBand2(float NewGainBand2)
{
  if (FGainBand2 != NewGainBand2)
  {
    FGainBand2 = NewGainBand2;
    update();
  }
}

float DNREQPanel::getGainBand3()
{
  return FGainBand3;
}

void DNREQPanel::setGainBand3(float NewGainBand3)
{
  if (FGainBand3 != NewGainBand3)
  {
    FGainBand3 = NewGainBand3;
    update();
  }
}

float DNREQPanel::getGainBand4()
{
  return FGainBand4;
}

void DNREQPanel::setGainBand4(float NewGainBand4)
{
  if (FGainBand4 != NewGainBand4)
  {
    FGainBand4 = NewGainBand4;
    update();
  }
}

float DNREQPanel::getGainBand5()
{
  return FGainBand5;
}

void DNREQPanel::setGainBand5(float NewGainBand5)
{
  if (FGainBand5 != NewGainBand5)
  {
    FGainBand5 = NewGainBand5;
    update();
  }
}

float DNREQPanel::getGainBand6()
{
  return FGainBand6;
}

void DNREQPanel::setGainBand6(float NewGainBand6)
{
  if (FGainBand6 != NewGainBand6)
  {
    FGainBand6 = NewGainBand6;
    update();
  }
}

float DNREQPanel::getFrequencyBand1()
{
  return FFrequencyBand1;
}

void DNREQPanel::setFrequencyBand1(float NewFrequencyBand1)
{
  if (FFrequencyBand1 != NewFrequencyBand1)
  {
    FFrequencyBand1 = NewFrequencyBand1;
    update();
  }
}

float DNREQPanel::getFrequencyBand2()
{
  return FFrequencyBand2;
}

void DNREQPanel::setFrequencyBand2(float NewFrequencyBand2)
{
  if (FFrequencyBand2 != NewFrequencyBand2)
  {
    FFrequencyBand2 = NewFrequencyBand2;
    update();
  }
}

float DNREQPanel::getFrequencyBand3()
{
  return FFrequencyBand3;
}

void DNREQPanel::setFrequencyBand3(float NewFrequencyBand3)
{
  if (FFrequencyBand3 != NewFrequencyBand3)
  {
    FFrequencyBand3 = NewFrequencyBand3;
    update();
  }
}

float DNREQPanel::getFrequencyBand4()
{
  return FFrequencyBand4;
}

void DNREQPanel::setFrequencyBand4(float NewFrequencyBand4)
{
  if (FFrequencyBand4 != NewFrequencyBand4)
  {
    FFrequencyBand4 = NewFrequencyBand4;
    update();
  }
}

float DNREQPanel::getFrequencyBand5()
{
  return FFrequencyBand5;
}

void DNREQPanel::setFrequencyBand5(float NewFrequencyBand5)
{
  if (FFrequencyBand5 != NewFrequencyBand5)
  {
    FFrequencyBand5 = NewFrequencyBand5;
    update();
  }
}

float DNREQPanel::getFrequencyBand6()
{
  return FFrequencyBand6;
}

void DNREQPanel::setFrequencyBand6(float NewFrequencyBand6)
{
  if (FFrequencyBand6 != NewFrequencyBand6)
  {
    FFrequencyBand6 = NewFrequencyBand6;
    update();
  }
}


float DNREQPanel::getBandwidthBand1()
{
  return FBandwidthBand1;
}

void DNREQPanel::setBandwidthBand1(float NewBandwidthBand1)
{
  if (FBandwidthBand1 != NewBandwidthBand1)
  {
    FBandwidthBand1 = NewBandwidthBand1;
    update();
  }
}

float DNREQPanel::getBandwidthBand2()
{
  return FBandwidthBand2;
}

void DNREQPanel::setBandwidthBand2(float NewBandwidthBand2)
{
  if (FBandwidthBand2 != NewBandwidthBand2)
  {
    FBandwidthBand2 = NewBandwidthBand2;
    update();
  }
}

float DNREQPanel::getBandwidthBand3()
{
  return FBandwidthBand3;
}

void DNREQPanel::setBandwidthBand3(float NewBandwidthBand3)
{
  if (FBandwidthBand3 != NewBandwidthBand3)
  {
    FBandwidthBand3 = NewBandwidthBand3;
    update();
  }
}

float DNREQPanel::getBandwidthBand4()
{
  return FBandwidthBand4;
}

void DNREQPanel::setBandwidthBand4(float NewBandwidthBand4)
{
  if (FBandwidthBand4 != NewBandwidthBand4)
  {
    FBandwidthBand4 = NewBandwidthBand4;
    update();
  }
}

float DNREQPanel::getBandwidthBand5()
{
  return FBandwidthBand5;
}

void DNREQPanel::setBandwidthBand5(float NewBandwidthBand5)
{
  if (FBandwidthBand5 != NewBandwidthBand5)
  {
    FBandwidthBand5 = NewBandwidthBand5;
    update();
  }
}

float DNREQPanel::getBandwidthBand6()
{
  return FBandwidthBand6;
}

void DNREQPanel::setBandwidthBand6(float NewBandwidthBand6)
{
  if (FBandwidthBand6 != NewBandwidthBand6)
  {
    FBandwidthBand6 = NewBandwidthBand6;
    update();
  }
}


int DNREQPanel::getNrOfPoints()
{
  return FNrOfPoints;
}

void DNREQPanel::setNrOfPoints(int NewNrOfPoints)
{
  if (FNrOfPoints != NewNrOfPoints)
  {
    FNrOfPoints = NewNrOfPoints;
    update();
  }
}

int DNREQPanel::getAxisBorderWidth()
{
  return FAxisBorderWidth;
}

void DNREQPanel::setAxisBorderWidth(int NewAxisBorderWidth)
{
  if (FAxisBorderWidth != NewAxisBorderWidth)
  {
    FAxisBorderWidth = NewAxisBorderWidth;
    update();
  }
}

int DNREQPanel::getAxisLeftMargin()
{
  return FAxisLeftMargin;
}

void DNREQPanel::setAxisLeftMargin(int NewAxisLeftMargin)
{
  if (FAxisLeftMargin != NewAxisLeftMargin)
  {
    FAxisLeftMargin = NewAxisLeftMargin;
    update();
  }
}

QColor DNREQPanel::getActiveCurveColor()
{
  return FActiveCurveColor;
}

void DNREQPanel::setActiveCurveColor(QColor NewActiveCurveColor)
{
  if (FActiveCurveColor != NewActiveCurveColor)
  {
    FActiveCurveColor = NewActiveCurveColor;
    update();
  }
}

int DNREQPanel::getActiveCurveWidth()
{
  return FActiveCurveWidth;
}

void DNREQPanel::setActiveCurveWidth(int NewActiveCurveWidth)
{
  if (FActiveCurveWidth != NewActiveCurveWidth)
  {
    FActiveCurveWidth = NewActiveCurveWidth;
    update();
  }
}

QColor DNREQPanel::getTotalCurveColor()
{
  return FTotalCurveColor;
}

void DNREQPanel::setTotalCurveColor(QColor NewTotalCurveColor)
{
  if (FTotalCurveColor != NewTotalCurveColor)
  {
    FTotalCurveColor = NewTotalCurveColor;
    update();
  }
}

int DNREQPanel::getTotalCurveWidth()
{
  return FTotalCurveWidth;
}

void DNREQPanel::setTotalCurveWidth(int NewTotalCurveWidth)
{
  if (FTotalCurveWidth != NewTotalCurveWidth)
  {
    FTotalCurveWidth = NewTotalCurveWidth;
    update();
  }
}

QColor DNREQPanel::getAnchorBand1Color()
{
  return FAnchorBand1Color;
}

void DNREQPanel::setAnchorBand1Color(QColor NewAnchorBand1Color)
{
  if (FAnchorBand1Color != NewAnchorBand1Color)
  {
    FAnchorBand1Color = NewAnchorBand1Color;
    update();
  }
}

QColor DNREQPanel::getAnchorBand2Color()
{
  return FAnchorBand2Color;
}

void DNREQPanel::setAnchorBand2Color(QColor NewAnchorBand2Color)
{
  if (FAnchorBand2Color != NewAnchorBand2Color)
  {
    FAnchorBand2Color = NewAnchorBand2Color;
    update();
  }
}

QColor DNREQPanel::getAnchorBand3Color()
{
  return FAnchorBand3Color;
}

void DNREQPanel::setAnchorBand3Color(QColor NewAnchorBand3Color)
{
  if (FAnchorBand3Color != NewAnchorBand3Color)
  {
    FAnchorBand3Color = NewAnchorBand3Color;
    update();
  }
}

QColor DNREQPanel::getAnchorBand4Color()
{
  return FAnchorBand4Color;
}

void DNREQPanel::setAnchorBand4Color(QColor NewAnchorBand4Color)
{
  if (FAnchorBand4Color != NewAnchorBand4Color)
  {
    FAnchorBand4Color = NewAnchorBand4Color;
    update();
  }
}

QColor DNREQPanel::getAnchorBand5Color()
{
  return FAnchorBand5Color;
}

void DNREQPanel::setAnchorBand5Color(QColor NewAnchorBand5Color)
{
  if (FAnchorBand5Color != NewAnchorBand5Color)
  {
    FAnchorBand5Color = NewAnchorBand5Color;
    update();
  }
}

QColor DNREQPanel::getAnchorBand6Color()
{
  return FAnchorBand6Color;
}

void DNREQPanel::setAnchorBand6Color(QColor NewAnchorBand6Color)
{
  if (FAnchorBand6Color != NewAnchorBand6Color)
  {
    FAnchorBand6Color = NewAnchorBand6Color;
    update();
  }
}

bool DNREQPanel::getDrawAnchors()
{
  return FDrawAnchors;
}

void DNREQPanel::setDrawAnchors(bool NewDrawAnchors)
{
  if (FDrawAnchors != NewDrawAnchors)
  {
    FDrawAnchors = NewDrawAnchors;
    update();
  }
}

int DNREQPanel::getTypeBand1()
{
  return FTypeBand1;
}

void DNREQPanel::setTypeBand1(int NewTypeBand1)
{
  if (FTypeBand1 != NewTypeBand1)
  {
    FTypeBand1 = NewTypeBand1;
    update();
  }
}

int DNREQPanel::getTypeBand2()
{
  return FTypeBand2;
}

void DNREQPanel::setTypeBand2(int NewTypeBand2)
{
  if (FTypeBand2 != NewTypeBand2)
  {
    FTypeBand2 = NewTypeBand2;
    update();
  }
}

int DNREQPanel::getTypeBand3()
{
  return FTypeBand3;
}

void DNREQPanel::setTypeBand3(int NewTypeBand3)
{
  if (FTypeBand3 != NewTypeBand3)
  {
    FTypeBand3 = NewTypeBand3;
    update();
  }
}

int DNREQPanel::getTypeBand4()
{
  return FTypeBand4;
}

void DNREQPanel::setTypeBand4(int NewTypeBand4)
{
  if (FTypeBand4 != NewTypeBand4)
  {
    FTypeBand4 = NewTypeBand4;
    update();
  }
}

int DNREQPanel::getTypeBand5()
{
  return FTypeBand5;
}

void DNREQPanel::setTypeBand5(int NewTypeBand5)
{
  if (FTypeBand5 != NewTypeBand5)
  {
    FTypeBand5 = NewTypeBand5;
    update();
  }
}

int DNREQPanel::getTypeBand6()
{
  return FTypeBand6;
}

void DNREQPanel::setTypeBand6(int NewTypeBand6)
{
  if (FTypeBand6 != NewTypeBand6)
  {
    FTypeBand6 = NewTypeBand6;
    update();
  }
}


int DNREQPanel::getSamplerate()
{
  return FSamplerate;
}

void DNREQPanel::setSamplerate(int NewSamplerate)
{
  if (FSamplerate != NewSamplerate)
  {
    FSamplerate = NewSamplerate;
    update();
  }
}

double DNREQPanel::getNequistDivide()
{
  return FNequistDivide;
}

void DNREQPanel::setNequistDivide(double NewNequistDivide)
{
  if (FNequistDivide != NewNequistDivide)
  {
    FNequistDivide = NewNequistDivide;
    update();
  }
}

int DNREQPanel::getAnchorPickupSize()
{
  return FAnchorPickupSize;
}

void DNREQPanel::setAnchorPickupSize(int NewAnchorPickupSize)
{
  if (FAnchorPickupSize != NewAnchorPickupSize)
  {
    FAnchorPickupSize = NewAnchorPickupSize;
    update();   
  }
}

bool DNREQPanel::getLogGrid()
{
  return FLogGrid;
}

void DNREQPanel::setLogGrid(bool NewLogGrid)
{
  if (FLogGrid != NewLogGrid)
  {
    FLogGrid = NewLogGrid;
    update();
  }
}

bool DNREQPanel::getEQOn()
{
  return FEQOn;
}

void DNREQPanel::setEQOn(bool NewEQOn)
{
  if (FEQOn != NewEQOn)
  {
    FEQOn = NewEQOn;
    update();
  }
}

bool DNREQPanel::getOnBand1()
{
  return FOnBand1;
}

void DNREQPanel::setOnBand1(bool NewOnBand1)
{
  if (FOnBand1 != NewOnBand1)
  {
    FOnBand1 = NewOnBand1;
    update();
  }
}

bool DNREQPanel::getOnBand2()
{
  return FOnBand2;
}

void DNREQPanel::setOnBand2(bool NewOnBand2)
{
  if (FOnBand2 != NewOnBand2)
  {
    FOnBand2 = NewOnBand2;
    update();
  }
}

bool DNREQPanel::getOnBand3()
{
  return FOnBand3;
}

void DNREQPanel::setOnBand3(bool NewOnBand3)
{
  if (FOnBand3 != NewOnBand3)
  {
    FOnBand3 = NewOnBand3;
    update();
  }
}

bool DNREQPanel::getOnBand4()
{
  return FOnBand4;
}

void DNREQPanel::setOnBand4(bool NewOnBand4)
{
  if (FOnBand4 != NewOnBand4)
  {
    FOnBand4 = NewOnBand4;
    update();
  }
}

bool DNREQPanel::getOnBand5()
{
  return FOnBand5;
}

void DNREQPanel::setOnBand5(bool NewOnBand5)
{
  if (FOnBand5 != NewOnBand5)
  {
    FOnBand5 = NewOnBand5;
    update();
  }
}

bool DNREQPanel::getOnBand6()
{
  return FOnBand6;
}

void DNREQPanel::setOnBand6(bool NewOnBand6)
{
  if (FOnBand6 != NewOnBand6)
  {
    FOnBand6 = NewOnBand6;
    update();
  }
}

bool DNREQPanel::getShowFrequencyText()
{
  return FShowFrequencyText;
}

void DNREQPanel::setShowFrequencyText(bool NewShowFrequencyText)
{
  if (FShowFrequencyText != NewShowFrequencyText)
  {
    FShowFrequencyText = NewShowFrequencyText;
    update();
  }
}

bool DNREQPanel::getShowGainText()
{
  return FShowGainText;
}

void DNREQPanel::setShowGainText(bool NewShowGainText)
{
  if (FShowGainText != NewShowGainText)
  {
    FShowGainText = NewShowGainText;
    update();
  }
}

float DNREQPanel::getSlopeBand1()
{
  return FSlopeBand1;
}

void DNREQPanel::setSlopeBand1(float NewSlopeBand1)
{
  if (FSlopeBand1 != NewSlopeBand1)
  {
    FSlopeBand1 = NewSlopeBand1;
    update();
  }
}

float DNREQPanel::getSlopeBand2()
{
  return FSlopeBand2;
}

void DNREQPanel::setSlopeBand2(float NewSlopeBand2)
{
  if (FSlopeBand2 != NewSlopeBand2)
  {
    FSlopeBand2 = NewSlopeBand2;
    update();
  }
}

float DNREQPanel::getSlopeBand3()
{
  return FSlopeBand3;
}

void DNREQPanel::setSlopeBand3(float NewSlopeBand3)
{
  if (FSlopeBand3 != NewSlopeBand3)
  {
    FSlopeBand3 = NewSlopeBand3;
    update();
  }
}

float DNREQPanel::getSlopeBand4()
{
  return FSlopeBand4;
}

void DNREQPanel::setSlopeBand4(float NewSlopeBand4)
{
  if (FSlopeBand4 != NewSlopeBand4)
  {
    FSlopeBand4 = NewSlopeBand4;
    update();
  }
}

float DNREQPanel::getSlopeBand5()
{
  return FSlopeBand5;
}

void DNREQPanel::setSlopeBand5(float NewSlopeBand5)
{
  if (FSlopeBand5 != NewSlopeBand5)
  {
    FSlopeBand5 = NewSlopeBand5;
    update();
  }
}

float DNREQPanel::getSlopeBand6()
{
  return FSlopeBand6;
}

void DNREQPanel::setSlopeBand6(float NewSlopeBand6)
{
  if (FSlopeBand6 != NewSlopeBand6)
  {
    FSlopeBand6 = NewSlopeBand6;
    update();
  }
}

