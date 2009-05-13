////////////////////////////////////////////////////////////////////
//     File:   ddpci2040.H
//
//     Purpose: This is the Public include file for the C54x EVM
//              device drivers and client DLL.
//
//     Copyright (c) 1999, DNA Enterprises, Inc.
////////////////////////////////////////////////////////////////////
#ifndef _DDPCI2040_H
#define _DDPCI2040_H

struct pci2040_ioctl_linux
{
  unsigned int FunctionNr;
  unsigned int BufferLength;
  unsigned char *Buffer;
};

#define PCI2040_IOCTL_LINUX _IOWR('H',1, struct pci2040_ioctl_linux)

// Reset Driver and EVM Board
// Input:  none
// Output: none
#define IOCTL_PCI2040_DRIVER_RESET            0

// Dump PCI Configuration Registers
// Input:  none
// Output: PPCI2040_CONFIGURATION - pointer to structure containing
//         the PCI configuration information
// Notes:  this call reads the PCI configuration to the caller supplied buffer.
#define IOCTL_PCI2040_DUMP_CONFIGURATION      1

// Store PCI Configuration Registers
// Input: PPCI2040_CONFIGURATION - pointer to structure containing
//         the PCI configuration information
// Output:  none
// Notes:  this call writes the PCI configuration in the caller supplied buffer.
#define IOCTL_PCI2040_STORE_CONFIGURATION     2

// Read PCI Configuration Register
// Input:
// Output:
#define IOCTL_PCI2040_READ_PCI_REG            3

// Write PCI Configuration Register
// Input:
// Output: none
#define IOCTL_PCI2040_WRITE_PCI_REG           4

// Map HPI CSR registers into user memory space
// Input:  none
// Output: PPCI2040_HPI_CSR_ADDRESSES - structure containing user-mapped
//         pointer to the mapped HPI CSR registers and number of bytes
//         of mapped memory.
// Notes:  the memory must be unmapped using
//         IOCTL_PCI2040_UNMAP_HPI_CSR_SPACES,
//         pointer is vaild only in the caller's address space.
//         Only one active mapping of the HPI regs is allowed.
#define IOCTL_PCI2040_HPI_CSR_SPACES      5


// Map DSP HPI Registers into User's Memory Space
// Input:  none
// Output: PPCI2040_DSP_HPI_ADDRESSES - structure containing user-mapped
//         pointer to the mapped DSP HPI mwmory and number of bytes
//         of mapped memory.
// Notes:  the memory must be unmapped using
//         IOCTL_PCI2040_UNMAP_DSP_HPI_SPACES,
//         pointer is vaild only in the caller's address space.
//         Only one active mapping of the DSP HPI space is allowed.
#define IOCTL_PCI2040_DSP_HPI_SPACES      6

// Map GP Bus (TBC) into User's Memory Space
// Input:
// Output:
// Notes:
#define IOCTL_PCI2040_GPB_TBC_SPACES      7

// The following is used for the DSP reset IOCTLs
typedef enum {
  Select_DSP0 = 0,        // Selects DSP 0 as target
  Select_DSP1,            // "
  Select_DSP2,
  Select_DSP3
} DSPSelectEnum, *PDSPSelectEnum;

// Hold DSP in Reset State
// Input:  PDSPSelectEnum containing the number of the DSP to reset
// Output: none
#define IOCTL_PCI2040_DSP_RESET_HOLD         11

// Release DSP from Reset State
// Input:  PDSPSelectEnum containing the number of the DSP to reset
// Output: none
#define IOCTL_PCI2040_DSP_RESET_RELEASE      12

// Reset Install RING3 interrupt event handle
// Input:  Kernel32 mapped event handle
// Output: none
// Note:
#define IOCTL_PCI2040_INSTALL_INTERRUPT_EVENT   13

// Reset Clear RING3 interrupt event handle
// Input:  none
// Output: none
// Note:
#define IOCTL_PCI2040_CLEAR_INTERRUPT_EVENT     14

// Get event information
// Input:  none
// Output: Event status
// Note:  The returned status uses values defined for the HPI CSR
//        control register and HPI ERROR detail that has been shifted
//        16 bits to the left (see defines below).
#define IOCTL_PCI2040_GET_INTERRUPT_INFORMATION 15

// Wait in driver for interrupt signal (NT only)
// Input:  none
// Output: none
// Note:   WinNT only
#define IOCTL_PCI2040_WAITFOR_PROCESSOR_SIGNAL  16

#define MAX_PCI2040_BOARDS  8

#define HPICSR_SIZE         4096        // Size of HPICSR segment to map
#define HPI_MAPPED_DSP_SZ   1024        // Size of segment for each DSP
#define CSpace_SIZE         32768       // Size of Control Space segment to map
#define GPB_SIZE            256         // Size of General Purpose segment to map

// Structure used to access the PCI configuration
typedef struct _PCI2040_CONFIGURATION {
  unsigned short VendorID;       // Vendor ID
  unsigned short DeviceID;       // Device ID
  unsigned short Command;        // Command
  unsigned short Status;         // Status
  unsigned char RevisionID;     // Revision ID
  unsigned char ProgIf;         // Prog
  unsigned char SubClass;       // Sub-class
  unsigned char BaseClass;      // Base-class
  unsigned char CacheLineSize;  // Cache Line Size
  unsigned char LatencyTimer;   // Latency Timer
  unsigned char HeaderType;     // Header Type
  unsigned char BIST;           // BIST
  unsigned long HPICSRBar;      // HPI CSR Memory Base Addrress
  unsigned long CSpaceBar;      // Control Space Base Address
  unsigned long GPBBar;         // GPBus Base Address
  unsigned long Reserve0[4];    // Reserved
  unsigned short SubVendorID;    // Subsystem Vendor ID
  unsigned short SubSysID;       // Subsystem ID
  unsigned long Reserve1;       // Reserved
  unsigned char CapPointer;     // Capability Pointer
  unsigned char Reserve2[3];    // Reserved
  unsigned long Reserve3;       // Reserved
  unsigned char IRQLine;        // Interrupt Line
  unsigned char IRQPin;         // Interrupt Pin
  unsigned char MinGnt;         // Min Grant
  unsigned char MaxLat;         // Max latency
  unsigned short Reserve4[2];    // Reserved
  unsigned char GPBSelect;      // GPIO Select
  unsigned char GPBInData;      // GPIO Input Data
  unsigned char GPBDataDir;     // GPIO Direction Control
  unsigned char GPBOutData;     // GPIO Output Data
  unsigned char GPBIntType;     // GPIO Interrupt Type
  unsigned char Reserve5[3];    // Reserved
  unsigned short MiscControl;    // Misc Control
  unsigned char Reserve14;      // Reserved
  unsigned char Diag;           // Diagnostic
  unsigned char PMCapID;        // PM Capability ID
  unsigned char PMNext;         // PM Next-Item Pointer
  unsigned short PMCap;          // Power Management Capabilities
  unsigned short PMCntlStat;     // PM Control / Status
  unsigned short Reserve6;       // Reserved
  unsigned long HPICSRIOBar;    // HPI CSR I/O Base Address
  unsigned char HSCapID;        // HS Capability ID
  unsigned char HSNext;         // HS Next-Item Pointer
  unsigned char HSCSR;          // HS_CSR
  unsigned char Reserve7[5];    // Reserved
} PCI2040_DUMP_CONFIG_REGS, *PPCI2040_DUMP_CONFIG_REGS;

// Defines for register numbers
#define VENDORID_OFFSET     0       // Vendor ID
#define DEVICEID_OFFSET     2       // Device ID
#define COMMAND_OFFSET      4       // Command
#define STATUS_OFFSET       6       // Status
#define REVISIONID_OFFSET   8       // Revision ID
#define PROGIF_OFFSET       9       // Prog
#define SUBCLASS_OFFSET     10      // Sub-class
#define BASECLASS_OFFSET    11      // Base-class
#define CACHELINESIZE_OFFSET 12     // Cache Line Size
#define LATENCYTIMER_OFFSET 13      // Latency Timer
#define HEADERTYPE_OFFSET   14      // Header Type
#define BIST_OFFSET         15      // BIST
#define HPICSRBAR_OFFSET    16      // HPI CSR Memory Base Addrress
#define CSPACEBAR_OFFSET    20      // Control Space Base Address
#define GPBBAR_OFFSET       24      // GPBus Base Address
#define SUBVENDORID_OFFSET  44      // Subsystem Vendor ID
#define SUBSYSID_OFFSET     46      // Subsystem ID
#define CAPPOINTER_OFFSET   52      // Capability Pointer
#define IRQLINE_OFFSET      60      // Interrupt Line
#define IRQPIN_OFFSET       61      // Interrupt Pin
#define MINGNT_OFFSET       62      // Min Grant
#define MAXLAT_OFFSET       63      // Max latency
#define GPBSELECT_OFFSET    68      // GPIO Select
#define GPBINDATA_OFFSET    69      // GPIO Input Data
#define GPBDATADIR_OFFSET   70      // GPIO Direction Control
#define GPBOUTDATA_OFFSET   71      // GPIO Output Data
#define GPBINTTYPE_OFFSET   72      // GPIO Interrupt Type
#define MISCCONTROL_OFFSET  76      // Misc Control
#define DIAG_OFFSET         79      // Diagnostic
#define PMCAPID_OFFSET      80      // PM Capability ID
#define PMNEXT_OFFSET       81      // PM Next-Item Pointer
#define PMCAP_OFFSET        82      // Power Management Capabilities
#define PMCNTLSTAT_OFFSET   84      // PM Control / Status
#define HPICSRIOBAR_OFFSET  88      // HPI CSR I/O Base Address
#define HSCAPID_OFFSET      92      // HS Capability ID
#define HSNEXT_OFFSET       93      // HS Next-Item Pointer
#define HSCSR_OFFSET        94      // HS_CSR


// Enum for writing and reading individual registers
typedef enum  {
  ENUM_VENDORID_OFFSET    = VENDORID_OFFSET,
  ENUM_DEVICEID_OFFSET    = DEVICEID_OFFSET,
  ENUM_COMMAND_OFFSET     = COMMAND_OFFSET,
  ENUM_STATUS_OFFSET      = STATUS_OFFSET,
  ENUM_REVISIONID_OFFSET  = REVISIONID_OFFSET,
  ENUM_PROGIF_OFFSET      = PROGIF_OFFSET,
  ENUM_SUBCLASS_OFFSET    = SUBCLASS_OFFSET,
  ENUM_BASECLASS_OFFSET   = BASECLASS_OFFSET,
  ENUM_CACHELINESIZE_OFFSET = CACHELINESIZE_OFFSET,
  ENUM_LATENCYTIMER_OFFSET = LATENCYTIMER_OFFSET,
  ENUM_HEADERTYPE_OFFSET  = HEADERTYPE_OFFSET,
  ENUM_BIST_OFFSET        = BIST_OFFSET,
  ENUM_HPICSRBAR_OFFSET   = HPICSRBAR_OFFSET,
  ENUM_CSPACEBAR_OFFSET   = CSPACEBAR_OFFSET,
  ENUM_GPBBAR_OFFSET      = GPBBAR_OFFSET,
  ENUM_SUBVENDORID_OFFSET = SUBVENDORID_OFFSET,
  ENUM_SUBSYSID_OFFSET    = SUBSYSID_OFFSET,
  ENUM_CAPPOINTER_OFFSET  = CAPPOINTER_OFFSET,
  ENUM_IRQLINE_OFFSET     = IRQLINE_OFFSET,
  ENUM_IRQPIN_OFFSET      = IRQPIN_OFFSET,
  ENUM_MINGNT_OFFSET      = MINGNT_OFFSET,
  ENUM_MAXLAT_OFFSET      = MAXLAT_OFFSET,
  ENUM_GPBSELECT_OFFSET   = GPBSELECT_OFFSET,
  ENUM_GPBINDATA_OFFSET   = GPBINDATA_OFFSET,
  ENUM_GPBDATADIR_OFFSET  = GPBDATADIR_OFFSET,
  ENUM_GPBOUTDATA_OFFSET  = GPBOUTDATA_OFFSET,
  ENUM_GPBINTTYPE_OFFSET  = GPBINTTYPE_OFFSET,
  ENUM_MISCCONTROL_OFFSET = MISCCONTROL_OFFSET,
  ENUM_DIAG_OFFSET        = DIAG_OFFSET,
  ENUM_PMCAPID_OFFSET     = PMCAPID_OFFSET,
  ENUM_PMNEXT_OFFSET      = PMNEXT_OFFSET,
  ENUM_PMCAP_OFFSET       = PMCAP_OFFSET,
  ENUM_PMCNTLSTAT_OFFSET  = PMCNTLSTAT_OFFSET,
  ENUM_HPICSRIOBAR_OFFSET = HPICSRIOBAR_OFFSET,
  ENUM_HSCAPID_OFFSET     = HSCAPID_OFFSET,
  ENUM_HSNEXT_OFFSET      = HSNEXT_OFFSET,
  ENUM_HSCSR_OFFSET       = HSCSR_OFFSET,
  ENUM_INVALID            = -1
} PCI2040_REG_OFFSETS, *PPCI2040_REG_OFFSETS;

// Structure used for HPI CSR Space
typedef struct _PCI2040_HPI_CSR {
  unsigned long IntEventSet;
  unsigned long IntEventClear;
  unsigned long IntMaskSet;
  unsigned long IntMaskClear;
  unsigned short HPIErrorReport;
  unsigned short Reserved1;
  unsigned short HPIResetRegister;
  unsigned short HPIImplementation;
  unsigned short HPIDataWidth;
  unsigned short Reserved2;
} PCI2040_HPI_CSR, *PPCI2040_HPI_CSR;

// Definitions for IntEventSet and IntEventClear
#define INTDSP0  (1<<0)     // Bit set if an interrupt has been generated by a device connected
// to the HPI[0] interface. Software can set this bit for diagnostics.
#define INTDSP1  (1<<1)     // Bit set if an interrupt has been generated by a device connected
// to the HPI[1] interface. Software can set this bit for diagnostics.
#define INTDSP2  (1<<2)     // Bit set if an interrupt has been generated by a device connected
// to the HPI[2] interface. Software can set this bit for diagnostics.
#define INTDSP3  (1<<3)     // Bit set if an interrupt has been generated by a device connected
// to the HPI[3] interface. Software can set this bit for diagnostics.
#define GPINT    (1<<26)    // Bit set if an interrupt has been generated by a device connected
// to the GPINT# interface. Software can set this bit for diagnostics.
#define INTGPIO2 (1<<27)    // Set when GPIO2Pin selects GPIO2 as an interrupt event input, and the
// event type selected by the GPIO Interrupt Event Type Register occurs.
#define INTGPIO3 (1<<28)    // Set when GPIO3Pin selects GPIO3 as an interrupt event input, and the
// event type selected by the GPIO Interrupt Event Type Register occurs.
#define GPERROR  (1<<29)    // Bit set upon serious error conditions on the GP interface, and
// allows software to gracefully terminate communication with a GP device.
#define HPIERROR (1<<30)    // Bit set upon serious error conditions on the HPI interface, and allows
// software to gracefully terminate communication with an HPI device.
// This bit is the OR combination of the HPI errors in the HPI Error Report
// Register.

// This macro is provided for retrieving HPI error detail
// using the IOCTL_PCI2040_GET_INTERRUPT_INFORMATION.  The
// HPI error detail is returned starting at bit location 16
// and follows the order defined for the HPI Error fields.
#define GetHPIErrorDetail(a)    ((a>>16)&0xF)

// Definitions for IntMaskSet and IntMaskClear
// 28:0 --- When set, interrupt will be generated when the corresponding interrupt
//          event bit (See ABOVE) is set.
#define GPERRORMASK (1<<29) // When set, an interrupt will be generated when the IntEvent.GPError
// event bit is set. When set, the GP state machine will never cause target
// aborts on PCI and will return the PCI slave zeros on such errors. When
// set, errors on posted writes will not cause SERR# signal assertions
// enabled by SERR_EN. When this bit is cleared, target aborts may occur and
// SERR# may be signaled as a result of a posted write error.
#define HPIERRORMASK (1<<30) // When set, an interrupt will be generated when the IntEvent.HPIError event
// bit is set (See Table 34). When set, the HPI state machine will never cause
// target aborts on PCI and will return the PCI slave zeros on such errors.
// When set, errors on posted writes will not cause SERR# signal assertions
// enabled by SERR_EN. When this bit is cleared, target aborts may occur and
// SERR# may be signaled as a result of a posted write error.
#define MASTERINTENABLE (1<<31) // When set, external interrupts will be generated in accordance with the
// IntMask register. If clear, no external interrupts will be generated.

// Definitions for HPIErrorReport
#define HPIERR0 (1<<0)      // Bit set if a serious error occurs on the HPI[0] interface.
#define HPIERR1 (1<<1)      // Bit set if a serious error occurs on the HPI[1] interface.
#define HPIERR2 (1<<2)      // Bit set if a serious error occurs on the HPI[2] interface.
#define HPIERR3 (1<<3)      // Bit set if a serious error occurs on the HPI[3] interface.

// Definitions for HPIResetRegister
#define HPI0_RST (1<<0)     // HPI Reset 0. When set, HRST0# is asserted.
#define HPI1_RST (1<<1)     // HPI Reset 1. When set, HRST1# is asserted.
#define HPI2_RST (1<<2)     // HPI Reset 2. When set, HRST2# is asserted.
#define HPI3_RST (1<<3)     // HPI Reset 3. When set, HRST3# is asserted.

// Definitions for HPIImplementation
#define DSP_PRSNT0 (1<<0)   // DSP0 Present. These bits indicate if the DSP0 is present on the HPI I/F.
#define DSP_PRSNT1 (1<<1)   // DSP1 Present. These bits indicate if the DSP1 is present on the HPI I/F.
#define DSP_PRSNT2 (1<<2)   // DSP2 Present. These bits indicate if the DSP2 is present on the HPI I/F.
#define DSP_PRSNT3 (1<<3)   // DSP3 Present. These bits indicate if the DSP3 is present on the HPI I/F.

// Definitions for HPIDataWidth
#define DWIDTH0 (1<<0)      // When set, the HPI[0] data bus is 16-bits (C6x). When zero, it is 8-bits (C54x).
#define DWIDTH1 (1<<1)      // When set, the HPI[1] data bus is 16-bits (C6x). When zero, it is 8-bits (C54x).
#define DWIDTH2 (1<<2)      // When set, the HPI[2] data bus is 16-bits (C6x). When zero, it is 8-bits (C54x).
#define DWIDTH3 (1<<3)      // When set, the HPI[3] data bus is 16-bits (C6x). When zero, it is 8-bits (C54x).

// Structure used to write PCI register
typedef struct _PCI2040_WRITE_REG
{
  PCI2040_REG_OFFSETS Regoff;     // Register to read/write
  unsigned long       DataVal;    // Data to read/write
} PCI2040_WRITE_REG, *PPCI2040_WRITE_REG;

// Structure used to get pci resource (per BAR)
typedef struct _PCI2040_RESOURCE
{
  unsigned long PhysicalAddress;   // Physical start address of resource
  unsigned long Length;     // Length in bytes of resource.
} PCI2040_RESOURCE, *PPCI2040_RESOURCE;

// Useful PCI Register value definitions
// PCI Control Register
#define PCIREG_CTL_FBBEN    (1<<9)  // Fast Back-to-Back Enable. This bit 
// controls whether or not the device
// is allowed to perform back to back
// capability for bus master transaction.
// This bit is hardwired to 0 and indicates
// that FBB transfers are not supported
#define PCIREG_CTL_SERREN   (1<<8)  // System Error (SERR#) Enable. This bit
// is an enable for the output driver on
// the SERR# pin. If this bit is cleared,
// and a system error condition is set,
// the error signal will not appear on the
// external SERR# pin
#define PCIREG_CTL_STEPEN   (1<<7)  // Address/Data Stepping Control. This bit
// indicates whether or not the device
// performs address stepping. This bit is
// hard wired to "0"
#define PCIREG_CTL_PERREN   (1<<6)  // Parity Error Response Enable. This bit
// controls whether or not the device
// responds to detected parity errors.
// If this bit is set, the device will
// respond normally to parity errors.
// Otherwise, the device will ignore detected
// parity errors
#define PCIREG_CTL_VGAEN    (1<<5)  // VGA Palette Snoop. This bit is not applicable
// and is hardwired to a "0"
#define PCIREG_CTL_MWIEN    (1<<4)  // Memory Write and Invalidate Enable. This bit
// enables the device to use the Memory Write and
// Invalidate command. This device does not support
// MWI and uses MW instead. This bit is hardwired
// to "0"
#define PCIREG_CTL_Special  (1<<3)  // Special Cycle. This bit controls the devices
// response to special cycle commands. This device
// does not monitor any special commands, this bit
// is set to "0"
#define PCIREG_CTL_MASTEN   (1<<2)  // Bus Master Control. This bit allows a PCI device
// to function as a bus master. This bit is hardwired
// to "0"
#define PCIREG_CTL_MEMEN    (1<<1)  // Memory Space Enable. This bit enables the device
// to respond to memory accesses to any of the
// defined base address memory regions. If this bit
// is cleared, the device will not respond to memory
// mapped accesses.
#define PCIREG_CTL_IOEN     (1<<0)  // I/O Space Control. This bit enables the device
// to respond to I/O accesses within its defined
// base address register I/O regions

// HPI Control Register (HPIC) CSpaceaddr+0
// *** NOTE ***
// The following definitions are defined for the C54X DSP.
// If a version of this software is created for the C6X, then the definitions and use
// in the drivers need to be changed.
//
#define HPIC_BOB    (1<<0|1<<8)     // The byte-order-bit. This bit determines the placement
// for the two bytes of a transfer. If BOB=1, the
// first byte of a transfer is least significant. If
// BOB=0, the first byte is most significant. This bit
// can only be accessed (written or read) by the host,
// and it must be initialized before the first data or
// address register access.
#define HPIC_DSPINT (1<<2|1<<10)    // The host-to-54xx interrupt. When the host writes a
// 1 to this bit, a 54xx interrupt is generated. The
// bit can only be written by the host, and it is always
// read as 0 by both the host and the 54xx. When
// the host writes to HPIC, both bytes must write the
// same value.
#define HPIC_HINT   (1<<3|1<<11)    // The 54xx-to-host interrupt. This bit determines the
// state of the 54xx HINT output, which can be
// used to interrupt the host. When the HINT bit is set
// to 1, the HINT output is driven low, and when the
// bit is cleared to 0, the output is driven high. The
// HINT bit can only be set by the 54xx, and it can
// only be cleared by the host. The host clears the bit
// by writing a 1 to it.
#define HPIC_XHPIA  (1<<4|1<<12)    // The extended HPI address bit. This bit determines 
// which address bits the HPIA register sets. When
// the XHPIA bit is cleared to zero, the HPIA register
// reflects the state of the lower 16 HPI-8 address
// bits A[0:15]. When the bit is set to one, the seven
// LSBs of the HPIA register reflect the state of the
// HPI-8 extended address bits. Only the host has access                                    // to this bit, and it is not initialized after reset.
#endif
