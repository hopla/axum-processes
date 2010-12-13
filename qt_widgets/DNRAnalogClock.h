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

#ifndef DNRANALOGCLOCK_H
#define DNRANALOGCLOCK_H

#include <QWidget>
#include <QtDesigner/QDesignerExportWidget>

class QDESIGNER_WIDGET_EXPORT DNRAnalogClock : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(bool HourLines READ getHourLines WRITE setHourLines);
    Q_PROPERTY(int HourLinesLength READ getHourLinesLength WRITE setHourLinesLength);
    Q_PROPERTY(int HourLinesWidth READ getHourLinesWidth WRITE setHourLinesWidth);
    Q_PROPERTY(QColor HourLinesColor READ getHourLinesColor WRITE setHourLinesColor);
    Q_PROPERTY(bool MinuteLines READ getMinuteLines WRITE setMinuteLines);
    Q_PROPERTY(int MinuteLinesLength READ getMinuteLinesLength WRITE setMinuteLinesLength);
    Q_PROPERTY(int MinuteLinesWidth READ getMinuteLinesWidth WRITE setMinuteLinesWidth);
    Q_PROPERTY(QColor MinuteLinesColor READ getMinuteLinesColor WRITE setMinuteLinesColor);
    Q_PROPERTY(bool SecondDots READ getSecondDots WRITE setSecondDots);
    Q_PROPERTY(bool SecondDotsCountDown READ getSecondDotsCountDown WRITE setSecondDotsCountDown);
    Q_PROPERTY(int DotSize READ getDotSize WRITE setDotSize);
    Q_PROPERTY(QColor DotColor READ getDotColor WRITE setDotColor);
    Q_PROPERTY(bool Hands READ getHands WRITE setHands);
    Q_PROPERTY(QColor HourHandColor READ getHourHandColor WRITE setHourHandColor);
    Q_PROPERTY(int HourHandBorder READ getHourHandBorder WRITE setHourHandBorder);
    Q_PROPERTY(int HourHandLength READ getHourHandLength WRITE setHourHandLength);
    Q_PROPERTY(int HourHandWidth READ getHourHandWidth WRITE setHourHandWidth);
    Q_PROPERTY(QColor MinuteHandColor READ getMinuteHandColor WRITE setMinuteHandColor);
    Q_PROPERTY(int MinuteHandBorder READ getMinuteHandBorder WRITE setMinuteHandBorder);
    Q_PROPERTY(int MinuteHandLength READ getMinuteHandLength WRITE setMinuteHandLength);
    Q_PROPERTY(int MinuteHandWidth READ getMinuteHandWidth WRITE setMinuteHandWidth);
    Q_PROPERTY(bool EndTime READ getEndTime WRITE setEndTime);
    Q_PROPERTY(int EndTimeLength READ getEndTimeLength WRITE setEndTimeLength);
    Q_PROPERTY(int EndTimeWidth READ getEndTimeWidth WRITE setEndTimeWidth);
    Q_PROPERTY(int EndTimeHour READ getEndTimeHour WRITE setEndTimeHour);
    Q_PROPERTY(int EndTimeMinute READ getEndTimeMinute WRITE setEndTimeMinute);
    Q_PROPERTY(int EndTimeSecond READ getEndTimeSecond WRITE setEndTimeSecond);
    Q_PROPERTY(QColor EndTimeColor READ getEndTimeColor WRITE setEndTimeColor);
    Q_PROPERTY(float CountDownTime READ getCountDownTime WRITE setCountDownTime);
    Q_PROPERTY(int CountDownWidth READ getCountDownWidth WRITE setCountDownWidth);
    Q_PROPERTY(int CountDownSpacing READ getCountDownSpacing WRITE setCountDownSpacing);
    Q_PROPERTY(QColor CountDownColor READ getCountDownColor WRITE setCountDownColor);

public:
    DNRAnalogClock(QWidget *parent = 0);

    bool FHourLines;
    int FHourLinesLength;
    int FHourLinesWidth;
    QColor FHourLinesColor;

    bool FMinuteLines;
    int FMinuteLinesLength;
    int FMinuteLinesWidth;
    QColor FMinuteLinesColor;

    bool FSecondDots;
    bool FSecondDotsCountDown;
    int FDotSize;
    QColor FDotColor;

    bool FHands;
    QColor FHourHandColor;
    int FHourHandBorder;
    int FHourHandLength;
    int FHourHandWidth;

    QColor FMinuteHandColor;
    int FMinuteHandBorder;
    int FMinuteHandLength;
    int FMinuteHandWidth;

    void setHourLines(bool NewHourLines);
    bool getHourLines();

    void setHourLinesLength(int NewHourLinesLength);
    int getHourLinesLength();

    void setHourLinesWidth(int NewHourLinesWidth);
    int getHourLinesWidth();

    void setHourLinesColor(QColor NewHourLinesColor);
    QColor getHourLinesColor();

    void setMinuteLines(bool NewMinuteLines);
    bool getMinuteLines();

    void setMinuteLinesLength(int NewMinuteLinesLength);
    int getMinuteLinesLength();

    void setMinuteLinesWidth(int NewMinuteLinesWidth);
    int getMinuteLinesWidth();

    void setMinuteLinesColor(QColor NewMinuteLinesColor);
    QColor getMinuteLinesColor();

    void setSecondDots(bool NewSecondDots);
    bool getSecondDots();

    void setSecondDotsCountDown(bool NewSecondDotsCountDown);
    bool getSecondDotsCountDown();

    void setDotSize(int NewDotSize);
    int getDotSize();

    void setDotColor(QColor NewDotColor);
    QColor getDotColor();

    void setHands(bool NewHands);
    bool getHands();

    void setHourHandColor(QColor NewHourHandColor);
    QColor getHourHandColor();

    void setHourHandBorder(int NewHourHandBorder);
    int getHourHandBorder();

    void setHourHandLength(int NewHourHandLength);
    int getHourHandLength();

    void setHourHandWidth(int NewHourHandWidth);
    int getHourHandWidth();

    void setMinuteHandColor(QColor NewMinuteHandColor);
    QColor getMinuteHandColor();

    void setMinuteHandBorder(int NewMinuteHandBorder);
    int getMinuteHandBorder();

    void setMinuteHandLength(int NewMinuteHandLength);
    int getMinuteHandLength();

    void setMinuteHandWidth(int NewMinuteHandWidth);
    int getMinuteHandWidth();

    void setEndTime(bool NewEndTime);
    bool getEndTime();

    void setEndTimeLength(int NewEndTimeLength);
    int getEndTimeLength();

    void setEndTimeWidth(int NewEndTimeWidth);
    int getEndTimeWidth();

    void setEndTimeHour(int NewEndTimeHour);
    int getEndTimeHour();

    void setEndTimeMinute(int NewEndTimeMinute);
    int getEndTimeMinute();

    void setEndTimeSecond(int NewEndTimeSecond);
    int getEndTimeSecond();

    void setEndTimeColor(QColor NewEndTimeColor);
    QColor getEndTimeColor();

    void setCountDownTime(float NewCountDownTime);
    float getCountDownTime();

    void setCountDownWidth(int NewCountDownWidth);
    int getCountDownWidth();

    void setCountDownSpacing(int NewCountDownSpacing);
    int getCountDownSpacing();

    void setCountDownColor(QColor NewCountDownColor);
    QColor getCountDownColor();

protected:
    int FHour;
    int FMinute;
    int FSecond;

    bool FEndTime;
    int FEndTimeLength;
    int FEndTimeWidth;
    int FEndTimeHour;
    int FEndTimeMinute;
    int FEndTimeSecond;
    QColor FEndTimeColor;

    float FCountDownTime;
    int FCountDownWidth;
    int FCountDownSpacing;
    QColor FCountDownColor;

    void paintEvent(QPaintEvent *event);

public slots:
    void checkTime();
};

#endif
