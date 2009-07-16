/********************************************************************************
** Form generated from reading ui file 'browserwidget.ui'
**
** Created: Thu Mar 26 17:59:09 2009
**      by: Qt User Interface Compiler version 4.4.3
**
** WARNING! All changes made in this file will be lost when recompiling ui file!
********************************************************************************/

#ifndef UI_BROWSERWIDGET_H
#define UI_BROWSERWIDGET_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QLabel>
#include <QtGui/QTabWidget>
#include <QtGui/QWidget>
#include <QtWebKit/QWebView>
#include "DNRAnalogClock.h"
#include "DNRDigitalClock.h"
#include "DNRImage.h"
#include "DNRPPMMeter.h"

QT_BEGIN_NAMESPACE

class Ui_Browser
{
public:
    QAction *insertRowAction;
    QAction *deleteRowAction;
    QLabel *label;
    QLabel *label_2;
    QTabWidget *tabWidget;
    QWidget *tab;
    DNRDigitalClock *NewDNRDigitalClock;
    DNRPPMMeter *NewDNRPPMMeter_3;
    DNRPPMMeter *NewDNRPPMMeter_4;
    DNRAnalogClock *NewDNRAnalogClock;
    QLabel *label_4;
    QLabel *label_3;
    DNRPPMMeter *NewDNRPPMMeter;
    DNRImage *NewDNRImage;
    DNRPPMMeter *NewDNRPPMMeter_2;
    QWidget *tab_2;
    QWebView *webView;
    QLabel *label_7;

    void setupUi(QWidget *Browser)
    {
    if (Browser->objectName().isEmpty())
        Browser->setObjectName(QString::fromUtf8("Browser"));
    Browser->resize(1024, 768);
    QSizePolicy sizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);
    sizePolicy.setHeightForWidth(Browser->sizePolicy().hasHeightForWidth());
    Browser->setSizePolicy(sizePolicy);
    QPalette palette;
    QBrush brush(QColor(255, 255, 255, 255));
    brush.setStyle(Qt::SolidPattern);
    palette.setBrush(QPalette::Active, QPalette::Base, brush);
    QBrush brush1(QColor(0, 0, 0, 255));
    brush1.setStyle(Qt::SolidPattern);
    palette.setBrush(QPalette::Active, QPalette::Window, brush1);
    palette.setBrush(QPalette::Active, QPalette::AlternateBase, brush1);
    palette.setBrush(QPalette::Inactive, QPalette::Base, brush);
    palette.setBrush(QPalette::Inactive, QPalette::Window, brush1);
    palette.setBrush(QPalette::Inactive, QPalette::AlternateBase, brush1);
    QBrush brush2(QColor(234, 234, 234, 255));
    brush2.setStyle(Qt::SolidPattern);
    palette.setBrush(QPalette::Disabled, QPalette::Base, brush2);
    palette.setBrush(QPalette::Disabled, QPalette::Window, brush1);
    palette.setBrush(QPalette::Disabled, QPalette::AlternateBase, brush1);
    Browser->setPalette(palette);
    QIcon icon;
    icon.addPixmap(QPixmap(QString::fromUtf8("Skins/Axum.png")), QIcon::Normal, QIcon::Off);
    Browser->setWindowIcon(icon);
    Browser->setAutoFillBackground(false);
    Browser->setStyleSheet(QString::fromUtf8("QTabWidget\n"
"{\n"
"	background: rgb(0,0,0);\n"
"}\n"
"QTabWidget::pane\n"
"{ \n"
"	border-top: -2px solid #00FF00;\n"
"}\n"
"\n"
"QTabWidget::tab-bar \n"
"{         \n"
"	left: 15px;\n"
"}\n"
"\n"
""));
    insertRowAction = new QAction(Browser);
    insertRowAction->setObjectName(QString::fromUtf8("insertRowAction"));
    insertRowAction->setEnabled(false);
    deleteRowAction = new QAction(Browser);
    deleteRowAction->setObjectName(QString::fromUtf8("deleteRowAction"));
    deleteRowAction->setEnabled(false);
    label = new QLabel(Browser);
    label->setObjectName(QString::fromUtf8("label"));
    label->setGeometry(QRect(0, 0, 1024, 94));
    QFont font;
    font.setFamily(QString::fromUtf8("Arial Unicode MS"));
    font.setPointSize(48);
    font.setBold(true);
    font.setWeight(75);
    label->setFont(font);
    label->setMouseTracking(false);
    label->setAcceptDrops(false);
    label->setStyleSheet(QString::fromUtf8("QLabel\n"
"{\n"
"	color: rgba(200, 200, 200, 255);\n"
"	background: rgba(0, 0, 0, 255);\n"
"}"));
    label->setTextFormat(Qt::AutoText);
    label->setAlignment(Qt::AlignCenter);
    label_2 = new QLabel(Browser);
    label_2->setObjectName(QString::fromUtf8("label_2"));
    label_2->setGeometry(QRect(0, 674, 1024, 94));
    QFont font1;
    font1.setFamily(QString::fromUtf8("Arial Unicode MS"));
    font1.setPointSize(12);
    label_2->setFont(font1);
    label_2->setMouseTracking(false);
    label_2->setAcceptDrops(false);
    label_2->setStyleSheet(QString::fromUtf8("QLabel\n"
"{\n"
"	color: rgba(200, 200, 200, 255);\n"
"	background: rgba(0, 0, 0, 255);\n"
"}"));
    label_2->setTextFormat(Qt::PlainText);
    label_2->setAlignment(Qt::AlignCenter);
    tabWidget = new QTabWidget(Browser);
    tabWidget->setObjectName(QString::fromUtf8("tabWidget"));
    tabWidget->setGeometry(QRect(0, 73, 1024, 601));
    tabWidget->setAcceptDrops(false);
    tabWidget->setMouseTracking(false);
    tabWidget->setStyleSheet(QString::fromUtf8(""));
    tabWidget->setTabPosition(QTabWidget::North);
    tabWidget->setTabShape(QTabWidget::Rounded);
    tab = new QWidget();
    tab->setObjectName(QString::fromUtf8("tab"));
    NewDNRDigitalClock = new DNRDigitalClock(tab);
    NewDNRDigitalClock->setObjectName(QString::fromUtf8("NewDNRDigitalClock"));
    NewDNRDigitalClock->setGeometry(QRect(569, 325, 211, 41));
    QFont font2;
    font2.setFamily(QString::fromUtf8("Verdana"));
    font2.setPointSize(12);
    font2.setBold(true);
    font2.setWeight(75);
    NewDNRDigitalClock->setFont(font2);
    NewDNRPPMMeter_3 = new DNRPPMMeter(tab);
    NewDNRPPMMeter_3->setObjectName(QString::fromUtf8("NewDNRPPMMeter_3"));
    NewDNRPPMMeter_3->setGeometry(QRect(216, 44, 10, 448));
    NewDNRPPMMeter_3->setGradientForground(true);
    NewDNRPPMMeter_4 = new DNRPPMMeter(tab);
    NewDNRPPMMeter_4->setObjectName(QString::fromUtf8("NewDNRPPMMeter_4"));
    NewDNRPPMMeter_4->setGeometry(QRect(298, 44, 10, 448));
    NewDNRPPMMeter_4->setGradientForground(true);
    NewDNRAnalogClock = new DNRAnalogClock(tab);
    NewDNRAnalogClock->setObjectName(QString::fromUtf8("NewDNRAnalogClock"));
    NewDNRAnalogClock->setGeometry(QRect(469, 82, 410, 410));
    NewDNRAnalogClock->setHourLines(false);
    NewDNRAnalogClock->setMinuteLines(false);
    label_4 = new QLabel(tab);
    label_4->setObjectName(QString::fromUtf8("label_4"));
    label_4->setGeometry(QRect(205, 506, 114, 30));
    QFont font3;
    font3.setFamily(QString::fromUtf8("Arial Unicode MS"));
    font3.setPointSize(14);
    font3.setBold(true);
    font3.setWeight(75);
    label_4->setFont(font3);
    label_4->setAcceptDrops(false);
    label_4->setStyleSheet(QString::fromUtf8("QLabel\n"
"{\n"
"	color: rgba(208, 208, 0, 255);\n"
"	background: rgba(0, 0, 0, 255);\n"
"}"));
    label_4->setTextFormat(Qt::PlainText);
    label_4->setAlignment(Qt::AlignCenter);
    label_3 = new QLabel(tab);
    label_3->setObjectName(QString::fromUtf8("label_3"));
    label_3->setGeometry(QRect(26, 506, 114, 30));
    label_3->setFont(font3);
    label_3->setAcceptDrops(false);
    label_3->setStyleSheet(QString::fromUtf8("QLabel\n"
"{\n"
"	color: rgba(208, 208, 0, 255);\n"
"	background: rgba(0, 0, 0, 255);\n"
"}"));
    label_3->setTextFormat(Qt::PlainText);
    label_3->setAlignment(Qt::AlignCenter);
    NewDNRPPMMeter = new DNRPPMMeter(tab);
    NewDNRPPMMeter->setObjectName(QString::fromUtf8("NewDNRPPMMeter"));
    NewDNRPPMMeter->setGeometry(QRect(36, 44, 10, 448));
    NewDNRPPMMeter->setGradientForground(true);
    NewDNRImage = new DNRImage(tab);
    NewDNRImage->setObjectName(QString::fromUtf8("NewDNRImage"));
    NewDNRImage->setGeometry(QRect(0, 0, 1024, 580));
    QSizePolicy sizePolicy1(QSizePolicy::Preferred, QSizePolicy::Preferred);
    sizePolicy1.setHorizontalStretch(0);
    sizePolicy1.setVerticalStretch(0);
    sizePolicy1.setHeightForWidth(NewDNRImage->sizePolicy().hasHeightForWidth());
    NewDNRImage->setSizePolicy(sizePolicy1);
    NewDNRImage->setScaleImage(false);
    NewDNRPPMMeter_2 = new DNRPPMMeter(tab);
    NewDNRPPMMeter_2->setObjectName(QString::fromUtf8("NewDNRPPMMeter_2"));
    NewDNRPPMMeter_2->setGeometry(QRect(119, 44, 10, 448));
    NewDNRPPMMeter_2->setProperty("dBPosition", QVariant(0));
    NewDNRPPMMeter_2->setGradientForground(true);
    tabWidget->addTab(tab, QString());
    NewDNRImage->raise();
    NewDNRDigitalClock->raise();
    NewDNRPPMMeter_3->raise();
    NewDNRPPMMeter_4->raise();
    NewDNRAnalogClock->raise();
    label_4->raise();
    label_3->raise();
    NewDNRPPMMeter->raise();
    NewDNRPPMMeter_2->raise();
    tab_2 = new QWidget();
    tab_2->setObjectName(QString::fromUtf8("tab_2"));
    webView = new QWebView(tab_2);
    webView->setObjectName(QString::fromUtf8("webView"));
    webView->setMouseTracking(false);
    webView->setAcceptDrops(false);
    webView->setGeometry(QRect(0, 37, 1024, 545));
    webView->setUrl(QUrl("http://Service:Service@192.168.0.200/new/skin_table.html?file=main.php"));
    label_7 = new QLabel(tab_2);
    label_7->setObjectName(QString::fromUtf8("label_7"));
    label_7->setGeometry(QRect(0, 0, 1024, 37));
    label_7->setFont(font);
    label_7->setAcceptDrops(false);
    label_7->setStyleSheet(QString::fromUtf8("QLabel\n"
"{\n"
"	color: rgba(120, 120, 120, 255);\n"
"	background: rgba(120, 120, 120, 255);\n"
"}"));
    label_7->setTextFormat(Qt::LogText);
    label_7->setAlignment(Qt::AlignCenter);
    tabWidget->addTab(tab_2, QString());

    retranslateUi(Browser);

    tabWidget->setCurrentIndex(1);


    QMetaObject::connectSlotsByName(Browser);
    } // setupUi

    void retranslateUi(QWidget *Browser)
    {
    Browser->setWindowTitle(QApplication::translate("Browser", "Axum", 0, QApplication::UnicodeUTF8));
    insertRowAction->setText(QApplication::translate("Browser", "&Insert Row", 0, QApplication::UnicodeUTF8));

#ifndef QT_NO_STATUSTIP
    insertRowAction->setStatusTip(QApplication::translate("Browser", "Inserts a new Row", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_STATUSTIP

    deleteRowAction->setText(QApplication::translate("Browser", "&Delete Row", 0, QApplication::UnicodeUTF8));

#ifndef QT_NO_STATUSTIP
    deleteRowAction->setStatusTip(QApplication::translate("Browser", "Deletes the current Row", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_STATUSTIP

    label->setText(QApplication::translate("Browser", "A\342\210\231X\342\210\231U\342\210\231M", 0, QApplication::UnicodeUTF8));
    label_2->setText(QApplication::translate("Browser", "(C) Copyright 2009 - D&R Electronica Weesp B.V.", 0, QApplication::UnicodeUTF8));

#ifndef QT_NO_TOOLTIP
    NewDNRDigitalClock->setToolTip(QApplication::translate("Browser", "The current time", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_TOOLTIP


#ifndef QT_NO_WHATSTHIS
    NewDNRDigitalClock->setWhatsThis(QApplication::translate("Browser", "The digital clock widget displays the current time.", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_WHATSTHIS


#ifndef QT_NO_TOOLTIP
    NewDNRPPMMeter_3->setToolTip(QApplication::translate("Browser", "The PPM Meter", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_TOOLTIP


#ifndef QT_NO_WHATSTHIS
    NewDNRPPMMeter_3->setWhatsThis(QApplication::translate("Browser", "The PPM Meter widget displays ", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_WHATSTHIS


#ifndef QT_NO_TOOLTIP
    NewDNRPPMMeter_4->setToolTip(QApplication::translate("Browser", "The PPM Meter", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_TOOLTIP


#ifndef QT_NO_WHATSTHIS
    NewDNRPPMMeter_4->setWhatsThis(QApplication::translate("Browser", "The PPM Meter widget displays ", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_WHATSTHIS


#ifndef QT_NO_TOOLTIP
    NewDNRAnalogClock->setToolTip(QApplication::translate("Browser", "The current time", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_TOOLTIP


#ifndef QT_NO_WHATSTHIS
    NewDNRAnalogClock->setWhatsThis(QApplication::translate("Browser", "The analog clock widget displays the current time.", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_WHATSTHIS

    label_4->setText(QApplication::translate("Browser", "Std 1", 0, QApplication::UnicodeUTF8));
    label_3->setText(QApplication::translate("Browser", "CRM", 0, QApplication::UnicodeUTF8));

#ifndef QT_NO_TOOLTIP
    NewDNRPPMMeter->setToolTip(QApplication::translate("Browser", "The PPM Meter", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_TOOLTIP


#ifndef QT_NO_WHATSTHIS
    NewDNRPPMMeter->setWhatsThis(QApplication::translate("Browser", "The PPM Meter widget displays ", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_WHATSTHIS


#ifndef QT_NO_TOOLTIP
    NewDNRImage->setToolTip(QApplication::translate("Browser", "The Image", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_TOOLTIP


#ifndef QT_NO_WHATSTHIS
    NewDNRImage->setWhatsThis(QApplication::translate("Browser", "The Image widget displays ", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_WHATSTHIS

    NewDNRImage->setSkinEnvironmentVariable(QApplication::translate("Browser", "SkinPath", 0, QApplication::UnicodeUTF8));
    NewDNRImage->setImageFileName(QApplication::translate("Browser", "MeterBackground.png", 0, QApplication::UnicodeUTF8));

#ifndef QT_NO_TOOLTIP
    NewDNRPPMMeter_2->setToolTip(QApplication::translate("Browser", "The PPM Meter", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_TOOLTIP


#ifndef QT_NO_WHATSTHIS
    NewDNRPPMMeter_2->setWhatsThis(QApplication::translate("Browser", "The PPM Meter widget displays ", 0, QApplication::UnicodeUTF8));
#endif // QT_NO_WHATSTHIS

    tabWidget->setTabText(tabWidget->indexOf(tab), QApplication::translate("Browser", "Meters", 0, QApplication::UnicodeUTF8));
    label_7->setText(QString());
    tabWidget->setTabText(tabWidget->indexOf(tab_2), QApplication::translate("Browser", "Webview", 0, QApplication::UnicodeUTF8));
    Q_UNUSED(Browser);
    } // retranslateUi

};

namespace Ui {
    class Browser: public Ui_Browser {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_BROWSERWIDGET_H
