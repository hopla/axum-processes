#include "common.h"
#include "axum_engine.h"
#include "ddpci2040.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>

//for now use external variables but we have to make a better abstraction
extern unsigned int ModuleDSPEntryPoint; 
extern unsigned int ModuleDSPRoutingFrom;
extern unsigned int ModuleDSPUpdate_InputGainFactor;
extern unsigned int ModuleDSPUpdate_LevelFactor; 
extern unsigned int ModuleDSPFilterCoefficients;
extern unsigned int ModuleDSPEQCoefficients;
extern unsigned int ModuleDSPDynamicsOriginalFactor;
extern unsigned int ModuleDSPDynamicsProcessedFactor;
extern unsigned int ModuleDSPMeterPPM;
extern unsigned int ModuleDSPMeterVU;
extern unsigned int ModuleDSPPhaseRMS;
extern unsigned int ModuleDSPSmoothFactor;
extern unsigned int ModuleDSPPPMReleaseFactor;
extern unsigned int ModuleDSPVUReleaseFactor;
extern unsigned int ModuleDSPRMSReleaseFactor;

extern unsigned int SummingDSPEntryPoint;
extern unsigned int SummingDSPUpdate_MatrixFactor;
extern unsigned int SummingDSPBussMeterPPM;
extern unsigned int SummingDSPBussMeterVU;
extern unsigned int SummingDSPPhaseRMS;
extern unsigned int SummingDSPSelectedMixMinusBuss;
extern unsigned int SummingDSPSmoothFactor;
extern unsigned int SummingDSPVUReleaseFactor;
extern unsigned int SummingDSPPhaseRelease;

extern unsigned int FXDSPEntryPoint;

extern volatile unsigned long *PtrDSP_HPIC[4];
extern volatile unsigned long *PtrDSP_HPIA[4];
extern volatile unsigned long *PtrDSP_HPIDAutoIncrement[4];
extern volatile unsigned long *PtrDSP_HPID[4];

int dsp_open()
{
  int fd;
  //Initialize DSPs
  //**************************************************************/
  //Setup DSP(s)
  //**************************************************************/
  fd = open("/dev/pci2040", O_RDWR);
  if (fd<0)
  {
    log_write("PCI2040 open error!");
    return 0; 
  }
  else
  {
    log_write("PCI2040 opened!");

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

      if (!ProgramEEPROM(fd))
      {
        log_write("PCI card not initialized by EEPROM !\nThe card is initialized!\nEEPROM programming failed\n");
        return 0;
      }
      else
      {
        log_write("PCI card not initialized by EEPROM !\nThe card is initialized and the EEPROM programmed!\n");
        return 0;
      }
    }
    else if ((HPIConfigurationRegisters.MiscControl&0xC000)==0xC000)
    {
      log_write("PCI card is initialized but the EEPROM gives an error!\nThe EEPROM is NOT programmed !\n");
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
      
      unsigned long cntPtrAddress = (unsigned long)PtrDSP_HPI;
      for (int cntDSP=0; cntDSP<4; cntDSP++)
      {
        PtrDSP_HPIC[cntDSP] = (unsigned long *)cntPtrAddress;
        cntPtrAddress += 0x800;
        PtrDSP_HPIDAutoIncrement[cntDSP] = (unsigned long *)cntPtrAddress;
        cntPtrAddress += 0x800;
        PtrDSP_HPIA[cntDSP] = (unsigned long *)cntPtrAddress;
        cntPtrAddress += 0x800;
        PtrDSP_HPID[cntDSP] = (unsigned long *)cntPtrAddress;
        cntPtrAddress += 0x800;

      }

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
      *PtrDSP_HPIC[0] |= 0x00010001;
      *PtrDSP_HPIC[1] |= 0x00010001;
      *PtrDSP_HPIC[2] |= 0x00010001;
      *PtrDSP_HPIC[3] |= 0x00010001;

      //file descriptor not used further
      close(fd);
//-------------------------------------------------------------------------
//Load DSP1&2 with module firmware
//-------------------------------------------------------------------------
      FILE *DSPMappings = fopen("/var/lib/axum/dsp/AxumModule.map", "r");
      if (DSPMappings == NULL)
      {
        log_write("Module DSP Mappings open error");
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
            //printf("Items: %d\n" , ItemsConverted);
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
        log_write("Module DSP Mappings imported");
      }

      int DSPFirmware = open("/var/lib/axum/dsp/AxumModule.b0", O_RDONLY);
      if (DSPFirmware<0)
      {
        log_write("Module DSP Firmware open error");
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
          //printf("DSPFirmware word: %08lx\n", *PtrData);

          for (int cntDSP=0; cntDSP<2; cntDSP++)
          {
            *PtrDSP_HPIA[cntDSP] = 0x10001c00+cntAddress;
            *PtrDSP_HPID[cntDSP] = *PtrData;
          }
          cntAddress+=4;
        }
        close(DSPFirmware);

        //Set in bootloader the entry point and let it GO!
        for (int cntDSP=0; cntDSP<2; cntDSP++)
        {
          //Entry point
          *PtrDSP_HPIA[cntDSP] = 0x10000714;
          *PtrDSP_HPID[cntDSP] = ModuleDSPEntryPoint;
        }
        log_write("Module DSP firmware loaded");
      }

//-------------------------------------------------------------------------
//Load DSP3 with summing firmware
//-------------------------------------------------------------------------
      DSPMappings = fopen("/var/lib/axum/dsp/AxumSumming.map", "r");
      if (DSPMappings == NULL)
      {
        log_write("Summing DSP Mappings open error!");
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
        log_write("Summing DSP Mappings imported");
      }

      DSPFirmware = open("/var/lib/axum/dsp/AxumSumming.b0", O_RDONLY);
      if (DSPFirmware<0)
      {
        log_write("Summing DSP Firmware open error");
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
            *PtrDSP_HPIA[cntDSP] = 0x10001c00+cntAddress;
            *PtrDSP_HPID[cntDSP] = *PtrData;
          }
          cntAddress+=4;
        }
        close(DSPFirmware);

        //Set in bootloader the entry point and let it GO!
        for (int cntDSP=2; cntDSP<3; cntDSP++)
        {
          //Entry point
          *PtrDSP_HPIA[cntDSP] = 0x10000714;
          *PtrDSP_HPID[cntDSP] = SummingDSPEntryPoint;
        }

        log_write("Module DSP firmware loaded");
      }

//-------------------------------------------------------------------------
//Load DSP4 with FX firmware
//-------------------------------------------------------------------------
      DSPMappings = fopen("/var/lib/axum/dsp/AxumFX1.map", "r");
      if (DSPMappings == NULL)
      {
        log_write("FX DSP Mappings open error");
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
        log_write("FX DSP Mappings imported");
      }

      DSPFirmware = open("/var/lib/axum/dsp/AxumFX1.b0", O_RDONLY);
      if (DSPFirmware<0)
      {
        log_write("FX DSP Firmware open error");
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
            *PtrDSP_HPIA[cntDSP] = 0x10001c00+cntAddress;
            *PtrDSP_HPID[cntDSP] = *PtrData;
          }
          cntAddress+=4;
        }
        close(DSPFirmware);

        //Set in bootloader the entry point and let it GO!
        for (int cntDSP=3; cntDSP<4; cntDSP++)
        {
          //Entry point
          *PtrDSP_HPIA[cntDSP] = 0x10000714;
          *PtrDSP_HPID[cntDSP] = FXDSPEntryPoint;
        }

        log_write("FX DSP firmware loaded");
      }

      for (int cntDSP=0; cntDSP<4; cntDSP++)
      {
        //Run
        *PtrDSP_HPIA[cntDSP] = 0x10000718;
        *PtrDSP_HPID[cntDSP] = 0x00000001;
      }
      log_write("DSPs running.");
    }
  }

//**************************************************************/
//End Setup DSP(s)
//**************************************************************/
  delay_ms(1);

  //default most significant is send first, we do both
  *PtrDSP_HPIC[0] |= 0x00010001;
  *PtrDSP_HPIC[1] |= 0x00010001;
  *PtrDSP_HPIC[2] |= 0x00010001;
  *PtrDSP_HPIC[3] |= 0x00010001;

  //initialize DSPs after DSP are completely booted!
  delay_ms(50);
  
  return 1;
}

void dsp_close()
{
}
