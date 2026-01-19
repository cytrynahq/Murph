# Murph - Windows Hardware Identifier Spoofing Driver


A sophisticated Windows kernel-mode driver designed to obfuscate hardware identifiers across multiple system components including CPU, GPU, storage, network adapters, and BIOS/SMBIOS firmware information.

## Project Overview

**Murph** is a kernel-mode device driver (.sys) that executes a comprehensive hardware identification spoofing routine at driver initialization. It modifies and randomizes hardware serial numbers and identifiers to prevent system tracking and identification mechanisms from establishing persistent device fingerprints.

The driver implements no permanent hooks or background monitoring. Instead, it performs targeted modifications to hardware identifiers upon load and optionally supports periodic re-spoofing of identifiers via a timer-based mechanism.

⚠️ **Important Notice**

This driver may be partially unstable or imperfect. It was one of my first kernel-mode driver projects and was recovered from an old storage device.  
As a result, some code paths may be outdated, unoptimized, or require refactoring before use in production or research environments.

## Core Architecture

### Module Structure

The project is organized into three main components:

#### 1. **Core Module** (`Source/Core/`)

- **kernel_interface.hxx**: Defines kernel-mode structures and callbacks for:
  - System information queries (KERNEL_INFORMATION_CLASS enumeration)
  - Module discovery (PKERNEL_MODULE_LIST structures)
  - Storage device configuration (STORAGE_DEVICE_IDENTIFIER, STORAGE_UNIT_CONFIGURATION)
  - SMBIOS table definitions and manipulation
  - Storage port registration callbacks
  - Disk S.M.A.R.T control routine signatures

- **main.cxx**: Driver entry point that:
  - Initializes debug logging with build timestamp
  - Executes storage spoofing immediately
  - Executes firmware identifier obfuscation immediately
  - Returns STATUS_SUCCESS upon completion

#### 2. **Managers Module** (`Source/Managers/`)

This module contains specialized managers for different hardware components:

##### **StorageManager** (storage_manager.hxx/.cxx)
Modifies disk drive identifiers through StorPort driver interface:
- Scans pattern-matched functions in `storport.sys` to locate register callback
- Iterates through device chains (typically RAID ports 0-1)
- Modifies serial number strings in STORAGE_DEVICE_IDENTIFIER structures
- Disables S.M.A.R.T health monitoring on all devices
- Allocates random string buffers that get registered with StorPort
- Processes both NVMe (port 1) and SATA (port 0) interfaces

##### **FirmwareConfiguration** (firmware_config.hxx/.cxx)
Obfuscates SMBIOS firmware tables:
- Scans and processes SMBIOS tables (0: BIOS, 1: System, 2: Baseboard, 3: Chassis)
- Extracts and randomizes string values maintaining original length
- Obfuscates UUID/GUID fields in SMBIOS entries
- Modifies system registry keys for persistence:
  - SystemBiosVersion
  - SystemManufacturer, SystemProductName, SystemSerialNumber
  - ACPI RSDP table references
- Generates realistic motherboard manufacturer names and model numbers

##### **CpuManager** (cpu_manager.hxx/.cxx)
Spoofs CPU processor identifiers:
- Selects random realistic CPU model strings (Intel Core i9/i7, AMD Ryzen 9/7)
- Modifies registry entries:
  - ProcessorNameString
  - VendorIdentifier (GenuineIntel or AuthenticAMD)
- Updates kernel registry at `HKLM\Hardware\Description\System\CentralProcessor\0`

##### **GpuManager** (gpu_manager.hxx/.cxx)
Randomizes graphics adapter identifiers:
- Applies realistic GPU model names (NVIDIA RTX, AMD Radeon, Intel Arc)
- Modifies registry paths:
  - `Video` class registry
  - `GraphicsDrivers` driver information
  - Display adapter class GUID registry
- Updates InstalledDisplayDrivers and DeviceDesc entries

##### **NetworkManager** (network_manager.hxx/.cxx)
Spoofs network adapter MAC addresses:
- Generates random MAC addresses with realistic vendor prefixes
- Iterates through network adapter subkeys (0-31)
- Sets NetworkAddress registry values
- Applies realistic network adapter descriptions (Intel, Broadcom, Qualcomm, Realtek, NVIDIA nForce)
- Formats MAC as XX-XX-XX-XX-XX-XX

##### **ProcessManager** (process_manager.hxx/.cxx)
Manages process enumeration and termination:
- FindProcessByName: Enumerates all processes, queries image filenames, performs case-insensitive comparison
- TerminateProcess: Terminates specified process via ZwTerminateProcess
- RestartExplorer: Terminates explorer.exe process
- DelayedWait: Kernel-mode sleep function using KeDelayExecutionThread

##### **ServiceManager** (service_manager.hxx/.cxx)
Manages system service state (kernel-mode):
- RestartWmiService: Terminates wmiprvse.exe (WMI provider process)
- RestartServiceByName: Selects appropriate action based on service name
- StopServiceByName, StartServiceByName: Request handlers with service recovery logic
- Note: Actual service control is limited in kernel-mode; relies on automatic Windows recovery

##### **ThreadManager** (thread_manager.hxx/.cxx)
Schedules delayed operations in worker threads:
- InitializeWorkerSystem: Prepares worker thread infrastructure
- ScheduleExplorerRestart: Creates worker thread that waits specified delay then restarts explorer.exe
- ScheduleWmiRestart: Creates worker thread that waits then restarts WMI
- Allocates DELAYED_OPERATION structures containing operation type and delay in milliseconds
- Uses PsCreateSystemThread for background execution

##### **PeriodicSpoofer** (periodic_spoofer.hxx/.cxx)
Implements timer-based periodic re-spoofing:
- Maintains global KTIMER and KDPC structures
- Executes one spoofer per cycle in round-robin fashion (disk → firmware → CPU → GPU)
- Default interval: 180 seconds per spoofer (3 minutes)
- SpoofTimerDpc: DPC routine invoked by timer, creates worker thread for actual spoofing
- ExecuteSpooferCycle: Manually triggers all spoofers in sequence
- GetSpooferCycleCount: Returns total number of spoof cycles completed

#### 3. **Utilities Module** (`Source/Utilities/`)

##### **Logger** (logger.hxx/.cxx)
Centralized kernel-mode debug logging:
- Output(): Logs messages with [DRIVER] prefix via vDbgPrintExWithPrefix
- Status(): Logs messages with [STATUS] prefix for status-level information
- Uses variadic arguments for formatted output

##### **KernelUtilities** (kernel_utilities.hxx/.cxx)
Utility functions for kernel-mode operations:
- ResolveModuleBase(): Queries kernel module list, finds base address by name (used for storport.sys, disk.sys)
- MatchBytePattern(): Matches byte sequences against pattern masks ('x' for literal, '?' for wildcard)
- ScanBuffer(): Linear byte pattern scanning in memory regions
- ScanImageSections(): Scans PE image sections (.text and PAGE) for patterns (used to locate StorPort callbacks and disk S.M.A.R.T functions)
- GenerateRandomString(): Produces random alphanumeric strings of specified length using RtlRandomEx seeded with KeQueryTimeIncrement()

## Operational Flow

### Initialization Phase (DriverEntry)

1. Driver loads and prints build date/time
2. Calls StorageManager::DisableSmartOnAllDevices()
   - Locates disk.sys module
   - Performs pattern scan to find S.M.A.R.T control function
   - Enumerates all disk driver managed devices
   - Disables S.M.A.R.T monitoring for each device
3. Calls StorageManager::ReplaceAllDiskIdentifiers()
   - Locates storport.sys module
   - Performs pattern scan to find StorPort register callback
   - Iterates RAID ports (0 and 1)
   - Modifies serial number strings for each disk device
4. Calls FirmwareConfiguration::ReplaceAllFirmwareSerials()
   - Scans and modifies SMBIOS firmware tables
   - Updates system registry for persistence
5. Returns STATUS_SUCCESS

### Spoofing Operations

Each operation follows a similar pattern:

1. **Pattern Scanning**: Uses byte-pattern matching with masks to locate kernel functions without hardcoded addresses
2. **Safe Memory Access**: Wrapped in __try/__except blocks for SEH error handling
3. **Registry Modification**: Uses ZwOpenKey/ZwSetValueKey for persistent changes
4. **Logging**: Detailed status messages at each step for debugging

## Technical Features

### Pattern-Based Function Resolution

Rather than relying on specific kernel versions or hardcoded addresses, the driver uses byte-pattern scanning to locate critical functions:
- Scans only .text and PAGE sections of target modules
- Supports wildcard patterns for instruction variations
- Located functions are immediately invoked via function pointers

### Memory Safety

- All kernel pool allocations tagged with 'KrnL' or operation-specific tags
- Proper reference counting and dereferencing of kernel objects
- Exception handling around device enumeration and manipulation
- Null pointer checks before all dereferenced accesses

### String Handling

- Kernel-mode safe string functions (RtlStringCbPrintfW, RtlAnsiStringToUnicodeString)
- Manual character set encoding for MAC addresses (no swprintf_s in kernel)
- Length preservation in obfuscated values (original length maintained after randomization)

### Registry Persistence

Changes persist across reboots through:
- Direct registry key modification via Zw* functions
- Modification of both kernel registry (immediate effect) and system registry (persistence)
- SMBIOS firmware tables modified in-memory

## Build Configuration

- **Project Type**: Windows Driver Kit (WDK) Visual Studio Project
- **Language**: C++17 with kernel-mode extensions
- **Target**: x64 Windows architecture
- **Output**: Murph.sys (kernel-mode device driver)
- **Dependencies**: ntifs.h, ntstrsafe.h, standard WDK libraries

## Security Considerations

This driver operates with unrestricted kernel-mode access and can:
- Modify system firmware identifiers
- Terminate system processes
- Directly manipulate hardware registrations
- Access protected kernel data structures

**Use with caution and only in controlled environments.**

## Files and Responsibilities

```
Murph/
├── Source/
│   ├── Core/
│   │   ├── kernel_interface.hxx        # Kernel structures and definitions
│   │   └── main.cxx                    # Driver entry point
│   ├── Managers/
│   │   ├── cpu_manager.*               # CPU identifier spoofing
│   │   ├── gpu_manager.*               # GPU identifier spoofing
│   │   ├── storage_manager.*           # Disk serial modification
│   │   ├── firmware_config.*           # SMBIOS firmware modification
│   │   ├── network_manager.*           # Network adapter MAC spoofing
│   │   ├── process_manager.*           # Process enumeration/termination
│   │   ├── service_manager.*           # Service management
│   │   ├── thread_manager.*            # Delayed operation scheduling
│   │   └── periodic_spoofer.*          # Timer-based periodic spoofing
│   └── Utilities/
│       ├── logger.*                    # Debug logging facility
│       └── kernel_utilities.*          # Memory operations, pattern scanning
├── Murph.vcxproj                       # Visual Studio project file
└── Murph.vcxproj.user                  # User-specific project settings
```

## Testing and Debugging

Debug output is available via kernel debugger or DebugView:
- [DRIVER] messages for general driver operations
- [STATUS] messages for status-level information including spoofing results
- Detailed parameter and serial number changes logged

## Dependencies and Requirements

- Windows WDK 10 or later
- Visual Studio 2019 or later with WDK integration
- Target OS: Windows 10/11 x64
- Administrator/Kernel privileges required for driver installation

## Disclaimer

This software is provided for educational and authorized use only. The driver modifies system hardware identifiers which may violate terms of service of certain applications or platforms. Users are responsible for compliance with applicable laws and terms of service.

---

**Version**: 1.0  
**Language**: C++17 (Kernel-Mode)  
**Architecture**: x64 Windows
