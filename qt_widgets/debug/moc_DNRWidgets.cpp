/****************************************************************************
** Meta object code from reading C++ file 'DNRWidgets.h'
**
** Created: Tue Apr 28 10:56:30 2009
**      by: The Qt Meta Object Compiler version 61 (Qt 4.5.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../DNRWidgets.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'DNRWidgets.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 61
#error "This file was generated using the moc from 4.5.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_DNRWidgets[] = {

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

static const char qt_meta_stringdata_DNRWidgets[] = {
    "DNRWidgets\0"
};

const QMetaObject DNRWidgets::staticMetaObject = {
    { &QObject::staticMetaObject, qt_meta_stringdata_DNRWidgets,
      qt_meta_data_DNRWidgets, 0 }
};

const QMetaObject *DNRWidgets::metaObject() const
{
    return &staticMetaObject;
}

void *DNRWidgets::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_DNRWidgets))
        return static_cast<void*>(const_cast< DNRWidgets*>(this));
    if (!strcmp(_clname, "QDesignerCustomWidgetCollectionInterface"))
        return static_cast< QDesignerCustomWidgetCollectionInterface*>(const_cast< DNRWidgets*>(this));
    if (!strcmp(_clname, "com.trolltech.Qt.Designer.CustomWidgetCollection"))
        return static_cast< QDesignerCustomWidgetCollectionInterface*>(const_cast< DNRWidgets*>(this));
    return QObject::qt_metacast(_clname);
}

int DNRWidgets::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    return _id;
}
QT_END_MOC_NAMESPACE
