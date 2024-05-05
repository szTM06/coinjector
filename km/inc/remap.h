#pragma once

// single process version //

#include <ntifs.h>
#include <ntddk.h>
#include <intrin.h>

#include "ia32.h"
#include "mm.h"

#define MM_SYSTEM_PROCESS		(HANDLE)0x4
#define AMT_REMAPPABLE_PG		0x20
#define AMT_LIST_REMAPPED		0x2
#define MAX_SAFE_REMAPPABLE		0x20

extern UNICODE_STRING DeviceName;
extern UNICODE_STRING DosSymlink;

typedef struct _REMAP {
	PPTE		TableEntry;
	PTE			RealEntry;
	PTE			FakeEntry;
	PEPROCESS	Process;
	INT64		RealPFN;
	INT64		FakePFN;
	PVOID		VirtualBase;
	HANDLE		ExclusiveOwner; // can this page only be remapped by this thread id. as opposed to any thread on the remaplist
	int			Remapped;
	INT64		DirectoryBase;
	PMDL		Mdl;
} REMAP, * PREMAP;

typedef REMAP REMAPPABLE_PAGE, * PREMAPPABLE_PAGE;

// TProcessId is the process ID you want the memory to be loaded in
// TVirtualPage is the virtual page you want the memory to be loaded in INSIDE TProcess
// S equivalents are for the source (source process and source page) ((where the memory comes from))
// RemapList is the list index you want this remap in
// RemapPage is the index into the list you want this remap (this matters less just ensure you are writing to a free entry)
typedef struct _MMREMAP {
	HANDLE		TProcessId;
	PVOID		TVirtualPage;
	HANDLE		SProcessId;
	PVOID		SVirtualPage;
	int			RemapList;
	int			RemapPage;
} MMREMAP, * PMMREMAP;

#define THREAD_UPDATE_CALLER	0x0
#define	THREAD_UPDATE_HANDLE	0x1

typedef struct _THREADACT {
	int			flags;
	HANDLE		thread;
} THREADACT, * PTHREADACT;

void					MeRemapDispatch();
INT64					MeGetPFNVirtual(PVOID virt);
PPTE					MeGetPTEVirtual(PVOID virt);

NTSTATUS				MeSetupRemappablePage(PVOID realaddr, HANDLE realproc, PVOID fakeaddr, HANDLE fakeproc, PREMAPPABLE_PAGE map);
void					MeDeleteRemappablePage(PREMAPPABLE_PAGE map);

NTSTATUS				MeRemapRemappablePages(PREMAPPABLE_PAGE pageArray, size_t pageArraySize);
NTSTATUS				MeRestoreRemappablePages(PREMAPPABLE_PAGE pageArray);

NTSTATUS				MeRemapMemory(PREMAPPABLE_PAGE map);
NTSTATUS				MeRestoreMemory(PREMAPPABLE_PAGE map);

int						MeIsValidMap(PREMAP map);

extern char				ThreadList[0x10000];	// i dont give a fuck i have 32 gigabytes

extern REMAPPABLE_PAGE	RemapList[AMT_REMAPPABLE_PG];		// default values set to 2 lists that can each remap 4 pages
extern INT64			CRemappedPages;
extern UINT64			TDirectoryBase;