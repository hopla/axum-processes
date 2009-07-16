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

#ifndef DNRROTARYKNOB_H
#define DNRROTARYKNOB_H

#include "DNRDefines.h"
#include <QWidget>
#include <QtDesigner/QDesignerExportWidget>

class QDESIGNER_WIDGET_EXPORT DNRRotaryKnob : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int Number READ getNumber WRITE setNumber);
    Q_PROPERTY(double Position READ getPosition WRITE setPosition);
    Q_PROPERTY(QColor BorderColor READ getBorderColor WRITE setBorderColor);
    Q_PROPERTY(QColor KnobColor READ getKnobColor WRITE setKnobColor);
    Q_PROPERTY(bool ScaleKnobImage READ getScaleKnobImage WRITE setScaleKnobImage);
    Q_PROPERTY(QString SkinEnvironmentVariable READ getSkinEnvironmentVariable WRITE setSkinEnvironmentVariable);
    Q_PROPERTY(QString RotaryKnobFileName READ getRotaryKnobFileName WRITE setRotaryKnobFileName);
public:
    DNRRotaryKnob(QWidget *parent = 0);
    QImage *RotaryKnobQImage;

    int FNumber;
    void setNumber(int NewNumber);
    int getNumber();

	QColor FBorderColor;
	const QColor & getBorderColor() const;
    virtual void setBorderColor(const QColor & NewBorderColor);

	QColor FKnobColor;
	const QColor & getKnobColor() const;
    virtual void setKnobColor(const QColor & NewKnobColor);

    QString FSkinEnvironmentVariable;
    void setSkinEnvironmentVariable(const QString &NewSkinEnvironmentVariable);
    QString getSkinEnvironmentVariable() const;

    QString FRotaryKnobFileName;
    void setRotaryKnobFileName(const QString &NewRotaryKnobFileName);
    QString getRotaryKnobFileName() const;

    bool FScaleKnobImage;
    void setScaleKnobImage(bool NewScaleKnobImage);
    bool getScaleKnobImage();

    double FPosition;
    void setPosition(double NewPosition);
public slots:
    void setPosition(int_number ChannelNr, double_position NewPosition);
public:
    double getPosition();

protected:
    void paintEvent(QPaintEvent *event);
    void mouseMoveEvent(QMouseEvent *ev);
    
    double PreviousDegrees;

signals:
	void KnobMoved(int_number ChannelNr, double_position NewPosition);
};

#endif
