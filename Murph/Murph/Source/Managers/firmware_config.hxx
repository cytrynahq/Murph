#pragma once
#include "../Core/kernel_interface.hxx"

namespace FirmwareConfiguration
{
	char* ExtractTableString(SMBIOS_TABLE_HEADER* tableHeader, SMBIOS_STRING_INDEX stringIndex);
	void ObfuscateTableString(char* stringPointer);
	void ObfuscateUUID(GUID* uuid);
	NTSTATUS ProcessBiosTable(SMBIOS_TABLE_HEADER* tableHeader);
	NTSTATUS ProcessSystemTable(SMBIOS_TABLE_HEADER* tableHeader);
	NTSTATUS ProcessBoardTable(SMBIOS_TABLE_HEADER* tableHeader);
	NTSTATUS ProcessChassisTable(SMBIOS_TABLE_HEADER* tableHeader);
	NTSTATUS ScanAndModifySmbiosTables(void* tableBase, ULONG tableSize);
	NTSTATUS ModifySystemRegistry();
	NTSTATUS ReplaceAllFirmwareSerials();
}
