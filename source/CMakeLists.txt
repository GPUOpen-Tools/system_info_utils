#######################################################################################################################
### Copyright (c) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.
#######################################################################################################################

project(system_info)

add_library(${PROJECT_NAME} STATIC
        definitions.h
        system_info_reader.h
        system_info_reader.cpp
        driver_overrides_definitions.h
        driver_overrides_reader.h
        driver_overrides_reader.cpp)
set_target_properties(${PROJECT_NAME} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON)

if (SYSTEM_INFO_BUILD_RDF_INTERFACES)
    target_compile_definitions(${PROJECT_NAME} PUBLIC -DSYSTEM_INFO_ENABLE_RDF)
    target_compile_definitions(${PROJECT_NAME} PUBLIC -DDRIVER_OVERRIDES_ENABLE_RDF)
    target_link_libraries(${PROJECT_NAME} PUBLIC amdrdf)
endif ()

# Needed for ETW reporting
if (WIN32)
    target_link_libraries(${PROJECT_NAME} PUBLIC Netapi32 Secur32)
endif()

target_include_directories(${PROJECT_NAME}
        PUBLIC . ./third_party/json/include/)

if (SYSTEM_INFO_ENABLE_PACKAGING)
    install(TARGETS ${PROJECT_NAME}
            ARCHIVE DESTINATION bin COMPONENT system_info_api
            RUNTIME DESTINATION bin COMPONENT system_info_api
            LIBRARY DESTINATION lib COMPONENT system_info_api)
    install(FILES system_info_reader.h DESTINATION inc COMPONENT system_info_api)
endif ()

if (DRIVER_OVERRIDES_ENABLE_PACKAGING)
	install(TARGETS ${PROJECT_NAME}
			ARCHIVE DESTINATION bin COMPONENT driver_overrides_api
			RUNTIME DESTINATION bin COMPONENT driver_overrides_api
			LIBRARY DESTINATION lib COMPONENT driver_overrides_api)
	install(FILES driver_overrides_reader.h DESTINATION inc COMPONENT driver_overrides_api)
endif ()
