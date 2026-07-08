// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Platform-specific utility functions for safe string and memory operations

#ifndef SYSTEM_INFO_UTILS_SOURCE_PLATFORM_UTILS_H_
#define SYSTEM_INFO_UTILS_SOURCE_PLATFORM_UTILS_H_

#include <cstring>

#ifdef _WIN32
#include <string.h>
#endif

namespace system_info_utils
{
    namespace platform
    {
        /// @brief Safe string length function
        /// @param str The null-terminated string
        /// @param max_count Maximum number of characters to examine
        /// @return The length of the string in bytes, excluding the null terminator
        inline size_t Strnlen_s(const char* str, size_t max_count)
        {
#ifdef _WIN32
            return strnlen_s(str, max_count);
#else
            return strnlen(str, max_count);
#endif
        }

        /// @brief Safe memory copy function
        /// @param dest Pointer to the destination buffer
        /// @param dest_size Size of the destination buffer in bytes
        /// @param src Pointer to the source buffer
        /// @param count Number of bytes to copy
        /// @return 0 on success, non-zero on error
        inline int Memcpy_s(void* dest, size_t dest_size, const void* src, size_t count)
        {
#ifdef _WIN32
            return memcpy_s(dest, dest_size, src, count);
#else
            if (dest == nullptr || src == nullptr || count > dest_size)
            {
                return -1;
            }
            memcpy(dest, src, count);
            return 0;
#endif
        }

    }  // namespace platform
}  // namespace system_info_utils

#endif
