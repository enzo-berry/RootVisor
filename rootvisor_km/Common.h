#pragma once

#include <ntddk.h>
#include <wdf.h>

#include "EptRoutines.h"

//////////////////////////////////////////////////
//					Enums						//
//////////////////////////////////////////////////

typedef enum _SEGMENT_REGISTERS
{
    ES = 0,
    CS,
    SS,
    DS,
    FS,
    GS,
    LDTR,
    TR
} SEGMENT_REGISTERS;

//////////////////////////////////////////////////
//					Constants					//
//////////////////////////////////////////////////

// Alignment Size
#define __CPU_INDEX__ KeGetCurrentProcessorNumberEx(NULL)

// Alignment Size
#define ALIGNMENT_PAGE_SIZE 4096

// Maximum x64 Address
#define MAXIMUM_ADDRESS 0xffffffffffffffff

// Pool tag
#define POOLTAG 0x48564653 // [H]yper[V]isor [F]rom [S]cratch (HVFS)

// System and User ring definitions
#define DPL_USER 3
#define DPL_SYSTEM 0

// RPL Mask
#define RPL_MASK 3

// VMX Operation Constants
#define VMX_SUCCESS 0
#define VMX_ERROR_CODE_SUCCESS 0
#define VMX_ERROR_CODE_FAILED_WITH_STATUS 1
#define VMX_ERROR_CODE_FAILED 2

// CPUID Constants
#define CPUID_VMX_FEATURE_BIT 5
#define CPUID_VENDOR_STRING_LENGTH 12

// MSR Bitmap Constants
#define MSR_LOW_RANGE_MAX 0x00001FFF
#define MSR_HIGH_RANGE_MIN 0xC0000000
#define MSR_HIGH_RANGE_MAX 0xC0001FFF

// CF Flag for failure indication
#define RFLAGS_CF_MASK 0x1

//////////////////////////////////////////////////
//					 Structures					//
//////////////////////////////////////////////////

typedef struct _GUEST_REGS
{
    ULONG64 rax; // 0x00
    ULONG64 rcx;
    ULONG64 rdx; // 0x10
    ULONG64 rbx;
    ULONG64 rsp; // 0x20         // rsp is not stored here
    ULONG64 rbp;
    ULONG64 rsi; // 0x30
    ULONG64 rdi;
    ULONG64 r8; // 0x40
    ULONG64 r9;
    ULONG64 r10; // 0x50
    ULONG64 r11;
    ULONG64 r12; // 0x60
    ULONG64 r13;
    ULONG64 r14; // 0x70
    ULONG64 r15;
} GUEST_REGS, *PGUEST_REGS;

typedef union _RFLAGS
{
    struct
    {
        unsigned Reserved1 : 10;
        unsigned ID        : 1; // Identification flag
        unsigned VIP       : 1; // Virtual interrupt pending
        unsigned VIF       : 1; // Virtual interrupt flag
        unsigned AC        : 1; // Alignment check
        unsigned VM        : 1; // Virtual 8086 mode
        unsigned RF        : 1; // Resume flag
        unsigned Reserved2 : 1;
        unsigned NT        : 1; // Nested task flag
        unsigned IOPL      : 2; // I/O privilege level
        unsigned OF        : 1;
        unsigned DF        : 1;
        unsigned IF        : 1; // Interrupt flag
        unsigned TF        : 1; // Task flag
        unsigned SF        : 1; // Sign flag
        unsigned ZF        : 1; // Zero flag
        unsigned Reserved3 : 1;
        unsigned AF        : 1; // Borrow flag
        unsigned Reserved4 : 1;
        unsigned PF        : 1; // Parity flag
        unsigned Reserved5 : 1;
        unsigned CF        : 1; // Carry flag [Bit 0]
        unsigned Reserved6 : 32;
    } Fields;

    ULONG64 Content;
} RFLAGS, *PRFLAGS;

typedef union _SEGMENT_ATTRIBUTES
{
    USHORT UCHARs;
    struct
    {
        USHORT TYPE : 4; /* 0;  Bit 40-43 */
        USHORT S    : 1; /* 4;  Bit 44 */
        USHORT DPL  : 2; /* 5;  Bit 45-46 */
        USHORT P    : 1; /* 7;  Bit 47 */

        USHORT AVL : 1; /* 8;  Bit 52 */
        USHORT L   : 1; /* 9;  Bit 53 */
        USHORT DB  : 1; /* 10; Bit 54 */
        USHORT G   : 1; /* 11; Bit 55 */
        USHORT GAP : 4;

    } Fields;
} SEGMENT_ATTRIBUTES, *PSEGMENT_ATTRIBUTES;

typedef struct _SEGMENT_SELECTOR
{
    USHORT SEL;
    SEGMENT_ATTRIBUTES ATTRIBUTES;
    ULONG32 LIMIT;
    ULONG64 BASE;
} SEGMENT_SELECTOR, *PSEGMENT_SELECTOR;

typedef struct _SEGMENT_DESCRIPTOR
{
    USHORT LIMIT0;
    USHORT BASE0;
    UCHAR BASE1;
    UCHAR ATTR0;
    UCHAR LIMIT1ATTR1;
    UCHAR BASE2;
} SEGMENT_DESCRIPTOR, *PSEGMENT_DESCRIPTOR;

typedef struct _CPUID
{
    int eax;
    int ebx;
    int ecx;
    int edx;
} CPUID, *PCPUID;

//////////////////////////////////////////////////
//				 Function Types					//
//////////////////////////////////////////////////

typedef void (*RunOnLogicalCoreFunc)(ULONG ProcessorID);


//////////////////////////////////////////////////
//					Logging						//
//////////////////////////////////////////////////

// Types
typedef enum _LOG_TYPE
{
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR
} LOG_TYPE;

// Defines
#define LogInfo(format, ...) DbgPrint("[+] Information (%s:%d) | " format "\n", __FUNCTION__, __LINE__, __VA_ARGS__)

#define LogWarning(format, ...) DbgPrint("[-] Warning (%s:%d) | " format "\n", __FUNCTION__, __LINE__, __VA_ARGS__)

#define LogError(format, ...)                                                                                          \
    DbgPrint("[!] Error (%s:%d) | " format "\n", __FUNCTION__, __LINE__, __VA_ARGS__);                                 \
    DbgBreakPoint()

// Log without any prefix
#define Log(format, ...) DbgPrint(format "\n", __VA_ARGS__)


//////////////////////////////////////////////////
//			 Function Definitions				//
//////////////////////////////////////////////////

// Set and Get bits related to MSR Bitmaps Settings
void
SetBit(PVOID Addr, UINT64 bit, BOOLEAN Set);

BYTE
GetBit(PVOID Addr, UINT64 bit);

// Address Translations
UINT64
VirtualAddressToPhysicalAddress(PVOID VirtualAddress);

UINT64
PhysicalAddressToVirtualAddress(UINT64 PhysicalAddress);
