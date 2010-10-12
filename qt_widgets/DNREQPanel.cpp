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
  float Coefficients[6];

  FBackgroundColor = QColor(22,51,105);
  FAxisColor = QColor(135, 168, 228);
  FGridColor = QColor(35, 80, 163);
  FLogGridColor = QColor(35, 80, 163);
  FTextColor = QColor(192,192,192);
  FVerticalGridAtdB = 6;
  FAnchorSize = 3;
  FGainBand1 = 0;
  FGainBand2 = 0;
  FGainBand3 = 0;
  FGainBand4 = 0;
  FGainBand5 = 0;
  FGainBand6 = 0;
  FFrequencyBand1 = 12000;
  FFrequencyBand2 = 10000;
  FFrequencyBand3 = 8000;
  FFrequencyBand4 = 6000;
  FFrequencyBand5 = 4000;
  FFrequencyBand6 = 2000;
  FBandwidthBand1 = 1;
  FBandwidthBand2 = 1;
  FBandwidthBand3 = 1;
  FBandwidthBand4 = 1;
  FBandwidthBand5 = 1;
  FBandwidthBand6 = 1;
  FNrOfPoints = 1024;
  FAxisBorderWidth = 20;
  FAxisLeftMargin = 20;
  FActiveCurveColor = QColor(192, 192, 192);
  FActiveCurveWidth = 1;
  FActiveCurveFillColor = QColor (0, 255, 0, 80);
  FInactiveCurveColor = QColor(0, 0, 0);
  FInactiveCurveWidth = 1;
  FInactiveCurveFillColor = QColor (0, 0, 0, 0);
  FAnchorBand1Color = QColor(226, 197, 182);
  FAnchorBand2Color = QColor(255, 255, 196);
  FAnchorBand3Color = QColor(199, 243, 205);
  FAnchorBand4Color = QColor(236, 213, 253);
  FAnchorBand5Color = QColor(187, 221, 255);
  FAnchorBand6Color = QColor(221, 221, 221);
  FDrawAnchors = true;
  FTypeBand1 = 3;
  FTypeBand2 = 3;
  FTypeBand3 = 3;
  FTypeBand4 = 3;
  FTypeBand5 = 3;
  FTypeBand6 = 3;
  FSamplerate = 48000;
  FNequistDivide = 2.04;
  FAnchorPickupSize = 6;
  FLogGrid = true;
  FEQOn = false;
  FOnBand1 = true;
  FOnBand2 = true;
  FOnBand3 = true;
  FOnBand4 = true;
  FOnBand5 = true;
  FOnBand6 = true;
  FShowFrequencyText = true;
  FShowGainText = true;
  FSlopeBand1 = 1;
  FSlopeBand2 = 1;
  FSlopeBand3 = 1;
  FSlopeBand4 = 1;
  FSlopeBand5 = 1;
  FSlopeBand6 = 1;

  GainFactorBand1 = CalculateEQ(Coefficients, FGainBand1, FFrequencyBand1, FBandwidthBand1, FSlopeBand1, FTypeBand1, FOnBand1);
  GainFactorBand2 = CalculateEQ(Coefficients, FGainBand2, FFrequencyBand2, FBandwidthBand2, FSlopeBand2, FTypeBand2, FOnBand2);
  GainFactorBand3 = CalculateEQ(Coefficients, FGainBand3, FFrequencyBand3, FBandwidthBand3, FSlopeBand3, FTypeBand3, FOnBand3);
  GainFactorBand4 = CalculateEQ(Coefficients, FGainBand4, FFrequencyBand4, FBandwidthBand4, FSlopeBand4, FTypeBand4, FOnBand4);
  GainFactorBand5 = CalculateEQ(Coefficients, FGainBand5, FFrequencyBand5, FBandwidthBand5, FSlopeBand5, FTypeBand5, FOnBand5);
  GainFactorBand6 = CalculateEQ(Coefficients, FGainBand6, FFrequencyBand6, FBandwidthBand6, FSlopeBand6, FTypeBand6, FOnBand6);
  CalculateZeroPosition(ZerosXBand1, ZerosYBand1, Coefficients);
  CalculatePolePosition(PolesXBand1, PolesYBand1, Coefficients);
  CalculateZeroPosition(ZerosXBand2, ZerosYBand2, Coefficients);
  CalculatePolePosition(PolesXBand2, PolesYBand2, Coefficients);
  CalculateZeroPosition(ZerosXBand3, ZerosYBand3, Coefficients);
  CalculatePolePosition(PolesXBand3, PolesYBand3, Coefficients);
  CalculateZeroPosition(ZerosXBand4, ZerosYBand4, Coefficients);
  CalculatePolePosition(PolesXBand4, PolesYBand4, Coefficients);
  CalculateZeroPosition(ZerosXBand5, ZerosYBand5, Coefficients);
  CalculatePolePosition(PolesXBand5, PolesYBand5, Coefficients);
  CalculateZeroPosition(ZerosXBand6, ZerosYBand6, Coefficients);
  CalculatePolePosition(PolesXBand6, PolesYBand6, Coefficients);
  CalculateCurve();
}

void DNREQPanel::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setPen(Qt::NoPen);
	painter.setBrush(FBackgroundColor);

	painter.drawRect(0,0, width(), height());

	int BorderWidth = FAxisBorderWidth;
	int HorizontalAxisLength = width()-(2*BorderWidth)-FAxisLeftMargin;
	int VerticalAxisLength = height()-(2*BorderWidth);
	int DistanceTo10k = ((float)HorizontalAxisLength*(log10(10000)-1))/log10((FSamplerate)/(FNequistDivide*10));
	int TextWidth, TextHeight;
	int X, Y;

	if (FLogGrid)
	{
    painter.setPen(QPen(FLogGridColor, 1));

    for (int Freq=10; Freq<100; Freq+=10)
    {
      X = FAxisLeftMargin+BorderWidth+((float)HorizontalAxisLength*(log10(Freq)-1))/log10((FSamplerate)/(FNequistDivide*10));
      painter.drawLine(X, BorderWidth, X, height()-BorderWidth);
    }
    for (int Freq=100; Freq<1000; Freq+=100)
    {
      X = FAxisLeftMargin+BorderWidth+((float)HorizontalAxisLength*(log10(Freq)-1))/log10((FSamplerate)/(FNequistDivide*10));
      painter.drawLine(X, BorderWidth, X, height()-BorderWidth);
    }
    for (int Freq=1000; Freq<10000; Freq+=1000)
    {
      X = FAxisLeftMargin+BorderWidth+((float)HorizontalAxisLength*(log10(Freq)-1))/log10((FSamplerate)/(FNequistDivide*10));
      painter.drawLine(X, BorderWidth, X, height()-BorderWidth);
    }
    for (int Freq=10000; Freq<((FSamplerate)/(FNequistDivide)); Freq+=10000)
    {
      X = FAxisLeftMargin+BorderWidth+((float)HorizontalAxisLength*(log10(Freq)-1))/log10((FSamplerate)/(FNequistDivide*10));
      painter.drawLine(X, BorderWidth, X, height()-BorderWidth);
    }
  }

  TextHeight = fontMetrics().height();
  int TextMiddle = (fontMetrics().height()/2)-fontMetrics().descent();

  //Vertical 100Hz
  X = FAxisLeftMargin+BorderWidth+(DistanceTo10k/3);
  painter.setPen(QPen(FGridColor, 1));
  painter.drawLine(X, BorderWidth, X, height()-BorderWidth);
  if (FShowFrequencyText)
  {
    painter.setPen(QPen(FTextColor, 1));
    TextWidth = fontMetrics().width("100Hz");
    painter.drawText(FAxisLeftMargin+BorderWidth+(DistanceTo10k/3)-(TextWidth/2), height()-BorderWidth+TextHeight, "100Hz");
  }

  //Vertical 1kHz
  X = FAxisLeftMargin+BorderWidth+((DistanceTo10k*2)/3);
  painter.setPen(QPen(FGridColor, 1));
  painter.drawLine(X, BorderWidth, X, height()-BorderWidth);
  if (FShowFrequencyText)
  {
    painter.setPen(QPen(FTextColor, 1));
    TextWidth = fontMetrics().width("1kHz");
    painter.drawText(FAxisLeftMargin+BorderWidth+(DistanceTo10k*2/3)-(TextWidth/2), height()-BorderWidth+TextHeight, "1kHz");
  }

  //Vertical 10kHz
  X = FAxisLeftMargin+BorderWidth+DistanceTo10k;
  painter.setPen(QPen(FGridColor, 1));
  painter.drawLine(X, BorderWidth, X, height()-BorderWidth);
  if (FShowFrequencyText)
  {
    painter.setPen(QPen(FTextColor, 1));
    TextWidth = fontMetrics().width("10kHz");
    painter.drawText(FAxisLeftMargin+BorderWidth+DistanceTo10k-(TextWidth/2), height()-BorderWidth+TextHeight, "10kHz");
  }

  painter.setPen(QPen(FGridColor, 1));
  for (int cntLines=1; cntLines<=3; cntLines++)
  {
    Y = height()/2+(cntLines*VerticalAxisLength/(36/FVerticalGridAtdB));
    painter.drawLine(FAxisLeftMargin+BorderWidth, Y, width()-BorderWidth, Y);

    Y = height()/2-(cntLines*VerticalAxisLength/(36/FVerticalGridAtdB));
    painter.drawLine(FAxisLeftMargin+BorderWidth, Y, width()-BorderWidth, Y);
  }

  painter.setPen(QPen(FAxisColor, 1));
  painter.drawLine(FAxisLeftMargin+BorderWidth, height()/2, width()-BorderWidth, height()/2);
  if (FShowGainText)
  {
    painter.setPen(QPen(FTextColor, 1));
    TextWidth = fontMetrics().width("+18dB");
    painter.drawText(FAxisLeftMargin+BorderWidth-TextWidth-2, BorderWidth+TextMiddle, "+18dB");
    TextWidth = fontMetrics().width("+12dB");
    painter.drawText(FAxisLeftMargin+BorderWidth-TextWidth-2, (((height()/2-BorderWidth)*1)/3)+BorderWidth+TextMiddle, "+12dB");
    TextWidth = fontMetrics().width("+6dB");
    painter.drawText(FAxisLeftMargin+BorderWidth-TextWidth-2, (((height()/2-BorderWidth)*2)/3)+BorderWidth+TextMiddle, "+6dB");
    TextWidth = fontMetrics().width("0dB");
    painter.drawText(FAxisLeftMargin+BorderWidth-TextWidth-2, (height()/2)+TextMiddle, "0dB");
    TextWidth = fontMetrics().width("-6dB");
    painter.drawText(FAxisLeftMargin+BorderWidth-TextWidth-2, (height()-BorderWidth)-(((height()/2-BorderWidth)*2)/3)+TextMiddle, "-6dB");
    TextWidth = fontMetrics().width("-12dB");
    painter.drawText(FAxisLeftMargin+BorderWidth-TextWidth-2, (height()-BorderWidth)-(((height()/2-BorderWidth)*1)/3)+TextMiddle, "-12dB");
    TextWidth = fontMetrics().width("-18dB");
    painter.drawText(FAxisLeftMargin+BorderWidth-TextWidth-2, (height()-BorderWidth)+TextMiddle, "-18dB");
  }

  //vertical 10Hz
  painter.setPen(QPen(FAxisColor, 1));
  painter.drawLine(FAxisLeftMargin+BorderWidth, BorderWidth, FAxisLeftMargin+BorderWidth, height()-BorderWidth);
  if (FShowFrequencyText)
  {
    painter.setPen(QPen(FTextColor, 1));
    TextWidth = fontMetrics().width("10Hz");
    painter.drawText(FAxisLeftMargin+BorderWidth-(TextWidth/2), height()-BorderWidth+TextHeight, "10Hz");
  }

  //Draw active curve
  if (FEQOn)
  {
    painter.setPen(QPen(FActiveCurveColor, FActiveCurveWidth));
    painter.setBrush(FActiveCurveFillColor);
  }
  else
  {
    painter.setPen(QPen(FInactiveCurveColor, FInactiveCurveWidth));
    painter.setBrush(FInactiveCurveFillColor);
  }
  painter.drawPolygon(ActiveCurve, FNrOfPoints);


  if (FDrawAnchors)
  {
    //Band 6
    if (FTypeBand6 != OFF)
    {
      painter.setPen(QPen(FAnchorBand6Color));
      painter.setBrush(FAnchorBand6Color);

      X = FAxisLeftMargin+BorderWidth+((float)HorizontalAxisLength*(log10(FFrequencyBand6)-1))/log10((FSamplerate)/(FNequistDivide*10));
      Y = height()-BorderWidth-(((float)(FGainBand6+18)*VerticalAxisLength)/36);

      painter.drawEllipse(X-FAnchorSize, Y-FAnchorSize, FAnchorSize*2, FAnchorSize*2);
    }

    //Band 5
    if (FTypeBand5 != OFF)
    {
      painter.setPen(QPen(FAnchorBand5Color));
      painter.setBrush(FAnchorBand5Color);

      X = FAxisLeftMargin+BorderWidth+((float)HorizontalAxisLength*(log10(FFrequencyBand5)-1))/log10((FSamplerate)/(FNequistDivide*10));
      Y = height()-BorderWidth-(((float)(FGainBand5+18)*VerticalAxisLength)/36);

      painter.drawEllipse(X-FAnchorSize, Y-FAnchorSize, FAnchorSize*2, FAnchorSize*2);
    }

    //Band 4
    if (FTypeBand4 != OFF)
    {
      painter.setPen(QPen(FAnchorBand4Color));
      painter.setBrush(FAnchorBand4Color);

      X = FAxisLeftMargin+BorderWidth+((float)HorizontalAxisLength*(log10(FFrequencyBand4)-1))/log10((FSamplerate)/(FNequistDivide*10));
      Y = height()-BorderWidth-(((float)(FGainBand4+18)*VerticalAxisLength)/36);

      painter.drawEllipse(X-FAnchorSize, Y-FAnchorSize, FAnchorSize*2, FAnchorSize*2);
    }

    //Band 3
    if (FTypeBand3 != OFF)
    {
      painter.setPen(QPen(FAnchorBand3Color));
      painter.setBrush(FAnchorBand3Color);

      X = FAxisLeftMargin+BorderWidth+((float)HorizontalAxisLength*(log10(FFrequencyBand3)-1))/log10((FSamplerate)/(FNequistDivide*10));
      Y = height()-BorderWidth-(((float)(FGainBand3+18)*VerticalAxisLength)/36);

      painter.drawEllipse(X-FAnchorSize, Y-FAnchorSize, FAnchorSize*2, FAnchorSize*2);
    }

    //Band 2
    if (FTypeBand2 != OFF)
    {
      painter.setPen(QPen(FAnchorBand2Color));
      painter.setBrush(FAnchorBand2Color);

      X = FAxisLeftMargin+BorderWidth+((float)HorizontalAxisLength*(log10(FFrequencyBand2)-1))/log10((FSamplerate)/(FNequistDivide*10));
      Y = height()-BorderWidth-(((float)(FGainBand2+18)*VerticalAxisLength)/36);

      painter.drawEllipse(X-FAnchorSize, Y-FAnchorSize, FAnchorSize*2, FAnchorSize*2);
    }
    
    //Band 1
    if (FTypeBand1 != OFF)
    {
      painter.setPen(QPen(FAnchorBand1Color));
      painter.setBrush(FAnchorBand1Color);

      X = FAxisLeftMargin+BorderWidth+((float)HorizontalAxisLength*(log10(FFrequencyBand1)-1))/log10((FSamplerate)/(FNequistDivide*10));
      Y = height()-BorderWidth-(((float)(FGainBand1+18)*VerticalAxisLength)/36);

      painter.drawEllipse(X-FAnchorSize, Y-FAnchorSize, FAnchorSize*2, FAnchorSize*2);
    }
  }
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

QColor DNREQPanel::getTextColor()
{
  return FTextColor;
}

void DNREQPanel::setTextColor(QColor NewTextColor)
{
  if (FTextColor != NewTextColor)
  {
    FTextColor = NewTextColor;
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

int DNREQPanel::getAnchorSize()
{
  return FAnchorSize;
}

void DNREQPanel::setAnchorSize(int NewAnchorSize)
{
  if (FAnchorSize != NewAnchorSize)
  {
    FAnchorSize = NewAnchorSize;
    update();
  }
}

double DNREQPanel::getGainBand1()
{
  return FGainBand1;
}

void DNREQPanel::setGainBand1(double NewGainBand1)
{
  float Coefficients[6];

  if (FGainBand1 != NewGainBand1)
  {
    FGainBand1 = NewGainBand1;
    GainFactorBand1 = CalculateEQ(Coefficients, FGainBand1, FFrequencyBand1, FBandwidthBand1,FSlopeBand1, FTypeBand1, FOnBand1);
    CalculateZeroPosition(ZerosXBand1, ZerosYBand1, Coefficients);
    CalculatePolePosition(PolesXBand1, PolesYBand1, Coefficients);
    CalculateCurve();
    update();
  }
}

double DNREQPanel::getGainBand2()
{
  return FGainBand2;
}

void DNREQPanel::setGainBand2(double NewGainBand2)
{
  float Coefficients[6];

  if (FGainBand2 != NewGainBand2)
  {
    FGainBand2 = NewGainBand2;
    GainFactorBand2 = CalculateEQ(Coefficients, FGainBand2, FFrequencyBand2, FBandwidthBand2,FSlopeBand2, FTypeBand2, FOnBand2);
    CalculateZeroPosition(ZerosXBand2, ZerosYBand2, Coefficients);
    CalculatePolePosition(PolesXBand2, PolesYBand2, Coefficients);
    CalculateCurve();
    update();
  }
}

double DNREQPanel::getGainBand3()
{
  return FGainBand3;
}

void DNREQPanel::setGainBand3(double NewGainBand3)
{
  float Coefficients[6];

  if (FGainBand3 != NewGainBand3)
  {
    FGainBand3 = NewGainBand3;
    GainFactorBand3 = CalculateEQ(Coefficients, FGainBand3, FFrequencyBand3, FBandwidthBand3,FSlopeBand3, FTypeBand3, FOnBand3);
    CalculateZeroPosition(ZerosXBand3, ZerosYBand3, Coefficients);
    CalculatePolePosition(PolesXBand3, PolesYBand3, Coefficients);
    CalculateCurve();
    update();
  }
}

double DNREQPanel::getGainBand4()
{
  return FGainBand4;
}

void DNREQPanel::setGainBand4(double NewGainBand4)
{
  float Coefficients[6];

  if (FGainBand4 != NewGainBand4)
  {
    FGainBand4 = NewGainBand4;
    GainFactorBand4 = CalculateEQ(Coefficients, FGainBand4, FFrequencyBand4, FBandwidthBand4,FSlopeBand4, FTypeBand4, FOnBand4);
    CalculateZeroPosition(ZerosXBand4, ZerosYBand4, Coefficients);
    CalculatePolePosition(PolesXBand4, PolesYBand4, Coefficients);
    CalculateCurve();
    update();
  }
}

double DNREQPanel::getGainBand5()
{
  return FGainBand5;
}

void DNREQPanel::setGainBand5(double NewGainBand5)
{
  float Coefficients[6];

  if (FGainBand5 != NewGainBand5)
  {
    FGainBand5 = NewGainBand5;
    GainFactorBand5 = CalculateEQ(Coefficients, FGainBand5, FFrequencyBand5, FBandwidthBand5,FSlopeBand5, FTypeBand5, FOnBand5);
    CalculateZeroPosition(ZerosXBand5, ZerosYBand5, Coefficients);
    CalculatePolePosition(PolesXBand5, PolesYBand5, Coefficients);
    CalculateCurve();
    update();
  }
}

double DNREQPanel::getGainBand6()
{
  return FGainBand6;
}

void DNREQPanel::setGainBand6(double NewGainBand6)
{
  float Coefficients[6];

  if (FGainBand6 != NewGainBand6)
  {
    FGainBand6 = NewGainBand6;
    GainFactorBand6 = CalculateEQ(Coefficients, FGainBand6, FFrequencyBand6, FBandwidthBand6,FSlopeBand6, FTypeBand6, FOnBand6);
    CalculateZeroPosition(ZerosXBand6, ZerosYBand6, Coefficients);
    CalculatePolePosition(PolesXBand6, PolesYBand6, Coefficients);
    CalculateCurve();
    update();
  }
}

int DNREQPanel::getFrequencyBand1()
{
  return FFrequencyBand1;
}

void DNREQPanel::setFrequencyBand1(int NewFrequencyBand1)
{
  float Coefficients[6];

  if (FFrequencyBand1 != NewFrequencyBand1)
  {
    FFrequencyBand1 = NewFrequencyBand1;
    GainFactorBand1 = CalculateEQ(Coefficients, FGainBand1, FFrequencyBand1, FBandwidthBand1,FSlopeBand1, FTypeBand1, FOnBand1);
    CalculateZeroPosition(ZerosXBand1, ZerosYBand1, Coefficients);
    CalculatePolePosition(PolesXBand1, PolesYBand1, Coefficients);
    CalculateCurve();
    update();
  }
}

int DNREQPanel::getFrequencyBand2()
{
  return FFrequencyBand2;
}

void DNREQPanel::setFrequencyBand2(int NewFrequencyBand2)
{
  float Coefficients[6];

  if (FFrequencyBand2 != NewFrequencyBand2)
  {
    FFrequencyBand2 = NewFrequencyBand2;
    GainFactorBand2 = CalculateEQ(Coefficients, FGainBand2, FFrequencyBand2, FBandwidthBand2,FSlopeBand2, FTypeBand2, FOnBand2);
    CalculateZeroPosition(ZerosXBand2, ZerosYBand2, Coefficients);
    CalculatePolePosition(PolesXBand2, PolesYBand2, Coefficients);
    CalculateCurve();
    update();
  }
}

int DNREQPanel::getFrequencyBand3()
{
  return FFrequencyBand3;
}

void DNREQPanel::setFrequencyBand3(int NewFrequencyBand3)
{
  float Coefficients[6];

  if (FFrequencyBand3 != NewFrequencyBand3)
  {
    FFrequencyBand3 = NewFrequencyBand3;
    GainFactorBand3 = CalculateEQ(Coefficients, FGainBand3, FFrequencyBand3, FBandwidthBand3,FSlopeBand3, FTypeBand3, FOnBand3);
    CalculateZeroPosition(ZerosXBand3, ZerosYBand3, Coefficients);
    CalculatePolePosition(PolesXBand3, PolesYBand3, Coefficients);
    CalculateCurve();
    update();
  }
}

int DNREQPanel::getFrequencyBand4()
{
  return FFrequencyBand4;
}

void DNREQPanel::setFrequencyBand4(int NewFrequencyBand4)
{
  float Coefficients[6];

  if (FFrequencyBand4 != NewFrequencyBand4)
  {
    FFrequencyBand4 = NewFrequencyBand4;
    GainFactorBand4 = CalculateEQ(Coefficients, FGainBand4, FFrequencyBand4, FBandwidthBand4,FSlopeBand4, FTypeBand4, FOnBand4);
    CalculateZeroPosition(ZerosXBand4, ZerosYBand4, Coefficients);
    CalculatePolePosition(PolesXBand4, PolesYBand4, Coefficients);
    CalculateCurve();
    update();
  }
}

int DNREQPanel::getFrequencyBand5()
{
  return FFrequencyBand5;
}

void DNREQPanel::setFrequencyBand5(int NewFrequencyBand5)
{
  float Coefficients[6];

  if (FFrequencyBand5 != NewFrequencyBand5)
  {
    FFrequencyBand5 = NewFrequencyBand5;
    GainFactorBand5 = CalculateEQ(Coefficients, FGainBand5, FFrequencyBand5, FBandwidthBand5,FSlopeBand5, FTypeBand5, FOnBand5);
    CalculateZeroPosition(ZerosXBand5, ZerosYBand5, Coefficients);
    CalculatePolePosition(PolesXBand5, PolesYBand5, Coefficients);
    CalculateCurve();
    update();
  }
}

int DNREQPanel::getFrequencyBand6()
{
  return FFrequencyBand6;
}

void DNREQPanel::setFrequencyBand6(int NewFrequencyBand6)
{
  float Coefficients[6];

  if (FFrequencyBand6 != NewFrequencyBand6)
  {
    FFrequencyBand6 = NewFrequencyBand6;
    GainFactorBand6 = CalculateEQ(Coefficients, FGainBand6, FFrequencyBand6, FBandwidthBand6,FSlopeBand6, FTypeBand6, FOnBand6);
    CalculateZeroPosition(ZerosXBand6, ZerosYBand6, Coefficients);
    CalculatePolePosition(PolesXBand6, PolesYBand6, Coefficients);
    CalculateCurve();
    update();
  }
}


double DNREQPanel::getBandwidthBand1()
{
  return FBandwidthBand1;
}

void DNREQPanel::setBandwidthBand1(double NewBandwidthBand1)
{
  float Coefficients[6];

  if (FBandwidthBand1 != NewBandwidthBand1)
  {
    FBandwidthBand1 = NewBandwidthBand1;
    GainFactorBand1 = CalculateEQ(Coefficients, FGainBand1, FFrequencyBand1, FBandwidthBand1,FSlopeBand1, FTypeBand1, FOnBand1);
    CalculateZeroPosition(ZerosXBand1, ZerosYBand1, Coefficients);
    CalculatePolePosition(PolesXBand1, PolesYBand1, Coefficients);
    CalculateCurve();
    update();
  }
}

double DNREQPanel::getBandwidthBand2()
{
  return FBandwidthBand2;
}

void DNREQPanel::setBandwidthBand2(double NewBandwidthBand2)
{
  float Coefficients[6];

  if (FBandwidthBand2 != NewBandwidthBand2)
  {
    FBandwidthBand2 = NewBandwidthBand2;
    GainFactorBand2 = CalculateEQ(Coefficients, FGainBand2, FFrequencyBand2, FBandwidthBand2,FSlopeBand2, FTypeBand2, FOnBand2);
    CalculateZeroPosition(ZerosXBand2, ZerosYBand2, Coefficients);
    CalculatePolePosition(PolesXBand2, PolesYBand2, Coefficients);
    CalculateCurve();
    update();
  }
}

double DNREQPanel::getBandwidthBand3()
{
  return FBandwidthBand3;
}

void DNREQPanel::setBandwidthBand3(double NewBandwidthBand3)
{
  float Coefficients[6];

  if (FBandwidthBand3 != NewBandwidthBand3)
  {
    FBandwidthBand3 = NewBandwidthBand3;
    GainFactorBand3 = CalculateEQ(Coefficients, FGainBand3, FFrequencyBand3, FBandwidthBand3,FSlopeBand3, FTypeBand3, FOnBand3);
    CalculateZeroPosition(ZerosXBand3, ZerosYBand3, Coefficients);
    CalculatePolePosition(PolesXBand3, PolesYBand3, Coefficients);
    CalculateCurve();
    update();
  }
}

double DNREQPanel::getBandwidthBand4()
{
  return FBandwidthBand4;
}

void DNREQPanel::setBandwidthBand4(double NewBandwidthBand4)
{
  float Coefficients[6];

  if (FBandwidthBand4 != NewBandwidthBand4)
  {
    FBandwidthBand4 = NewBandwidthBand4;
    GainFactorBand4 = CalculateEQ(Coefficients, FGainBand4, FFrequencyBand4, FBandwidthBand4,FSlopeBand4, FTypeBand4, FOnBand4);
    CalculateZeroPosition(ZerosXBand4, ZerosYBand4, Coefficients);
    CalculatePolePosition(PolesXBand4, PolesYBand4, Coefficients);
    CalculateCurve();
    update();
  }
}

double DNREQPanel::getBandwidthBand5()
{
  return FBandwidthBand5;
}

void DNREQPanel::setBandwidthBand5(double NewBandwidthBand5)
{
  float Coefficients[6];

  if (FBandwidthBand5 != NewBandwidthBand5)
  {
    FBandwidthBand5 = NewBandwidthBand5;
    GainFactorBand5 = CalculateEQ(Coefficients, FGainBand5, FFrequencyBand5, FBandwidthBand5,FSlopeBand5, FTypeBand5, FOnBand5);
    CalculateZeroPosition(ZerosXBand5, ZerosYBand5, Coefficients);
    CalculatePolePosition(PolesXBand5, PolesYBand5, Coefficients);
    CalculateCurve();
    update();
  }
}

double DNREQPanel::getBandwidthBand6()
{
  return FBandwidthBand6;
}

void DNREQPanel::setBandwidthBand6(double NewBandwidthBand6)
{
  float Coefficients[6];

  if (FBandwidthBand6 != NewBandwidthBand6)
  {
    FBandwidthBand6 = NewBandwidthBand6;
    GainFactorBand6 = CalculateEQ(Coefficients, FGainBand6, FFrequencyBand6, FBandwidthBand6,FSlopeBand6, FTypeBand6, FOnBand6);
    CalculateZeroPosition(ZerosXBand6, ZerosYBand6, Coefficients);
    CalculatePolePosition(PolesXBand6, PolesYBand6, Coefficients);
    CalculateCurve();
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
    if (NewNrOfPoints>1024)
    {
      NewNrOfPoints = 1024;
    }
    else if (NewNrOfPoints<0)
    {
      NewNrOfPoints = 0;
    }
    FNrOfPoints = NewNrOfPoints;
    CalculateCurve();
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
    CalculateCurve();
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
    CalculateCurve();
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

QColor DNREQPanel::getActiveCurveFillColor()
{
  return FActiveCurveFillColor;
}

void DNREQPanel::setActiveCurveFillColor(QColor NewActiveCurveFillColor)
{
  if (FActiveCurveFillColor != NewActiveCurveFillColor)
  {
    FActiveCurveFillColor = NewActiveCurveFillColor;
    update();
  }
}

QColor DNREQPanel::getInactiveCurveColor()
{
  return FInactiveCurveColor;
}

void DNREQPanel::setInactiveCurveColor(QColor NewInactiveCurveColor)
{
  if (FInactiveCurveColor != NewInactiveCurveColor)
  {
    FInactiveCurveColor = NewInactiveCurveColor;
    update();
  }
}

int DNREQPanel::getInactiveCurveWidth()
{
  return FInactiveCurveWidth;
}

void DNREQPanel::setInactiveCurveWidth(int NewInactiveCurveWidth)
{
  if (FInactiveCurveWidth != NewInactiveCurveWidth)
  {
    FInactiveCurveWidth = NewInactiveCurveWidth;
    update();
  }
}

QColor DNREQPanel::getInactiveCurveFillColor()
{
  return FInactiveCurveFillColor;
}

void DNREQPanel::setInactiveCurveFillColor(QColor NewInactiveCurveFillColor)
{
  if (FInactiveCurveFillColor != NewInactiveCurveFillColor)
  {
    FInactiveCurveFillColor = NewInactiveCurveFillColor;
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
  float Coefficients[6];

  if (FTypeBand1 != NewTypeBand1)
  {
    FTypeBand1 = NewTypeBand1;
    GainFactorBand1 = CalculateEQ(Coefficients, FGainBand1, FFrequencyBand1, FBandwidthBand1,FSlopeBand1, FTypeBand1, FOnBand1);
    CalculateZeroPosition(ZerosXBand1, ZerosYBand1, Coefficients);
    CalculatePolePosition(PolesXBand1, PolesYBand1, Coefficients);
    CalculateCurve();
    update();
  }
}

int DNREQPanel::getTypeBand2()
{
  return FTypeBand2;
}

void DNREQPanel::setTypeBand2(int NewTypeBand2)
{
  float Coefficients[6];

  if (FTypeBand2 != NewTypeBand2)
  {
    FTypeBand2 = NewTypeBand2;
    GainFactorBand2 = CalculateEQ(Coefficients, FGainBand2, FFrequencyBand2, FBandwidthBand2,FSlopeBand2, FTypeBand2, FOnBand2);
    CalculateZeroPosition(ZerosXBand2, ZerosYBand2, Coefficients);
    CalculatePolePosition(PolesXBand2, PolesYBand2, Coefficients);
    CalculateCurve();
    update();
  }
}

int DNREQPanel::getTypeBand3()
{
  return FTypeBand3;
}

void DNREQPanel::setTypeBand3(int NewTypeBand3)
{
  float Coefficients[6];

  if (FTypeBand3 != NewTypeBand3)
  {
    FTypeBand3 = NewTypeBand3;
    GainFactorBand3 = CalculateEQ(Coefficients, FGainBand3, FFrequencyBand3, FBandwidthBand3,FSlopeBand3, FTypeBand3, FOnBand3);
    CalculateZeroPosition(ZerosXBand3, ZerosYBand3, Coefficients);
    CalculatePolePosition(PolesXBand3, PolesYBand3, Coefficients);
    CalculateCurve();
    update();
  }
}

int DNREQPanel::getTypeBand4()
{
  return FTypeBand4;
}

void DNREQPanel::setTypeBand4(int NewTypeBand4)
{
  float Coefficients[6];

  if (FTypeBand4 != NewTypeBand4)
  {
    FTypeBand4 = NewTypeBand4;
    GainFactorBand4 = CalculateEQ(Coefficients, FGainBand4, FFrequencyBand4, FBandwidthBand4,FSlopeBand4, FTypeBand4, FOnBand4);
    CalculateZeroPosition(ZerosXBand4, ZerosYBand4, Coefficients);
    CalculatePolePosition(PolesXBand4, PolesYBand4, Coefficients);
    CalculateCurve();
    update();
  }
}

int DNREQPanel::getTypeBand5()
{
  return FTypeBand5;
}

void DNREQPanel::setTypeBand5(int NewTypeBand5)
{
  float Coefficients[6];

  if (FTypeBand5 != NewTypeBand5)
  {
    FTypeBand5 = NewTypeBand5;
    GainFactorBand5 = CalculateEQ(Coefficients, FGainBand5, FFrequencyBand5, FBandwidthBand5,FSlopeBand5, FTypeBand5, FOnBand5);
    CalculateZeroPosition(ZerosXBand5, ZerosYBand5, Coefficients);
    CalculatePolePosition(PolesXBand5, PolesYBand5, Coefficients);
    CalculateCurve();
    update();
  }
}

int DNREQPanel::getTypeBand6()
{
  return FTypeBand6;
}

void DNREQPanel::setTypeBand6(int NewTypeBand6)
{
  float Coefficients[6];

  if (FTypeBand6 != NewTypeBand6)
  {
    FTypeBand6 = NewTypeBand6;
    GainFactorBand6 = CalculateEQ(Coefficients, FGainBand6, FFrequencyBand6, FBandwidthBand6,FSlopeBand6, FTypeBand6, FOnBand6);
    CalculateZeroPosition(ZerosXBand6, ZerosYBand6, Coefficients);
    CalculatePolePosition(PolesXBand6, PolesYBand6, Coefficients);
    CalculateCurve();
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
  float Coefficients[6];

  if (FOnBand1 != NewOnBand1)
  {
    FOnBand1 = NewOnBand1;
    GainFactorBand1 = CalculateEQ(Coefficients, FGainBand1, FFrequencyBand1, FBandwidthBand1,FSlopeBand1, FTypeBand1, FOnBand1);
    CalculateZeroPosition(ZerosXBand1, ZerosYBand1, Coefficients);
    CalculatePolePosition(PolesXBand1, PolesYBand1, Coefficients);
    CalculateCurve();
    update();
  }
}

bool DNREQPanel::getOnBand2()
{
  return FOnBand2;
}

void DNREQPanel::setOnBand2(bool NewOnBand2)
{
  float Coefficients[6];

  if (FOnBand2 != NewOnBand2)
  {
    FOnBand2 = NewOnBand2;
    GainFactorBand2 = CalculateEQ(Coefficients, FGainBand2, FFrequencyBand2, FBandwidthBand2,FSlopeBand2, FTypeBand2, FOnBand2);
    CalculateZeroPosition(ZerosXBand2, ZerosYBand2, Coefficients);
    CalculatePolePosition(PolesXBand2, PolesYBand2, Coefficients);
    CalculateCurve();
    update();
  }
}

bool DNREQPanel::getOnBand3()
{
  return FOnBand3;
}

void DNREQPanel::setOnBand3(bool NewOnBand3)
{
  float Coefficients[6];

  if (FOnBand3 != NewOnBand3)
  {
    FOnBand3 = NewOnBand3;
    GainFactorBand3 = CalculateEQ(Coefficients, FGainBand3, FFrequencyBand3, FBandwidthBand3,FSlopeBand3, FTypeBand3, FOnBand3);
    CalculateZeroPosition(ZerosXBand3, ZerosYBand3, Coefficients);
    CalculatePolePosition(PolesXBand3, PolesYBand3, Coefficients);
    CalculateCurve();
    update();
  }
}

bool DNREQPanel::getOnBand4()
{
  return FOnBand4;
}

void DNREQPanel::setOnBand4(bool NewOnBand4)
{
  float Coefficients[6];

  if (FOnBand4 != NewOnBand4)
  {
    FOnBand4 = NewOnBand4;
    GainFactorBand4 = CalculateEQ(Coefficients, FGainBand4, FFrequencyBand4, FBandwidthBand4,FSlopeBand4, FTypeBand4, FOnBand4);
    CalculateZeroPosition(ZerosXBand4, ZerosYBand4, Coefficients);
    CalculatePolePosition(PolesXBand4, PolesYBand4, Coefficients);
    CalculateCurve();
    update();
  }
}

bool DNREQPanel::getOnBand5()
{
  return FOnBand5;
}

void DNREQPanel::setOnBand5(bool NewOnBand5)
{
  float Coefficients[6];

  if (FOnBand5 != NewOnBand5)
  {
    FOnBand5 = NewOnBand5;
    GainFactorBand5 = CalculateEQ(Coefficients, FGainBand5, FFrequencyBand5, FBandwidthBand5,FSlopeBand5, FTypeBand5, FOnBand5);
    CalculateZeroPosition(ZerosXBand5, ZerosYBand5, Coefficients);
    CalculatePolePosition(PolesXBand5, PolesYBand5, Coefficients);
    CalculateCurve();
    update();
  }
}

bool DNREQPanel::getOnBand6()
{
  return FOnBand6;
}

void DNREQPanel::setOnBand6(bool NewOnBand6)
{
  float Coefficients[6];

  if (FOnBand6 != NewOnBand6)
  {
    FOnBand6 = NewOnBand6;
    GainFactorBand6 = CalculateEQ(Coefficients, FGainBand6, FFrequencyBand6, FBandwidthBand6,FSlopeBand6, FTypeBand6, FOnBand6);
    CalculateZeroPosition(ZerosXBand6, ZerosYBand6, Coefficients);
    CalculatePolePosition(PolesXBand6, PolesYBand6, Coefficients);
    CalculateCurve();
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

double DNREQPanel::getSlopeBand1()
{
  return FSlopeBand1;
}

void DNREQPanel::setSlopeBand1(double NewSlopeBand1)
{
  float Coefficients[6];

  if (FSlopeBand1 != NewSlopeBand1)
  {
    FSlopeBand1 = NewSlopeBand1;
    GainFactorBand1 = CalculateEQ(Coefficients, FGainBand1, FFrequencyBand1, FBandwidthBand1,FSlopeBand1, FTypeBand1, FOnBand1);
    CalculateZeroPosition(ZerosXBand1, ZerosYBand1, Coefficients);
    CalculatePolePosition(PolesXBand1, PolesYBand1, Coefficients);
    CalculateCurve();
    update();
  }
}

double DNREQPanel::getSlopeBand2()
{
  return FSlopeBand2;
}

void DNREQPanel::setSlopeBand2(double NewSlopeBand2)
{
  float Coefficients[6];

  if (FSlopeBand2 != NewSlopeBand2)
  {
    FSlopeBand2 = NewSlopeBand2;
    GainFactorBand2 = CalculateEQ(Coefficients, FGainBand2, FFrequencyBand2, FBandwidthBand2,FSlopeBand2, FTypeBand2, FOnBand2);
    CalculateZeroPosition(ZerosXBand2, ZerosYBand2, Coefficients);
    CalculatePolePosition(PolesXBand2, PolesYBand2, Coefficients);
    CalculateCurve();
    update();
  }
}

double DNREQPanel::getSlopeBand3()
{
  return FSlopeBand3;
}

void DNREQPanel::setSlopeBand3(double NewSlopeBand3)
{
  float Coefficients[6];

  if (FSlopeBand3 != NewSlopeBand3)
  {
    FSlopeBand3 = NewSlopeBand3;
    GainFactorBand3 = CalculateEQ(Coefficients, FGainBand3, FFrequencyBand3, FBandwidthBand3,FSlopeBand3, FTypeBand3, FOnBand3);
    CalculateZeroPosition(ZerosXBand3, ZerosYBand3, Coefficients);
    CalculatePolePosition(PolesXBand3, PolesYBand3, Coefficients);
    CalculateCurve();
    update();
  }
}

double DNREQPanel::getSlopeBand4()
{
  return FSlopeBand4;
}

void DNREQPanel::setSlopeBand4(double NewSlopeBand4)
{
  float Coefficients[6];

  if (FSlopeBand4 != NewSlopeBand4)
  {
    FSlopeBand4 = NewSlopeBand4;
    GainFactorBand4 = CalculateEQ(Coefficients, FGainBand4, FFrequencyBand4, FBandwidthBand4,FSlopeBand4, FTypeBand4, FOnBand4);
    CalculateZeroPosition(ZerosXBand4, ZerosYBand4, Coefficients);
    CalculatePolePosition(PolesXBand4, PolesYBand4, Coefficients);
    CalculateCurve();
    update();
  }
}

double DNREQPanel::getSlopeBand5()
{
  return FSlopeBand5;
}

void DNREQPanel::setSlopeBand5(double NewSlopeBand5)
{
  float Coefficients[6];

  if (FSlopeBand5 != NewSlopeBand5)
  {
    FSlopeBand5 = NewSlopeBand5;
    GainFactorBand5 = CalculateEQ(Coefficients, FGainBand5, FFrequencyBand5, FBandwidthBand5,FSlopeBand5, FTypeBand5, FOnBand5);
    CalculateZeroPosition(ZerosXBand5, ZerosYBand5, Coefficients);
    CalculatePolePosition(PolesXBand5, PolesYBand5, Coefficients);
    CalculateCurve();
    update();
  }
}

double DNREQPanel::getSlopeBand6()
{
  return FSlopeBand6;
}

void DNREQPanel::setSlopeBand6(double NewSlopeBand6)
{
  float Coefficients[6];

  if (FSlopeBand6 != NewSlopeBand6)
  {
    FSlopeBand6 = NewSlopeBand6;
    GainFactorBand6 = CalculateEQ(Coefficients, FGainBand6, FFrequencyBand6, FBandwidthBand6,FSlopeBand6, FTypeBand6, FOnBand6);
    CalculateZeroPosition(ZerosXBand6, ZerosYBand6, Coefficients);
    CalculatePolePosition(PolesXBand6, PolesYBand6, Coefficients);
    CalculateCurve();
    update();
  }
}

//Supporting routines for calculations
void DNREQPanel::CalculateCurve()
{
  int cnt;
  double Freq, Alpha;
  double X,Y, DeltaX, DeltaY;
  double Length, dBGain;

  int BorderWidth = FAxisBorderWidth;

  int HorizontalAxisLength = width()-(2*BorderWidth)-FAxisLeftMargin;
  int VerticalAxisLength = height()-(2*BorderWidth);

  for (cnt=0; cnt<FNrOfPoints-2; cnt++)
  {
    Freq = pow(10,(float) (cnt*log10((FSamplerate)/(FNequistDivide*10)))/(FNrOfPoints-2))*10;
    Alpha = (double)(M_PI*2*Freq)/(FSamplerate);

    X = cos(Alpha);
    Y = sin(Alpha);

    Length = 1;

    // Band 1
    DeltaX = X-ZerosXBand1[0];
    DeltaY = Y-ZerosYBand1[0];
    Length *= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

    DeltaX = X-ZerosXBand1[1];
    DeltaY = Y-ZerosYBand1[1];
    Length *= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

    DeltaX = X-PolesXBand1[0];
    DeltaY = Y-PolesYBand1[0];
    Length /= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

	  DeltaX = X-PolesXBand1[1];
	  DeltaY = Y-PolesYBand1[1];
	  Length /= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

	  Length *= GainFactorBand1;

    // Band 2
    DeltaX = X-ZerosXBand2[0];
	  DeltaY = Y-ZerosYBand2[0];
	  Length *= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

    DeltaX = X-ZerosXBand2[1];
	  DeltaY = Y-ZerosYBand2[1];
	  Length *= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

	  DeltaX = X-PolesXBand2[0];
	  DeltaY = Y-PolesYBand2[0];
	  Length /= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

	  DeltaX = X-PolesXBand2[1];
	  DeltaY = Y-PolesYBand2[1];
	  Length /= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

	  Length *= GainFactorBand2;

    // Band 3
    DeltaX = X-ZerosXBand3[0];
	  DeltaY = Y-ZerosYBand3[0];
	  Length *= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

	  DeltaX = X-ZerosXBand3[1];
	  DeltaY = Y-ZerosYBand3[1];
	  Length *= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

	  DeltaX = X-PolesXBand3[0];
	  DeltaY = Y-PolesYBand3[0];
	  Length /= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

  	DeltaX = X-PolesXBand3[1];
  	DeltaY = Y-PolesYBand3[1];
  	Length /= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

  	Length *= GainFactorBand3;

    // Band 4
    DeltaX = X-ZerosXBand4[0];
	  DeltaY = Y-ZerosYBand4[0];
	  Length *= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

	  DeltaX = X-ZerosXBand4[1];
	  DeltaY = Y-ZerosYBand4[1];
	  Length *= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

	  DeltaX = X-PolesXBand4[0];
	  DeltaY = Y-PolesYBand4[0];
	  Length /= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

	  DeltaX = X-PolesXBand4[1];
	  DeltaY = Y-PolesYBand4[1];
	  Length /= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

	  Length *= GainFactorBand4;

    // Band 5
    DeltaX = X-ZerosXBand5[0];
	  DeltaY = Y-ZerosYBand5[0];
	  Length *= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

	  DeltaX = X-ZerosXBand5[1];
	  DeltaY = Y-ZerosYBand5[1];
	  Length *= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

	  DeltaX = X-PolesXBand5[0];
	  DeltaY = Y-PolesYBand5[0];
	  Length /= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

	  DeltaX = X-PolesXBand5[1];
	  DeltaY = Y-PolesYBand5[1];
	  Length /= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

	  Length *= GainFactorBand5;

    // Band 6
    DeltaX = X-ZerosXBand6[0];
	  DeltaY = Y-ZerosYBand6[0];
	  Length *= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

	  DeltaX = X-ZerosXBand6[1];
	  DeltaY = Y-ZerosYBand6[1];
	  Length *= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

	  DeltaX = X-PolesXBand6[0];
	  DeltaY = Y-PolesYBand6[0];
	  Length /= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

	  DeltaX = X-PolesXBand6[1];
	  DeltaY = Y-PolesYBand6[1];
	  Length /= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

	  Length *= GainFactorBand6;

    if (Length==0)
      dBGain = -90;
    else
		  dBGain = log10(Length)*-20;

    int X1 = FAxisLeftMargin+BorderWidth+((float)(cnt*HorizontalAxisLength)/FNrOfPoints);
    int Y1 = height()-BorderWidth-(((float)(18-dBGain)*VerticalAxisLength)/36);
	  ActiveCurve[cnt] = QPoint(X1, Y1);
  }
  ActiveCurve[0] = QPoint(FAxisLeftMargin+BorderWidth, height()-BorderWidth-(VerticalAxisLength/2));
  ActiveCurve[FNrOfPoints-2] = QPoint(FAxisLeftMargin+BorderWidth+HorizontalAxisLength, height()-BorderWidth-(VerticalAxisLength/2));
}

void DNREQPanel::CalculateCurveOn()
{
  int cnt;
  double Freq, Alpha;
  double X,Y, DeltaX, DeltaY;
  double Length, dBGain;

  int BorderWidth = FAxisBorderWidth;

  int HorizontalAxisLength = width()-(2*BorderWidth)-FAxisLeftMargin;
  int VerticalAxisLength = height()-(2*BorderWidth);

  for (cnt=0; cnt<FNrOfPoints; cnt++)
  {
    Freq = pow(10,(float) (cnt*log10((FSamplerate)/(FNequistDivide*10)))/FNrOfPoints)*10;
    Alpha = (double)(M_PI*2*Freq)/(FSamplerate);

    X = cos(Alpha);
    Y = sin(Alpha);

    Length = 1;

    // Band 1
    DeltaX = X-ZerosXBand1On[0];
    DeltaY = Y-ZerosYBand1On[0];
    Length *= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

    DeltaX = X-ZerosXBand1On[1];
    DeltaY = Y-ZerosYBand1On[1];
    Length *= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

    DeltaX = X-PolesXBand1On[0];
    DeltaY = Y-PolesYBand1On[0];
    Length /= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

	  DeltaX = X-PolesXBand1On[1];
	  DeltaY = Y-PolesYBand1On[1];
	  Length /= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

	  Length *= GainFactorBand1On;

    // Band 2
    DeltaX = X-ZerosXBand2On[0];
	  DeltaY = Y-ZerosYBand2On[0];
	  Length *= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

	  DeltaX = X-ZerosXBand2On[1];
	  DeltaY = Y-ZerosYBand2On[1];
	  Length *= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

  	DeltaX = X-PolesXBand2On[0];
  	DeltaY = Y-PolesYBand2On[0];
  	Length /= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

  	DeltaX = X-PolesXBand2On[1];
  	DeltaY = Y-PolesYBand2On[1];
    Length /= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

  	Length *= GainFactorBand2On;

    // Band 3
    DeltaX = X-ZerosXBand3On[0];
	  DeltaY = Y-ZerosYBand3On[0];
	  Length *= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

	  DeltaX = X-ZerosXBand3On[1];
	  DeltaY = Y-ZerosYBand3On[1];
	  Length *= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

  	DeltaX = X-PolesXBand3On[0];
  	DeltaY = Y-PolesYBand3On[0];
  	Length /= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

  	DeltaX = X-PolesXBand3On[1];
  	DeltaY = Y-PolesYBand3On[1];
  	Length /= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

   	Length *= GainFactorBand3On;

    // Band 4
    DeltaX = X-ZerosXBand4On[0];
	  DeltaY = Y-ZerosYBand4On[0];
	  Length *= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

	  DeltaX = X-ZerosXBand4On[1];
	  DeltaY = Y-ZerosYBand4On[1];
	  Length *= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

  	DeltaX = X-PolesXBand4On[0];
  	DeltaY = Y-PolesYBand4On[0];
  	Length /= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

  	DeltaX = X-PolesXBand4On[1];
  	DeltaY = Y-PolesYBand4On[1];
  	Length /= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

  	Length *= GainFactorBand4On;

    // Band 5
    DeltaX = X-ZerosXBand5On[0];
	  DeltaY = Y-ZerosYBand5On[0];
	  Length *= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

	  DeltaX = X-ZerosXBand5On[1];
	  DeltaY = Y-ZerosYBand5On[1];
	  Length *= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

  	DeltaX = X-PolesXBand5On[0];
  	DeltaY = Y-PolesYBand5On[0];
  	Length /= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

  	DeltaX = X-PolesXBand5On[1];
  	DeltaY = Y-PolesYBand5On[1];
  	Length /= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

  	Length *= GainFactorBand5On;

    // Band 6
    DeltaX = X-ZerosXBand6On[0];
	  DeltaY = Y-ZerosYBand6On[0];
	  Length *= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

	  DeltaX = X-ZerosXBand6On[1];
	  DeltaY = Y-ZerosYBand6On[1];
	  Length *= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

  	DeltaX = X-PolesXBand6On[0];
  	DeltaY = Y-PolesYBand6On[0];
  	Length /= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

  	DeltaX = X-PolesXBand6On[1];
  	DeltaY = Y-PolesYBand6On[1];
  	Length /= sqrt(DeltaX*DeltaX+DeltaY*DeltaY);

  	Length *= GainFactorBand6On;

    if (Length==0)
      dBGain = -90;
    else
		  dBGain = log10(Length)*-20;

    int X1 = FAxisLeftMargin+BorderWidth+((float)(cnt*HorizontalAxisLength)/FNrOfPoints);
    int Y1 = height()-BorderWidth-(((float)(18-dBGain)*VerticalAxisLength)/36);
	  TotalCurve[cnt] = QPoint(X1, Y1);
  }
}

void DNREQPanel::CalculatePolePosition(float *X, float *Y, float *Coefficients)
{
	float a = Coefficients[3];
	float b = Coefficients[4];
	float c = Coefficients[5];

	float R = 0;
	float I = 0;
	float B = b/(2*a);

	if (((b*b) - (4*a*c))>0)
	{
		R =  sqrt((b*b)-4*a*c)/(2*a);

		X[0] = (R-B);
		Y[0] = 0;

		X[1] = (-R-B);
		Y[1] = 0;
	}
	else
	{
		I =  sqrt(4*a*c-(b*b))/(2*a);
		X[0] = -B;
		Y[0] = I;

		X[1] = -B;
		Y[1] = -I;
	}
}

void DNREQPanel::CalculateZeroPosition(float *X, float *Y, float *Coefficients)
{
    //TODO: Add your source code here

    // zeros
    float a = Coefficients[0];
    float b = Coefficients[1];
    float c = Coefficients[2];

    float R = 0;
    float I = 0;
    float B = b/(2*a);

        if (((b*b) - (4*a*c))>0)
	{
		R =  sqrt((b*b)-4*a*c)/(2*a);

		X[0] = (R-B);
		Y[0] = 0;

		X[1] = (-R-B);
		Y[1] = 0;
	}
	else
	{
		I =  sqrt(4*a*c-(b*b))/(2*a);
		X[0] = -B;
		Y[0] = I;

		X[1] = -B;
		Y[1] = -I;
	}
}

float DNREQPanel::CalculateEQ(float *Coefficients, double Gain, int Frequency, double Bandwidth, double Slope, int Type, bool On)
{
  // [0] = CoefficientZero0;
  // [1] = CoefficientZero1;
  // [2] = CoefficientZero2;
  // [3] = CoefficientPole0;
  // [4] = CoefficientPole1;
  // [5] = CoefficientPole2;
  double b0,b1,b2;
  double a0,a1,a2;
  bool Zolzer = true;

  //default is off
  b0 = 1;
  b1 = 0;
  b2 = 0;

  a0 = 1;
  a1 = 0;
  a2 = 0;

  if (!On)
  {
    Type = OFF;
  }

  if (!Zolzer)
  {
    double A = pow(10, (double)Gain/40);
    double Omega = (2*M_PI*Frequency)/FSamplerate;
    double Cs = cos(Omega);
    double Sn = sin(Omega);
    double Q = Sn/(log(2)*Bandwidth*Omega);
    double Alpha;

    if (Type==PEAKINGEQ)
    {
      Alpha = Sn*sinhl(1/(2*Q))*2*pow(10, (double)fabs(Gain)/40);
    }
    else
    {
      Alpha = Sn*sinhl(1/(2*Q));
    }

    switch (Type)
    {
      case LPF:
      {// LPF
        b0 = (1 - Cs)/2;
       	b1 = 1 - Cs;
       	b2 = (1 - Cs)/2;

       	a0 = 1 + Alpha;
       	a1 = -2*Cs;
       	a2 = 1 - Alpha;
      }
      break;
      case HPF:
      {// HPF
        b0 = (1 + Cs)/2;
       	b1 = -1 - Cs;
       	b2 = (1 + Cs)/2;

       	a0 = 1 + Alpha;
       	a1 = -2*Cs;
       		a2 = 1 - Alpha;
      }
      break;
      case BPF:
      {// BPF
        b0 = Alpha;
      	b1 = 0;
       	b2 = -Alpha;

       	a0 = 1 + Alpha;
       	a1 = -2*Cs;
       	a2 = 1 - Alpha;
      }
      break;
      case NOTCH:
      {// notch
        if (Gain<0)
       	{
       	  b0 = 1 + pow(10, (double)Gain/20)*Sn*sinh(1/(2*Q));
       		b1 = -2*Cs;
       		b2 = 1 - pow(10, (double)Gain/20)*Sn*sinh(1/(2*Q));

       		a0 = 1 + Sn*sinh(1/(2*Q));
       	 	a1 = -2*Cs;
       		a2 = 1 - Sn*sinh(1/(2*Q));
       	}
       	else
       	{
       	  b0 = 1 + Sn*sinh(1/(2*Q));
       		b1 = -2*Cs;
       		b2 = 1 - Sn*sinh(1/(2*Q));

       		a0 = 1 + pow(10, (double)-Gain/20)*Sn*sinh(1/(2*Q));
       	 	a1 = -2*Cs;
       		a2 = 1 - pow(10, (double)-Gain/20)*Sn*sinh(1/(2*Q));
       	}
      }
      break;
      case PEAKINGEQ:
      {	//Peaking EQ
        b0 = 1 + Alpha*A;
      	b1 = -2*Cs;
      	b2 = 1 - Alpha*A;

        a0 = 1 + Alpha/A;
      	a1 = -2*Cs;
      	a2 = 1 - Alpha/A;
      }
      break;
      case LOWSHELF:
      {// lowShelf
        b0 =   A*((A+1) - ((A-1)*Cs));// + (Beta*Sn));
      	b1 = 2*A*((A-1) - ((A+1)*Cs));
      	b2 =   A*((A+1) - ((A-1)*Cs));// - (Beta*Sn));

      	a0 =      (A+1) + ((A-1)*Cs);// + (Beta*Sn);
      	a1 =	-2*((A-1) + ((A+1)*Cs));
      	a2 =		 (A+1) + ((A-1)*Cs);// - (Beta*Sn);
      }
      break;
      case HIGHSHELF:
      {// highShelf
        b0 =    A*((A+1) + ((A-1)*Cs));// + (Beta*Sn));
      	b1 = -2*A*((A-1) + ((A+1)*Cs));
      	b2 =    A*((A+1) + ((A-1)*Cs));// - (Beta*Sn));

      	a0 =      (A+1) - ((A-1)*Cs);// + (Beta*Sn);
      	a1 =	 2*((A-1) - ((A+1)*Cs));
      	a2 =		 (A+1) - ((A-1)*Cs);// - (Beta*Sn);
      }
      break;
    }
  }
  else
  {
    //double A = pow(10, (double)Gain/40);
    double Omega = (2*M_PI*Frequency)/FSamplerate;
    double Cs = cos(Omega);
    double Sn = sin(Omega);
    double Q = Sn/(log(2)*Bandwidth*Omega);
    double Alpha;

    if (Type==PEAKINGEQ)
    {
      Alpha = Sn*sinhl(1/(2*Q))*2*pow(10, (double)fabs(Gain)/40);
    }
    else
    {
      Alpha = Sn*sinhl(1/(2*Q));
    }

   	double K = tan(Omega/2);
   	switch (Type)
	  {
		  case LPF:
		  {
        a0 = 1;
				a1 = (2*((K*K)-1))/(1+sqrt(2)*K+(K*K));
				a2 = (1-sqrt(2)*K+(K*K))/(1+sqrt(2)*K+(K*K));

				b0 = (K*K)/(1+sqrt(2)*K+(K*K));
				b1 = (2*(K*K))/(1+sqrt(2)*K+(K*K));
				b2 = (K*K)/(1+sqrt(2)*K+(K*K));
			}
   		break;
   		case HPF:
   		{// HPF
				a0 = 1;
				a1 = (2*((K*K)-1))/(1+sqrt(2)*K+(K*K));
				a2 = (1-sqrt(2)*K+(K*K))/(1+sqrt(2)*K+(K*K));

				b0 = 1/(1+sqrt(2)*K+(K*K));
				b1 = -2/(1+sqrt(2)*K+(K*K));
				b2 = 1/(1+sqrt(2)*K+(K*K));
   		}
   		break;
   		case BPF:
   		{// BPF
   			b0 = Alpha;
   			b1 = 0;
   			b2 = -Alpha;

   			a0 = 1 + Alpha;
   			a1 = -2*Cs;
   			a2 = 1 - Alpha;
	   	}
   		break;
   		case NOTCH:
   		{// notch
   			if (Gain<0)
   			{
   				b0 = 1+ pow(10,(double)Gain/20)*Alpha;
   				b1 = -2*Cs;
   				b2 = 1- pow(10,(double)Gain/20)*Alpha;

   				a0 = 1 + Alpha;
   				a1 = -2*Cs;
   				a2 = 1 - Alpha;
   			}
   			else
   			{
					double A = pow(10,(double)Gain/20);
					a0 = 1;
					a1 = (2*((K*K)-1))/(1+(K/Q)+(K*K));
					a2 = (1-(K/Q)+(K*K))/(1+(K/Q)+(K*K));

					b0 = (1+((A*K)/Q)+(K*K))/(1+(K/Q)+(K*K));
					b1 = (2*((K*K)-1))/(1+(K/Q)+(K*K));
					b2 = (1-((A*K)/Q)+(K*K))/(1+(K/Q)+(K*K));
   			}
   		}
   		break;
      case PEAKINGEQ:
   		{	//Peaking EQ
				if (Gain>0)
				{
					float A = pow(10,(double)Gain/20);
					a0 = 1;
					a1 = (2*((K*K)-1))/(1+(K/Q)+(K*K));
					a2 = (1-(K/Q)+(K*K))/(1+(K/Q)+(K*K));

					b0 = (1+((A*K)/Q)+(K*K))/(1+(K/Q)+(K*K));
					b1 = (2*((K*K)-1))/(1+(K/Q)+(K*K));
					b2 = (1-((A*K)/Q)+(K*K))/(1+(K/Q)+(K*K));
				}
				else
				{
					float A = pow(10,(double)-Gain/20);

					a0 = 1;
					a1 = (2*((K*K)-1))/(1+((A*K)/Q)+(K*K));
					a2 = (1-((A*K)/Q)+(K*K))/(1+((A*K)/Q)+(K*K));

					b0 = (1+(K/Q)+(K*K))/(1+((A*K)/Q)+(K*K));
					b1 = (2*((K*K)-1))/(1+((A*K)/Q)+(K*K));
					b2 = (1-(K/Q)+(K*K))/(1+((A*K)/Q)+(K*K));
				}
   		}
   		break;
   		case LOWSHELF:
   		{// lowShelf
   			if (Gain>0)
   			{
   				double A = pow(10,(double)Gain/20);
   				a0 = 1;
   				a1 = (2*((K*K)-1))/(1+(sqrt(2*Slope)*K)+(K*K));
   				a2 = (1-(sqrt(2*Slope)*K)+(K*K))/(1+(sqrt(2*Slope)*K)+(K*K));

	   			b0 = (1+(sqrt(2*A*Slope)*K)+(A*K*K))/(1+(sqrt(2*Slope)*K)+(K*K));
   				b1 = (2*((A*K*K)-1))/(1+(sqrt(2*Slope)*K)+(K*K));
   				b2 = (1-(sqrt(2*A*Slope)*K)+(A*K*K))/(1+(sqrt(2*Slope)*K)+(K*K));
   			}
   			else
   			{
   				double A = pow(10,(double)-Gain/20);
   				a0 = 1;
   				a1 = (2*((A*K*K)-1))/(1+(sqrt(2*A*Slope)*K)+(A*K*K));
   				a2 = (1-(sqrt(2*A*Slope)*K)+(A*K*K))/(1+(sqrt(2*A*Slope)*K)+(A*K*K));

   				b0 = (1+(sqrt(2*Slope)*K)+(K*K))/(1+(sqrt(2*A*Slope)*K)+(A*K*K));
   				b1 = (2*((K*K)-1))/(1+(sqrt(2*A*Slope)*K)+(A*K*K));
   				b2 = (1-(sqrt(2*Slope)*K)+(K*K))/(1+(sqrt(2*A*Slope)*K)+(A*K*K));
   			}
   		}
   		break;
   		case HIGHSHELF:
   		{// highShelf
   			if (Gain>0)
   			{
   				double A = pow(10,(double)Gain/20);
   				a0 = 1;
   				a1 = (2*((K*K)-1))/(1+(sqrt(2*Slope)*K)+(K*K));
   				a2 = (1-(sqrt(2*Slope)*K)+(K*K))/(1+(sqrt(2*Slope)*K)+(K*K));

   				b0 = (A+(sqrt(2*A*Slope)*K)+(K*K))/(1+(sqrt(2*Slope)*K)+(K*K));
   				b1 = (2*((K*K)-A))/(1+(sqrt(2*Slope)*K)+(K*K));
   				b2 = (A-(sqrt(2*A*Slope)*K)+(K*K))/(1+(sqrt(2*Slope)*K)+(K*K));
   			}
   			else
   			{
   				double A = pow(10,(double)-Gain/20);
   				a0 = 1;
   				a1 = (2*(((K*K)/A)-1))/(1+(sqrt((2*Slope)/A)*K)+((K*K)/A));
   				a2 = (1-(sqrt((2*Slope)/A)*K)+((K*K)/A))/(1+(sqrt((2*Slope)/A)*K)+((K*K)/A));

   				b0 = (1+(sqrt(2*Slope)*K)+(K*K))/(A+(sqrt(2*A*Slope)*K)+(K*K));
   				b1 = (2*((K*K)-1))/(A+(sqrt(2*A*Slope)*K)+(K*K));
   				b2 = (1-(sqrt(2*Slope)*K)+(K*K))/(A+(sqrt(2*A*Slope)*K)+(K*K));
   			}
   		}
   		break;
	  }
  }

  Coefficients[0] = b0;
  Coefficients[1] = b1;
  Coefficients[2] = b2;
  Coefficients[3] = a0;
  Coefficients[4] = a1;
  Coefficients[5] = a2;

  return b0/a0;
}

void DNREQPanel::resizeEvent(QResizeEvent *event)
{
  CalculateCurve();
  update();
}
