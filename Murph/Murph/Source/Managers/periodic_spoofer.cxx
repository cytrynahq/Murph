#include <ntifs.h>
#include "../Utilities/logger.hxx"
#include "storage_manager.hxx"
#include "firmware_config.hxx"
#include "gpu_manager.hxx"
#include "cpu_manager.hxx"
#include "periodic_spoofer.hxx"

static KTIMER g_SpoofTimer = { 0 };
static KDPC g_SpoofDpc = { 0 };
static HANDLE g_SpoofWorkerThread = nullptr;
static volatile BOOLEAN g_SpoofActive = FALSE;
static ULONG g_CycleCount = 0;
static ULONG g_IntervalSeconds = 180;
static volatile ULONG g_SpooferIndex = 0;

VOID SpoofTimerDpc(PKDPC dpc, PVOID context, PVOID sysArg1, PVOID sysArg2)
{
	UNREFERENCED_PARAMETER(dpc);
	UNREFERENCED_PARAMETER(context);
	UNREFERENCED_PARAMETER(sysArg1);
	UNREFERENCED_PARAMETER(sysArg2);

	if (!g_SpoofActive)
		return;

	ULONG currentSpoofer = g_SpooferIndex % 4;
	g_SpooferIndex++;
	g_CycleCount++;

	NTSTATUS status = PsCreateSystemThread(&g_SpoofWorkerThread, THREAD_ALL_ACCESS, nullptr, nullptr, nullptr, 
		[](PVOID param) -> VOID
		{
			UNREFERENCED_PARAMETER(param);
			ULONG spooferIndex = (g_SpooferIndex - 1) % 4;

			Logger::Output("=== SPOOF CYCLE #%lu - EXECUTING SPOOFER #%lu ===\n", g_CycleCount, spooferIndex);

			switch (spooferIndex)
			{
				case 0:
					Logger::Output(">>> Spoofing Disk Serial Number\n");
					StorageManager::DisableSmartOnAllDevices();
					StorageManager::ReplaceAllDiskIdentifiers();
					break;
				case 1:
					Logger::Output(">>> Spoofing Firmware/SMBIOS Serials\n");
					FirmwareConfiguration::ReplaceAllFirmwareSerials();
					break;
				case 2:
					Logger::Output(">>> Spoofing CPU Identifiers\n");
					CpuManager::SpoofCpuIdentifiers();
					break;
				case 3:
					Logger::Output(">>> Spoofing GPU Identifiers\n");
					GpuManager::RandomizeGpuIdentifiers();
					break;
			}

			Logger::Output("=== SPOOF CYCLE #%lu COMPLETED ===\n", g_CycleCount);

			PsTerminateSystemThread(STATUS_SUCCESS);
		}, 
		nullptr);

	if (NT_SUCCESS(status) && g_SpoofWorkerThread)
	{
		ZwClose(g_SpoofWorkerThread);
		g_SpoofWorkerThread = nullptr;
	}

	// Reschedule timer for next spoofer
	if (g_SpoofActive)
	{
		LARGE_INTEGER dueTime;
		dueTime.QuadPart = -10000000LL * g_IntervalSeconds;

		KeSetTimer(&g_SpoofTimer, dueTime, &g_SpoofDpc);
	}
}

NTSTATUS PeriodicSpoofer::InitializePeriodicSpoofer(ULONG intervalSeconds)
{
	if (intervalSeconds == 0 || intervalSeconds > 86400)
		return STATUS_INVALID_PARAMETER;

	g_IntervalSeconds = intervalSeconds;
	g_SpooferIndex = 0;

	Logger::Output("Periodic spoofer initialized | Interval: %lu seconds per spoofer\n", intervalSeconds);

	return STATUS_SUCCESS;
}

NTSTATUS PeriodicSpoofer::StartPeriodicSpoofer()
{
	if (g_SpoofActive)
	{
		Logger::Output("Periodic spoofer already running\n");
		return STATUS_ALREADY_INITIALIZED;
	}

	g_SpoofActive = TRUE;
	g_CycleCount = 0;

	// Initialize timer and DPC
	KeInitializeTimer(&g_SpoofTimer);
	KeInitializeDpc(&g_SpoofDpc, SpoofTimerDpc, nullptr);

	// Schedule first timer
	LARGE_INTEGER dueTime;
	dueTime.QuadPart = -10000000LL * g_IntervalSeconds;

	KeSetTimer(&g_SpoofTimer, dueTime, &g_SpoofDpc);

	Logger::Output("Periodic spoofer started | Next cycle in %lu seconds\n", g_IntervalSeconds);

	return STATUS_SUCCESS;
}

NTSTATUS PeriodicSpoofer::StopPeriodicSpoofer()
{
	if (!g_SpoofActive)
	{
		Logger::Output("Periodic spoofer is not running\n");
		return STATUS_NOT_FOUND;
	}

	g_SpoofActive = FALSE;

	KeCancelTimer(&g_SpoofTimer);

	Logger::Output("Periodic spoofer stopped after %lu cycles\n", g_CycleCount);

	return STATUS_SUCCESS;
}

NTSTATUS PeriodicSpoofer::ExecuteSpooferCycle()
{
	if (!g_SpoofActive)
		return STATUS_NOT_FOUND;

	Logger::Output("Manual spoof cycle executed\n");

	StorageManager::DisableSmartOnAllDevices();
	StorageManager::ReplaceAllDiskIdentifiers();
	Logger::Output("  > Disk serial spoofed\n");

	FirmwareConfiguration::ReplaceAllFirmwareSerials();
	Logger::Output("  > Firmware serial spoofed\n");

	CpuManager::SpoofCpuIdentifiers();
	Logger::Output("  > CPU identifier spoofed\n");

	GpuManager::RandomizeGpuIdentifiers();
	Logger::Output("  > GPU identifier spoofed\n");

	return STATUS_SUCCESS;
}

ULONG PeriodicSpoofer::GetSpooferCycleCount()
{
	return g_CycleCount;
}
