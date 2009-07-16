/****************************************************************************
** Meta object code from reading C++ file 'DNRIndication.h'
**
** Created: Thu Jul 16 10:30:31 2009
**      by: The Qt Meta Object Compiler version 61 (Qt 4.5.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "DNRIndication.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'DNRIndication.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 61
#error "This file was generated using the moc from 4.5.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_DNRIndication[] = {

 // content:
       2,       // revision
       0,       // classname
       0,    0, // classinfo
       1,   12, // methods
       7,   17, // properties
       0,    0, // enums/sets
       0,    0, // constructors

 // slots: signature, parameters, type, tag, flags
      31,   15,   14,   14, 0x0a,

 // properties: name, type, flags
      72,   68, 0x02095103,
      79,   68, 0x02095103,
      93,   85, 0x0a095103,
     117,   85, 0x0a095103,
     137,   85, 0x0a095103,
     157,   85, 0x0a095103,
     177,   85, 0x0a095103,

       0        // eod
};

static const char qt_meta_stringdata_DNRIndication[] = {
    "DNRIndication\0\0Number,NewState\0"
    "setState(int_number,double_position)\0"
    "int\0Number\0State\0QString\0"
    "SkinEnvironmentVariable\0State1ImageFileName\0"
    "State2ImageFileName\0State3ImageFileName\0"
    "State4ImageFileName\0"
};

const QMetaObject DNRIndication::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_DNRIndication,
      qt_meta_data_DNRIndication, 0 }
};

const QMetaObject *DNRIndication::metaObject() const
{
    return &staticMetaObject;
}

void *DNRIndication::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_DNRIndication))
        return static_cast<void*>(const_cast< DNRIndication*>(this));
    return QWidget::qt_metacast(_clname);
}

int DNRIndication::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: setState((*reinterpret_cast< int_number(*)>(_a[1])),(*reinterpret_cast< double_position(*)>(_a[2]))); break;
        default: ;
        }
        _id -= 1;
    }
#ifndef QT_NO_PROPERTIES
      else if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< int*>(_v) = getNumber(); break;
        case 1: *reinterpret_cast< int*>(_v) = getState(); break;
        case 2: *reinterpret_cast< QString*>(_v) = getSkinEnvironmentVariable(); break;
        case 3: *reinterpret_cast< QString*>(_v) = getState1ImageFileName(); break;
        case 4: *reinterpret_cast< QString*>(_v) = getState2ImageFileName(); break;
        case 5: *reinterpret_cast< QString*>(_v) = getState3ImageFileName(); break;
        case 6: *reinterpret_cast< QString*>(_v) = getState4ImageFileName(); break;
        }
        _id -= 7;
    } else if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: setNumber(*reinterpret_cast< int*>(_v)); break;
        case 1: setState(*reinterpret_cast< int*>(_v)); break;
        case 2: setSkinEnvironmentVariable(*reinterpret_cast< QString*>(_v)); break;
        case 3: setState1ImageFileName(*reinterpret_cast< QString*>(_v)); break;
        case 4: setState2ImageFileName(*reinterpret_cast< QString*>(_v)); break;
        case 5: setState3ImageFileName(*reinterpret_cast< QString*>(_v)); break;
        case 6: setState4ImageFileName(*reinterpret_cast< QString*>(_v)); break;
        }
        _id -= 7;
    } else if (_c == QMetaObject::ResetProperty) {
        _id -= 7;
    } else if (_c == QMetaObject::QueryPropertyDesignable) {
        _id -= 7;
    } else if (_c == QMetaObject::QueryPropertyScriptable) {
        _id -= 7;
    } else if (_c == QMetaObject::QueryPropertyStored) {
        _id -= 7;
    } else if (_c == QMetaObject::QueryPropertyEditable) {
        _id -= 7;
    } else if (_c == QMetaObject::QueryPropertyUser) {
        _id -= 7;
    }
#endif // QT_NO_PROPERTIES
    return _id;
}
QT_END_MOC_NAMESPACE
