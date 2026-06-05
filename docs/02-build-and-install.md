# 2. Build and Install Guide

## Prerequisites

| Requirement | Details |
|-------------|---------|
| OS | Ubuntu 20.04+ (or compatible Linux) |
| CMake | >= 3.6 |
| C++ Compiler | GCC or Clang with C++11 support |
| URM Core (Libs and Headers) | Needs to be Built and installed first (discussed next) |


---

## Step 1: Build and Install URM

The extensions depend on the URM core libraries and headers.
Follow the instructions at:
[URM-Build](https://github.com/qualcomm/userspace-resource-manager/blob/main/README.md#requirements)

After this step, the following must be available on your system:
- Libraries: libUrmExtAPIs.so, libRestuneCore.so, libUrmAuxUtils.so
- Headers: Urm/Extensions.h, Urm/UrmPlatformAL.h, Urm/UrmAPIs.h

---

## Step 2: Build URM Extensions

    git clone https://github.com/qualcomm/userspace-resource-manager-extensions
    cd userspace-resource-manager-extensions

    # Create a clean build directory
    rm -rf build && mkdir build && cd build

    # Configure with CMake
    cmake .. -DCMAKE_INSTALL_PREFIX=/

    # Build
    cmake --build .


---

### What Gets Built

The CMake build compiles all .cpp files under Extensions/ into a single shared library:

| Output | Description |
|--------|-------------|
| UrmPlugin.so | The extension plugin loaded by URM at startup |

### CMakeLists.txt Summary

---

## Step 3: Install

```bash
    sudo cmake --install .
```

### Install Manifest

| Source | Installed To | Permissions |
|--------|-------------|-------------|
| UrmPlugin.so | /usr/lib/urm/ | Standard library |
| Configs/*.yaml | /etc/urm/target/ | 644 |
| Configs/target-specific/qcm6490/ | /etc/urm/target/qcm6490/ | 644 |
| Configs/target-specific/qcs8300/ | /etc/urm/target/qcs8300/ | 644 |
| Configs/target-specific/qcs9100/ | /etc/urm/target/qcs9100/ | 644 |
| initscripts/post_boot/*.sh | /etc/urm/initscripts/post_boot/ | 755 |

Note:
- The library installs to CMAKE_INSTALL_LIBDIR/urm/ which resolves to /usr/lib/urm/
on most systems.
- Configs install to CMAKE_INSTALL_SYSCONFDIR/urm/target/ = /etc/urm/target/.

---

## Step 4: Start URM Server

From the Build Directory, simply run the executable:

```bash
sudo ./urm
```

---
