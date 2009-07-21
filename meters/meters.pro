CONFIG += release
TEMPLATE = app
TARGET = axum-meters
QT += sql webkit
INCLUDEPATH = ../common/

HEADERS =	browser.h 					\
				connectionwidget.h		\
				chasewidget.h				\

SOURCES = 	main.cpp						\
				browser.cpp					\
				connectionwidget.cpp		\
				chasewidget.cpp			\

FORMS += browserwidget.ui

target.path = /usr/bin
INSTALLS += target
LIBS += -lDNRWidgets -lmbn ../common/common.a


