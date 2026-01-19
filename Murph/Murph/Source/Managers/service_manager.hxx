#pragma once
#include <ntifs.h>

namespace ServiceManager
{
	NTSTATUS RestartWmiService();
	NTSTATUS RestartServiceByName(const wchar_t* serviceName);
	NTSTATUS StopServiceByName(const wchar_t* serviceName);
	NTSTATUS StartServiceByName(const wchar_t* serviceName);
}
