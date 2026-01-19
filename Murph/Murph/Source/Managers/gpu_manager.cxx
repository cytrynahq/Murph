#include <ntifs.h>
#include "../Utilities/logger.hxx"
#include "../Utilities/kernel_utilities.hxx"
#include "gpu_manager.hxx"

static SIZE_T SafeWcslen(const WCHAR* str, SIZE_T maxLen)
{
	if (!str)
		return 0;
	
	SIZE_T len = 0;
	while (len < maxLen && str[len] != L'\0')
		len++;
	
	return len;
}

NTSTATUS GpuManager::RandomizeGpuIdentifiers()
{
	Logger::Output("Randomizing GPU identifiers...\n");
	return ModifyGpuRegistry();
}

NTSTATUS GpuManager::ModifyGpuRegistry()
{
	Logger::Output("Modifying GPU registry entries\n");

	HANDLE hKey = nullptr;
	OBJECT_ATTRIBUTES objAttr;
	UNICODE_STRING regPath;

	// Realistic GPU model list
	const WCHAR* gpuModels[] = {
		L"NVIDIA GeForce RTX 4090",
		L"NVIDIA GeForce RTX 4080 Super",
		L"AMD Radeon RX 7900 XTX",
		L"Intel Arc A770",
		L"NVIDIA GeForce RTX 3090 Ti",
		L"AMD Radeon RX 6900 XT"
	};
	
	unsigned long seed = KeQueryTimeIncrement();
	unsigned int randomIdx = RtlRandomEx(&seed) % 6;
	const WCHAR* selectedGpu = gpuModels[randomIdx];

	// Access video adapter registry
	RtlInitUnicodeString(&regPath, L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\Video");
	InitializeObjectAttributes(&objAttr, &regPath, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, nullptr, nullptr);

	NTSTATUS status = ZwOpenKey(&hKey, KEY_ALL_ACCESS, &objAttr);
	if (NT_SUCCESS(status))
	{
		UNICODE_STRING valueName;
		RtlInitUnicodeString(&valueName, L"InstalledDisplayDrivers");
		SIZE_T gpuLen = SafeWcslen(selectedGpu, 256);
		ZwSetValueKey(hKey, &valueName, 0, REG_SZ, static_cast<PVOID>(const_cast<WCHAR*>(selectedGpu)), static_cast<ULONG>((gpuLen + 1) * sizeof(WCHAR)));
		
		RtlInitUnicodeString(&valueName, L"DeviceDesc");
		ZwSetValueKey(hKey, &valueName, 0, REG_SZ, static_cast<PVOID>(const_cast<WCHAR*>(selectedGpu)), static_cast<ULONG>((gpuLen + 1) * sizeof(WCHAR)));
		
		ZwClose(hKey);
	}

	// Also modify display driver registry
	RtlInitUnicodeString(&regPath, L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\GraphicsDrivers");
	InitializeObjectAttributes(&objAttr, &regPath, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, nullptr, nullptr);

	if (NT_SUCCESS(ZwOpenKey(&hKey, KEY_ALL_ACCESS, &objAttr)))
	{
		WCHAR newDriverId[256];
		KernelUtilities::GenerateRandomString(reinterpret_cast<char*>(newDriverId), 250);

		UNICODE_STRING valueName;
		RtlInitUnicodeString(&valueName, L"DriverVersion");
		ZwSetValueKey(hKey, &valueName, 0, REG_SZ, newDriverId, sizeof(newDriverId));

		RtlInitUnicodeString(&valueName, L"DeviceID");
		ZwSetValueKey(hKey, &valueName, 0, REG_SZ, newDriverId, sizeof(newDriverId));

		ZwClose(hKey);
	}

	// Modify NVIDIA/AMD specific registry paths if present
	RtlInitUnicodeString(&regPath, L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\Class\\{4D36E968-E325-11CE-BFC1-08002BE10318}");
	InitializeObjectAttributes(&objAttr, &regPath, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, nullptr, nullptr);

	if (NT_SUCCESS(ZwOpenKey(&hKey, KEY_ALL_ACCESS, &objAttr)))
	{
		WCHAR classId[256];
		KernelUtilities::GenerateRandomString(reinterpret_cast<char*>(classId), 250);

		UNICODE_STRING valueName;
		RtlInitUnicodeString(&valueName, L"DeviceDesc");
		ZwSetValueKey(hKey, &valueName, 0, REG_SZ, classId, sizeof(classId));

		RtlInitUnicodeString(&valueName, L"DriverDesc");
		ZwSetValueKey(hKey, &valueName, 0, REG_SZ, classId, sizeof(classId));

		ZwClose(hKey);
	}

	Logger::Status("GPU registry modifications applied\n");
	return STATUS_SUCCESS;
}
