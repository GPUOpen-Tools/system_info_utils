//=============================================================================
// Copyright (c) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief System info reader implementation
//=============================================================================

#include "system_info_reader.h"

#include <cstdint>
#include <cstring>
#include <sstream>
#include <type_traits>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "definitions.h"

namespace
{
    /// @brief Gets the property from the JSON object with a default value if that key is not present.
    /// @tparam [in] T The type to convert the specified JSON property into.
    /// @param [in] parent The object to get the property from.
    /// @param [in] name The name of the property to get from the parent object.
    /// @param [in] fallback The default value to use if the value is not present.
    /// @return The value from the JSON object or the default value.
    template <typename T>
    T Get(const rapidjson::Value& parent, const char* name, const T& fallback)
    {
        if (!parent.IsObject() || !parent.HasMember(name))
        {
            return fallback;
        }

        const auto& val = parent[name];

        if constexpr (std::is_same_v<T, std::string>)
        {
            return val.IsString() ? std::string(val.GetString()) : fallback;
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            return val.IsBool() ? val.GetBool() : fallback;
        }
        else if constexpr (std::is_same_v<T, uint64_t>)
        {
            return val.IsUint64() ? val.GetUint64() : fallback;
        }
        else if constexpr (std::is_same_v<T, uint32_t>)
        {
            return val.IsUint() ? val.GetUint() : fallback;
        }
        else if constexpr (std::is_unsigned_v<T>)
        {
            if (val.IsUint64())
                return static_cast<T>(val.GetUint64());
            if (val.IsUint())
                return static_cast<T>(val.GetUint());
            return fallback;
        }
        else
        {
            return fallback;
        }
    }

    /// @brief Check if a node with the given name exists as a child of the provided parent node.
    ///
    /// @param [in] parent The parent JSON node whose children will be searched.
    /// @param [in] name The name of the child node to search for.
    /// @return True when a child JSON node with the given name exists, and false if it doesn't.
    bool DoesNodeExist(const rapidjson::Value& parent, const char* name)
    {
        return parent.IsObject() && parent.HasMember(name);
    }

    /// @brief Serialize a RapidJSON value to a JSON string.
    /// @param [in] value The JSON value to serialize.
    /// @return The JSON string representation.
    std::string DumpJson(const rapidjson::Value& value)
    {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        value.Accept(writer);
        return std::string(buffer.GetString(), buffer.GetSize());
    }

}  // namespace

namespace system_info_utils
{
    /// @brief The V1 implementation handles parsing Version 1 of the system info JSON.
    class SystemInfoParserV1
    {
    public:
        /// @brief Constructor.
        SystemInfoParserV1() = default;

        /// @brief Destructor.
        virtual ~SystemInfoParserV1() = default;

        /// @brief Process the System Info value JSON node.
        ///
        /// @param [in] system_value_node The parent JSON node containing system info fields.
        /// @param [in, out] system_info The structure to be populated with the parsed system info members.
        virtual void ProcessSystemValueNode(const rapidjson::Value& system_value_node, SystemInfo& system_info)
        {
            if (DoesNodeExist(system_value_node, kNodeStringDevDriver))
            {
                ProcessDevDriverNode(system_value_node[kNodeStringDevDriver], system_info.devdriver);
            }

            if (DoesNodeExist(system_value_node, kNodeStringDriver))
            {
                ProcessDriverNode(system_value_node[kNodeStringDriver], system_info.driver);
            }

            if (DoesNodeExist(system_value_node, kNodeStringDrivers))
            {
                ProcessDriversNode(system_value_node[kNodeStringDrivers], system_info.drivers);
            }

            if (DoesNodeExist(system_value_node, kNodeStringOs))
            {
                ProcessOsNode(system_value_node[kNodeStringOs], system_info.os);
            }

            if (DoesNodeExist(system_value_node, kNodeStringCpus))
            {
                ProcessCpusNode(system_value_node[kNodeStringCpus], system_info.cpus);
            }

            if (DoesNodeExist(system_value_node, kNodeStringGpus))
            {
                ProcessGpusNodes(system_value_node[kNodeStringGpus], system_info.gpus);
            }
        }

    protected:
        /// @brief Process the DevDriver info JSON node.
        ///
        /// @param [in] dev_driver_root The parent JSON node for the DevDriver section.
        /// @param [in, out] dev_driver_info The structure to be populated with the parsed DevDriver info.
        virtual void ProcessDevDriverNode(const rapidjson::Value& dev_driver_root, DevDriverInfo& dev_driver_info)
        {
            if (DoesNodeExist(dev_driver_root, kNodeStringVersion))
            {
                const auto& version_root = dev_driver_root[kNodeStringVersion];

                dev_driver_info.major_version = Get<uint32_t>(version_root, kNodeStringMajor, 0);
            }
            dev_driver_info.tag = Get<std::string>(dev_driver_root, kNodeStringTag, "");
        }

        /// @brief Process the OS Memory info JSON node.
        ///
        /// @param [in] memory_root The parent JSON node for the memory section.
        /// @param [in, out] memory_info The structure to be populated with the parsed system memory info.
        virtual void ProcessOsMemoryNode(const rapidjson::Value& memory_root, OsMemoryInfo& memory_info)
        {
            memory_info.physical = Get<uint64_t>(memory_root, kNodeStringMemoryPhysical, 0);
            memory_info.swap     = Get<uint64_t>(memory_root, kNodeStringMemorySwap, 0);
            memory_info.type     = Get<std::string>(memory_root, kNodeStringName, "");
        }

        /// @brief Process the Event Tracing for Windows info JSON node.
        ///
        /// @param [in] etw_root The parent JSON node for the ETW Info section.
        /// @param [in, out] etw_info The structure to be populated with the parsed ETW info.
        virtual void ProcessEtwNode(const rapidjson::Value& etw_root, EtwSupportInfo& etw_info)
        {
            etw_info.is_supported                    = Get<bool>(etw_root, kNodeStringSupported, false);
            etw_info.has_permission                  = Get<bool>(etw_root, kNodeStringHasPermission, false);
            etw_info.status_code                     = Get<uint32_t>(etw_root, kNodeStringStatusCode, 0);
            etw_info.needs_rgp_registry_or_usergroup = Get<bool>(etw_root, kNodeStringEtwRegistryOrUserGroup, false);
        }

        /// @brief Process the Operating System info JSON node.
        ///
        /// @param [in] os_root The parent JSON node for the operating system section.
        /// @param [in, out] os_info The structure to be populated with the parsed operating system info.
        virtual void ProcessOsNode(const rapidjson::Value& os_root, OsInfo& os_info)
        {
            os_info.name     = Get<std::string>(os_root, kNodeStringName, "");
            os_info.desc     = Get<std::string>(os_root, kNodeStringDescription, "");
            os_info.hostname = Get<std::string>(os_root, kNodeStringHostName, "");

            if (DoesNodeExist(os_root, kNodeStringMemory))
            {
                ProcessOsMemoryNode(os_root[kNodeStringMemory], os_info.memory);
            }

            if (DoesNodeExist(os_root, kNodeStringConfig))
            {
                const auto& config_root = os_root[kNodeStringConfig];

                if (DoesNodeExist(config_root, kNodeStringLinux))
                {
                    const auto& linux_root = config_root[kNodeStringLinux];

                    os_info.config.power_dpm_writable = Get<bool>(linux_root, kNodeStringPowerDpmWritable, false);

                    if (DoesNodeExist(linux_root, kNodeStringDrm))
                    {
                        const auto& drm_root             = linux_root[kNodeStringDrm];
                        os_info.config.drm_major_version = Get<uint32_t>(drm_root, kNodeStringMajor, 0);
                        os_info.config.drm_minor_version = Get<uint32_t>(drm_root, kNodeStringMinor, 0);
                    }
                }

                if (DoesNodeExist(config_root, kNodeStringWindows))
                {
                    const auto& windows_root = config_root[kNodeStringWindows];
                    if (DoesNodeExist(windows_root, kNodeStringEtwSupport))
                    {
                        ProcessEtwNode(windows_root[kNodeStringEtwSupport], os_info.config.etw_support_info);
                    }
                }
            }
        }

        /// @brief Process the CPU info JSON node.
        ///
        /// @param [in] cpu_root The parent JSON node for the CPU section.
        /// @param [in, out] cpu_info The structure to be populated with the parsed CPU info.
        virtual void ProcessCpusNode(const rapidjson::Value& cpus_root, std::vector<CpuInfo>& cpus)
        {
            if (!cpus_root.IsArray())
            {
                return;
            }

            for (rapidjson::SizeType i = 0; i < cpus_root.Size(); ++i)
            {
                const rapidjson::Value& cpu_node = cpus_root[i];

                CpuInfo cpu = {};

                cpu.name               = Get<std::string>(cpu_node, kNodeStringName, "");
                cpu.architecture       = Get<std::string>(cpu_node, kNodeStringArchitecture, "");
                cpu.cpu_id             = Get<std::string>(cpu_node, kNodeStringCpuId, "");
                cpu.device_id          = Get<std::string>(cpu_node, kNodeStringCpuDeviceId, "");
                cpu.vendor_id          = Get<std::string>(cpu_node, kNodeStringCpuVendorId, "");
                cpu.num_logical_cores  = Get<uint32_t>(cpu_node, kNodeStringCpuLogicalCoreCount, 0);
                cpu.num_physical_cores = Get<uint32_t>(cpu_node, kNodeStringCpuPhysicalCoreCount, 0);

                if (DoesNodeExist(cpu_node, kNodeStringVirtualization))
                {
                    cpu.virtualization = Get<std::string>(cpu_node, kNodeStringVirtualization, "");
                }

                if (DoesNodeExist(cpu_node, kNodeStringSpeed))
                {
                    const auto& cpu_speed_node = cpu_node[kNodeStringSpeed];
                    cpu.max_clock_speed        = Get<uint32_t>(cpu_speed_node, kNodeStringMax, 0);
                }

                cpu.timestamp_clock_frequency = Get<uint64_t>(cpu_node, kNodeStringCpuTimeClockFreq, 0);

                cpus.push_back(cpu);
            }
        }

        /// @brief Process the GPU PCI info JSON node.
        ///
        /// @param [in] pci_root The parent JSON node for the PCI info section.
        /// @param [in, out] pci_info The structure to be populated with the parsed PCI info.
        virtual void ProcessGpuPciNode(const rapidjson::Value& pci_root, PciInfo& pci_info)
        {
            pci_info.id       = 0;
            pci_info.bus      = Get<uint32_t>(pci_root, kNodeStringPciBus, 0);
            pci_info.device   = Get<uint32_t>(pci_root, kNodeStringDevice, 0);
            pci_info.function = Get<uint32_t>(pci_root, kNodeStringPciFunction, 0);
        }

        /// @brief Processes the CU mask list.
        /// @param [in] cu_mask_root The json node that has the CU mask.
        /// @param [in, out] asicInfo The structure to be populated with the parsed CU mask.
        virtual void ProcessCuMaskNode(const rapidjson::Value& cu_mask_root, AsicInfo& asicInfo)
        {
            if (!cu_mask_root.IsArray())
            {
                return;
            }

            for (rapidjson::SizeType i = 0; i < cu_mask_root.Size(); ++i)
            {
                const auto& shader_array_list = cu_mask_root[i];

                if (!shader_array_list.IsArray())
                {
                    asicInfo.cu_mask.clear();
                    return;
                }

                auto& shader_array_masks = asicInfo.cu_mask.emplace_back();
                for (rapidjson::SizeType j = 0; j < shader_array_list.Size(); ++j)
                {
                    const auto& shader_array_mask_item = shader_array_list[j];

                    if (!shader_array_mask_item.IsUint())
                    {
                        asicInfo.cu_mask.clear();
                        return;
                    }

                    shader_array_masks.push_back(shader_array_mask_item.GetUint());
                }
            }
        }

        /// @brief Process the clock frequency info JSON node.
        ///
        /// @param [in] clock_hz_root The parent JSON node containing the clock Hz info.
        /// @param [in, out] clock_info The structure to be populated with the parsed clock frequency info.
        virtual void ProcessClockInfoNode(const rapidjson::Value& clock_hz_root, ClockInfo& clock_info)
        {
            clock_info.min = Get<uint64_t>(clock_hz_root, kNodeStringMin, 0);
            clock_info.max = Get<uint64_t>(clock_hz_root, kNodeStringMax, 0);
        }

        /// @brief Process the ASIC Id info JSON node.
        ///
        /// @param [in] asic_id_info_root The parent JSON node containing the ASIC Id info.
        /// @param [in, out] asic_id_info The structure to be populated with the parsed ASIC Id info.
        virtual void ProcessAsicIdInfoNode(const rapidjson::Value& asic_id_info_root, IdInfo& asic_id_info)
        {
            asic_id_info.gfx_engine = Get<uint32_t>(asic_id_info_root, kNodeStringAsicGfxEngine, 0);
            asic_id_info.family     = Get<uint32_t>(asic_id_info_root, kNodeStringAsicFamily, 0);
            asic_id_info.e_rev      = Get<uint32_t>(asic_id_info_root, kNodeStringAsicERev, 0);
            asic_id_info.revision   = Get<uint32_t>(asic_id_info_root, kNodeStringAsicRevision, 0);
            asic_id_info.device     = Get<uint32_t>(asic_id_info_root, kNodeStringDevice, 0);
            asic_id_info.subsystem  = Get<uint32_t>(asic_id_info_root, kNodeStringAsicSubsystem, 0);
            asic_id_info.vendor     = Get<uint32_t>(asic_id_info_root, kNodeStringAsicVendor, 0);

            const std::string luid_str = Get<std::string>(asic_id_info_root, kNodeStringAsicLuid, "");
            memset(asic_id_info.luid, 0, sizeof(asic_id_info.luid));

            for (size_t i = 0; i < luid_str.length(); i += 2)
            {
                const std::string byte_str = luid_str.substr(i, 2);
                asic_id_info.luid[i / 2]   = static_cast<uint8_t>(strtol(byte_str.c_str(), nullptr, 16));
            }
        }

        /// @brief Process an individual GPU's ASIC info JSON node.
        ///
        /// @param [in] asic_root The parent JSON node containing the GPU's ASIC info.
        /// @param [in, out] asic_info The structure to be populated with the parsed ASIC info.
        virtual void ProcessGpuAsicNode(const rapidjson::Value& asic_root, AsicInfo& asic_info)
        {
            asic_info.gpu_index        = Get<uint32_t>(asic_root, kNodeStringAsicGpuIndex, static_cast<uint32_t>(-1));
            asic_info.gpu_counter_freq = Get<uint32_t>(asic_root, kNodeStringAsicGpuCounterFrequency, 0);

            asic_info.num_shader_engines           = Get<uint32_t>(asic_root, kNodeStringAsicNumSe, 0);
            asic_info.num_shader_arrays_per_engine = Get<uint32_t>(asic_root, kNodeStringAsicNumSaPerSe, 0);
            asic_info.num_cus                      = Get<uint32_t>(asic_root, kNodeStringAsicNumCus, 0);

            if (DoesNodeExist(asic_root, kNodeStringAsicCuMask))
            {
                ProcessCuMaskNode(asic_root[kNodeStringAsicCuMask], asic_info);
            }

            if (DoesNodeExist(asic_root, kNodeStringAsicEngineClockSpeed))
            {
                ProcessClockInfoNode(asic_root[kNodeStringAsicEngineClockSpeed], asic_info.engine_clock_hz);
            }

            if (DoesNodeExist(asic_root, kNodeStringAsicIds))
            {
                ProcessAsicIdInfoNode(asic_root[kNodeStringAsicIds], asic_info.id_info);
            }
        }

        /// @brief Process a GPU device's memory heap info JSON node.
        ///
        /// @param [in] heaps_root The parent JSON node containing the device's memory heaps info.
        /// @param [in, out] heaps The vector of structures to be populated with the parsed memory heap info.
        virtual void ProcessGpuMemoryHeapsNode(const rapidjson::Value& heaps_root, std::vector<HeapInfo>& heaps)
        {
            if (!heaps_root.IsObject())
            {
                return;
            }

            for (auto heap_iter = heaps_root.MemberBegin(); heap_iter != heaps_root.MemberEnd(); ++heap_iter)
            {
                const rapidjson::Value& current_heap_node = heap_iter->value;

                HeapInfo current_heap  = {};
                current_heap.heap_type = std::string(heap_iter->name.GetString());
                current_heap.phys_addr = Get<uint64_t>(current_heap_node, kNodeStringPhysicalAddress, 0);
                current_heap.size      = Get<uint64_t>(current_heap_node, kNodeStringSize, 0);

                heaps.push_back(current_heap);
            }
        }

        /// @brief Process a device's excluded memory region info JSON node.
        ///
        /// @param [in] excluded_ranges_root The parent JSON node containing the device's excluded memory range info.
        /// @param [in, out] excluded_ranges The vector of structures to be populated with the parsed excluded memory range info.
        virtual void ProcessExcludedVaRanges(const rapidjson::Value& excluded_ranges_root, std::vector<ExcludedRangeInfo>& excluded_ranges)
        {
            if (!excluded_ranges_root.IsArray())
            {
                return;
            }

            for (rapidjson::SizeType i = 0; i < excluded_ranges_root.Size(); ++i)
            {
                const rapidjson::Value& excluded_range_root = excluded_ranges_root[i];

                ExcludedRangeInfo current_range = {};
                current_range.base              = Get<uint64_t>(excluded_range_root, kNodeStringBase, 0);
                current_range.size              = Get<uint64_t>(excluded_range_root, kNodeStringSize, 0);

                excluded_ranges.push_back(current_range);
            }
        }

        /// @brief Process a device's memory info JSON node.
        ///
        /// @param [in] memory_root The parent JSON node containing the device's memory info.
        /// @param [in, out] memory_info The structure to be populated with the parsed memory info.
        virtual void ProcessGpuMemoryNode(const rapidjson::Value& memory_root, MemoryInfo& memory_info)
        {
            memory_info.type              = Get<std::string>(memory_root, kNodeStringType, "");
            memory_info.mem_ops_per_clock = Get<uint32_t>(memory_root, kNodeStringMemoryOpsPerClock, 0);
            memory_info.bus_bit_width     = Get<uint32_t>(memory_root, kNodeStringMemoryBusBitWidth, 0);
            memory_info.bandwidth         = Get<uint64_t>(memory_root, kNodeStringMemoryBandwith, 0);

            if (DoesNodeExist(memory_root, kNodeStringMemoryClockSpeed))
            {
                ProcessClockInfoNode(memory_root[kNodeStringMemoryClockSpeed], memory_info.mem_clock_hz);
            }

            if (DoesNodeExist(memory_root, kNodeStringHeaps))
            {
                ProcessGpuMemoryHeapsNode(memory_root[kNodeStringHeaps], memory_info.heaps);
            }

            if (DoesNodeExist(memory_root, kNodeStringExcludedVaRanges))
            {
                ProcessExcludedVaRanges(memory_root[kNodeStringExcludedVaRanges], memory_info.excluded_va_ranges);
            }
        }

        /// @brief Process a software version info JSON node.
        ///
        /// @param [in] sw_version_root The parent JSON node containing the software version info.
        /// @param [in, out] version The structure to be populated with the parsed version info.
        virtual void ProcessSoftwareVersionNode(const rapidjson::Value& sw_version_root, SoftwareVersion& version)
        {
            version.major = Get<uint32_t>(sw_version_root, kNodeStringMajor, 0);
            version.minor = Get<uint32_t>(sw_version_root, kNodeStringMinor, 0);
            version.misc  = Get<uint32_t>(sw_version_root, kNodeStringMisc, 0);
        }

        /// @brief Process the parent GPUs JSON node. There may be 1 or more devices to process.
        ///
        /// @param [in] gpus_root The parent JSON node with info for all connected GPUs.
        /// @param [in, out] gpus The vector of structures to be populated with the parsed GpuInfo fields.
        virtual void ProcessGpusNodes(const rapidjson::Value& gpus_root, std::vector<GpuInfo>& gpus)
        {
            if (!gpus_root.IsArray())
            {
                return;
            }

            for (rapidjson::SizeType i = 0; i < gpus_root.Size(); ++i)
            {
                const rapidjson::Value& gpu_node = gpus_root[i];

                GpuInfo gpu = {};

                gpu.name = Get<std::string>(gpu_node, kNodeStringName, "");

                if (DoesNodeExist(gpu_node, kNodeStringPci))
                {
                    ProcessGpuPciNode(gpu_node[kNodeStringPci], gpu.pci);
                }

                if (DoesNodeExist(gpu_node, kNodeStringAsic))
                {
                    ProcessGpuAsicNode(gpu_node[kNodeStringAsic], gpu.asic);
                }

                if (DoesNodeExist(gpu_node, kNodeStringMemory))
                {
                    ProcessGpuMemoryNode(gpu_node[kNodeStringMemory], gpu.memory);
                }

                if (DoesNodeExist(gpu_node, kNodeStringBigSw))
                {
                    ProcessSoftwareVersionNode(gpu_node[kNodeStringBigSw], gpu.big_sw);
                }

                if (DoesNodeExist(gpu_node, kNodeStringDriverIndex))
                {
                    gpu.driver_index = Get<uint32_t>(gpu_node, kNodeStringDriverIndex, 0);
                }

                gpus.push_back(gpu);
            }
        }

        virtual void ProcessDriverNode(const rapidjson::Value& driver_node, DriverInfo& driver)
        {
            driver.name              = Get<std::string>(driver_node, kNodeStringName, "");
            driver.description       = Get<std::string>(driver_node, kNodeStringDescription, "");
            driver.software_version  = Get<std::string>(driver_node, kNodeStringDriverSoftwareVersion, "");
            driver.packaging_version = Get<std::string>(driver_node, kNodeStringDriverPackagingVersion, "");

            if (DoesNodeExist(driver_node, kNodeStringDriverPackagingDate))
            {
                driver.packaging_date = Get<std::string>(driver_node, kNodeStringDriverPackagingDate, "");
            }
            else
            {
                driver.packaging_date = std::nullopt;
            }

            if (DoesNodeExist(driver_node, kNodeStringPci))
            {
                PciInfo pci_info = {};
                ProcessGpuPciNode(driver_node[kNodeStringPci], pci_info);
                driver.pci = pci_info;
            }

            if (DoesNodeExist(driver_node, kNodeStringPcis))
            {
                const auto& pcis_node = driver_node[kNodeStringPcis];
                if (pcis_node.IsArray())
                {
                    for (rapidjson::SizeType i = 0; i < pcis_node.Size(); ++i)
                    {
                        PciInfo pci_info = {};
                        ProcessGpuPciNode(pcis_node[i], pci_info);
                        driver.pcis.push_back(pci_info);
                    }
                }
            }

            // Strip the Debian epoch prefix (e.g. "1:") if present.
            size_t epoch_pos = driver.packaging_version.find(':');
            if (epoch_pos != std::string::npos)
            {
                driver.packaging_version = driver.packaging_version.substr(epoch_pos + 1);
            }

            // Parse major and minor
            if (!driver.packaging_version.empty())
            {
                size_t first = driver.packaging_version.find_first_of('.');
                if (first == std::string::npos)
                {
                    return;
                }

                driver.packaging_version_major = std::atoi(driver.packaging_version.substr(0, first).c_str());

                size_t rest = first + 1;
                size_t next = driver.packaging_version.find_first_not_of("0123456789", rest);
                if (next == std::string::npos)
                {
                    return;
                }

                driver.packaging_version_minor = std::atoi(driver.packaging_version.substr(rest, next - rest).c_str());
            }
        }

        /// @brief Process the drivers array JSON node.
        ///
        /// @param [in] drivers_node The JSON array node containing driver entries.
        /// @param [in, out] drivers The vector to be populated with parsed driver info entries.
        virtual void ProcessDriversNode(const rapidjson::Value& drivers_node, std::vector<DriverInfo>& drivers)
        {
            if (!drivers_node.IsArray())
            {
                return;
            }

            for (rapidjson::SizeType i = 0; i < drivers_node.Size(); ++i)
            {
                DriverInfo driver{};
                ProcessDriverNode(drivers_node[i], driver);
                drivers.push_back(driver);
            }
        }
    };

    class SystemInfoParserV2 : public SystemInfoParserV1
    {
    public:
        virtual void ProcessSystemValueNode(const rapidjson::Value& system_value_node, SystemInfo& system_info)
        {
            SystemInfoParserV1::ProcessSystemValueNode(system_value_node, system_info);

            if (DoesNodeExist(system_value_node, kNodeStringProcesses))
            {
                ProcessProcessListNode(system_value_node[kNodeStringProcesses], system_info.processes);
            }
        }

        virtual void ProcessProcessListNode(const rapidjson::Value& process_list_node, std::vector<Process>& processes)
        {
            if (!process_list_node.IsArray())
            {
                return;
            }

            for (rapidjson::SizeType i = 0; i < process_list_node.Size(); ++i)
            {
                const rapidjson::Value& process_node = process_list_node[i];

                Process p;
                p.name = Get<std::string>(process_node, kNodeStringName, "");
                p.path = Get<std::string>(process_node, kNodeStringPath, "");
                p.id   = Get<uint32_t>(process_node, kNodeStringProcessId, 0);

                processes.push_back(p);
            }
        }
    };

    /// @brief Create a parser to parse a versioned chunk of System Info JSON.
    /// @param [in] version_number The version number of the parser instance to create.
    /// @return A system info JSON parser.
    static std::shared_ptr<SystemInfoParserV1> CreateSystemInfoParser(const uint32_t version_number)
    {
        std::shared_ptr<SystemInfoParserV1> result = nullptr;

        switch (version_number)
        {
        case 1:
            result = std::make_shared<SystemInfoParserV1>();
            break;
        case 2:
            result = std::make_shared<SystemInfoParserV2>();
            break;
        default:
            return nullptr;
            break;
        }

        return result;
    }

    /// @brief Process the System Info JSON node. Includes parsing the structure revision number.
    ///
    /// @param [in] system_node The parent JSON node containing system info value fields.
    /// @param [in, out] system_info The structure to be populated with the parsed system info members.
    /// @return True if parsing was successful, and false if it failed.
    static bool ProcessSystemNode(const rapidjson::Value& system_node, SystemInfo& system_info)
    {
        // Handle version 2+ versioning object
        if (DoesNodeExist(system_node, kNodeStringVersion) && system_node[kNodeStringVersion].IsObject())
        {
            const auto& version_node  = system_node[kNodeStringVersion];
            system_info.version.major = Get<uint32_t>(version_node, kNodeStringMajor, 2);
            system_info.version.minor = Get<uint32_t>(version_node, kNodeStringMinor, 0);
            system_info.version.patch = Get<uint32_t>(version_node, kNodeStringPatch, 0);
            system_info.version.build = Get<uint32_t>(version_node, kNodeStringBuild, 0);
        }
        else
        {
            system_info.version.major = Get<uint32_t>(system_node, kNodeStringVersion, 1);
        }

        std::shared_ptr<SystemInfoParserV1> parser = CreateSystemInfoParser(system_info.version.major);
        assert(parser != nullptr);
        if (parser != nullptr)
        {
            parser->ProcessSystemValueNode(system_node, system_info);
            return true;
        }

        return false;
    }

    bool SystemInfoReader::Parse(const std::string& json, system_info_utils::SystemInfo& system_info)
    {
        rapidjson::Document structure;
        structure.Parse(json.c_str());

        if (structure.HasParseError() || !structure.IsObject())
        {
            return false;
        }

        // Process the 'system' node
        if (DoesNodeExist(structure, kNodeStringSystem))
        {
            return ProcessSystemNode(structure[kNodeStringSystem], system_info);
        }
        else
        {
            // Process a system info only chunk of JSON. Presumably from an RDF file.
            return ProcessSystemNode(structure, system_info);
        }
    }

    std::string SystemInfoReader::Parse(const std::string& json)
    {
        rapidjson::Document structure;
        structure.Parse(json.c_str());

        if (structure.HasParseError() || !structure.IsObject())
        {
            return "";
        }

        // Process the 'system' node
        if (DoesNodeExist(structure, kNodeStringSystem))
        {
            return DumpJson(structure[kNodeStringSystem]);
        }
        else
        {
            // Process a system info only chunk of JSON. Presumably from an RDF file.
            return json;
        }
    }

#ifdef SYSTEM_INFO_ENABLE_RDF
    bool SystemInfoReader::Parse(rdfChunkFile* file, SystemInfo& system_info)
    {
        assert(file != nullptr);

        bool result = false;

        int contains{};
        rdfChunkFileContainsChunk(file, kSystemInfoChunkIdentifier, 0, &contains);
        if (!contains)
        {
            return result;
        }

        // Access system info chunk data
        uint32_t version{};
        rdfChunkFileGetChunkVersion(file, kSystemInfoChunkIdentifier, 0, &version);
        if (version > kSystemInfoChunkVersionMax)
        {
            return result;
        }

        // Access chunk data
        int64_t chunk_size{};
        rdfChunkFileGetChunkDataSize(file, kSystemInfoChunkIdentifier, 0, &chunk_size);

        char* buffer = new char[chunk_size + 1];
        rdfChunkFileReadChunkData(file, kSystemInfoChunkIdentifier, 0, buffer);
        buffer[chunk_size] = 0;

        result = Parse(buffer, system_info);
        delete[] buffer;

        return result;
    }

#endif
}  // namespace system_info_utils
