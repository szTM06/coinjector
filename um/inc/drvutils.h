#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>

typedef struct _MMREMAP {
	HANDLE		TProcessId;
	PVOID		TVirtualPage;
	HANDLE		SProcessId;
	PVOID		SVirtualPage;
	int			RemapList;
	int			RemapPage;
} MMREMAP, * PMMREMAP;

#define AMT_REMAPPABLE_PG		0x20

#define DRV_STANDARD_HANDLE L"\\\\.\\Buh"

#define CTL_ADDCURRENTTHREAD            CTL_CODE( FILE_DEVICE_UNKNOWN, 0x20, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define CTL_DELCURRENTTHREAD            CTL_CODE( FILE_DEVICE_UNKNOWN, 0x10, METHOD_BUFFERED, FILE_ANY_ACCESS )

#define CTL_ADDBYIDTHREAD               CTL_CODE( FILE_DEVICE_UNKNOWN, 0x22, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define CTL_DELBYIDTHREAD               CTL_CODE( FILE_DEVICE_UNKNOWN, 0x12, METHOD_BUFFERED, FILE_ANY_ACCESS )

#define CTL_ENABLEGLOBALHOOK            CTL_CODE( FILE_DEVICE_UNKNOWN, 0x40, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define CTL_DISABLEGLOBALHOOK           CTL_CODE( FILE_DEVICE_UNKNOWN, 0x80, METHOD_BUFFERED, FILE_ANY_ACCESS )

#define CTL_DEFINENEWRMPAGE             CTL_CODE( FILE_DEVICE_UNKNOWN, 0x200, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define CTL_DELETEOLDRMPAGE             CTL_CODE( FILE_DEVICE_UNKNOWN, 0x100, METHOD_BUFFERED, FILE_ANY_ACCESS )

#define CTL_PANIC                       CTL_CODE( FILE_DEVICE_UNKNOWN, 0x102, METHOD_BUFFERED, FILE_ANY_ACCESS )

int CreateRemappableThread(HANDLE proc, PVOID startpoint, PVOID dllbase);
int RemapContiguousReflectedPages(int realproc, int fakeproc, int RemappablePages, PCHAR dllbase);
NTSTATUS MeDisableCfgForThreadsInProcess(HANDLE realproc);