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

#ifndef DNRFADER_H
#define DNRFADER_H

//10cm fader is outside measurement 12,8 cm
#define FADER_GRAPHICS_RESOLUTION_Y    256.0
//10cm fader knob width 1,0 cm
#define FADER_GRAPHICS_RESOLUTION_X    27.0

//10cm fader shaft-length 11 cm
#define FADER_SHAFT_LENGTH             220.0

//10cm fader Track-length 10 cm
#define FADER_TRACK_LENGTH             200.0

//10cm fader knob width 1,0 cm
#define FADER_KNOB_WIDTH               27.0
//10cm fader knob height 2,56 cm
#define FADER_KNOB_HEIGHT              48.0

#include <DNRDefines.h>
#include <QWidget>
#include <QtDesigner/QDesignerExportWidget>

class QDESIGNER_WIDGET_EXPORT DNRFader : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int Number READ getNumber WRITE setNumber);
    Q_PROPERTY(double Position READ getPosition WRITE setPosition);
    Q_PROPERTY(int ShaftWidth READ getShaftWidth WRITE setShaftWidth);
    Q_PROPERTY(QColor BorderColor READ getBorderColor WRITE setBorderColor);
    Q_PROPERTY(QColor KnobColor READ getKnobColor WRITE setKnobColor);
    Q_PROPERTY(QString SkinEnvironmentVariable READ getSkinEnvironmentVariable WRITE setSkinEnvironmentVariable);
    Q_PROPERTY(QString FaderKnobFileName READ getFaderKnobFileName WRITE setFaderKnobFileName);
public:
    DNRFader(QWidget *parent = 0);
    QImage *FaderKnobQImage;

    int FNumber;
    void setNumber(int NewNumber);
    int getNumber();

    double  FPosition;
    void setPosition(double NewPosition);
public slots:
    void setPosition(int_number ChannelNr, double_position NewPosition);
public:
    double getPosition();

    int FShaftWidth;
    void setShaftWidth(int NewShaftWidth);
    int getShaftWidth();

	QColor FBorderColor;
	const QColor & getBorderColor() const;
    virtual void setBorderColor(const QColor & NewBorderColor);

	QColor FKnobColor;
	const QColor & getKnobColor() const;
    virtual void setKnobColor(const QColor & NewKnobColor);

    QString FSkinEnvironmentVariable;
    void setSkinEnvironmentVariable(const QString &NewSkinEnvironmentVariable);
    QString getSkinEnvironmentVariable() const;

    QString FFaderKnobFileName;
    void setFaderKnobFileName(const QString &NewFaderKnobFileName);
    QString getFaderKnobFileName() const;

protected:
    void paintEvent(QPaintEvent *event);
    void mouseMoveEvent(QMouseEvent *ev);

signals:
	void FaderMoved(int_number Number, double_position NewPosition);


};

#endif
