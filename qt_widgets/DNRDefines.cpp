#include <QMetaType>
#include "DNRDefines.h"

void registerDNRTypes()
{
   qRegisterMetaType<double_position>("double_position");
   qRegisterMetaType<double_db>("double_db");
   qRegisterMetaType<double_phase>("double_phase");
   qRegisterMetaType<int_number>("int_number");
   qRegisterMetaType<char_none>("char_none");
}
