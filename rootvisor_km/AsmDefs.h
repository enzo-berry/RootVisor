#pragma once

#include <ntddk.h>

// ====================  Vmx Operations ====================
// File : AsmVmxOperation.asm
extern void inline AsmEnableVmxOperation();
extern void inline AsmRestoreToVmxOffState();
extern NTSTATUS inline AsmVmxVmcall(
    unsigned long long VmcallNumber,
    unsigned long long OptionalParam1,
    unsigned long long OptionalParam2,
    unsigned long long OptionalParam3);

// ====================  Vmx Context State Operations ====================
// File : AsmVmxContextState.asm
extern void
AsmVmxSaveState();
extern void
AsmVmxRestoreState();


// ====================  Vmx VM-Exit Handler ====================
// File : AsmVmexitHandler.asm
extern void
AsmVmexitHandler();
extern void inline AsmSaveVmxOffState();


// ====================  Extended Page Tables ====================
// File : AsmEpt.asm
extern unsigned char inline AsmInvept(unsigned long Type, void* Descriptors);


// ====================  Get segment registers ====================
// File : AsmSegmentRegs.asm

// Segment registers
extern unsigned short
AsmGetCs();
extern unsigned short
AsmGetDs();
extern unsigned short
AsmGetEs();
extern unsigned short
AsmGetSs();
extern unsigned short
AsmGetFs();
extern unsigned short
AsmGetGs();
extern unsigned short
AsmGetLdtr();
extern unsigned short
AsmGetTr();

// Gdt related functions
extern unsigned long long inline AsmGetGdtBase();
extern unsigned short
AsmGetGdtLimit();

// Idt related functions
extern unsigned long long inline AsmGetIdtBase();
extern unsigned short
AsmGetIdtLimit();


// ====================  Common Functions ====================
// File : AsmCommon.asm
extern unsigned short
AsmGetRflags();
extern void inline AsmCliInstruction();
extern void inline AsmStiInstruction();
