#include "../inc/nthooks.h"
#include "../inc/remap.h"

int HalpCallbackEnabled = 0;
int (*KeIsKernelCetEnabled)() = 0;			// this is the original dispatch function on my machine idk if its different for others i only have one pc (and i do not care)

NTSTATUS NtiInstallHookContextSwitch(PVOID callback) {
	if (HalpCallbackEnabled == 1) {
		return STATUS_UNSUCCESSFUL;			// dont change hook while it is active. probably fine but ive had enough bugchecks
	}

	UNICODE_STRING HalpDispatchStr = RTL_CONSTANT_STRING(L"HalPrivateDispatchTable");
	PVOID* HalpDispatch = MmGetSystemRoutineAddress(&HalpDispatchStr);
	if (HalpDispatch == 0)
		return STATUS_UNSUCCESSFUL;

	_disable();
	ULONG oldCR0 = (ULONG)__readcr0();
	__writecr0(oldCR0 & (~(1 << 16)));		// disable cr0.wp

	KeIsKernelCetEnabled = (int (*)())_InterlockedExchange64((PLONG64)&HalpDispatch[0x400 / sizeof(PVOID)], (LONG64)callback);

	__writecr0(oldCR0);
	_enable();
	return STATUS_SUCCESS;
}

NTSTATUS NtiRemoveHookContextSwitch() {
	if (HalpCallbackEnabled == 1) {
		return STATUS_UNSUCCESSFUL;			// dont unhook while running
	}

	UNICODE_STRING HalpDispatchStr = RTL_CONSTANT_STRING(L"HalPrivateDispatchTable");
	PVOID* HalpDispatch = MmGetSystemRoutineAddress(&HalpDispatchStr);
	if (!HalpDispatch)
		return STATUS_UNSUCCESSFUL;

	_disable();
	ULONG oldCR0 = (ULONG)__readcr0();
	__writecr0(oldCR0 & (~(1 << 16)));		// disable cr0.wp

	_InterlockedExchange64((PLONG64)&HalpDispatch[0x400 / sizeof(PVOID)], (LONG64)KeIsKernelCetEnabled);

	__writecr0(oldCR0);
	_enable();
	return STATUS_SUCCESS;
}

int NtiActivateHookContextSwitch() {
	UNICODE_STRING KeSetLastBranchRecordInUse = RTL_CONSTANT_STRING(L"KeSetLastBranchRecordInUse");
	int KiCpuFlagsOffset = 0;
	CHAR* KeSetLastBranchRecordInUseFn = MmGetSystemRoutineAddress(&KeSetLastBranchRecordInUse);
	if (KeSetLastBranchRecordInUseFn == 0)
		return 0;

	KeSetLastBranchRecordInUseFn += 8;
	KiCpuFlagsOffset = *(int*)KeSetLastBranchRecordInUseFn;
	KeSetLastBranchRecordInUseFn += KiCpuFlagsOffset;
	int* KiCpuTracingFlags = (int*)(KeSetLastBranchRecordInUseFn + 5);

	*KiCpuTracingFlags = 2;
	HalpCallbackEnabled = 1;
	return 1;
}

int NtiDeactivateHookContextSwitch() {
	UNICODE_STRING KeSetLastBranchRecordInUse = RTL_CONSTANT_STRING(L"KeSetLastBranchRecordInUse");
	int KiCpuFlagsOffset = 0;
	CHAR* KeSetLastBranchRecordInUseFn = MmGetSystemRoutineAddress(&KeSetLastBranchRecordInUse);
	if (KeSetLastBranchRecordInUseFn == 0)
		return 0;

	KeSetLastBranchRecordInUseFn += 8;
	KiCpuFlagsOffset = *(int*)KeSetLastBranchRecordInUseFn;
	KeSetLastBranchRecordInUseFn += KiCpuFlagsOffset;
	int* KiCpuTracingFlags = (int*)(KeSetLastBranchRecordInUseFn + 5);

	*KiCpuTracingFlags = 0;
	HalpCallbackEnabled = 0;
	return 1;
}

int NtipCSCallback() {
	if (PsGetCurrentThreadId() != 0) { // idk what these id 0 threads are but i dont want them 
		HANDLE hThr = PsGetCurrentThreadId();
		char remap = ThreadList[(INT64)hThr];
		CR3 cr3 = { 0 };
		cr3.AsUInt = __readcr3();

		// if current thread is not in process and doesnt remap return
		if (cr3.AddressOfPageDirectory != TDirectoryBase && remap == 0) {
			return KeIsKernelCetEnabled();
		}

		// if nothing is remapped and we arent mapping anything return
		if ((CRemappedPages == 0 && remap == 0) || (CRemappedPages != 0 && remap != 0)) {
			return KeIsKernelCetEnabled();
		}

		MeRemapDispatch();
	}
	return KeIsKernelCetEnabled();
}