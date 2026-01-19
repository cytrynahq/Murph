#include <ntifs.h>
#include "../Core/kernel_interface.hxx"
#include "../Utilities/logger.hxx"
#include "process_manager.hxx"

PEPROCESS ProcessManager::FindProcessByName(const char* processName)
{
	if (!processName)
		return nullptr;

	PEPROCESS foundProcess = nullptr;

	for (ULONG pid = 4; pid < 16384; pid += 4)
	{
		PEPROCESS process = nullptr;
		NTSTATUS status = PsLookupProcessByProcessId(reinterpret_cast<HANDLE>(pid), &process);

		if (NT_SUCCESS(status) && process)
		{
			HANDLE processHandle = nullptr;
			OBJECT_ATTRIBUTES objAttr;
			CLIENT_ID clientId;
			
			clientId.UniqueProcess = reinterpret_cast<HANDLE>(pid);
			clientId.UniqueThread = nullptr;
			
			InitializeObjectAttributes(&objAttr, nullptr, OBJ_KERNEL_HANDLE, nullptr, nullptr);
			
			status = ZwOpenProcess(&processHandle, PROCESS_QUERY_INFORMATION, &objAttr, &clientId);
			
			if (NT_SUCCESS(status) && processHandle)
			{
				UNICODE_STRING* imageNameBuffer = static_cast<UNICODE_STRING*>(
					ExAllocatePoolWithTag(PagedPool, 512, 'PrcM')
				);
				
				if (imageNameBuffer)
				{
					ULONG returnLength = 0;
					
					status = ZwQueryInformationProcess(
						processHandle,
						ProcessImageFileName,
						imageNameBuffer,
						512,
						&returnLength
					);
					
					if (NT_SUCCESS(status))
					{
						// Extract filename from path (last component after '\')
						wchar_t* filename = imageNameBuffer->Buffer;
						wchar_t* lastSlash = nullptr;
						
						if (filename)
						{
							for (wchar_t* p = filename; *p; p++)
							{
								if (*p == L'\\')
									lastSlash = p;
							}
							
							if (lastSlash)
								filename = lastSlash + 1;
							
							wchar_t targetName[256] = { 0 };
							UNICODE_STRING targetNameString;
							RtlInitAnsiString(reinterpret_cast<PANSI_STRING>(&targetNameString), processName);
							
							UNICODE_STRING unicodeTargetName;
							RtlInitEmptyUnicodeString(&unicodeTargetName, targetName, sizeof(targetName));
							
							RtlAnsiStringToUnicodeString(&unicodeTargetName, reinterpret_cast<PANSI_STRING>(&targetNameString), FALSE);
							
							if (RtlEqualUnicodeString(imageNameBuffer, &unicodeTargetName, TRUE))
							{
								foundProcess = process;
								ExFreePool(imageNameBuffer);
								ZwClose(processHandle);
								return foundProcess;
							}
						}
					}
					
					ExFreePool(imageNameBuffer);
				}
				
				ZwClose(processHandle);
			}
			
			ObDereferenceObject(process);
		}
	}

	return nullptr;
}

NTSTATUS ProcessManager::TerminateProcess(PEPROCESS processHandle)
{
	if (!processHandle)
		return STATUS_INVALID_PARAMETER;

	HANDLE processId = PsGetProcessId(processHandle);
	if (!processId)
		return STATUS_UNSUCCESSFUL;

	NTSTATUS status = ZwTerminateProcess(processId, STATUS_SUCCESS);

	Logger::Output("Process termination initiated: %p | Status: 0x%X\n", processId, status);

	return status;
}

NTSTATUS ProcessManager::RestartExplorer()
{
	Logger::Output("Initiating explorer.exe restart sequence\n");

	PEPROCESS explorerProcess = FindProcessByName("explorer.exe");

	if (!explorerProcess)
	{
		Logger::Output("explorer.exe process not found in system\n");
		return STATUS_NOT_FOUND;
	}

	NTSTATUS terminateStatus = TerminateProcess(explorerProcess);
	ObDereferenceObject(explorerProcess);

	if (!NT_SUCCESS(terminateStatus))
	{
		Logger::Output("Failed to terminate explorer.exe: 0x%X\n", terminateStatus);
		return terminateStatus;
	}

	Logger::Output("explorer.exe successfully terminated\n");

	return STATUS_SUCCESS;
}

void ProcessManager::DelayedWait(ULONG delayMs)
{
	LARGE_INTEGER waitInterval;
	waitInterval.QuadPart = -10000LL * delayMs;

	KeDelayExecutionThread(KernelMode, FALSE, &waitInterval);
}


