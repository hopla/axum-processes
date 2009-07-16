/****************************************************************************
** Meta object code from reading C++ file 'DNRImage.h'
**
** Created: Thu Jul 16 10:30:21 2009
**      by: The Qt Meta Object Compiler version 61 (Qt 4.5.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "DNRImage.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'DNRImage.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 61
#error "This file was generated using the moc from 4.5.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_DNRImage[] = {

 // content:
       2,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       3,   12, // properties
       0,    0, // enums/sets
       0,    0, // constructors

 // properties: name, type, flags
      17,    9, 0x0a095103,
      41,    9, 0x0a095103,
      60,   55, 0x01095103,

       0        // eod
};

static const char qt_meta_stringdata_DNRImage[] = {
    "DNRImage\0QString\0SkinEnvironmentVariable\0"
    "ImageFileName\0bool\0ScaleImage\0"
};

const QMetaObject DNRImage::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_DNRImage,
      qt_meta_data_DNRImage, 0 }
};

const QMetaObject *DNRImage::metaObject() const
{
    return &staticMetaObject;
}

void *DNRImage::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_DNRImage))
        return static_cast<void*>(const_cast< DNRImage*>(this));
    return QWidget::qt_metacast(_clname);
}

int DNRImage::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    
#ifndef QT_NO_PROPERTIES
     if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< QString*>(_v) = getSkinEnvironmentVariable(); break;
        case 1: *reinterpret_cast< QString*>(_v) = getImageFileName(); break;
        case 2: *reinterpret_cast< bool*>(_v) = getScaleImage(); break;
        }
        _id -= 3;
    } else if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: setSkinEnvironmentVariable(*reinterpret_cast< QString*>(_v)); break;
        case 1: setImageFileName(*reinterpret_cast< QString*>(_v)); break;
        case 2: setScaleImage(*reinterpret_cast< bool*>(_v)); break;
        }
        _id -= 3;
    } else if (_c == QMetaObject::ResetProperty) {
        _id -= 3;
    } else if (_c == QMetaObject::QueryPropertyDesignable) {
        _id -= 3;
    } else if (_c == QMetaObject::QueryPropertyScriptable) {
        _id -= 3;
    } else if (_c == QMetaObject::QueryPropertyStored) {
        _id -= 3;
    } else if (_c == QMetaObject::QueryPropertyEditable) {
        _id -= 3;
    } else if (_c == QMetaObject::QueryPropertyUser) {
        _id -= 3;
    }
#endif // QT_NO_PROPERTIES
    return _id;
}
QT_END_MOC_NAMESPACE
