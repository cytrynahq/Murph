#include <ntifs.h>
#include <windef.h>
#include <ntimage.h>
#include "../Core/kernel_interface.hxx"
#include "kernel_utilities.hxx"

PVOID KernelUtilities::ResolveModuleBase(const char* moduleName)
{
	ULONG requiredSize = 0;
	NTSTATUS status = ZwQuerySystemInformation(ModuleInfo, &requiredSize, 0, &requiredSize);
	
	if (status != STATUS_INFO_LENGTH_MISMATCH)
		return nullptr;

	PKERNEL_MODULE_LIST moduleTable = static_cast<PKERNEL_MODULE_LIST>(
		ExAllocatePoolWithTag(NonPagedPool, requiredSize, KERNEL_POOL_SIGNATURE)
	);
	
	if (!moduleTable)
		return nullptr;

	status = ZwQuerySystemInformation(ModuleInfo, moduleTable, requiredSize, nullptr);
	
	PVOID foundBase = nullptr;
	
	if (NT_SUCCESS(status))
	{
		for (ULONG_PTR idx = 0; idx < moduleTable->ModuleCount; idx++)
		{
			if (strstr(moduleTable->Modules[idx].ImageName, moduleName))
			{
				foundBase = moduleTable->Modules[idx].Base;
				break;
			}
		}
	}

	ExFreePool(moduleTable);
	return foundBase;
}

bool KernelUtilities::MatchBytePattern(const char* data, const char* pattern, const char* mask)
{
	for (; *mask; ++data, ++pattern, ++mask)
	{
		if (*mask == 'x' && *data != *pattern)
			return false;
	}
	return true;
}

PVOID KernelUtilities::ScanBuffer(PVOID bufferStart, int maxLength, const char* pattern, const char* mask)
{
	int patternLength = static_cast<int>(strlen(mask));
	int scanLimit = maxLength - patternLength;
	
	for (int offset = 0; offset <= scanLimit; offset++)
	{
		const char* currentPosition = static_cast<char*>(bufferStart) + offset;
		if (MatchBytePattern(currentPosition, pattern, mask))
			return const_cast<char*>(currentPosition);
	}
	
	return nullptr;
}

PVOID KernelUtilities::ScanImageSections(PVOID moduleBase, const char* pattern, const char* mask)
{
	PVOID matchAddress = nullptr;
	
	PIMAGE_DOS_HEADER dosHeader = static_cast<PIMAGE_DOS_HEADER>(moduleBase);
	PIMAGE_NT_HEADERS ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(
		static_cast<char*>(moduleBase) + dosHeader->e_lfanew
	);
	
	PIMAGE_SECTION_HEADER sections = IMAGE_FIRST_SECTION(ntHeaders);
	
	for (USHORT sectionIdx = 0; sectionIdx < ntHeaders->FileHeader.NumberOfSections; sectionIdx++)
	{
		PIMAGE_SECTION_HEADER currentSection = &sections[sectionIdx];
		
		// Search in .text and PAGE sections
		if (memcmp(currentSection->Name, ".text", 5) == 0 || memcmp(currentSection->Name, "PAGE", 4) == 0)
		{
			PVOID sectionAddress = static_cast<char*>(moduleBase) + currentSection->VirtualAddress;
			matchAddress = ScanBuffer(sectionAddress, currentSection->Misc.VirtualSize, pattern, mask);
			
			if (matchAddress)
				break;
		}
	}
	
	return matchAddress;
}

void KernelUtilities::GenerateRandomString(char* outputBuffer, int length)
{
	if (!outputBuffer)
		return;

	static const char characterSet[] = 
		"0123456789"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz";

	ULONG randomSeed = KeQueryTimeIncrement();

	for (int idx = 0; idx < length; idx++)
	{
		ULONG randomValue = RtlRandomEx(&randomSeed);
		int charIndex = randomValue % (sizeof(characterSet) - 1);
		outputBuffer[idx] = characterSet[charIndex];
	}
	
	outputBuffer[length] = '\0';
}
