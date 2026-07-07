cmake_minimum_required(VERSION 3.6)

set(CMAKE_CXX_STANDARD 11 LANGUAGES CXX)
add_compile_options(-Wall -Wextra)

project("URM-Extensions")
option(BUILD_SHARED_LIBS "Build shared libraries" ON)

include(GNUInstallDirs)

# Install the extension "UrmPlugin.so" lib
file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Extensions/*.cpp")
add_library(UrmPlugin ${SOURCES})
set_target_properties(UrmPlugin PROPERTIES VERSION 1.0.0 SOVERSION 1)
target_link_libraries(UrmPlugin UrmExtAPIs RestuneCore UrmAuxUtils)
target_include_directories(UrmPlugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Extensions/Include)
# install to standard /usr/lib or /lib
install(TARGETS UrmPlugin DESTINATION ${CMAKE_INSTALL_LIBDIR}/urm/)

# Install the extension "UrmRelayFeat.so" lib
file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Relay/*.cpp")
add_library(UrmRelayFeat ${SOURCES})
set_target_properties(UrmRelayFeat PROPERTIES VERSION 1.0.0 SOVERSION 1)
target_link_libraries(UrmRelayFeat UrmExtAPIs RestuneCore UrmAuxUtils)
target_include_directories(UrmRelayFeat PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Relay/Include)
# install to standard /usr/lib or /lib
install(TARGETS UrmRelayFeat DESTINATION ${CMAKE_INSTALL_LIBDIR}/)

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

# Install post_boot scripts to /etc/urm/initscripts/post_boot/
install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/initscripts/post_boot/
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/urm/initscripts/post_boot
    FILES_MATCHING
    PATTERN "*.sh"
    PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE
)
