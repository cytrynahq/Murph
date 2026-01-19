#pragma once
#include <ntifs.h>

namespace PeriodicSpoofer
{
	NTSTATUS InitializePeriodicSpoofer(ULONG intervalSeconds);
	NTSTATUS StartPeriodicSpoofer();
	NTSTATUS StopPeriodicSpoofer();
	NTSTATUS ExecuteSpooferCycle();
	ULONG GetSpooferCycleCount();
}
