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

#ifndef DNRPHASEMETER_H
#define DNRPHASEMETER_H

#include "DNRDefines.h"
#include <QWidget>
#include <QtDesigner/QDesignerExportWidget>

class QDESIGNER_WIDGET_EXPORT DNRPhaseMeter : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(double Position READ getPosition WRITE setPosition);
    Q_PROPERTY(double MinPosition READ getMinPosition WRITE setMinPosition);
    Q_PROPERTY(double MaxPosition READ getMaxPosition WRITE setMaxPosition);
    Q_PROPERTY(int PointerWidth READ getPointerWidth WRITE setPointerWidth);
    Q_PROPERTY(QColor PointerMinColor READ getPointerMinColor WRITE setPointerMinColor);
    Q_PROPERTY(QColor PointerMaxColor READ getPointerMaxColor WRITE setPointerMaxColor);
    Q_PROPERTY(QColor BackgroundMonoColor READ getBackgroundMonoColor WRITE setBackgroundMonoColor);
    Q_PROPERTY(QColor BackgroundOutOfPhaseColor READ getBackgroundOutOfPhaseColor WRITE setBackgroundOutOfPhaseColor);
    Q_PROPERTY(bool GradientBackground READ getGradientBackground WRITE setGradientBackground);
    Q_PROPERTY(bool Horizontal READ getHorizontal WRITE setHorizontal);
    Q_PROPERTY(QString SkinEnvironmentVariable READ getSkinEnvironmentVariable WRITE setSkinEnvironmentVariable);
    Q_PROPERTY(QString PhaseMeterBackgroundFileName READ getPhaseMeterBackgroundFileName WRITE setPhaseMeterBackgroundFileName);
public:
    DNRPhaseMeter(QWidget *parent = 0);
    QImage *PhaseMeterBackgroundQImage;

    double FPosition;
public slots:
	void setPosition(double_phase NewPosition);
public:
    double getPosition();

    double FMinPosition;
    void setMinPosition(double NewMinPosition);
    double getMinPosition();

    double FMaxPosition;
    void setMaxPosition(double NewMaxPosition);
    double getMaxPosition();

    int FPointerWidth;
    void setPointerWidth(int NewPointerWidth);
    int getPointerWidth();

    QColor FPointerMinColor;
	const QColor & getPointerMinColor() const;
    virtual void setPointerMinColor(const QColor & NewPointerMinColor);

    QColor FPointerMaxColor;
	const QColor & getPointerMaxColor() const;
    virtual void setPointerMaxColor(const QColor & NewPointerMaxColor);

    QColor FBackgroundMonoColor;
	const QColor & getBackgroundMonoColor() const;
    virtual void setBackgroundMonoColor(const QColor & NewBackgroundMonoColor);

    QColor FBackgroundOutOfPhaseColor;
	const QColor & getBackgroundOutOfPhaseColor() const;
    virtual void setBackgroundOutOfPhaseColor(const QColor & NewBackgroundOutOfPhaseColor);

    bool FGradientBackground;
    void setGradientBackground(bool NewGradientBackground);
    bool getGradientBackground();

    bool FHorizontal;
    void setHorizontal(bool NewHorizontal);
    bool getHorizontal();

    QString FSkinEnvironmentVariable;
    void setSkinEnvironmentVariable(const QString &NewSkinEnvironmentVariable);
    QString getSkinEnvironmentVariable() const;

    QString FPhaseMeterBackgroundFileName;
    void setPhaseMeterBackgroundFileName(const QString &NewPhaseMeterBackgroundFileName);
    QString getPhaseMeterBackgroundFileName() const;

protected:
    void paintEvent(QPaintEvent *event);
//    void mouseMoveEvent(QMouseEvent *ev);

};

#endif
