cmake_minimum_required(VERSION 3.6)
set(CMAKE_CXX_STANDARD 11 LANGUAGES CXX)
project("URM-Extensions")
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
include(GNUInstallDirs)

# URM headers path - passed from BitBake
set(URM_SOURCE_PATH "" CACHE PATH "Path to URM source directory")

if(NOT URM_SOURCE_PATH)
    message(FATAL_ERROR "URM_SOURCE_PATH must be set to the URM source directory")
endif()

# Create Urm/ directory and copy all necessary headers
set(URM_HEADERS_DIR "${URM_SOURCE_PATH}/extensions/Include")
set(URM_HEADERS_DEST "${URM_HEADERS_DIR}/Urm")

file(MAKE_DIRECTORY ${URM_HEADERS_DEST})

# Copy headers from extensions/Include
file(GLOB URM_EXT_HEADERS "${URM_HEADERS_DIR}/*.h")
file(COPY ${URM_EXT_HEADERS} DESTINATION ${URM_HEADERS_DEST})

# Copy headers from modula
file(GLOB_RECURSE URM_MODULA_HEADERS "${URM_SOURCE_PATH}/modula/*.h")
file(COPY ${URM_MODULA_HEADERS} DESTINATION ${URM_HEADERS_DEST})

# Copy headers from resource-tuner
file(GLOB_RECURSE URM_RESTUNE_HEADERS "${URM_SOURCE_PATH}/resource-tuner/*.h")
file(COPY ${URM_RESTUNE_HEADERS} DESTINATION ${URM_HEADERS_DEST})

# Add URM headers include path
include_directories(${URM_HEADERS_DIR})

# Install the extension "UrmPlugin.so" lib
file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Extensions/*.cpp")
add_library(UrmPlugin ${SOURCES})
set_target_properties(UrmPlugin PROPERTIES VERSION 1.0.0 SOVERSION 1)

# Set linker flags to ensure GNU_HASH
set_target_properties(UrmPlugin PROPERTIES
    LINK_FLAGS "-Wl,-O1 -Wl,--hash-style=gnu -Wl,--as-needed"
)

target_link_libraries(UrmPlugin UrmExtAPIs RestuneCore UrmAuxUtils)
target_include_directories(UrmPlugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Extensions/Include)

# Add explicit permissions to library install
install(
    TARGETS UrmPlugin
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/urm/
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

# Install the configs to /etc/urm/target
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
    FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)