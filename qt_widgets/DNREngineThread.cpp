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

#include "DNREngineThread.h"
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

int fd = -1;

//Encode 8-to-7 bits
unsigned char Encode8to7bits(unsigned char *Buffer, unsigned char BufferLength, unsigned char *Buffer7Bit)
{   
   unsigned char cntByte;   
   unsigned char cntBuffer7Bit;   
   unsigned char Mask1;   
   unsigned char Mask2;   
   unsigned char BitShift;
   
   cntBuffer7Bit = 0;   
   Buffer7Bit[cntBuffer7Bit] = 0;   
   for (cntByte=0; cntByte<BufferLength; cntByte++)   
   {      
      BitShift = cntByte%7;       
      Mask1 = 0x7F>>BitShift;      
      Mask2 = Mask1^0xFF;                                        
      
      Buffer7Bit[cntBuffer7Bit++] |= (Buffer[cntByte]&Mask1)<<BitShift;
      Buffer7Bit[cntBuffer7Bit  ]  = (Buffer[cntByte]&Mask2)>>(7-BitShift);      
      if (Mask2 == 0xFE)      
      {
         cntBuffer7Bit++;         
         Buffer7Bit[cntBuffer7Bit] = 0x00;      
      }   
   }   
   if ((cntByte%7) != 0)   
   {      
      cntBuffer7Bit++;   
   }      
         
   return cntBuffer7Bit;
}

//Decode 7-to-8 bits
unsigned char Decode7to8bits(unsigned char *Buffer, unsigned char BufferLength, unsigned char *Buffer8Bit)
{   
   unsigned char cntByte;
   unsigned char cntBuffer8Bit;
   unsigned char Mask1;
   unsigned char Mask2;

   cntBuffer8Bit = 0;
   Buffer8Bit[cntBuffer8Bit] = Buffer[0]&0x7F;
   for (cntByte=1; cntByte<BufferLength; cntByte++)
   {
      Mask1 = (0x7F>>(cntByte&0x07))<<(cntByte&0x07);
      Mask2 = Mask1^0x7F;

      if (Mask2 != 0x00)
      {
         Buffer8Bit[cntBuffer8Bit++] |= (Buffer[cntByte]&Mask2)<<(8-(cntByte&0x07));
      }
      Buffer8Bit[cntBuffer8Bit] = (Buffer[cntByte]&Mask1)>>(cntByte&0x07);
   }

   return cntBuffer8Bit;
}

void SendSerialMessage(unsigned long int ToAddress, unsigned int MessageType, unsigned char *Data, unsigned char DataLength)
{
#define MAX_DNR_DATA_SIZE           99    //Number of 8 bits bytes
#define MAX_DNR_CAN_BUFFER_SIZE     127   //Number of 7 bits bytes
#define PROTOCOL_MAJOR_VERSION      0
#define PROTOCOL_MINOR_VERSION      1
#define PROTOCOL_OVERHEAD           13    //Number of transport protocol defined bytes
   unsigned char TransmitMessageBuffer[MAX_DNR_CAN_BUFFER_SIZE];
   unsigned char cntTransmitMessageBuffer;
   unsigned char TransmitMessageBufferLength;
   unsigned long LocalAddress = 0x00000000;
   
   if (DataLength <= MAX_DNR_DATA_SIZE)   
   {      
      unsigned char cntByte;      
      unsigned char MessageLength;               
      unsigned char NewDataLength;
     
      cntTransmitMessageBuffer = 0;
      //StartOfMessage
      TransmitMessageBuffer[cntTransmitMessageBuffer++] = 0x80 | ((ToAddress>>28)&0x01);
      //ToAddress         
      TransmitMessageBuffer[cntTransmitMessageBuffer++] = (ToAddress>>21)&0x7F;   //7 bits         
      TransmitMessageBuffer[cntTransmitMessageBuffer++] = (ToAddress>>14)&0x7F;   //7 bits         
      TransmitMessageBuffer[cntTransmitMessageBuffer++] = (ToAddress>> 7)&0x7F;   //7 bits         
      TransmitMessageBuffer[cntTransmitMessageBuffer++] =  ToAddress     &0x7F;   //7 bits         
      //FromAddress         
      TransmitMessageBuffer[cntTransmitMessageBuffer++] = (LocalAddress>>21)&0x7F;   //7 bits         
      TransmitMessageBuffer[cntTransmitMessageBuffer++] = (LocalAddress>>14)&0x7F;   //7 bits         
      TransmitMessageBuffer[cntTransmitMessageBuffer++] = (LocalAddress>> 7)&0x7F;   //7 bits         
      TransmitMessageBuffer[cntTransmitMessageBuffer++] =  LocalAddress     &0x7F;   //7 bits         
      //MessageType         
      TransmitMessageBuffer[cntTransmitMessageBuffer++] = (MessageType>>7)&0x7F;   //7 bits         
      TransmitMessageBuffer[cntTransmitMessageBuffer++] = (MessageType   )&0x7F;   //7 bits         
      //MessageLength
      NewDataLength = Encode8to7bits(Data, DataLength, &TransmitMessageBuffer[cntTransmitMessageBuffer+1]);
      MessageLength = (NewDataLength+PROTOCOL_OVERHEAD);          
      TransmitMessageBuffer[cntTransmitMessageBuffer++] = MessageLength;                       
      
      cntTransmitMessageBuffer += NewDataLength;
      
      //EndOfMessage         
      TransmitMessageBuffer[cntTransmitMessageBuffer++] = 0xFF;                     
      TransmitMessageBufferLength = MessageLength;                  
      
      for (cntByte=cntTransmitMessageBuffer; cntByte<MAX_DNR_CAN_BUFFER_SIZE; cntByte++)         
      {            
         TransmitMessageBuffer[cntByte] = 0x00;         
      }
   }
   
   if (fd>=0)
   {      
      write(fd, TransmitMessageBuffer, TransmitMessageBufferLength);
   }
}
/*
void ProcessSambaNetMessage(unsigned long int ToAddress, unsigned long int FromAddress, unsigned int MessageType, unsigned char *Data, unsigned char DataLength)
{   
   switch (MessageType)   
   {      
      case 0:      
      {  //Address Reservation         
         unsigned char AddressReservationType;         
         unsigned int ReceivedManufacturerID;         
         unsigned int ReceivedProductID;         
         unsigned int ReceivedUniqueMediaAccessID;         
         unsigned long int ReceivedMambaNetAddress;                  
         
         AddressReservationType        = Data[0];         
         ReceivedManufacturerID        = ( ( unsigned int ) Data[1] ) <<8;         
         ReceivedManufacturerID        |=               Data[2];         
         ReceivedProductID             = ( ( unsigned int ) Data[3] ) <<8;         
         ReceivedProductID             |=               Data[4];         
         ReceivedUniqueMediaAccessID   = ( ( unsigned int ) Data[5] ) <<8;         
         ReceivedUniqueMediaAccessID   |=               Data[6];            
         ReceivedMambaNetAddress        = ( ( ( unsigned long int ) Data[ 7] ) <<24 ) &0xFF000000;         
         ReceivedMambaNetAddress       |= ( ( ( unsigned long int ) Data[ 8] ) <<16 ) &0x00FF0000;         
         ReceivedMambaNetAddress       |= ( ( ( unsigned long int ) Data[ 9] ) << 8 ) &0x0000FF00;         
         ReceivedMambaNetAddress       |= ( ( ( unsigned long int ) Data[10] ) ) &0x000000FF;            
         
         if ( ( ReceivedManufacturerID       == ManufacturerID ) &&
              ( ReceivedProductID            == ProductID ) && 
              ( ReceivedUniqueMediaAccessID  == UniqueMediaAccessID ) )
         {  //Reservation message for my Unique ID.            
            if (AddressReservationType == 1)            
            {  //Response to a request.               
               LocalMambaNetAddress = ReceivedMambaNetAddress;            
            }         
         }            
      }      
      break;      
      case 1:      
      {  //Data         
         if (ToAddress == 0x10000000)         
         { //Broadcast            
         }         
         else if (ToAddress == LocalMambaNetAddress)         
         {            
         }      
      }      
      break;   
   }
}
*/
   
class DNREngineThreadPrivate : public QThreadPrivate
{
    Q_DECLARE_PUBLIC(DNREngineThread)
public:
	DNREngineThreadPrivate()
	{}
};

DNREngineThread::DNREngineThread(QObject *parent): QThread(parent)
{
   registerDNRTypes();

   for (int cntChannel=0; cntChannel<MAXNUMBEROFCHANNELS; cntChannel++)
	{
		AxumData.ModuleData[cntChannel].FaderPosition = 0;
		PreviousAxumData.ModuleData[cntChannel].FaderPosition = 0;
	}
   ProcessID = 0;
   LastElapsedTime = 0;
   abort = false;
}

DNREngineThread::~DNREngineThread()
{
   abort = true;
	wait();
}

QObjectList DNREngineThread::GetReceiverList(const char *signal)
{
	return d_func()->receiverList(signal);
}

QStringList DNREngineThread::GetReceivingSlotsList(const char *signal, const char *receiver)
{
	return d_func()->receivingSlotsList(signal, receiver);
}

void DNREngineThread::run()
{
	//counter for thread
	int cntStudio;
	int cntOutput;

	SIGNALLING_STRUCT Signalling;
	SIGNALLING_STRUCT PreviousSignalling;

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

	PreviousSignalling.CUEActive = 0;
	PreviousSignalling.CommActive = 0;
	PreviousSignalling.Redlight1Active = 0;
	PreviousSignalling.Redlight2Active = 0;
	PreviousSignalling.Redlight3Active = 0;
	PreviousSignalling.Redlight4Active = 0;
	PreviousSignalling.Redlight5Active = 0;
	PreviousSignalling.Redlight6Active = 0;
	PreviousSignalling.Redlight7Active = 0;
	PreviousSignalling.Redlight8Active = 0;
	PreviousSignalling.CRMMuteActive = 0;
	PreviousSignalling.CUEMuteActive = 0;
	PreviousSignalling.Studio1MuteActive = 0;
	PreviousSignalling.Studio2MuteActive = 0;
	PreviousSignalling.Studio3MuteActive = 0;
	PreviousSignalling.Studio4MuteActive = 0;
	PreviousSignalling.Studio5MuteActive = 0;
	PreviousSignalling.Studio6MuteActive = 0;
	PreviousSignalling.Studio7MuteActive = 0;
	PreviousSignalling.Studio8MuteActive = 0;
	for (cntStudio=0; cntStudio<9; cntStudio++)
	{
		for (cntOutput=0; cntOutput<3; cntOutput++)
		{
			PreviousSignalling.RelatedOutput[cntStudio][cntOutput] = 0;
		}
	}
   ProcessID = getpid();
         
   serial_struct SerialInformation;              
   struct termios tio;
   int ioctl_arg;
   fd_set readfs;
   
   fd = open("/dev/ttyS1", O_RDWR | O_NOCTTY | O_NDELAY);
   if (fd<0)
   {
   }
   else
   {    
      if (fcntl(fd, F_SETFL, FNDELAY) == -1)
      {
      }
      
      if (tcgetattr(fd, &tio) != 0)
      {
      }
      
      if (cfsetspeed(&tio, B500000) != 0)
      {
      }
      
      tio.c_cflag &= ~CRTSCTS;
      tio.c_cflag |= CLOCAL;
      tio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
      tio.c_oflag &= ~OPOST;
      
      if (tcsetattr(fd, TCSANOW, &tio) != 0)
      {
      }
   
#define N_MAMBANET   16
      ioctl_arg = N_MAMBANET;
      if (ioctl (fd, TIOCSETD, &ioctl_arg) == -1)
      {
         printf("could not change the line discipline to N_MAMBANET.r\n");
      }
/*
      * Ioctl-commands
 */
#define MAMBANET_ENABLE_SIGNALS      0x5301
#define MAMBANET_SETPRIORITY         0x5302

      /* Options for MAMBANET_SETPRIORITY */
#define MAMBANET_MASTER   0
#define MAMBANET_SLAVE    1

      /* Options for MAMBANET_ENABLE_SIGNALS */
#define MAMBANET_SIG_ACK   0x0001
#define MAMBANET_SIG_DATA  0x0002
#define MAMBANET_SIG_ALL   0x000f
#define MAMBANET_SIG_NONE  0x0000
#define MAMBANET_USE_SIGIO 0x1000
      ioctl_arg = MAMBANET_SIG_ALL;
      if (ioctl (fd, MAMBANET_ENABLE_SIGNALS, ioctl_arg) == -1)
      {
         printf("could not enable signals.r\n");
      }
   
      FD_SET(fd, &readfs);       // set testing for source 
   }
   
   int maxfd;
   struct timeval tw;           
   
   maxfd = fd+1;              // maximum bit entry (fd) to test 

   // block until input becomes available
   tw.tv_sec = 0;           
   tw.tv_usec = 1000;
   
   
   forever
   {
		if (abort)
      {
         ::close((int)fd);
			return;
      }
      
      if (fd>=0)
      {
         FD_SET(fd, &readfs);       // set testing for source 
         select(maxfd, &readfs, NULL, NULL, &tw);          
         if (FD_ISSET(fd, &readfs))         // input from source 1 available 
         {        
            int NrOfBytesReceived = read(fd, ReceiveBuffer, 128);
            SerialPortDebugMessage.sprintf("Received %d\n", NrOfBytesReceived);
            
            if (NrOfBytesReceived>0)
            {
               unsigned char Buffer8Bit[99];
               unsigned char Buffer8BitLength;
               unsigned int Level;
               unsigned int MessageType;
               
               MessageType = (((unsigned int)ReceiveBuffer[9])<<7)&0x3F80;
               MessageType |= (ReceiveBuffer[10]&0x7F);
               
               if (MessageType == 1)
               {
                  Buffer8BitLength = Decode7to8bits(&ReceiveBuffer[PROTOCOL_OVERHEAD-1], cntReceiveBuffer-PROTOCOL_OVERHEAD, Buffer8Bit);
                  Level = ((unsigned int)Buffer8Bit[0]<<8) | Buffer8Bit[1];

                  emit FaderLevelChanged(0, Level*4);
               }
            }
         }
      }
      else
      {
         SerialPortDebugMessage.sprintf("fd = %d\n", fd);
      }
      

#ifdef Q_OS_WIN32
		QueryPerformanceCounter(&newTime);
		newNumberOfSeconds = (double)newTime.QuadPart/freq.QuadPart;
#else
	   clock_gettime(CLOCK_MONOTONIC, &newTime);
   	newNumberOfSeconds = newTime.tv_sec+((double)newTime.tv_nsec/1000000000);
#endif

      LastElapsedTime = newNumberOfSeconds-previousNumberOfSeconds;
      previousNumberOfSeconds = newNumberOfSeconds;

//Timers
		//30Hz timers for meter releas
      ElapsedTime = newNumberOfSeconds-previousNumberOfSeconds_ms;
      if (ElapsedTime >= 0.033)
      {
         emit TimerTick(0);
         previousNumberOfSeconds_ms = newNumberOfSeconds;
      }
      //1 Second timer for debug
      ElapsedTime = newNumberOfSeconds-previousNumberOfSeconds_s;
      if (ElapsedTime >= 0.33)
      {
         float level = -20+(((float)rand()*25)/RAND_MAX);
         
         emit MeterChange(level);
         previousNumberOfSeconds_s = newNumberOfSeconds;
      }

		//Start of signalling functions
	   Signalling.CUEActive = 0;
		Signalling.CommActive = 0;
		Signalling.Redlight1Active = 0;
		Signalling.Redlight2Active = 0;
		Signalling.Redlight3Active = 0;
		Signalling.Redlight4Active = 0;
		Signalling.Redlight5Active = 0;
		Signalling.Redlight6Active = 0;
		Signalling.Redlight7Active = 0;
		Signalling.Redlight8Active = 0;
	   Signalling.CRMMuteActive = 0;
   	Signalling.CUEMuteActive = 0;
	   Signalling.Studio1MuteActive = 0;
   	Signalling.Studio2MuteActive = 0;
	   Signalling.Studio3MuteActive = 0;
   	Signalling.Studio4MuteActive = 0;
	   Signalling.Studio5MuteActive = 0;
   	Signalling.Studio6MuteActive = 0;
	   Signalling.Studio7MuteActive = 0;
   	Signalling.Studio8MuteActive = 0;
	   for (cntStudio=0; cntStudio<9; cntStudio++)
   	{
      	for (cntOutput=0; cntOutput<3; cntOutput++)
	      {
         	Signalling.RelatedOutput[cntStudio][cntOutput] = 0;
      	}
   	}

      for (int cntChannel=0; cntChannel<MAXNUMBEROFCHANNELS; cntChannel++)
      {
         if (PreviousAxumData.ModuleData[cntChannel].FaderPosition != AxumData.ModuleData[cntChannel].FaderPosition)
         {
				if (AxumData.ModuleData[cntChannel].FaderPosition > FADER_CLOSE_LEVEL)
				{
					if (AxumData.ModuleData[cntChannel].Settings.Redlight1)
					{
						Signalling.Redlight1Active = 1;
					}
				}
            emit FaderLevelChanged(cntChannel, AxumData.ModuleData[cntChannel].FaderPosition);
            PreviousAxumData.ModuleData[cntChannel].FaderPosition = AxumData.ModuleData[cntChannel].FaderPosition;
         }

         if (PreviousAxumData.ModuleData[cntChannel].EQBand1.Level != AxumData.ModuleData[cntChannel].EQBand1.Level)
         {
            emit EQBand1LevelChanged(cntChannel, AxumData.ModuleData[cntChannel].EQBand1.Level);
            PreviousAxumData.ModuleData[cntChannel].EQBand1.Level = AxumData.ModuleData[cntChannel].EQBand1.Level;
         }

         if (PreviousAxumData.ModuleData[cntChannel].EQBand2.Level != AxumData.ModuleData[cntChannel].EQBand2.Level)
         {
            emit EQBand2LevelChanged(cntChannel, AxumData.ModuleData[cntChannel].EQBand2.Level);
            PreviousAxumData.ModuleData[cntChannel].EQBand2.Level = AxumData.ModuleData[cntChannel].EQBand2.Level;
         }

         if (PreviousAxumData.ModuleData[cntChannel].EQBand3.Level != AxumData.ModuleData[cntChannel].EQBand3.Level)
         {
            emit EQBand3LevelChanged(cntChannel, AxumData.ModuleData[cntChannel].EQBand3.Level);
            PreviousAxumData.ModuleData[cntChannel].EQBand3.Level = AxumData.ModuleData[cntChannel].EQBand3.Level;
         }

         if (PreviousAxumData.ModuleData[cntChannel].EQBand4.Level != AxumData.ModuleData[cntChannel].EQBand4.Level)
         {
            emit EQBand4LevelChanged(cntChannel, AxumData.ModuleData[cntChannel].EQBand4.Level);
            PreviousAxumData.ModuleData[cntChannel].EQBand4.Level = AxumData.ModuleData[cntChannel].EQBand4.Level;
         }
      }

      if (PreviousSignalling.Redlight1Active != Signalling.Redlight1Active)
      {
			if (Signalling.Redlight1Active)
			{
				emit Redlight1Changed(POSITION_RESOLUTION);
			}
			else
			{
				emit Redlight1Changed(0);
			}
			PreviousSignalling.Redlight1Active = Signalling.Redlight1Active;
		}
      usleep(100);
   }
}

void DNREngineThread::doBand1EQPositionChange(int_number ChannelNr, double_position Position)
{
	AxumData.ModuleData[ChannelNr].EQBand1.Level = Position;
}

void DNREngineThread::doBand2EQPositionChange(int_number ChannelNr, double_position Position)
{
	AxumData.ModuleData[ChannelNr].EQBand2.Level = Position;
}

void DNREngineThread::doBand3EQPositionChange(int_number ChannelNr, double_position Position)
{
	AxumData.ModuleData[ChannelNr].EQBand3.Level = Position;
}

void DNREngineThread::doBand4EQPositionChange(int_number ChannelNr, double_position Position)
{
	AxumData.ModuleData[ChannelNr].EQBand4.Level = Position;
}

void DNREngineThread::doFaderChange(int_number ChannelNr, double_position Position)
{
	AxumData.ModuleData[ChannelNr].FaderPosition = Position;

   if (ChannelNr == 0)
   {
      unsigned char TransmitBuffer[2];
      unsigned int PositionToTransmit;
      
      PositionToTransmit = Position/4;
      
      TransmitBuffer[0] = (PositionToTransmit>>8)&0xFF;
      TransmitBuffer[1] = PositionToTransmit&0xFF;
      
      SendSerialMessage(0x00000002, 1, TransmitBuffer, 2);
   }
}

void DNREngineThread::doRedlight1SettingChange(int_number ChannelNr, double_position Setting)
{
	if (Setting == 0)
	{
		AxumData.ModuleData[ChannelNr].Settings.Redlight1 = 0;
	}
	else
	{
		AxumData.ModuleData[ChannelNr].Settings.Redlight1 = 1;
	}
}

int DNREngineThread::getProcessID()
{
   return ProcessID;
}

