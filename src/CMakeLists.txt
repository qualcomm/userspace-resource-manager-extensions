
cmake_minimum_required(VERSION 3.10)
project(urm_daemon)

set(CMAKE_CXX_STANDARD 11)

add_executable(urm_daemon urm.cpp)

# Link against the dynamic loading library
target_link_libraries(urm_daemon dl)

# Install rule to place the binary in /usr/bin/
install(TARGETS urm_daemon DESTINATION /usr/bin)
