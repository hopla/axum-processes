/****************************************************************************
** Meta object code from reading C++ file 'DNREngine.h'
**
** Created: Mon Oct 22 15:13:42 2007
**      by: The Qt Meta Object Compiler version 59 (Qt 4.2.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../DNREngine.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'DNREngine.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 59
#error "This file was generated using the moc from 4.2.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

static const uint qt_meta_data_DNREngine[] = {

 // content:
       1,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       1,   10, // properties
       0,    0, // enums/sets

 // properties: name, type, flags
      14,   10, 0x02095103,

       0        // eod
};

static const char qt_meta_stringdata_DNREngine[] = {
    "DNREngine\0int\0NumberOfChannels\0"
};

const QMetaObject DNREngine::staticMetaObject = {
    { &DNRWidget::staticMetaObject, qt_meta_stringdata_DNREngine,
      qt_meta_data_DNREngine, 0 }
};

const QMetaObject *DNREngine::metaObject() const
{
    return &staticMetaObject;
}

void *DNREngine::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_DNREngine))
	return static_cast<void*>(const_cast<DNREngine*>(this));
    return DNRWidget::qt_metacast(_clname);
}

int DNREngine::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = DNRWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    
#ifndef QT_NO_PROPERTIES
     if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< int*>(_v) = getNumberOfChannels(); break;
        }
        _id -= 1;
    } else if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: setNumberOfChannels(*reinterpret_cast< int*>(_v)); break;
        }
        _id -= 1;
    } else if (_c == QMetaObject::ResetProperty) {
        _id -= 1;
    } else if (_c == QMetaObject::QueryPropertyDesignable) {
        _id -= 1;
    } else if (_c == QMetaObject::QueryPropertyScriptable) {
        _id -= 1;
    } else if (_c == QMetaObject::QueryPropertyStored) {
        _id -= 1;
    } else if (_c == QMetaObject::QueryPropertyEditable) {
        _id -= 1;
    } else if (_c == QMetaObject::QueryPropertyUser) {
        _id -= 1;
    }
#endif // QT_NO_PROPERTIES
    return _id;
}
