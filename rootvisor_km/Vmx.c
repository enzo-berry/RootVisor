#include "Vmx.h"

#include <ntddk.h>
#include <wdf.h>

#include "Msr.h"
#include "Utils.h"

// AsmEnableVmxOperation
VIRTUAL_MACHINE_STATE* g_GuestState;
int ProcessorCounts;

BOOLEAN
IsVmxSupported()
{
    CPUID Data = {0};

    //
    // Check for the VMX bit
    //
    __cpuid((int*)&Data, 1);
    if ( (Data.ecx & (1 << 5)) == 0 )
        return FALSE;

    IA32_FEATURE_CONTROL_MSR Control = {0};
    Control.All                      = __readmsr(MSR_IA32_FEATURE_CONTROL);

    //
    // BIOS lock check
    //
    if ( Control.Fields.Lock == 0 )
    {
        Control.Fields.Lock        = TRUE;
        Control.Fields.EnableVmxon = TRUE;
        __writemsr(MSR_IA32_FEATURE_CONTROL, Control.All);
    }
    else if ( Control.Fields.EnableVmxon == FALSE )
    {
        DbgPrint("[*] VMX locked off in BIOS");
        return FALSE;
    }

    return TRUE;
}

BOOLEAN
InitializeVmx()
{
    if ( !IsVmxSupported() )
    {
        DbgPrint("[*] VMX is not supported in this machine !");
        return FALSE;
    }

    PAGED_CODE();

    ProcessorCounts = KeQueryActiveProcessorCount(0);
    g_GuestState    = ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(VIRTUAL_MACHINE_STATE) * ProcessorCounts, POOLTAG);

    DbgPrint("\n=====================================================\n");

    KAFFINITY AffinityMask;
    for ( size_t i = 0; i < ProcessorCounts; i++ )
    {
        AffinityMask = PowerOfTwo(i);

        KeSetSystemAffinityThread(AffinityMask);

        DbgPrint("\t\tCurrent thread is executing in %d th logical processor.", i);

        //
        // Enabling VMX Operation
        //
        AsmEnableVmxOperation();

        DbgPrint("[*] VMX Operation Enabled Successfully !");

        AllocateVmxonRegion(&g_GuestState[i]);
        AllocateVmcsRegion(&g_GuestState[i]);

        DbgPrint("[*] VMCS Region is allocated at  ===============> %llx", g_GuestState[i].VmcsRegion);
        DbgPrint("[*] VMXON Region is allocated at ===============> %llx", g_GuestState[i].VmxonRegion);

        DbgPrint("\n=====================================================\n");
    }

    return TRUE;
}

VOID
TerminateVmx()
{
    DbgPrint("\n[*] Terminating VMX...\n");

    KAFFINITY AffinityMask;
    for ( size_t i = 0; i < ProcessorCounts; i++ )
    {
        AffinityMask = PowerOfTwo(i);
        KeSetSystemAffinityThread(AffinityMask);
        DbgPrint("\t\tCurrent thread is executing in %d th logical processor.", i);

        __vmx_off();
        MmFreeContiguousMemory(PhysicalToVirtualAddress(g_GuestState[i].VmxonRegion));
        MmFreeContiguousMemory(PhysicalToVirtualAddress(g_GuestState[i].VmcsRegion));
    }

    DbgPrint("[*] VMX Operation turned off successfully. \n");
}

UINT64
VirtualToPhysicalAddress(void* Va)
{
    return MmGetPhysicalAddress(Va).QuadPart;
}

PVOID
PhysicalToVirtualAddress(UINT64 Pa)
{
    PHYSICAL_ADDRESS PhysicalAddr;
    PhysicalAddr.QuadPart = Pa;

    return MmGetVirtualForPhysical(PhysicalAddr);
}


BOOLEAN
AllocateVmxonRegion(IN VIRTUAL_MACHINE_STATE* GuestState)
{
    // at IRQL > DISPATCH_LEVEL memory allocation routines don't work
    if ( KeGetCurrentIrql() > DISPATCH_LEVEL )
        KeRaiseIrqlToDpcLevel();

    PHYSICAL_ADDRESS PhysicalMax = {0};
    PhysicalMax.QuadPart         = MAXULONG64;

    int VMXONSize = 2 * VMXON_SIZE;
    BYTE* Buffer  = MmAllocateContiguousMemory(
        VMXONSize + ALIGNMENT_PAGE_SIZE,
        PhysicalMax); // Allocating a 4-KByte Contigous Memory region

    PHYSICAL_ADDRESS Highest = {0}; //, Lowest = {0};
    Highest.QuadPart         = ~0;

    // BYTE* Buffer = MmAllocateContiguousMemorySpecifyCache(VMXONSize + ALIGNMENT_PAGE_SIZE, Lowest, Highest, Lowest,
    // MmNonCached);

    if ( Buffer == NULL )
    {
        DbgPrint("[*] Error : Couldn't Allocate Buffer for VMXON Region.");
        return FALSE; // ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }
    UINT64 PhysicalBuffer = VirtualToPhysicalAddress(Buffer);

    // zero-out memory
    RtlSecureZeroMemory(Buffer, VMXONSize + ALIGNMENT_PAGE_SIZE);
    UINT64 AlignedPhysicalBuffer =
        (UINT64)((ULONG_PTR)(PhysicalBuffer + ALIGNMENT_PAGE_SIZE - 1) & ~(ALIGNMENT_PAGE_SIZE - 1));

    UINT64 AlignedVirtualBuffer = (UINT64)((ULONG_PTR)(Buffer + ALIGNMENT_PAGE_SIZE - 1) & ~(ALIGNMENT_PAGE_SIZE - 1));

    DbgPrint("[*] Virtual allocated buffer for VMXON at %llx", Buffer);
    DbgPrint("[*] Virtual aligned allocated buffer for VMXON at %llx", AlignedVirtualBuffer);
    DbgPrint("[*] Aligned physical buffer allocated for VMXON at %llx", AlignedPhysicalBuffer);

    // get IA32_VMX_BASIC_MSR RevisionId

    IA32_VMX_BASIC_MSR basic = {0};

    basic.All = __readmsr(MSR_IA32_VMX_BASIC);

    DbgPrint("[*] MSR_IA32_VMX_BASIC (MSR 0x480) Revision Identifier %llx", basic.Fields.RevisionIdentifier);

    // Changing Revision Identifier
    *(UINT64*)AlignedVirtualBuffer = basic.Fields.RevisionIdentifier;

    int Status = __vmx_on(&AlignedPhysicalBuffer);
    if ( Status )
    {
        DbgPrint("[*] VMXON failed with status %d\n", Status);
        return FALSE;
    }

    GuestState->VmxonRegion = AlignedPhysicalBuffer;

    return TRUE;
}

BOOLEAN
AllocateVmcsRegion(IN VIRTUAL_MACHINE_STATE* GuestState)
{
    //
    // at IRQL > DISPATCH_LEVEL memory allocation routines don't work
    //
    if ( KeGetCurrentIrql() > DISPATCH_LEVEL )
        KeRaiseIrqlToDpcLevel();

    PHYSICAL_ADDRESS PhysicalMax = {0};
    PhysicalMax.QuadPart         = MAXULONG64;

    int VMCSSize = 2 * VMCS_SIZE;
    BYTE* Buffer = MmAllocateContiguousMemory(
        VMCSSize + ALIGNMENT_PAGE_SIZE,
        PhysicalMax); // Allocating a 4-KByte Contigous Memory region

    PHYSICAL_ADDRESS Highest = {0}; //, Lowest = {0};
    Highest.QuadPart         = ~0;

    // BYTE* Buffer = MmAllocateContiguousMemorySpecifyCache(VMXONSize + ALIGNMENT_PAGE_SIZE, Lowest, Highest, Lowest,
    // MmNonCached);

    UINT64 PhysicalBuffer = VirtualToPhysicalAddress(Buffer);
    if ( Buffer == NULL )
    {
        DbgPrint("[*] Error : Couldn't Allocate Buffer for VMCS Region.");
        return FALSE; // ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }
    // zero-out memory
    RtlSecureZeroMemory(Buffer, VMCSSize + ALIGNMENT_PAGE_SIZE);
    UINT64 AlignedPhysicalBuffer =
        (UINT64)((ULONG_PTR)(PhysicalBuffer + ALIGNMENT_PAGE_SIZE - 1) & ~(ALIGNMENT_PAGE_SIZE - 1));

    UINT64 AlignedVirtualBuffer = (UINT64)((ULONG_PTR)(Buffer + ALIGNMENT_PAGE_SIZE - 1) & ~(ALIGNMENT_PAGE_SIZE - 1));

    DbgPrint("[*] Virtual allocated buffer for VMCS at %llx", Buffer);
    DbgPrint("[*] Virtual aligned allocated buffer for VMCS at %llx", AlignedVirtualBuffer);
    DbgPrint("[*] Aligned physical buffer allocated for VMCS at %llx", AlignedPhysicalBuffer);

    // get IA32_VMX_BASIC_MSR RevisionId

    IA32_VMX_BASIC_MSR basic = {0};

    basic.All = __readmsr(MSR_IA32_VMX_BASIC);

    DbgPrint("[*] MSR_IA32_VMX_BASIC (MSR 0x480) Revision Identifier %llx", basic.Fields.RevisionIdentifier);

    // Changing Revision Identifier
    *(UINT64*)AlignedVirtualBuffer = basic.Fields.RevisionIdentifier;

    int Status = __vmx_vmptrld(&AlignedPhysicalBuffer);
    if ( Status )
    {
        DbgPrint("[*] VMCS failed with status %d\n", Status);
        return FALSE;
    }

    GuestState->VmcsRegion = AlignedPhysicalBuffer;

    return TRUE;
}

void
RunOnEachLogicalProcessor(void* (*FunctionPtr)())
{
    KAFFINITY AffinityMask;
    for ( size_t i = 0; i < KeQueryActiveProcessors(); i++ )
    {
        AffinityMask = PowerOfTwo(i);
        KeSetSystemAffinityThread(AffinityMask);

        DbgPrint("=====================================================");
        DbgPrint("Current thread is executing in %d th logical processor.", i);

        FunctionPtr();
    }
}
