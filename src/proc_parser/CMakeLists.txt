cmake_minimum_required(VERSION 3.15)

# Project
project(parserlib LANGUAGES CXX)

# Match your original flags/requirements
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)  # equivalent to -fPIC for all targets

# Add warnings similar to your Makefile
add_compile_options(-Wall -Wextra)

# Where to look for headers (adjust if yours are elsewhere)
# If headers are in project root, you can use ${CMAKE_CURRENT_SOURCE_DIR} instead of include
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Sources to merge into a single library
set(PARSER_LIB_SOURCES
    scanner.cpp
    parser.cpp
    proc_stats.cpp
)

# Build the shared library named 'parser' -> will emit libparser.so on *nix
add_library(parser SHARED ${PARSER_LIB_SOURCES})

# Optionally set version info for the .so (can be omitted)
set_target_properties(parser PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
)

# If other targets need to include this library's headers
target_include_directories(parser
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

install(TARGETS parser DESTINATION lib)
install(FILES IgnoreTokens.txt DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/classifier)
