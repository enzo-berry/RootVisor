#ifndef _MEMORY_H_
#define _MEMORY_H_

#include <ntddk.h>

#include "Vmx.h"

UINT64
VirtualToPhysicalAddress(void* va);

UINT64
PhysicalToVirtualAddress(UINT64 pa);

BOOLEAN
AllocateVmxonRegion(VIRTUAL_MACHINE_STATE* GuestState);

BOOLEAN
AllocateVmcsRegion(VIRTUAL_MACHINE_STATE* GuestState);

#endif // _MEMORY_H_