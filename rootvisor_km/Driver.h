#pragma once

#include <ntddk.h>

// Load & Unload
NTSTATUS
DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath);
VOID
DrvUnload(PDRIVER_OBJECT DriverObject);

// IRP Major Functions
NTSTATUS
DrvCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS
DrvClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS
DrvUnsupported(PDEVICE_OBJECT DeviceObject, PIRP Irp);