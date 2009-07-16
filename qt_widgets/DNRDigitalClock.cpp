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

#include <QtGui>

#include "DNRDigitalClock.h"

DNRDigitalClock::DNRDigitalClock(QWidget *parent)
    : QWidget(parent)
{
    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(update()));
    timer->start(1000);

    setWindowTitle(tr("Digital Clock"));
    resize(100, 30);
}

void DNRDigitalClock::paintEvent(QPaintEvent *)
{
    QColor TextColor(0, 0, 0);

    int side = qMin(width(), height());

    QTime Time = QTime::currentTime();
    QString TimeString = Time.toString("hh:mm:ss");

    QDate Date = QDate::currentDate();
    QString DateString = Date.toString();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.setPen(TextColor);
    painter.setBrush(Qt::NoBrush);

    int HalfHeight = height()/2;
    painter.drawText( 0, 0, width(), HalfHeight, Qt::AlignCenter, TimeString);
    painter.drawText( 0, HalfHeight, width(), HalfHeight, Qt::AlignCenter, DateString);

}
