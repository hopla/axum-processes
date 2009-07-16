/****************************************************************************
** Meta object code from reading C++ file 'DNRDigitalClockPlugin.h'
**
** Created: Tue Apr 28 10:47:29 2009
**      by: The Qt Meta Object Compiler version 61 (Qt 4.5.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../DNRDigitalClockPlugin.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'DNRDigitalClockPlugin.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 61
#error "This file was generated using the moc from 4.5.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_DigitalClockPlugin[] = {

 // content:
       2,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors

       0        // eod
};

static const char qt_meta_stringdata_DigitalClockPlugin[] = {
    "DigitalClockPlugin\0"
};

const QMetaObject DigitalClockPlugin::staticMetaObject = {
    { &QObject::staticMetaObject, qt_meta_stringdata_DigitalClockPlugin,
      qt_meta_data_DigitalClockPlugin, 0 }
};

const QMetaObject *DigitalClockPlugin::metaObject() const
{
    return &staticMetaObject;
}

void *DigitalClockPlugin::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_DigitalClockPlugin))
        return static_cast<void*>(const_cast< DigitalClockPlugin*>(this));
    if (!strcmp(_clname, "QDesignerCustomWidgetInterface"))
        return static_cast< QDesignerCustomWidgetInterface*>(const_cast< DigitalClockPlugin*>(this));
    if (!strcmp(_clname, "com.trolltech.Qt.Designer.CustomWidget"))
        return static_cast< QDesignerCustomWidgetInterface*>(const_cast< DigitalClockPlugin*>(this));
    return QObject::qt_metacast(_clname);
}

int DigitalClockPlugin::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    return _id;
}
QT_END_MOC_NAMESPACE
