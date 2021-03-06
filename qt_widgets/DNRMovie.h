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

#ifndef DNRMOVIE_H
#define DNRMOVIE_H

#include "DNRDefines.h"
#include <QWidget>
#include <QtDesigner/QDesignerExportWidget>

class QDESIGNER_WIDGET_EXPORT DNRMovie : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString SkinEnvironmentVariable READ getSkinEnvironmentVariable WRITE setSkinEnvironmentVariable);
    Q_PROPERTY(QString MovieFileName READ getMovieFileName WRITE setMovieFileName);
    Q_PROPERTY(bool ScaleMovie READ getScaleMovie WRITE setScaleMovie);
public:
    DNRMovie(QWidget *parent = 0);
    QMovie *MovieQMovie;

    QString FSkinEnvironmentVariable;
    void setSkinEnvironmentVariable(const QString &NewSkinEnvironmentVariable);
    QString getSkinEnvironmentVariable() const;

    QString FMovieFileName;
    void setMovieFileName(const QString &NewMovieFileName);
    QString getMovieFileName() const;

    bool FScaleMovie;
    void setScaleMovie(bool NewScaleMovie);
    bool getScaleMovie();

public slots:
    void paint();

protected:
    void paintEvent(QPaintEvent *event);
//    void mouseMoveEvent(QMouseEvent *ev);

};

#endif
