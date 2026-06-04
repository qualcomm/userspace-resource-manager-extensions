# 2. Build and Install Guide

## Prerequisites

| Requirement | Details |
|-------------|---------|
| OS | Ubuntu 20.04+ (or compatible Linux) |
| CMake | >= 3.6 |
| C++ Compiler | GCC or Clang with C++11 support |
| URM Core | Built and installed first (see Step 1) |
| URM Libraries | UrmExtAPIs, RestuneCore, UrmAuxUtils |
| URM Headers | Urm/Extensions.h, Urm/UrmPlatformAL.h |

---

## Step 1: Build and Install URM Core

The extensions depend on the URM core libraries and headers.
Follow the instructions at:
https://github.com/qualcomm/userspace-resource-manager#build-and-install-instructions

After this step, the following must be available on your system:
- Libraries: libUrmExtAPIs.so, libRestuneCore.so, libUrmAuxUtils.so
- Headers: Urm/Extensions.h, Urm/UrmPlatformAL.h, Urm/UrmAPIs.h

---

## Step 2: Clone the Extensions Repository

    git clone https://github.com/rajulup/userspace-resource-manager-extensions.git
    cd userspace-resource-manager-extensions

---

## Step 3: Build the Extension Plugin

    # Create a clean build directory
    rm -rf build && mkdir build && cd build

    # Configure with CMake
    cmake .. -DCMAKE_INSTALL_PREFIX=/

    # Build
    cmake --build .

### What Gets Built

The CMake build compiles all .cpp files under Extensions/ into a single shared library:

| Output | Description |
|--------|-------------|
| UrmPlugin.so | The extension plugin loaded by URM at startup |

Source files compiled:
- Extensions/PostProcessingBlock.cpp - GStreamer workload post-processor
- Extensions/CyclicTestsExt.cpp - RT benchmark (cyclictest) extension

### CMakeLists.txt Summary

The build system:
1. Globs all *.cpp files from Extensions/ into a single UrmPlugin shared library target.
2. Links against UrmExtAPIs, RestuneCore, and UrmAuxUtils.
3. Sets VERSION 1.0.0 and SOVERSION 1 on the library.
4. Adds -Wall -Wextra compiler warnings.
5. Installs the library to CMAKE_INSTALL_LIBDIR/urm/ (typically /usr/lib/urm/).

---

## Step 4: Install

    sudo cmake --install .

### Install Manifest

| Source | Installed To | Permissions |
|--------|-------------|-------------|
| UrmPlugin.so | /usr/lib/urm/ | Standard library |
| Configs/*.yaml | /etc/urm/target/ | 644 |
| Configs/target-specific/qcm6490/ | /etc/urm/target/qcm6490/ | 644 |
| Configs/target-specific/qcs8300/ | /etc/urm/target/qcs8300/ | 644 |
| Configs/target-specific/qcs9100/ | /etc/urm/target/qcs9100/ | 644 |
| initscripts/post_boot/*.sh | /etc/urm/initscripts/post_boot/ | 755 |

Note: The library installs to CMAKE_INSTALL_LIBDIR/urm/ which resolves to /usr/lib/urm/
on most systems. Configs install to CMAKE_INSTALL_SYSCONFDIR/urm/target/ = /etc/urm/target/.

---

## Step 5: Start URM Server

    urmserver &

URM will automatically:
1. Scan /usr/lib/urm/ and load UrmPlugin.so
2. Execute the plugin constructor, registering all callbacks
3. Load configs from /etc/urm/target/ and the target-specific subdirectory
4. Begin serving resource tuning requests with the extended set

---

## Verification

### Check the plugin is loaded

    ls -la /usr/lib/urm/UrmPlugin.so
    ls /etc/urm/target/
    journalctl -u urmserver | grep -i plugin

### Check target detection

    cat /sys/devices/soc0/machine | tr [:upper:] [:lower:]
    # Example output: qcm6490
    ls /etc/urm/target/qcm6490/

### Verify post-boot scripts

    ls -la /etc/urm/initscripts/post_boot/
    /etc/urm/initscripts/post_boot/post_boot.sh

---

## Uninstall

    sudo rm /usr/lib/urm/UrmPlugin.so
    sudo rm -rf /etc/urm/target/
    sudo rm -rf /etc/urm/initscripts/

---

## Build Options

| CMake Option | Default | Description |
|-------------|---------|-------------|
| BUILD_SHARED_LIBS | ON | Build as shared library (.so) |
| CMAKE_INSTALL_PREFIX | / | Installation root prefix |
| CMAKE_CXX_STANDARD | 11 | C++ standard version |

---

## Troubleshooting

| Problem | Likely Cause | Fix |
|---------|-------------|-----|
| cmake cannot find UrmExtAPIs | URM core not installed | Complete Step 1 first |
| UrmPlugin.so not loaded by URM | Wrong install path | Verify CMAKE_INSTALL_LIBDIR resolves to /usr/lib |
| Target-specific config not applied | Wrong machine name | Check cat /sys/devices/soc0/machine output |
| Post-boot script not running | Missing execute permission | chmod +x /etc/urm/initscripts/post_boot/*.sh |
| Build fails with missing headers | URM headers not on include path | Ensure URM core make install completed |