/*****************************************************
 *                                                   *
 * The dsp functions uses an internal mutex locking. *
 * This is the reason its not allowed to use these   *
 * functions in a 'signal handler'.                  *
 * You also may not use the dsp_lock functions/mutex *
 * outside these dsp_functions                       *
 *                                                   *
 *****************************************************/

#include "common.h"
#include "engine.h"
#include "dsp.h"
#include "ddpci2040.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

#define EEPROM_DELAY_TIME   100

//#define LOG_DEBUG_ENABLED

#ifdef LOG_DEBUG_ENABLED
  #define LOG_DEBUG(...) log_write(__VA_ARGS__)
#else
  #define LOG_DEBUG(...)
#endif

unsigned int ModuleDSPEntryPoint                = 0x00000000;
unsigned int ModuleDSPRoutingFrom               = 0x00000000;
unsigned int ModuleDSPUpdate_InputGainFactor    = 0x00000000;
unsigned int ModuleDSPUpdate_LevelFactor        = 0x00000000;
unsigned int ModuleDSPFilterCoefficients        = 0x00000000;
unsigned int ModuleDSPEQCoefficients            = 0x00000000;
unsigned int ModuleDSPDynamicsOriginalFactor    = 0x00000000;
unsigned int ModuleDSPDynamicsProcessedFactor   = 0x00000000;
unsigned int ModuleDSPMeterPPM                  = 0x00000000;
unsigned int ModuleDSPMeterVU                   = 0x00000000;
unsigned int ModuleDSPPhaseRMS                  = 0x00000000;
unsigned int ModuleDSPSmoothFactor              = 0x00000000;
unsigned int ModuleDSPPPMReleaseFactor          = 0x00000000;
unsigned int ModuleDSPVUReleaseFactor           = 0x00000000;
unsigned int ModuleDSPRMSReleaseFactor          = 0x00000000;

unsigned int SummingDSPEntryPoint               = 0x00000000;
unsigned int SummingDSPUpdate_MatrixFactor      = 0x00000000;
unsigned int SummingDSPBussMeterPPM             = 0x00000000;
unsigned int SummingDSPBussMeterVU              = 0x00000000;
unsigned int SummingDSPPhaseRMS                 = 0x00000000;
unsigned int SummingDSPSelectedMixMinusBuss     = 0x00000000;
unsigned int SummingDSPSmoothFactor             = 0x00000000;
unsigned int SummingDSPVUReleaseFactor          = 0x00000000;
unsigned int SummingDSPPhaseRelease             = 0x00000000;

unsigned int FXDSPEntryPoint                    = 0x00000000;

pthread_mutex_t dsp_mutex = PTHREAD_MUTEX_INITIALIZER;

int dsp_init(char *devname, DSPCARD_STRUCT *dsp_card);
bool dsp_program_eeprom(int fd);
void dsp_lock(int l);

DSP_HANDLER_STRUCT *dsp_open()
{
  int cntDSPCard;

  DSP_HANDLER_STRUCT *dsp_handler;
  LOG_DEBUG("[%s] enter", __func__);

  dsp_handler = (DSP_HANDLER_STRUCT *) calloc(1, sizeof(DSP_HANDLER_STRUCT));
  if (dsp_handler == NULL)
  {
    fprintf(stderr, "Couldn't allocate memory for 'dsp_handler'");
    return NULL;
  }

  cntDSPCard = 0;
  if (dsp_init((char *)"/dev/dsp0", &dsp_handler->dspcard[cntDSPCard]))
  {
    dsp_handler->dspcard[cntDSPCard].slot = 0;
    cntDSPCard++;
  }
  if (dsp_init((char *)"/dev/dsp1", &dsp_handler->dspcard[cntDSPCard]))
  {
    dsp_handler->dspcard[cntDSPCard].slot = 1;
    cntDSPCard++;
  }
  if (dsp_init((char *)"/dev/dsp2", &dsp_handler->dspcard[cntDSPCard]))
  {
    dsp_handler->dspcard[cntDSPCard].slot = 2;
    cntDSPCard++;
  }
  if (dsp_init((char *)"/dev/dsp3", &dsp_handler->dspcard[cntDSPCard]))
  {
    dsp_handler->dspcard[cntDSPCard].slot = 3;
    cntDSPCard++;
  }

  if (!cntDSPCard)
  {
    fprintf(stderr, "No programmed DSP card found.\n");
    return NULL;
  }
  log_write("%d DSP card(s) found", cntDSPCard);

  LOG_DEBUG("[%s] leave", __func__);

  return dsp_handler;
}

int dsp_init(char *devname, DSPCARD_STRUCT *dspcard)
{
  int fd;
  LOG_DEBUG("[%s] enter", __func__);

  fd = open(devname, O_RDWR);
  if (fd<0)
  {
    return 0;
  }
  log_write("PCI card %s opened, fd=%d", devname, fd);

  pci2040_ioctl_linux pci2040_ioctl_message;
  PCI2040_DUMP_CONFIG_REGS HPIConfigurationRegisters;

  pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_DUMP_CONFIGURATION;
  pci2040_ioctl_message.BufferLength = sizeof(PCI2040_DUMP_CONFIG_REGS);
  pci2040_ioctl_message.Buffer = (unsigned char *)&HPIConfigurationRegisters;
  ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);

  if ((!(HPIConfigurationRegisters.MiscControl&0x20)) || (HPIConfigurationRegisters.GPBBar == 0x00000000))
  {
    //Misc control
    HPIConfigurationRegisters.MiscControl |= 0x20;
    // GP Select register
    HPIConfigurationRegisters.GPBSelect = 0x00;
    // GP Direction Control
    HPIConfigurationRegisters.GPBDataDir |= 0x0B;
    // GP OutputData
    HPIConfigurationRegisters.GPBOutData |= 0x03;
    // GP IntType
    HPIConfigurationRegisters.GPBIntType = 0x00;

    // Hot Swap LED (for Check of functions only)
    // PCIRegisters.HSCSR |= 0x08;

    //This does not function
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_STORE_CONFIGURATION;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_DUMP_CONFIG_REGS);
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);

    if (!dsp_program_eeprom(fd))
    {
      fprintf(stderr, "PCI card (%s) not initialized by EEPROM !\nThe card is initialized!\nEEPROM programming failed\n", devname);
      return 0;
    }
    else
    {
      fprintf(stderr, "PCI card (%s) not initialized by EEPROM !\nThe card is initialized and the EEPROM programmed!\n", devname);
      return 0;
    }
  }
  else if ((HPIConfigurationRegisters.MiscControl&0xC000)==0xC000)
  {
    fprintf(stderr, "PCI card (%s) is initialized but the EEPROM gives an error!\nThe EEPROM is NOT programmed !\n", devname);
    return 0;
  }
  else
  {
    PCI2040_RESOURCE res;

    //Get PCI_resources
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_HPI_CSR_SPACES;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_RESOURCE);
    pci2040_ioctl_message.Buffer = (unsigned char *)&res;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    unsigned long *PtrHPI_CSR = (unsigned long *)mmap(0, res.Length, PROT_READ|PROT_WRITE, MAP_SHARED | MAP_LOCKED, fd, res.PhysicalAddress);

    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_DSP_HPI_SPACES;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_RESOURCE);
    pci2040_ioctl_message.Buffer = (unsigned char *)&res;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    unsigned long *PtrDSP_HPI = (unsigned long *)mmap(0, res.Length, PROT_READ|PROT_WRITE, MAP_SHARED | MAP_LOCKED, fd, res.PhysicalAddress);

    //pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_GPB_TBC_SPACES;
    //pci2040_ioctl_message.BufferLength = sizeof(PCI2040_RESOURCE);
    //pci2040_ioctl_message.Buffer = (unsigned char *)&res;
    //ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    //unsigned long *PtrGPB_TBC = (unsigned long *)mmap(0, res.Length, PROT_READ|PROT_WRITE, MAP_SHARED | MAP_LOCKED, fd, res.PhysicalAddress);

    dsp_lock(1);
    unsigned long cntPtrAddress = (unsigned long)PtrDSP_HPI;
    for (int cntDSP=0; cntDSP<4; cntDSP++)
    {
      dspcard->dsp_regs[cntDSP].HPIC = (unsigned long *)cntPtrAddress;
      cntPtrAddress += 0x800;
      dspcard->dsp_regs[cntDSP].HPID_Inc = (unsigned long *)cntPtrAddress;

      cntPtrAddress += 0x800;
      dspcard->dsp_regs[cntDSP].HPIA = (unsigned long *)cntPtrAddress;
      cntPtrAddress += 0x800;
      dspcard->dsp_regs[cntDSP].HPID = (unsigned long *)cntPtrAddress;
      cntPtrAddress += 0x800;
    }
    dsp_lock(0);

//GPIO5 = 1, so HCS1-4 are disconnected
//         pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_DUMP_CONFIGURATION;
//         pci2040_ioctl_message.BufferLength = sizeof(PCI2040_DUMP_CONFIG_REGS);
//         ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);

    HPIConfigurationRegisters.GPBSelect = 0x00;
    HPIConfigurationRegisters.GPBDataDir |= 0x08;
    HPIConfigurationRegisters.GPBOutData |= 0x08;

    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_STORE_CONFIGURATION;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_DUMP_CONFIG_REGS);
    pci2040_ioctl_message.Buffer = (unsigned char *)&HPIConfigurationRegisters;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);

    //Setup the PCI2040 CSR
    dsp_lock(1);
    // HPI Reset
    *((unsigned long *)((unsigned long)PtrHPI_CSR+0x14)) |= 0x0000000F;

    // Int Mask Register(SET)
    //*((unsigned long *)((unsigned long)PtrHPI_CSR+0x08)) = 0xFC00000F;
    *((unsigned long *)((unsigned long)PtrHPI_CSR+0x08)) = 0x00000000;

    // Int Mask Register(CLEAR)
    *((unsigned long *)((unsigned long)PtrHPI_CSR+0x0C)) = 0x00000000;

    // DSP implementation
    *((unsigned long *)((unsigned long)PtrHPI_CSR+0x16)) &= 0xFFFFFFF0;
    *((unsigned long *)((unsigned long)PtrHPI_CSR+0x16)) |= 0x0000000F;

    // HPI Datawidth
    *((unsigned long *)((unsigned long)PtrHPI_CSR+0x18)) |= 0x0000000F;

    // HPI UnReset
    *((unsigned long *)((unsigned long)PtrHPI_CSR+0x14)) &= 0xFFFFFFF0;
    dsp_lock(0);

    delay_ms(1);
    HPIConfigurationRegisters.GPBSelect = 0x00;
    HPIConfigurationRegisters.GPBDataDir |= 0x08;
    HPIConfigurationRegisters.GPBOutData &= 0xF7;

    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_STORE_CONFIGURATION;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_DUMP_CONFIG_REGS);
    pci2040_ioctl_message.Buffer = (unsigned char *)&HPIConfigurationRegisters;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);

    delay_ms(500);
    delay_ms(500);
    delay_ms(500);

    //default most significant is send first, we do both
    dsp_lock(1);
    *dspcard->dsp_regs[0].HPIC |= 0x00010001;
    *dspcard->dsp_regs[1].HPIC |= 0x00010001;
    *dspcard->dsp_regs[2].HPIC |= 0x00010001;
    *dspcard->dsp_regs[3].HPIC |= 0x00010001;
    dsp_lock(0);

    //file descriptor not used further
    close(fd);
    log_write("Closed fd %d", fd);

//-------------------------------------------------------------------------
//Load DSP1&2 with module firmware
//-------------------------------------------------------------------------
    FILE *DSPMappings = fopen("/var/lib/axum/dsp/AxumModule.map", "r");
    if (DSPMappings == NULL)
    {
      fprintf(stderr, "Module DSP Mappings open error\n");
      return 0;
    }
    else
    {
      int readedbytes;
      size_t len = 0;
      char *line = NULL;
      unsigned long int MappingAddress;
      char MappingVariableName[256];

      while ((readedbytes = getline(&line, &len, DSPMappings)) != -1)
      {
        memset(MappingVariableName, 0, 256);
        int ItemsConverted = sscanf(line, "%lx%s", &MappingAddress, MappingVariableName);
        if (ItemsConverted != EOF)
        {
          if (ItemsConverted != 0)
          {
            if (strcmp(MappingVariableName, "_c_int00") == 0)
            {
              ModuleDSPEntryPoint = MappingAddress;
            }
            else if (strcmp(MappingVariableName, "_RoutingFrom") == 0)
            {
              ModuleDSPRoutingFrom = MappingAddress;
            }
            else if (strcmp(MappingVariableName, "_Update_InputGainFactor") == 0)
            {
              ModuleDSPUpdate_InputGainFactor = MappingAddress;
            }
            else if (strcmp(MappingVariableName, "_Update_LevelFactor") == 0)
            {
              ModuleDSPUpdate_LevelFactor = MappingAddress;
            }
            else if (strcmp(MappingVariableName, "_EQCoefficients") == 0)
            {
              ModuleDSPEQCoefficients = MappingAddress;
            }
            else if (strcmp(MappingVariableName, "_FilterCoefficients") == 0)
            {
              ModuleDSPFilterCoefficients = MappingAddress;
            }
            else if (strcmp(MappingVariableName, "_DynamicsOriginalFactor") == 0)
            {
              ModuleDSPDynamicsOriginalFactor = MappingAddress;
            }
            else if (strcmp(MappingVariableName, "_DynamicsProcessedFactor") == 0)
            {
              ModuleDSPDynamicsProcessedFactor = MappingAddress;
            }
            else if (strcmp(MappingVariableName, "_MeterPPM") == 0)
            {
              ModuleDSPMeterPPM = MappingAddress;
            }
            else if (strcmp(MappingVariableName, "_MeterVU") == 0)
            {
              ModuleDSPMeterVU = MappingAddress;
            }
            else if (strcmp(MappingVariableName, "_PhaseRMS") == 0)
            {
              ModuleDSPPhaseRMS = MappingAddress;
            }
            else if (strcmp(MappingVariableName, "_SmoothFactor") == 0)
            {
              ModuleDSPSmoothFactor = MappingAddress;
            }
            else if (strcmp(MappingVariableName, "_PPMReleaseFactor") == 0)
            {
              ModuleDSPPPMReleaseFactor = MappingAddress;
            }
            else if (strcmp(MappingVariableName, "_VUReleaseFactor") == 0)
            {
              ModuleDSPVUReleaseFactor = MappingAddress;
            }
            else if (strcmp(MappingVariableName, "_RMSReleaseFactor") == 0)
            {
              ModuleDSPRMSReleaseFactor = MappingAddress;
            }
          }
        }
        if (line)
        {
          free(line);
          line = NULL;
        }
      }
      fclose(DSPMappings);
    }

    int DSPFirmware = open("/var/lib/axum/dsp/AxumModule.b0", O_RDONLY);
    if (DSPFirmware<0)
    {
      fprintf(stderr, "Module DSP Firmware open error\n");
      return 0;
    }
    else
    {
      unsigned char ReadBuf[4];
      unsigned long *PtrData = (unsigned long *)ReadBuf;
      unsigned long cntAddress;

      cntAddress = 0;
      //read(DSPFirmware, ReadBuf, 4);
      while (read(DSPFirmware, ReadBuf, 4) == 4)
      {
        for (int cntDSP=0; cntDSP<2; cntDSP++)
        {
          dsp_lock(1);
          *dspcard->dsp_regs[cntDSP].HPIA = 0x10001c00+cntAddress;
          *dspcard->dsp_regs[cntDSP].HPID = *PtrData;
          dsp_lock(0);
        }
        cntAddress+=4;
      }
      close(DSPFirmware);

      //Set in bootloader the entry point and let it GO!
      for (int cntDSP=0; cntDSP<2; cntDSP++)
      {
        //Entry point
        dsp_lock(1);
        *dspcard->dsp_regs[cntDSP].HPIA = 0x10000714;
        *dspcard->dsp_regs[cntDSP].HPID = ModuleDSPEntryPoint;
        dsp_lock(0);
      }
    }

//-------------------------------------------------------------------------
//Load DSP3 with summing firmware
//-------------------------------------------------------------------------
    DSPMappings = fopen("/var/lib/axum/dsp/AxumSumming.map", "r");
    if (DSPMappings == NULL)
    {
      fprintf(stderr, "Summing DSP Mappings open error.\n");
      return 0;
    }
    else
    {
      int readedbytes;
      size_t len = 0;
      char *line = NULL;
      unsigned long int MappingAddress;
      char MappingVariableName[256];

      while ((readedbytes = getline(&line, &len, DSPMappings)) != -1)
      {
        memset(MappingVariableName, 0, 256);
        int ItemsConverted = sscanf(line, "%lx%s", &MappingAddress, MappingVariableName);
        if (ItemsConverted != EOF)
        {
          if (ItemsConverted != 0)
          {
            if (strcmp(MappingVariableName, "_c_int00") == 0)
            {
              SummingDSPEntryPoint = MappingAddress;
            }
            else if (strcmp(MappingVariableName, "_Update_MatrixFactor") == 0)
            {
              SummingDSPUpdate_MatrixFactor = MappingAddress;
            }
            else if (strcmp(MappingVariableName, "_BussMeterPPM") == 0)
            {
              SummingDSPBussMeterPPM = MappingAddress;
            }
            else if (strcmp(MappingVariableName, "_BussMeterVU") == 0)
            {
              SummingDSPBussMeterVU = MappingAddress;
            }
            else if (strcmp(MappingVariableName, "_PhaseRMS") == 0)
            {
              SummingDSPPhaseRMS = MappingAddress;
            }
            else if (strcmp(MappingVariableName, "_SelectedMixMinusBuss") == 0)
            {
              SummingDSPSelectedMixMinusBuss = MappingAddress;
            }
            else if (strcmp(MappingVariableName, "_SmoothFactor") == 0)
            {
              SummingDSPSmoothFactor = MappingAddress;
            }
            else if (strcmp(MappingVariableName, "_VUReleaseFactor") == 0)
            {
              SummingDSPVUReleaseFactor = MappingAddress;
            }
            else if (strcmp(MappingVariableName, "_PhaseRelease") == 0)
            {
              SummingDSPPhaseRelease = MappingAddress;
            }
          }
        }
        if (line)
        {
          free(line);
          line = NULL;
        }
      }
      fclose(DSPMappings);
    }

    DSPFirmware = open("/var/lib/axum/dsp/AxumSumming.b0", O_RDONLY);
    if (DSPFirmware<0)
    {
      fprintf(stderr, "Summing DSP Firmware open error.\n");
      return 0;
    }
    else
    {
      unsigned char ReadBuf[4];
      unsigned long *PtrData = (unsigned long *)ReadBuf;
      unsigned long cntAddress;

      cntAddress = 0;
      while (read(DSPFirmware, ReadBuf, 4) == 4)
      {
        for (int cntDSP=2; cntDSP<3; cntDSP++)
        {
          dsp_lock(1);
          *dspcard->dsp_regs[cntDSP].HPIA = 0x10001c00+cntAddress;
          *dspcard->dsp_regs[cntDSP].HPID = *PtrData;
          dsp_lock(0);
        }
        cntAddress+=4;
      }
      close(DSPFirmware);

      //Set in bootloader the entry point and let it GO!
      for (int cntDSP=2; cntDSP<3; cntDSP++)
      {
        //Entry point
        dsp_lock(1);
        *dspcard->dsp_regs[cntDSP].HPIA = 0x10000714;
        *dspcard->dsp_regs[cntDSP].HPID = SummingDSPEntryPoint;
        dsp_lock(0);
      }
    }

//-------------------------------------------------------------------------
//Load DSP4 with FX firmware
//-------------------------------------------------------------------------
    DSPMappings = fopen("/var/lib/axum/dsp/AxumFX1.map", "r");
    if (DSPMappings == NULL)
    {
      fprintf(stderr, "FX DSP Mappings open error.\n");
      return 0;
    }
    else
    {
      int readedbytes;
      size_t len = 0;
      char *line = NULL;
      unsigned long int MappingAddress;
      char MappingVariableName[256];

      while ((readedbytes = getline(&line, &len, DSPMappings)) != -1)
      {
        memset(MappingVariableName, 0, 256);
        int ItemsConverted = sscanf(line, "%lx%s", &MappingAddress, MappingVariableName);
        if (ItemsConverted != EOF)
        {
          if (ItemsConverted != 0)
          {
            if (strcmp(MappingVariableName, "_c_int00") == 0)
            {
              FXDSPEntryPoint = MappingAddress;
            }
          }
        }
        if (line)
        {
          free(line);
          line = NULL;
        }
      }
      fclose(DSPMappings);
    }

    DSPFirmware = open("/var/lib/axum/dsp/AxumFX1.b0", O_RDONLY);
    if (DSPFirmware<0)
    {
      fprintf(stderr, "FX DSP Firmware open error\n");
      return 0;
    }
    else
    {
      unsigned char ReadBuf[4];
      unsigned long *PtrData = (unsigned long *)ReadBuf;
      unsigned long cntAddress;

      cntAddress = 0;
      while (read(DSPFirmware, ReadBuf, 4) == 4)
      {
        for (int cntDSP=3; cntDSP<4; cntDSP++)
        {
          dsp_lock(1);
          *dspcard->dsp_regs[cntDSP].HPIA = 0x10001c00+cntAddress;
          *dspcard->dsp_regs[cntDSP].HPID = *PtrData;
          dsp_lock(0);
        }
        cntAddress+=4;
      }
      close(DSPFirmware);

      //Set in bootloader the entry point and let it GO!
      for (int cntDSP=3; cntDSP<4; cntDSP++)
      {
        //Entry point
        dsp_lock(1);
        *dspcard->dsp_regs[cntDSP].HPIA = 0x10000714;
        *dspcard->dsp_regs[cntDSP].HPID = FXDSPEntryPoint;
        dsp_lock(0);
      }
    }

    for (int cntDSP=0; cntDSP<4; cntDSP++)
    {
      //Run
      dsp_lock(1);
      *dspcard->dsp_regs[cntDSP].HPIA = 0x10000718;
      *dspcard->dsp_regs[cntDSP].HPID = 0x00000001;
      dsp_lock(0);
    }
  }

//**************************************************************/
//End Setup DSP(s)
//**************************************************************/
  delay_ms(1);

  //default most significant is send first, we do both
  dsp_lock(1);
  *dspcard->dsp_regs[0].HPIC |= 0x00010001;
  *dspcard->dsp_regs[1].HPIC |= 0x00010001;
  *dspcard->dsp_regs[2].HPIC |= 0x00010001;
  *dspcard->dsp_regs[3].HPIC |= 0x00010001;
  dsp_lock(0);

  //initialize DSPs after DSP are completely booted!
  delay_ms(50);

  LOG_DEBUG("[%s] leave", __func__);
  return 1;
}

void dsp_close(DSP_HANDLER_STRUCT *dsp_handler)
{
  LOG_DEBUG("[%s] enter", __func__);
  free(dsp_handler);
  LOG_DEBUG("[%s] leave", __func__);
}

int dsp_force_eeprom_prg(char *devname)
{
  int fd;

  LOG_DEBUG("[%s] enter", __func__);
  fd = open(devname, O_RDWR);
  if (fd<0)
  {
    LOG_DEBUG("[%s] leave with error", __func__);
    return 0;
  }
  dsp_program_eeprom(fd);
  LOG_DEBUG("[%s] leave", __func__);
  return 1;
}


bool dsp_program_eeprom(int fd)
{
  LOG_DEBUG("[%s] enter", __func__);
  struct
  {
    unsigned char SubClass;
    unsigned char BaseClass;
    unsigned char SubSysByte0;
    unsigned char SubSysByte1;
    unsigned char SubSysByte2;
    unsigned char SubSysByte3;
    unsigned char GPIOSelect;
    unsigned char RSVD_0;
    unsigned char RSVD_1;
    unsigned char MiscCtrlByte0;
    unsigned char MiscCtrlByte1;
    unsigned char Diagnostic;
    unsigned char HPI_ImpByte0;
    unsigned char RSVD_2;
    unsigned char HPI_DWByte0;
    unsigned char RSVD_3;
    unsigned char RSVD[16];
  } EEpromRegisters;

  EEpromRegisters.SubClass = 0x80;
  EEpromRegisters.BaseClass = 0x06;
  EEpromRegisters.SubSysByte0 = 0x00;
  EEpromRegisters.SubSysByte1 = 0x00;
  EEpromRegisters.SubSysByte2 = 0x00;
  EEpromRegisters.SubSysByte3 = 0x00;
  EEpromRegisters.GPIOSelect = 0x3C;
  EEpromRegisters.RSVD_0 = 0x00;
  EEpromRegisters.RSVD_1 = 0x00;
  EEpromRegisters.MiscCtrlByte0 = 0x27;
  EEpromRegisters.MiscCtrlByte1 = 0x00;
  EEpromRegisters.Diagnostic = 0x00;
  EEpromRegisters.HPI_ImpByte0 = 0x0F; //(Axum-RACK-DSP is 0x0F)
  EEpromRegisters.RSVD_2 = 0x00;
  EEpromRegisters.HPI_DWByte0 = 0x0F;
  EEpromRegisters.RSVD_3 = 0x00;
  for (int cnt=0; cnt<16; cnt++)
  {
    EEpromRegisters.RSVD[cnt] = 0x00;
  }

  dsp_lock(1);
  pci2040_ioctl_linux pci2040_ioctl_message;
  PCI2040_WRITE_REG  reg;

  reg.Regoff = ENUM_GPBOUTDATA_OFFSET;
  //Clock High & Data High
  reg.DataVal = 0x03;

  printf("EEPROM delay time: %d uS\n", EEPROM_DELAY_TIME);

  pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
  pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
  pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
  ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
  delay_us(EEPROM_DELAY_TIME);

  ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);

  //ByteWrite to EEPROM
  for (unsigned char cntByte=0; cntByte<32; cntByte++)
  {
    printf("Write byte %d: 0x%02X\n", cntByte, ((unsigned char *)&EEpromRegisters.SubClass)[cntByte]);
    //StartCondition (Data goes Low (bit 1) & Clock stays high (bit 0))
    reg.DataVal &= 0xFD;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(EEPROM_DELAY_TIME);

    //I2C Address
    for (int cntBit=0; cntBit<8; cntBit++)
    {
      unsigned char Mask = 0x80>>cntBit;

      //Clock Low
      reg.DataVal &= 0xFE;
      pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
      pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
      pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
      ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
      delay_us(EEPROM_DELAY_TIME);

      //Set I2C Address
      if (0xA0 & Mask)
        reg.DataVal |= 0x02;
      else
        reg.DataVal &= 0xFD;

      pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
      pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
      pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
      ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
      delay_us(EEPROM_DELAY_TIME);

      //Clock High
      reg.DataVal |= 0x01;
      pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
      pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
      pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
      ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
      delay_us(EEPROM_DELAY_TIME);
    }
    //Acknowledge
    //Clock Low & Data sould be an input
    reg.DataVal &= 0xFE;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(EEPROM_DELAY_TIME);

    //Change data for input
    reg.Regoff = ENUM_GPBDATADIR_OFFSET;
    reg.DataVal = 0x01;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(EEPROM_DELAY_TIME);

    reg.Regoff = ENUM_GPBOUTDATA_OFFSET;

    //Clock High
    reg.DataVal |= 0x01;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(EEPROM_DELAY_TIME);

    //Clock Low
    reg.DataVal &= 0xFE;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(EEPROM_DELAY_TIME);

    //Change Data for Output
    reg.Regoff = ENUM_GPBDATADIR_OFFSET;
    reg.DataVal = 0x03;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(EEPROM_DELAY_TIME);

    reg.Regoff = ENUM_GPBOUTDATA_OFFSET;
    //EEProm Address
    for (int cntBit=0; cntBit<8; cntBit++)
    {
      unsigned char Mask = 0x80>>cntBit;

      //Clock Low
      reg.DataVal &= 0xFE;
      pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
      pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
      pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
      ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
      delay_us(EEPROM_DELAY_TIME);

      //Set EEPROM Address
      if (cntByte & Mask)
        reg.DataVal |= 0x02;
      else
        reg.DataVal &= 0xFD;
      pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
      pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
      pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
      ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
      delay_us(EEPROM_DELAY_TIME);

      //Clock High
      reg.DataVal |= 0x01;
      pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
      pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
      pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
      ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
      delay_us(EEPROM_DELAY_TIME);
    }
    //Acknowledge
    //Clock Low & Data sould be an input
    reg.DataVal &= 0xFE;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(EEPROM_DELAY_TIME);

    //Change data for input
    reg.Regoff = ENUM_GPBDATADIR_OFFSET;
    reg.DataVal = 0x01;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(EEPROM_DELAY_TIME);

    reg.Regoff = ENUM_GPBOUTDATA_OFFSET;
    //Clock High
    reg.DataVal |= 0x01;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(EEPROM_DELAY_TIME);

    //Clock Low
    reg.DataVal &= 0xFE;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(EEPROM_DELAY_TIME);

    //Change Data for Output
    reg.Regoff = ENUM_GPBDATADIR_OFFSET;
    reg.DataVal = 0x03;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(EEPROM_DELAY_TIME);

    reg.Regoff = ENUM_GPBOUTDATA_OFFSET;
    //EEProm Data
    for (int cntBit=0; cntBit<8; cntBit++)
    {
      unsigned char Mask = 0x80>>cntBit;

      //Clock Low
      reg.DataVal &= 0xFE;
      pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
      pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
      pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
      ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
      delay_us(EEPROM_DELAY_TIME);

      //Set EEPROM Data
      if (((unsigned char *)&EEpromRegisters.SubClass)[cntByte] & Mask)
        reg.DataVal |= 0x02;
      else
        reg.DataVal &= 0xFD;
      pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
      pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
      pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
      ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
      delay_us(EEPROM_DELAY_TIME);

      //Clock High
      reg.DataVal |= 0x01;
      pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
      pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
      pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
      ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
      delay_us(EEPROM_DELAY_TIME);
    }
    //Acknowledge
    //Clock Low & Data sould be an input
    reg.DataVal &= 0xFE;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(EEPROM_DELAY_TIME);

    //Change data for input
    reg.Regoff = ENUM_GPBDATADIR_OFFSET;
    reg.DataVal = 0x01;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(EEPROM_DELAY_TIME);

    reg.Regoff = ENUM_GPBOUTDATA_OFFSET;
    //Clock High
    reg.DataVal |= 0x01;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;

    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(EEPROM_DELAY_TIME);

    //Clock Low
    reg.DataVal &= 0xFE;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(EEPROM_DELAY_TIME);

    //Change Data for Output
    reg.Regoff = ENUM_GPBDATADIR_OFFSET;
    reg.DataVal = 0x03;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(EEPROM_DELAY_TIME);

    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(EEPROM_DELAY_TIME);

    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(EEPROM_DELAY_TIME);

    reg.Regoff = ENUM_GPBOUTDATA_OFFSET;
    //Stopbit

    //Data Low
    reg.DataVal &= 0xFD;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(EEPROM_DELAY_TIME);

    //Data High
    reg.DataVal |= 0x02;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(EEPROM_DELAY_TIME);

    //Clock High
    reg.DataVal |= 0x01;
    pci2040_ioctl_message.FunctionNr = IOCTL_PCI2040_WRITE_PCI_REG;
    pci2040_ioctl_message.BufferLength = sizeof(PCI2040_WRITE_REG);
    pci2040_ioctl_message.Buffer = (unsigned char *)&reg;
    ioctl(fd, PCI2040_IOCTL_LINUX, &pci2040_ioctl_message);
    delay_us(EEPROM_DELAY_TIME);
  }
  dsp_lock(0);
  LOG_DEBUG("[%s] leave", __func__);
  return true;
}

int dsp_card_available(DSP_HANDLER_STRUCT *dsp_handler, unsigned char CardNr)
{
  LOG_DEBUG("[%s] enter", __func__);
  dsp_lock(1);
  if (CardNr<4)
  {
    if ((dsp_handler->dspcard[CardNr].dsp_regs[0].HPIA != NULL) &&
        (dsp_handler->dspcard[CardNr].dsp_regs[1].HPIA != NULL) &&
        (dsp_handler->dspcard[CardNr].dsp_regs[2].HPIA != NULL) &&
        (dsp_handler->dspcard[CardNr].dsp_regs[3].HPIA != NULL))
    {
      dsp_lock(0);
      LOG_DEBUG("[%s] leave", __func__);
      return 1;
    }
  }
  dsp_lock(0);
  LOG_DEBUG("[%s] leave", __func__);
  return 0;
}

void dsp_set_interpolation(DSP_HANDLER_STRUCT *dsp_handler, int Samplerate)
{
  float AdjustedOffset = (0.002*48000)/Samplerate;
  float SmoothFactor = (1-AdjustedOffset);
  int cntDSPCard;
  LOG_DEBUG("[%s] enter", __func__);

  dsp_lock(1);
  for (cntDSPCard=0; cntDSPCard<4; cntDSPCard++)
  {
    DSPCARD_STRUCT *dspcard = &dsp_handler->dspcard[cntDSPCard];

    AdjustedOffset = (0.1*48000)/Samplerate;
    float PPMReleaseFactor = (1-AdjustedOffset);

    AdjustedOffset = (0.00019186*48000)/Samplerate;
    float VUReleaseFactor = (1-AdjustedOffset);

    AdjustedOffset = (0.00043891*48000)/Samplerate;
    float RMSReleaseFactor = (1-AdjustedOffset);
    //DSP1
    if (dspcard->dsp_regs[0].HPIA != NULL)
    {
      *dspcard->dsp_regs[0].HPIA = ModuleDSPSmoothFactor;
      *((float *)dspcard->dsp_regs[0].HPID) = SmoothFactor;

      *dspcard->dsp_regs[0].HPIA = ModuleDSPPPMReleaseFactor;
      *((float *)dspcard->dsp_regs[0].HPID) = PPMReleaseFactor;

      *dspcard->dsp_regs[0].HPIA = ModuleDSPVUReleaseFactor;
      *((float *)dspcard->dsp_regs[0].HPID) = VUReleaseFactor;

      *dspcard->dsp_regs[0].HPIA = ModuleDSPRMSReleaseFactor;
      *((float *)dspcard->dsp_regs[0].HPID) = RMSReleaseFactor;
    }

    //DSP2
    if (dspcard->dsp_regs[1].HPIA != NULL)
    {
      *dspcard->dsp_regs[1].HPIA = ModuleDSPSmoothFactor;
      *((float *)dspcard->dsp_regs[1].HPID) = SmoothFactor;

      *dspcard->dsp_regs[1].HPIA = ModuleDSPPPMReleaseFactor;
      *((float *)dspcard->dsp_regs[1].HPID) = PPMReleaseFactor;

      *dspcard->dsp_regs[1].HPIA = ModuleDSPVUReleaseFactor;
      *((float *)dspcard->dsp_regs[1].HPID) = VUReleaseFactor;

      *dspcard->dsp_regs[1].HPIA = ModuleDSPRMSReleaseFactor;
      *((float *)dspcard->dsp_regs[1].HPID) = RMSReleaseFactor;
    }

    //DSP3
    AdjustedOffset = (0.002*48000)/Samplerate;
    SmoothFactor = (1-(AdjustedOffset*4)); //*4 for interpolation

    AdjustedOffset = (0.001*48000)/Samplerate;
    VUReleaseFactor = (1-AdjustedOffset);

    float PhaseRelease = ((0.0002*48000)/Samplerate);
    if (dspcard->dsp_regs[2].HPIA != NULL)
    {
      *dspcard->dsp_regs[2].HPIA = SummingDSPSmoothFactor;
      *((float *)dspcard->dsp_regs[2].HPID) = SmoothFactor;

      *dspcard->dsp_regs[2].HPIA = SummingDSPVUReleaseFactor;
      *((float *)dspcard->dsp_regs[2].HPID) = VUReleaseFactor;

      *dspcard->dsp_regs[2].HPIA = SummingDSPPhaseRelease;
      *((float *)dspcard->dsp_regs[2].HPID) = PhaseRelease;
    }
  }
  dsp_lock(0);
  LOG_DEBUG("[%s] leave", __func__);
}

void dsp_set_eq(DSP_HANDLER_STRUCT *dsp_handler, unsigned int SystemChannelNr, unsigned char BandNr)
{
  float Coefs[6];
  float a0 = 1;
  float a1 = 0;
  float a2 = 0;
  float b1 = 0;
  float b2 = 0;
  unsigned char DSPCardNr = (SystemChannelNr/64);
  unsigned char DSPCardChannelNr = SystemChannelNr%64;
  unsigned char DSPNr = (DSPCardChannelNr)/32;
  unsigned char DSPChannelNr = DSPCardChannelNr%32;
  LOG_DEBUG("[%s] enter", __func__);

  dsp_lock(1);
  DSPCARD_STRUCT *dspcard = &dsp_handler->dspcard[DSPCardNr];
  if (dspcard->data.ChannelData[DSPCardChannelNr].EQBand[BandNr].On)
  {
    float           Level               = dspcard->data.ChannelData[DSPCardChannelNr].EQBand[BandNr].Level;
    unsigned int    Frequency           = dspcard->data.ChannelData[DSPCardChannelNr].EQBand[BandNr].Frequency;
    float           Bandwidth           = dspcard->data.ChannelData[DSPCardChannelNr].EQBand[BandNr].Bandwidth;
    float           Slope               = dspcard->data.ChannelData[DSPCardChannelNr].EQBand[BandNr].Slope;
    FilterType      Type                = dspcard->data.ChannelData[DSPCardChannelNr].EQBand[BandNr].Type;

    CalculateEQ(Coefs, Level, Frequency, Bandwidth, Slope, Type);

    a0 = Coefs[0]/Coefs[3];
    a1 = Coefs[1]/Coefs[3];
    a2 = Coefs[2]/Coefs[3];
    b1 = Coefs[4]/Coefs[3];
    b2 = Coefs[5]/Coefs[3];
  }

  if (dspcard->dsp_regs[DSPNr].HPIA != NULL)
  {
    *dspcard->dsp_regs[DSPNr].HPIA = ModuleDSPEQCoefficients+(((DSPChannelNr*5)+(BandNr*32*5))*4);
    *((float *)dspcard->dsp_regs[DSPNr].HPID_Inc) = -b1;
    *((float *)dspcard->dsp_regs[DSPNr].HPID_Inc) = -b2;
    *((float *)dspcard->dsp_regs[DSPNr].HPID_Inc) = a0;
    *((float *)dspcard->dsp_regs[DSPNr].HPID_Inc) = a1;
    *((float *)dspcard->dsp_regs[DSPNr].HPID_Inc) = a2;
  }
  dsp_lock(0);
  LOG_DEBUG("[%s] leave", __func__);
}

void dsp_set_ch(DSP_HANDLER_STRUCT *dsp_handler, unsigned int SystemChannelNr)
{
  unsigned char DSPCardNr = (SystemChannelNr/64);
  unsigned char DSPCardChannelNr = SystemChannelNr%64;
  unsigned char DSPNr = (DSPCardChannelNr)/32;
  unsigned char DSPChannelNr = DSPCardChannelNr%32;
  LOG_DEBUG("[%s] enter", __func__);

  dsp_lock(1);
  DSPCARD_STRUCT *dspcard = &dsp_handler->dspcard[DSPCardNr];
  if (dspcard->dsp_regs[DSPNr].HPIA != NULL)
  {
    //Routing from (0: Gain input is default '0'->McASPA)

    //Routing from (1: EQ input is '2'->Gain output or '1'->McASPB)
    *dspcard->dsp_regs[DSPNr].HPIA = ModuleDSPRoutingFrom+((1*32*4)+(DSPChannelNr*4));
    if (dspcard->data.ChannelData[DSPCardChannelNr].Insert)
    {
      *((int *)dspcard->dsp_regs[DSPNr].HPID) = 1;
    }
    else
    {
      *((int *)dspcard->dsp_regs[DSPNr].HPID) = 2;
    }

    //Routing from (3: McASPA input (insert out) is '5'->Level output)
    *dspcard->dsp_regs[DSPNr].HPIA = ModuleDSPRoutingFrom+((3*32*4)+(DSPChannelNr*4));
    *((int *)dspcard->dsp_regs[DSPNr].HPID) = 2;

    //Routing from (3: McASPA input (insert out) is '2'->Gain output)
//      *dspcard->dsp_regs[DSPNr].HPIA = ModuleDSPRoutingFrom+((3*32*4)+(DSPChannelNr*4));
//      *((int *)dspcard->dsp_regs[DSPNr].HPID) = 2;

    //Routing from (5: level meter input is '0'->McASPA  or '1'->McASPB)
    *dspcard->dsp_regs[DSPNr].HPIA = ModuleDSPRoutingFrom+((5*32*4)+(DSPChannelNr*4));
    if (dspcard->data.ChannelData[DSPCardChannelNr].Insert)
    {
      *((int *)dspcard->dsp_regs[DSPNr].HPID) = 1;
    }
    else
    {
      *((int *)dspcard->dsp_regs[DSPNr].HPID) = 0;
    }

    //Routing from (6: level input is '1'->McASPB (insert input)
    *dspcard->dsp_regs[DSPNr].HPIA = ModuleDSPRoutingFrom+((6*32*4)+(DSPChannelNr*4));
    *((int *)dspcard->dsp_regs[DSPNr].HPID) = 1;

    //Gain section
    float factor = pow10(dspcard->data.ChannelData[DSPCardChannelNr].Gain/20);
    if (dspcard->data.ChannelData[DSPCardChannelNr].PhaseReverse)
    {
      factor *= -1;
    }
    *dspcard->dsp_regs[DSPNr].HPIA = ModuleDSPUpdate_InputGainFactor+(DSPChannelNr*4);
    *((float *)dspcard->dsp_regs[DSPNr].HPID) = factor;

    //Filter section
    float Coefs[6];
    float a0 = 1;
    float a1 = 0;
    float a2 = 0;
    float b1 = 0;
    float b2 = 0;
    if (dspcard->data.ChannelData[DSPCardChannelNr].Filter.On)
    {
      float           Level               = dspcard->data.ChannelData[DSPCardChannelNr].Filter.Level;
      unsigned int    Frequency           = dspcard->data.ChannelData[DSPCardChannelNr].Filter.Frequency;
      float           Bandwidth           = dspcard->data.ChannelData[DSPCardChannelNr].Filter.Bandwidth;
      float           Slope                   = dspcard->data.ChannelData[DSPCardChannelNr].Filter.Slope;
      FilterType      Type                    = dspcard->data.ChannelData[DSPCardChannelNr].Filter.Type;

      CalculateEQ(Coefs, Level, Frequency, Bandwidth, Slope, Type);

      a0 = Coefs[0]/Coefs[3];
      a1 = Coefs[1]/Coefs[3];
      a2 = Coefs[2]/Coefs[3];
      b1 = Coefs[4]/Coefs[3];
      b2 = Coefs[5]/Coefs[3];
    }
    *dspcard->dsp_regs[DSPNr].HPIA = ModuleDSPFilterCoefficients+((DSPChannelNr*5)*4);
    *((float *)dspcard->dsp_regs[DSPNr].HPID_Inc) = -b1;
    *((float *)dspcard->dsp_regs[DSPNr].HPID_Inc) = -b2;
    *((float *)dspcard->dsp_regs[DSPNr].HPID_Inc) = a0;
    *((float *)dspcard->dsp_regs[DSPNr].HPID_Inc) = a1;
    *((float *)dspcard->dsp_regs[DSPNr].HPID_Inc) = a2;

    float DynamicsProcessedFactor = 0;
    float DynamicsOriginalFactor = 1;
    if (dspcard->data.ChannelData[DSPCardChannelNr].Dynamics.On)
    {
      DynamicsProcessedFactor = (float)dspcard->data.ChannelData[DSPCardChannelNr].Dynamics.Percent/100;
      DynamicsOriginalFactor = 1-DynamicsProcessedFactor;
    }

    *dspcard->dsp_regs[DSPNr].HPIA = ModuleDSPDynamicsOriginalFactor+(DSPChannelNr*4);
    *((float *)dspcard->dsp_regs[DSPNr].HPID_Inc) = DynamicsOriginalFactor;

    *dspcard->dsp_regs[DSPNr].HPIA = ModuleDSPDynamicsProcessedFactor+(DSPChannelNr*4);
    *((float *)dspcard->dsp_regs[DSPNr].HPID_Inc) = DynamicsProcessedFactor;
  }
  dsp_lock(0);

  //dsp_set_eq uses intern dsp_lock, so places outside local dsp_lock
  for (int cntBand=0; cntBand<6; cntBand++)
  {
    dsp_set_eq(dsp_handler, DSPCardChannelNr, cntBand);
  }
  LOG_DEBUG("[%s] leave", __func__);
}

void dsp_set_buss_lvl(DSP_HANDLER_STRUCT *dsp_handler, unsigned int SystemChannelNr)
{
  unsigned char DSPCardNr = (SystemChannelNr/64);
  unsigned char DSPCardChannelNr = SystemChannelNr%64;
  LOG_DEBUG("[%s] enter", __func__);

  dsp_lock(1);
  DSPCARD_STRUCT *dspcard = &dsp_handler->dspcard[DSPCardNr];
  if (dspcard->dsp_regs[2].HPIA != NULL)
  {
    for (int cntBuss=0; cntBuss<32; cntBuss++)
    {
      float factor = 0;

      if (dspcard->data.ChannelData[DSPCardChannelNr].Buss[cntBuss].On)
      {
        if (dspcard->data.ChannelData[DSPCardChannelNr].Buss[cntBuss].Level<-120)
        {
          factor = 0;
        }
        else
        {
          factor = pow10(dspcard->data.ChannelData[DSPCardChannelNr].Buss[cntBuss].Level/20);
        }
      }

      *dspcard->dsp_regs[2].HPIA = SummingDSPUpdate_MatrixFactor+((cntBuss+(DSPCardChannelNr*32))*4);
      *((float *)dspcard->dsp_regs[2].HPID) = factor;
    }
  }
  dsp_lock(0);
  LOG_DEBUG("[%s] leave", __func__);
}

void dsp_set_mixmin(DSP_HANDLER_STRUCT *dsp_handler, unsigned int SystemChannelNr)
{
  unsigned char DSPCardNr = (SystemChannelNr/64);
  unsigned char DSPCardChannelNr = SystemChannelNr%64;
  unsigned char DSPChannelNr = DSPCardChannelNr%32;
  LOG_DEBUG("[%s] enter", __func__);

  dsp_lock(1);
  DSPCARD_STRUCT *dspcard = &dsp_handler->dspcard[DSPCardNr];
  if (dspcard->dsp_regs[2].HPIA != NULL)
  {
    *dspcard->dsp_regs[2].HPIA = SummingDSPSelectedMixMinusBuss+(DSPChannelNr*4);
    *((int *)dspcard->dsp_regs[2].HPID) = dspcard->data.MixMinusData[DSPCardChannelNr].Buss;
  }
  dsp_lock(0);
  LOG_DEBUG("[%s] leave", __func__);
}

void dsp_set_buss_mstr_lvl(DSP_HANDLER_STRUCT *dsp_handler)
{
  LOG_DEBUG("[%s] enter", __func__);

  for (int cntDSPCard=0; cntDSPCard<4; cntDSPCard++)
  {
    dsp_lock(1);
    DSPCARD_STRUCT *dspcard = &dsp_handler->dspcard[cntDSPCard];
    if (dspcard->dsp_regs[2].HPIA != NULL)
    {
      for (int cntBuss=0; cntBuss<32; cntBuss++)
      {
        float factor = pow10(dspcard->data.BussMasterData[cntBuss].Level/20);
        if (!dspcard->data.BussMasterData[cntBuss].On)
        {
          factor = 0;
        }

        *dspcard->dsp_regs[2].HPIA = SummingDSPUpdate_MatrixFactor+((64*32)*4)+(cntBuss*4);
        *((float *)dspcard->dsp_regs[2].HPID) = factor;
      }
    }
    dsp_lock(0);
  }
  LOG_DEBUG("[%s] leave", __func__);
}

void dsp_set_monitor_buss(DSP_HANDLER_STRUCT *dsp_handler, unsigned int MonitorChannelNr)
{
  unsigned char DSPCardNr = (MonitorChannelNr/8);
  unsigned char DSPCardMonitorChannelNr = MonitorChannelNr%8;
  float factor = 0;
  LOG_DEBUG("[%s] enter", __func__);

  dsp_lock(1);
  DSPCARD_STRUCT *dspcard = &dsp_handler->dspcard[DSPCardNr];
  if (dspcard->dsp_regs[2].HPIA != NULL)
  {
    for (int cntMonitorInput=0; cntMonitorInput<48; cntMonitorInput++)
    {
      if (dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr].Level[cntMonitorInput]<=-140)
      {
        factor = 0;
      }
      else
      {
        factor = pow10(dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr].Level[cntMonitorInput]/20);
      }

      *dspcard->dsp_regs[2].HPIA = SummingDSPUpdate_MatrixFactor+((64*32)*4)+(32*4)+(DSPCardMonitorChannelNr*4)+((cntMonitorInput*8)*4);
      *((float *)dspcard->dsp_regs[2].HPID) = factor;
    }

    if (dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr].MasterLevel<=-140)
    {
      factor = 0;
    }
    else
    {
      factor = pow10(dspcard->data.MonitorChannelData[DSPCardMonitorChannelNr].MasterLevel/20);
    }
    *dspcard->dsp_regs[2].HPIA = SummingDSPUpdate_MatrixFactor+((64*32)*4)+(32*4)+(DSPCardMonitorChannelNr*4)+((48*8)*4);
    *((float *)dspcard->dsp_regs[2].HPID) = factor;
  }
  dsp_lock(0);
  LOG_DEBUG("[%s] leave", __func__);
}

void dsp_read_buss_meters(DSP_HANDLER_STRUCT *dsp_handler, float *SummingdBLevel)
{
  LOG_DEBUG("[%s] enter", __func__);

  dsp_lock(1);
  DSPCARD_STRUCT *dspcard = &dsp_handler->dspcard[0];
  if (dspcard->dsp_regs[2].HPIA != NULL)
  {
    //VU or PPM
    //float dBLevel[48];
    for (int cntChannel=0; cntChannel<48; cntChannel++)
    {
      unsigned int MeterAddress = SummingDSPBussMeterPPM+cntChannel*4;
      //if (VUMeter)
      //{
      //  MeterAddress = SummingDSPBussMeterVU+cntChannel*4;
      //}

      *dspcard->dsp_regs[2].HPIA = MeterAddress;
      float LinearLevel = *((float *)dspcard->dsp_regs[2].HPID);
      //if (!VUMeter)
      //{
      *((float *)dspcard->dsp_regs[2].HPID) = 0;
      //}
      if (LinearLevel != 0)
      {
        SummingdBLevel[cntChannel] = 20*log10(LinearLevel/2147483647);
      }
      else
      {
        SummingdBLevel[cntChannel] = -2000;
      }
    }
  }
  dsp_lock(0);
  LOG_DEBUG("[%s] leave", __func__);
}

void dsp_read_module_meters(DSP_HANDLER_STRUCT *dsp_handler, float *dBLevel)
{
  int cntDSPCard;
  LOG_DEBUG("[%s] enter", __func__);

  for (cntDSPCard=0; cntDSPCard<4; cntDSPCard++)
  {
    dsp_lock(1);
    DSPCARD_STRUCT *dspcard = &dsp_handler->dspcard[cntDSPCard];
    if (dspcard->dsp_regs[0].HPIA != NULL)
    {
      for (int cntChannel=0; cntChannel<32; cntChannel++)
      {
        *dspcard->dsp_regs[0].HPIA = ModuleDSPMeterPPM+cntChannel*4;
        float LinearLevel = *((float *)dspcard->dsp_regs[0].HPID);
        *((float *)dspcard->dsp_regs[0].HPID) = 0;
        if (LinearLevel != 0)
        {
          dBLevel[cntChannel+(cntDSPCard*64)] = 20*log10(LinearLevel/2147483647);
        }
        else
        {
          dBLevel[cntChannel+(cntDSPCard*64)] = -2000;
        }
      }
    }
    if (dspcard->dsp_regs[1].HPIA != NULL)
    {
      for (int cntChannel=0; cntChannel<32; cntChannel++)
      {
        *dspcard->dsp_regs[1].HPIA = ModuleDSPMeterPPM+cntChannel*4;
        float LinearLevel = *((float *)dspcard->dsp_regs[1].HPID);
        *((float *)dspcard->dsp_regs[1].HPID) = 0;
        if (LinearLevel != 0)
        {
          dBLevel[32+cntChannel+(cntDSPCard*64)] = 20*log10(LinearLevel/2147483647);
        }
        else
        {
          dBLevel[32+cntChannel+(cntDSPCard*64)] = -2000;
        }
      }
    }
    dsp_lock(0);
  }
  LOG_DEBUG("[%s] leave", __func__);
}

void dsp_lock(int l)
{
  if(l) {
    pthread_mutex_lock(&dsp_mutex);
  } else {
    pthread_mutex_unlock(&dsp_mutex);
  }
}

