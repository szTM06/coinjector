#include <ntifs.h>
#include <ntddk.h>

int (*KeIsKernelCetEnabled)();
extern int HalpCallbackEnabled;

NTSTATUS NtiInstallHookContextSwitch(PVOID callback);
NTSTATUS NtiRemoveHookContextSwitch();
int NtiActivateHookContextSwitch();
int NtiDeactivateHookContextSwitch();

int NtipCSCallback();