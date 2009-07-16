/****************************************************************************
** Meta object code from reading C++ file 'DNRNetworkServer.h'
**
** Created: Tue Apr 28 10:56:08 2009
**      by: The Qt Meta Object Compiler version 61 (Qt 4.5.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../DNRNetworkServer.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'DNRNetworkServer.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 61
#error "This file was generated using the moc from 4.5.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_DNRNetworkServer[] = {

 // content:
       2,       // revision
       0,       // classname
       0,    0, // classinfo
       5,   12, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors

 // signals: signature, parameters, type, tag, flags
      37,   18,   17,   17, 0x05,

 // slots: signature, parameters, type, tag, flags
      86,   17,   17,   17, 0x08,
     104,   17,   17,   17, 0x08,
     118,   17,   17,   17, 0x08,
     151,   18,   17,   17, 0x0a,

       0        // eod
};

static const char qt_meta_stringdata_DNRNetworkServer[] = {
    "DNRNetworkServer\0\0ChannelNr,Position\0"
    "FaderPositionChanged(int_number,double_position)\0"
    "doReadTCPSocket()\0doConnected()\0"
    "RemoveFromClientConnectionList()\0"
    "doFaderPositionChange(int_number,double_position)\0"
};

const QMetaObject DNRNetworkServer::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_DNRNetworkServer,
      qt_meta_data_DNRNetworkServer, 0 }
};

const QMetaObject *DNRNetworkServer::metaObject() const
{
    return &staticMetaObject;
}

void *DNRNetworkServer::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_DNRNetworkServer))
        return static_cast<void*>(const_cast< DNRNetworkServer*>(this));
    return QWidget::qt_metacast(_clname);
}

int DNRNetworkServer::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: FaderPositionChanged((*reinterpret_cast< int_number(*)>(_a[1])),(*reinterpret_cast< double_position(*)>(_a[2]))); break;
        case 1: doReadTCPSocket(); break;
        case 2: doConnected(); break;
        case 3: RemoveFromClientConnectionList(); break;
        case 4: doFaderPositionChange((*reinterpret_cast< int_number(*)>(_a[1])),(*reinterpret_cast< double_position(*)>(_a[2]))); break;
        default: ;
        }
        _id -= 5;
    }
    return _id;
}

// SIGNAL 0
void DNRNetworkServer::FaderPositionChanged(int_number _t1, double_position _t2)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
QT_END_MOC_NAMESPACE
