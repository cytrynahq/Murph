#include <ntifs.h>
#include "../Utilities/logger.hxx"
#include "process_manager.hxx"
#include "service_manager.hxx"
#include "thread_manager.hxx"

typedef struct _DELAYED_OPERATION
{
	enum OperationType
	{
		RestartExplorerOp = 1,
		RestartWmiOp = 2
	} OperationType;

	ULONG DelayMs;
} DELAYED_OPERATION, *PDELAYED_OPERATION;

VOID DelayedOperationWorker(PVOID parameter)
{
	if (!parameter)
	{
		Logger::Output("Worker thread received null parameter\n");
		PsTerminateSystemThread(STATUS_INVALID_PARAMETER);
	}

	PDELAYED_OPERATION operation = static_cast<PDELAYED_OPERATION>(parameter);

	Logger::Output("Delayed operation worker started | Delay: %lu ms | Type: %lu\n", 
		operation->DelayMs, operation->OperationType);

	ProcessManager::DelayedWait(operation->DelayMs);

	switch (operation->OperationType)
	{
		case DELAYED_OPERATION::RestartExplorerOp:
			Logger::Output("Executing delayed explorer.exe restart\n");
			ProcessManager::RestartExplorer();
			break;

		case DELAYED_OPERATION::RestartWmiOp:
			Logger::Output("Executing delayed WMI service restart\n");
			ServiceManager::RestartWmiService();
			break;

		default:
			Logger::Output("Unknown delayed operation type: %lu\n", operation->OperationType);
			break;
	}

	// Cleanup operation structure
	ExFreePool(operation);

	Logger::Output("Delayed operation worker terminating\n");
	PsTerminateSystemThread(STATUS_SUCCESS);
}

NTSTATUS ThreadManager::InitializeWorkerSystem()
{
	Logger::Output("Worker thread system initialized\n");
	return STATUS_SUCCESS;
}

NTSTATUS ThreadManager::ScheduleExplorerRestart(ULONG delaySeconds)
{
	PDELAYED_OPERATION operation = static_cast<PDELAYED_OPERATION>(
		ExAllocatePoolWithTag(NonPagedPool, sizeof(DELAYED_OPERATION), 'WrkT')
	);

	if (!operation)
	{
		Logger::Output("Failed to allocate memory for explorer restart operation\n");
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	operation->OperationType = DELAYED_OPERATION::RestartExplorerOp;
	operation->DelayMs = delaySeconds * 1000;

	Logger::Status("Scheduling explorer.exe restart in %lu seconds\n", delaySeconds);

	HANDLE threadHandle = nullptr;
	NTSTATUS status = PsCreateSystemThread(&threadHandle, THREAD_ALL_ACCESS, nullptr, nullptr, nullptr, DelayedOperationWorker, operation);

	if (!NT_SUCCESS(status))
	{
		Logger::Output("Failed to create explorer restart worker thread: 0x%X\n", status);
		ExFreePool(operation);
		return status;
	}

	ZwClose(threadHandle);

	Logger::Output("Explorer restart worker thread created successfully\n");
	return STATUS_SUCCESS;
}

NTSTATUS ThreadManager::ScheduleWmiRestart(ULONG delaySeconds)
{
	PDELAYED_OPERATION operation = static_cast<PDELAYED_OPERATION>(
		ExAllocatePoolWithTag(NonPagedPool, sizeof(DELAYED_OPERATION), 'WrkT')
	);

	if (!operation)
	{
		Logger::Output("Failed to allocate memory for WMI restart operation\n");
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	operation->OperationType = DELAYED_OPERATION::RestartWmiOp;
	operation->DelayMs = delaySeconds * 1000;

	Logger::Status("Scheduling WMI service restart in %lu seconds\n", delaySeconds);

	HANDLE threadHandle = nullptr;
	NTSTATUS status = PsCreateSystemThread(&threadHandle, THREAD_ALL_ACCESS, nullptr, nullptr, nullptr, DelayedOperationWorker, operation);

	if (!NT_SUCCESS(status))
	{
		Logger::Output("Failed to create WMI restart worker thread: 0x%X\n", status);
		ExFreePool(operation);
		return status;
	}

	ZwClose(threadHandle);

	Logger::Output("WMI restart worker thread created successfully\n");
	return STATUS_SUCCESS;
}

void ThreadManager::CleanupWorkerSystem()
{
	Logger::Output("Worker thread system cleaned up\n");
}
