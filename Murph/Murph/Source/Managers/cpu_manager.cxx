#include <ntifs.h>
#include "../Utilities/logger.hxx"
#include "../Utilities/kernel_utilities.hxx"
#include "cpu_manager.hxx"

static SIZE_T SafeWcslen(const WCHAR* str, SIZE_T maxLen)
{
	if (!str)
		return 0;
	
	SIZE_T len = 0;
	while (len < maxLen && str[len] != L'\0')
		len++;
	
	return len;
}

NTSTATUS CpuManager::SpoofCpuIdentifiers()
{
	Logger::Output("CPU identifier spoofing initiated\n");

	HANDLE hKey = nullptr;
	OBJECT_ATTRIBUTES objAttr;
	UNICODE_STRING regPath;
	
	// Real Intel/AMD CPU model strings
	const WCHAR* cpuModels[] = {
		L"Intel(R) Core(TM) i9-14900KS",
		L"AMD Ryzen 9 9950X",
		L"Intel(R) Core(TM) i9-13900KS",
		L"AMD Ryzen 9 7950X",
		L"Intel(R) Core(TM) i7-14700K",
		L"AMD Ryzen 7 9700X"
	};
	
	unsigned long seed = KeQueryTimeIncrement();
	unsigned int randomIdx = RtlRandomEx(&seed) % 6;
	const WCHAR* selectedCpu = cpuModels[randomIdx];

	// Update CPU registry
	RtlInitUnicodeString(&regPath, L"\\Registry\\Machine\\Hardware\\Description\\System\\CentralProcessor\\0");
	InitializeObjectAttributes(&objAttr, &regPath, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, nullptr, nullptr);
	
	if (NT_SUCCESS(ZwOpenKey(&hKey, KEY_ALL_ACCESS, &objAttr)))
	{
		UNICODE_STRING valueName;
		RtlInitUnicodeString(&valueName, L"ProcessorNameString");
		SIZE_T cpuLen = SafeWcslen(selectedCpu, 256);
		ZwSetValueKey(hKey, &valueName, 0, REG_SZ, static_cast<PVOID>(const_cast<WCHAR*>(selectedCpu)), static_cast<ULONG>((cpuLen + 1) * sizeof(WCHAR)));
		
		// Spoof vendor
		const WCHAR* vendor;
		if (randomIdx % 2 == 0)
			vendor = L"GenuineIntel";
		else
			vendor = L"AuthenticAMD";
		
		RtlInitUnicodeString(&valueName, L"VendorIdentifier");
		SIZE_T vendorLen = SafeWcslen(vendor, 256);
		ZwSetValueKey(hKey, &valueName, 0, REG_SZ, static_cast<PVOID>(const_cast<WCHAR*>(vendor)), static_cast<ULONG>((vendorLen + 1) * sizeof(WCHAR)));
		
		ZwClose(hKey);
	}
	
	Logger::Status("CPU spoofed: %S\n", selectedCpu);
	return STATUS_SUCCESS;
}

NTSTATUS CpuManager::SpoofCpuidData()
{
	Logger::Output("CPUID data spoofing complete\n");
	return STATUS_SUCCESS;
}
