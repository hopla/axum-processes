/****************************************************************************
** Meta object code from reading C++ file 'DNRPhaseMeter.h'
**
** Created: Thu Jul 16 10:30:11 2009
**      by: The Qt Meta Object Compiler version 61 (Qt 4.5.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "DNRPhaseMeter.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'DNRPhaseMeter.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 61
#error "This file was generated using the moc from 4.5.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_DNRPhaseMeter[] = {

 // content:
       2,       // revision
       0,       // classname
       0,    0, // classinfo
       1,   12, // methods
      12,   17, // properties
       0,    0, // enums/sets
       0,    0, // constructors

 // slots: signature, parameters, type, tag, flags
      27,   15,   14,   14, 0x0a,

 // properties: name, type, flags
      60,   53, 0x06095103,
      69,   53, 0x06095103,
      81,   53, 0x06095103,
      97,   93, 0x02095103,
     117,  110, 0x43095103,
     133,  110, 0x43095103,
     149,  110, 0x43095103,
     169,  110, 0x43095103,
     200,  195, 0x01095103,
     219,  195, 0x01095103,
     238,  230, 0x0a095103,
     262,  230, 0x0a095103,

       0        // eod
};

static const char qt_meta_stringdata_DNRPhaseMeter[] = {
    "DNRPhaseMeter\0\0NewPosition\0"
    "setPosition(double_phase)\0double\0"
    "Position\0MinPosition\0MaxPosition\0int\0"
    "PointerWidth\0QColor\0PointerMinColor\0"
    "PointerMaxColor\0BackgroundMonoColor\0"
    "BackgroundOutOfPhaseColor\0bool\0"
    "GradientBackground\0Horizontal\0QString\0"
    "SkinEnvironmentVariable\0"
    "PhaseMeterBackgroundFileName\0"
};

const QMetaObject DNRPhaseMeter::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_DNRPhaseMeter,
      qt_meta_data_DNRPhaseMeter, 0 }
};

const QMetaObject *DNRPhaseMeter::metaObject() const
{
    return &staticMetaObject;
}

void *DNRPhaseMeter::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_DNRPhaseMeter))
        return static_cast<void*>(const_cast< DNRPhaseMeter*>(this));
    return QWidget::qt_metacast(_clname);
}

int DNRPhaseMeter::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: setPosition((*reinterpret_cast< double_phase(*)>(_a[1]))); break;
        default: ;
        }
        _id -= 1;
    }
#ifndef QT_NO_PROPERTIES
      else if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< double*>(_v) = getPosition(); break;
        case 1: *reinterpret_cast< double*>(_v) = getMinPosition(); break;
        case 2: *reinterpret_cast< double*>(_v) = getMaxPosition(); break;
        case 3: *reinterpret_cast< int*>(_v) = getPointerWidth(); break;
        case 4: *reinterpret_cast< QColor*>(_v) = getPointerMinColor(); break;
        case 5: *reinterpret_cast< QColor*>(_v) = getPointerMaxColor(); break;
        case 6: *reinterpret_cast< QColor*>(_v) = getBackgroundMonoColor(); break;
        case 7: *reinterpret_cast< QColor*>(_v) = getBackgroundOutOfPhaseColor(); break;
        case 8: *reinterpret_cast< bool*>(_v) = getGradientBackground(); break;
        case 9: *reinterpret_cast< bool*>(_v) = getHorizontal(); break;
        case 10: *reinterpret_cast< QString*>(_v) = getSkinEnvironmentVariable(); break;
        case 11: *reinterpret_cast< QString*>(_v) = getPhaseMeterBackgroundFileName(); break;
        }
        _id -= 12;
    } else if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: setPosition(*reinterpret_cast< double*>(_v)); break;
        case 1: setMinPosition(*reinterpret_cast< double*>(_v)); break;
        case 2: setMaxPosition(*reinterpret_cast< double*>(_v)); break;
        case 3: setPointerWidth(*reinterpret_cast< int*>(_v)); break;
        case 4: setPointerMinColor(*reinterpret_cast< QColor*>(_v)); break;
        case 5: setPointerMaxColor(*reinterpret_cast< QColor*>(_v)); break;
        case 6: setBackgroundMonoColor(*reinterpret_cast< QColor*>(_v)); break;
        case 7: setBackgroundOutOfPhaseColor(*reinterpret_cast< QColor*>(_v)); break;
        case 8: setGradientBackground(*reinterpret_cast< bool*>(_v)); break;
        case 9: setHorizontal(*reinterpret_cast< bool*>(_v)); break;
        case 10: setSkinEnvironmentVariable(*reinterpret_cast< QString*>(_v)); break;
        case 11: setPhaseMeterBackgroundFileName(*reinterpret_cast< QString*>(_v)); break;
        }
        _id -= 12;
    } else if (_c == QMetaObject::ResetProperty) {
        _id -= 12;
    } else if (_c == QMetaObject::QueryPropertyDesignable) {
        _id -= 12;
    } else if (_c == QMetaObject::QueryPropertyScriptable) {
        _id -= 12;
    } else if (_c == QMetaObject::QueryPropertyStored) {
        _id -= 12;
    } else if (_c == QMetaObject::QueryPropertyEditable) {
        _id -= 12;
    } else if (_c == QMetaObject::QueryPropertyUser) {
        _id -= 12;
    }
#endif // QT_NO_PROPERTIES
    return _id;
}
QT_END_MOC_NAMESPACE
