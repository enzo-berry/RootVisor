/*
   This file contains the headers for Hypervisor Routines which have to be called by external codes,
        DO NOT DIRECTLY CALL VMX FUNCTIONS,
            instead use these routines.
*/

#pragma once

#include "Common.h"
#include "VmxRoutines.h"

//////////////////////////////////////////////////
//					Functions					//
//////////////////////////////////////////////////

// Detect whether Vmx is supported or not
BOOLEAN
HvIsVmxSupported();
// Initialize Vmx
NTSTATUS
HvVmxInitialize();

// Set Guest Selector Registers
BOOLEAN
HvSetGuestSelector(PVOID GdtBase, ULONG SegmentRegister, USHORT Selector);
// Get Segment Descriptor
BOOLEAN
HvGetSegmentDescriptor(PSEGMENT_SELECTOR SegmentSelector, USHORT Selector, PUCHAR GdtBase);
// Set Msr Bitmap
BOOLEAN
HvSetMsrBitmap(ULONG64 Msr, INT ProcessorID, BOOLEAN ReadDetection, BOOLEAN WriteDetection);

// Returns the Cpu Based and Secondary Processor Based Controls and other controls based on hardware support
ULONG
HvAdjustControls(ULONG Ctl, ULONG Msr);

// Notify all cores about EPT Invalidation
VOID
HvNotifyAllToInvalidateEpt();
// Handle Cpuid
VOID
HvHandleCpuid(PGUEST_REGS RegistersState);
// Fill guest selector data
VOID
HvFillGuestSelectorData(PVOID GdtBase, ULONG SegmentRegister, USHORT Selector);
// Handle Guest's Control Registers Access
VOID
HvHandleControlRegisterAccess(PGUEST_REGS GuestState);
// Handle Guest's Msr read
VOID
HvHandleMsrRead(PGUEST_REGS GuestRegs);
// Handle Guest's Msr write
VOID
HvHandleMsrWrite(PGUEST_REGS GuestRegs);
// Resume GUEST_RIP to next instruction
VOID
HvResumeToNextInstruction();
// Invalidate EPT using Vmcall (should be called from Vmx non root mode)
VOID
HvInvalidateEptByVmcall(ULONG_PTR Context);
// The broadcast function which initialize the guest
VOID
HvDpcBroadcastInitializeGuest(struct _KDPC* Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2);
// The broadcast function which terminate the guest
VOID
HvDpcBroadcastTerminateGuest(struct _KDPC* Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2);
// Terminate Vmx on all logical cores.
VOID
HvTerminateVmx();

// Returns the stack pointer, to change in the case of Vmxoff
UINT64
HvReturnStackPointerForVmxoff();
// Returns the instruction pointer, to change in the case of Vmxoff
UINT64
HvReturnInstructionPointerForVmxoff();