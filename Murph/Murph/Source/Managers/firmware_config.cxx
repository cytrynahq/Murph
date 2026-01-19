#include <ntifs.h>
#include "../Utilities/logger.hxx"
#include "../Utilities/kernel_utilities.hxx"
#include "../Core/kernel_interface.hxx"
#include "firmware_config.hxx"

static SIZE_T SafeWcslen(const WCHAR* str, SIZE_T maxLen)
{
	if (!str)
		return 0;
	
	SIZE_T len = 0;
	while (len < maxLen && str[len] != L'\0')
		len++;
	
	return len;
}

NTSTATUS FirmwareConfiguration::ModifySystemRegistry()
{
	Logger::Status("Modifying system registry for firmware persistence...\n");

	UNICODE_STRING sysRegPath;
	RtlInitUnicodeString(&sysRegPath, L"\\Registry\\Machine\\HARDWARE\\Description\\System");

	OBJECT_ATTRIBUTES objAttr;
	InitializeObjectAttributes(&objAttr, &sysRegPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, nullptr, nullptr);

	HANDLE keyHandle = nullptr;
	NTSTATUS status = ZwOpenKey(&keyHandle, KEY_ALL_ACCESS, &objAttr);
	if (!NT_SUCCESS(status))
	{
		Logger::Output("Cannot open system registry\n");
		return status;
	}

	WCHAR systemUuid[128];
	ULONG seed = KeQueryTimeIncrement();
	WCHAR charset[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

	RtlZeroMemory(systemUuid, sizeof(systemUuid));
	for (int i = 0; i < 36; i++)
	{
		if (i == 8 || i == 13 || i == 18 || i == 23)
			systemUuid[i] = L'-';
		else
		{
			ULONG randomVal = RtlRandomEx(&seed);
			systemUuid[i] = charset[randomVal % 36];
		}
	}

	UNICODE_STRING bioVersionName;
	RtlInitUnicodeString(&bioVersionName, L"SystemBiosVersion");
	ZwSetValueKey(keyHandle, &bioVersionName, 0, REG_SZ, systemUuid, sizeof(systemUuid));

	UNICODE_STRING identifierName;
	RtlInitUnicodeString(&identifierName, L"Identifier");
	ZwSetValueKey(keyHandle, &identifierName, 0, REG_SZ, systemUuid, sizeof(systemUuid));

	ZwClose(keyHandle);

	UNICODE_STRING acpiPath;
	RtlInitUnicodeString(&acpiPath, L"\\Registry\\Machine\\HARDWARE\\ACPITAB");

	InitializeObjectAttributes(&objAttr, &acpiPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, nullptr, nullptr);
	status = ZwOpenKey(&keyHandle, KEY_ALL_ACCESS, &objAttr);
	
	if (NT_SUCCESS(status))
	{
		UNICODE_STRING biosDataName;
		RtlInitUnicodeString(&biosDataName, L"RSDP");
		ZwSetValueKey(keyHandle, &biosDataName, 0, REG_BINARY, systemUuid, sizeof(systemUuid));
		ZwClose(keyHandle);
	}

	UNICODE_STRING sysInfoPath;
	RtlInitUnicodeString(&sysInfoPath, L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\SystemInformation");
	InitializeObjectAttributes(&objAttr, &sysInfoPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, nullptr, nullptr);
	
	if (NT_SUCCESS(ZwOpenKey(&keyHandle, KEY_ALL_ACCESS, &objAttr)))
	{
		const WCHAR* manufacturers[] = { L"ASUS", L"MSI", L"Gigabyte", L"ASRock", L"Biostar", L"ECS" };
		unsigned long mfgSeed = KeQueryTimeIncrement();
		const WCHAR* manufacturer = manufacturers[RtlRandomEx(&mfgSeed) % 6];
		
		UNICODE_STRING valueName;
		RtlInitUnicodeString(&valueName, L"SystemManufacturer");
		SIZE_T mfgLen = SafeWcslen(manufacturer, 256);
		ZwSetValueKey(keyHandle, &valueName, 0, REG_SZ, static_cast<PVOID>(const_cast<WCHAR*>(manufacturer)), static_cast<ULONG>((mfgLen + 1) * sizeof(WCHAR)));
		
		const WCHAR* productNames[] = { 
			L"PRIME Z890-A", 
			L"B850-E EDGE WIFI", 
			L"ROG STRIX Z890-E", 
			L"MEG Z890-ACE",
			L"X870E-E GAMING WIFI",
			L"Z790-E D4"
		};
		const WCHAR* productName = productNames[RtlRandomEx(&mfgSeed) % 6];
		
		RtlInitUnicodeString(&valueName, L"SystemProductName");
		SIZE_T prodLen = SafeWcslen(productName, 256);
		ZwSetValueKey(keyHandle, &valueName, 0, REG_SZ, static_cast<PVOID>(const_cast<WCHAR*>(productName)), static_cast<ULONG>((prodLen + 1) * sizeof(WCHAR)));
		
		WCHAR serialNum[64];
		unsigned int num1 = RtlRandomEx(&mfgSeed) % 1000;
		unsigned int num2 = RtlRandomEx(&mfgSeed) % 100000;
		
		// Build serial: AUS + 3-digit + 5-digit
		int pos = 0;
		serialNum[pos++] = L'A';
		serialNum[pos++] = L'U';
		serialNum[pos++] = L'S';
		
		// Add first 3 digits
		serialNum[pos++] = L'0' + (num1 / 100);
		serialNum[pos++] = L'0' + ((num1 / 10) % 10);
		serialNum[pos++] = L'0' + (num1 % 10);
		
		serialNum[pos++] = L'0' + (num2 / 10000);
		serialNum[pos++] = L'0' + ((num2 / 1000) % 10);
		serialNum[pos++] = L'0' + ((num2 / 100) % 10);
		serialNum[pos++] = L'0' + ((num2 / 10) % 10);
		serialNum[pos++] = L'0' + (num2 % 10);
		serialNum[pos] = L'\0';
		
		RtlInitUnicodeString(&valueName, L"SystemSerialNumber");
		SIZE_T serialLen = SafeWcslen(serialNum, 64);
		ZwSetValueKey(keyHandle, &valueName, 0, REG_SZ, static_cast<PVOID>(serialNum), static_cast<ULONG>((serialLen + 1) * sizeof(WCHAR)));
		
		ZwClose(keyHandle);
	}

	Logger::Status("System registry modified successfully\n");
	return STATUS_SUCCESS;
}

char* FirmwareConfiguration::ExtractTableString(SMBIOS_TABLE_HEADER* tableHeader, SMBIOS_STRING_INDEX stringIndex)
{
	if (!tableHeader || !stringIndex)
		return nullptr;

	const char* stringBase = reinterpret_cast<const char*>(tableHeader) + tableHeader->Length;

	if (*stringBase == 0)
		return nullptr;

	// Walk through null-terminated strings to reach the target index
	while (--stringIndex)
	{
		stringBase += strlen(stringBase) + 1;
	}

	return const_cast<char*>(stringBase);
}

void FirmwareConfiguration::ObfuscateTableString(char* stringPointer)
{
	if (!stringPointer)
		return;

	const int stringLength = static_cast<int>(strlen(stringPointer));
	if (stringLength <= 0)
		return;

	// Allocate temporary buffer for randomized string
	char* randomBuffer = static_cast<char*>(ExAllocatePoolWithTag(NonPagedPool, stringLength, KERNEL_POOL_SIGNATURE));
	if (!randomBuffer)
		return;

	// Generate randomized string in temp buffer
	KernelUtilities::GenerateRandomString(randomBuffer, stringLength);
	randomBuffer[stringLength] = '\0';

	// Copy randomized string back to original location
	memcpy(stringPointer, randomBuffer, stringLength);

	ExFreePool(randomBuffer);
}

void FirmwareConfiguration::ObfuscateUUID(GUID* uuid)
{
	if (!uuid)
		return;

	ULONG seed = KeQueryTimeIncrement();

	// Randomize each part of the GUID
	uuid->Data1 = RtlRandomEx(&seed);
	uuid->Data2 = (USHORT)RtlRandomEx(&seed);
	uuid->Data3 = (USHORT)RtlRandomEx(&seed);

	for (int i = 0; i < 8; i++)
	{
		uuid->Data4[i] = (UCHAR)RtlRandomEx(&seed);
	}

	Logger::Output("UUID obfuscated\n");
}

NTSTATUS FirmwareConfiguration::ProcessBiosTable(SMBIOS_TABLE_HEADER* tableHeader)
{
	if (!tableHeader || !tableHeader->Length)
		return STATUS_UNSUCCESSFUL;

	SMBIOS_SYSTEM_BIOS* biosTable = reinterpret_cast<SMBIOS_SYSTEM_BIOS*>(tableHeader);

	char* vendorString = ExtractTableString(tableHeader, biosTable->Vendor);
	if (vendorString && strlen(vendorString) > 0)
	{
		Logger::Output("Obfuscating BIOS vendor: %s\n", vendorString);
		ObfuscateTableString(vendorString);
	}

	return STATUS_SUCCESS;
}

NTSTATUS FirmwareConfiguration::ProcessSystemTable(SMBIOS_TABLE_HEADER* tableHeader)
{
	if (!tableHeader || !tableHeader->Length)
		return STATUS_UNSUCCESSFUL;

	SMBIOS_SYSTEM_INFO* systemTable = reinterpret_cast<SMBIOS_SYSTEM_INFO*>(tableHeader);

	char* manufacturerString = ExtractTableString(tableHeader, systemTable->Manufacturer);
	if (manufacturerString && strlen(manufacturerString) > 0)
	{
		Logger::Output("Obfuscating System manufacturer: %s\n", manufacturerString);
		ObfuscateTableString(manufacturerString);
	}

	char* productString = ExtractTableString(tableHeader, systemTable->ProductName);
	if (productString && strlen(productString) > 0)
	{
		Logger::Output("Obfuscating System product: %s\n", productString);
		ObfuscateTableString(productString);
	}

	char* serialString = ExtractTableString(tableHeader, systemTable->SerialNumber);
	if (serialString && strlen(serialString) > 0)
	{
		Logger::Output("Obfuscating System serial: %s\n", serialString);
		ObfuscateTableString(serialString);
	}

	// Randomize system UUID
	ObfuscateUUID(&systemTable->SystemUuid);

	return STATUS_SUCCESS;
}

NTSTATUS FirmwareConfiguration::ProcessBoardTable(SMBIOS_TABLE_HEADER* tableHeader)
{
	if (!tableHeader || !tableHeader->Length)
		return STATUS_UNSUCCESSFUL;

	SMBIOS_BASEBOARD_INFO* boardTable = reinterpret_cast<SMBIOS_BASEBOARD_INFO*>(tableHeader);

	char* manufacturerString = ExtractTableString(tableHeader, boardTable->Manufacturer);
	if (manufacturerString && strlen(manufacturerString) > 0)
	{
		Logger::Output("Obfuscating Baseboard manufacturer: %s\n", manufacturerString);
		ObfuscateTableString(manufacturerString);
	}

	char* productString = ExtractTableString(tableHeader, boardTable->ProductName);
	if (productString && strlen(productString) > 0)
	{
		Logger::Output("Obfuscating Baseboard product: %s\n", productString);
		ObfuscateTableString(productString);
	}

	char* serialString = ExtractTableString(tableHeader, boardTable->SerialNumber);
	if (serialString && strlen(serialString) > 0)
	{
		Logger::Output("Obfuscating Baseboard serial: %s\n", serialString);
		ObfuscateTableString(serialString);
	}

	return STATUS_SUCCESS;
}

NTSTATUS FirmwareConfiguration::ProcessChassisTable(SMBIOS_TABLE_HEADER* tableHeader)
{
	if (!tableHeader || !tableHeader->Length)
		return STATUS_UNSUCCESSFUL;

	SMBIOS_CHASSIS_INFO* chassisTable = reinterpret_cast<SMBIOS_CHASSIS_INFO*>(tableHeader);

	char* manufacturerString = ExtractTableString(tableHeader, chassisTable->Manufacturer);
	if (manufacturerString && strlen(manufacturerString) > 0)
	{
		Logger::Output("Obfuscating Chassis manufacturer: %s\n", manufacturerString);
		ObfuscateTableString(manufacturerString);
	}

	char* serialString = ExtractTableString(tableHeader, chassisTable->SerialNumber);
	if (serialString && strlen(serialString) > 0)
	{
		Logger::Output("Obfuscating Chassis serial: %s\n", serialString);
		ObfuscateTableString(serialString);
	}

	return STATUS_SUCCESS;
}

NTSTATUS FirmwareConfiguration::ScanAndModifySmbiosTables(void* tableBase, ULONG tableSize)
{
	if (!tableBase || tableSize == 0 || tableSize > 0x10000)
		return STATUS_INVALID_PARAMETER;

	__try
	{
		char* currentAddress = static_cast<char*>(tableBase);
		char* boundaryAddress = currentAddress + tableSize;

		while (currentAddress < boundaryAddress)
		{
			// Validate we haven't gone past boundary
			if (currentAddress + sizeof(SMBIOS_TABLE_HEADER) > boundaryAddress)
				break;

			SMBIOS_TABLE_HEADER* tableHeader = reinterpret_cast<SMBIOS_TABLE_HEADER*>(currentAddress);
			
			// Validate table header
			if (tableHeader->Length == 0 || tableHeader->Length > 256)
				break;

			// End-of-tables marker (Type 127, Length 4)
			if (tableHeader->Type == 127 && tableHeader->Length == 4)
				break;

			// Validate table doesn't extend past boundary
			if (currentAddress + tableHeader->Length > boundaryAddress)
				break;

			// Process table based on type
			__try
			{
				switch (tableHeader->Type)
				{
					case 0:
						ProcessBiosTable(tableHeader);
						break;
					case 1:
						ProcessSystemTable(tableHeader);
						break;
					case 2:
						ProcessBoardTable(tableHeader);
						break;
					case 3:
						ProcessChassisTable(tableHeader);
						break;
				}
			}
			__except(EXCEPTION_EXECUTE_HANDLER)
			{
				Logger::Output("Exception processing SMBIOS table type %d, continuing...\n", tableHeader->Type);
			}

			// Skip to next table (after length + terminating double null)
			char* nextTable = currentAddress + tableHeader->Length;
			if (nextTable >= boundaryAddress)
				break;

			// Find end of string area (double null terminator)
			int safetyCount = 0;
			while (nextTable < boundaryAddress && safetyCount < 512)
			{
				if (*nextTable == 0 && *(nextTable + 1) == 0)
				{
					nextTable += 2;
					break;
				}
				nextTable++;
				safetyCount++;
			}

			if (nextTable >= boundaryAddress || safetyCount >= 512)
				break;

			currentAddress = nextTable;
		}
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		Logger::Output("Exception in ScanAndModifySmbiosTables\n");
		return STATUS_UNSUCCESSFUL;
	}

	return STATUS_SUCCESS;
}

NTSTATUS FirmwareConfiguration::ReplaceAllFirmwareSerials()
{
	Logger::Status("Spoofing all firmware serials (SMBIOS, Motherboard, System)...\n");
	
	// Locate SMBIOS physical address via pattern scan in ntoskrnl
	PVOID ntoskrnlBase = KernelUtilities::ResolveModuleBase("ntoskrnl.exe");
	if (!ntoskrnlBase)
	{
		Logger::Output("Cannot locate ntoskrnl.exe\n");
		return STATUS_UNSUCCESSFUL;
	}

	// Find SMBIOS physical address reference
	PVOID physicalAddrPattern = KernelUtilities::ScanImageSections(
		ntoskrnlBase,
		"\x48\x8B\x0D\x00\x00\x00\x00\x48\x85\xC9\x74\x00\x8B\x15",
		"xxx????xxxx?xx"
	);

	if (!physicalAddrPattern)
	{
		Logger::Output("Cannot locate SMBIOS physical address pattern\n");
		return STATUS_UNSUCCESSFUL;
	}

	// Resolve physical address pointer from instruction
	PPHYSICAL_ADDRESS physicalAddress = reinterpret_cast<PPHYSICAL_ADDRESS>(
		reinterpret_cast<char*>(physicalAddrPattern) + 7 + 
		*reinterpret_cast<int*>(reinterpret_cast<char*>(physicalAddrPattern) + 3)
	);

	if (!physicalAddress || !physicalAddress->QuadPart)
	{
		Logger::Output("SMBIOS physical address is null\n");
		return STATUS_UNSUCCESSFUL;
	}

	// Find SMBIOS table length reference
	PVOID lengthPattern = KernelUtilities::ScanImageSections(
		ntoskrnlBase,
		"\x8B\x1D\x00\x00\x00\x00\x48\x8B\xD0\x44\x8B\xC3\x48\x8B\xCD\xE8\x00\x00\x00\x00\x8B\xD3\x48\x8B",
		"xx????xxxxxxxxxx????xxxx"
	);

	if (!lengthPattern)
	{
		Logger::Output("Cannot locate SMBIOS table length pattern\n");
		return STATUS_UNSUCCESSFUL;
	}

	// Resolve table length from instruction
	ULONG tableLength = *reinterpret_cast<ULONG*>(
		static_cast<char*>(lengthPattern) + 6 + 
		*reinterpret_cast<int*>(static_cast<char*>(lengthPattern) + 2)
	);

	if (!tableLength || tableLength > 0x10000)
	{
		Logger::Output("SMBIOS table length is invalid\n");
		return STATUS_UNSUCCESSFUL;
	}

	// Map SMBIOS structures into virtual memory
	PVOID mappedTables = MmMapIoSpace(*physicalAddress, tableLength, MmNonCached);
	if (!mappedTables)
	{
		Logger::Output("Cannot map SMBIOS structures\n");
		return STATUS_UNSUCCESSFUL;
	}

	__try
	{
		// Modify SMBIOS tables directly in physical memory
		ScanAndModifySmbiosTables(mappedTables, tableLength);
		Logger::Status("SMBIOS tables modified in physical memory\n");
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		Logger::Output("Exception modifying SMBIOS tables\n");
		MmUnmapIoSpace(mappedTables, tableLength);
		return STATUS_UNSUCCESSFUL;
	}

	MmUnmapIoSpace(mappedTables, tableLength);

	// Also modify registry for persistence
	HANDLE hKey = nullptr;
	UNICODE_STRING regPath;
	OBJECT_ATTRIBUTES objAttr;

	// Modify System UUID
	RtlInitUnicodeString(&regPath, L"\\Registry\\Machine\\HARDWARE\\Description\\System");
	InitializeObjectAttributes(&objAttr, &regPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, nullptr, nullptr);

	if (NT_SUCCESS(ZwOpenKey(&hKey, KEY_ALL_ACCESS, &objAttr)))
	{
		__try
		{
			// Generate valid UUID format: 8-4-4-4-12 hex characters
			WCHAR newUuid[64];
			ULONG seed = KeQueryTimeIncrement();
			const WCHAR* hexChars = L"0123456789ABCDEF";
			
			// Format: XXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX
			int pos = 0;
			for (int i = 0; i < 8; i++)
				newUuid[pos++] = hexChars[RtlRandomEx(&seed) % 16];
			newUuid[pos++] = L'-';
			for (int i = 0; i < 4; i++)
				newUuid[pos++] = hexChars[RtlRandomEx(&seed) % 16];
			newUuid[pos++] = L'-';
			for (int i = 0; i < 4; i++)
				newUuid[pos++] = hexChars[RtlRandomEx(&seed) % 16];
			newUuid[pos++] = L'-';
			for (int i = 0; i < 4; i++)
				newUuid[pos++] = hexChars[RtlRandomEx(&seed) % 16];
			newUuid[pos++] = L'-';
			for (int i = 0; i < 12; i++)
				newUuid[pos++] = hexChars[RtlRandomEx(&seed) % 16];
			newUuid[pos] = L'\0';

			UNICODE_STRING valueName;
			RtlInitUnicodeString(&valueName, L"SystemBiosVersion");
			SIZE_T uuidLen = SafeWcslen(newUuid, 64);
			ZwSetValueKey(hKey, &valueName, 0, REG_SZ, static_cast<PVOID>(newUuid), static_cast<ULONG>((uuidLen + 1) * sizeof(WCHAR)));

			RtlInitUnicodeString(&valueName, L"Identifier");
			ZwSetValueKey(hKey, &valueName, 0, REG_SZ, static_cast<PVOID>(newUuid), static_cast<ULONG>((uuidLen + 1) * sizeof(WCHAR)));

			RtlInitUnicodeString(&valueName, L"HardwareAbstractionLayer");
			ZwSetValueKey(hKey, &valueName, 0, REG_SZ, static_cast<PVOID>(newUuid), static_cast<ULONG>((uuidLen + 1) * sizeof(WCHAR)));

			ZwClose(hKey);
			Logger::Status("System registry UUIDs modified\n");
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
			if (hKey)
				ZwClose(hKey);
			Logger::Output("Exception in system UUID modification, continuing...\n");
		}
	}

	// Modify Motherboard/DMI data registry
	RtlInitUnicodeString(&regPath, L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Control\\SystemInformation");
	InitializeObjectAttributes(&objAttr, &regPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, nullptr, nullptr);

	if (NT_SUCCESS(ZwOpenKey(&hKey, KEY_ALL_ACCESS, &objAttr)))
	{
		__try
		{
			WCHAR newData[256];
			KernelUtilities::GenerateRandomString(reinterpret_cast<char*>(newData), 250);
			newData[255] = L'\0';

			UNICODE_STRING valueName;
			RtlInitUnicodeString(&valueName, L"ComputerHardwareId");
			SIZE_T dataLen = SafeWcslen(newData, 256);
			ZwSetValueKey(hKey, &valueName, 0, REG_SZ, static_cast<PVOID>(newData), static_cast<ULONG>((dataLen + 1) * sizeof(WCHAR)));

			ZwClose(hKey);
			Logger::Status("Motherboard registry modified\n");
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
			if (hKey)
				ZwClose(hKey);
			Logger::Output("Exception in motherboard registry modification, continuing...\n");
		}
	}

	// Also call ModifySystemRegistry for additional registry entries
	__try
	{
		ModifySystemRegistry();
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		Logger::Output("Exception in ModifySystemRegistry, continuing...\n");
	}

	return STATUS_SUCCESS;
}
