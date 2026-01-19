#pragma once
#include <ntifs.h>

namespace ThreadManager
{
	NTSTATUS InitializeWorkerSystem();
	NTSTATUS ScheduleExplorerRestart(ULONG delaySeconds);
	NTSTATUS ScheduleWmiRestart(ULONG delaySeconds);
	void CleanupWorkerSystem();
}
