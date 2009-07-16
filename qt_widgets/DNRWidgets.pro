CONFIG += qt release plugin designer
TEMPLATE = lib
QT += network sql

HEADERS = DNRAnalogClock.h \
  DNRAnalogClockPlugin.h \
  DNRDigitalClock.h \
  DNRDigitalClockPlugin.h \
  DNRFader.h \
  DNRFaderPlugin.h \
  DNRRotaryKnob.h \
  DNRRotaryKnobPlugin.h \
  DNRButton.h \
  DNRButtonPlugin.h \
  DNRPPMMeter.h \
  DNRPPMMeterPlugin.h\
  DNRVUMeter.h \
  DNRVUMeterPlugin.h \
  DNRPhaseMeter.h \
  DNRPhaseMeterPlugin.h \
  DNRImage.h \
  DNRImagePlugin.h \
  DNRIndication.h \
  DNRIndicationPlugin.h \
  DNRNetworkServer.h \
  DNRNetworkServerPlugin.h \
  DNRNetworkClient.h \
  DNRNetworkClientPlugin.h \
  DNRWidgets.h \
  DNRDefines.h 

SOURCES = DNRAnalogClock.cpp \
  DNRAnalogClockPlugin.cpp \
  DNRDigitalClock.cpp \
  DNRDigitalClockPlugin.cpp \
  DNRFader.cpp \
  DNRFaderPlugin.cpp \
  DNRRotaryKnob.cpp \
  DNRRotaryKnobPlugin.cpp \
  DNRButton.cpp \
  DNRButtonPlugin.cpp \
  DNRPPMMeter.cpp \
  DNRPPMMeterPlugin.cpp \
  DNRVUMeter.cpp \
  DNRVUMeterPlugin.cpp \
  DNRPhaseMeter.cpp \
  DNRPhaseMeterPlugin.cpp \
  DNRImage.cpp \
  DNRImagePlugin.cpp \
  DNRIndication.cpp \
  DNRIndicationPlugin.cpp \
  DNRNetworkServer.cpp \
  DNRNetworkServerPlugin.cpp \
  DNRNetworkClient.cpp \
  DNRNetworkClientPlugin.cpp \
  DNRWidgets.cpp \
  DNRDefines.cpp 

headers.path = /usr/include
headers.files = $$HEADERS
target.path = /usr/lib
INSTALLS += headers target
