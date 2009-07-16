/****************************************************************************
**
** Copyright (C) 2005-2006 Trolltech ASA. All rights reserved.
**
** This file is part of the example classes of the Qt Toolkit.
**
** This file may be used under the terms of the GNU General Public
** License version 2.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of
** this file.  Please review the following information to ensure GNU
** General Public Licensing requirements will be met:
** http://www.trolltech.com/products/qt/opensource.html
**
** If you are unsure which license is appropriate for your use, please
** review the following information:
** http://www.trolltech.com/products/qt/licensing.html or contact the
** sales department at sales@trolltech.com.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

#include "DNRTerminalThread.h"
#include "qstringlist.h"
#include <private/qthread_p.h>
#include "linux/serial.h" //for serial_struct
#include <asm/ioctls.h>   //for TIOCGSERIAL
#include <sys/ioctl.h>
#include <termios.h>
#include <errno.h>
//#include <fcntl.h>
//#include <sys/signal.h>
//#include <sys/types.h>

#ifdef Q_OS_WIN32
#include <windows.h> // for QueryPerformanceCounter
#endif

/*************************/
/* serial port functions */
class DNRTerminalThreadPrivate : public QThreadPrivate
{
    Q_DECLARE_PUBLIC(DNRTerminalThread)
public:
	DNRTerminalThreadPrivate()
	{}
};

DNRTerminalThread::DNRTerminalThread(QObject *parent): QThread(parent)
{
   registerDNRTypes();

   ProcessID = 0;
   LastElapsedTime = 0;
   abort = false;
   
   SerialPortHandle = open("/dev/ttyS1", O_RDWR | O_NOCTTY | O_NDELAY);
   fcntl(SerialPortHandle, F_SETFL, FNDELAY); // don't block serial read
   
   if (SerialPortHandle<0)
   {
      SerialPortDebugMessage.sprintf("open /dev/ttyS1 failed: %d %s\n", errno, strerror(errno));
   }
   else
   {
      serial_struct SerialInformation;
                 
      struct termios tio;
      
      if (tcgetattr(SerialPortHandle, &tio) != 0)
      {
      }
      
      if (cfsetspeed(&tio, B500000) != 0)
      {
         SerialPortDebugMessage.sprintf("cfsetspeed(&tio, B500000) failed: %d %s\n", errno, strerror(errno));
      }
      //cfgetspeed();
      
      tio.c_cflag &= ~CRTSCTS;
      tio.c_cflag |= CLOCAL;
      tio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
      tio.c_oflag &= ~OPOST;
      
      if (tcsetattr(SerialPortHandle, TCSANOW, &tio) != 0)
      {
      }
   }
}

DNRTerminalThread::~DNRTerminalThread()
{
   ::close((int)SerialPortHandle);
      
   abort = true;
	wait();
}

QObjectList DNRTerminalThread::GetReceiverList(const char *signal)
{
	return d_func()->receiverList(signal);
}

QStringList DNRTerminalThread::GetReceivingSlotsList(const char *signal, const char *receiver)
{
	return d_func()->receivingSlotsList(signal, receiver);
}

void DNRTerminalThread::run()
{
	//counter for thread
#ifdef Q_OS_WIN32
	LARGE_INTEGER freq, newTime;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&newTime);
	double newNumberOfSeconds = (double)newTime.QuadPart/freq.QuadPart;
#else
   timespec newTime;
   clock_gettime(CLOCK_MONOTONIC, &newTime);
   double newNumberOfSeconds = newTime.tv_sec+((double)newTime.tv_nsec/1000000000);
#endif

   double previousNumberOfSeconds = newNumberOfSeconds;
   double previousNumberOfSeconds_ms = newNumberOfSeconds;
   double previousNumberOfSeconds_s = newNumberOfSeconds;
   double ElapsedTime;

   ProcessID = getpid();
         
   forever
   {
		if (abort)
			return;
      
      if (SerialPortHandle>=0)
      {
         char TemporyReceiveBuffer[4096];
         
         memset(TemporyReceiveBuffer, 0x00, 4096);
         int NrOfBytesReceived = read(SerialPortHandle,TemporyReceiveBuffer, 4096);
         
         if (NrOfBytesReceived < 0) 
         {
            if (errno == EAGAIN) 
            {
               //SerialPortDebugMessage.sprintf("SERIAL EAGAIN ERROR\n");
            } 
            else 
            {
               SerialPortDebugMessage.sprintf("SERIAL read error %d %s\n", errno, strerror(errno));
            }
         }
         else
         {
            //SerialPortDebugMessage.sprintf("Bytes received %d\n", NrOfBytesReceived);
            
            emit SerialInputCharacters(TemporyReceiveBuffer);
         }
      }
      else
      {
         SerialPortDebugMessage.sprintf("SerialPortHandle = %d\n", SerialPortHandle);
      }
      
      emit SerialDebugMessage(SerialPortDebugMessage);
      
      unsigned char TempBuffer[1] = {0xAA};
      write(SerialPortHandle, TempBuffer, 1);
      

#ifdef Q_OS_WIN32
		QueryPerformanceCounter(&newTime);
		newNumberOfSeconds = (double)newTime.QuadPart/freq.QuadPart;
#else
	   clock_gettime(CLOCK_MONOTONIC, &newTime);
   	newNumberOfSeconds = newTime.tv_sec+((double)newTime.tv_nsec/1000000000);
#endif

      LastElapsedTime = newNumberOfSeconds-previousNumberOfSeconds;
      previousNumberOfSeconds = newNumberOfSeconds;

      usleep(10000);
   }
}

int DNRTerminalThread::getProcessID()
{
   return ProcessID;
}

