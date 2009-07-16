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

#ifndef DNRPPMMETER_H
#define DNRPPMMETER_H

#include "DNRDefines.h"
#include <QWidget>
#include <QtDesigner/QDesignerExportWidget>

//Values for MeterCurve
#define DIVCURVE 80.0
#define MINCURVE 0.0


class QDESIGNER_WIDGET_EXPORT DNRPPMMeter : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(double dBPosition READ getdBPosition WRITE setdBPosition);
    Q_PROPERTY(double MindBPosition READ getMindBPosition WRITE setMindBPosition);
    Q_PROPERTY(double MaxdBPosition READ getMaxdBPosition WRITE setMaxdBPosition);
    Q_PROPERTY(double ReleasePerSecond READ getReleasePerSecond WRITE setReleasePerSecond);
    Q_PROPERTY(bool DINCurve READ getDINCurve WRITE setDINCurve);
    Q_PROPERTY(bool GradientBackground READ getGradientBackground WRITE setGradientBackground);
    Q_PROPERTY(bool GradientForground READ getGradientForground WRITE setGradientForground);
    Q_PROPERTY(QColor MaxColor READ getMaxColor WRITE setMaxColor);
    Q_PROPERTY(QColor MinColor READ getMinColor WRITE setMinColor);
    Q_PROPERTY(QColor MaxBackgroundColor READ getMaxBackgroundColor WRITE setMaxBackgroundColor);
    Q_PROPERTY(QColor MinBackgroundColor READ getMinBackgroundColor WRITE setMinBackgroundColor);

    Q_PROPERTY(QString SkinEnvironmentVariable READ getSkinEnvironmentVariable WRITE setSkinEnvironmentVariable);
    Q_PROPERTY(QString PPMMeterBackgroundFileName READ getPPMMeterBackgroundFileName WRITE setPPMMeterBackgroundFileName);
public:
    DNRPPMMeter(QWidget *parent = 0);
    QImage *PPMMeterBackgroundQImage;

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

    bool FDINCurve;
    void setDINCurve(bool NewDINCurve);
    bool getDINCurve();

    bool FGradientBackground;
    void setGradientBackground(bool NewGradientBackground);
    bool getGradientBackground();

    bool FGradientForground;
    void setGradientForground(bool NewGradientForground);
    bool getGradientForground();

    QColor FMaxColor;
	const QColor & getMaxColor() const;
    virtual void setMaxColor(const QColor & NewMaxColor);

    QColor FMinColor;
	const QColor & getMinColor() const;
    virtual void setMinColor(const QColor & NewMinColor);

    QColor FMaxBackgroundColor;
	const QColor & getMaxBackgroundColor() const;
    virtual void setMaxBackgroundColor(const QColor & NewMaxBackgroundColor);

    QColor FMinBackgroundColor;
	const QColor & getMinBackgroundColor() const;
    virtual void setMinBackgroundColor(const QColor & NewMinBackgroundColor);

    QString FSkinEnvironmentVariable;
    void setSkinEnvironmentVariable(const QString &NewSkinEnvironmentVariable);
    QString getSkinEnvironmentVariable() const;

    QString FPPMMeterBackgroundFileName;
    void setPPMMeterBackgroundFileName(const QString &NewPPMMeterBackgroundFileName);
    QString getPPMMeterBackgroundFileName() const;

private:
	double LiniearMin;
	double LiniearRange;
	double dBRange;

protected:
    void paintEvent(QPaintEvent *event);
//    void mouseMoveEvent(QMouseEvent *ev);

    double previousNumberOfSeconds;
};

#endif
