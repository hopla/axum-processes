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

#ifndef DNRBUTTON_H
#define DNRBUTTON_H

#include "DNRDefines.h"
#include <QWidget>
#include <QtDesigner/QDesignerExportWidget>

class QDESIGNER_WIDGET_EXPORT DNRButton : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int Number READ getNumber WRITE setNumber);
    Q_PROPERTY(QString ButtonText READ getButtonText WRITE setButtonText);
    Q_PROPERTY(bool Position READ getPosition WRITE setPosition);
    Q_PROPERTY(bool State READ getState WRITE setState);
    Q_PROPERTY(QString SkinEnvironmentVariable READ getSkinEnvironmentVariable WRITE setSkinEnvironmentVariable);
    Q_PROPERTY(QString UpOffImageFileName READ getUpOffImageFileName WRITE setUpOffImageFileName);
    Q_PROPERTY(QString UpOnImageFileName READ getUpOnImageFileName WRITE setUpOnImageFileName);
    Q_PROPERTY(QString DownOffImageFileName READ getDownOffImageFileName WRITE setDownOffImageFileName);
    Q_PROPERTY(QString DownOnImageFileName READ getDownOnImageFileName WRITE setDownOnImageFileName);
    Q_PROPERTY(QColor BorderColor READ getBorderColor WRITE setBorderColor);
    Q_PROPERTY(QColor UpOffColor READ getUpOffColor WRITE setUpOffColor);
    Q_PROPERTY(QColor UpOnColor READ getUpOnColor WRITE setUpOnColor);
    Q_PROPERTY(QColor DownOffColor READ getDownOffColor WRITE setDownOffColor);
    Q_PROPERTY(QColor DownOnColor READ getDownOnColor WRITE setDownOnColor);

public:
    DNRButton(QWidget *parent = 0);

    QImage *UpOffImage;
    QImage *UpOnImage;
    QImage *DownOffImage;
    QImage *DownOnImage;

    int FNumber;
    void setNumber(int NewNumber);
    int getNumber();

    bool FPosition;
    void setPosition(bool NewPosition);
    bool getPosition();

    bool FState;
	void setState(bool NewState);
public slots:
	void setState(int_number ChannelNr, double_position NewState);
public:
    bool getState();

    QString FButtonText;
    void setButtonText(const QString &NewButtonText);
    QString getButtonText() const;

    QString FSkinEnvironmentVariable;
    void setSkinEnvironmentVariable(const QString &NewSkinEnvironmentVariable);
    QString getSkinEnvironmentVariable() const;

    QString FUpOffImageFileName;
    void setUpOffImageFileName(const QString &NewUpOffImageFileName);
    QString getUpOffImageFileName() const;

    QString FUpOnImageFileName;
    void setUpOnImageFileName(const QString &NewUpOnImageFileName);
    QString getUpOnImageFileName() const;

    QString FDownOffImageFileName;
    void setDownOffImageFileName(const QString &NewDownOffImageFileName);
    QString getDownOffImageFileName() const;

    QString FDownOnImageFileName;
    void setDownOnImageFileName(const QString &NewDownOnImageFileName);
    QString getDownOnImageFileName() const;

	QColor FBorderColor;
	const QColor & getBorderColor() const;
    virtual void setBorderColor(const QColor & NewBorderColor);

	QColor FUpOffColor;
	const QColor & getUpOffColor() const;
    virtual void setUpOffColor(const QColor & NewUpOffColor);

    QColor FUpOnColor;
	const QColor & getUpOnColor() const;
    virtual void setUpOnColor(const QColor & NewUpOnColor);

    QColor FDownOffColor;
	const QColor & getDownOffColor() const;
    virtual void setDownOffColor(const QColor & NewDownOffColor);

    QColor FDownOnColor;
	const QColor & getDownOnColor() const;
    virtual void setDownOnColor(const QColor & NewDownOnColor);

protected:
    void paintEvent(QPaintEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

signals:
	void positionChanged(int_number Number, double_position NewPosition);

};

#endif
