#pragma once
#include <ntifs.h>

namespace Logger
{
	void Output(const char* format, ...);
	void Status(const char* format, ...);
}
