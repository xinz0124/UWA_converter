cmake_minimum_required(VERSION 3.16)

project(gs_format_converter LANGUAGES C CXX)

# cJSON is versioned with this project as a Git submodule.
set(ENABLE_CJSON_TEST OFF CACHE BOOL "Disable cJSON tests" FORCE)
set(ENABLE_CJSON_UTILS OFF CACHE BOOL "Disable cJSON utilities" FORCE)
set(ENABLE_TARGET_EXPORT OFF CACHE BOOL "Disable cJSON install exports" FORCE)
set(ENABLE_CUSTOM_COMPILER_FLAGS OFF CACHE BOOL "Use project compiler flags" FORCE)
# cJSON 1.7.19 declares compatibility with an older CMake policy baseline.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
add_subdirectory(thirdparty/cJSON EXCLUDE_FROM_ALL)

add_executable(gs_format_converter gs_format_converter.cpp)
target_compile_features(gs_format_converter PRIVATE cxx_std_17)
target_compile_options(gs_format_converter PRIVATE -Wall -Wextra)
target_include_directories(gs_format_converter PRIVATE thirdparty/cJSON)
target_link_libraries(gs_format_converter PRIVATE cjson)
