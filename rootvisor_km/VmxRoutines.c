#include "VmxRoutines.h"

#include "AsmDefs.h"
#include "Common.h"
#include "Dpc.h"
#include "EptRoutines.h"
#include "GlobalVariables.h"
#include "HypervisorRoutines.h"
#include "MsrDefs.h"
#include "Vmcall.h"

/* Initialize VMX Operation */
BOOLEAN
VmxInitializer()
{
    int ProcessorCount;

    if ( !HvIsVmxSupported() )
    {
        LogError("VMX is not supported in this machine !");
        return FALSE;
    }

    PAGED_CODE();

    ProcessorCount = KeQueryActiveProcessorCount(0);

    // Allocate global variable to hold Guest(s) state
    GuestState = ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(VIRTUAL_MACHINE_STATE) * ProcessorCount, POOLTAG);

    if ( !GuestState )
    {
        LogError("Insufficient memory for guest states");
        return FALSE;
    }

    // Zero memory
    RtlZeroMemory(GuestState, sizeof(VIRTUAL_MACHINE_STATE) * ProcessorCount);

    // Allocate	global variable to hold Ept State
    EptState = ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(EPT_STATE), POOLTAG);

    if ( !EptState )
    {
        LogError("Insufficient memory for EPT state");
        // Cleanup previously allocated memory
        ExFreePoolWithTag(GuestState, POOLTAG);
        GuestState = NULL;
        return FALSE;
    }

    // Zero memory
    RtlZeroMemory(EptState, sizeof(EPT_STATE));

    // Check whether EPT is supported or not
    if ( !EptCheckFeatures() )
    {
        LogError("Your processor doesn't support all EPT features");
        return FALSE;
    }
    else
    {
        // Our processor supports EPT, now let's build MTRR
        LogInfo("Your processor supports all EPT features");

        // Build MTRR Map
        if ( !EptBuildMtrrMap() )
        {
            LogError("Could not build Mtrr memory map");
            return FALSE;
        }
        LogInfo("Mtrr memory map built successfully");
    }

    if ( !EptLogicalProcessorInitialize() )
    {
        // There were some errors in EptLogicalProcessorInitialize
        return FALSE;
    }

    // Allocate and run Vmxon and Vmptrld on all logical cores
    KeGenericCallDpc(VmxDpcBroadcastAllocateVmxonRegions, 0x0);

    // Everything is ok, let's return true
    return TRUE;
}

/* Virtualizing an already running system, this function won't return TRUE as when Vmlaunch is executed the
   rest of the function never executes but returning FALSE is an indication of error.
*/
BOOLEAN
VmxVirtualizeCurrentSystem(PVOID GuestStack)
{
    ULONG64 ErrorCode;
    INT ProcessorID;

    ProcessorID = KeGetCurrentProcessorNumber();

    Log("======================== Virtualizing Current System (Logical Core : 0x%x) ========================",
        ProcessorID);

    // Clear the VMCS State
    if ( !VmxClearVmcsState(&GuestState[ProcessorID]) )
    {
        LogError("Failed to clear vmcs");
        return FALSE;
    }

    // Load VMCS (Set the Current VMCS)
    if ( !VmxLoadVmcs(&GuestState[ProcessorID]) )
    {
        LogError("Failed to load vmcs");
        return FALSE;
    }

    LogInfo("Setting up VMCS for current logical core");
    VmxSetupVmcs(&GuestState[ProcessorID], GuestStack);

    LogInfo("Executing VMLAUNCH on logical core %d", ProcessorID);

    __vmx_vmlaunch();

    /* if Vmlaunch succeed will never be here ! */

    // Execute Vmxoff
    __vmx_off();

    ErrorCode = 0;
    __vmx_vmread(VM_INSTRUCTION_ERROR, &ErrorCode);
    LogError("VMLAUNCH Error : 0x%llx", ErrorCode);

    LogWarning("VMXOFF Executed Successfully");

    DbgBreakPoint();
    return FALSE;
}


/* Broadcast to terminate VMX on all logical cores */
BOOLEAN
VmxTerminate()
{
    int CurrentCoreIndex;
    NTSTATUS Status;

    // Get the current core index
    CurrentCoreIndex = KeGetCurrentProcessorNumber();

    LogInfo("\tTerminating VMX on logical core %d", CurrentCoreIndex);

    // Execute Vmcall to to turn off vmx from Vmx root mode
    Status = AsmVmxVmcall(VMCALL_VMXOFF, NULL, NULL, NULL);

    // Free the destination memory
    MmFreeContiguousMemory((PVOID)GuestState[CurrentCoreIndex].VmxonRegionVirtualAddress);
    MmFreeContiguousMemory((PVOID)GuestState[CurrentCoreIndex].VmcsRegionVirtualAddress);
    ExFreePoolWithTag((PVOID)GuestState[CurrentCoreIndex].VmmStack, POOLTAG);
    ExFreePoolWithTag((PVOID)GuestState[CurrentCoreIndex].MsrBitmapVirtualAddress, POOLTAG);

    if ( Status == STATUS_SUCCESS )
    {
        return TRUE;
    }

    return FALSE;
}

/* Implementation of Vmptrst instruction */
VOID
VmxVmptrst()
{
    PHYSICAL_ADDRESS VmcsPhysicalAddr;
    VmcsPhysicalAddr.QuadPart = 0;
    __vmx_vmptrst((unsigned __int64*)&VmcsPhysicalAddr);

    LogInfo("Vmptrst result : %llx", VmcsPhysicalAddr);
}

/* Clearing Vmcs status using Vmclear instruction */
BOOLEAN
VmxClearVmcsState(VIRTUAL_MACHINE_STATE* CurrentGuestState)
{
    int VmclearStatus;

    // Clear the state of the VMCS to inactive
    VmclearStatus = __vmx_vmclear(&CurrentGuestState->VmcsRegionPhysicalAddress);

    LogInfo("Vmcs Vmclear Status : %d", VmclearStatus);

    if ( VmclearStatus )
    {
        // Otherwise terminate the VMX
        LogWarning("VMCS failed to clear ( status : %d )", VmclearStatus);
        __vmx_off();
        return FALSE;
    }
    return TRUE;
}

/* Implementation of Vmptrld instruction */
BOOLEAN
VmxLoadVmcs(VIRTUAL_MACHINE_STATE* CurrentGuestState)
{

    int VmptrldStatus;

    VmptrldStatus = __vmx_vmptrld(&CurrentGuestState->VmcsRegionPhysicalAddress);
    if ( VmptrldStatus )
    {
        LogWarning("VMCS failed to load ( status : %d )", VmptrldStatus);
        return FALSE;
    }
    return TRUE;
}

/* Create and Configure a Vmcs Layout */
BOOLEAN
VmxSetupVmcs(VIRTUAL_MACHINE_STATE* CurrentGuestState, PVOID GuestStack)
{

    ULONG64 GdtBase                  = 0;
    SEGMENT_SELECTOR SegmentSelector = {0};
    ULONG CpuBasedVmExecControls;
    ULONG SecondaryProcBasedVmExecControls;

    __vmx_vmwrite(HOST_ES_SELECTOR, AsmGetEs() & 0xF8);
    __vmx_vmwrite(HOST_CS_SELECTOR, AsmGetCs() & 0xF8);
    __vmx_vmwrite(HOST_SS_SELECTOR, AsmGetSs() & 0xF8);
    __vmx_vmwrite(HOST_DS_SELECTOR, AsmGetDs() & 0xF8);
    __vmx_vmwrite(HOST_FS_SELECTOR, AsmGetFs() & 0xF8);
    __vmx_vmwrite(HOST_GS_SELECTOR, AsmGetGs() & 0xF8);
    __vmx_vmwrite(HOST_TR_SELECTOR, AsmGetTr() & 0xF8);

    // Setting the link pointer to the required value for 4KB VMCS.
    __vmx_vmwrite(VMCS_LINK_POINTER, ~0ULL);

    __vmx_vmwrite(GUEST_IA32_DEBUGCTL, __readmsr(MSR_IA32_DEBUGCTL) & 0xFFFFFFFF);
    __vmx_vmwrite(GUEST_IA32_DEBUGCTL_HIGH, __readmsr(MSR_IA32_DEBUGCTL) >> 32);

    /* Time-stamp counter offset */
    __vmx_vmwrite(TSC_OFFSET, 0);
    __vmx_vmwrite(TSC_OFFSET_HIGH, 0);

    __vmx_vmwrite(PAGE_FAULT_ERROR_CODE_MASK, 0);
    __vmx_vmwrite(PAGE_FAULT_ERROR_CODE_MATCH, 0);

    __vmx_vmwrite(VM_EXIT_MSR_STORE_COUNT, 0);
    __vmx_vmwrite(VM_EXIT_MSR_LOAD_COUNT, 0);

    __vmx_vmwrite(VM_ENTRY_MSR_LOAD_COUNT, 0);
    __vmx_vmwrite(VM_ENTRY_INTR_INFO_FIELD, 0);

    GdtBase = AsmGetGdtBase();

    HvFillGuestSelectorData((PVOID)GdtBase, ES, AsmGetEs());
    HvFillGuestSelectorData((PVOID)GdtBase, CS, AsmGetCs());
    HvFillGuestSelectorData((PVOID)GdtBase, SS, AsmGetSs());
    HvFillGuestSelectorData((PVOID)GdtBase, DS, AsmGetDs());
    HvFillGuestSelectorData((PVOID)GdtBase, FS, AsmGetFs());
    HvFillGuestSelectorData((PVOID)GdtBase, GS, AsmGetGs());
    HvFillGuestSelectorData((PVOID)GdtBase, LDTR, AsmGetLdtr());
    HvFillGuestSelectorData((PVOID)GdtBase, TR, AsmGetTr());

    __vmx_vmwrite(GUEST_FS_BASE, __readmsr(MSR_FS_BASE));
    __vmx_vmwrite(GUEST_GS_BASE, __readmsr(MSR_GS_BASE));

    CpuBasedVmExecControls = HvAdjustControls(
        CPU_BASED_ACTIVATE_MSR_BITMAP | CPU_BASED_ACTIVATE_SECONDARY_CONTROLS,
        MSR_IA32_VMX_PROCBASED_CTLS);

    __vmx_vmwrite(CPU_BASED_VM_EXEC_CONTROL, CpuBasedVmExecControls);

    LogInfo("Cpu Based VM Exec Controls (Based on MSR_IA32_VMX_PROCBASED_CTLS) : 0x%x", CpuBasedVmExecControls);

    SecondaryProcBasedVmExecControls = HvAdjustControls(
        CPU_BASED_CTL2_RDTSCP | CPU_BASED_CTL2_ENABLE_EPT | CPU_BASED_CTL2_ENABLE_INVPCID |
            CPU_BASED_CTL2_ENABLE_XSAVE_XRSTORS,
        MSR_IA32_VMX_PROCBASED_CTLS2);

    __vmx_vmwrite(SECONDARY_VM_EXEC_CONTROL, SecondaryProcBasedVmExecControls);
    LogInfo(
        "Secondary Proc Based VM Exec Controls (MSR_IA32_VMX_PROCBASED_CTLS2) : 0x%x",
        SecondaryProcBasedVmExecControls);

    __vmx_vmwrite(PIN_BASED_VM_EXEC_CONTROL, HvAdjustControls(0, MSR_IA32_VMX_PINBASED_CTLS));
    __vmx_vmwrite(VM_EXIT_CONTROLS, HvAdjustControls(VM_EXIT_IA32E_MODE, MSR_IA32_VMX_EXIT_CTLS));
    __vmx_vmwrite(VM_ENTRY_CONTROLS, HvAdjustControls(VM_ENTRY_IA32E_MODE, MSR_IA32_VMX_ENTRY_CTLS));

    __vmx_vmwrite(CR3_TARGET_COUNT, 0);
    __vmx_vmwrite(CR3_TARGET_VALUE0, 0);
    __vmx_vmwrite(CR3_TARGET_VALUE1, 0);
    __vmx_vmwrite(CR3_TARGET_VALUE2, 0);
    __vmx_vmwrite(CR3_TARGET_VALUE3, 0);

    __vmx_vmwrite(CR0_GUEST_HOST_MASK, 0);
    __vmx_vmwrite(CR4_GUEST_HOST_MASK, 0);

    __vmx_vmwrite(CR0_READ_SHADOW, 0);
    __vmx_vmwrite(CR4_READ_SHADOW, 0);

    __vmx_vmwrite(GUEST_CR0, __readcr0());
    __vmx_vmwrite(GUEST_CR3, __readcr3());
    __vmx_vmwrite(GUEST_CR4, __readcr4());

    __vmx_vmwrite(GUEST_DR7, 0x400);

    __vmx_vmwrite(HOST_CR0, __readcr0());
    __vmx_vmwrite(HOST_CR4, __readcr4());

    /*
    Because we may be executing in an arbitrary user-mode, process as part
    of the DPC interrupt we execute in We have to save Cr3, for HOST_CR3
    */

    __vmx_vmwrite(HOST_CR3, InitiateCr3);

    __vmx_vmwrite(GUEST_GDTR_BASE, AsmGetGdtBase());
    __vmx_vmwrite(GUEST_IDTR_BASE, AsmGetIdtBase());
    __vmx_vmwrite(GUEST_GDTR_LIMIT, AsmGetGdtLimit());
    __vmx_vmwrite(GUEST_IDTR_LIMIT, AsmGetIdtLimit());

    __vmx_vmwrite(GUEST_RFLAGS, AsmGetRflags());

    __vmx_vmwrite(GUEST_SYSENTER_CS, __readmsr(MSR_IA32_SYSENTER_CS));
    __vmx_vmwrite(GUEST_SYSENTER_EIP, __readmsr(MSR_IA32_SYSENTER_EIP));
    __vmx_vmwrite(GUEST_SYSENTER_ESP, __readmsr(MSR_IA32_SYSENTER_ESP));

    HvGetSegmentDescriptor(&SegmentSelector, AsmGetTr(), (PUCHAR)AsmGetGdtBase());
    __vmx_vmwrite(HOST_TR_BASE, SegmentSelector.BASE);

    __vmx_vmwrite(HOST_FS_BASE, __readmsr(MSR_FS_BASE));
    __vmx_vmwrite(HOST_GS_BASE, __readmsr(MSR_GS_BASE));

    __vmx_vmwrite(HOST_GDTR_BASE, AsmGetGdtBase());
    __vmx_vmwrite(HOST_IDTR_BASE, AsmGetIdtBase());

    __vmx_vmwrite(HOST_IA32_SYSENTER_CS, __readmsr(MSR_IA32_SYSENTER_CS));
    __vmx_vmwrite(HOST_IA32_SYSENTER_EIP, __readmsr(MSR_IA32_SYSENTER_EIP));
    __vmx_vmwrite(HOST_IA32_SYSENTER_ESP, __readmsr(MSR_IA32_SYSENTER_ESP));

    // Set MSR Bitmaps
    __vmx_vmwrite(MSR_BITMAP, CurrentGuestState->MsrBitmapPhysicalAddress);

    // Set up EPT
    __vmx_vmwrite(EPT_POINTER, EptState->EptPointer.Flags);

    // setup guest rsp
    __vmx_vmwrite(GUEST_RSP, (ULONG64)GuestStack);

    // setup guest rip
    __vmx_vmwrite(GUEST_RIP, (ULONG64)AsmVmxRestoreState);

    __vmx_vmwrite(HOST_RSP, ((ULONG64)CurrentGuestState->VmmStack + VMM_STACK_SIZE - 1));
    __vmx_vmwrite(HOST_RIP, (ULONG64)AsmVmexitHandler);

    return TRUE;
}


/* Resume vm using Vmresume instruction */
VOID
VmxVmresume()
{
    ULONG64 ErrorCode;

    __vmx_vmresume();

    // if VMRESUME succeed will never be here !

    ErrorCode = 0;
    __vmx_vmread(VM_INSTRUCTION_ERROR, &ErrorCode);
    __vmx_off();
    LogError("Error in executing Vmresume , status : 0x%llx", ErrorCode);

    // It's such a bad error because we don't where to go !
    // prefer to break
    DbgBreakPoint();
}

/* Prepare and execute Vmxoff instruction */
VOID
VmxVmxoff()
{
    int CurrentProcessorIndex;
    UINT64 GuestRSP; // Save a pointer to guest rsp for times that we want to return to previous guest stateS
    UINT64 GuestRIP; // Save a pointer to guest rip for times that we want to return to previous guest state
    UINT64 GuestCr3;
    UINT64 ExitInstructionLength;


    // Initialize the variables
    ExitInstructionLength = 0;
    GuestRIP              = 0;
    GuestRSP              = 0;

    CurrentProcessorIndex = KeGetCurrentProcessorNumber();

    /*
    According to SimpleVisor :
        Our callback routine may have interrupted an arbitrary user process,
        and therefore not a thread running with a system-wide page directory.
        Therefore if we return back to the original caller after turning off
        VMX, it will keep our current "host" CR3 value which we set on entry
        to the PML4 of the SYSTEM process. We want to return back with the
        correct value of the "guest" CR3, so that the currently executing
        process continues to run with its expected address space mappings.
    */

    __vmx_vmread(GUEST_CR3, &GuestCr3);
    __writecr3(GuestCr3);

    // Read guest rsp and rip
    __vmx_vmread(GUEST_RIP, &GuestRIP);
    __vmx_vmread(GUEST_RSP, &GuestRSP);

    // Read instruction length
    __vmx_vmread(VM_EXIT_INSTRUCTION_LEN, &ExitInstructionLength);
    GuestRIP += ExitInstructionLength;

    // Set the previous registe states
    GuestState[CurrentProcessorIndex].VmxoffState.GuestRip = GuestRIP;
    GuestState[CurrentProcessorIndex].VmxoffState.GuestRsp = GuestRSP;

    // Notify the Vmexit handler that VMX already turned off
    GuestState[CurrentProcessorIndex].VmxoffState.IsVmxoffExecuted = TRUE;

    // Execute Vmxoff
    __vmx_off();
}

/* Main Vmexit events handler */
BOOLEAN
VmxVmexitHandler(PGUEST_REGS GuestRegs)
{
    int CurrentProcessorIndex;
    UINT64 GuestPhysicalAddr;
    UINT64 GuestRip;
    ULONG ExitReason;
    ULONG ExitQualification;
    ULONG Rflags;
    ULONG EcxReg;
    // ULONG ExitInstructionLength;

    CurrentProcessorIndex = KeGetCurrentProcessorNumber();

    // Indicates we are in Vmx root mode in this logical core
    GuestState[CurrentProcessorIndex].IsOnVmxRootMode = TRUE;

    GuestState[CurrentProcessorIndex].IncrementRip = TRUE;

    ExitReason = 0;
    __vmx_vmread(VM_EXIT_REASON, &ExitReason);

    ExitQualification = 0;
    __vmx_vmread(EXIT_QUALIFICATION, &ExitQualification);

    ExitReason &= 0xffff;

    // Debugging purpose
    // LogInfo("VM_EXIT_REASON : 0x%llx", ExitReason);
    // LogInfo("EXIT_QUALIFICATION : 0x%llx", ExitQualification);

    switch ( ExitReason )
    {
    case EXIT_REASON_TRIPLE_FAULT:
    {
        DbgBreakPoint();
        break;
    }

        // 25.1.2  Instructions That Cause VM Exits Unconditionally
        // The following instructions cause VM exits when they are executed in VMX non-root operation: CPUID, GETSEC,
        // INVD, and XSETBV. This is also true of instructions introduced with VMX, which include: INVEPT, INVVPID,
        // VMCALL, VMCLEAR, VMLAUNCH, VMPTRLD, VMPTRST, VMRESUME, VMXOFF, and VMXON.

    case EXIT_REASON_VMCLEAR:
    case EXIT_REASON_VMPTRLD:
    case EXIT_REASON_VMPTRST:
    case EXIT_REASON_VMREAD:
    case EXIT_REASON_VMRESUME:
    case EXIT_REASON_VMWRITE:
    case EXIT_REASON_VMXOFF:
    case EXIT_REASON_VMXON:
    case EXIT_REASON_VMLAUNCH:
    {
        /* Target guest tries to execute VM Instruction, it probably causes a fatal error or system halt as the system
           might think it has VMX feature enabled while it's not available due to our use of hypervisor.	*/

        LogWarning("Guest attempted to execute VMX instruction (exit reason: 0x%llx)", ExitReason);

        Rflags = 0;
        __vmx_vmread(GUEST_RFLAGS, &Rflags);
        __vmx_vmwrite(GUEST_RFLAGS, Rflags | RFLAGS_CF_MASK); // Set CF=1 to indicate VM instruction failure
        break;
    }

    case EXIT_REASON_CR_ACCESS:
    {
        HvHandleControlRegisterAccess(GuestRegs);
        break;
    }
    case EXIT_REASON_MSR_READ:
    {

        EcxReg = GuestRegs->rcx & 0xffffffff;
        HvHandleMsrRead(GuestRegs);

        break;
    }
    case EXIT_REASON_MSR_WRITE:
    {
        EcxReg = GuestRegs->rcx & 0xffffffff;
        HvHandleMsrWrite(GuestRegs);

        break;
    }
    case EXIT_REASON_CPUID:
    {
        HvHandleCpuid(GuestRegs);

        /***  It's better to turn off hypervisor from Vmcall ***/

        /*
        VmexitStatus = HvHandleCpuid(GuestRegs);

        // Detect whether we have to turn off VMX or Not
        if (VmexitStatus)
        {
            // We have to save GUEST_RIP & GUEST_RSP somewhere to restore them directly

            ExitInstructionLength = 0;
            GuestRIP = 0;
            GuestRSP = 0;
            __vmx_vmread(GUEST_RIP, &GuestRIP);
            __vmx_vmread(GUEST_RSP, &GuestRSP);
            __vmx_vmread(VM_EXIT_INSTRUCTION_LEN, &ExitInstructionLength);
            GuestRIP += ExitInstructionLength;
        }
        */
        break;
    }

    case EXIT_REASON_IO_INSTRUCTION:
    {
        DbgBreakPoint();
        break;
    }
    case EXIT_REASON_EPT_VIOLATION:
    {
        // Reading guest physical address
        GuestPhysicalAddr = 0;
        __vmx_vmread(GUEST_PHYSICAL_ADDRESS, &GuestPhysicalAddr);
        LogInfo("Guest Physical Address : 0x%llx", GuestPhysicalAddr);

        // Reading guest's RIP
        GuestRip = 0;
        __vmx_vmread(GUEST_RIP, &GuestRip);
        LogInfo("Guest Rip : 0x%llx", GuestRip);

        if ( !EptHandleEptViolation(ExitQualification, GuestPhysicalAddr) )
        {
            LogError("There were errors in handling Ept Violation");
        }

        break;
    }
    case EXIT_REASON_EPT_MISCONFIG:
    {
        GuestPhysicalAddr = 0;
        __vmx_vmread(GUEST_PHYSICAL_ADDRESS, &GuestPhysicalAddr);

        EptHandleMisconfiguration(GuestPhysicalAddr);

        break;
    }
    case EXIT_REASON_VMCALL:
    {
        GuestRegs->rax = VmxVmcallHandler(GuestRegs->rcx, GuestRegs->rdx, GuestRegs->r8, GuestRegs->r9);
        break;
    }
    default:
    {
        LogWarning("Unkown Vmexit, reason : 0x%llx", ExitReason);
        DbgBreakPoint();
        break;
    }
    }

    if ( !GuestState[CurrentProcessorIndex].VmxoffState.IsVmxoffExecuted &&
         GuestState[CurrentProcessorIndex].IncrementRip )
    {
        HvResumeToNextInstruction();
    }

    // Set indicator of Vmx noon root mode to false
    GuestState[CurrentProcessorIndex].IsOnVmxRootMode = FALSE;

    if ( GuestState[CurrentProcessorIndex].VmxoffState.IsVmxoffExecuted )
    {
        return TRUE;
    }

    return FALSE;
}


/* Allocates Vmx regions for all logical cores (Vmxon region and Vmcs region) */
BOOLEAN
VmxDpcBroadcastAllocateVmxonRegions(
    struct _KDPC* Dpc,
    PVOID DeferredContext,
    PVOID SystemArgument1,
    PVOID SystemArgument2)
{
    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(DeferredContext);

    int CurrentProcessorNumber = KeGetCurrentProcessorNumber();

    LogInfo("Allocating Vmx Regions for logical core %d", CurrentProcessorNumber);

    // Enabling VMX Operation
    AsmEnableVmxOperation();

    LogInfo("VMX-Operation Enabled Successfully");

    if ( !VmxAllocateVmxonRegion(&GuestState[CurrentProcessorNumber]) )
    {
        LogError("Error in allocating memory for Vmxon region");
        return FALSE;
    }
    if ( !VmxAllocateVmcsRegion(&GuestState[CurrentProcessorNumber]) )
    {
        LogError("Error in allocating memory for Vmcs region");
        return FALSE;
    }

    // Wait for all DPCs to synchronize at this point
    KeSignalCallDpcSynchronize(SystemArgument2);

    // Mark the DPC as being complete
    KeSignalCallDpcDone(SystemArgument1);

    return TRUE;
}

/* Allocates Vmxon region and set the Revision ID based on IA32_VMX_BASIC_MSR */
BOOLEAN
VmxAllocateVmxonRegion(VIRTUAL_MACHINE_STATE* CurrentGuestState)
{
    PHYSICAL_ADDRESS PhysicalMax   = {0};
    IA32_VMX_BASIC_MSR VmxBasicMsr = {0};
    int VmxonSize;
    int VmxonStatus;
    BYTE* VmxonRegion;
    UINT64 VmxonRegionPhysicalAddr;
    UINT64 AlignedVmxonRegion;
    UINT64 AlignedVmxonRegionPhysicalAddr;


    // at IRQL > DISPATCH_LEVEL memory allocation routines don't work
    if ( KeGetCurrentIrql() > DISPATCH_LEVEL )
        KeRaiseIrqlToDpcLevel();

    PhysicalMax.QuadPart = MAXULONG64;

    VmxonSize = 2 * VMXON_SIZE;

    // Allocating a 4-KByte Contigous Memory region
    VmxonRegion = MmAllocateContiguousMemory(VmxonSize + ALIGNMENT_PAGE_SIZE, PhysicalMax);

    if ( VmxonRegion == NULL )
    {
        LogError("Couldn't Allocate Buffer for VMXON Region.");
        return FALSE;
    }

    VmxonRegionPhysicalAddr = VirtualAddressToPhysicalAddress(VmxonRegion);

    // zero-out memory
    RtlSecureZeroMemory(VmxonRegion, VmxonSize + ALIGNMENT_PAGE_SIZE);


    AlignedVmxonRegion = (UINT64)((ULONG_PTR)(VmxonRegion + ALIGNMENT_PAGE_SIZE - 1) & ~(ALIGNMENT_PAGE_SIZE - 1));
    LogInfo("VMXON Region Address : %llx", AlignedVmxonRegion);

    // 4 kb >= buffers are aligned, just a double check to ensure if it's aligned
    AlignedVmxonRegionPhysicalAddr =
        (UINT64)((ULONG_PTR)(VmxonRegionPhysicalAddr + ALIGNMENT_PAGE_SIZE - 1) & ~(ALIGNMENT_PAGE_SIZE - 1));
    LogInfo("VMXON Region Physical Address : %llx", AlignedVmxonRegionPhysicalAddr);

    // get IA32_VMX_BASIC_MSR RevisionId
    VmxBasicMsr.All = __readmsr(MSR_IA32_VMX_BASIC);
    LogInfo("Revision Identifier (MSR_IA32_VMX_BASIC - MSR 0x480) : 0x%x", VmxBasicMsr.Fields.RevisionIdentifier);

    // Changing Revision Identifier
    *(UINT64*)AlignedVmxonRegion = VmxBasicMsr.Fields.RevisionIdentifier;

    // Execute Vmxon instruction
    VmxonStatus = __vmx_on(&AlignedVmxonRegionPhysicalAddr);
    if ( VmxonStatus )
    {
        LogError("Executing Vmxon instruction failed with status : %d", VmxonStatus);
        return FALSE;
    }

    CurrentGuestState->VmxonRegionPhysicalAddress = AlignedVmxonRegionPhysicalAddr;

    // We save the allocated buffer (not the aligned buffer) because we want to free it in vmx termination
    CurrentGuestState->VmxonRegionVirtualAddress = (UINT64)VmxonRegion;

    return TRUE;
}

/* Allocate Vmcs region and set the Revision ID based on IA32_VMX_BASIC_MSR */
BOOLEAN
VmxAllocateVmcsRegion(VIRTUAL_MACHINE_STATE* CurrentGuestState)
{
    PHYSICAL_ADDRESS PhysicalMax = {0};
    int VmcsSize;
    BYTE* VmcsRegion;
    UINT64 VmcsPhysicalAddr;
    UINT64 AlignedVmcsRegion;
    UINT64 AlignedVmcsRegionPhysicalAddr;
    IA32_VMX_BASIC_MSR VmxBasicMsr = {0};


    // at IRQL > DISPATCH_LEVEL memory allocation routines don't work
    if ( KeGetCurrentIrql() > DISPATCH_LEVEL )
        KeRaiseIrqlToDpcLevel();

    PhysicalMax.QuadPart = MAXULONG64;

    VmcsSize   = 2 * VMCS_SIZE;
    VmcsRegion = MmAllocateContiguousMemory(
        VmcsSize + ALIGNMENT_PAGE_SIZE,
        PhysicalMax); // Allocating a 4-KByte Contigous Memory region

    if ( VmcsRegion == NULL )
    {
        LogError("Couldn't Allocate Buffer for VMCS Region.");
        return FALSE;
    }
    RtlSecureZeroMemory(VmcsRegion, VmcsSize + ALIGNMENT_PAGE_SIZE);

    VmcsPhysicalAddr = VirtualAddressToPhysicalAddress(VmcsRegion);

    AlignedVmcsRegion = (UINT64)((ULONG_PTR)(VmcsRegion + ALIGNMENT_PAGE_SIZE - 1) & ~(ALIGNMENT_PAGE_SIZE - 1));
    LogInfo("VMCS Region Address : %llx", AlignedVmcsRegion);

    AlignedVmcsRegionPhysicalAddr =
        (UINT64)((ULONG_PTR)(VmcsPhysicalAddr + ALIGNMENT_PAGE_SIZE - 1) & ~(ALIGNMENT_PAGE_SIZE - 1));
    LogInfo("VMCS Region Physical Address : %llx", AlignedVmcsRegionPhysicalAddr);

    // get IA32_VMX_BASIC_MSR RevisionId
    VmxBasicMsr.All = __readmsr(MSR_IA32_VMX_BASIC);
    LogInfo("Revision Identifier (MSR_IA32_VMX_BASIC - MSR 0x480) : 0x%x", VmxBasicMsr.Fields.RevisionIdentifier);


    // Changing Revision Identifier
    *(UINT64*)AlignedVmcsRegion = VmxBasicMsr.Fields.RevisionIdentifier;

    CurrentGuestState->VmcsRegionPhysicalAddress = AlignedVmcsRegionPhysicalAddr;
    // We save the allocated buffer (not the aligned buffer) because we want to free it in vmx termination
    CurrentGuestState->VmcsRegionVirtualAddress = (UINT64)VmcsRegion;

    return TRUE;
}

/* Allocate VMM Stack */
BOOLEAN
VmxAllocateVmmStack(SIZE_T ProcessorID)
{
    UINT64 VmmStack;

    // Input validation
    if ( ProcessorID >= KeQueryActiveProcessorCount(0) )
    {
        LogError("Invalid processor ID: %zu", ProcessorID);
        return FALSE;
    }

    // Allocate stack for the VM Exit Handler.
    VmmStack = (UINT64)ExAllocatePool2(POOL_FLAG_NON_PAGED, VMM_STACK_SIZE, POOLTAG);

    if ( VmmStack == (UINT64)NULL )
    {
        LogError("Insufficient memory for VMM stack allocation (processor %zu)", ProcessorID);
        return FALSE;
    }

    GuestState[ProcessorID].VmmStack = VmmStack;
    RtlZeroMemory((VOID*)GuestState[ProcessorID].VmmStack, VMM_STACK_SIZE);

    LogInfo("VMM Stack allocated for processor %zu at address: 0x%llx", ProcessorID, GuestState[ProcessorID].VmmStack);

    return TRUE;
}

/* Allocate a buffer forr Msr Bitmap */
BOOLEAN
VmxAllocateMsrBitmap(SIZE_T ProcessorID)
{
    // Allocate memory for MSRBitMap
    GuestState[ProcessorID].MsrBitmapVirtualAddress =
        (UINT64)ExAllocatePool2(POOL_FLAG_NON_PAGED, PAGE_SIZE, POOLTAG); // should be aligned

    if ( (PVOID)GuestState[ProcessorID].MsrBitmapVirtualAddress == NULL )
    {
        LogError("Insufficient memory in allocationg Msr bitmaps");
        return FALSE;
    }
    RtlZeroMemory((PVOID)GuestState[ProcessorID].MsrBitmapVirtualAddress, PAGE_SIZE);

    GuestState[ProcessorID].MsrBitmapPhysicalAddress =
        VirtualAddressToPhysicalAddress((PVOID)GuestState[ProcessorID].MsrBitmapVirtualAddress);

    LogInfo("Msr Bitmap Virtual Address : 0x%llx", GuestState[ProcessorID].MsrBitmapVirtualAddress);
    LogInfo("Msr Bitmap Physical Address : 0x%llx", GuestState[ProcessorID].MsrBitmapPhysicalAddress);

    // (Uncomment if you want to break on RDMSR and WRMSR to a special MSR Register)

    /*
    if (HvSetMsrBitmap(0xc0000082, ProcessorID, TRUE, TRUE))
    {
        LogError("Invalid parameters sent to the HvSetMsrBitmap function");
        return FALSE;
    }
    */

    return TRUE;
}
