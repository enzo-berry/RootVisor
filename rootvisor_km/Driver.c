#include "Driver.h"

#include <ntddk.h>
#include <wdf.h>
#include <wdm.h>

#include "Common.h"
#include "HypervisorRoutines.h"

/* Main Driver Entry in the case of driver load */
NTSTATUS
DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    NTSTATUS Ntstatus           = STATUS_SUCCESS;
    UINT64 Index                = 0;
    PDEVICE_OBJECT DeviceObject = NULL;
    UNICODE_STRING DriverName, DosDeviceName;

    UNREFERENCED_PARAMETER(RegistryPath);

    LogInfo("Rootvisor loaded :)");

    RtlInitUnicodeString(&DriverName, L"\\Device\\MyHypervisorDevice");

    RtlInitUnicodeString(&DosDeviceName, L"\\DosDevices\\MyHypervisorDevice");

    Ntstatus = IoCreateDevice(
        DriverObject,
        0,
        &DriverName,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &DeviceObject);

    if ( Ntstatus == STATUS_SUCCESS )
    {
        for ( Index = 0; Index < IRP_MJ_MAXIMUM_FUNCTION; Index++ )
            DriverObject->MajorFunction[Index] = DrvUnsupported;

        LogInfo("Setting device major functions");
        DriverObject->MajorFunction[IRP_MJ_CLOSE]  = DrvClose;
        DriverObject->MajorFunction[IRP_MJ_CREATE] = DrvCreate;

        DriverObject->DriverUnload = DrvUnload;
        IoCreateSymbolicLink(&DosDeviceName, &DriverName);
    }

    return Ntstatus;
}

/* Run in the case of driver unload to unregister the devices */
VOID
DrvUnload(PDRIVER_OBJECT DriverObject)
{
    UNICODE_STRING DosDeviceName;

    RtlInitUnicodeString(&DosDeviceName, L"\\DosDevices\\MyHypervisorDevice");
    IoDeleteSymbolicLink(&DosDeviceName);
    IoDeleteDevice(DriverObject->DeviceObject);

    LogWarning("Hypervisor From Scratch's driver unloaded");
}

/* IRP_MJ_CREATE Function handler */
NTSTATUS
DrvCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    LogInfo("Rootvisor Started...");

    if ( HvVmxInitialize() )
    {
        LogInfo("Rootvisor loaded successfully :)");
    }
    else
    {
        LogError("Rootvisor was not loaded :(");
    }

    Irp->IoStatus.Status      = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

/* IRP_MJ_CLOSE Function handler*/
NTSTATUS
DrvClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    LogInfo("Terminating VMX...");

    // Terminating Vmx
    HvTerminateVmx();
    LogInfo("VMX Operation turned off successfully :)");


    Irp->IoStatus.Status      = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

/* Unsupported message for all other IRP_MJ_* handlers */
NTSTATUS
DrvUnsupported(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    LogWarning("This function is not supported :(");

    Irp->IoStatus.Status      = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}