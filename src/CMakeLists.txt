
cmake_minimum_required(VERSION 3.10)
project(classifier_daemon)

set(CMAKE_CXX_STANDARD 11)

add_subdirectory(proc_parser)

add_executable(classifier classifier.cpp)

# Link against the dynamic loading library
#target_link_libraries(classifier dl ClientAPIs parser)
target_link_libraries(classifier dl parser)

# Install rule to place the binary in /usr/bin/
install(TARGETS classifier RUNTIME DESTINATION bin)
