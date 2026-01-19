#include <ntifs.h>
#include <ntstrsafe.h>
#include "../Utilities/kernel_utilities.hxx"
#include "../Core/kernel_interface.hxx"
#include "../Utilities/logger.hxx"
#include "storage_manager.hxx"

void StorageManager::DisableStorageHealthMonitoring(PSTORAGE_UNIT_CONFIGURATION storageUnit)
{
	if (!storageUnit)
		return;
	
	storageUnit->HealthInfo.Health.HealthFlags = 0;
}

PDEVICE_OBJECT StorageManager::AcquireRaidDeviceObject(const wchar_t* raidPortName)
{
	UNICODE_STRING devicePath;
	RtlInitUnicodeString(&devicePath, raidPortName);

	PFILE_OBJECT fileObject = nullptr;
	PDEVICE_OBJECT deviceObject = nullptr;
	
	NTSTATUS status = IoGetDeviceObjectPointer(&devicePath, FILE_READ_DATA, &fileObject, &deviceObject);
	
	if (!NT_SUCCESS(status))
		return nullptr;

	return deviceObject->DriverObject->DeviceObject;
}

NTSTATUS StorageManager::IterateAndModifyDeviceChain(PDEVICE_OBJECT deviceChain, StorPortRegisterCallback registerCallback)
{
	NTSTATUS resultStatus = STATUS_NOT_FOUND;

	if (!deviceChain)
		return STATUS_INVALID_PARAMETER;

	__try
	{
		while (deviceChain->NextDevice)
		{
			if (deviceChain->DeviceType == FILE_DEVICE_DISK)
			{
				if (!deviceChain->DeviceExtension)
				{
					deviceChain = deviceChain->NextDevice;
					continue;
				}

				PSTORAGE_UNIT_CONFIGURATION storageConfig = static_cast<PSTORAGE_UNIT_CONFIGURATION>(deviceChain->DeviceExtension);
				
				if (!storageConfig)
				{
					deviceChain = deviceChain->NextDevice;
					continue;
				}

				PSTORAGE_DEVICE_IDENTIFIER deviceId = &storageConfig->DeviceInfo.Device;
				
				if (!deviceId || !deviceId->SerialNumber.Buffer)
				{
					deviceChain = deviceChain->NextDevice;
					continue;
				}

				const int serialLength = deviceId->SerialNumber.Length;

				if (!serialLength)
				{
					deviceChain = deviceChain->NextDevice;
					continue;
				}

				// Store original for logging
				char originalSerial[256] = { 0 };
				memcpy(originalSerial, deviceId->SerialNumber.Buffer, serialLength);
				originalSerial[serialLength] = '\0';

				// Allocate buffer for new serial and generate random text
				char* newSerialBuffer = static_cast<char*>(ExAllocatePoolWithTag(NonPagedPool, serialLength, KERNEL_POOL_SIGNATURE));
				if (!newSerialBuffer)
				{
					deviceChain = deviceChain->NextDevice;
					continue;
				}

				newSerialBuffer[serialLength] = '\0';

				// Generate randomized serial
				KernelUtilities::GenerateRandomString(newSerialBuffer, serialLength);

				// Update string descriptor to point to new buffer
				RtlInitString(&deviceId->SerialNumber, newSerialBuffer);

				Logger::Status("Disk serial modified: %s -> %s\n", originalSerial, newSerialBuffer);

				resultStatus = STATUS_SUCCESS;

				// Free buffer - callback should copy the serial to permanent storage
				ExFreePool(newSerialBuffer);

				// Disable S.M.A.R.T monitoring (set bits directly for reliability)
				DisableStorageHealthMonitoring(storageConfig);

				// Notify StorPort of changes - this should copy the serial to registry
				if (registerCallback)
				{
					__try
					{
						registerCallback(storageConfig);
					}
					__except(EXCEPTION_EXECUTE_HANDLER)
					{
						Logger::Output("Exception in registerCallback, continuing...\n");
					}
				}
			}

			deviceChain = deviceChain->NextDevice;
		}
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		Logger::Output("Exception in IterateAndModifyDeviceChain\n");
		return STATUS_UNSUCCESSFUL;
	}

	return resultStatus;
}

NTSTATUS StorageManager::ReplaceAllDiskIdentifiers()
{
	PVOID storPortBase = KernelUtilities::ResolveModuleBase("storport.sys");
	if (!storPortBase)
	{
		Logger::Output("Cannot locate storport.sys\n");
		return STATUS_UNSUCCESSFUL;
	}

	StorPortRegisterCallback registerCallback = static_cast<StorPortRegisterCallback>(
		KernelUtilities::ScanImageSections(
			storPortBase,
			"\x48\x89\x5C\x24\x00\x55\x56\x57\x48\x83\xEC\x50",
			"xxxx?xxxxxxx"
		)
	);

	if (!registerCallback)
	{
		Logger::Output("Cannot locate StorPort register callback\n");
		return STATUS_UNSUCCESSFUL;
	}

	NTSTATUS finalStatus = STATUS_NOT_FOUND;

	// Loop through RAID ports - typically NVMe on port 1, SATA on port 0
	for (int portIndex = 0; portIndex < 2; portIndex++)
	{
		wchar_t portNameBuffer[18];
		RtlStringCbPrintfW(portNameBuffer, sizeof(portNameBuffer), L"\\Device\\RaidPort%d", portIndex);

		__try
		{
			PDEVICE_OBJECT raidDevice = AcquireRaidDeviceObject(portNameBuffer);
			if (!raidDevice)
				continue;

			NTSTATUS loopResult = IterateAndModifyDeviceChain(raidDevice, registerCallback);
			if (NT_SUCCESS(loopResult))
				finalStatus = loopResult;
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
			Logger::Output("Exception accessing RaidPort%d, continuing...\n", portIndex);
		}
	}

	return finalStatus;
}

NTSTATUS StorageManager::DisableSmartOnAllDevices()
{
	PVOID diskSysBase = KernelUtilities::ResolveModuleBase("disk.sys");
	if (!diskSysBase)
	{
		Logger::Output("Cannot locate disk.sys\n");
		return STATUS_UNSUCCESSFUL;
	}

	DiskSmartControlRoutine smartControlFunction = static_cast<DiskSmartControlRoutine>(
		KernelUtilities::ScanImageSections(
			diskSysBase,
			"\x4C\x8B\xDC\x49\x89\x5B\x10\x49\x89\x7B\x18\x55\x49\x8D\x6B\xA1\x48\x81\xEC\x00\x00\x00\x00\x48\x8B\x05\x00\x00\x00\x00\x48\x33\xC4\x48\x89\x45\x4F",
			"xxxxxxxxxxxxxxxxxxx????xxx????xxxxxxx"
		)
	);

	if (!smartControlFunction)
	{
		Logger::Output("Cannot locate S.M.A.R.T control function\n");
		return STATUS_UNSUCCESSFUL;
	}

	UNICODE_STRING diskDriverName;
	RtlInitUnicodeString(&diskDriverName, L"\\Driver\\Disk");

	PDRIVER_OBJECT diskDriver = nullptr;
	NTSTATUS status = ObReferenceObjectByName(
		&diskDriverName,
		OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
		nullptr,
		0,
		*IoDriverObjectType,
		KernelMode,
		nullptr,
		reinterpret_cast<PVOID*>(&diskDriver)
	);

	if (!NT_SUCCESS(status))
	{
		Logger::Output("Cannot acquire disk driver reference\n");
		return STATUS_UNSUCCESSFUL;
	}

	PDEVICE_OBJECT deviceObjectList[64];
	RtlZeroMemory(deviceObjectList, sizeof(deviceObjectList));

	ULONG deviceCount = 0;
	status = IoEnumerateDeviceObjectList(diskDriver, deviceObjectList, sizeof(deviceObjectList), &deviceCount);

	if (!NT_SUCCESS(status))
	{
		Logger::Output("Cannot enumerate disk devices\n");
		ObDereferenceObject(diskDriver);
		return STATUS_UNSUCCESSFUL;
	}

	// Disable S.M.A.R.T on each device
	__try
	{
		for (ULONG deviceIdx = 0; deviceIdx < deviceCount; deviceIdx++)
		{
			PDEVICE_OBJECT currentDevice = deviceObjectList[deviceIdx];
			if (!currentDevice || !currentDevice->DeviceExtension)
			{
				if (currentDevice)
					ObDereferenceObject(currentDevice);
				continue;
			}

			__try
			{
				smartControlFunction(currentDevice->DeviceExtension, false);
			}
			__except(EXCEPTION_EXECUTE_HANDLER)
			{
				Logger::Output("Exception in smartControlFunction for device %lu\n", deviceIdx);
			}

			ObDereferenceObject(currentDevice);
		}
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		Logger::Output("Exception in device processing loop\n");
		// Clean up remaining device references
		for (ULONG deviceIdx = 0; deviceIdx < deviceCount; deviceIdx++)
		{
			if (deviceObjectList[deviceIdx])
				ObDereferenceObject(deviceObjectList[deviceIdx]);
		}
		ObDereferenceObject(diskDriver);
		return STATUS_UNSUCCESSFUL;
	}

	ObDereferenceObject(diskDriver);
	return STATUS_SUCCESS;
}
