cmake_minimum_required(VERSION 3.6)

set(CMAKE_CXX_STANDARD 11 LANGUAGES CXX)

project("URM-Extensions")
option(BUILD_SHARED_LIBS "Build shared libraries" ON)

include(GNUInstallDirs)

# Install the extension "UrmPlugin.so" lib
file(GLOB SOURCES "Extensions/*.cpp")
add_library(UrmPlugin ${SOURCES})
set_target_properties(UrmPlugin PROPERTIES VERSION 1.0.0 SOVERSION 1)
target_link_libraries(UrmPlugin UrmExtAPIs UrmAuxUtils)
# install to standard /usr/lib or /lib
install(TARGETS UrmPlugin DESTINATION ${CMAKE_INSTALL_LIBDIR}/urm/)

# Install the configs to /etc/urm/custom
file(GLOB pluginConfigs "${CMAKE_CURRENT_SOURCE_DIR}/Configs/*.yaml")
install(
    FILES ${pluginConfigs}
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/urm/target
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)

# Install target-configs to /etc/urm/target/
file(GLOB targetConfigs "${CMAKE_CURRENT_SOURCE_DIR}/Configs/target-specific/*")
install(
    DIRECTORY ${targetConfigs}
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/urm/target
)
