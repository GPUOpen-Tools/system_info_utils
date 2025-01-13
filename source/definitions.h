//=============================================================================
// Copyright (c) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief JSON key definitions
//=============================================================================

#ifndef SYSTEM_INFO_UTILS_SOURCE_DEFINITIONS_H_
#define SYSTEM_INFO_UTILS_SOURCE_DEFINITIONS_H_

#if defined(__clang__)
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
#define SYSTEM_INFO_THROW(exception) throw exception
#define SYSTEM_INFO_TRY try
#define SYSTEM_INFO_CATCH(exception) catch (exception)
#define SYSTEM_INFO_UNUSED(x) (void)(x)
#else
#include <cstdlib>
#define SYSTEM_INFO_THROW(exception) std::abort()
#define SYSTEM_INFO_TRY if (true)
#define SYSTEM_INFO_CATCH(exception) if (false)
#define SYSTEM_INFO_UNUSED(x)
#endif
#else
#if defined(_MSC_VER)
#include <vcruntime.h>  // for _HAS_EXCEPTIONS
#endif
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || (_HAS_EXCEPTIONS)
#define SYSTEM_INFO_THROW(exception) throw exception
#define SYSTEM_INFO_TRY try
#define SYSTEM_INFO_CATCH(exception) catch (exception)
#define SYSTEM_INFO_UNUSED(x) (void)(x)
#else
#include <cstdlib>
#define SYSTEM_INFO_THROW(exception) std::abort()
#define SYSTEM_INFO_TRY if (true)
#define SYSTEM_INFO_CATCH(exception) if (false)
#define SYSTEM_INFO_UNUSED(x)
#endif
#endif

static constexpr const char* kSystemInfoChunkIdentifier = "SystemInfo";
static constexpr uint32_t    kSystemInfoChunkVersion    = 1;                        ///< Current system info chunk version
static constexpr uint32_t    kSystemInfoChunkVersionMax = kSystemInfoChunkVersion;  ///< Maximum supported chunk version

static constexpr const char* kNodeStringDriver                  = "driver";
static constexpr const char* kNodeStringSystem                  = "system";
static constexpr const char* kNodeStringName                    = "name";
static constexpr const char* kNodeStringDescription             = "description";
static constexpr const char* kNodeStringVersion                 = "version";
static constexpr const char* kNodeStringDriverPackagingVersion  = "packagingVersion";
static constexpr const char* kNodeStringDriverSoftwareVersion   = "softwareVersion";
static constexpr const char* kNodeStringOs                      = "os";
static constexpr const char* kNodeStringVirtualization          = "virtualization";
static constexpr const char* kNodeStringType                    = "type";
static constexpr const char* kNodeStringHostName                = "hostname";
static constexpr const char* kNodeStringMemory                  = "memory";
static constexpr const char* kNodeStringMemoryPhysical          = "physical";
static constexpr const char* kNodeStringMemorySwap              = "swap";
static constexpr const char* kNodeStringCpus                    = "cpus";
static constexpr const char* kNodeStringProcesses               = "processes";
static constexpr const char* kNodeStringProcessId               = "processId";
static constexpr const char* kNodeStringPath                    = "path";
static constexpr const char* kNodeStringArchitecture            = "architecture";
static constexpr const char* kNodeStringCpuVendorId             = "vendorId";
static constexpr const char* kNodeStringCpuTimeClockFreq        = "cpuTimeClockFreq";
static constexpr const char* kNodeStringCpuPhysicalCoreCount    = "numPhysicalCores";
static constexpr const char* kNodeStringCpuLogicalCoreCount     = "numLogicalCores";
static constexpr const char* kNodeStringSpeed                   = "speed";
static constexpr const char* kNodeStringCpuId                   = "cpuId";
static constexpr const char* kNodeStringCpuDeviceId             = "deviceId";
static constexpr const char* kNodeStringGpus                    = "gpus";
static constexpr const char* kNodeStringPci                     = "pci";
static constexpr const char* kNodeStringPciBus                  = "bus";
static constexpr const char* kNodeStringDevice                  = "device";
static constexpr const char* kNodeStringPciFunction             = "function";
static constexpr const char* kNodeStringAsic                    = "asic";
static constexpr const char* kNodeStringAsicGpuIndex            = "gpuIndex";
static constexpr const char* kNodeStringAsicGpuCounterFrequency = "gpuCounterFreq";
static constexpr const char* kNodeStringAsicNumSe               = "numShaderEngines";
static constexpr const char* kNodeStringAsicNumSaPerSe          = "numShaderArraysPerEngine";
static constexpr const char* kNodeStringAsicCuMask              = "cuMask";
static constexpr const char* kNodeStringAsicNumCus              = "numCus";
static constexpr const char* kNodeStringAsicEngineClockSpeed    = "engineClockHz";
static constexpr const char* kNodeStringMin                     = "min";
static constexpr const char* kNodeStringMax                     = "max";
static constexpr const char* kNodeStringAsicIds                 = "ids";
static constexpr const char* kNodeStringAsicGfxEngine           = "gfxEngine";
static constexpr const char* kNodeStringAsicFamily              = "family";
static constexpr const char* kNodeStringAsicERev                = "eRev";
static constexpr const char* kNodeStringAsicRevision            = "revision";
static constexpr const char* kNodeStringAsicSubsystem           = "subsystem";
static constexpr const char* kNodeStringAsicVendor              = "vendor";
static constexpr const char* kNodeStringAsicLuid                = "luid";
static constexpr const char* kNodeStringMemoryOpsPerClock       = "memOpsPerClock";
static constexpr const char* kNodeStringMemoryBusBitWidth       = "busBitWidth";
static constexpr const char* kNodeStringMemoryBandwith          = "bandwidthBytesPerSec";
static constexpr const char* kNodeStringMemoryClockSpeed        = "memClockHz";
static constexpr const char* kNodeStringHeaps                   = "heaps";
static constexpr const char* kNodeStringLocal                   = "local";
static constexpr const char* kNodeStringPhysicalAddress         = "physicalAddress";
static constexpr const char* kNodeStringSize                    = "size";
static constexpr const char* kNodeStringInvisible               = "invisible";
static constexpr const char* kNodeStringHbccSize                = "hbccSize";
static constexpr const char* kNodeStringExcludedVaRanges        = "excludedVaRanges";
static constexpr const char* kNodeStringBase                    = "base";
static constexpr const char* kNodeStringBigSw                   = "bigSw";
static constexpr const char* kNodeStringMajor                   = "major";
static constexpr const char* kNodeStringMinor                   = "minor";
static constexpr const char* kNodeStringPatch                   = "patch";
static constexpr const char* kNodeStringBuild                   = "build";
static constexpr const char* kNodeStringMisc                    = "misc";
static constexpr const char* kNodeStringConfig                  = "config";
static constexpr const char* kNodeStringDrm                     = "drm";
static constexpr const char* kNodeStringIsClosedSource          = "isClosedSource";
static constexpr const char* kNodeStringEtwSupport              = "etwSupport";
static constexpr const char* kNodeStringSupported               = "isSupported";
static constexpr const char* kNodeStringEtwRegistryOrUserGroup  = "needsRegistryOrUserGroup";
static constexpr const char* kNodeStringHasPermission           = "hasPermission";
static constexpr const char* kNodeStringStatusCode              = "statusCode";
static constexpr const char* kNodeStringPowerDpmWritable        = "powerDpmWritable";
static constexpr const char* kNodeStringDevDriver               = "devdriver";
static constexpr const char* kNodeStringTag                     = "tag";
static constexpr const char* kNodeStringLinux                   = "linux";
static constexpr const char* kNodeStringWindows                 = "windows";
#endif
