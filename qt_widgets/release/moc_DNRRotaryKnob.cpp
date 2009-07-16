/****************************************************************************
** Meta object code from reading C++ file 'DNRRotaryKnob.h'
**
** Created: Tue Apr 28 10:47:44 2009
**      by: The Qt Meta Object Compiler version 61 (Qt 4.5.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../DNRRotaryKnob.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'DNRRotaryKnob.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 61
#error "This file was generated using the moc from 4.5.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_DNRRotaryKnob[] = {

 // content:
       2,       // revision
       0,       // classname
       0,    0, // classinfo
       2,   12, // methods
       7,   22, // properties
       0,    0, // enums/sets
       0,    0, // constructors

 // signals: signature, parameters, type, tag, flags
      37,   15,   14,   14, 0x05,

 // slots: signature, parameters, type, tag, flags
      75,   15,   14,   14, 0x0a,

 // properties: name, type, flags
     119,  115, 0x02095103,
     133,  126, 0x06095103,
     149,  142, 0x43095103,
     161,  142, 0x43095103,
     176,  171, 0x01095103,
     199,  191, 0x0a095103,
     223,  191, 0x0a095103,

       0        // eod
};

static const char qt_meta_stringdata_DNRRotaryKnob[] = {
    "DNRRotaryKnob\0\0ChannelNr,NewPosition\0"
    "KnobMoved(int_number,double_position)\0"
    "setPosition(int_number,double_position)\0"
    "int\0Number\0double\0Position\0QColor\0"
    "BorderColor\0KnobColor\0bool\0ScaleKnobImage\0"
    "QString\0SkinEnvironmentVariable\0"
    "RotaryKnobFileName\0"
};

const QMetaObject DNRRotaryKnob::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_DNRRotaryKnob,
      qt_meta_data_DNRRotaryKnob, 0 }
};

const QMetaObject *DNRRotaryKnob::metaObject() const
{
    return &staticMetaObject;
}

void *DNRRotaryKnob::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_DNRRotaryKnob))
        return static_cast<void*>(const_cast< DNRRotaryKnob*>(this));
    return QWidget::qt_metacast(_clname);
}

int DNRRotaryKnob::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: KnobMoved((*reinterpret_cast< int_number(*)>(_a[1])),(*reinterpret_cast< double_position(*)>(_a[2]))); break;
        case 1: setPosition((*reinterpret_cast< int_number(*)>(_a[1])),(*reinterpret_cast< double_position(*)>(_a[2]))); break;
        default: ;
        }
        _id -= 2;
    }
#ifndef QT_NO_PROPERTIES
      else if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< int*>(_v) = getNumber(); break;
        case 1: *reinterpret_cast< double*>(_v) = getPosition(); break;
        case 2: *reinterpret_cast< QColor*>(_v) = getBorderColor(); break;
        case 3: *reinterpret_cast< QColor*>(_v) = getKnobColor(); break;
        case 4: *reinterpret_cast< bool*>(_v) = getScaleKnobImage(); break;
        case 5: *reinterpret_cast< QString*>(_v) = getSkinEnvironmentVariable(); break;
        case 6: *reinterpret_cast< QString*>(_v) = getRotaryKnobFileName(); break;
        }
        _id -= 7;
    } else if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: setNumber(*reinterpret_cast< int*>(_v)); break;
        case 1: setPosition(*reinterpret_cast< double*>(_v)); break;
        case 2: setBorderColor(*reinterpret_cast< QColor*>(_v)); break;
        case 3: setKnobColor(*reinterpret_cast< QColor*>(_v)); break;
        case 4: setScaleKnobImage(*reinterpret_cast< bool*>(_v)); break;
        case 5: setSkinEnvironmentVariable(*reinterpret_cast< QString*>(_v)); break;
        case 6: setRotaryKnobFileName(*reinterpret_cast< QString*>(_v)); break;
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

// SIGNAL 0
void DNRRotaryKnob::KnobMoved(int_number _t1, double_position _t2)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
QT_END_MOC_NAMESPACE
