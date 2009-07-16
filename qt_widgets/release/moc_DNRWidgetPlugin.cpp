/****************************************************************************
** Meta object code from reading C++ file 'DNRWidgetPlugin.h'
**
** Created: Mon Oct 22 15:12:45 2007
**      by: The Qt Meta Object Compiler version 59 (Qt 4.2.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../DNRWidgetPlugin.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'DNRWidgetPlugin.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 59
#error "This file was generated using the moc from 4.2.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

static const uint qt_meta_data_WidgetPlugin[] = {

 // content:
       1,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       0,    0, // properties
       0,    0, // enums/sets

       0        // eod
};

static const char qt_meta_stringdata_WidgetPlugin[] = {
    "WidgetPlugin\0"
};

const QMetaObject WidgetPlugin::staticMetaObject = {
    { &QObject::staticMetaObject, qt_meta_stringdata_WidgetPlugin,
      qt_meta_data_WidgetPlugin, 0 }
};

const QMetaObject *WidgetPlugin::metaObject() const
{
    return &staticMetaObject;
}

void *WidgetPlugin::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_WidgetPlugin))
	return static_cast<void*>(const_cast<WidgetPlugin*>(this));
    if (!strcmp(_clname, "QDesignerCustomWidgetInterface"))
	return static_cast<QDesignerCustomWidgetInterface*>(const_cast<WidgetPlugin*>(this));
    if (!strcmp(_clname, "com.trolltech.Qt.Designer.CustomWidget"))
	return static_cast<QDesignerCustomWidgetInterface*>(const_cast<WidgetPlugin*>(this));
    return QObject::qt_metacast(_clname);
}

int WidgetPlugin::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    return _id;
}
