# 1. Architecture Overview

## Table of Contents
1. [Architectural Layers](#architectural-layers)
2. [Plugin Loading Sequence](#plugin-loading-sequence)
3. [Config Processing Order](#config-processing-order)
4. [Key Interfaces](#key-interfaces)
5. [Data Flow Diagram](#data-flow-diagram)

---

## Architectural Layers

```
+----------------------------------------------------------+
|          Userspace Resource Manager (URM) Core           |
|  Standard Resources: CPU, GPU, Memory, Cgroups, etc.     |
|  Standard Signals: App lifecycle, display state, etc.    |
|                        ^                                 |
|                        | Extension Interface             |
+------------------------|---------------------------------+
                         |
+------------------------|---------------------------------+
|    URM Extensions (This Project)                         |
|                                                          |
|  Configs/                                                |
|    ResourcesConfig.yaml  - custom resource definitions   |
|    SignalsConfig.yaml    - custom signal definitions      |
|    PerApp.yaml           - per-app thread/resource maps  |
|    InitConfig.yaml       - IRQ affinity init settings    |
|    target-specific/      - per-target overrides          |
|      alorp/                                              |
|      qcm6490/                                            |
|      qcs615/                                             |
|      qcs8300/                                            |
|      qcs9100/  (also covers qcs9075)                     |
|                                                          |
|  Extensions/                                             |
|    CamPostProcessing.cpp  - GStreamer workload detector  |
|    GenieT2T.cpp           - AI inference extension       |
|    PreemptRtExtn.cpp      - RT benchmark extension       |
|    PredefCallbacks.cpp    - Predefined IRQ callbacks     |
|    Helpers.cpp            - Shared utility functions     |
|                                                          |
|  initscripts/post_boot/                                  |
|    post_boot.sh             - dispatcher script          |
|    post_boot_common.sh      - common kernel tuning       |
|    post_boot_alorp.sh       - ALORP kernel tuning        |
|    post_boot_qcm6490.sh     - QCM6490 kernel tuning      |
|    post_boot_qcs615.sh      - QCS615 kernel tuning       |
|    post_boot_qcs8300.sh     - QCS8300 kernel tuning      |
|    post_boot_qcs9075.sh     - QCS9075 kernel tuning      |
|    post_boot_qcs9100.sh     - QCS9100 kernel tuning      |
+----------------------------------------------------------+
```

---

## Plugins

Plugins allow users to influence URM behaviour, through well defined hooks. When URM starts, it performs the following steps in order:

1. **Load base upstream resources and signals** from the core URM config.
2. **Discover extension plugins** - scans /usr/lib/urm/ for *.so files.
3. **Load UrmPlugin.so** - the shared library built from this project.
4. **Execute constructor functions** - __attribute__((constructor)) functions run automatically.
   - `CamPostProcessing.cpp` registers `WorkloadPostprocessCallback` for `gst-launch-1.0` and `gst-camera-per-port-example`.
   - `GenieT2T.cpp` registers `workloadPostprocessCallback` for `genie-t2t-run`.
5. **Load custom configs** from /etc/urm/target/ (generic) and /etc/urm/target/<target>/ (target-specific).
6. **Merge resources and signals** - custom definitions override or extend the upstream set.
7. **Start serving requests** with the fully extended resource/signal set.

---

## Config Processing Order

URM processes configuration in the following order:

```
1: Upstream (common) Configurations
2: Target-Agnostic Configurations
3. Target-Specific Configurations (indexed by target name)
4. Any custom configurations (registered through the Extensions Callback)
```

Configuration Overriding is supported, for example a Target-Specific Resource with the same ResCode as one in the Upstream Configurations, then the Upstream Configuration, in this case, will be Overridden.


Note:
The target name is detected at runtime from /sys/devices/soc0/machine and lowercased.

---

## Key Interfaces

### Headers (from URM core)

| Header | Purpose |
|--------|---------|
| Urm/Extensions.h | Macro APIs for registering callbacks |
| Urm/UrmPlatformAL.h | Signal ID enums (URM_SIG_*) |
| Urm/UrmAPIs.h | Client API: tuneResources(), sendSignal() |
| Urm/SignalInternal.h | acquireSignal(), releaseSignal() |
| Urm/TargetRegistry.h | GET_TARGET_INFO, GET_MASK, GET_MAX_CLUSTER |
| Urm/ResourceRegistry.h | Resource registry access |

### Registration Macros

| Macro | Purpose |
|-------|---------|
| URM_REGISTER_RES_APPLIER_CB(resCode, fn) | Register custom resource apply handler |
| URM_REGISTER_RES_TEAR_CB(resCode, fn) | Register custom resource teardown handler |
| URM_REGISTER_POST_PROCESS_CB(procName, fn) | Register post-process callback for a process name |

### Callback Signatures

```cpp
// Resource apply/teardown callback
typedef void (*ResourceLifecycleCallback)(void*);

// Post-process callback
typedef void (*PostProcessingCallback)(void*); // where the argument is PostProcessCBData*

struct PostProcessCBData {
    pid_t    mPid;      // Process ID that triggered the signal
    uint32_t mSigId;    // Signal ID -- can be modified by the callback
    uint32_t mSigType;  // Signal type/variant -- can be modified by the callback
    int64_t  mHandleAcq; // Handle returned by acquireSignal() (set by callback)
};
```

---

## Data Flow Diagram

```
Client Application
       |
       |  tuneResources(duration, props, numRes, &resource)
       |  sendSignal(sigId, sigType, pid)
       v
+-----------------------------------------------------+
|                   URM Core Server                   |
|                                                     |
|  1. Receive signal (SigId + SigType + PID)          |
|  2. Look up registered post-process callbacks       |
|  3. Invoke post-process callback (e.g.              |
|     WorkloadPostprocessCallback in                  |
|     CamPostProcessing.cpp):                         |
|     -> reads /proc/<pid>/cmdline                    |
|     -> detects GStreamer elements                   |
|     -> updates SigId / SigType / extraArgs          |
|     -> calls acquireSignal() directly               |
|  4. Match updated signal against SignalsConfig      |
|  5. For each resource in matched signal:            |
|     a. If custom applier registered -> call it      |
|     b. Else -> write value to sysfs Path            |
|  6. Hold resources for Timeout ms                   |
|  7. On expiry -> teardown callback OR restore sysfs |
+-----------------------------------------------------+
       |
       v
  Linux Kernel
  (sysfs, /proc, cgroups, cpufreq, kgsl, IRQ)
```

---
