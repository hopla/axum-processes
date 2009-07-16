CONFIG += release
TEMPLATE = app
TARGET = axum-meters
QT += sql webkit

HEADERS =	browser.h 					\
				connectionwidget.h		\
				mambanet_network.h		\
				ThreadListener.h			\
				chasewidget.h				\
				mambanet_stack_axum.h

SOURCES = 	main.cpp						\
				browser.cpp					\
				connectionwidget.cpp		\
				mambanet_network.cpp		\
				ThreadListener.cpp		\
				chasewidget.cpp			\
				mambanet_stack_axum.cpp

FORMS += browserwidget.ui

target.path = /usr/bin
INSTALLS += target
LIBS += -lDNRWidgets


