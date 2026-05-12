project(RapidJSON VERSION 1.1.0 LANGUAGES CXX)

add_library(rapidjson INTERFACE)
target_include_directories(rapidjson SYSTEM INTERFACE "include")

# C++ STL defines an interface for "iterator" objects. It also supplies a class,
# std::iterator that includes all of these methods to implement. This has been
# deprecated in C++17, in favor of using templates instead of inheritance.
# We disable this because we don't want to modify third party code if it can be helped.
#
# Marking includes as SYSTEM suppresses warnings from rapidjson headers on
# GCC/Clang (-isystem) and MSVC 19.29+ (/external:W0 + /external:I).
#
# See this blog for more details about these warnings:
#       https://mariusbancila.ro/blog/2018/07/09/visual-studio-warning-macros-for-c17-deprecated-features/
target_compile_definitions(rapidjson INTERFACE "_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING")

if(MSVC)
    # Tell MSVC to suppress warnings from headers marked as SYSTEM/external.
    # /external:W0 is supported in MSVC 19.29+ (VS 2019 16.10+).
    target_compile_options(rapidjson INTERFACE /external:W0)
endif()

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 8.0)
        target_compile_options(rapidjson INTERFACE -Wno-class-memaccess)
    endif()
endif()
