/****************************************************************************
** Meta object code from reading C++ file 'DNRPhaseMeterPlugin.h'
**
** Created: Thu Jul 16 10:30:16 2009
**      by: The Qt Meta Object Compiler version 61 (Qt 4.5.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "DNRPhaseMeterPlugin.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'DNRPhaseMeterPlugin.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 61
#error "This file was generated using the moc from 4.5.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_PhaseMeterPlugin[] = {

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

static const char qt_meta_stringdata_PhaseMeterPlugin[] = {
    "PhaseMeterPlugin\0"
};

const QMetaObject PhaseMeterPlugin::staticMetaObject = {
    { &QObject::staticMetaObject, qt_meta_stringdata_PhaseMeterPlugin,
      qt_meta_data_PhaseMeterPlugin, 0 }
};

const QMetaObject *PhaseMeterPlugin::metaObject() const
{
    return &staticMetaObject;
}

void *PhaseMeterPlugin::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_PhaseMeterPlugin))
        return static_cast<void*>(const_cast< PhaseMeterPlugin*>(this));
    if (!strcmp(_clname, "QDesignerCustomWidgetInterface"))
        return static_cast< QDesignerCustomWidgetInterface*>(const_cast< PhaseMeterPlugin*>(this));
    if (!strcmp(_clname, "com.trolltech.Qt.Designer.CustomWidget"))
        return static_cast< QDesignerCustomWidgetInterface*>(const_cast< PhaseMeterPlugin*>(this));
    return QObject::qt_metacast(_clname);
}

int PhaseMeterPlugin::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    return _id;
}
QT_END_MOC_NAMESPACE
