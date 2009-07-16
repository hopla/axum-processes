//
// C++ Implementation: DNRWidgets
//
// Description:
//
//
// Author: Anton Prins <a.prins@d-r.nl>, (C) 2007
//
// Copyright: See COPYING file that comes with this distribution
//
//
#include "DNRWidgets.h"

DNRWidgets::DNRWidgets(QObject *parent)
   : QObject(parent)
{
        widgets.append(new AnalogClockPlugin(this));
        widgets.append(new DigitalClockPlugin(this));
        widgets.append(new FaderPlugin(this));
        widgets.append(new RotaryKnobPlugin(this));
        widgets.append(new ButtonPlugin(this));
        widgets.append(new PPMMeterPlugin(this));
        widgets.append(new VUMeterPlugin(this));
        widgets.append(new PhaseMeterPlugin(this));
        widgets.append(new ImagePlugin(this));
        widgets.append(new IndicationPlugin(this));
//        widgets.append(new EnginePlugin(this));
        widgets.append(new NetworkServerPlugin(this));
        widgets.append(new NetworkClientPlugin(this));
//        widgets.append(new MySQLClientPlugin(this));
//        widgets.append(new TerminalPlugin(this));
}

QList<QDesignerCustomWidgetInterface*> DNRWidgets::customWidgets() const
{
        return widgets;
}

Q_EXPORT_PLUGIN(DNRWidgets)
