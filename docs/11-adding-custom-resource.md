# 11. Adding a Custom Resource

This guide walks through the end-to-end process of adding a new custom resource.

---

## Overview

A custom resource consists of:
1. A YAML definition in ResourcesConfig.yaml
2. (Optional) A C++ applier/teardown callback in an extension module
3. References in SignalsConfig.yaml to use the resource

---

## Step 1: Choose ResType and ResID

| ResType | Reserved For |
|---------|-------------|
| 0x05 | GPU (KGSL) resources |
| 0x80 | RT benchmark resources |
| 0xf0 | Special/AI resources |
| Other | Available for custom use |

Choose a ResType and ResID that does not conflict with existing entries.
The full ResCode = (ResType << 16) | ResID.
Example: ResType=0x81, ResID=0x0001 => ResCode=0x00810001

---

## Step 2: Add to ResourcesConfig.yaml

Add an entry to Configs/ResourcesConfig.yaml:

    ResourceConfigs:
      - ResType: "0x81"
        ResID: "0x0001"
        Name: "RES_MY_CUSTOM_RESOURCE"
        Path: "/sys/my/sysfs/path"
        Supported: true
        LowThreshold: 0
        HighThreshold: 100
        Permissions: "third_party"
        Modes: ["display_on", "doze"]
        Policy: "pass_through"
        ApplyType: "global"

If the resource requires a custom callback (no sysfs path), set Path to empty string.

---

## Step 3: Register Custom Callbacks (if no sysfs path)

Create or edit a .cpp file in Extensions/:

    #include <Urm/Extensions.h>

    static int32_t applyMyResource(void* res) {
        // Custom apply logic
        return 0;
    }

    static int32_t teardownMyResource(void* res) {
        // Custom teardown/restore logic
        return 0;
    }

    __attribute__((constructor))
    static void registerMyResource() {
        URM_REGISTER_RES_APPLIER_CB(0x00810001, applyMyResource)
        URM_REGISTER_RES_TEAR_CB(0x00810001, teardownMyResource)
    }

---

## Step 4: Reference in SignalsConfig.yaml

Add the resource to a signal definition:

    SignalConfigs:
      - SigId: "0x0001"
        Category: "0x03"
        Name: "URM_SIG_VIDEO_DECODE"
        Enable: true
        Resources:
          - {ResCode: "0x00810001", Values: [42]}

---

## Step 5: Build and Install

    rm -rf build && mkdir build && cd build
    cmake .. -DCMAKE_INSTALL_PREFIX=/
    cmake --build .
    sudo cmake --install .

---

## Step 6: Test from a Client

    #include <Urm/UrmAPIs.h>

    SysResource resource;
    resource.mResCode = 0x00810001;
    resource.mResInfo = 0;
    resource.mNumValues = 1;
    resource.mResValue.value = 42;
    int64_t handle = tuneResources(5000, 0, 1, &resource);

---

## Policy Selection Guide

| Use Case | Policy |
|----------|--------|
| Frequency cap (lower = more conservative) | lower_is_better |
| Minimum floor (higher = more performance) | higher_is_better |
| RT/special resources (last write wins) | pass_through |

---

## Troubleshooting

| Problem | Cause | Fix |
|---------|-------|-----|
| Resource not applied | Callback not registered | Check __attribute__((constructor)) function |
| sysfs write fails | Wrong path or permissions | Verify path exists and is writable |
| ResCode conflict | Duplicate ResType+ResID | Choose a unique ResType/ResID combination |
| Resource ignored | Supported: false | Set Supported: true in ResourcesConfig.yaml |