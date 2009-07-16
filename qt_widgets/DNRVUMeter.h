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

#ifndef DNRVUMETER_H
#define DNRVUMETER_H

#include "DNRDefines.h"
#include <QWidget>
#include <QtDesigner/QDesignerExportWidget>

#define DIVCURVE_POSITIVE	63.0
#define MINCURVE_POSITIVE	0.0

#define DIVCURVE_NEGATIVE	50.0
#define MINCURVE_NEGATIVE	-2.0


class QDESIGNER_WIDGET_EXPORT DNRVUMeter : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(double dBPosition READ getdBPosition WRITE setdBPosition);
    Q_PROPERTY(double MindBPosition READ getMindBPosition WRITE setMindBPosition);
    Q_PROPERTY(double MaxdBPosition READ getMaxdBPosition WRITE setMaxdBPosition);
    Q_PROPERTY(double ReleasePerSecond READ getReleasePerSecond WRITE setReleasePerSecond);
    Q_PROPERTY(bool VUCurve READ getVUCurve WRITE setVUCurve);
    Q_PROPERTY(int PointerLength READ getPointerLength WRITE setPointerLength);
    Q_PROPERTY(int PointerStartY READ getPointerStartY WRITE setPointerStartY);
    Q_PROPERTY(int PointerWidth READ getPointerWidth WRITE setPointerWidth);
    Q_PROPERTY(QColor PointerColor READ getPointerColor WRITE setPointerColor);
    Q_PROPERTY(QString SkinEnvironmentVariable READ getSkinEnvironmentVariable WRITE setSkinEnvironmentVariable);
    Q_PROPERTY(QString VUMeterBackgroundFileName READ getVUMeterBackgroundFileName WRITE setVUMeterBackgroundFileName);
public:
    DNRVUMeter(QWidget *parent = 0);
    QImage *VUMeterBackgroundQImage;

    double FdBPosition;
public slots:
	void setdBPosition(double_db NewdBPosition);
public:
    double getdBPosition();

    double FMindBPosition;
    void setMindBPosition(double NewMindBPosition);
    double getMindBPosition();

    double FMaxdBPosition;
    void setMaxdBPosition(double NewMaxdBPosition);
    double getMaxdBPosition();

    double FReleasePerSecond;
    void setReleasePerSecond(double NewReleasePerSecond);
    double getReleasePerSecond();

	bool FVUCurve;
    void setVUCurve(bool NewVUCurve);
    bool getVUCurve();

    int FPointerLength;
    void setPointerLength(int NewPointerLength);
    int getPointerLength();

    int FPointerStartY;
    void setPointerStartY(int NewPointerStartY);
    int getPointerStartY();

    int FPointerWidth;
    void setPointerWidth(int NewPointerWidth);
    int getPointerWidth();

    QColor FPointerColor;
	const QColor & getPointerColor() const;
    virtual void setPointerColor(const QColor & NewPointerColor);

    QString FSkinEnvironmentVariable;
    void setSkinEnvironmentVariable(const QString &NewSkinEnvironmentVariable);
    QString getSkinEnvironmentVariable() const;

    QString FVUMeterBackgroundFileName;
    void setVUMeterBackgroundFileName(const QString &NewVUMeterBackgroundFileName);
    QString getVUMeterBackgroundFileName() const;

protected:
    void paintEvent(QPaintEvent *event);
//    void mouseMoveEvent(QMouseEvent *ev);

    double previousNumberOfSeconds;

private:
	double LiniearMinPositive;
  	double LiniearRangePositive;
	double LiniearMinNegative;
  	double LiniearRangeNegative;
	double dBRange;

public slots:
	void doRelease(char_none unused);
};

#endif
