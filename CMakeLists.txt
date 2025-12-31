cmake_minimum_required(VERSION 3.6)

set(CMAKE_CXX_STANDARD 17 LANGUAGES CXX)

project("URM-Extensions")

# Install the extension "RestunePlugin.so" lib
file(GLOB SOURCES "src/*.cpp")
add_library(RestunePlugin ${SOURCES})
set_target_properties(RestunePlugin PROPERTIES VERSION 1.0.0 SOVERSION 1)
target_link_libraries(RestunePlugin UrmExtAPIs UrmAuxUtils)
# install to standard /usr/lib or /lib
install(TARGETS RestunePlugin DESTINATION ${CMAKE_INSTALL_LIBDIR})
