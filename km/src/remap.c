//#define MULTILIST
#include "../inc/remap.h"
#include "../inc/nthooks.h"

// i am not a good developer this is not good code

UNICODE_STRING		DeviceName = RTL_CONSTANT_STRING(L"\\Device\\Buh");
UNICODE_STRING		DosSymlink = RTL_CONSTANT_STRING(L"\\DosDevices\\Buh");

// get page frame number of a virtual address
INT64 MeGetPFNVirtual(PVOID virt) {
	if (virt == 0)
		return 0;
	CR3 cr3 = { 0 };
	cr3.AsUInt = __readcr3();
	PPTE pptevirt = PTEForVirtualAddress(cr3.AddressOfPageDirectory * PAGE_SIZE, (INT64)virt);
	if (pptevirt == 0) {
		return 0;
	}
	return pptevirt->pfn;
}

// get pointer to page table entry of a virtual address
PPTE MeGetPTEVirtual(PVOID virt) {
	if (virt == 0)
		return 0;
	CR3 cr3 = { 0 };
	cr3.AsUInt = __readcr3();
	return PTEForVirtualAddress(cr3.AddressOfPageDirectory * PAGE_SIZE, (INT64)virt);
}

// ensure readaddr is in context of running thread
// otherwise probably break lol
// other functions dont have to actually worry about this once we get the pfn tbh
// realproc and fakeproc are used to find pfn for that address space
// however for obvious reasons realproc is the only one that really matters during the actual pfn swap
NTSTATUS MeSetupRemappablePage(PVOID realaddr, HANDLE realproc, PVOID fakeaddr, HANDLE fakeproc, PREMAPPABLE_PAGE map) {
	KAPC_STATE RealAPC = { 0 };
	KAPC_STATE FakeAPC = { 0 };
	PEPROCESS fakeEProcess = 0;

	if (realproc == 0 || fakeproc == 0) {
		return STATUS_UNSUCCESSFUL;
	}

	memset(map, 0, sizeof(REMAPPABLE_PAGE));
	map->VirtualBase = realaddr;

	// get RealPFN + PTE backup
	PsLookupProcessByProcessId(realproc, &map->Process);
	KeStackAttachProcess(map->Process, &RealAPC);
	map->Mdl = IoAllocateMdl(map->VirtualBase, PAGE_SIZE, 0, 0, 0);
	if (map->Mdl == 0) {
		// huh
		KeUnstackDetachProcess(&RealAPC);
		return STATUS_UNSUCCESSFUL;
	}

	MmProbeAndLockPages(map->Mdl, KernelMode, IoReadAccess);
	map->RealPFN = MeGetPFNVirtual(realaddr);
	map->TableEntry = MeGetPTEVirtual(realaddr);
	if (map->RealPFN == 0 || map->TableEntry == 0) {
		KeUnstackDetachProcess(&RealAPC);
		MmUnlockPages(map->Mdl);
		IoFreeMdl(map->Mdl);
		return STATUS_ACCESS_VIOLATION;
	}

	CR3 cr3 = { 0 };
	cr3.AsUInt = __readcr3();
	if (TDirectoryBase == 0) {
		TDirectoryBase = cr3.AddressOfPageDirectory;
	}

	map->RealEntry.value = map->TableEntry->value;
	KeUnstackDetachProcess(&RealAPC);

	// now get fake pfn
	PsLookupProcessByProcessId(fakeproc, &fakeEProcess);
	KeStackAttachProcess(fakeEProcess, &FakeAPC);
	map->FakePFN = MeGetPFNVirtual(fakeaddr);
	if (map->FakePFN == 0) {
		KeUnstackDetachProcess(&FakeAPC);
		return STATUS_UNSUCCESSFUL;
	}

	KeUnstackDetachProcess(&FakeAPC);
	return STATUS_SUCCESS;
}

void MeDeleteRemappablePage(PREMAPPABLE_PAGE map) {
	if (map->VirtualBase == 0)
		return;
	KAPC_STATE RealAPC = { 0 };
	KeStackAttachProcess(map->Process, &RealAPC);
	MmUnlockPages(map->Mdl);
	IoFreeMdl(map->Mdl);
	memset(map, 0, sizeof(REMAPPABLE_PAGE));
	KeUnstackDetachProcess(&RealAPC);
	return;
}

// is remap valid?
// ie pte pointer is valid and there is a valid page frame number
int MeIsValidMap(PREMAP map) {
	if (map->TableEntry == 0) {
		return 0;
	}

	if (map->RealPFN == 0) {
		return 0;
	}
	return 1;
}

// i really do not like this code it is very messy
// especially because this is called every context switch :skull:
void MeRemapDispatch() {
	CR3 cr3 = { 0 };
	cr3.AsUInt = __readcr3();

	_disable();
	ULONG oldCR0 = (ULONG)__readcr0();
	__writecr0(oldCR0 & (~(1 << 16))); 			// disable cr0.wp

	if (cr3.AddressOfPageDirectory != TDirectoryBase) { // if our thread is not running within the address space of the remap drag it in
		CR3 cr3_swap = cr3;
		cr3_swap.AddressOfPageDirectory = TDirectoryBase;	// i had a fun triple fault here which was awesome fun
															// i actually had to setup a vm ):
															// turns out for some reason TDirectoryBase was sometimes just null
															// cr3=0x0000000000000000 is definitely not ideal...
		__writecr3(cr3_swap.AsUInt);
	}
 												// we need to unmap pages and/or remap pages (no we dont because this version doesnt support multiple remap lists, we just restore)
	if (CRemappedPages != 0) {
		MeRestoreRemappablePages(RemapList);
	}
	else {										// we remap pages
		MeRemapRemappablePages(RemapList, AMT_REMAPPABLE_PG);
	}

	__writecr0(oldCR0);
	__writecr3(cr3.AsUInt);
	_enable();
	return;
}

// when this is called PLEASE ensure map->VirtualBase is actually in the context of the running thread...
NTSTATUS MeRemapMemory(PREMAPPABLE_PAGE map) {
	if (map->VirtualBase == 0) {				// is this even valid?
		return STATUS_UNSUCCESSFUL;
	}

	if (map->Remapped != 1 && map->TableEntry != 0 && map->RealPFN != 0) {
		map->TableEntry->pfn = map->FakePFN;
		map->TableEntry->writable = 1;			// enable write on new page
		map->TableEntry->execution_disabled = 0;
		map->Remapped = 1;
		__invlpg(map->VirtualBase);	// probably unnecessary but idc
		CRemappedPages++;
	}
	return STATUS_SUCCESS;
}

// ALL CALLS TO REMAPMEMORY SHOULD HAVE CORRESPONDING CALL TO RESTOREMEMORY
NTSTATUS MeRestoreMemory(PREMAPPABLE_PAGE map) {
	// PFN LIST CORRUPT
	// :hmmm: emoji
	if (map->VirtualBase == 0) {
		return STATUS_UNSUCCESSFUL;
	}

	if (map->Remapped != 0) {
		map->TableEntry->value = map->RealEntry.value;
		map->Remapped = 0;
		__invlpg(map->VirtualBase);	// probably unnecessary but idc
		CRemappedPages--;
	}
	return STATUS_SUCCESS;
}

// Remap all pages in pageArray
// Unless page has an exclusive thread owner in which case it MAY not be remapped
NTSTATUS MeRemapRemappablePages(PREMAPPABLE_PAGE pageArray, size_t pageArraySize) {
	for (int i = 0; i < pageArraySize; i++) {
		if (pageArray[i].ExclusiveOwner != 0 && pageArray[i].ExclusiveOwner != PsGetCurrentThreadId()) {
			continue;
		}
		MeRemapMemory(&pageArray[i]);
	}
	return STATUS_SUCCESS;
}

// Restore all pages in pageArray
// This is a lot more intensive than remap all pages because i cant do thread indexing trickery ):
// but thankfully i can continue pretty quickly if the map remap status is 0
NTSTATUS MeRestoreRemappablePages(PREMAPPABLE_PAGE pageArray) {
	for (int i = 0; i < AMT_REMAPPABLE_PG; i++) {
		MeRestoreMemory(&pageArray[i]);
		if (CRemappedPages == 0)
			return STATUS_SUCCESS;
	}
	return STATUS_SUCCESS;
}

char 			ThreadList[0x10000] = { 0 };

REMAPPABLE_PAGE RemapList[AMT_REMAPPABLE_PG] = { 0 };		// default values set to 2 lists that can each remap 4 pages
INT64 			CRemappedPages = 0;
UINT64 			TDirectoryBase = 0;								// for single list single processes this is the CR3 with PTEs for the original page