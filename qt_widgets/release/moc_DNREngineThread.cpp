/****************************************************************************
** Meta object code from reading C++ file 'DNREngineThread.h'
**
** Created: Mon Oct 22 15:13:48 2007
**      by: The Qt Meta Object Compiler version 59 (Qt 4.2.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../DNREngineThread.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'DNREngineThread.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 59
#error "This file was generated using the moc from 4.2.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

static const uint qt_meta_data_DNREngineThread[] = {

 // content:
       1,       // revision
       0,       // classname
       0,    0, // classinfo
      14,   10, // methods
       0,    0, // properties
       0,    0, // enums/sets

 // signals: signature, parameters, type, tag, flags
      36,   17,   16,   16, 0x05,
      82,   17,   16,   16, 0x05,
     130,   17,   16,   16, 0x05,
     178,   17,   16,   16, 0x05,
     226,   17,   16,   16, 0x05,
     281,  274,   16,   16, 0x05,
     302,   16,   16,   16, 0x05,
     334,  325,   16,   16, 0x05,

 // slots: signature, parameters, type, tag, flags
     368,   17,   16,   16, 0x0a,
     420,   17,   16,   16, 0x0a,
     472,   17,   16,   16, 0x0a,
     524,   17,   16,   16, 0x0a,
     576,   17,   16,   16, 0x0a,
     618,   17,   16,   16, 0x0a,

       0        // eod
};

static const char qt_meta_stringdata_DNREngineThread[] = {
    "DNREngineThread\0\0ChannelNr,Position\0"
    "FaderLevelChanged(int_number,double_position)\0"
    "EQBand1LevelChanged(int_number,double_position)\0"
    "EQBand2LevelChanged(int_number,double_position)\0"
    "EQBand3LevelChanged(int_number,double_position)\0"
    "EQBand4LevelChanged(int_number,double_position)\0"
    "unused\0TimerTick(char_none)\0"
    "MeterChange(double_db)\0Position\0"
    "Redlight1Changed(double_position)\0"
    "doBand1EQPositionChange(int_number,double_position)\0"
    "doBand2EQPositionChange(int_number,double_position)\0"
    "doBand3EQPositionChange(int_number,double_position)\0"
    "doBand4EQPositionChange(int_number,double_position)\0"
    "doFaderChange(int_number,double_position)\0"
    "doRedlight1SettingChange(int_number,double_position)\0"
};

const QMetaObject DNREngineThread::staticMetaObject = {
    { &QThread::staticMetaObject, qt_meta_stringdata_DNREngineThread,
      qt_meta_data_DNREngineThread, 0 }
};

const QMetaObject *DNREngineThread::metaObject() const
{
    return &staticMetaObject;
}

void *DNREngineThread::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_DNREngineThread))
	return static_cast<void*>(const_cast<DNREngineThread*>(this));
    return QThread::qt_metacast(_clname);
}

int DNREngineThread::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QThread::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: FaderLevelChanged((*reinterpret_cast< int_number(*)>(_a[1])),(*reinterpret_cast< double_position(*)>(_a[2]))); break;
        case 1: EQBand1LevelChanged((*reinterpret_cast< int_number(*)>(_a[1])),(*reinterpret_cast< double_position(*)>(_a[2]))); break;
        case 2: EQBand2LevelChanged((*reinterpret_cast< int_number(*)>(_a[1])),(*reinterpret_cast< double_position(*)>(_a[2]))); break;
        case 3: EQBand3LevelChanged((*reinterpret_cast< int_number(*)>(_a[1])),(*reinterpret_cast< double_position(*)>(_a[2]))); break;
        case 4: EQBand4LevelChanged((*reinterpret_cast< int_number(*)>(_a[1])),(*reinterpret_cast< double_position(*)>(_a[2]))); break;
        case 5: TimerTick((*reinterpret_cast< char_none(*)>(_a[1]))); break;
        case 6: MeterChange((*reinterpret_cast< double_db(*)>(_a[1]))); break;
        case 7: Redlight1Changed((*reinterpret_cast< double_position(*)>(_a[1]))); break;
        case 8: doBand1EQPositionChange((*reinterpret_cast< int_number(*)>(_a[1])),(*reinterpret_cast< double_position(*)>(_a[2]))); break;
        case 9: doBand2EQPositionChange((*reinterpret_cast< int_number(*)>(_a[1])),(*reinterpret_cast< double_position(*)>(_a[2]))); break;
        case 10: doBand3EQPositionChange((*reinterpret_cast< int_number(*)>(_a[1])),(*reinterpret_cast< double_position(*)>(_a[2]))); break;
        case 11: doBand4EQPositionChange((*reinterpret_cast< int_number(*)>(_a[1])),(*reinterpret_cast< double_position(*)>(_a[2]))); break;
        case 12: doFaderChange((*reinterpret_cast< int_number(*)>(_a[1])),(*reinterpret_cast< double_position(*)>(_a[2]))); break;
        case 13: doRedlight1SettingChange((*reinterpret_cast< int_number(*)>(_a[1])),(*reinterpret_cast< double_position(*)>(_a[2]))); break;
        }
        _id -= 14;
    }
    return _id;
}

// SIGNAL 0
void DNREngineThread::FaderLevelChanged(int_number _t1, double_position _t2)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void DNREngineThread::EQBand1LevelChanged(int_number _t1, double_position _t2)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void DNREngineThread::EQBand2LevelChanged(int_number _t1, double_position _t2)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void DNREngineThread::EQBand3LevelChanged(int_number _t1, double_position _t2)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void DNREngineThread::EQBand4LevelChanged(int_number _t1, double_position _t2)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void DNREngineThread::TimerTick(char_none _t1)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}

// SIGNAL 6
void DNREngineThread::MeterChange(double_db _t1)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 6, _a);
}

// SIGNAL 7
void DNREngineThread::Redlight1Changed(double_position _t1)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 7, _a);
}
