#######################################################################################################################
### Copyright (c) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.
#######################################################################################################################

cmake_minimum_required(VERSION 3.16)
project(system_info_utils)

# Define version information
set(SYSTEM_INFO_MAJOR_VERSION 2)
set(SYSTEM_INFO_MINOR_VERSION 0)
if (NOT SYSTEM_INFO_PATCH_NUMBER)
    set(SYSTEM_INFO_PATCH_NUMBER 0)
endif ()
if (NOT SYSTEM_INFO_BUILD_NUMBER)
    set(SYSTEM_INFO_BUILD_NUMBER 0)
endif ()
string(TIMESTAMP DATE "\"%m/%d/%Y\"")
string(TIMESTAMP YEAR "\"%Y\"")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Buildinfo.properties.in" "${CMAKE_CURRENT_SOURCE_DIR}/Buildinfo.properties")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/source/version.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/source/version.h")

option(SYSTEM_INFO_BUILD_RDF_INTERFACES "Build with rdf interfaces for read and write." OFF)

if (WIN32)
    # wmi library
    add_subdirectory(source/third_party/wmi)

    if (MSVC)
        target_compile_options(wmi
            PRIVATE
                "/wd4244" # disable conversion warnings
        )
    endif()
endif ()

# System Info reader/writer
add_subdirectory(source)

if (SYSTEM_INFO_ENABLE_PACKAGING)
    # Packaging

    set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
    set(CPACK_COMPONENTS_GROUPING IGNORE)

    if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
        set(CPACK_ARCHIVE_SYSTEM_INFO_API_FILE_NAME "system_info_debug-${SYSTEM_INFO_MAJOR_VERSION}.${SYSTEM_INFO_MINOR_VERSION}.${SYSTEM_INFO_PATCH_NUMBER}.${SYSTEM_INFO_BUILD_NUMBER}")
    else ()
        set(CPACK_ARCHIVE_SYSTEM_INFO_API_FILE_NAME "system_info_${SYSTEM_INFO_MAJOR_VERSION}.${SYSTEM_INFO_MINOR_VERSION}.${SYSTEM_INFO_PATCH_NUMBER}.${SYSTEM_INFO_BUILD_NUMBER}")
    endif ()

    include(CPack)

    cpack_add_component(system_info_api
            DISPLAY_NAME "System Info API"
            DESCRIPTION "System info API for use by applications needing to parse system info chunks.")

    cpack_add_component(driver_overrides_api
            DISPLAY_NAME "Driver Overrides API"
            DESCRIPTION "Driver Overrides API for use by applications needing to parse DriverOverrides chunk.")
endif ()
