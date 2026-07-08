// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief System info writer definition

#ifndef SYSTEM_INFO_UTILS_SOURCE_SYSTEM_INFO_WRITER_H_
#define SYSTEM_INFO_UTILS_SOURCE_SYSTEM_INFO_WRITER_H_

#include <ddApi.h>
#include <ddUriInterface.h>

#ifdef SYSTEM_INFO_ENABLE_RDF
#include <amdrdf.h>
#endif

namespace system_info_utils
{
    /// @brief JSON writer for system info
    class SystemInfoWriter
    {
    public:
        /// @brief Default constructor
        SystemInfoWriter() = delete;

        /// @brief Default destructor
        ~SystemInfoWriter() = delete;

        /// @brief Writes system information JSON to structured writer
        /// @param [in] writer The structured writer
        static void WriteSystemInfo(DevDriver::IStructuredWriter* writer);

#ifdef SYSTEM_INFO_ENABLE_RDF
        /// @brief Writes system info chunk to RDF file
        /// @param [in] file_writer The RDF file writer
        /// @param [in] json The system info JSON
        /// @return DD_RESULT_SUCCESS or error code
        static DD_RESULT WriteRdfChunk(rdfChunkFileWriter* file_writer, const std::string& json);
#endif

    private:
        /// @brief Writes developer driver information to structured writer
        /// @param [in] writer The structured writer
        static void WriteDevDriverInfo(DevDriver::IStructuredWriter* writer);

        /// @brief Writes driver information to structured writer
        /// @param [in] writer The structured writer
        static void WriteDriverInfo(DevDriver::IStructuredWriter* writer);

        /// @brief Writes all available drivers information to structured writer
        /// @param [in] writer The structured writer
        static void WriteAllDriversInfo(DevDriver::IStructuredWriter* writer);

        /// @brief Writes operating-system information to structured Writer
        /// @param [in] writer The structured writer
        static void WriteOsInfo(DevDriver::IStructuredWriter* writer);

        /// @brief Writes CPU information to structured writer
        /// @param [in] writer The structured writer
        static void WriteCpuInfo(DevDriver::IStructuredWriter* writer);

        /// @brief Writes GPU information to structured writer
        /// @param [in] writer The structured writer
        static void WriteGpuInfo(DevDriver::IStructuredWriter* writer);
    };

}  // namespace system_info_utils

#endif
