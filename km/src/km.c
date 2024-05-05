#include <ntifs.h>
#include <ntddk.h>
#include <intrin.h>

#include "../inc/mm.h"
#include "../inc/remap.h"
#include "../inc/nthooks.h"

#define CTL_ADDCURRENTTHREAD            CTL_CODE( FILE_DEVICE_UNKNOWN, 0x20, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define CTL_DELCURRENTTHREAD            CTL_CODE( FILE_DEVICE_UNKNOWN, 0x10, METHOD_BUFFERED, FILE_ANY_ACCESS )

#define CTL_ADDBYIDTHREAD               CTL_CODE( FILE_DEVICE_UNKNOWN, 0x22, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define CTL_DELBYIDTHREAD               CTL_CODE( FILE_DEVICE_UNKNOWN, 0x12, METHOD_BUFFERED, FILE_ANY_ACCESS )

#define CTL_ENABLEGLOBALHOOK            CTL_CODE( FILE_DEVICE_UNKNOWN, 0x40, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define CTL_DISABLEGLOBALHOOK           CTL_CODE( FILE_DEVICE_UNKNOWN, 0x80, METHOD_BUFFERED, FILE_ANY_ACCESS )

#define CTL_DEFINENEWRMPAGE             CTL_CODE( FILE_DEVICE_UNKNOWN, 0x200, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define CTL_DELETEOLDRMPAGE             CTL_CODE( FILE_DEVICE_UNKNOWN, 0x100, METHOD_BUFFERED, FILE_ANY_ACCESS )

#define CTL_PANIC                       CTL_CODE( FILE_DEVICE_UNKNOWN, 0x102, METHOD_BUFFERED, FILE_ANY_ACCESS )

void DriverUnload(PDRIVER_OBJECT DriverObject) {
    NtiDeactivateHookContextSwitch();
    NtiRemoveHookContextSwitch();
    IoDeleteSymbolicLink(&DosSymlink);
    IoDeleteDevice(DriverObject->DeviceObject);
    if (CRemappedPages > 0) {
        MeRestoreRemappablePages(RemapList);
    }

    for (int i = 0; i < AMT_REMAPPABLE_PG; i++) {
        MeDeleteRemappablePage(&RemapList[i]);
    }

    memset(ThreadList, 0, sizeof(ThreadList));
}

// deviceiocontrol
NTSTATUS IODispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);

    PIO_STACK_LOCATION Stack = IoGetCurrentIrpStackLocation(Irp);
    ULONG IoControlCode = Stack->Parameters.DeviceIoControl.IoControlCode;
    ULONG InputBufferLength = Stack->Parameters.DeviceIoControl.InputBufferLength;
    char* SystemBuffer = (char*)Irp->AssociatedIrp.SystemBuffer;
    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    PMMREMAP Remap = 0;

    if (Stack->MajorFunction != IRP_MJ_DEVICE_CONTROL) {
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }

	switch (IoControlCode)
	{
        case CTL_PANIC:
        {
            NtiDeactivateHookContextSwitch();
            NtiRemoveHookContextSwitch();
            if (CRemappedPages > 0) {
                MeRestoreRemappablePages(RemapList);
            }

            for (int i = 0; i < AMT_REMAPPABLE_PG; i++) {
                MeDeleteRemappablePage(&RemapList[i]);
            }

            memset(ThreadList, 0, sizeof(ThreadList));
            Status = STATUS_SUCCESS;
            break;
        }

        case CTL_ADDBYIDTHREAD:
        {
            if (InputBufferLength != sizeof(INT64)) {
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            ThreadList[*(INT64*)SystemBuffer] = 1;
            Status = STATUS_SUCCESS;
            break;
        }

        case CTL_DELBYIDTHREAD:
        {
            if (InputBufferLength != sizeof(INT64)) {
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            ThreadList[*(INT64*)SystemBuffer] = 0;
            Status = STATUS_SUCCESS;
            break;
        }

        case CTL_ADDCURRENTTHREAD:
        {
            ThreadList[(INT64)PsGetCurrentThreadId()] = 1;
            MeRemapDispatch();
            Status = STATUS_SUCCESS;
            break;
        }

        case CTL_DELCURRENTTHREAD:
        {
            ThreadList[(INT64)PsGetCurrentThreadId()] = 0; // index 1
            MeRemapDispatch();
            Status = STATUS_SUCCESS;
            break;
        }

        case CTL_ENABLEGLOBALHOOK:
        {
            if (NtiInstallHookContextSwitch((PVOID)NtipCSCallback) == STATUS_SUCCESS) {
                if (NtiActivateHookContextSwitch() == 1) {
                    Status = STATUS_SUCCESS;
                    break;
                }
            }
            break;
        }

        case CTL_DISABLEGLOBALHOOK:
        {
            if (NtiDeactivateHookContextSwitch() == STATUS_SUCCESS) {
                if (NtiRemoveHookContextSwitch() == 1) {
                    Status = STATUS_SUCCESS;
                    break;
                }
            }
            break;
        }

        case CTL_DEFINENEWRMPAGE:
        {
            if (InputBufferLength != sizeof(MMREMAP)) {
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            // else trust the caller that they didnt fuck up
            Remap = (PMMREMAP)SystemBuffer;
            if (Remap->RemapPage > AMT_REMAPPABLE_PG) {
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            Status = MeSetupRemappablePage(Remap->TVirtualPage, Remap->TProcessId, Remap->SVirtualPage, Remap->SProcessId, &RemapList[Remap->RemapPage]);
            break;
        }

        case CTL_DELETEOLDRMPAGE:
        {
            if (InputBufferLength != sizeof(INT64)) {
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            Remap = (PMMREMAP)SystemBuffer;
            MeDeleteRemappablePage(&RemapList[Remap->RemapList]);
            Status = STATUS_SUCCESS;
            break;
        }
	}

    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    PDEVICE_OBJECT DeviceObj = 0;
    Status = IoCreateDevice(DriverObject, 0, &DeviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObj);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    DriverObject->DriverUnload = DriverUnload;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = IODispatch;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = IODispatch;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = IODispatch;

    Status = IoCreateSymbolicLink(&DosSymlink, &DeviceName);
    if (!NT_SUCCESS(Status)) {
        IoDeleteSymbolicLink(&DosSymlink);
        IoDeleteDevice(DeviceObj);
        return Status;
    }

    Status = STATUS_SUCCESS;
    return Status;
}