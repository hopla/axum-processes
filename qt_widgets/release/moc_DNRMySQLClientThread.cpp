/****************************************************************************
** Meta object code from reading C++ file 'DNRMySQLClientThread.h'
**
** Created: Mon Oct 22 15:14:10 2007
**      by: The Qt Meta Object Compiler version 59 (Qt 4.2.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../DNRMySQLClientThread.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'DNRMySQLClientThread.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 59
#error "This file was generated using the moc from 4.2.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

static const uint qt_meta_data_DNRMySQLClientThread[] = {

 // content:
       1,       // revision
       0,       // classname
       0,    0, // classinfo
       2,   10, // methods
       0,    0, // properties
       0,    0, // enums/sets

 // signals: signature, parameters, type, tag, flags
      30,   22,   21,   21, 0x05,
      74,   55,   21,   21, 0x05,

       0        // eod
};

static const char qt_meta_stringdata_DNRMySQLClientThread[] = {
    "DNRMySQLClientThread\0\0Message\0"
    "SQLErrorMessage(QString)\0ChannelNr,Position\0"
    "HardwareStatusChange(int_number,double_position)\0"
};

const QMetaObject DNRMySQLClientThread::staticMetaObject = {
    { &QThread::staticMetaObject, qt_meta_stringdata_DNRMySQLClientThread,
      qt_meta_data_DNRMySQLClientThread, 0 }
};

const QMetaObject *DNRMySQLClientThread::metaObject() const
{
    return &staticMetaObject;
}

void *DNRMySQLClientThread::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_DNRMySQLClientThread))
	return static_cast<void*>(const_cast<DNRMySQLClientThread*>(this));
    return QThread::qt_metacast(_clname);
}

int DNRMySQLClientThread::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QThread::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: SQLErrorMessage((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 1: HardwareStatusChange((*reinterpret_cast< int_number(*)>(_a[1])),(*reinterpret_cast< double_position(*)>(_a[2]))); break;
        }
        _id -= 2;
    }
    return _id;
}

// SIGNAL 0
void DNRMySQLClientThread::SQLErrorMessage(QString _t1)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void DNRMySQLClientThread::HardwareStatusChange(int_number _t1, double_position _t2)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}
