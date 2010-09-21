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
//    Q_PROPERTY(TAnchors Anchors READ getAnchors WRITE setAnchors);
    Q_PROPERTY(int VerticalGridAtdB READ getVerticalGridAtdB WRITE setVerticalGridAtdB);
    Q_PROPERTY(float AnchorSize READ getAnchorSize WRITE setAnchorSize);
    Q_PROPERTY(float GainBand1 READ getGainBand1 WRITE setGainBand1);
    Q_PROPERTY(float GainBand2 READ getGainBand2 WRITE setGainBand2);
    Q_PROPERTY(float GainBand3 READ getGainBand3 WRITE setGainBand3);
    Q_PROPERTY(float GainBand4 READ getGainBand4 WRITE setGainBand4);
    Q_PROPERTY(float GainBand5 READ getGainBand5 WRITE setGainBand5);
    Q_PROPERTY(float GainBand6 READ getGainBand6 WRITE setGainBand6);
    Q_PROPERTY(float FrequencyBand1 READ getFrequencyBand1 WRITE setFrequencyBand1);
    Q_PROPERTY(float FrequencyBand2 READ getFrequencyBand2 WRITE setFrequencyBand2);
    Q_PROPERTY(float FrequencyBand3 READ getFrequencyBand3 WRITE setFrequencyBand3);
    Q_PROPERTY(float FrequencyBand4 READ getFrequencyBand4 WRITE setFrequencyBand4);
    Q_PROPERTY(float FrequencyBand5 READ getFrequencyBand5 WRITE setFrequencyBand5);
    Q_PROPERTY(float FrequencyBand6 READ getFrequencyBand6 WRITE setFrequencyBand6);
    Q_PROPERTY(float BandwidthBand1 READ getBandwidthBand1 WRITE setBandwidthBand1);
    Q_PROPERTY(float BandwidthBand2 READ getBandwidthBand2 WRITE setBandwidthBand2);
    Q_PROPERTY(float BandwidthBand3 READ getBandwidthBand3 WRITE setBandwidthBand3);
    Q_PROPERTY(float BandwidthBand4 READ getBandwidthBand4 WRITE setBandwidthBand4);
    Q_PROPERTY(float BandwidthBand5 READ getBandwidthBand5 WRITE setBandwidthBand5);
    Q_PROPERTY(float BandwidthBand6 READ getBandwidthBand6 WRITE setBandwidthBand6);
    Q_PROPERTY(int NrOfPoints READ getNrOfPoints WRITE setNrOfPoints);
    Q_PROPERTY(int AxisBorderWidth READ getAxisBorderWidth WRITE setAxisBorderWidth);
    Q_PROPERTY(int AxisLeftMargin READ getAxisLeftMargin WRITE setAxisLeftMargin);
    Q_PROPERTY(QColor ActiveCurveColor READ getActiveCurveColor WRITE setActiveCurveColor);
    Q_PROPERTY(int ActiveCurveWidth READ getActiveCurveWidth WRITE setActiveCurveWidth);
    Q_PROPERTY(QColor TotalCurveColor READ getTotalCurveColor WRITE setTotalCurveColor);
    Q_PROPERTY(int TotalCurveWidth READ getTotalCurveWidth WRITE setTotalCurveWidth);
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
    Q_PROPERTY(float SlopeBand1 READ getSlopeBand1 WRITE setSlopeBand1);
    Q_PROPERTY(float SlopeBand2 READ getSlopeBand2 WRITE setSlopeBand2);
    Q_PROPERTY(float SlopeBand3 READ getSlopeBand3 WRITE setSlopeBand3);
    Q_PROPERTY(float SlopeBand4 READ getSlopeBand4 WRITE setSlopeBand4);
    Q_PROPERTY(float SlopeBand5 READ getSlopeBand5 WRITE setSlopeBand5);
    Q_PROPERTY(float SlopeBand6 READ getSlopeBand6 WRITE setSlopeBand6);
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

//    TAnchors getAnchors();
//    void setAnchors(TAnchors NewAnchors);

    int getVerticalGridAtdB();
    void setVerticalGridAtdB(int NewVerticalGridAtdB);

    float getAnchorSize();
    void setAnchorSize(float NewAnchorSize);

    float getGainBand1();
    void setGainBand1(float NewGainBand1);
    float getGainBand2();
    void setGainBand2(float NewGainBand2);
    float getGainBand3();
    void setGainBand3(float NewGainBand3);
    float getGainBand4();
    void setGainBand4(float NewGainBand4);
    float getGainBand5();
    void setGainBand5(float NewGainBand5);
    float getGainBand6();
    void setGainBand6(float NewGainBand6);

    float getFrequencyBand1();
    void setFrequencyBand1(float NewFrequencyBand1);
    float getFrequencyBand2();
    void setFrequencyBand2(float NewFrequencyBand2);
    float getFrequencyBand3();
    void setFrequencyBand3(float NewFrequencyBand3);
    float getFrequencyBand4();
    void setFrequencyBand4(float NewFrequencyBand4);
    float getFrequencyBand5();
    void setFrequencyBand5(float NewFrequencyBand5);
    float getFrequencyBand6();
    void setFrequencyBand6(float NewFrequencyBand6);

    float getBandwidthBand1();
    void setBandwidthBand1(float NewBandwidthBand1);
    float getBandwidthBand2();
    void setBandwidthBand2(float NewBandwidthBand2);
    float getBandwidthBand3();
    void setBandwidthBand3(float NewBandwidthBand3);
    float getBandwidthBand4();
    void setBandwidthBand4(float NewBandwidthBand4);
    float getBandwidthBand5();
    void setBandwidthBand5(float NewBandwidthBand5);
    float getBandwidthBand6();
    void setBandwidthBand6(float NewBandwidthBand6);

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

    QColor getTotalCurveColor();
    void setTotalCurveColor(QColor NewTotalCurveColor);

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

    float getSlopeBand1();
    void setSlopeBand1(float NewSlopeBand1);
    float getSlopeBand2();
    void setSlopeBand2(float NewSlopeBand2);
    float getSlopeBand3();
    void setSlopeBand3(float NewSlopeBand3);
    float getSlopeBand4();
    void setSlopeBand4(float NewSlopeBand4);
    float getSlopeBand5();
    void setSlopeBand5(float NewSlopeBand5);
    float getSlopeBand6();
    void setSlopeBand6(float NewSlopeBand6);

private:
    QColor FBackgroundColor;
    QColor FAxisColor;
    QColor FGridColor;
    QColor FLogGridColor;
//    TAnchors FAnchors;
    int FVerticalGridAtdB;
    float FAnchorSize;
    float FGainBand1;
    float FGainBand2;
    float FGainBand3;
    float FGainBand4;
    float FGainBand5;
    float FGainBand6;
    float FFrequencyBand1;
    float FFrequencyBand2;
    float FFrequencyBand3;
    float FFrequencyBand4;
    float FFrequencyBand5;
    float FFrequencyBand6;
    float FBandwidthBand1;
    float FBandwidthBand2;
    float FBandwidthBand3;
    float FBandwidthBand4;
    float FBandwidthBand5;
    float FBandwidthBand6;
    int FNrOfPoints;
    int FAxisBorderWidth;
    int FAxisLeftMargin;
    QColor FActiveCurveColor;
    int FActiveCurveWidth;
    QColor FTotalCurveColor;
    int FTotalCurveWidth;
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
    float FSlopeBand1;
    float FSlopeBand2;
    float FSlopeBand3;
    float FSlopeBand4;
    float FSlopeBand5;
    float FSlopeBand6;

protected:
    void paintEvent(QPaintEvent *event);
//    void mouseMoveEvent(QMouseEvent *ev);
};

#endif
