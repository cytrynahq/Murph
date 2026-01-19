#include <ntifs.h>
#include <ntstrsafe.h>
#include "../Utilities/logger.hxx"
#include "network_manager.hxx"

static SIZE_T SafeWcslen(const WCHAR* str, SIZE_T maxLen)
{
	if (!str)
		return 0;
	
	SIZE_T len = 0;
	while (len < maxLen && str[len] != L'\0')
		len++;
	
	return len;
}

NTSTATUS NetworkManager::SpoofAllMacAddresses()
{
	Logger::Output("MAC address spoofing initiated\n");

	HANDLE hKey = nullptr;
	OBJECT_ATTRIBUTES objAttr;
	UNICODE_STRING regPath;
	
	// Generate realistic MAC address
	unsigned char macAddr[6];
	unsigned long seed = KeQueryTimeIncrement();
	
	// Use common vendors (Intel, Nvidia, Broadcom, etc.)
	const unsigned char vendors[] = { 0x00, 0x1A, 0xA0, 0x50, 0xF2, 0x14, 0x08, 0x00 };
	unsigned int vendorIdx = RtlRandomEx(&seed) % 8;
	
	macAddr[0] = vendors[vendorIdx];
	for (int i = 1; i < 6; i++) {
		macAddr[i] = (unsigned char)(RtlRandomEx(&seed) % 256);
	}
	
	// Format MAC as string manually (no swprintf_s in kernel)
	WCHAR macString[32];
	const WCHAR* hexChars = L"0123456789ABCDEF";
	int idx = 0;
	for (int i = 0; i < 6; i++)
	{
		macString[idx++] = hexChars[(macAddr[i] >> 4) & 0xF];
		macString[idx++] = hexChars[macAddr[i] & 0xF];
		if (i < 5)
			macString[idx++] = L'-';
	}
	macString[idx] = L'\0';

	// Access network adapters registry
	RtlInitUnicodeString(&regPath, L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}");
	InitializeObjectAttributes(&objAttr, &regPath, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, nullptr, nullptr);

	NTSTATUS status = ZwOpenKey(&hKey, KEY_ALL_ACCESS, &objAttr);
	if (NT_SUCCESS(status))
	{
		// Apply MAC to all adapters found
		for (int adapterIdx = 0; adapterIdx < 32; adapterIdx++)
		{
			HANDLE adapterKey = nullptr;
			WCHAR subKeyName[64];
			RtlStringCbPrintfW(subKeyName, sizeof(subKeyName), L"%04d", adapterIdx);

			UNICODE_STRING adapterName;
			RtlInitUnicodeString(&adapterName, subKeyName);

			InitializeObjectAttributes(&objAttr, &adapterName, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, hKey, nullptr);
			
			if (NT_SUCCESS(ZwOpenKey(&adapterKey, KEY_ALL_ACCESS, &objAttr)))
			{
				UNICODE_STRING valueName;
				RtlInitUnicodeString(&valueName, L"NetworkAddress");
				SIZE_T macLen = SafeWcslen(macString, 32);
				ZwSetValueKey(adapterKey, &valueName, 0, REG_SZ, static_cast<PVOID>(macString), static_cast<ULONG>((macLen + 1) * sizeof(WCHAR)));

				// Spoof driver description
				const WCHAR* adapterNames[] = {
					L"Intel Ethernet Controller (2.5G)",
					L"NVIDIA nForce Networking Controller",
					L"Broadcom NetXtreme Gigabit Ethernet",
					L"Realtek PCIe GigE Family Controller",
					L"Qualcomm Atheros Killer E2400 Gigabit Ethernet"
				};
				
				unsigned long seed2 = KeQueryTimeIncrement() + adapterIdx;
				const WCHAR* adapterDesc = adapterNames[RtlRandomEx(&seed2) % 5];
				
				RtlInitUnicodeString(&valueName, L"DriverDesc");
				SIZE_T descLen = SafeWcslen(adapterDesc, 256);
				ZwSetValueKey(adapterKey, &valueName, 0, REG_SZ, static_cast<PVOID>(const_cast<WCHAR*>(adapterDesc)), static_cast<ULONG>((descLen + 1) * sizeof(WCHAR)));

				ZwClose(adapterKey);
				Logger::Status("Adapter %d spoofed with MAC: %S\n", adapterIdx, macString);
			}
		}
	}

	ZwClose(hKey);
	Logger::Status("Network adapter spoofing complete, MAC: %S\n", macString);
	return STATUS_SUCCESS;
}
