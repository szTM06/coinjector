#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include <psapi.h>
#include <excpt.h>
#include <TlHelp32.h>

#include "../inc/drvutils.h"

#define RELOC_FLAG(RInf) ((RInf >> 0x0C) == IMAGE_REL_BASED_DIR64)

// me when i forget SetThreadContext exists
char Shellcode[] = { 	0x48, 0x8d, 0x05, 0x00, 0x00, 0x00, 0x00, 	// lea rax, [rip]
						0x48, 0x83, 0xe8, 0x07, 					// sub rax, 0x7
						0x48, 0x91, 								// xchg rax, rcx
						0x4d, 0x31, 0xc0, 							// xor r8, r8
						0x41, 0x8d, 0x50, 0x01, 					// lea edx, [r8+0x1]
						0xff, 0xe0 									// jmp rax
					};

// TProcessId is the process ID you want the memory to be loaded in
// TVirtualPage is the virtual page you want the memory to be loaded in INSIDE TProcess
// S equivalents are for the source (source process and source page) ((where the memory comes from))
// RemapList is the list index you want this remap in
// RemapPage is unused lol make it 0

int mapdllatlocation(HANDLE dll, PVOID virtualbase, int filesize, PVOID *dllentrypoint) {
	PCHAR filesrc = (PCHAR)malloc(filesize);
	if (!filesrc)
		return 0;		// malloc gets returned to os anyway idgaf fuck you 

	puts("[+] loading dll");

	if (ReadFile(dll, filesrc, filesize, 0, 0) == 0)
		return 0;

	PIMAGE_DOS_HEADER doshdr = (PIMAGE_DOS_HEADER)filesrc;
	PIMAGE_NT_HEADERS nthdr = (PIMAGE_NT_HEADERS)(filesrc + doshdr->e_lfanew);
	PIMAGE_OPTIONAL_HEADER opthdr = &nthdr->OptionalHeader;
	PIMAGE_FILE_HEADER filehdr = &nthdr->FileHeader;

	PCHAR base = 0;
	__try {
		printf("[+] allocating memory at address %p\n", virtualbase);
		base = (PCHAR)VirtualAlloc(virtualbase, opthdr->SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		printf("[+] memory allocated at address %p\n", base);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		puts("[!] ummm...");
		TerminateProcess(GetCurrentProcess(), -1);
	}

	if (base == 0) {
		printf("[!] cant allocate memory at that address - error %d\n", GetLastError());
		return 0;
	}

	puts("[+] copying sections");
	PIMAGE_SECTION_HEADER chdr = IMAGE_FIRST_SECTION(nthdr);
	for (int i = 0; i < filehdr->NumberOfSections; i++, chdr++) {
		if (chdr->SizeOfRawData) {
			memcpy(base + chdr->VirtualAddress, filesrc + chdr->PointerToRawData, chdr->SizeOfRawData);
		}
	}

	int (*DllEntry)(PVOID, INT64, INT64) = (int (*)(PVOID, INT64, INT64))((INT64)virtualbase + opthdr->AddressOfEntryPoint);
	*dllentrypoint = DllEntry;

	PVOID LocDelta = (PVOID)((INT64)virtualbase - opthdr->ImageBase);

	if (LocDelta != 0) {
		if (!opthdr->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size) {
			puts("[!] image not relocatable");
			return 0;
		}
		PIMAGE_BASE_RELOCATION RelocationData = (PIMAGE_BASE_RELOCATION)((INT64)virtualbase + opthdr->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
		while (RelocationData->VirtualAddress) {
			puts("[+] resolving relocs");
			int AmtEntries = (RelocationData->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
			PWORD RelInfo = (PWORD)((INT64)RelocationData + 0x8);

			for (int i = 0; i != AmtEntries; i++, RelInfo++) {
				if (RELOC_FLAG(*RelInfo)) {
					PUINT_PTR patch = (PUINT_PTR)((INT64)virtualbase + RelocationData->VirtualAddress + ((*RelInfo) & 0xfff));
					*patch += (INT64)LocDelta;
				}
			}
			RelocationData = (PIMAGE_BASE_RELOCATION)((INT64)(RelocationData) + RelocationData->SizeOfBlock);
		}
	}

	if (opthdr->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size) {
		puts("[+] resolving imports");
		PIMAGE_IMPORT_DESCRIPTOR ImportDes = (PIMAGE_IMPORT_DESCRIPTOR)((INT64)virtualbase + opthdr->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
		while (ImportDes->Name) {
			PCHAR mName = (PCHAR)((INT64)virtualbase + ImportDes->Name);
			HINSTANCE hDll = LoadLibraryA(mName);

			if (hDll == 0) {
				puts("umm,m,mmmm");
				return 0;
			}

			PULONG_PTR ThunkRef = (PULONG_PTR)((INT64)virtualbase + ImportDes->OriginalFirstThunk);
			PULONG_PTR FuncRef = (PULONG_PTR)((INT64)virtualbase + ImportDes->FirstThunk);

			if (!ThunkRef) {
				ThunkRef = FuncRef;
			}

			for (; *ThunkRef; ThunkRef++, FuncRef++) {
				if (IMAGE_SNAP_BY_ORDINAL(*ThunkRef)) {
					*FuncRef = (ULONG_PTR)GetProcAddress(hDll, (PCHAR)(*ThunkRef & 0xffff));
				}
				else {
					PIMAGE_IMPORT_BY_NAME Import = (PIMAGE_IMPORT_BY_NAME)((INT64)virtualbase + *ThunkRef);
					*FuncRef = (ULONG_PTR)GetProcAddress(hDll, Import->Name);
				}
			}
			ImportDes++;
		}
	}

	if (opthdr->DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].Size) {
		puts("[i] tls callbacks found!!!");
		puts("[i] tls callbacks work in the address space of the INJECTOR. NOT the process you are injecting into");
		puts("[i] but really what are you using tls callbacks for tho (no my anti debugging trick from the 2000s :CCCC)");
		PIMAGE_TLS_DIRECTORY tlsshit = (PIMAGE_TLS_DIRECTORY)((INT64)virtualbase + opthdr->DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].VirtualAddress);
		PIMAGE_TLS_CALLBACK* callbacks = (PIMAGE_TLS_CALLBACK*)tlsshit->AddressOfCallBacks;

		for (; callbacks && *callbacks; callbacks++) {
			(*callbacks)(virtualbase, DLL_PROCESS_ATTACH, 0);
		}
	}

	memcpy(virtualbase, Shellcode, sizeof(Shellcode));
	puts("[+] dll is MAPPED :3");
	return opthdr->SizeOfImage;
}

// UTILS

BOOL WINAPI CtrlHandler(DWORD dwCtrlType) {
	puts("[i] closing");
	HANDLE DrvHandle = CreateFileW(DRV_STANDARD_HANDLE, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

	if (DrvHandle == INVALID_HANDLE_VALUE) {
		puts("[!] error connecting to driver");
		return FALSE;
	}

	DeviceIoControl(DrvHandle, CTL_PANIC, 0, 0, 0, 0, 0, (LPOVERLAPPED)NULL);
	// panic is a pretty nice function with a bad name
	// really just clears everything
	return FALSE;
}

DWORD FindProcessId(PWCHAR processName)
{
	PROCESSENTRY32 processInfo;
	processInfo.dwSize = sizeof(processInfo);

	HANDLE processSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	if (processSnapshot == INVALID_HANDLE_VALUE)
		return 0;

	Process32First(processSnapshot, &processInfo);
	if (!(wcscmp(processName, processInfo.szExeFile))) {
		CloseHandle(processSnapshot);
		return processInfo.th32ProcessID;
	}

	while (Process32Next(processSnapshot, &processInfo))
	{
		if (!(wcscmp(processName, processInfo.szExeFile))) {
			CloseHandle(processSnapshot);
			return processInfo.th32ProcessID;
		}
	}

	CloseHandle(processSnapshot);
	return 0;
}

int main(int argc, char** argv) {

	if (argc != 4) {
		printf("usage - %s <exe to inject> <dll_to_meme> <dll_to_inject>\n", argv[0]);
		return -1;
	}

	if (SetConsoleCtrlHandler(CtrlHandler, 1) == 0) {
		puts("[!] error setting exit handler");
		return -1;
	}

	LoadLibraryA("user32.dll");
	WCHAR exename[MAX_PATH] = { 0 };
	mbstowcs(exename, argv[1], 120);
	int targetpid = FindProcessId(exename);
	PCHAR injectdll = argv[3];
	PCHAR replacedll = argv[2];
	printf("[+] injecting into %s - PID %d\n", argv[1], targetpid);

	HANDLE proc = OpenProcess(PROCESS_ALL_ACCESS, 0, targetpid);	// obviously maybe change this to less detectable method
	if (proc == 0) {
		puts("[!] nt bro something broke");
		return -1;
	}

	DWORD size = 0;
	EnumProcessModules(proc, 0, 0, &size);
	HMODULE* procmodules = (HMODULE*)malloc(size);
	if (procmodules == 0) {
		return -1;
	}

	EnumProcessModules(proc, procmodules, size, &size);
	char buf[50] = { 0 };
	HMODULE dllbase = 0;
	for (int i = 0; i <= size / sizeof(HMODULE); i++) {
		GetModuleBaseNameA(proc, procmodules[i], buf, sizeof(buf));
		if (_strcmpi(buf, replacedll) == 0) {
			dllbase = procmodules[i];
			printf("[+] found %p\n", dllbase);
			break;
		}
	}

	if (dllbase == 0) {
		puts("[!] cannot find dll in target process");
		return -1;
	}

	OFSTRUCT useless = { 0 };
	HFILE filedll = (HFILE)OpenFile(injectdll, &useless, 0);
	PVOID dllentrypoint;
	int SizeOfImage = mapdllatlocation((HANDLE)filedll, dllbase, GetFileSize(filedll, 0), &dllentrypoint);

	if (SizeOfImage == 0) {
		puts("[!] error loading dll");
		return 1;
	}

	int RemappablePages = SizeOfImage / 0x1000;
	if (RemapContiguousReflectedPages(targetpid, GetCurrentProcessId(), RemappablePages, (PCHAR)dllbase) != 0) {
		return 1;
	}

	SetProcessAffinityMask(proc, 0x2);
	MeDisableCfgForThreadsInProcess((HANDLE)targetpid);
	CreateRemappableThread(proc, dllentrypoint, dllbase);
	return 0;
}