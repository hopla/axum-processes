//
// C++ Interface: DNRWidgets
//
// Description:
//
//
// Author: Anton Prins <a.prins@d-r.nl>, (C) 2007
//
// Copyright: See COPYING file that comes with this distribution
//
//
#include "DNRAnalogClockPlugin.h"
#include "DNRDigitalClockPlugin.h"
#include "DNRFaderPlugin.h"
#include "DNRRotaryKnobPlugin.h"
#include "DNRButtonPlugin.h"
#include "DNRPPMMeterPlugin.h"
#include "DNRVUMeterPlugin.h"
#include "DNRPhaseMeterPlugin.h"
#include "DNRImagePlugin.h"
#include "DNRIndicationPlugin.h"
#include "DNREQPanelPlugin.h"
//#include "DNREnginePlugin.h"
#include "DNRNetworkServerPlugin.h"
#include "DNRNetworkClientPlugin.h"
//#include "DNRMySQLClientPlugin.h"
//#include "DNRTerminalPlugin.h"


#include <QtDesigner/QtDesigner>
#include <QtCore/qplugin.h>

class DNRWidgets: public QObject, public QDesignerCustomWidgetCollectionInterface
{
        Q_OBJECT
        Q_INTERFACES(QDesignerCustomWidgetCollectionInterface)

public:
        DNRWidgets(QObject *parent = 0);

        virtual QList<QDesignerCustomWidgetInterface*> customWidgets() const;

private:
        QList<QDesignerCustomWidgetInterface*> widgets;
};
