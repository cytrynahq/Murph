#include <ntifs.h>
#include "../Utilities/logger.hxx"
#include "../Managers/storage_manager.hxx"
#include "../Managers/firmware_config.hxx"

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT driverObject, PUNICODE_STRING registryPath)
{
	UNREFERENCED_PARAMETER(driverObject);
	UNREFERENCED_PARAMETER(registryPath);

	Logger::Output("Driver loaded. Build on %s\n", __DATE__);

	StorageManager::DisableSmartOnAllDevices();
	StorageManager::ReplaceAllDiskIdentifiers();
	FirmwareConfiguration::ReplaceAllFirmwareSerials();

	Logger::Output("Spoofing completed\n");

	return STATUS_SUCCESS;
}
