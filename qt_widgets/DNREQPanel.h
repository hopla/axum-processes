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

#ifndef DNREQPANEL_H
#define DNREQPANEL_H

#include "DNRDefines.h"
#include <QWidget>
#include <QtDesigner/QDesignerExportWidget>

class QDESIGNER_WIDGET_EXPORT DNREQPanel : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QColor BackgroundColor READ getBackgroundColor WRITE setBackgroundColor);
    Q_PROPERTY(QColor AxisColor READ getAxisColor WRITE setAxisColor);
    Q_PROPERTY(QColor GridColor READ getGridColor WRITE setGridColor);
    Q_PROPERTY(QColor LogGridColor READ getLogGridColor WRITE setLogGridColor);
    Q_PROPERTY(QColor TextColor READ getTextColor WRITE setTextColor);
//    Q_PROPERTY(TAnchors Anchors READ getAnchors WRITE setAnchors);
    Q_PROPERTY(int VerticalGridAtdB READ getVerticalGridAtdB WRITE setVerticalGridAtdB);
    Q_PROPERTY(int AnchorSize READ getAnchorSize WRITE setAnchorSize);
    Q_PROPERTY(int FrequencyLowCut READ getFrequencyLowCut WRITE setFrequencyLowCut);
    Q_PROPERTY(bool LCOn READ getLCOn WRITE setLCOn);
    Q_PROPERTY(double GainBand1 READ getGainBand1 WRITE setGainBand1);
    Q_PROPERTY(double GainBand2 READ getGainBand2 WRITE setGainBand2);
    Q_PROPERTY(double GainBand3 READ getGainBand3 WRITE setGainBand3);
    Q_PROPERTY(double GainBand4 READ getGainBand4 WRITE setGainBand4);
    Q_PROPERTY(double GainBand5 READ getGainBand5 WRITE setGainBand5);
    Q_PROPERTY(double GainBand6 READ getGainBand6 WRITE setGainBand6);
    Q_PROPERTY(int FrequencyBand1 READ getFrequencyBand1 WRITE setFrequencyBand1);
    Q_PROPERTY(int FrequencyBand2 READ getFrequencyBand2 WRITE setFrequencyBand2);
    Q_PROPERTY(int FrequencyBand3 READ getFrequencyBand3 WRITE setFrequencyBand3);
    Q_PROPERTY(int FrequencyBand4 READ getFrequencyBand4 WRITE setFrequencyBand4);
    Q_PROPERTY(int FrequencyBand5 READ getFrequencyBand5 WRITE setFrequencyBand5);
    Q_PROPERTY(int FrequencyBand6 READ getFrequencyBand6 WRITE setFrequencyBand6);
    Q_PROPERTY(double BandwidthBand1 READ getBandwidthBand1 WRITE setBandwidthBand1);
    Q_PROPERTY(double BandwidthBand2 READ getBandwidthBand2 WRITE setBandwidthBand2);
    Q_PROPERTY(double BandwidthBand3 READ getBandwidthBand3 WRITE setBandwidthBand3);
    Q_PROPERTY(double BandwidthBand4 READ getBandwidthBand4 WRITE setBandwidthBand4);
    Q_PROPERTY(double BandwidthBand5 READ getBandwidthBand5 WRITE setBandwidthBand5);
    Q_PROPERTY(double BandwidthBand6 READ getBandwidthBand6 WRITE setBandwidthBand6);
    Q_PROPERTY(int NrOfPoints READ getNrOfPoints WRITE setNrOfPoints);
    Q_PROPERTY(int AxisBorderWidth READ getAxisBorderWidth WRITE setAxisBorderWidth);
    Q_PROPERTY(int AxisLeftMargin READ getAxisLeftMargin WRITE setAxisLeftMargin);
    Q_PROPERTY(QColor ActiveCurveColor READ getActiveCurveColor WRITE setActiveCurveColor);
    Q_PROPERTY(int ActiveCurveWidth READ getActiveCurveWidth WRITE setActiveCurveWidth);
    Q_PROPERTY(QColor ActiveCurveFillColor READ getActiveCurveFillColor WRITE setActiveCurveFillColor);
    Q_PROPERTY(QColor InactiveCurveColor READ getInactiveCurveColor WRITE setInactiveCurveColor);
    Q_PROPERTY(int InactiveCurveWidth READ getInactiveCurveWidth WRITE setInactiveCurveWidth);
    Q_PROPERTY(QColor InactiveCurveFillColor READ getInactiveCurveFillColor WRITE setInactiveCurveFillColor);
    Q_PROPERTY(QColor AnchorBand1Color READ getAnchorBand1Color WRITE setAnchorBand1Color);
    Q_PROPERTY(QColor AnchorBand2Color READ getAnchorBand2Color WRITE setAnchorBand2Color);
    Q_PROPERTY(QColor AnchorBand3Color READ getAnchorBand3Color WRITE setAnchorBand3Color);
    Q_PROPERTY(QColor AnchorBand4Color READ getAnchorBand4Color WRITE setAnchorBand4Color);
    Q_PROPERTY(QColor AnchorBand5Color READ getAnchorBand5Color WRITE setAnchorBand5Color);
    Q_PROPERTY(QColor AnchorBand6Color READ getAnchorBand6Color WRITE setAnchorBand6Color);
    Q_PROPERTY(bool DrawAnchors READ getDrawAnchors WRITE setDrawAnchors);
    Q_PROPERTY(int TypeBand1 READ getTypeBand1 WRITE setTypeBand1);
    Q_PROPERTY(int TypeBand2 READ getTypeBand2 WRITE setTypeBand2);
    Q_PROPERTY(int TypeBand3 READ getTypeBand3 WRITE setTypeBand3);
    Q_PROPERTY(int TypeBand4 READ getTypeBand4 WRITE setTypeBand4);
    Q_PROPERTY(int TypeBand5 READ getTypeBand5 WRITE setTypeBand5);
    Q_PROPERTY(int TypeBand6 READ getTypeBand6 WRITE setTypeBand6);
    Q_PROPERTY(int Samplerate READ getSamplerate WRITE setSamplerate);
    Q_PROPERTY(double NequistDivide READ getNequistDivide WRITE setNequistDivide);
    Q_PROPERTY(int AnchorPickupSize READ getAnchorPickupSize WRITE setAnchorPickupSize);
    Q_PROPERTY(bool LogGrid READ getLogGrid WRITE setLogGrid);
    Q_PROPERTY(bool EQOn READ getEQOn WRITE setEQOn);
    Q_PROPERTY(bool OnBand1 READ getOnBand1 WRITE setOnBand1);
    Q_PROPERTY(bool OnBand2 READ getOnBand2 WRITE setOnBand2);
    Q_PROPERTY(bool OnBand3 READ getOnBand3 WRITE setOnBand3);
    Q_PROPERTY(bool OnBand4 READ getOnBand4 WRITE setOnBand4);
    Q_PROPERTY(bool OnBand5 READ getOnBand5 WRITE setOnBand5);
    Q_PROPERTY(bool OnBand6 READ getOnBand6 WRITE setOnBand6);
    Q_PROPERTY(bool ShowFrequencyText READ getShowFrequencyText WRITE setShowFrequencyText);
    Q_PROPERTY(bool ShowGainText READ getShowGainText WRITE setShowGainText);
    Q_PROPERTY(double SlopeBand1 READ getSlopeBand1 WRITE setSlopeBand1);
    Q_PROPERTY(double SlopeBand2 READ getSlopeBand2 WRITE setSlopeBand2);
    Q_PROPERTY(double SlopeBand3 READ getSlopeBand3 WRITE setSlopeBand3);
    Q_PROPERTY(double SlopeBand4 READ getSlopeBand4 WRITE setSlopeBand4);
    Q_PROPERTY(double SlopeBand5 READ getSlopeBand5 WRITE setSlopeBand5);
    Q_PROPERTY(double SlopeBand6 READ getSlopeBand6 WRITE setSlopeBand6);
public:
    DNREQPanel(QWidget *parent = 0);

    QColor getBackgroundColor();
    void setBackgroundColor(QColor NewBackgroundColor);

    QColor getAxisColor();
    void setAxisColor(QColor NewAxisColor);

    QColor getGridColor();
    void setGridColor(QColor NewGridColor);

    QColor getLogGridColor();
    void setLogGridColor(QColor NewLogGridColor);

    QColor getTextColor();
    void setTextColor(QColor NewTextColor);

//    TAnchors getAnchors();
//    void setAnchors(TAnchors NewAnchors);

    int getVerticalGridAtdB();
    void setVerticalGridAtdB(int NewVerticalGridAtdB);

    int getAnchorSize();
    void setAnchorSize(int NewAnchorSize);

    int getFrequencyLowCut();
    void setFrequencyLowCut(int NewFrequencyLowCut);
    
    bool getLCOn();
    void setLCOn(bool NewLCOn);

    double getGainBand1();
    void setGainBand1(double NewGainBand1);
    double getGainBand2();
    void setGainBand2(double NewGainBand2);
    double getGainBand3();
    void setGainBand3(double NewGainBand3);
    double getGainBand4();
    void setGainBand4(double NewGainBand4);
    double getGainBand5();
    void setGainBand5(double NewGainBand5);
    double getGainBand6();
    void setGainBand6(double NewGainBand6);

    int getFrequencyBand1();
    void setFrequencyBand1(int NewFrequencyBand1);
    int getFrequencyBand2();
    void setFrequencyBand2(int NewFrequencyBand2);
    int getFrequencyBand3();
    void setFrequencyBand3(int NewFrequencyBand3);
    int getFrequencyBand4();
    void setFrequencyBand4(int NewFrequencyBand4);
    int getFrequencyBand5();
    void setFrequencyBand5(int NewFrequencyBand5);
    int getFrequencyBand6();
    void setFrequencyBand6(int NewFrequencyBand6);

    double getBandwidthBand1();
    void setBandwidthBand1(double NewBandwidthBand1);
    double getBandwidthBand2();
    void setBandwidthBand2(double NewBandwidthBand2);
    double getBandwidthBand3();
    void setBandwidthBand3(double NewBandwidthBand3);
    double getBandwidthBand4();
    void setBandwidthBand4(double NewBandwidthBand4);
    double getBandwidthBand5();
    void setBandwidthBand5(double NewBandwidthBand5);
    double getBandwidthBand6();
    void setBandwidthBand6(double NewBandwidthBand6);

    int getNrOfPoints();
    void setNrOfPoints(int NewNrOfPoints);

    int getAxisBorderWidth();
    void setAxisBorderWidth(int NewAxisBorderWidth);

    int getAxisLeftMargin();
    void setAxisLeftMargin(int NewAxisLeftMargin);

    QColor getActiveCurveColor();
    void setActiveCurveColor(QColor NewActiveCurveColor);

    int getActiveCurveWidth();
    void setActiveCurveWidth(int setActiveCurveWidth);

    QColor getActiveCurveFillColor();
    void setActiveCurveFillColor(QColor NewActiveCurveFillColor);

    QColor getInactiveCurveColor();
    void setInactiveCurveColor(QColor NewInactiveCurveColor);

    int getInactiveCurveWidth();
    void setInactiveCurveWidth(int setInactiveCurveWidth);

    QColor getInactiveCurveFillColor();
    void setInactiveCurveFillColor(QColor NewInactiveCurveFillColor);

    int getTotalCurveWidth();
    void setTotalCurveWidth(int NewTotalCurveWidth);

    QColor getAnchorBand1Color();
    void setAnchorBand1Color(QColor NewAnchorBand1Color);
    QColor getAnchorBand2Color();
    void setAnchorBand2Color(QColor NewAnchorBand2Color);
    QColor getAnchorBand3Color();
    void setAnchorBand3Color(QColor NewAnchorBand3Color);
    QColor getAnchorBand4Color();
    void setAnchorBand4Color(QColor NewAnchorBand4Color);
    QColor getAnchorBand5Color();
    void setAnchorBand5Color(QColor NewAnchorBand5Color);
    QColor getAnchorBand6Color();
    void setAnchorBand6Color(QColor NewAnchorBand6Color);

    bool getDrawAnchors();
    void setDrawAnchors(bool NewDrawAnchors);

    int getTypeBand1();
    void setTypeBand1(int NewTypeBand1);
    int getTypeBand2();
    void setTypeBand2(int NewTypeBand2);
    int getTypeBand3();
    void setTypeBand3(int NewTypeBand3);
    int getTypeBand4();
    void setTypeBand4(int NewTypeBand4);
    int getTypeBand5();
    void setTypeBand5(int NewTypeBand5);
    int getTypeBand6();
    void setTypeBand6(int NewTypeBand6);

    int getSamplerate();
    void setSamplerate(int NewSamplerate);

    double getNequistDivide();
    void setNequistDivide(double NewNequistDivide);

    int getAnchorPickupSize();
    void setAnchorPickupSize(int NewAnchorPickupSize);

    bool getLogGrid();
    void setLogGrid(bool NewLogGrid);

    bool getEQOn();
    void setEQOn(bool NewEQOn);

    bool getOnBand1();
    void setOnBand1(bool NewOnBand1);
    bool getOnBand2();
    void setOnBand2(bool NewOnBand2);
    bool getOnBand3();
    void setOnBand3(bool NewOnBand3);
    bool getOnBand4();
    void setOnBand4(bool NewOnBand4);
    bool getOnBand5();
    void setOnBand5(bool NewOnBand5);
    bool getOnBand6();
    void setOnBand6(bool NewOnBand6);
    
    bool getShowFrequencyText();
    void setShowFrequencyText(bool NewShowFrequencyText);

    bool getShowGainText();
    void setShowGainText(bool NewShowGainText);

    double getSlopeBand1();
    void setSlopeBand1(double NewSlopeBand1);
    double getSlopeBand2();
    void setSlopeBand2(double NewSlopeBand2);
    double getSlopeBand3();
    void setSlopeBand3(double NewSlopeBand3);
    double getSlopeBand4();
    void setSlopeBand4(double NewSlopeBand4);
    double getSlopeBand5();
    void setSlopeBand5(double NewSlopeBand5);
    double getSlopeBand6();
    void setSlopeBand6(double NewSlopeBand6);

private:
    QColor FBackgroundColor;
    QColor FAxisColor;
    QColor FGridColor;
    QColor FLogGridColor;
    QColor FTextColor;
//    TAnchors FAnchors;
    int FVerticalGridAtdB;
    int FAnchorSize;
    
    int FFrequencyLowCut;
    bool FLCOn;
    
    double FGainBand1;
    double FGainBand2;
    double FGainBand3;
    double FGainBand4;
    double FGainBand5;
    double FGainBand6;
    int FFrequencyBand1;
    int FFrequencyBand2;
    int FFrequencyBand3;
    int FFrequencyBand4;
    int FFrequencyBand5;
    int FFrequencyBand6;
    double FBandwidthBand1;
    double FBandwidthBand2;
    double FBandwidthBand3;
    double FBandwidthBand4;
    double FBandwidthBand5;
    double FBandwidthBand6;
    int FNrOfPoints;
    int FAxisBorderWidth;
    int FAxisLeftMargin;
    QColor FActiveCurveColor;
    int FActiveCurveWidth;
    QColor FActiveCurveFillColor;
    QColor FInactiveCurveColor;
    int FInactiveCurveWidth;
    QColor FInactiveCurveFillColor;
    QColor FAnchorBand1Color;
    QColor FAnchorBand2Color;
    QColor FAnchorBand3Color;
    QColor FAnchorBand4Color;
    QColor FAnchorBand5Color;
    QColor FAnchorBand6Color;
    bool FDrawAnchors;
    int FTypeBand1;
    int FTypeBand2;
    int FTypeBand3;
    int FTypeBand4;
    int FTypeBand5;
    int FTypeBand6;
    int FSamplerate;
    double FNequistDivide;
    int FAnchorPickupSize;
    bool FLogGrid;
    bool FEQOn;
    bool FOnBand1;
    bool FOnBand2;
    bool FOnBand3;
    bool FOnBand4;
    bool FOnBand5;
    bool FOnBand6;
    bool FShowFrequencyText;
    bool FShowGainText;
    double FSlopeBand1;
    double FSlopeBand2;
    double FSlopeBand3;
    double FSlopeBand4;
    double FSlopeBand5;
    double FSlopeBand6;
    
    QPoint EQCurve[1024];
    QPoint LCCurve[1024];

    float GainFactorLowCut;

    float GainFactorBand1;
    float GainFactorBand2;
    float GainFactorBand3;
    float GainFactorBand4;
    float GainFactorBand5;
    float GainFactorBand6;
    float GainFactorBand1On;
    float GainFactorBand2On;
    float GainFactorBand3On;
    float GainFactorBand4On;
    float GainFactorBand5On;
    float GainFactorBand6On;

    float PolesXLowCut[2];
    float PolesYLowCut[2];
    float ZerosXLowCut[2];
    float ZerosYLowCut[2];

    float PolesXBand1[2];
    float PolesYBand1[2];
    float ZerosXBand1[2];
    float ZerosYBand1[2];
    float PolesXBand2[2];
    float PolesYBand2[2];
    float ZerosXBand2[2];
    float ZerosYBand2[2];
    float PolesXBand3[2];
    float PolesYBand3[2];
    float ZerosXBand3[2];
    float ZerosYBand3[2];
    float PolesXBand4[2];
    float PolesYBand4[2];
    float ZerosXBand4[2];
    float ZerosYBand4[2];
    float PolesXBand5[2];
    float PolesYBand5[2];
    float ZerosXBand5[2];
    float ZerosYBand5[2];
    float PolesXBand6[2];
    float PolesYBand6[2];
    float ZerosXBand6[2];
    float ZerosYBand6[2];
    float PolesXBand1On[2];
    float PolesYBand1On[2];
    float ZerosXBand1On[2];
    float ZerosYBand1On[2];
    float PolesXBand2On[2];
    float PolesYBand2On[2];
    float ZerosXBand2On[2];
    float ZerosYBand2On[2];
    float PolesXBand3On[2];
    float PolesYBand3On[2];
    float ZerosXBand3On[2];
    float ZerosYBand3On[2];
    float PolesXBand4On[2];
    float PolesYBand4On[2];
    float ZerosXBand4On[2];
    float ZerosYBand4On[2];
    float PolesXBand5On[2];
    float PolesYBand5On[2];
    float ZerosXBand5On[2];
    float ZerosYBand5On[2];
    float PolesXBand6On[2];
    float PolesYBand6On[2];
    float ZerosXBand6On[2];
    float ZerosYBand6On[2];

protected:
    void paintEvent(QPaintEvent *event);
    void CalculateEQCurve();
    void CalculateLCCurve();
    void CalculatePolePosition(float *X, float *Y, float *Coefficients);
    void CalculateZeroPosition(float *X, float *Y, float *Coefficients);
    float CalculateEQ(float *Coefficients, double Gain, int Frequency, double Bandwidth, double Slope, int Type, bool On);

//    void mouseMoveEvent(QMouseEvent *ev);
    void resizeEvent(QResizeEvent *event);
};

#endif
