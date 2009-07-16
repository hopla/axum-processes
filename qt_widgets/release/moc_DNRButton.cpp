/****************************************************************************
** Meta object code from reading C++ file 'DNRButton.h'
**
** Created: Tue Apr 28 10:47:54 2009
**      by: The Qt Meta Object Compiler version 61 (Qt 4.5.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../DNRButton.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'DNRButton.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 61
#error "This file was generated using the moc from 4.5.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_DNRButton[] = {

 // content:
       2,       // revision
       0,       // classname
       0,    0, // classinfo
       2,   12, // methods
      14,   22, // properties
       0,    0, // enums/sets
       0,    0, // constructors

 // signals: signature, parameters, type, tag, flags
      30,   11,   10,   10, 0x05,

 // slots: signature, parameters, type, tag, flags
      93,   74,   10,   10, 0x0a,

 // properties: name, type, flags
     134,  130, 0x02095103,
     149,  141, 0x0a095103,
     165,  160, 0x01095103,
     174,  160, 0x01095103,
     180,  141, 0x0a095103,
     204,  141, 0x0a095103,
     223,  141, 0x0a095103,
     241,  141, 0x0a095103,
     262,  141, 0x0a095103,
     289,  282, 0x43095103,
     301,  282, 0x43095103,
     312,  282, 0x43095103,
     322,  282, 0x43095103,
     335,  282, 0x43095103,

       0        // eod
};

static const char qt_meta_stringdata_DNRButton[] = {
    "DNRButton\0\0Number,NewPosition\0"
    "positionChanged(int_number,double_position)\0"
    "ChannelNr,NewState\0"
    "setState(int_number,double_position)\0"
    "int\0Number\0QString\0ButtonText\0bool\0"
    "Position\0State\0SkinEnvironmentVariable\0"
    "UpOffImageFileName\0UpOnImageFileName\0"
    "DownOffImageFileName\0DownOnImageFileName\0"
    "QColor\0BorderColor\0UpOffColor\0UpOnColor\0"
    "DownOffColor\0DownOnColor\0"
};

const QMetaObject DNRButton::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_DNRButton,
      qt_meta_data_DNRButton, 0 }
};

const QMetaObject *DNRButton::metaObject() const
{
    return &staticMetaObject;
}

void *DNRButton::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_DNRButton))
        return static_cast<void*>(const_cast< DNRButton*>(this));
    return QWidget::qt_metacast(_clname);
}

int DNRButton::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: positionChanged((*reinterpret_cast< int_number(*)>(_a[1])),(*reinterpret_cast< double_position(*)>(_a[2]))); break;
        case 1: setState((*reinterpret_cast< int_number(*)>(_a[1])),(*reinterpret_cast< double_position(*)>(_a[2]))); break;
        default: ;
        }
        _id -= 2;
    }
#ifndef QT_NO_PROPERTIES
      else if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< int*>(_v) = getNumber(); break;
        case 1: *reinterpret_cast< QString*>(_v) = getButtonText(); break;
        case 2: *reinterpret_cast< bool*>(_v) = getPosition(); break;
        case 3: *reinterpret_cast< bool*>(_v) = getState(); break;
        case 4: *reinterpret_cast< QString*>(_v) = getSkinEnvironmentVariable(); break;
        case 5: *reinterpret_cast< QString*>(_v) = getUpOffImageFileName(); break;
        case 6: *reinterpret_cast< QString*>(_v) = getUpOnImageFileName(); break;
        case 7: *reinterpret_cast< QString*>(_v) = getDownOffImageFileName(); break;
        case 8: *reinterpret_cast< QString*>(_v) = getDownOnImageFileName(); break;
        case 9: *reinterpret_cast< QColor*>(_v) = getBorderColor(); break;
        case 10: *reinterpret_cast< QColor*>(_v) = getUpOffColor(); break;
        case 11: *reinterpret_cast< QColor*>(_v) = getUpOnColor(); break;
        case 12: *reinterpret_cast< QColor*>(_v) = getDownOffColor(); break;
        case 13: *reinterpret_cast< QColor*>(_v) = getDownOnColor(); break;
        }
        _id -= 14;
    } else if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: setNumber(*reinterpret_cast< int*>(_v)); break;
        case 1: setButtonText(*reinterpret_cast< QString*>(_v)); break;
        case 2: setPosition(*reinterpret_cast< bool*>(_v)); break;
        case 3: setState(*reinterpret_cast< bool*>(_v)); break;
        case 4: setSkinEnvironmentVariable(*reinterpret_cast< QString*>(_v)); break;
        case 5: setUpOffImageFileName(*reinterpret_cast< QString*>(_v)); break;
        case 6: setUpOnImageFileName(*reinterpret_cast< QString*>(_v)); break;
        case 7: setDownOffImageFileName(*reinterpret_cast< QString*>(_v)); break;
        case 8: setDownOnImageFileName(*reinterpret_cast< QString*>(_v)); break;
        case 9: setBorderColor(*reinterpret_cast< QColor*>(_v)); break;
        case 10: setUpOffColor(*reinterpret_cast< QColor*>(_v)); break;
        case 11: setUpOnColor(*reinterpret_cast< QColor*>(_v)); break;
        case 12: setDownOffColor(*reinterpret_cast< QColor*>(_v)); break;
        case 13: setDownOnColor(*reinterpret_cast< QColor*>(_v)); break;
        }
        _id -= 14;
    } else if (_c == QMetaObject::ResetProperty) {
        _id -= 14;
    } else if (_c == QMetaObject::QueryPropertyDesignable) {
        _id -= 14;
    } else if (_c == QMetaObject::QueryPropertyScriptable) {
        _id -= 14;
    } else if (_c == QMetaObject::QueryPropertyStored) {
        _id -= 14;
    } else if (_c == QMetaObject::QueryPropertyEditable) {
        _id -= 14;
    } else if (_c == QMetaObject::QueryPropertyUser) {
        _id -= 14;
    }
#endif // QT_NO_PROPERTIES
    return _id;
}

// SIGNAL 0
void DNRButton::positionChanged(int_number _t1, double_position _t2)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
QT_END_MOC_NAMESPACE
