cmake_minimum_required(VERSION 3.15)

# Main project for the classifier daemon
project(classifier_daemon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_compile_options(-Wall -Wextra)

# Include directories for the entire project
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Define the parser library
set(PARSER_LIB_SOURCES
    src/process_scanner.cpp
    src/proc_tokenizer.cpp
    src/proc_metrics.cpp
)

add_library(parser SHARED ${PARSER_LIB_SOURCES})

set_target_properties(parser PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
)

target_include_directories(parser
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Define the classifier executable
add_executable(classifier src/classifier.cpp)

# Link classifier with parser and dl
target_link_libraries(classifier dl parser)

# Installation rules
install(TARGETS parser DESTINATION lib)
install(TARGETS classifier RUNTIME DESTINATION bin)
install(FILES Configs/IgnoreTokens.txt DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/classifier)
install(FILES ${CMAKE_SOURCE_DIR}/Services/classifier.service DESTINATION ${CMAKE_INSTALL_LIBDIR}/systemd/system/)
