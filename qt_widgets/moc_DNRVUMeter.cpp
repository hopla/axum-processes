/****************************************************************************
** Meta object code from reading C++ file 'DNRVUMeter.h'
**
** Created: Thu Jul 16 10:30:00 2009
**      by: The Qt Meta Object Compiler version 61 (Qt 4.5.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "DNRVUMeter.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'DNRVUMeter.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 61
#error "This file was generated using the moc from 4.5.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_DNRVUMeter[] = {

 // content:
       2,       // revision
       0,       // classname
       0,    0, // classinfo
       2,   12, // methods
      11,   22, // properties
       0,    0, // enums/sets
       0,    0, // constructors

 // slots: signature, parameters, type, tag, flags
      26,   12,   11,   11, 0x0a,
      58,   51,   11,   11, 0x0a,

 // properties: name, type, flags
      86,   79, 0x06095003,
      97,   79, 0x06095103,
     111,   79, 0x06095103,
     125,   79, 0x06095103,
     147,  142, 0x01095103,
     159,  155, 0x02095103,
     173,  155, 0x02095103,
     187,  155, 0x02095103,
     207,  200, 0x43095103,
     228,  220, 0x0a095103,
     252,  220, 0x0a095103,

       0        // eod
};

static const char qt_meta_stringdata_DNRVUMeter[] = {
    "DNRVUMeter\0\0NewdBPosition\0"
    "setdBPosition(double_db)\0unused\0"
    "doRelease(char_none)\0double\0dBPosition\0"
    "MindBPosition\0MaxdBPosition\0"
    "ReleasePerSecond\0bool\0VUCurve\0int\0"
    "PointerLength\0PointerStartY\0PointerWidth\0"
    "QColor\0PointerColor\0QString\0"
    "SkinEnvironmentVariable\0"
    "VUMeterBackgroundFileName\0"
};

const QMetaObject DNRVUMeter::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_DNRVUMeter,
      qt_meta_data_DNRVUMeter, 0 }
};

const QMetaObject *DNRVUMeter::metaObject() const
{
    return &staticMetaObject;
}

void *DNRVUMeter::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_DNRVUMeter))
        return static_cast<void*>(const_cast< DNRVUMeter*>(this));
    return QWidget::qt_metacast(_clname);
}

int DNRVUMeter::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: setdBPosition((*reinterpret_cast< double_db(*)>(_a[1]))); break;
        case 1: doRelease((*reinterpret_cast< char_none(*)>(_a[1]))); break;
        default: ;
        }
        _id -= 2;
    }
#ifndef QT_NO_PROPERTIES
      else if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< double*>(_v) = getdBPosition(); break;
        case 1: *reinterpret_cast< double*>(_v) = getMindBPosition(); break;
        case 2: *reinterpret_cast< double*>(_v) = getMaxdBPosition(); break;
        case 3: *reinterpret_cast< double*>(_v) = getReleasePerSecond(); break;
        case 4: *reinterpret_cast< bool*>(_v) = getVUCurve(); break;
        case 5: *reinterpret_cast< int*>(_v) = getPointerLength(); break;
        case 6: *reinterpret_cast< int*>(_v) = getPointerStartY(); break;
        case 7: *reinterpret_cast< int*>(_v) = getPointerWidth(); break;
        case 8: *reinterpret_cast< QColor*>(_v) = getPointerColor(); break;
        case 9: *reinterpret_cast< QString*>(_v) = getSkinEnvironmentVariable(); break;
        case 10: *reinterpret_cast< QString*>(_v) = getVUMeterBackgroundFileName(); break;
        }
        _id -= 11;
    } else if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: setdBPosition(*reinterpret_cast< double*>(_v)); break;
        case 1: setMindBPosition(*reinterpret_cast< double*>(_v)); break;
        case 2: setMaxdBPosition(*reinterpret_cast< double*>(_v)); break;
        case 3: setReleasePerSecond(*reinterpret_cast< double*>(_v)); break;
        case 4: setVUCurve(*reinterpret_cast< bool*>(_v)); break;
        case 5: setPointerLength(*reinterpret_cast< int*>(_v)); break;
        case 6: setPointerStartY(*reinterpret_cast< int*>(_v)); break;
        case 7: setPointerWidth(*reinterpret_cast< int*>(_v)); break;
        case 8: setPointerColor(*reinterpret_cast< QColor*>(_v)); break;
        case 9: setSkinEnvironmentVariable(*reinterpret_cast< QString*>(_v)); break;
        case 10: setVUMeterBackgroundFileName(*reinterpret_cast< QString*>(_v)); break;
        }
        _id -= 11;
    } else if (_c == QMetaObject::ResetProperty) {
        _id -= 11;
    } else if (_c == QMetaObject::QueryPropertyDesignable) {
        _id -= 11;
    } else if (_c == QMetaObject::QueryPropertyScriptable) {
        _id -= 11;
    } else if (_c == QMetaObject::QueryPropertyStored) {
        _id -= 11;
    } else if (_c == QMetaObject::QueryPropertyEditable) {
        _id -= 11;
    } else if (_c == QMetaObject::QueryPropertyUser) {
        _id -= 11;
    }
#endif // QT_NO_PROPERTIES
    return _id;
}
QT_END_MOC_NAMESPACE
