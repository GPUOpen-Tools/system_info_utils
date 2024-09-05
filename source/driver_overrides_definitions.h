//=============================================================================
// Copyright (c) 2024 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief Definitions for Driver Override RDF chunk.
//=============================================================================

#ifndef DRIVER_OVERRIDES_UTILS_SOURCE_DRIVER_OVERRIDES_DEFINITIONS_H_
#define DRIVER_OVERRIDES_UTILS_SOURCE_DRIVER_OVERRIDES_DEFINITIONS_H_

namespace driver_overrides_utils
{
    static constexpr const char* kDriverOverridesChunkIdentifier = "DriverOverrides";
    static constexpr uint32_t    kDriverOverridesChunkVersion    = 3;                             ///< Current Driver Override chunk version.
    static constexpr uint32_t    kDriverOverridesChunkVersionMin = 2;                             ///< Minimum supported chunk version.
    static constexpr uint32_t    kDriverOverridesChunkVersionMax = kDriverOverridesChunkVersion;  ///< Maximum supported chunk version.

    static constexpr const char* kDriverOverridesmiscellaneousStructure = "Misc.";  ///< String used for unnamed structures.

    static constexpr const char* kNodeStringIsDriverExperiments = "IsDriverExperiments";
    static constexpr const char* kNodeStringComponents          = "Components";
    static constexpr const char* kNodeStringComponent           = "Component";
    static constexpr const char* kNodeStringStructures          = "Structures";
    static constexpr const char* kNodeStringStructure           = "Structure";
    static constexpr const char* kNodeStringSettingName         = "SettingName";
    static constexpr const char* kNodeStringCurrent             = "Current";
    static constexpr const char* kNodeStringValue               = "Value";
    static constexpr const char* kNodeStringUserOverride        = "UserOverride";
    static constexpr const char* kNodeStringDescription         = "Description";
    static constexpr const char* kNodeStringSupported           = "Supported";

}  // namespace driver_overrides_utils
#endif  // DRIVER_OVERRIDES_UTILS_SOURCE_DRIVER_OVERRIDES_DEFINITIONS_H_
