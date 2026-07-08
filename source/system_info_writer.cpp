// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief System info writer implementation

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>

#define SECURITY_WIN32
#include <security.h>

#include <lm.h>

#include <wmi.hpp>
#include <wmiclasses.hpp>
#endif

#include "system_info_writer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <vector>

#include <rapidjson/document.h>

#include <ddAmdGpuInfo.h>

#include "ddAdapterInfo.h"
#include "definitions.h"
#include "platform_utils.h"
#include "system_info_reader.h"
#include "version.h"

static constexpr uint32_t kFamilyRaphael   = 0x95;
static constexpr uint32_t kFamilyMendocino = 0x97;

namespace
{
    bool IsIntegrated(const uint32_t family)
    {
        return family == kFamilyRaphael || family == kFamilyMendocino;
    }

    std::optional<std::string> ParsePackagingDate(const std::string& packaging_version)
    {
        if (const std::string_view view = packaging_version; !view.empty())
        {
            // Check for the beginning and end on what should be the packaging date
            // Example: 25.10.25.04-250728a-417802E
            if (const auto date_begin = view.find_first_of('-'); date_begin != std::string_view::npos)
            {
                const auto date_begin_string = view.substr(date_begin + 1);
                if (const auto date_end = date_begin_string.find_first_of('-'); date_end != std::string_view::npos)
                {
                    // Parse out just the date string (YY/MM/DD)
                    const auto date_parse_string = date_begin_string.substr(0, date_end).substr(0, 6);
                    return std::string(date_parse_string);
                }
            }
        }

        return std::nullopt;
    }

    struct PciLocation
    {
        uint32_t bus      = 0;
        uint32_t device   = 0;
        uint32_t function = 0;
    };

    struct DriverInfo
    {
        std::string name;
        std::string description;
        std::string packaging_version;
        std::string software_version;
        uint32_t    pci_bus            = 0;
        uint32_t    pci_device         = 0;
        uint32_t    pci_function       = 0;
        bool        has_pci_location   = false;  ///< True if PCI BDF was successfully resolved.
        uint32_t    hardware_device_id = 0;      ///< PCI device ID parsed from the hardware ID (DEV_XXXX).

        // Used after deduplication to store all PCI locations and device IDs that share this driver.
        std::vector<PciLocation> all_pci_locations;
        std::vector<uint32_t>    all_hardware_device_ids;
    };

    /// @brief Determines if two driver entries represent the same driver installation.
    bool IsSameDriver(const DriverInfo& a, const DriverInfo& b)
    {
        return a.name == b.name && a.packaging_version == b.packaging_version && a.software_version == b.software_version;
    }

    /// @brief Deduplicates a list of driver entries, merging entries with identical version info.
    /// Merged entries accumulate all PCI locations and hardware device IDs for GPU-to-driver mapping.
    std::vector<DriverInfo> DeduplicateDrivers(const std::vector<DriverInfo>& drivers)
    {
        std::vector<DriverInfo> result;

        for (const auto& driver : drivers)
        {
            bool merged = false;
            for (auto& existing : result)
            {
                if (IsSameDriver(existing, driver))
                {
                    if (driver.has_pci_location)
                    {
                        existing.all_pci_locations.push_back({driver.pci_bus, driver.pci_device, driver.pci_function});

                        // Promote primary PCI fields if the existing entry had none
                        if (!existing.has_pci_location)
                        {
                            existing.pci_bus          = driver.pci_bus;
                            existing.pci_device       = driver.pci_device;
                            existing.pci_function     = driver.pci_function;
                            existing.has_pci_location = true;
                        }
                    }
                    if (driver.hardware_device_id != 0)
                    {
                        existing.all_hardware_device_ids.push_back(driver.hardware_device_id);
                    }
                    merged = true;
                    break;
                }
            }

            if (!merged)
            {
                DriverInfo entry = driver;
                if (driver.has_pci_location)
                {
                    entry.all_pci_locations.push_back({driver.pci_bus, driver.pci_device, driver.pci_function});
                }
                if (driver.hardware_device_id != 0)
                {
                    entry.all_hardware_device_ids.push_back(driver.hardware_device_id);
                }
                result.push_back(std::move(entry));
            }
        }

        return result;
    }

#ifdef _WIN32
    bool IsUserInGroup(LPCWSTR user, LPCWSTR group)
    {
        DWORD                    num_entries_total = 0;
        DWORD                    num_entries_read  = 0;
        LOCALGROUP_USERS_INFO_0* groups            = nullptr;

        if (const DWORD result = NetUserGetLocalGroups(
                nullptr, user, 0, LG_INCLUDE_INDIRECT, reinterpret_cast<LPBYTE*>(&groups), MAX_PREFERRED_LENGTH, &num_entries_read, &num_entries_total);
            result == NERR_Success)
        {
            for (DWORD i = 0; i < num_entries_read; ++i)
            {
                const LOCALGROUP_USERS_INFO_0& info = groups[i];
                if (_wcsicmp(info.lgrui0_name, group) == 0)
                {
                    NetApiBufferFree(groups);
                    return true;
                }
            }

            NetApiBufferFree(groups);
        }

        return false;
    }

    bool HasRegKey(HKEY parent, LPCSTR key)
    {
        HKEY    out;
        LSTATUS status = RegOpenKeyExA(parent, key, 0, KEY_READ, &out);
        if (status != ERROR_SUCCESS)
        {
            return false;
        }

        RegCloseKey(out);
        return true;
    }

    bool HasRegKeyAndValue(HKEY parent, LPCSTR key, LPCSTR value)
    {
        HKEY    out;
        LSTATUS status = RegOpenKeyExA(parent, key, 0, KEY_READ, &out);
        if (status != ERROR_SUCCESS)
        {
            return false;
        }

        DWORD data_type;
        status = RegQueryValueExA(out, value, NULL, &data_type, NULL, NULL);
        if (status != ERROR_SUCCESS)
        {
            RegCloseKey(out);
            return false;
        }

        RegCloseKey(out);
        return true;
    }

    bool NeedsAddUserToGroup()
    {
        static constexpr const wchar_t* kPerformanceLogGroup = L"Performance Log Users";
        static constexpr const char*    kRegistryValue       = "RgpEnableEtw";

        WCHAR user_name[UNLEN + 1];
        DWORD user_name_size = ARRAYSIZE(user_name);

        if (GetUserNameExW(NameSamCompatible, user_name, &user_name_size))
        {
            if (!IsUserInGroup(user_name, kPerformanceLogGroup))
            {
                return true;
            }
        }

        // The service name needs to be queried dynamically from KMD.
        const std::wstring serviceStr  = DevDriver::QueryServiceString();
        const std::wstring fullRegPath = L"SYSTEM\\CurrentControlSet\\Services\\" + serviceStr + L"\\Parameters";

        size_t len            = wcstombs(nullptr, fullRegPath.c_str(), 0) + 1;
        char*  fullRegPathBuf = new char[len];
        wcstombs(fullRegPathBuf, fullRegPath.c_str(), len);

        if (HasRegKey(HKEY_LOCAL_MACHINE, fullRegPathBuf) && !HasRegKeyAndValue(HKEY_LOCAL_MACHINE, fullRegPathBuf, kRegistryValue))
        {
            delete[] fullRegPathBuf;
            return true;
        }

        delete[] fullRegPathBuf;

        if (HasRegKey(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\amdwddmg\\Parameters") &&
            !HasRegKeyAndValue(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\amdwddmg\\Parameters", kRegistryValue))
        {
            return true;
        }

        return false;
    }

    std::optional<std::string> QueryRegistryString(const std::string& folder, const std::string& key)
    {
        // Open the path to the registry key being queried.
        HKEY    hkey = nullptr;
        LRESULT res  = RegOpenKey(HKEY_LOCAL_MACHINE, folder.c_str(), &hkey);
        if (res == ERROR_SUCCESS)
        {
            DWORD                            key_type       = REG_SZ;
            const size_t                     version_length = 256;
            std::array<char, version_length> value{};
            DWORD                            value_length = version_length;
            res = RegQueryValueEx(hkey, key.c_str(), nullptr, &key_type, reinterpret_cast<LPBYTE>(&value), &value_length);
            RegCloseKey(hkey);
            if (res == ERROR_SUCCESS)
            {
                // The key was read successfully, so copy the value to the output string.
                return std::string(value.begin(), value.end());
            }
        }

        return std::nullopt;
    }

    std::chrono::year_month_day DateFromPackagingString(const std::string& packaging_version)
    {
        auto date_string = ParsePackagingDate(packaging_version);
        if (date_string)
        {
            const auto&        date = date_string.value();
            std::chrono::year  year{2000 + std::stoi(date.substr(0, 2))};
            std::chrono::month month{static_cast<uint32_t>(std::stoi(date.substr(2, 2)))};
            std::chrono::day   day{static_cast<uint32_t>(std::stoi(date.substr(4, 2)))};
            return std::chrono::year_month_day{year / month / day};
        }

        return {};
    }

    struct WindowsDriverVersionInfo
    {
        std::string packaging_version;
        std::string software_version;
    };

    /// @brief Reads a DWORD registry value.
    /// @param [in] folder The registry key path.
    /// @param [in] key The value name to read.
    /// @return The DWORD value, or nullopt if not found.
    std::optional<uint32_t> QueryRegistryDword(const std::string& folder, const std::string& key)
    {
        HKEY    hkey = nullptr;
        LRESULT res  = RegOpenKeyExA(HKEY_LOCAL_MACHINE, folder.c_str(), 0, KEY_READ, &hkey);
        if (res == ERROR_SUCCESS)
        {
            DWORD key_type = REG_DWORD;
            DWORD value    = 0;
            DWORD size     = sizeof(value);
            res            = RegQueryValueExA(hkey, key.c_str(), nullptr, &key_type, reinterpret_cast<LPBYTE>(&value), &size);
            RegCloseKey(hkey);
            if (res == ERROR_SUCCESS && key_type == REG_DWORD)
            {
                return value;
            }
        }
        return std::nullopt;
    }

    /// @brief Parses the PCI device ID (DEV_XXXX) from a PCI hardware ID string.
    /// @param [in] hardware_id The hardware ID string (e.g., "VEN_1002&DEV_744C&SUBSYS_...")
    /// @return The parsed device ID, or 0 if parsing failed.
    uint32_t ParseDeviceIdFromHardwareId(const std::string& hardware_id)
    {
        std::string lower = hardware_id;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        auto pos = lower.find("dev_");
        if (pos != std::string::npos && pos + 8 <= lower.size())
        {
            std::string hex_str = lower.substr(pos + 4, 4);
            return static_cast<uint32_t>(std::strtoul(hex_str.c_str(), nullptr, 16));
        }
        return 0;
    }

    /// @brief Parses PCI bus/device/function from a LocationInformation registry string.
    /// Expected format: "PCI bus X, device Y, function Z"
    /// @param [in] location The LocationInformation string.
    /// @param [out] bus The parsed PCI bus number.
    /// @param [out] device The parsed PCI device number.
    /// @param [out] function The parsed PCI function number.
    /// @return true if parsing succeeded, false otherwise.
    bool ParseLocationInfo(const std::string& location, uint32_t& bus, uint32_t& device, uint32_t& function)
    {
        // Format: "PCI bus X, device Y, function Z"
        int b = 0, d = 0, f = 0;
        if (sscanf_s(location.c_str(), "PCI bus %d, device %d, function %d", &b, &d, &f) == 3)
        {
            bus      = static_cast<uint32_t>(b);
            device   = static_cast<uint32_t>(d);
            function = static_cast<uint32_t>(f);
            return true;
        }
        return false;
    }

    /// @brief Finds all AMD driver entries by enumerating PCI device instances in the registry.
    /// Uses SYSTEM\CurrentControlSet\Enum\PCI to find AMD display devices, reads PCI BDF
    /// from Address/BusNumber DWORD values or LocationInformation, and follows the Driver
    /// value to the class key for version info.
    /// @return A vector of all found AMD driver information entries with PCI location.
    std::vector<DriverInfo> FindAllDrivers()
    {
        std::vector<DriverInfo> results;

        const std::string enum_pci_path = "SYSTEM\\CurrentControlSet\\Enum\\PCI";

        HKEY enum_pci_key = nullptr;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, enum_pci_path.c_str(), 0, KEY_READ, &enum_pci_key) != ERROR_SUCCESS)
        {
            return results;
        }

        // Enumerate device ID subkeys (e.g., VEN_1002&DEV_744C&SUBSYS_...)
        char  device_id_name[256] = {};
        DWORD device_id_name_len  = sizeof(device_id_name);

        for (DWORD i = 0; RegEnumKeyExA(enum_pci_key, i, device_id_name, &device_id_name_len, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
             ++i, device_id_name_len = sizeof(device_id_name))
        {
            // Check if this is an AMD device (VEN_1002)
            std::string device_id_str(device_id_name);
            std::string device_id_lower = device_id_str;
            std::transform(device_id_lower.begin(), device_id_lower.end(), device_id_lower.begin(), ::tolower);
            if (device_id_lower.find("ven_1002") == std::string::npos)
            {
                continue;
            }

            // Parse the hardware device ID from the key name
            uint32_t hw_device_id = ParseDeviceIdFromHardwareId(device_id_str);

            // Open the device ID subkey to enumerate instance subkeys
            std::string device_id_path = enum_pci_path + "\\" + device_id_str;
            HKEY        device_id_key  = nullptr;
            if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, device_id_path.c_str(), 0, KEY_READ, &device_id_key) != ERROR_SUCCESS)
            {
                continue;
            }

            // Enumerate instance subkeys (e.g., "4&12345&0&0008")
            char  instance_name[256] = {};
            DWORD instance_name_len  = sizeof(instance_name);

            for (DWORD j = 0; RegEnumKeyExA(device_id_key, j, instance_name, &instance_name_len, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
                 ++j, instance_name_len = sizeof(instance_name))
            {
                std::string instance_path = device_id_path + "\\" + std::string(instance_name);

                // Check that this device has a display class GUID (class = Display)
                auto class_guid = QueryRegistryString(instance_path, "ClassGUID");
                if (!class_guid.has_value())
                {
                    continue;
                }
                std::string guid_lower = class_guid.value();
                std::transform(guid_lower.begin(), guid_lower.end(), guid_lower.begin(), ::tolower);
                if (guid_lower.find("{4d36e968-e325-11ce-bfc1-08002be10318}") == std::string::npos)
                {
                    continue;
                }

                DriverInfo info{};
                info.hardware_device_id = hw_device_id;

                // Try to get PCI BDF from Address and BusNumber DWORD values first
                auto bus_number = QueryRegistryDword(instance_path, "BusNumber");
                auto address    = QueryRegistryDword(instance_path, "Address");
                if (bus_number.has_value() && address.has_value())
                {
                    info.pci_bus          = bus_number.value();
                    info.pci_device       = (address.value() >> 16) & 0xFFFF;
                    info.pci_function     = address.value() & 0xFFFF;
                    info.has_pci_location = true;
                }
                else
                {
                    // Fallback: read LocationInformation string
                    auto location_info = QueryRegistryString(instance_path, "LocationInformation");
                    if (location_info.has_value())
                    {
                        info.has_pci_location = ParseLocationInfo(location_info.value(), info.pci_bus, info.pci_device, info.pci_function);
                    }
                }

                // Read DeviceDesc for description
                auto device_desc = QueryRegistryString(instance_path, "DeviceDesc");
                if (device_desc.has_value())
                {
                    // DeviceDesc may be in format "@driver.inf,%section%;Description" — extract the trailing part
                    std::string desc      = device_desc.value();
                    size_t      last_semi = desc.rfind(';');
                    info.description      = (last_semi != std::string::npos) ? desc.substr(last_semi + 1) : desc;
                }

                if (info.description.empty() || info.description.find_first_not_of('\0') == std::string::npos)
                {
                    info.description = "AMD Windows Driver";
                }

                // Read Mfg for manufacturer/name
                auto mfg = QueryRegistryString(instance_path, "Mfg");
                if (mfg.has_value())
                {
                    std::string name      = mfg.value();
                    size_t      last_semi = name.rfind(';');
                    info.name             = (last_semi != std::string::npos) ? name.substr(last_semi + 1) : name;
                }

                // Follow the Driver value to the class key for version info
                auto driver_key_ref = QueryRegistryString(instance_path, "Driver");
                if (driver_key_ref.has_value())
                {
                    std::string class_key_path = "SYSTEM\\CurrentControlSet\\Control\\Class\\" + driver_key_ref.value();

                    auto packaging_version = QueryRegistryString(class_key_path, "ReleaseVersion");
                    if (packaging_version.has_value())
                    {
                        info.packaging_version = packaging_version.value();
                    }

                    auto software_version = QueryRegistryString(class_key_path, "RadeonSoftwareVersion");
                    if (software_version.has_value())
                    {
                        info.software_version = software_version.value();
                    }
                }

                results.push_back(info);
            }

            RegCloseKey(device_id_key);
        }

        RegCloseKey(enum_pci_key);
        return results;
    }

    std::optional<WindowsDriverVersionInfo> FindPackagingAndSoftwareVersion()
    {
        // First try the standard location for the packaging and software version
        auto packaging_version = QueryRegistryString("SOFTWARE\\ATI Technologies\\Install", "ReleaseVersion");
        auto software_version  = QueryRegistryString("SOFTWARE\\ATI Technologies\\Install", "RadeonSoftwareVersion");
        if (packaging_version.has_value())
        {
            WindowsDriverVersionInfo info;
            info.packaging_version = packaging_version.value();
            if (software_version)
            {
                info.software_version = software_version.value();
            }
            return info;
        }

        // Otherwise, fallback to searching through driver key locations.

        // TODO: This logic currently doesn't filter any iGPU and assumes that the newest driver found is always associated
        // with a dGPU on the system. Future enhancements could be made to account for or filter iGPU.
        std::vector<WindowsDriverVersionInfo> results;
        std::string                           full_reg_key_path = "SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}\\0000";
        int                                   counter           = 0;
        while (HasRegKey(HKEY_LOCAL_MACHINE, full_reg_key_path.c_str()))
        {
            WindowsDriverVersionInfo info{};

            packaging_version = QueryRegistryString(full_reg_key_path, "ReleaseVersion");
            software_version  = QueryRegistryString(full_reg_key_path, "RadeonSoftwareVersion");
            if (software_version)  // optional
            {
                info.software_version = software_version.value();
            }

            if (packaging_version)
            {
                info.packaging_version = packaging_version.value();
                results.push_back(info);
            }

            counter++;
            full_reg_key_path.resize(full_reg_key_path.size() - 4);
            std::ostringstream oss;
            oss << std::setw(4) << std::setfill('0') << counter;
            full_reg_key_path += oss.str();
        }

        std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) {
            const std::chrono::year_month_day date_a = DateFromPackagingString(a.packaging_version);
            const std::chrono::year_month_day date_b = DateFromPackagingString(b.packaging_version);
            return date_a > date_b;
        });

        if (!results.empty())
        {
            return results.front();
        }

        return std::nullopt;
    }
#else
    /// @brief Non-Windows stub: no driver enumeration is performed.
    inline std::vector<DriverInfo> FindAllDrivers()
    {
        return {};
    }
#endif

#ifdef __linux__
    std::string ProcessCommand(const std::string& command, const std::string& output_file, bool strip = true)
    {
        std::string result;

        const int system_result = system(command.c_str());
        if (system_result == 0)
        {
            std::ifstream file_stream;
            file_stream.open(output_file);
            if (file_stream.is_open())
            {
                result = std::string((std::istreambuf_iterator<char>(file_stream)), std::istreambuf_iterator<char>());
                if (strip)
                {
                    result.erase(std::remove(result.begin(), result.end(), '\n'), result.end());
                }
                file_stream.close();
                remove(output_file.c_str());
            }
        }

        return result;
    }
#endif

    std::string QueryDriverName()
    {
        std::string result{};

#ifdef WIN32
        // No real AMD defined name for Windows driver, so we define our own.
        result = "AMD Windows";
#else
        // Determine which Linux os we are running
        DevDriver::Platform::OsInfo os_info{};
        DevDriver::Platform::QueryOsInfo(&os_info);

        std::vector<std::string> commands;
        if (DevDriver::Platform::Strcmpi(os_info.name, "Arch Linux") == 0)
        {
            // TODO: Arch Linux driver name
        }
        else if (DevDriver::Platform::Strcmpi(os_info.name, "Fedora Linux") == 0)
        {
            commands.emplace_back(
                R"(dnf info installed amdvlk | awk 'BEGIN{FS="Name"}; gsub(":", "", $2);' | awk '{sub(/^[ \t]+/, ""); print $0}' > /tmp/name.txt)");
        }
        else
        {
            // Support Ubuntu pro driver name
            commands.emplace_back(R"(dpkg-query -s vulkan-amdgpu-pro 2>/dev/null | awk 'BEGIN{FS="Package: "}{print $2}' | awk 'NF > 0' > /tmp/name.txt)");

            // Support Ubuntu closed amdvlk driver name
            commands.emplace_back(R"(dpkg-query -s vulkan-amdgpu 2>/dev/null | awk 'BEGIN{FS="Package: "}{print $2}' | awk 'NF > 0' > /tmp/name.txt)");

            // Support Ubuntu GitHub amdvlk driver name
            commands.emplace_back(R"(dpkg-query -s amdvlk 2>/dev/null | awk 'BEGIN{FS="Package: "}{print $2}' | awk 'NF > 0' > /tmp/name.txt)");

            // Support Ubuntu mesa AMDGPU vulkan driver name
            commands.emplace_back(
                R"(dpkg-query -s mesa-amdgpu-vulkan-drivers 2>/dev/null | awk 'BEGIN{FS="Package: "}{print $2}' | awk 'NF > 0' > /tmp/name.txt)");

            // Support Ubuntu mesa vulkan graphics driver name
            commands.emplace_back(R"(dpkg-query -s mesa-vulkan-drivers 2>/dev/null | awk 'BEGIN{FS="Package: "}{print $2}' | awk 'NF > 0' > /tmp/name.txt)");
        }

        for (const auto& command : commands)
        {
            const int system_result = system(command.c_str());
            if (system_result == 0)
            {
                const char*   name_file_name = "/tmp/name.txt";
                std::ifstream name_file_stream;
                name_file_stream.open(name_file_name);
                if (name_file_stream.is_open())
                {
                    std::string line_str;
                    getline(name_file_stream, line_str);
                    name_file_stream.close();
                    remove(name_file_name);
                    if (!line_str.empty())
                    {
                        result = line_str;
                        break;
                    }
                }
            }
        }
#endif

        return result;
    }

    std::string QueryDriverDescription()
    {
#ifdef WIN32
        return "AMD Windows Driver";
#elif defined(__linux__)
        std::string result{};

        // Determine which Linux os we are running
        DevDriver::Platform::OsInfo os_info{};
        DevDriver::Platform::QueryOsInfo(&os_info);

        std::vector<std::string> commands;
        if (DevDriver::Platform::Strcmpi(os_info.name, "Arch Linux") == 0)
        {
            // TODO: Arch Linux driver description
        }
        else if (DevDriver::Platform::Strcmpi(os_info.name, "Fedora Linux") == 0)
        {
            commands.emplace_back(
                R"(dnf info installed amdvlk | awk 'BEGIN{FS="Description"}; gsub(":", "", $2);' | awk '{sub(/^[ \t]+/, ""); print $0}' > /tmp/description.txt)");
        }
        else
        {
            // Support Ubuntu pro driver description
            commands.emplace_back(
                R"(dpkg-query -s vulkan-amdgpu-pro 2>/dev/null | awk 'BEGIN{FS="Description: "}{print $2}' | awk 'NF > 0' > /tmp/description.txt)");

            // Support Ubuntu AMDVLK driver description
            commands.emplace_back(
                R"(dpkg-query -s vulkan-amdgpu 2>/dev/null | awk 'BEGIN{FS="Description: "}{print $2}' | awk 'NF > 0' > /tmp/description.txt)");

            // Support Ubuntu AMDVLK driver description
            commands.emplace_back(R"(dpkg-query -s amdvlk 2>/dev/null | awk 'BEGIN{FS="Description: "}{print $2}' | awk 'NF > 0' > /tmp/description.txt)");

            // Support mesa AMDGPU vulkan driver
            commands.emplace_back(
                R"(dpkg-query -s mesa-amdgpu-vulkan-drivers 2>/dev/null | awk 'BEGIN{FS="Description: "}{print $2}' | awk 'NF > 0' > /tmp/description.txt)");

            // Support mesa vulkan graphics driver
            commands.emplace_back(
                R"(dpkg-query -s mesa-vulkan-drivers 2>/dev/null | awk 'BEGIN{FS="Description: "}{print $2}' | awk 'NF > 0' > /tmp/description.txt)");
        }

        for (const auto& command : commands)
        {
            const int system_result = system(command.c_str());
            if (system_result == 0)
            {
                // Read the description file that was just written.
                const char*   description_file_name = "/tmp/description.txt";
                std::ifstream description_file_stream;
                description_file_stream.open(description_file_name);
                if (description_file_stream.is_open())
                {
                    // Read the description string from the file.
                    std::string line_str;
                    getline(description_file_stream, line_str);
                    // Close the file stream and remove the file.
                    description_file_stream.close();
                    remove(description_file_name);
                    if (!line_str.empty())
                    {
                        result = line_str;
                        break;
                    }
                }
            }
        }

        return result;
#endif
    }

    std::string QueryDriverPackagingVersion()
    {
#ifdef WIN32
        auto result = FindPackagingAndSoftwareVersion();
        if (result)
        {
            return result.value().packaging_version;
        }

        // TODO: Change this to return optional
        return "";
#elif defined(__linux__)
        std::string result{};

        // Determine which Linux os we are running
        DevDriver::Platform::OsInfo os_info{};
        DevDriver::Platform::QueryOsInfo(&os_info);

        std::vector<std::string> commands;
        if (DevDriver::Platform::Strcmpi(os_info.name, "Arch Linux") == 0)
        {
            // Support Arch Linux driver version using pacman
            // NOTE: This specifically looks for the vulkan-amdgpu-pro package which should be fine
            // considering our tools specifically target Vulkan on Linux.
            commands.emplace_back(
                R"(pacman -Q --info vulkan-amdgpu-pro | awk '/Version/ { gsub("_",".", $3); gsub("-", ".", $3); print $3 }' > /tmp/version.txt)");
        }
        else if (DevDriver::Platform::Strcmpi(os_info.name, "Fedora Linux") == 0)
        {
            // Support Fedora Linux (Fedora 35+)
            // NOTE: This looks specifically for AMDVLK driver which is the AMD open source
            // vulkan driver. The is currently no official support for amdgpu-pro on Fedora
            commands.emplace_back("dnf info installed amdvlk | awk '/Version/ {print $3}' > /tmp/version.txt");
        }
        else
        {
            // Support Ubuntu pro driver version
            commands.emplace_back(R"(dpkg-query -s vulkan-amdgpu-pro 2>/dev/null | awk 'BEGIN{FS="Version: "}{print $2}' | awk 'NF > 0' > /tmp/version.txt)");

            // Support Ubuntu AMDVLK driver version
            commands.emplace_back(R"(dpkg-query -s vulkan-amdgpu 2>/dev/null | awk 'BEGIN{FS="Version: "}{print $2}' | awk 'NF > 0' > /tmp/version.txt)");

            // Support Ubuntu AMDVLK driver version
            commands.emplace_back(R"(dpkg-query -s amdvlk 2>/dev/null | awk 'BEGIN{FS="Version: "}{print $2}' | awk 'NF > 0' > /tmp/version.txt)");

            // Support Ubuntu mesa AMDGPU vulkan driver version
            commands.emplace_back(
                R"(dpkg-query -s mesa-amdgpu-vulkan-drivers 2>/dev/null | awk 'BEGIN{FS="Version: "}{print $2}' | awk 'NF > 0' > /tmp/version.txt)");

            // Support Ubuntu mesa driver version
            commands.emplace_back(R"(dpkg-query -s mesa-vulkan-drivers 2>/dev/null | awk 'BEGIN{FS="Version: "}{print $2}' | awk 'NF > 0' > /tmp/version.txt)");
        }

        for (const auto& command : commands)
        {
            const int system_result = system(command.c_str());
            if (system_result == 0)
            {
                const char*   version_file_name = "/tmp/version.txt";
                std::ifstream version_file_stream;
                version_file_stream.open(version_file_name);
                if (version_file_stream.is_open())
                {
                    std::string line_str;
                    getline(version_file_stream, line_str);
                    version_file_stream.close();
                    remove(version_file_name);
                    if (!line_str.empty())
                    {
                        result = line_str;
                        break;
                    }
                }
            }
        }

        return result;
#endif
    }

    std::string QueryDriverSoftwareVersion()
    {
#ifdef WIN32
        auto result = FindPackagingAndSoftwareVersion();
        if (result)
        {
            return result.value().software_version;
        }
#elif defined(__linux__)
        // No implementation for Linux
#endif
        return {};
    }

    void WritePlatformConfig(DevDriver::IStructuredWriter* writer)
    {
#ifdef __linux__
        writer->KeyAndBeginMap(kNodeStringLinux);
        {
            bool        power_dpm_writable = false;
            struct stat st{};

            // SEE: radeon_developer_panel/scripts/setup.sh for enabling clock mode settings
            constexpr const char* kCards[]              = {"card0", "card1", "card2", "card3"};
            constexpr const char* kPowerDpmFile         = "/device/power_dpm_force_performance_level";
            constexpr const char* kClocksFilePathPrefix = "/sys/class/drm/";
            for (auto kCard : kCards)
            {
                const std::string clocks_file = std::string(kClocksFilePathPrefix) + kCard + kPowerDpmFile;
                if (stat(clocks_file.c_str(), &st) == 0)
                {
                    mode_t permissions = st.st_mode;
                    if ((permissions & S_IWUSR) != 0 && (permissions & S_IWGRP) != 0 && (permissions & S_IWOTH) != 0)
                    {
                        power_dpm_writable = true;
                        break;
                    }
                }
            }
            writer->KeyAndValue(kNodeStringPowerDpmWritable, power_dpm_writable);

            // Query libdrm version
            DevDriver::Vector<DevDriver::AmdGpuInfo> gpus(DevDriver::Platform::GenericAllocCb);
            DD_UNHANDLED_RESULT(QueryGpuInfo(DevDriver::Platform::GenericAllocCb, &gpus));
            if (!gpus.IsEmpty())
            {
                // TODO: libdrm version ideally should not be GPU specific
                const DevDriver::AmdGpuInfo& gpu = gpus[0];
                writer->KeyAndBeginMap(kNodeStringDrm);
                {
                    writer->KeyAndValue(kNodeStringMajor, gpu.drmVersion.Major);
                    writer->KeyAndValue(kNodeStringMinor, gpu.drmVersion.Minor);
                }
                writer->EndMap();
            }
        }
        writer->EndMap();
#elif defined(_WIN32)
        writer->KeyAndBeginMap(kNodeStringWindows);
        {
            DevDriver::Platform::EtwSupportInfo etw_info = {};
            DD_UNHANDLED_RESULT(QueryEtwInfo(&etw_info));
            writer->KeyAndBeginMap(kNodeStringEtwSupport);
            {
                writer->KeyAndValue(kNodeStringSupported, etw_info.isSupported);
                writer->KeyAndValue(kNodeStringHasPermission, etw_info.hasPermission);
                writer->KeyAndValue(kNodeStringStatusCode, etw_info.statusCode);
                writer->KeyAndValue(kNodeStringEtwRegistryOrUserGroup, NeedsAddUserToGroup());
            }
            writer->EndMap();
        }
        writer->EndMap();
#else
        DD_UNUSED(writer);
#endif
    }

    template <typename T>
    concept GpuInfoType = std::is_same_v<T, DevDriver::ddAmdAdapterInfo> || std::is_same_v<T, DevDriver::AmdGpuInfo>;

    template <GpuInfoType T>
    void WriteSingleGpu(DevDriver::IStructuredWriter* writer, const T& gpu, int32_t driver_index = -1)
    {
        DD_ASSERT(writer != nullptr);

        writer->BeginMap();
        {
            writer->KeyAndValue(kNodeStringName, gpu.name);

            // PCI info
            writer->KeyAndBeginMap(kNodeStringPci);
            {
                writer->KeyAndValue(kNodeStringPciBus, gpu.pci.bus);
                writer->KeyAndValue(kNodeStringDevice, gpu.pci.device);
                writer->KeyAndValue(kNodeStringPciFunction, gpu.pci.function);
            }
            writer->EndMap();

#ifdef __linux__
            // libdrm information
            writer->KeyAndBeginMap(kNodeStringDrm);
            {
                writer->KeyAndValue(kNodeStringMajor, gpu.drmVersion.Major);
                writer->KeyAndValue(kNodeStringMinor, gpu.drmVersion.Minor);
            }
            writer->EndMap();
#endif

            // ASIC info
            writer->KeyAndBeginMap(kNodeStringAsic);
            {
                writer->KeyAndValue(kNodeStringAsicGpuIndex, gpu.asic.gpuIndex);
                writer->KeyAndValue(kNodeStringAsicGpuCounterFrequency, gpu.asic.gpuCounterFreq);

                writer->KeyAndValue(kNodeStringAsicNumSe, gpu.asic.numShaderEngines);
                writer->KeyAndValue(kNodeStringAsicNumSaPerSe, gpu.asic.numShaderArraysPerEngine);
                writer->KeyAndValue(kNodeStringAsicNumCus, gpu.asic.numCus);

                writer->KeyAndBeginList(kNodeStringAsicCuMask);

                for (uint32_t shader_engine = 0; shader_engine < gpu.asic.numShaderEngines; ++shader_engine)
                {
                    writer->BeginList();
                    for (uint32_t shader_array = 0; shader_array < gpu.asic.numShaderArraysPerEngine; ++shader_array)
                    {
                        const uint32_t shader_array_mask = gpu.asic.cuMask[shader_engine][shader_array];
                        writer->Value(shader_array_mask);
                    }
                    writer->EndList();
                }

                writer->EndList();

                writer->KeyAndBeginMap(kNodeStringAsicEngineClockSpeed);
                {
                    writer->KeyAndValue(kNodeStringMin, gpu.engineClocks.min);
                    writer->KeyAndValue(kNodeStringMax, gpu.engineClocks.max);
                }
                writer->EndMap();

                writer->KeyAndBeginMap(kNodeStringAsicIds);
                {
                    writer->KeyAndValue(kNodeStringAsicGfxEngine, gpu.asic.ids.gfxEngineId);
                    writer->KeyAndValue(kNodeStringAsicFamily, gpu.asic.ids.family);
                    writer->KeyAndValue(kNodeStringAsicERev, gpu.asic.ids.eRevId);
                    writer->KeyAndValue(kNodeStringAsicRevision, gpu.asic.ids.revisionId);
                    writer->KeyAndValue(kNodeStringDevice, gpu.asic.ids.deviceId);
                    writer->KeyAndValue(kNodeStringAsicSubsystem, gpu.asic.ids.subsystemId);
                    writer->KeyAndValue(kNodeStringAsicVendor, gpu.asic.ids.vendorId);

                    std::ostringstream luid;
                    for (size_t i = 0; i < sizeof(gpu.asic.ids.luid); ++i)
                    {
                        luid << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(gpu.asic.ids.luid[i]);
                    }

                    const std::string luid_str = luid.str();
                    writer->KeyAndValue(kNodeStringAsicLuid, luid_str.c_str());
                }
                writer->EndMap();
            }
            writer->EndMap();

            // Memory info
            writer->KeyAndBeginMap(kNodeStringMemory);
            {
                writer->KeyAndValueEnumOrHex(kNodeStringType, gpu.memory.type);

                writer->KeyAndValue(kNodeStringMemoryOpsPerClock, gpu.memory.memOpsPerClock);
                writer->KeyAndValue(kNodeStringMemoryBusBitWidth, gpu.memory.busBitWidth);

                auto compute_bandwidth_bytes = [gpu] {
                    // Bit-Bandwidth is computed as the multiple of several properties:
                    return static_cast<uint64_t>(gpu.memory.busBitWidth)       // Bits per MemOp
                           * static_cast<uint64_t>(gpu.memory.memOpsPerClock)  // MemOps per MemClock
                           * gpu.memory.clocksHz.max                           // MemClocks per second
                           / 8;                                                // Convert Bits to Bytes
                };

                writer->KeyAndValue(kNodeStringMemoryBandwith, compute_bandwidth_bytes());

                writer->KeyAndBeginMap(kNodeStringMemoryClockSpeed);
                {
                    writer->KeyAndValue(kNodeStringMin, gpu.memory.clocksHz.min);
                    writer->KeyAndValue(kNodeStringMax, gpu.memory.clocksHz.max);
                }
                writer->EndMap();

                writer->KeyAndBeginMap(kNodeStringHeaps);
                {
                    writer->KeyAndBeginMap(kNodeStringLocal);
                    {
                        writer->KeyAndValue(kNodeStringPhysicalAddress, gpu.memory.localHeap.physAddr);
                        writer->KeyAndValue(kNodeStringSize, gpu.memory.localHeap.size);
                    }
                    writer->EndMap();

                    writer->KeyAndBeginMap(kNodeStringInvisible);
                    {
                        writer->KeyAndValue(kNodeStringPhysicalAddress, gpu.memory.invisibleHeap.physAddr);
                        writer->KeyAndValue(kNodeStringSize, gpu.memory.invisibleHeap.size);
                    }
                    writer->EndMap();
                }
                writer->EndMap();

                // A non-zero value indicated that this memory type is supported
                if (gpu.memory.hbccSize != 0)
                {
                    writer->KeyAndValue(kNodeStringHbccSize, gpu.memory.hbccSize);
                }

                writer->KeyAndBeginList(kNodeStringExcludedVaRanges);
                {
                    for (size_t i = 0; i < DevDriver::AmdGpuInfo::kMaxExcludedVaRanges; i++)
                    {
                        const auto& range = gpu.memory.excludedVaRanges[i];
                        if (range.size == 0)
                        {
                            continue;
                        }

                        writer->BeginMap();
                        {
                            writer->KeyAndValue(kNodeStringBase, range.base);
                            writer->KeyAndValue(kNodeStringSize, range.size);
                        }
                        writer->EndMap();
                    }
                }
                writer->EndList();
            }
            writer->EndMap();

            writer->KeyAndBeginMap(kNodeStringBigSw);
            {
                writer->KeyAndValue(kNodeStringMajor, gpu.bigSwVersion.Major);
                writer->KeyAndValue(kNodeStringMinor, gpu.bigSwVersion.Minor);
                writer->KeyAndValue(kNodeStringMisc, gpu.bigSwVersion.Misc);
            }
            writer->EndMap();

            if (driver_index >= 0)
            {
                writer->KeyAndValue(kNodeStringDriverIndex, static_cast<uint32_t>(driver_index));
            }
        }
        writer->EndMap();
    }

#ifdef _WIN32
    std::string ArchitectureFromWmiProcessorId(int id)
    {
        // These values can be found in the documentation
        // for Win32_Processor WMI class, Architecture attribute
        switch (id)
        {
        case 0:
            return "x86";
        case 1:
            return "MIPS";
        case 2:
            return "Alpha";
        case 3:
            return "PowerPC";
        case 5:
            return "ARM";
        case 6:
            return "ia64";
        case 9:
            return "x64";
        case 12:
            return "ARM64";
        default:
            break;
        }

        return "Unknown";
    }
#endif

#ifdef __linux__
    void ParseLinuxCpuInfoJson(DevDriver::IStructuredWriter* writer, const std::string& json)
    {
        rapidjson::Document structure;
        structure.Parse(json.c_str());

        if (structure.HasParseError() || structure.MemberCount() == 0)
        {
            return;
        }

        // Get first member's value (the array)
        const auto& object_array = structure.MemberBegin()->value;
        DD_ASSERT(object_array.IsArray());

        const char* field_key = "field";
        const char* data_key  = "data";

        std::string architecture{};
        std::string cpu_name{};
        std::string vendor_id{};
        std::string cpu_family{};
        std::string model{};
        std::string stepping{};
        uint32_t    socket_count{};
        uint32_t    core_count{};
        uint64_t    min_speed{};
        uint64_t    max_speed{};
        uint32_t    logical_core_count{};
        for (rapidjson::SizeType i = 0; i < object_array.Size(); ++i)
        {
            const auto& object = object_array[i];
            if (!object.HasMember(field_key) || !object[field_key].IsString())
            {
                continue;
            }
            std::string field_name = object[field_key].GetString();
            if (!object.HasMember(data_key) || !object[data_key].IsString())
            {
                continue;
            }
            std::string data_value = object[data_key].GetString();

            if (field_name == "Architecture:")
            {
                architecture = data_value;
            }
            if (field_name == "Model name:")
            {
                cpu_name = data_value;
            }
            if (field_name == "Vendor ID:")
            {
                vendor_id = data_value;
            }
            if (field_name == "Socket(s):")
            {
                socket_count = std::atoi(data_value.c_str());
            }
            if (field_name == "Core(s) per socket:")
            {
                core_count = std::atoi(data_value.c_str());
            }
            if (field_name == "CPU min MHz:")
            {
                min_speed = std::atoi(data_value.c_str());
            }
            if (field_name == "CPU max MHz:")
            {
                max_speed = std::atoi(data_value.c_str());
            }
            if (field_name == "CPU(s):")
            {
                logical_core_count = std::atoi(data_value.c_str());
            }
            if (field_name == "CPU family:")
            {
                cpu_family = data_value;
            }
            if (field_name == "Model:")
            {
                model = data_value;
            }
            if (field_name == "Stepping:")
            {
                stepping = data_value;
            }
        }
        uint32_t physical_core_count = socket_count * core_count;

        // Build CPU ID string to match Windows WMI Description format
        std::string cpu_id{};
        if (!architecture.empty() && !cpu_family.empty() && !model.empty() && !stepping.empty())
        {
            cpu_id = architecture + " Family " + cpu_family + " Model " + model + " Stepping " + stepping;
        }

        // Detect virtualization support from /proc/cpuinfo flags
        const char*       virt_query     = R"(grep -m1 '^flags' /proc/cpuinfo | grep -oE 'svm|vmx' | head -1 > /tmp/virt.txt)";
        const std::string virtualization = ProcessCommand(virt_query, "/tmp/virt.txt");
        const char*       virt_status    = virtualization.empty() ? "disabled" : "enabled";

        writer->BeginMap();
        {
            writer->KeyAndValue(kNodeStringArchitecture, architecture.c_str());
            writer->KeyAndValue(kNodeStringName, cpu_name.c_str());
            writer->KeyAndValue(kNodeStringCpuVendorId, vendor_id.c_str());
            writer->KeyAndValue(kNodeStringCpuPhysicalCoreCount, physical_core_count);
            writer->KeyAndValue(kNodeStringCpuLogicalCoreCount, logical_core_count);
            writer->KeyAndBeginMap(kNodeStringSpeed);
            {
                writer->KeyAndValue(kNodeStringMin, min_speed);
                writer->KeyAndValue(kNodeStringMax, max_speed);
            }
            writer->EndMap();
            writer->KeyAndValue(kNodeStringCpuId, cpu_id.c_str());
            writer->KeyAndValue(kNodeStringCpuDeviceId, "CPU0");
            writer->KeyAndValue(kNodeStringVirtualization, virt_status);
            writer->KeyAndValue(kNodeStringCpuTimeClockFreq, static_cast<uint64_t>(1000000000LL));
        }
        writer->EndMap();
    }

    void ParseLinuxCpuInfoAwk(DevDriver::IStructuredWriter* writer)
    {
        writer->BeginMap();
        {
            // Access CPU architecture
            const char* arch_query = R"(lscpu | awk 'BEGIN{FS="Architecture:"}{ print $2}' | awk 'NF > 0' | awk '{gsub(/\n/, ""); print $1}' > /tmp/arch.txt)";
            const std::string arch = ProcessCommand(arch_query, "/tmp/arch.txt");
            writer->KeyAndValue(kNodeStringArchitecture, arch.c_str());

            // Access CPU name
            const char* name_query =
                R"(lscpu | awk 'BEGIN{FS="Model name:"}{ print $2}' | awk 'NF > 0' | awk '{sub(/^[ \t]+/, ""); print $0}' > /tmp/name.txt)";
            const std::string name = ProcessCommand(name_query, "/tmp/name.txt");
            writer->KeyAndValue(kNodeStringName, name.c_str());

            // Access CPU vendor ID
            const char* vendor_query =
                R"(lscpu | awk 'BEGIN{FS="Vendor ID:"}{ print $2}' | awk 'NF > 0' | awk '{sub(/^[ \t]+/, ""); print $0}' > /tmp/vendor.txt)";
            const std::string vendor = ProcessCommand(vendor_query, "/tmp/vendor.txt");
            writer->KeyAndValue(kNodeStringCpuVendorId, vendor.c_str());

            // Access physical core count
            const char* core_count_query =
                R"(lscpu | awk 'BEGIN{FS="Core\\(s\\) per socket:"}{print $2}' | awk 'NF > 0' | awk '{sub(/^[ \t]+/, ""); print $0}' > /tmp/cores.txt)";
            const char* socket_count_query =
                R"(lscpu | awk 'BEGIN{FS="Socket\\(s\\):"}{print $2}' | awk 'NF > 0' | awk '{sub(/^[ \t]+/, ""); print $0}' > /tmp/sockets.txt)";
            const uint32_t socket_count        = std::atoi(ProcessCommand(socket_count_query, "/tmp/sockets.txt").c_str());
            const uint32_t core_count          = std::atoi(ProcessCommand(core_count_query, "/tmp/cores.txt").c_str());
            const uint32_t physical_core_count = socket_count * core_count;
            writer->KeyAndValue(kNodeStringCpuPhysicalCoreCount, physical_core_count);

            // Access logical core count
            const char* logical_core_count_command =
                R"(lscpu | awk 'BEGIN{FS="CPU\\(s\\):"}{print $2}' | awk 'NF > 0' | awk '{print $1}' | head -1 > /tmp/logical_cores.txt)";
            const uint32_t logical_core_count = std::atoi(ProcessCommand(logical_core_count_command, "/tmp/logical_cores.txt").c_str());
            writer->KeyAndValue(kNodeStringCpuLogicalCoreCount, logical_core_count);

            writer->KeyAndBeginMap(kNodeStringSpeed);
            {
                // Access min speed
                const char* min_speed_query =
                    R"(lscpu | awk 'BEGIN{FS="CPU min MHz:"}{print $2}' | awk 'NF > 0' | awk '{sub(/^[ \t]+/, ""); print $0}' > /tmp/min_speed.txt)";
                const uint64_t min_speed = std::atoi(ProcessCommand(min_speed_query, "/tmp/min_speed.txt").c_str());
                writer->KeyAndValue(kNodeStringMin, min_speed);

                // Access max speed
                const char* speed_query =
                    R"(lscpu | awk 'BEGIN{FS="CPU max MHz:"}{print $2}' | awk 'NF > 0' | awk '{sub(/^[ \t]+/, ""); print $0}' > /tmp/speed.txt)";
                const uint64_t max_speed = std::atoi(ProcessCommand(speed_query, "/tmp/speed.txt").c_str());
                writer->KeyAndValue(kNodeStringMax, max_speed);
            }
            writer->EndMap();

            // Access CPU family, model, and stepping for CPU ID
            const char* family_query =
                R"(lscpu | awk 'BEGIN{FS="CPU family:"}{print $2}' | awk 'NF > 0' | awk '{sub(/^[ \t]+/, ""); print $0}' > /tmp/family.txt)";
            const std::string family = ProcessCommand(family_query, "/tmp/family.txt");

            const char* model_query =
                R"(lscpu | awk 'BEGIN{FS="Model:"}{print $2}' | awk 'NF > 0' | awk '{sub(/^[ \t]+/, ""); print $0}' | head -1 > /tmp/model.txt)";
            const std::string model = ProcessCommand(model_query, "/tmp/model.txt");

            const char* stepping_query =
                R"(lscpu | awk 'BEGIN{FS="Stepping:"}{print $2}' | awk 'NF > 0' | awk '{sub(/^[ \t]+/, ""); print $0}' > /tmp/stepping.txt)";
            const std::string stepping = ProcessCommand(stepping_query, "/tmp/stepping.txt");

            std::string cpu_id{};
            if (!arch.empty() && !family.empty() && !model.empty() && !stepping.empty())
            {
                cpu_id = arch + " Family " + family + " Model " + model + " Stepping " + stepping;
            }
            writer->KeyAndValue(kNodeStringCpuId, cpu_id.c_str());
            writer->KeyAndValue(kNodeStringCpuDeviceId, "CPU0");

            // Detect virtualization support from /proc/cpuinfo flags
            const char*       virt_query = R"(grep -m1 '^flags' /proc/cpuinfo | grep -oE 'svm|vmx' | head -1 > /tmp/virt.txt)";
            const std::string virt       = ProcessCommand(virt_query, "/tmp/virt.txt");
            writer->KeyAndValue(kNodeStringVirtualization, virt.empty() ? "disabled" : "enabled");
        }
        writer->EndMap();
    }

    void QueryLinuxCpuInfo(DevDriver::IStructuredWriter* writer)
    {
        DD_ASSERT(writer != nullptr);

        // Access CPU information using lscpu --json
        const char* query_json    = R"(lscpu --json > /tmp/cpuinfo.json)";
        std::string file_contents = ProcessCommand(query_json, "/tmp/cpuinfo.json", false);
        if (!file_contents.empty())
        {
            ParseLinuxCpuInfoJson(writer, file_contents);
        }
        else
        {
            // If JSON representation is unavailable, fallback to using
            // direct lscpu commands with awk to parse output.
            ParseLinuxCpuInfoAwk(writer);
        }
    }
#endif

    /// @brief Finds the index of a driver entry matching the given GPU's PCI info.
    /// First tries exact PCI BDF match (checking all_pci_locations for deduplicated entries,
    /// then the primary pci fields), then falls back to matching by hardware device ID.
    /// @param [in] drivers The list of all drivers (may be deduplicated).
    /// @param [in] bus The PCI bus number to match.
    /// @param [in] device The PCI device number to match.
    /// @param [in] function The PCI function number to match.
    /// @param [in] hw_device_id The hardware device ID (from asic.ids.deviceId) for fallback matching.
    /// @return The index into the drivers vector, or -1 if not found.
    int32_t FindDriverIndex(const std::vector<DriverInfo>& drivers, uint32_t bus, uint32_t device, uint32_t function, uint32_t hw_device_id)
    {
        // First try exact PCI BDF match against all known locations
        for (size_t i = 0; i < drivers.size(); ++i)
        {
            // Check deduplicated PCI locations list
            for (const auto& loc : drivers[i].all_pci_locations)
            {
                if (loc.bus == bus && loc.device == device && loc.function == function)
                {
                    return static_cast<int32_t>(i);
                }
            }

            // Also check primary PCI fields (for non-deduplicated entries)
            if (drivers[i].has_pci_location && drivers[i].pci_bus == bus && drivers[i].pci_device == device && drivers[i].pci_function == function)
            {
                return static_cast<int32_t>(i);
            }
        }

        // Fallback: match by hardware device ID against all known IDs
        for (size_t i = 0; i < drivers.size(); ++i)
        {
            for (const auto& id : drivers[i].all_hardware_device_ids)
            {
                if (id != 0 && id == hw_device_id)
                {
                    return static_cast<int32_t>(i);
                }
            }

            // Also check primary field (for non-deduplicated entries)
            if (drivers[i].hardware_device_id != 0 && drivers[i].hardware_device_id == hw_device_id)
            {
                return static_cast<int32_t>(i);
            }
        }

        return -1;
    }

    template <GpuInfoType T>
    void WriteAllGpus(DevDriver::IStructuredWriter* writer, DevDriver::Vector<T>& gpus, const std::vector<DriverInfo>& drivers)
    {
        // PHASE 1: Always write integrated cards last in system info gpu list
        // TODO: DevDriver::Vector should be updated to support STL algorithms
        // Then we could just sort the list before writing.

        // Write GPUs using ddAmdAdapterInfo
        for (const auto& gpu : gpus)
        {
            if (IsIntegrated(gpu.asic.ids.family))
            {
                continue;
            }

            int32_t driver_idx = FindDriverIndex(drivers, gpu.pci.bus, gpu.pci.device, gpu.pci.function, gpu.asic.ids.deviceId);
            WriteSingleGpu(writer, gpu, driver_idx);
        }

        for (const auto& gpu : gpus)
        {
            if (!IsIntegrated(gpu.asic.ids.family))
            {
                continue;
            }

            int32_t driver_idx = FindDriverIndex(drivers, gpu.pci.bus, gpu.pci.device, gpu.pci.function, gpu.asic.ids.deviceId);
            WriteSingleGpu(writer, gpu, driver_idx);
        }
    }

    /// @brief Writes all available drivers from a pre-computed deduplicated list.
    void WriteAllDriversInfoFromList(DevDriver::IStructuredWriter* writer, const std::vector<DriverInfo>& all_drivers)
    {
#ifdef _WIN32
        writer->KeyAndBeginList(kNodeStringDrivers);
        {
            for (const auto& driver : all_drivers)
            {
                writer->BeginMap();
                {
                    writer->KeyAndValue(kNodeStringName, driver.name.c_str());
                    writer->KeyAndValue(kNodeStringDescription, driver.description.c_str());
                    writer->KeyAndValue(kNodeStringDriverPackagingVersion, driver.packaging_version.c_str());

                    if (auto parsed_date = ParsePackagingDate(driver.packaging_version); parsed_date.has_value())
                    {
                        writer->KeyAndValue(kNodeStringDriverPackagingDate, parsed_date.value().c_str());
                    }

                    writer->KeyAndValue(kNodeStringDriverSoftwareVersion, driver.software_version.c_str());

                    // PCI location for mapping to GPU entries (only when resolved)
                    if (driver.has_pci_location)
                    {
                        writer->KeyAndBeginMap(kNodeStringPci);
                        {
                            writer->KeyAndValue(kNodeStringPciBus, driver.pci_bus);
                            writer->KeyAndValue(kNodeStringDevice, driver.pci_device);
                            writer->KeyAndValue(kNodeStringPciFunction, driver.pci_function);
                        }
                        writer->EndMap();
                    }

                    // All PCI locations for this driver — one entry per GPU sharing this driver installation
                    if (!driver.all_pci_locations.empty())
                    {
                        writer->KeyAndBeginList(kNodeStringPcis);
                        {
                            for (const auto& loc : driver.all_pci_locations)
                            {
                                writer->BeginMap();
                                {
                                    writer->KeyAndValue(kNodeStringPciBus, loc.bus);
                                    writer->KeyAndValue(kNodeStringDevice, loc.device);
                                    writer->KeyAndValue(kNodeStringPciFunction, loc.function);
                                }
                                writer->EndMap();
                            }
                        }
                        writer->EndList();
                    }
                }
                writer->EndMap();
            }
        }
        writer->EndList();
#endif
    }

    /// @brief Writes GPU info using a pre-computed deduplicated drivers list.
    void WriteGpuInfoFromList(DevDriver::IStructuredWriter* writer, const std::vector<DriverInfo>& all_drivers)
    {
        writer->KeyAndBeginList(kNodeStringGpus);
        {
            DevDriver::Vector<DevDriver::ddAmdAdapterInfo> adapters(DevDriver::Platform::GenericAllocCb);
            const DevDriver::Result                        result = QueryAdapterInfo(adapters);
            if (adapters.Size() > 0)
            {
                WriteAllGpus(writer, adapters, all_drivers);
            }

            if (result != DevDriver::Result::Success)
            {
                DevDriver::Vector<DevDriver::AmdGpuInfo> gpus(DevDriver::Platform::GenericAllocCb);
                DD_UNHANDLED_RESULT(QueryGpuInfo(DevDriver::Platform::GenericAllocCb, &gpus));

                // Only write GPU info if not already in adapter list
                for (auto& gpu : gpus)
                {
                    const auto gpu_id = (gpu.pci.bus << 16) | (gpu.pci.device << 8) | gpu.pci.function;
                    bool       found  = false;
                    for (const auto& adapter : adapters)
                    {
                        if (const auto adapter_id = (adapter.pci.bus << 16) | (adapter.pci.device << 8) | adapter.pci.function; gpu_id == adapter_id)
                        {
                            found = true;
                            break;
                        }
                    }

                    if (!found)
                    {
                        int32_t driver_idx = FindDriverIndex(all_drivers, gpu.pci.bus, gpu.pci.device, gpu.pci.function, gpu.asic.ids.deviceId);
                        WriteSingleGpu(writer, gpu, driver_idx);
                    }
                }
            }
        }
        writer->EndList();
    }
}  // namespace

namespace system_info_utils
{
    void SystemInfoWriter::WriteDevDriverInfo(DevDriver::IStructuredWriter* writer)
    {
        DD_ASSERT(writer != nullptr);

        writer->KeyAndBeginMap(kNodeStringDevDriver);
        {
            writer->KeyAndBeginMap(kNodeStringVersion);
            {
                writer->KeyAndValue(kNodeStringMajor, GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION);
            }
            writer->EndMap();
        }
        writer->EndMap();
    }

    void SystemInfoWriter::WriteSystemInfo(DevDriver::IStructuredWriter* writer)
    {
        // Write version information
        writer->KeyAndBeginMap(kNodeStringVersion);
        writer->KeyAndValue(kNodeStringMajor, SYSTEM_INFO_MAJOR_VERSION);
        writer->KeyAndValue(kNodeStringMinor, SYSTEM_INFO_MINOR_VERSION);
        writer->EndMap();

        // Developer driver information
        WriteDevDriverInfo(writer);

        // Operating System information
        WriteOsInfo(writer);

        // Driver information
        WriteDriverInfo(writer);

        // Compute the deduplicated driver list once for both drivers and GPU sections
        const auto all_drivers = DeduplicateDrivers(FindAllDrivers());

        // All available drivers per device
        WriteAllDriversInfoFromList(writer, all_drivers);

        // CPU information
        WriteCpuInfo(writer);

        // GPU information
        WriteGpuInfoFromList(writer, all_drivers);
    }

    void SystemInfoWriter::WriteDriverInfo(DevDriver::IStructuredWriter* writer)
    {
        DD_ASSERT(writer != nullptr);

        writer->KeyAndBeginMap(kNodeStringDriver);
        {
            const std::string name = QueryDriverName();
            // Driver name
            writer->KeyAndValue(kNodeStringName, name.c_str());

            // Driver description
            writer->KeyAndValue(kNodeStringDescription, QueryDriverDescription().c_str());

            // Driver packaging version
            const std::string packaging_version = QueryDriverPackagingVersion();
            writer->KeyAndValue(kNodeStringDriverPackagingVersion, packaging_version.c_str());

            // TODO: Support packaging date on Linux
            // Driver packaging date
            auto parsed_date = ParsePackagingDate(packaging_version);
            if (parsed_date.has_value())
            {
                writer->KeyAndValue(kNodeStringDriverPackagingDate, parsed_date.value().c_str());
            }

            // Driver software version
            const auto software_version = QueryDriverSoftwareVersion();
            writer->KeyAndValue(kNodeStringDriverSoftwareVersion, software_version.c_str());
        }
        writer->EndMap();
    }

    void SystemInfoWriter::WriteAllDriversInfo(DevDriver::IStructuredWriter* writer)
    {
        DD_ASSERT(writer != nullptr);
        // When called via WriteSystemInfo, the pre-computed list from WriteAllDriversInfoFromList is used.
        // This standalone overload is provided for callers that invoke WriteAllDriversInfo independently.
        WriteAllDriversInfoFromList(writer, DeduplicateDrivers(FindAllDrivers()));
    }

    void SystemInfoWriter::WriteOsInfo(DevDriver::IStructuredWriter* writer)
    {
        DD_ASSERT(writer != nullptr);

        DevDriver::Platform::OsInfo os_info{};
        DD_UNHANDLED_RESULT(DevDriver::Platform::QueryOsInfo(&os_info));

        std::string memory_type_name;
#ifdef _WIN32
        auto physical_memory_result = Wmi::retrieveWmi<Wmi::Win32_PhysicalMemory>("SMBIOSMemoryType");
        if (physical_memory_result)
        {
            memory_type_name = physical_memory_result->MemoryTypeName;
        }
#endif

        writer->KeyAndBeginMap(kNodeStringOs);
        {
            writer->KeyAndValue(kNodeStringType, os_info.type);
            writer->KeyAndValue(kNodeStringName, os_info.name);
            writer->KeyAndValue(kNodeStringDescription, os_info.description);
            writer->KeyAndValue(kNodeStringHostName, os_info.hostname);
            writer->KeyAndBeginMap(kNodeStringMemory);
            {
                writer->KeyAndValue(kNodeStringMemoryPhysical, os_info.physMemory);
                writer->KeyAndValue(kNodeStringMemorySwap, os_info.swapMemory);
                writer->KeyAndValue(kNodeStringName, memory_type_name.c_str());
            }
            writer->EndMap();

            // Write platform specific configuration
            writer->KeyAndBeginMap(kNodeStringConfig);
            {
                WritePlatformConfig(writer);
            }
            writer->EndMap();
        }
        writer->EndMap();
    }

    void SystemInfoWriter::WriteCpuInfo(DevDriver::IStructuredWriter* writer)
    {
        DD_ASSERT(writer != nullptr);

        writer->KeyAndBeginList(kNodeStringCpus);
        {
#ifdef _WIN32
            LARGE_INTEGER freq;
            QueryPerformanceFrequency(&freq);

            std::vector<Wmi::Win32_Processor> processors;
            auto                              wmi_result =
                Wmi::retrieveAllWmi<Wmi::Win32_Processor>(processors,
                                                          "Name, VirtualizationFirmwareEnabled, DeviceID, Description, Architecture, MaxClockSpeed, "
                                                          "NumberOfCores, NumberOfLogicalProcessors, Manufacturer");
            if (wmi_result)
            {
                for (auto& processor : processors)
                {
                    writer->BeginMap();

                    writer->KeyAndValue(kNodeStringName, processor.Name.c_str());
                    writer->KeyAndValue(kNodeStringVirtualization, processor.VirtualizationFirmwareEnabled ? "enabled" : "disabled");
                    writer->KeyAndValue(kNodeStringCpuDeviceId, processor.DeviceID.c_str());
                    writer->KeyAndValue(kNodeStringCpuId, processor.Description.c_str());
                    writer->KeyAndValue(kNodeStringArchitecture, ArchitectureFromWmiProcessorId(processor.Architecture).c_str());
                    writer->KeyAndBeginMap(kNodeStringSpeed);
                    {
                        writer->KeyAndValue(kNodeStringMax, processor.MaxClockSpeed);
                    }
                    writer->EndMap();
                    writer->KeyAndValue(kNodeStringCpuPhysicalCoreCount, processor.NumberOfCores);
                    writer->KeyAndValue(kNodeStringCpuLogicalCoreCount, processor.NumberOfLogicalProcessors);
                    writer->KeyAndValue(kNodeStringCpuVendorId, processor.Manufacturer.c_str());
                    writer->KeyAndValue(kNodeStringCpuTimeClockFreq, freq.QuadPart);

                    writer->EndMap();
                }
            }
#elif defined(__linux__)
            // TODO: Support multiple CPU on Linux
            QueryLinuxCpuInfo(writer);
#else
            // no-op on other platforms
#endif
        }
        writer->EndList();
    }

    void SystemInfoWriter::WriteGpuInfo(DevDriver::IStructuredWriter* writer)
    {
        DD_ASSERT(writer != nullptr);
        // When called via WriteSystemInfo, the pre-computed list from WriteGpuInfoFromList is used.
        // This standalone overload is provided for callers that invoke WriteGpuInfo independently.
        WriteGpuInfoFromList(writer, DeduplicateDrivers(FindAllDrivers()));
    }

#ifdef SYSTEM_INFO_ENABLE_RDF
    DD_RESULT SystemInfoWriter::WriteRdfChunk(rdfChunkFileWriter* file_writer, const std::string& json)
    {
        DD_ASSERT(file_writer != nullptr);

        // Ensure we are writing only the system info chunk data
        std::string chunk_data = SystemInfoReader::Parse(json);
        const auto  chunk_size = static_cast<int64_t>(chunk_data.size());

        // Write chunk to file
        rdfChunkCreateInfo create_info{};
        create_info.version    = kSystemInfoChunkVersion;
        create_info.headerSize = 0;
        create_info.pHeader    = nullptr;
        size_t identifier_len  = platform::Strnlen_s(kSystemInfoChunkIdentifier, sizeof(create_info.identifier));
        platform::Memcpy_s(create_info.identifier, sizeof(create_info.identifier), kSystemInfoChunkIdentifier, identifier_len);

        int index{};
        int rdf_result = rdfChunkFileWriterWriteChunk(file_writer, &create_info, chunk_size, chunk_data.data(), &index);
        if (rdf_result != rdfResultOk)
        {
            return DD_RESULT_DD_GENERIC_UNKNOWN;
        }

        return DD_RESULT_SUCCESS;
    };
#endif

}  // namespace system_info_utils
