cmake_minimum_required(VERSION 3.11)
project (Wmi)
set (CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -c -O2 -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64")

option(BUILD_TESTS "Build tests" OFF)

add_library(wmi STATIC
        src/wmi.cpp
        src/wmiresult.cpp)
target_include_directories(wmi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(wmi ole32 oleaut32 wbemuuid )

if (BUILD_TESTS)
add_executable(test_wmi src/test.cpp)
target_link_libraries(test_wmi wmi ole32 oleaut32 wbemuuid)
endif()


