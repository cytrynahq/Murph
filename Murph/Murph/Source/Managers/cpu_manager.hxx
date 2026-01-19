#pragma once

namespace CpuManager
{
	NTSTATUS SpoofCpuIdentifiers();
	NTSTATUS ModifyCpuRegistry();
	NTSTATUS SpoofCpuidData();
}
