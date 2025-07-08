#include <ntddk.h>
#include <wdf.h>
#include <wdm.h>

#include "asm.h"
#include "macros.h"

NTSTATUS
DrvUnsupported(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    PRINT("This function is not supported :( !");

    Irp->IoStatus.Status      = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

NTSTATUS
DrvRead(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    PRINT("Read Not implemented yet :( !");

    Irp->IoStatus.Status      = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

NTSTATUS
DrvWrite(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    PRINT("Write Not implemented yet :( !");

    Irp->IoStatus.Status      = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

NTSTATUS
DrvClose(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    PRINT("DrvClose Not implemented yet :( !");

    Irp->IoStatus.Status      = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

NTSTATUS
DrvCreate(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    //
    // Enabling VMX Operation
    //
    AsmEnableVmxOperation();
    PRINT("VMX Operation Enabled Successfully !");

    Irp->IoStatus.Status      = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

VOID
DrvUnload(PDRIVER_OBJECT DriverObject)
{
    UNICODE_STRING DosDeviceName;

    PRINT("DrvUnload Called !");

    RtlInitUnicodeString(&DosDeviceName, L"\\DosDevices\\rootvisor");

    IoDeleteSymbolicLink(&DosDeviceName);
    IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS
DrvIoctlDispatcher(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{

    PIO_STACK_LOCATION IrpStack;
    ULONG InBufLength;
    ULONG OutBufLength;
    NTSTATUS NtStatus = STATUS_SUCCESS;

    IrpStack     = IoGetCurrentIrpStackLocation(Irp);
    InBufLength  = IrpStack->Parameters.DeviceIoControl.InputBufferLength;
    OutBufLength = IrpStack->Parameters.DeviceIoControl.OutputBufferLength;

    UNREFERENCED_PARAMETER(DeviceObject);

    PAGED_CODE();

    if ( !InBufLength || !OutBufLength )
    {
        NtStatus = STATUS_INVALID_PARAMETER;
        goto End;
    }


    switch ( IrpStack->Parameters.DeviceIoControl.IoControlCode )
    {
    default:
        PRINT("IoControlCode: %ul", IrpStack->Parameters.DeviceIoControl.IoControlCode);
        break;
    }

End:
    Irp->IoStatus.Status = NtStatus;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return NtStatus;
}

NTSTATUS
DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS NtStatus           = STATUS_SUCCESS;
    PDEVICE_OBJECT DeviceObject = NULL;
    UNICODE_STRING DriverName, DosDeviceName;

    PRINT("DriverEntry Called.");

    RtlInitUnicodeString(&DriverName, L"\\Device\\rootvisor");
    RtlInitUnicodeString(&DosDeviceName, L"\\DosDevices\\rootvisor");

    NtStatus = IoCreateDevice(
        DriverObject,
        0,
        &DriverName,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &DeviceObject);

    if ( NtStatus == STATUS_SUCCESS )
    {
        for ( LONG Index = 0; Index < IRP_MJ_MAXIMUM_FUNCTION; Index++ )
        {
            DriverObject->MajorFunction[Index] = DrvUnsupported;
        }

        PRINT("Setting Devices major functions.");
        DriverObject->MajorFunction[IRP_MJ_CLOSE]          = DrvClose;
        DriverObject->MajorFunction[IRP_MJ_CREATE]         = DrvCreate;
        DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DrvIoctlDispatcher;

        DriverObject->MajorFunction[IRP_MJ_READ]  = DrvRead;
        DriverObject->MajorFunction[IRP_MJ_WRITE] = DrvWrite;

        DriverObject->DriverUnload = DrvUnload;

        IoCreateSymbolicLink(&DosDeviceName, &DriverName);
    }
    return NtStatus;
}
