cmake_minimum_required(VERSION 3.16)

project(gs_format_converter LANGUAGES CXX)

find_path(JSONC_INCLUDE_DIR json-c/json.h
    HINTS /opt/homebrew/include /usr/local/include)
find_library(JSONC_LIBRARY NAMES json-c
    HINTS /opt/homebrew/lib /usr/local/lib)

if(NOT JSONC_INCLUDE_DIR OR NOT JSONC_LIBRARY)
    message(FATAL_ERROR "json-c was not found; install it before building")
endif()

add_executable(gs_format_converter gs_format_converter.cpp)
target_compile_features(gs_format_converter PRIVATE cxx_std_17)
target_compile_options(gs_format_converter PRIVATE -Wall -Wextra)
target_include_directories(gs_format_converter PRIVATE ${JSONC_INCLUDE_DIR})
target_link_libraries(gs_format_converter PRIVATE ${JSONC_LIBRARY})
