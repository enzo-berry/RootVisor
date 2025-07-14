#include "Memory.h"

#include <ntddk.h>

#include "Msr.h"


UINT64
VirtualToPhysicalAddress(void* Va)
{
    return MmGetPhysicalAddress(Va).QuadPart;
}

UINT64
PhysicalToVirtualAddress(UINT64 Pa)
{
    PHYSICAL_ADDRESS PhysicalAddr;
    PhysicalAddr.QuadPart = Pa;

    return (UINT64)MmGetVirtualForPhysical(PhysicalAddr);
}

BOOLEAN
AllocateVmxonRegion(VIRTUAL_MACHINE_STATE* GuestState)
{
    //
    // at IRQL > DISPATCH_LEVEL memory allocation routines don't work
    //
    if ( KeGetCurrentIrql() > DISPATCH_LEVEL )
        KeRaiseIrqlToDpcLevel();

    PHYSICAL_ADDRESS PhysicalMax = {0};
    PhysicalMax.QuadPart         = MAXULONG64;

    int VMXONSize = 2 * VMXON_SIZE;
    PUCHAR Buffer = MmAllocateContiguousMemory(
        VMXONSize + ALIGNMENT_PAGE_SIZE,
        PhysicalMax); // Allocating a 4-KByte Contigous Memory region

    PHYSICAL_ADDRESS Highest = {0}; //, Lowest = {0};
    Highest.QuadPart         = ~0;

    if ( Buffer == NULL )
    {
        DbgPrint("[*] Error : Couldn't Allocate Buffer for VMXON Region.");
        return FALSE; // ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }
    UINT64 PhysicalBuffer = VirtualToPhysicalAddress(Buffer);

    //
    // zero-out memory
    //
    RtlSecureZeroMemory(Buffer, VMXONSize + ALIGNMENT_PAGE_SIZE);
    UINT64 AlignedPhysicalBuffer =
        (ULONG_PTR)((ULONG_PTR)(PhysicalBuffer + ALIGNMENT_PAGE_SIZE - 1) & ~(ALIGNMENT_PAGE_SIZE - 1));

    UINT64 AlignedVirtualBuffer =
        (ULONG_PTR)((ULONG_PTR)(Buffer + ALIGNMENT_PAGE_SIZE - 1) & ~(ALIGNMENT_PAGE_SIZE - 1));

    DbgPrint("[*] Virtual allocated buffer for VMXON at %llx", Buffer);
    DbgPrint("[*] Virtual aligned allocated buffer for VMXON at %llx", AlignedVirtualBuffer);
    DbgPrint("[*] Aligned physical buffer allocated for VMXON at %llx", AlignedPhysicalBuffer);

    //
    // get IA32_VMX_BASIC_MSR RevisionId
    //
    IA32_VMX_BASIC_MSR basic = {0};

    basic.All = __readmsr(MSR_IA32_VMX_BASIC);

    DbgPrint("[*] MSR_IA32_VMX_BASIC (MSR 0x480) Revision Identifier %llx", basic.Fields.RevisionIdentifier);

    //
    // Changing Revision Identifier
    //
    *(UINT64*)AlignedVirtualBuffer = basic.Fields.RevisionIdentifier;

    int status = __vmx_on(&AlignedPhysicalBuffer);
    if ( status )
    {
        DbgPrint("[*] VMXON failed with status %d\n", status);
        return FALSE;
    }

    GuestState->VmxoRegion = AlignedPhysicalBuffer;

    return TRUE;
}

BOOLEAN
AllocateVmcsRegion(VIRTUAL_MACHINE_STATE* GuestState)
{
    //
    // at IRQL > DISPATCH_LEVEL memory allocation routines don't work
    //
    if ( KeGetCurrentIrql() > DISPATCH_LEVEL )
        KeRaiseIrqlToDpcLevel();

    PHYSICAL_ADDRESS PhysicalMax = {0};
    PhysicalMax.QuadPart         = MAXULONG64;

    int VMCSSize  = 2 * VMCS_SIZE;
    PUCHAR Buffer = MmAllocateContiguousMemory(
        VMCSSize + ALIGNMENT_PAGE_SIZE,
        PhysicalMax); // Allocating a 4-KByte Contigous Memory region

    PHYSICAL_ADDRESS Highest = {0}; //, Lowest = {0};
    Highest.QuadPart         = ~0;

    UINT64 PhysicalBuffer = VirtualToPhysicalAddress(Buffer);
    if ( Buffer == NULL )
    {
        DbgPrint("[*] Error : Couldn't Allocate Buffer for VMCS Region.");
        return FALSE; // ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // zero-out memory
    //
    RtlSecureZeroMemory(Buffer, VMCSSize + ALIGNMENT_PAGE_SIZE);
    UINT64 AlignedPhysicalBuffer =
        (ULONG_PTR)((ULONG_PTR)(PhysicalBuffer + ALIGNMENT_PAGE_SIZE - 1) & ~(ALIGNMENT_PAGE_SIZE - 1));

    UINT64 AlignedVirtualBuffer =
        (ULONG_PTR)((ULONG_PTR)(Buffer + ALIGNMENT_PAGE_SIZE - 1) & ~(ALIGNMENT_PAGE_SIZE - 1));

    DbgPrint("[*] Virtual allocated buffer for VMCS at %llx", Buffer);
    DbgPrint("[*] Virtual aligned allocated buffer for VMCS at %llx", AlignedVirtualBuffer);
    DbgPrint("[*] Aligned physical buffer allocated for VMCS at %llx", AlignedPhysicalBuffer);

    //
    // get IA32_VMX_BASIC_MSR RevisionId
    //
    IA32_VMX_BASIC_MSR basic = {0};

    basic.All = __readmsr(MSR_IA32_VMX_BASIC);

    DbgPrint("[*] MSR_IA32_VMX_BASIC (MSR 0x480) Revision Identifier %llx", basic.Fields.RevisionIdentifier);

    //
    // Changing Revision Identifier
    //
    *(UINT64*)AlignedVirtualBuffer = basic.Fields.RevisionIdentifier;

    GuestState->VmcsRegion = AlignedPhysicalBuffer;

    return TRUE;
}