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

#ifndef DNRINDICATION_H
#define DNRINDICATION_H

#include "DNRDefines.h"
#include <QWidget>
#include <QtDesigner/QDesignerExportWidget>

class QDESIGNER_WIDGET_EXPORT DNRIndication : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int Number READ getNumber WRITE setNumber);
    Q_PROPERTY(int State READ getState WRITE setState);
    Q_PROPERTY(QString SkinEnvironmentVariable READ getSkinEnvironmentVariable WRITE setSkinEnvironmentVariable);
    Q_PROPERTY(QString State1ImageFileName READ getState1ImageFileName WRITE setState1ImageFileName);
    Q_PROPERTY(QString State2ImageFileName READ getState2ImageFileName WRITE setState2ImageFileName);
    Q_PROPERTY(QString State3ImageFileName READ getState3ImageFileName WRITE setState3ImageFileName);
    Q_PROPERTY(QString State4ImageFileName READ getState4ImageFileName WRITE setState4ImageFileName);

public:
    DNRIndication(QWidget *parent = 0);

    QImage *State1QImage;
    QImage *State2QImage;
    QImage *State3QImage;
    QImage *State4QImage;

    int FNumber;
    void setNumber(int NewNumber);
    int getNumber();

    int FState;
    void setState(int NewState);
public slots:
    void setState(int_number Number, double_position NewState);
public:
    int getState();

    QString FSkinEnvironmentVariable;
    void setSkinEnvironmentVariable(const QString &NewSkinEnvironmentVariable);
    QString getSkinEnvironmentVariable() const;

    QString FState1ImageFileName;
    void setState1ImageFileName(const QString &NewState1ImageFileName);
    QString getState1ImageFileName() const;

    QString FState2ImageFileName;
    void setState2ImageFileName(const QString &NewState2ImageFileName);
    QString getState2ImageFileName() const;

    QString FState3ImageFileName;
    void setState3ImageFileName(const QString &NewState3ImageFileName);
    QString getState3ImageFileName() const;

    QString FState4ImageFileName;
    void setState4ImageFileName(const QString &NewState4ImageFileName);
    QString getState4ImageFileName() const;

protected:
    void paintEvent(QPaintEvent *event);

signals:

};

#endif
