#include "../inc/mm.h"

// edited version of TranslateLinearAddress (https://github.com/waryas/UMPMLib/blob/master/MemoryOperationSample/PMemHelper.h)
// only supports 4kb page currently
// returns virtual address of PTE backing virtual address
PPTE PTEForVirtualAddress(uint64_t dtb, uint64_t virtaddr) {
	SHORT PML4 = (SHORT)((virtaddr >> 39) & 0x1FF);         //<! PML4 Entry Index
	SHORT DirectoryPtr = (SHORT)((virtaddr >> 30) & 0x1FF); //<! Page-Directory-Pointer Table Index
	SHORT Directory = (SHORT)((virtaddr >> 21) & 0x1FF);    //<! Page Directory Table Index
	SHORT Table = (SHORT)((virtaddr >> 12) & 0x1FF);        //<! Page Table Index

	SIZE_T BytesCopied = 0;

	uint64_t PML4E = 0;
	ReadPhysicalAddress(dtb + (uint64_t)PML4 * sizeof(uint64_t), (CHAR*)&PML4E, sizeof(PML4E), &BytesCopied);

	if (PML4E == 0)
		return 0;

	uint64_t PDPTE = 0;
	ReadPhysicalAddress((PML4E & 0xFFFF1FFFFFF000) + (uint64_t)DirectoryPtr * sizeof(uint64_t), (CHAR*)&PDPTE, sizeof(PDPTE), &BytesCopied);

	if (PDPTE == 0)
		return 0;

	if ((PDPTE & (1 << 7)) != 0)
	{
		return (PPTE)(PDPTE & 0xFFFFFC0000000) + (virtaddr & 0x3FFFFFFF);
	}

	uint64_t PDE = 0;
	ReadPhysicalAddress((PDPTE & 0xFFFFFFFFFF000) + (uint64_t)Directory * sizeof(uint64_t), (CHAR*)&PDE, sizeof(PDE), &BytesCopied);

	if (PDE == 0)
		return 0;

	if ((PDE & (1 << 7)) != 0)
	{
		return (PPTE)(PDE & 0xFFFFFFFE00000) + (virtaddr & 0x1FFFFF);
	}

	uint64_t PTE = 0;
	ReadPhysicalAddress((PDE & 0xFFFFFFFFFF000) + (uint64_t)Table * sizeof(uint64_t), (CHAR*)&PTE, sizeof(PTE), &BytesCopied);

	if (PTE == 0)
		return 0;

	//kprintf("%p", (PDE & 0xFFFFFFFFFF000) + (uint64_t)Table * sizeof(uint64_t));

	PHYSICAL_ADDRESS target = { 0 };
	target.QuadPart = (PDE & 0xFFFFFFFFFF000) + (uint64_t)Table * sizeof(uint64_t);
	PVOID targetvirt = MmGetVirtualForPhysical(target);
	return (PPTE)targetvirt;
}

NTSTATUS ReadPhysicalAddress(uint64_t TargetAddress, PVOID lpBuffer, SIZE_T Size, SIZE_T* BytesRead)
{
	MM_COPY_ADDRESS AddrToRead = { 0 };
	AddrToRead.PhysicalAddress.QuadPart = TargetAddress;
	return MmCopyMemory(lpBuffer, AddrToRead, Size, MM_COPY_MEMORY_PHYSICAL, BytesRead);
}