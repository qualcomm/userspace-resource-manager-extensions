# 1. Architecture Overview

> **Repository**: https://github.com/rajulup/userspace-resource-manager-extensions
> **License**: BSD-3-Clause-Clear
> **Depends on**: https://github.com/qualcomm/userspace-resource-manager

## Table of Contents
1. [System Layers](#system-layers)
2. [Plugin Loading Sequence](#plugin-loading-sequence)
3. [Config Resolution Order](#config-resolution-order)
4. [Signal Processing Pipeline](#signal-processing-pipeline)
5. [Key Interfaces](#key-interfaces)
6. [Data Flow Diagram](#data-flow-diagram)
7. [Component Responsibilities](#component-responsibilities)

---

## System Layers

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
|      qcm6490/SignalsConfig.yaml                          |
|      qcs8300/SignalsConfig.yaml                          |
|      qcs9100/SignalsConfig.yaml                          |
|                                                          |
|  Extensions/                                             |
|    PostProcessingBlock.cpp  - GStreamer workload detector |
|    CyclicTestsExt.cpp       - RT benchmark extension     |
|                                                          |
|  initscripts/post_boot/                                  |
|    post_boot.sh             - dispatcher script          |
|    post_boot_qcm6490.sh     - QCM6490 kernel tuning      |
|    post_boot_qcs8300.sh     - QCS8300 kernel tuning      |
|    post_boot_qcs9100.sh     - QCS9100 kernel tuning      |
+----------------------------------------------------------+
```

---

## Plugin Loading Sequence

When URM starts, it performs the following steps in order:

1. **Load base upstream resources and signals** from the core URM config.
2. **Discover extension plugins** - scans /usr/lib/urm/ for *.so files.
3. **Load UrmPlugin.so** - the shared library built from this project.
4. **Execute constructor functions** - __attribute__((constructor)) functions run automatically.
   - PostProcessingBlock registers WorkloadPostprocessCallback for gst-launch-1.0 and gst-camera-per-port-example.
5. **Load custom configs** from /etc/urm/target/ (generic) and /etc/urm/target/<target>/ (target-specific).
6. **Merge resources and signals** - custom definitions override or extend the upstream set.
7. **Start serving requests** with the fully extended resource/signal set.

---

## Config Resolution Order

URM resolves configuration in the following priority order (highest wins):

```
Priority 1 (highest): /etc/urm/target/<detected-target>/SignalsConfig.yaml
Priority 2:           /etc/urm/target/<detected-target>/ResourcesConfig.yaml
Priority 3:           /etc/urm/target/SignalsConfig.yaml   (generic)
Priority 4:           /etc/urm/target/ResourcesConfig.yaml (generic)
Priority 5 (lowest):  URM core built-in defaults
```

The target name is detected at runtime from /sys/devices/soc0/machine and lowercased.

---

## Signal Processing Pipeline

```
Client calls tuneResources() / sendSignal()
         |
         v
URM receives signal (SigId + SigType + PID)
         |
         v
Post-Process Callback invoked (if registered for this process name)
  PostProcessingBlock::PostProcess(pid, sigId, sigType)
    - reads /proc/<pid>/cmdline
    - detects GStreamer pipeline elements (encoder/decoder/preview)
    - updates sigId and sigType based on workload
         |
         v
Signal matched against SignalsConfig (SigId + SigType + TargetsEnabled)
         |
         v
Resource list extracted from matched signal config
         |
         v
For each resource:
  - Custom applier callback (if registered via URM_REGISTER_RES_APPLIER_CB)
  - OR default sysfs write to resource Path
         |
         v
Resources held for Timeout duration (or indefinitely if Timeout=-1)
         |
         v
Teardown: custom teardown callback OR restore previous sysfs value
```

---

## Key Interfaces

### Headers (from URM core)

| Header | Purpose |
|--------|---------|
| Urm/Extensions.h | Macro APIs for registering callbacks |
| Urm/UrmPlatformAL.h | Signal ID enums (URM_SIG_*) |
| Urm/UrmAPIs.h | Client API: tuneResources(), sendSignal() |

### Registration Macros

| Macro | Purpose |
|-------|---------|
| URM_REGISTER_RES_APPLIER_CB(resCode, fn) | Register custom resource apply handler |
| URM_REGISTER_RES_TEAR_CB(resCode, fn) | Register custom resource teardown handler |
| URM_REGISTER_POST_PROCESS_CB(procName, fn) | Register post-process callback for a process name |

### Callback Signatures

```cpp
// Resource apply/teardown callback
int32_t myApplier(void* res);
int32_t myTeardown(void* res);

// Post-process callback
void myPostProcess(void* context);  // context is PostProcessCBData*

struct PostProcessCBData {
    pid_t    mPid;     // Process ID that triggered the signal
    uint32_t mSigId;   // Signal ID -- can be modified by the callback
    uint32_t mSigType; // Signal type/variant -- can be modified by the callback
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
|  3. Invoke PostProcessingBlock::PostProcess()       |
|     -> reads /proc/<pid>/cmdline                    |
|     -> detects GStreamer elements                   |
|     -> updates SigId / SigType                     |
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

## Component Responsibilities

| Component | File(s) | Responsibility |
|-----------|---------|----------------|
| PostProcessingBlock | Extensions/PostProcessingBlock.cpp | Detects GStreamer multimedia workloads; maps generic signals to typed variants |
| CyclicTestsExt | Extensions/CyclicTestsExt.cpp | Registers custom resource appliers for RT benchmark (cyclictest) resources |
| ResourcesConfig | Configs/ResourcesConfig.yaml | Declares all custom resource IDs, sysfs paths, policies, and thresholds |
| SignalsConfig (generic) | Configs/SignalsConfig.yaml | Declares generic signals (app open, RT trigger, AI inference) |
| SignalsConfig (target) | Configs/target-specific/*/SignalsConfig.yaml | Per-target CPU frequency and cgroup tuning for multimedia signals |
| PerApp.yaml | Configs/PerApp.yaml | Maps process thread names to cgroup identifiers |
| InitConfig.yaml | Configs/InitConfig.yaml | IRQ affinity initialization settings |
| post_boot.sh | initscripts/post_boot/post_boot.sh | Dispatcher: auto-detects target and runs target-specific boot script |
| post_boot_*.sh | initscripts/post_boot/post_boot_<target>.sh | Target-specific kernel tuning (governor, RT clamp, swap, cgroups) |