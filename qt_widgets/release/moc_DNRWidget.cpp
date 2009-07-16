/****************************************************************************
** Meta object code from reading C++ file 'DNRWidget.h'
**
** Created: Mon Oct 22 15:12:42 2007
**      by: The Qt Meta Object Compiler version 59 (Qt 4.2.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../DNRWidget.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'DNRWidget.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 59
#error "This file was generated using the moc from 4.2.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

static const uint qt_meta_data_DNRWidget[] = {

 // content:
       1,       // revision
       0,       // classname
       0,    0, // classinfo
       2,   10, // methods
       0,    0, // properties
       0,    0, // enums/sets

 // slots: signature, parameters, type, tag, flags
      11,   10,   10,   10, 0x0a,
      37,   10,   10,   10, 0x0a,

       0        // eod
};

static const char qt_meta_stringdata_DNRWidget[] = {
    "DNRWidget\0\0PopupMenuConnectTrigger()\0"
    "PopupMenuDisconnectTrigger()\0"
};

const QMetaObject DNRWidget::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_DNRWidget,
      qt_meta_data_DNRWidget, 0 }
};

const QMetaObject *DNRWidget::metaObject() const
{
    return &staticMetaObject;
}

void *DNRWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_DNRWidget))
	return static_cast<void*>(const_cast<DNRWidget*>(this));
    return QWidget::qt_metacast(_clname);
}

int DNRWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: PopupMenuConnectTrigger(); break;
        case 1: PopupMenuDisconnectTrigger(); break;
        }
        _id -= 2;
    }
    return _id;
}
