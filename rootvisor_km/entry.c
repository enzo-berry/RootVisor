#include <ntddk.h>
#include <wdf.h>
#include <wdm.h>

#include "Asm.h"
#include "Utils.h"
#include "Vmx.h"

NTSTATUS
DrvUnsupported(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    DbgPrint("[*] This function is not supported :( !\n");

    Irp->IoStatus.Status      = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

NTSTATUS
DrvRead(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    DbgPrint("[*] Read Not implemented yet :( !\n");

    Irp->IoStatus.Status      = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

NTSTATUS
DrvWrite(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    DbgPrint("[*] Write Not implemented yet :( !\n");

    Irp->IoStatus.Status      = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

NTSTATUS
DrvClose(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    DbgPrint("[*] DrvClose Called !\n");

    //
    // executing VMXOFF on every logical processor
    //
    TerminateVmx();

    Irp->IoStatus.Status      = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

NTSTATUS
DrvCreate(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    DbgPrint("[*] DrvCreate Called !\n");

    if ( InitializeVmx() )
    {
        DbgPrint("[*] VMX Initiated Successfully.\n");
    }

    Irp->IoStatus.Status      = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

VOID
DrvUnload(PDRIVER_OBJECT DriverObject)
{
    UNICODE_STRING DosDeviceName;

    DbgPrint("[*] DrvUnload Called !\n");

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
        DbgPrint("[*] IoControlCode: %ul\n", IrpStack->Parameters.DeviceIoControl.IoControlCode);
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

    DbgPrint("[*] DriverEntry Called.\n");

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

        DbgPrint("[*] Setting Devices major functions.\n");
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
