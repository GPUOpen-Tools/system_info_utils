//=============================================================================
// Copyright (c) 2022-2024 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief System info reader definition
//=============================================================================

#ifndef SYSTEM_INFO_UTILS_SOURCE_SYSTEM_INFO_READER_H_
#define SYSTEM_INFO_UTILS_SOURCE_SYSTEM_INFO_READER_H_

#include <cstdint>
#include <string>
#include <vector>

#ifdef SYSTEM_INFO_ENABLE_RDF
#include <amdrdf.h>
#endif

namespace system_info_utils
{

    struct Version
    {
        uint32_t major;  ///< major version
        uint32_t minor;  ///< minor version
        uint32_t patch;  ///< patch version
        uint32_t build;  ///< build number
    };

    /// @brief A structure containing DevDriver version info.
    struct DevDriverInfo
    {
        uint32_t    major_version;  ///< The interface major version.
        std::string tag;            ///< The release tag name string.
    };

    /// @brief Structure containing the system memory info.
    struct OsMemoryInfo
    {
        uint64_t    physical;  ///< The total physical memory size in bytes.
        uint64_t    swap;      ///< The total swap memory size in bytes.
        std::string type;      ///< The memory type name
    };

    /// @brief Structure containing the Event Tracing for Windows information.
    struct EtwSupportInfo
    {
        bool     is_supported;                     ///< The flag indicating whether ETW is supported.
        bool     has_permission;                   ///< The flag indicating whether the account has permission to open an ETW session.
        uint32_t status_code;                      ///< The ETW status code received when attempting to open a session.
        bool     needs_rgp_registry_or_usergroup;  ///< The flag indicating registry or usergroup settings for RGP ETW capture needs setting.
    };

    /// @brief Structure containing the system's configuration info.
    struct ConfigInfo
    {
        bool           power_dpm_writable;  ///< Flag if power management file is writable on Linux.
        uint32_t       drm_major_version;   ///< libdrm major version.
        uint32_t       drm_minor_version;   ///< libdrm minor version.
        EtwSupportInfo etw_support_info;    ///< A field containing the Event Tracing for Windows info.
    };

    /// @brief Structure containing the operating system info.
    struct OsInfo
    {
        std::string  name;      ///< The OS name string.
        std::string  desc;      ///< The OS description string.
        std::string  hostname;  ///< The system hostname string.
        OsMemoryInfo memory;    ///< The system memory info structure.
        ConfigInfo   config;    ///< The OS configuration info.
    };

    /// @brief Structure containing CPU info for the system.
    struct CpuInfo
    {
        std::string name;                       ///< The CPU name ("AMD Ryzen 7 2700X Eight-Core Processor")
        std::string cpu_id;                     ///< The CPU identifier ("AM64 Family 23 Model 8 Stepping 2" etc.)
        std::string device_id;                  ///< The CPU slot identifier ("CPUO", "CPU1", etc)
        std::string architecture;               ///< The CPU architecture
        std::string vendor_id;                  ///< "AuthenticAMD" etc..
        std::string virtualization;             ///< The CPU has virtualization firmware enabled state
        uint32_t    num_physical_cores;         ///< The CPU physical core count
        uint32_t    num_logical_cores;          ///< The CPU logical core count
        uint32_t    max_clock_speed;            ///< The maximum CPU clock speed in MHz
        uint64_t    timestamp_clock_frequency;  ///< The CPU timestamp clock frequency in Hz
    };

    /// @brief Structure containing a single GPU's PCI connection info.
    struct PciInfo
    {
        uint32_t bus;       ///< The device bus number.
        uint32_t device;    ///< The device number.
        uint32_t function;  ///< The device function number.
    };

    /// @brief Structure containing clock timestamp info.
    struct ClockInfo
    {
        uint64_t min;  ///< The minimum clock value in Hz.
        uint64_t max;  ///< The maximum clock value in Hz.
    };

    /// @brief Structure containing ASIC identification info.
    struct IdInfo
    {
        uint32_t gfx_engine;  ///< The graphics engine id.
        uint32_t family;      ///< The hardware family ID.
        uint32_t e_rev;       ///< The hardware revision id.
        uint32_t revision;    ///< The PCI revision ID.
        uint32_t device;      ///< The PCI device ID.
        uint32_t subsystem;   ///< The PCI ID or ACPI ID of the adapter's hardware subsystem.
        uint32_t vendor;      ///< The PCI ID or ACPI ID of the adapter's hardware vendor.
        uint8_t  luid[8];     ///< The locally unique identifier for the adapter.
    };

    /// @brief Structure containing physical hardware identification info.
    struct AsicInfo
    {
        uint32_t  gpu_index;         ///< The index of the GPU as enumerated by the system.
        uint64_t  gpu_counter_freq;  ///< The GPU counter frequency in ticks.
        ClockInfo engine_clock_hz;   ///< The GPU engine clock info in Hz.

        uint32_t num_shader_engines;            ///< The number of shader engines on the GPU.
        uint32_t num_shader_arrays_per_engine;  ///< The number of shader arrays per shader engine on the GPU.

        /// @brief The mask that describes the active CUs on the GPU.
        ///
        /// The mask is organized by shader engine index first and then the shader array within the engine.
        std::vector<std::vector<uint32_t>> cu_mask;
        uint32_t                           num_cus;  ///< The number of compute units on the GPU.

        IdInfo id_info;  ///< The hardware info, used to uniquely identify a GPU in the system.
    };

    /// @brief Structure containing GPU memory heap identification info.
    struct HeapInfo
    {
        std::string heap_type;  ///< A string indicating the heap type (typically Local or Invisible).
        uint64_t    phys_addr;  ///< The physical heap location as a byte offset.
        uint64_t    size;       ///< The physical heap size in bytes.
    };

    /// @brief Structure containing memory info for an excluded region.
    struct ExcludedRangeInfo
    {
        uint64_t base;  ///< The base address identifying the beginning of an excluded memory region.
        uint64_t size;  ///< The total size in bytes of the excluded memory region.
    };

    /// @brief Structure containing memory info and statistics for the device.
    struct MemoryInfo
    {
        std::string                    type;                ///< A string indicating the type of GPU memory.
        uint32_t                       mem_ops_per_clock;   ///< The total count of memory operations per clock.
        uint32_t                       bus_bit_width;       ///< The total width of the memory bus in bits.
        uint64_t                       bandwidth;           ///< The total computed bandwidth of the memory bus in bytes/second.
        ClockInfo                      mem_clock_hz;        ///< The device memory clock range info in Hz.
        std::vector<HeapInfo>          heaps;               ///< A vector of available memory heaps.
        std::vector<ExcludedRangeInfo> excluded_va_ranges;  ///< A vector of excluded virtual address ranges.
    };

    /// @brief Structure containing software version numbers.
    struct SoftwareVersion
    {
        uint32_t major;  ///< The major version number.
        uint32_t minor;  ///< The minor version number.
        uint32_t misc;   ///< The subminor/misc/patch version number.
    };

    /// @brief Structure containing fields used to identify a GPU device connected to the system.
    struct GpuInfo
    {
        std::string     name;    ///< The GPU identification name string.
        PciInfo         pci;     ///< The GPU PCI connection info.
        AsicInfo        asic;    ///< The hardware's ASIC info.
        MemoryInfo      memory;  ///< The hardware's memory info.
        SoftwareVersion big_sw;  ///< The 'Big Software' release version number info.
    };

    /// @brief Structure defining a system process
    struct Process
    {
        std::string name;  ///< Process name
        std::string path;  ///< Process filepath
        uint32_t    id;    ///< Process ID
    };

    /// @brief Structure containing the driver software info.
    struct DriverInfo
    {
        uint32_t    packaging_version_major;  ///< The driver packaging major version
        uint32_t    packaging_version_minor;  ///< The driver packaging minor version
        std::string name;                     ///< The driver name
        std::string description;              ///< The driver description
        std::string packaging_version;        ///< The driver packaging version string.
        std::string software_version;         ///< The driver software version string. (Windows platform specific)
        bool        is_closed_source;         ///< Flag if driver is PRO (closed source)
    };

    /// @brief Structure containing fields used to identify the target system's hardware and software.
    struct SystemInfo
    {
        Version              version;    ///< A version number to identify the System Info structure revision number.
        DriverInfo           driver;     ///< A field containing GPU device driver info.
        DevDriverInfo        devdriver;  ///< A field containing Developer Driver info.
        OsInfo               os;         ///< A field containing the system's OS info.
        std::vector<CpuInfo> cpus;       ///< A vector of all CPU devices identified in the system.
        std::vector<GpuInfo> gpus;       ///< A vector of all GPU devices identified in the system.
        std::vector<Process> processes;  ///< A vector of running processes identified in the system.
    };

    /// @brief Parses system info JSON representation
    class SystemInfoReader
    {
    public:
        /// @brief Default constructor
        SystemInfoReader() = delete;

        /// @brief Default destructor
        ~SystemInfoReader() = delete;

        /// @brief Parses system info JSON representation
        /// @param [in] json The system info JSON
        /// @param [in, out] system_info The parsed JSON represented by system info structure
        /// @return true if successfully parsed, false otherwise
        static bool Parse(const std::string& json, SystemInfo& system_info);

        /// @brief Parses system info JSON representation
        /// @param [in] json The system info JSON
        /// @return system info JSON structure text
        static std::string Parse(const std::string& json);

#ifdef SYSTEM_INFO_ENABLE_RDF
#ifdef RDF_CXX_BINDINGS
        /// @brief Parses system info chunk from RDF file
        /// @param [in] file The RDF file
        /// @param [in, out] system_info The system info structure
        /// @return true on successful parse, false otherwise
        static bool Parse(rdf::ChunkFile& file, SystemInfo& system_info);
#endif
        /// @brief Parses system info chunk from RDF file
        /// @param [in] file The RDF file
        /// @param [in, out] system_info The system info structure
        /// @return true on successful parse, false otherwise
        static bool Parse(rdfChunkFile* file, SystemInfo& system_info);
#endif
    };
}  // namespace system_info_utils

#endif