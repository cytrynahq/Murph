#pragma once
#include <ntifs.h>
#include <ntstrsafe.h>

#define KERNEL_POOL_SIGNATURE 'KrnL'
#define MAX_DEVICE_CHAIN 128
#define MAX_RAID_PORTS 4

typedef enum _KERNEL_INFORMATION_CLASS
{
	InfoClassMin = 0,
	BasicInfo = 0,
	ProcessorInfo = 1,
	PerformanceInfo = 2,
	TimeOfDayInfo = 3,
	ModuleInfo = 11,
	HandleInfo = 16,
	InfoClassMax
} KERNEL_INFORMATION_CLASS;

typedef struct _KERNEL_MODULE_ENTRY
{
	ULONG_PTR Reserved[2];
	PVOID Base;
	ULONG Size;
	ULONG Flags;
	USHORT Index;
	USHORT Unknown;
	USHORT LoadCount;
	USHORT ModuleNameOffset;
	CHAR ImageName[256];
} KERNEL_MODULE_ENTRY, *PKERNEL_MODULE_ENTRY;

typedef struct _KERNEL_MODULE_LIST
{
	ULONG_PTR ModuleCount;
	KERNEL_MODULE_ENTRY Modules[1];
} KERNEL_MODULE_LIST, *PKERNEL_MODULE_LIST;

typedef struct _STORAGE_DEVICE_IDENTIFIER
{
	char Padding[0x8];
	STRING SerialNumber;
} STORAGE_DEVICE_IDENTIFIER, *PSTORAGE_DEVICE_IDENTIFIER;

typedef struct _STORAGE_HEALTH_TELEMETRY
{
	int HealthFlags;
} STORAGE_HEALTH_TELEMETRY, *PSTORAGE_HEALTH_TELEMETRY;

typedef struct _STORAGE_UNIT_CONFIGURATION
{
	union
	{
		struct
		{
			char Offset[0x68];
			STORAGE_DEVICE_IDENTIFIER Device;
		} DeviceInfo;

		struct
		{
			char Offset[0x7c8];
			STORAGE_HEALTH_TELEMETRY Health;
		} HealthInfo;
	};
} STORAGE_UNIT_CONFIGURATION, *PSTORAGE_UNIT_CONFIGURATION;

typedef __int64(__fastcall* StorPortRegisterCallback)(PSTORAGE_UNIT_CONFIGURATION cfg);

typedef NTSTATUS(__fastcall* DiskSmartControlRoutine)(void* config, bool enable);

typedef struct
{
	UINT8   Type;
	UINT8   Length;
	UINT8   Handle[2];
} SMBIOS_TABLE_HEADER;

typedef UINT8 SMBIOS_STRING_INDEX;

typedef struct
{
	SMBIOS_TABLE_HEADER Header;
	SMBIOS_STRING_INDEX Vendor;
	SMBIOS_STRING_INDEX BiosVersion;
	UINT8 BiosSegment[2];
	SMBIOS_STRING_INDEX BiosReleaseDate;
	UINT8 BiosSize;
	UINT8 BiosCharacteristics[8];
} SMBIOS_SYSTEM_BIOS;

typedef struct
{
	SMBIOS_TABLE_HEADER Header;
	SMBIOS_STRING_INDEX Manufacturer;
	SMBIOS_STRING_INDEX ProductName;
	SMBIOS_STRING_INDEX Version;
	SMBIOS_STRING_INDEX SerialNumber;
	GUID SystemUuid;
	UINT8 WakeUpType;
} SMBIOS_SYSTEM_INFO;

typedef struct
{
	SMBIOS_TABLE_HEADER Header;
	SMBIOS_STRING_INDEX Manufacturer;
	SMBIOS_STRING_INDEX ProductName;
	SMBIOS_STRING_INDEX Version;
	SMBIOS_STRING_INDEX SerialNumber;
} SMBIOS_BASEBOARD_INFO;

typedef struct
{
	SMBIOS_TABLE_HEADER Header;
	SMBIOS_STRING_INDEX Manufacturer;
	UINT8 ChassisType;
	SMBIOS_STRING_INDEX Version;
	SMBIOS_STRING_INDEX SerialNumber;
	SMBIOS_STRING_INDEX AssetTag;
	UINT8 BootupState;
	UINT8 PowerSupplyState;
	UINT8 ThermalState;
	UINT8 SecurityStatus;
	UINT8 OemDefined[4];
} SMBIOS_CHASSIS_INFO;

typedef struct
{
	UINT8 AnchorString[4];
	UINT8 EntryPointChecksum;
	UINT8 EntryPointLength;
	UINT8 MajorVersion;
	UINT8 MinorVersion;
	UINT16 MaxStructureSize;
	UINT8 EntryPointRevision;
	UINT8 FormattedArea[5];
	UINT8 IntermediateAnchorString[5];
	UINT8 IntermediateChecksum;
	UINT16 TableLength;
	UINT32 TableAddress;
	UINT16 NumberOfStructures;
	UINT8 SmbiosBcdRevision;
} SMBIOS_TABLE_ENTRY;

#define PROCESS_QUERY_INFORMATION 0x0400

extern "C"
{
	NTSTATUS ZwQuerySystemInformation(KERNEL_INFORMATION_CLASS infoClass, PVOID infoBuffer, ULONG bufferLength, PULONG returnedLength);
	NTSTATUS ObReferenceObjectByName(PUNICODE_STRING objName, ULONG attributes, PACCESS_STATE accessState, ACCESS_MASK desiredAccess, POBJECT_TYPE objType, KPROCESSOR_MODE accessMode, PVOID parseContext, PVOID* object);
	NTSTATUS ZwOpenProcess(PHANDLE processHandle, ACCESS_MASK desiredAccess, POBJECT_ATTRIBUTES objectAttributes, PCLIENT_ID clientId);
	NTSTATUS ZwQueryInformationProcess(HANDLE processHandle, PROCESSINFOCLASS processInformationClass, PVOID processInformation, ULONG processInformationLength, PULONG returnLength);
}

extern "C" POBJECT_TYPE* IoDriverObjectType;
