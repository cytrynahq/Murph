#pragma once
#include <ntifs.h>

namespace KernelUtilities
{
	PVOID ResolveModuleBase(const char* moduleName);
	bool MatchBytePattern(const char* data, const char* pattern, const char* mask);
	PVOID ScanBuffer(PVOID bufferStart, int maxLength, const char* pattern, const char* mask);
	PVOID ScanImageSections(PVOID moduleBase, const char* pattern, const char* mask);
	void GenerateRandomString(char* outputBuffer, int length);
}
