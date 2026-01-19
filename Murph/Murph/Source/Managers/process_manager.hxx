#pragma once
#include <ntifs.h>

namespace ProcessManager
{
	PEPROCESS FindProcessByName(const char* processName);
	NTSTATUS TerminateProcess(PEPROCESS processHandle);
	NTSTATUS RestartExplorer();
	void DelayedWait(ULONG delayMs);
}
