#pragma once
#include "../Core/kernel_interface.hxx"

namespace StorageManager
{
	void DisableStorageHealthMonitoring(PSTORAGE_UNIT_CONFIGURATION storageUnit);
	PDEVICE_OBJECT AcquireRaidDeviceObject(const wchar_t* raidPortName);
	NTSTATUS IterateAndModifyDeviceChain(PDEVICE_OBJECT deviceChain, StorPortRegisterCallback registerCallback);
	NTSTATUS ReplaceAllDiskIdentifiers();
	NTSTATUS DisableSmartOnAllDevices();
}
