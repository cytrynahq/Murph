#include <ntifs.h>
#include <ntstrsafe.h>
#include "../Utilities/logger.hxx"
#include "service_manager.hxx"
#include "process_manager.hxx"

NTSTATUS ServiceManager::RestartWmiService()
{
	Logger::Output("Initiating WMI service recovery sequence\n");

	PEPROCESS wmiProcess = ProcessManager::FindProcessByName("wmiprvse.exe");

	if (!wmiProcess)
	{
		Logger::Output("WMI provider process not currently running\n");
		return STATUS_SUCCESS;
	}

	NTSTATUS status = ProcessManager::TerminateProcess(wmiProcess);
	ObDereferenceObject(wmiProcess);

	if (!NT_SUCCESS(status))
	{
		Logger::Output("Failed to terminate WMI provider: 0x%X\n", status);
		return status;
	}

	Logger::Output("WMI service provider successfully terminated\n");
	Logger::Output("WMI will automatically restart on next access\n");

	return STATUS_SUCCESS;
}

NTSTATUS ServiceManager::RestartServiceByName(const wchar_t* serviceName)
{
	if (!serviceName)
		return STATUS_INVALID_PARAMETER;

	Logger::Status("Service restart requested (kernel-mode): %S\n", serviceName);

	if (wcscmp(serviceName, L"WmiPrvSe") == 0 || wcscmp(serviceName, L"Winmgmt") == 0)
	{
		return RestartWmiService();
	}

	Logger::Output("Service restart not directly supported in kernel-mode for: %S\n", serviceName);
	return STATUS_NOT_SUPPORTED;
}

NTSTATUS ServiceManager::StopServiceByName(const wchar_t* serviceName)
{
	if (!serviceName)
		return STATUS_INVALID_PARAMETER;

	Logger::Status("Service stop requested (kernel-mode): %S\n", serviceName);

	if (wcscmp(serviceName, L"WmiPrvSe") == 0)
	{
		PEPROCESS wmiProcess = ProcessManager::FindProcessByName("wmiprvse.exe");
		if (wmiProcess)
		{
			NTSTATUS status = ProcessManager::TerminateProcess(wmiProcess);
			ObDereferenceObject(wmiProcess);
			return status;
		}
	}

	Logger::Output("Service stop not directly supported for: %S\n", serviceName);
	return STATUS_NOT_SUPPORTED;
}

NTSTATUS ServiceManager::StartServiceByName(const wchar_t* serviceName)
{
	if (!serviceName)
		return STATUS_INVALID_PARAMETER;

	Logger::Status("Service start requested (kernel-mode): %S\n", serviceName);

	Logger::Output("Service will automatically restart via Windows service manager: %S\n", serviceName);

	return STATUS_SUCCESS;
}

