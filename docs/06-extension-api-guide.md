# 6. Extension API Guide

This guide explains how to write C++ extension modules that plug into URM.

---

## Overview

Extension modules are C++ shared libraries compiled into UrmPlugin.so.
They use three types of callbacks:

| Callback Type | Registration Macro | Purpose |
|--------------|-------------------|---------||
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
  callbackFn - Function with signature: int32_t fn(void* res)

### URM_REGISTER_RES_TEAR_CB

Registers a custom function to tear down (restore) a resource.

    URM_REGISTER_RES_TEAR_CB(resCode, callbackFn)

### URM_REGISTER_POST_PROCESS_CB

Registers a post-process callback for a specific process name.

    URM_REGISTER_POST_PROCESS_CB(processName, callbackFn)

The context parameter is a PostProcessCBData* containing:

    struct PostProcessCBData {
        pid_t    mPid;     // Process ID
        uint32_t mSigId;   // Signal ID (can be modified)
        uint32_t mSigType; // Signal type (can be modified)
    };

---

## Plugin Constructor Pattern

All registration calls must happen inside a function marked with
__attribute__((constructor)) so they execute when UrmPlugin.so is loaded.
This is exactly how PostProcessingBlock.cpp registers its callbacks:

    #include <Urm/Extensions.h>

    __attribute__((constructor))
    static void registerWithUrm() {
        URM_REGISTER_POST_PROCESS_CB("gst-launch-1.0", WorkloadPostprocessCallback)
        URM_REGISTER_POST_PROCESS_CB("gst-camera-per-port-example", WorkloadPostprocessCallback)
    }

---

## Writing a Resource Applier

Example: Custom CPU frequency governor setter

    #include <Urm/Extensions.h>
    #include <fstream>
    #include <string>

    static int32_t applyCpuFreqGov(void* res) {
        for (int cpu = 0; cpu < 8; cpu++) {
            std::string path = "/sys/devices/system/cpu/cpufreq/policy"
                               + std::to_string(cpu) + "/scaling_governor";
            std::ofstream f(path);
            if (f.is_open()) { f << "performance"; }
        }
        return 0;
    }

    static int32_t teardownCpuFreqGov(void* res) {
        for (int cpu = 0; cpu < 8; cpu++) {
            std::string path = "/sys/devices/system/cpu/cpufreq/policy"
                               + std::to_string(cpu) + "/scaling_governor";
            std::ofstream f(path);
            if (f.is_open()) { f << "schedutil"; }
        }
        return 0;
    }

    __attribute__((constructor))
    static void registerCpuFreqGov() {
        URM_REGISTER_RES_APPLIER_CB(0x00800001, applyCpuFreqGov)
        URM_REGISTER_RES_TEAR_CB(0x00800001, teardownCpuFreqGov)
    }

---

## Writing a Post-Process Callback

Post-process callbacks intercept signals before resource application.
They can modify both SigId and SigType to redirect the signal.

    #include <Urm/Extensions.h>
    #include <Urm/UrmPlatformAL.h>

    static void myPostProcess(void* context) {
        if (context == nullptr) return;
        PostProcessCBData* cbData = static_cast<PostProcessCBData*>(context);
        pid_t pid = cbData->mPid;
        uint32_t sigId = cbData->mSigId;
        uint32_t sigType = cbData->mSigType;

        // Inspect the process and modify sigId/sigType as needed
        if (isHeavyWorkload(pid)) { sigType = 5; }

        cbData->mSigId = sigId;
        cbData->mSigType = sigType;
    }

    __attribute__((constructor))
    static void registerMyCallback() {
        URM_REGISTER_POST_PROCESS_CB("my-process-name", myPostProcess)
    }

---

## Using the Client API

To send a signal or tune resources from a client application:

    #include <Urm/UrmAPIs.h>

    SysResource resource;
    resource.mResCode = 0x00050003;  // RES_KGSL_DEF_PWRLEVEL
    resource.mResInfo = 0;
    resource.mNumValues = 1;
    resource.mResValue.value = 3;    // Set power level to 3
    int64_t duration = 5000;         // Hold for 5 seconds (ms)
    int32_t properties = 0;
    int32_t numRes = 1;
    int64_t handle = tuneResources(duration, properties, numRes, &resource);

    // Send a signal (triggers post-processing + resource bundle)
    sendSignal(URM_SIG_VIDEO_DECODE, 0 /*sigType*/, getpid());

---

## Adding a New Extension Module

1. Create a new .cpp file in Extensions/
2. Include Urm/Extensions.h
3. Implement your applier/teardown/post-process functions
4. Register them in a __attribute__((constructor)) function
5. The CMakeLists.txt automatically globs all *.cpp files - no changes needed
6. Rebuild: cmake --build . && sudo cmake --install .

---

## Thread Safety Considerations

- Constructor functions run at library load time (single-threaded).
- Applier/teardown callbacks may be called from multiple threads concurrently.
- Post-process callbacks may be called concurrently for different PIDs.
- Use std::call_once or std::mutex for any shared state in callbacks.
- Reading /proc filesystem is safe from multiple threads (read-only kernel interface).

---

## Error Handling

- Resource applier/teardown callbacks should return 0 on success, non-zero on failure.
- Post-process callbacks have void return type; errors should be handled internally.
- If a sysfs write fails, log the error but do not crash - URM will continue.
- If a callback-only resource has no registered applier, URM will skip it silently.
