/****************************************************************************
** Meta object code from reading C++ file 'DNRTerminalThread.h'
**
** Created: Mon Oct 22 16:16:12 2007
**      by: The Qt Meta Object Compiler version 59 (Qt 4.2.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../DNRTerminalThread.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'DNRTerminalThread.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 59
#error "This file was generated using the moc from 4.2.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

static const uint qt_meta_data_DNRTerminalThread[] = {

 // content:
       1,       // revision
       0,       // classname
       0,    0, // classinfo
       2,   10, // methods
       0,    0, // properties
       0,    0, // enums/sets

 // signals: signature, parameters, type, tag, flags
      27,   19,   18,   18, 0x05,
      58,   19,   18,   18, 0x05,

       0        // eod
};

static const char qt_meta_stringdata_DNRTerminalThread[] = {
    "DNRTerminalThread\0\0Message\0"
    "SerialInputCharacters(QString)\0"
    "SerialDebugMessage(QString)\0"
};

const QMetaObject DNRTerminalThread::staticMetaObject = {
    { &QThread::staticMetaObject, qt_meta_stringdata_DNRTerminalThread,
      qt_meta_data_DNRTerminalThread, 0 }
};

const QMetaObject *DNRTerminalThread::metaObject() const
{
    return &staticMetaObject;
}

void *DNRTerminalThread::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_DNRTerminalThread))
	return static_cast<void*>(const_cast<DNRTerminalThread*>(this));
    return QThread::qt_metacast(_clname);
}

int DNRTerminalThread::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QThread::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: SerialInputCharacters((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 1: SerialDebugMessage((*reinterpret_cast< QString(*)>(_a[1]))); break;
        }
        _id -= 2;
    }
    return _id;
}

// SIGNAL 0
void DNRTerminalThread::SerialInputCharacters(QString _t1)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void DNRTerminalThread::SerialDebugMessage(QString _t1)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}
