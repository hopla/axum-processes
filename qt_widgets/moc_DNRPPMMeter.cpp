/****************************************************************************
** Meta object code from reading C++ file 'DNRPPMMeter.h'
**
** Created: Thu Jul 16 10:29:50 2009
**      by: The Qt Meta Object Compiler version 61 (Qt 4.5.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "DNRPPMMeter.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'DNRPPMMeter.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 61
#error "This file was generated using the moc from 4.5.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_DNRPPMMeter[] = {

 // content:
       2,       // revision
       0,       // classname
       0,    0, // classinfo
       1,   12, // methods
      13,   17, // properties
       0,    0, // enums/sets
       0,    0, // constructors

 // slots: signature, parameters, type, tag, flags
      27,   13,   12,   12, 0x0a,

 // properties: name, type, flags
      59,   52, 0x06095003,
      70,   52, 0x06095103,
      84,   52, 0x06095103,
      98,   52, 0x06095103,
     120,  115, 0x01095103,
     129,  115, 0x01095103,
     148,  115, 0x01095103,
     173,  166, 0x43095103,
     182,  166, 0x43095103,
     191,  166, 0x43095103,
     210,  166, 0x43095103,
     237,  229, 0x0a095103,
     261,  229, 0x0a095103,

       0        // eod
};

static const char qt_meta_stringdata_DNRPPMMeter[] = {
    "DNRPPMMeter\0\0NewdBPosition\0"
    "setdBPosition(double_db)\0double\0"
    "dBPosition\0MindBPosition\0MaxdBPosition\0"
    "ReleasePerSecond\0bool\0DINCurve\0"
    "GradientBackground\0GradientForground\0"
    "QColor\0MaxColor\0MinColor\0MaxBackgroundColor\0"
    "MinBackgroundColor\0QString\0"
    "SkinEnvironmentVariable\0"
    "PPMMeterBackgroundFileName\0"
};

const QMetaObject DNRPPMMeter::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_DNRPPMMeter,
      qt_meta_data_DNRPPMMeter, 0 }
};

const QMetaObject *DNRPPMMeter::metaObject() const
{
    return &staticMetaObject;
}

void *DNRPPMMeter::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_DNRPPMMeter))
        return static_cast<void*>(const_cast< DNRPPMMeter*>(this));
    return QWidget::qt_metacast(_clname);
}

int DNRPPMMeter::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: setdBPosition((*reinterpret_cast< double_db(*)>(_a[1]))); break;
        default: ;
        }
        _id -= 1;
    }
#ifndef QT_NO_PROPERTIES
      else if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< double*>(_v) = getdBPosition(); break;
        case 1: *reinterpret_cast< double*>(_v) = getMindBPosition(); break;
        case 2: *reinterpret_cast< double*>(_v) = getMaxdBPosition(); break;
        case 3: *reinterpret_cast< double*>(_v) = getReleasePerSecond(); break;
        case 4: *reinterpret_cast< bool*>(_v) = getDINCurve(); break;
        case 5: *reinterpret_cast< bool*>(_v) = getGradientBackground(); break;
        case 6: *reinterpret_cast< bool*>(_v) = getGradientForground(); break;
        case 7: *reinterpret_cast< QColor*>(_v) = getMaxColor(); break;
        case 8: *reinterpret_cast< QColor*>(_v) = getMinColor(); break;
        case 9: *reinterpret_cast< QColor*>(_v) = getMaxBackgroundColor(); break;
        case 10: *reinterpret_cast< QColor*>(_v) = getMinBackgroundColor(); break;
        case 11: *reinterpret_cast< QString*>(_v) = getSkinEnvironmentVariable(); break;
        case 12: *reinterpret_cast< QString*>(_v) = getPPMMeterBackgroundFileName(); break;
        }
        _id -= 13;
    } else if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: setdBPosition(*reinterpret_cast< double*>(_v)); break;
        case 1: setMindBPosition(*reinterpret_cast< double*>(_v)); break;
        case 2: setMaxdBPosition(*reinterpret_cast< double*>(_v)); break;
        case 3: setReleasePerSecond(*reinterpret_cast< double*>(_v)); break;
        case 4: setDINCurve(*reinterpret_cast< bool*>(_v)); break;
        case 5: setGradientBackground(*reinterpret_cast< bool*>(_v)); break;
        case 6: setGradientForground(*reinterpret_cast< bool*>(_v)); break;
        case 7: setMaxColor(*reinterpret_cast< QColor*>(_v)); break;
        case 8: setMinColor(*reinterpret_cast< QColor*>(_v)); break;
        case 9: setMaxBackgroundColor(*reinterpret_cast< QColor*>(_v)); break;
        case 10: setMinBackgroundColor(*reinterpret_cast< QColor*>(_v)); break;
        case 11: setSkinEnvironmentVariable(*reinterpret_cast< QString*>(_v)); break;
        case 12: setPPMMeterBackgroundFileName(*reinterpret_cast< QString*>(_v)); break;
        }
        _id -= 13;
    } else if (_c == QMetaObject::ResetProperty) {
        _id -= 13;
    } else if (_c == QMetaObject::QueryPropertyDesignable) {
        _id -= 13;
    } else if (_c == QMetaObject::QueryPropertyScriptable) {
        _id -= 13;
    } else if (_c == QMetaObject::QueryPropertyStored) {
        _id -= 13;
    } else if (_c == QMetaObject::QueryPropertyEditable) {
        _id -= 13;
    } else if (_c == QMetaObject::QueryPropertyUser) {
        _id -= 13;
    }
#endif // QT_NO_PROPERTIES
    return _id;
}
QT_END_MOC_NAMESPACE
