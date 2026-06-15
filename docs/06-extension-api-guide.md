# 6. Extension API Guide

This guide explains how to write C++ extension modules that plug into URM.

---

## Overview

Extension modules are C++ shared libraries compiled into UrmPlugin.so.
They use three types of callbacks:


| Callback Type | Registration Macro | Purpose |
|---------|-------|----------|
| Resource Applier | URM_REGISTER_RES_APPLIER_CB | Custom logic to apply a resource value |
| Resource Teardown | URM_REGISTER_RES_TEAR_CB | Custom logic to restore a resource |
| Post-Process | URM_REGISTER_POST_PROCESS_CB | Inspect/modify signal before resource application |

All macros are defined in Urm/Extensions.h.

---

## Registration Macros

### URM_REGISTER_RES_APPLIER_CB

Registers a custom function to apply a resource value.

    URM_REGISTER_RES_APPLIER_CB(resCode, callbackFn)

Parameters:
  resCode    - The full resource code (e.g., 0x00800001)
  callbackFn - Function with signature: void fn(void* context)

### URM_REGISTER_RES_TEAR_CB

Registers a custom function to tear down (restore) a resource.

    URM_REGISTER_RES_TEAR_CB(resCode, callbackFn)

### URM_REGISTER_POST_PROCESS_CB

Registers a post-process callback for a specific process name.

    URM_REGISTER_POST_PROCESS_CB(processName, callbackFn)

The context parameter is a PostProcessCBData* containing:

    struct PostProcessCBData {
        pid_t    mPid;      // Process ID
        uint32_t mSigId;    // Signal ID (can be modified)
        uint32_t mSigType;  // Signal type (can be modified)
        int64_t  mHandleAcq; // Handle returned by acquireSignal() (set by callback)
    };

---

## Plugin Constructor Pattern

All registration calls must happen inside a function marked with
__attribute__((constructor)) so they execute when UrmPlugin.so is loaded.
Example:

```cpp
#include <Urm/Extensions.h>

__attribute__((constructor))
static void registerWithUrm() {
    URM_REGISTER_POST_PROCESS_CB("gst-launch-1.0", WorkloadPostprocessCallback)
    URM_REGISTER_POST_PROCESS_CB("gst-camera-per-port-example", WorkloadPostprocessCallback)
}
```

---

## Writing a Resource Applier

Example: Custom CPU frequency governor setter (from PreemptRtExtn.cpp)

```cpp
#include <Urm/Extensions.h>
#include <dirent.h>
#include <fstream>
#include <string>
#include <vector>

static bool gCpufreqApplied = false;
static std::vector<std::pair<std::string, std::string>> gCpufreqGovBackup;

static void cpufreqGovApplierCallback(void* /*context*/) {
    if (gCpufreqApplied) return;
    gCpufreqGovBackup.clear();

    DIR* dir = opendir("/sys/devices/system/cpu/cpufreq/");
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strncmp(entry->d_name, "policy", 6) != 0) continue;
        std::string govFile = std::string("/sys/devices/system/cpu/cpufreq/")
                              + entry->d_name + "/scaling_governor";
        std::string oldVal;
        if (readLineFromFile(govFile, oldVal)) {
            gCpufreqGovBackup.emplace_back(govFile, oldVal);
            writeLineToFile(govFile, "performance");
        }
    }
    closedir(dir);
    gCpufreqApplied = !gCpufreqGovBackup.empty();
}

static void cpufreqGovTearCallback(void* /*context*/) {
    if (!gCpufreqApplied) return;
    for (const auto& kv : gCpufreqGovBackup) {
        AuxRoutines::writeToFile(kv.first, kv.second);
    }
    gCpufreqGovBackup.clear();
    gCpufreqApplied = false;
}

URM_REGISTER_RES_APPLIER_CB(0x00800001, cpufreqGovApplierCallback)
URM_REGISTER_RES_TEAR_CB   (0x00800001, cpufreqGovTearCallback)
```

---

## Writing a Post-Process Callback

### Simple variant: modify SigId/SigType only

Post-process callbacks intercept signals before resource application.
They can modify both SigId and SigType to redirect the signal.

```cpp
#include <Urm/Extensions.h>
#include <Urm/UrmPlatformAL.h>

static void myPostProcess(void* context) {
    if (context == nullptr) return;
    PostProcessCBData* cbData = static_cast<PostProcessCBData*>(context);

    // Inspect the process and modify sigId/sigType as needed
    cbData->mSigId   = CONSTRUCT_SIG_CODE(0xf1, 0x0123);
    cbData->mSigType = DEFAULT_SIGNAL_TYPE;
}

__attribute__((constructor))
static void registerMyCallback() {
    URM_REGISTER_POST_PROCESS_CB("my-process-name", myPostProcess)
}
```

### Advanced variant: call acquireSignal() directly

For workloads that need to pass extra attributes (e.g., FPS, resolution) to the signal
matching engine, the callback can call `acquireSignal()` directly and store the returned
handle in `cbData->mHandleAcq`. This is the pattern used by `CamPostProcessing.cpp`:

```cpp
#include <Urm/Extensions.h>
#include <Urm/SignalInternal.h>

static void WorkloadPostprocessCallback(void* context) {
    if (context == nullptr) return;
    PostProcessCBData* cbData = static_cast<PostProcessCBData*>(context);

    pid_t    pid     = cbData->mPid;
    uint32_t sigId   = cbData->mSigId;
    uint32_t sigType = cbData->mSigType;

    uint32_t* extraArgs = nullptr;
    // Detect workload from /proc/<pid>/cmdline and update sigId, sigType, extraArgs
    PostProcessingBlock::getInstance().PostProcess(pid, sigId, sigType, &extraArgs);

    // Acquire the signal with extra attributes (FPS, height, width)
    int64_t handle = acquireSignal(sigId, sigType, pid, pid,
                                   SIGNAL_EXTRA_ATTRS_COUNT, extraArgs);
    cbData->mHandleAcq = handle;
}
```

The `extraArgs` array carries `SIGNAL_EXTRA_ATTRS_COUNT` elements indexed by:
- `SIGNAL_EXTRA_ATTR_FPS`    — frame rate extracted from GStreamer pipeline cmdline
- `SIGNAL_EXTRA_ATTR_HEIGHT` — maximum height= value found in cmdline
- `SIGNAL_EXTRA_ATTR_WIDTH`  — maximum width= value found in cmdline

---

## Predefined Callbacks (PredefCallbacks.h)

`PredefCallbacks.h` provides a small registry of reusable callback pairs:

```cpp
enum PredefCallbackId : int32_t {
    IRQ_AFFINE_ALL = 0,
};

// Retrieve a predefined apply or tear callback by ID
ResourceLifecycleCallback getApplyCb(int32_t id);
ResourceLifecycleCallback getTearCb(int32_t id);
```

Usage (from GenieT2T.cpp):

```cpp
URM_REGISTER_RES_APPLIER_CB(0x00f00001, getApplyCb(IRQ_AFFINE_ALL))
URM_REGISTER_RES_TEAR_CB(0x00f00001, getTearCb(IRQ_AFFINE_ALL))
```

---
