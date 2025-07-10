#ifndef _VMX_H_
#define _VMX_H_

#include <ntddk.h>

#define ALIGNMENT_PAGE_SIZE 4096
#define MAXIMUM_ADDRESS 0xffffffffffffffff
#define VMCS_SIZE 4096
#define VMXON_SIZE 4096

typedef struct _VIRTUAL_MACHINE_STATE
{
    UINT64 VmxonRegion; // VMXON region
    UINT64 VmcsRegion;  // VMCS region
} VIRTUAL_MACHINE_STATE, *PVIRTUAL_MACHINE_STATE;

extern VIRTUAL_MACHINE_STATE* g_GuestState;
extern int g_ProcessorCounts;

#define POOLTAG 0x48564653 // [H]yper[V]isor [F]rom [S]cratch (HVFS)

BOOLEAN
IsVmxSupported();

BOOLEAN
InitializeVmx();

VOID
TerminateVmx();

UINT64
VirtualToPhysicalAddress(void* va);

PVOID
PhysicalToVirtualAddress(UINT64 pa);

BOOLEAN
AllocateVmxonRegion(IN VIRTUAL_MACHINE_STATE* GuestState);

BOOLEAN
AllocateVmcsRegion(IN VIRTUAL_MACHINE_STATE* GuestState);

void inline AsmEnableVmxOperation(void);

void
RunOnEachLogicalProcessor(void* (*FunctionPtr)());

#endif // _VMX_H_