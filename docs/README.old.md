# Userspace Resource Manager: A System Resource Tuning Framework

## Table of Contents

1. [Overview]((#1-overview))
2. [Getting Started](#2-getting-stated)
3. [Postboot](#3-post-boot)
4. [Signal Processing Pipeline](#signal-processing-pipeline)
5. [Key Interfaces](#key-interfaces)
6. [Data Flow Diagram](#data-flow-diagram)
7. [Component Responsibilities](#component-responsibilities)

# 1. Overview

The Userspace Resource Manager (uRM) extensions offers extension configs and plugins (extensions)

> **Repository**: https://github.com/rajulup/userspace-resource-manager-extensions
> **License**: BSD-3-Clause-Clear
> **Depends on**: https://github.com/qualcomm/userspace-resource-manager

# 2. Getting Started

To get started with the project:
[Build and install](../README.md#build-and-install-instructions)

# 3. Post Boot
Post-boot scripts apply target-specific tuning at system startup (service startup), for example configure CPU governor, RT scheduling, memory management, etc.

Post Boot scripts are maintained in the directory <project-root>/initscripts/post_boot. These scripts are installed to /etc/urm/initscripts/post_boot/.

Common post-boot configurations are stored in the script "post_boot_common.sh". In addition if target-specific configurations are needed, they can be applied through the script "post_boot_<target_name>.sh". Where "target_name" is obtained by reading the node "/sys/devices/soc0/machine".

```txt
[Service]
...
ExecStart=@CMAKE_INSTALL_FULL_BINDIR@/urm
ExecStartPost=-/etc/urm/initscripts/post_boot/post_boot.sh
```

## 3.1. Post Boot Script Inventory

| Script | Purpose |
|--------|--------|
| post_boot.sh | Dispatcher: runs common + target-specific scripts |
| post_boot_common.sh  | common system tuning |
| post_boot_qcm6490.sh | QCM6490 specific tuning |
| post_boot_qcs8300.sh | QCS8300 specific tuning |
| post_boot_qcs9075.sh | QCS9075 specific tuning |
| post_boot_qcs9100.sh | QCS9100 specific tuning |

All scripts are installed to /etc/urm/initscripts/post_boot/ with execute permissions.

## 3.2. Adding a New Target Script

1. Create initscripts/post_boot/post_boot_NEWTARGET.sh if you target specific tuning
2. The dispatcher will automatically pick it up based on /sys/devices/soc0/machine

# 4. Extended Configurable Resources and Signals

## 4.1. Resources

The following resource codes are supported resources through extensions.

|      Resource Name         |         Id        |
|----------------------------|-------------------|
|   RES_KGSL_DEF_PWRLEVEL    |   0x 00 05 0003   |
|   RES_KGSL_DEVFREQ_MAX     |   0x 00 05 0004   |
|   RES_KGSL_DEVFREQ_MIN     |   0x 00 05 0005   |
|   RES_KGSL_IDLE_TIMER      |   0x 00 05 0006   |
|   RES_KGSL_MAX_PWRLEVEL    |   0x 00 05 0007   |
|   RES_KGSL_MIN_PWRLEVEL    |   0x 00 05 0008   |
|   RES_KGSL_TOUCH_WAKE      |   0x 00 05 0009   |
|   RES_TIMER_MIGRATION      |   0x 00 80 0000   |
|   RES_CPU_FREQ_GOV         |   0x 00 80 0001   |
|   RES_IRQ_AFFINITY         |   0x 00 80 0002   |
|   RES_CPU_WQ_AFFINITY      |   0x 00 80 0003   |
|   RES_IRQ_AFFINE_ALL       |   0x 00 f0 0001   |

### 4.1.1 Resource Details

**RES_KGSL_DEF_PWRLEVEL** (0x00050003)
- Controls the default GPU power level when no explicit request is active.
- Lower values = lower power level = lower performance.
- Policy: lower_is_better (most conservative client wins).
- LowThreshold: 0

**RES_KGSL_DEVFREQ_MAX** (0x00050004)
- Sets the maximum GPU frequency via the devfreq governor.
- Caps GPU frequency to prevent thermal/power issues.
- Policy: lower_is_better (most restrictive cap wins).

**RES_KGSL_DEVFREQ_MIN** (0x00050005)
- Sets the minimum GPU frequency via the devfreq governor.
- Ensures GPU does not drop below a floor frequency.
- Policy: lower_is_better.

**RES_KGSL_IDLE_TIMER** (0x00050006)
- Controls the GPU idle timeout in milliseconds.
- Higher values keep GPU active longer before power collapse.
- Policy: higher_is_better (longest timeout wins).
- LowThreshold: 0

**RES_KGSL_MAX_PWRLEVEL** (0x00050007)
- Sets the maximum GPU power level index.
- Higher index = higher power level allowed.
- Policy: higher_is_better.
- LowThreshold: 0

**RES_KGSL_MIN_PWRLEVEL** (0x00050008)
- Sets the minimum GPU power level index.
- Prevents GPU from dropping below a minimum performance floor.
- Policy: higher_is_better.
- LowThreshold: 0

**RES_KGSL_TOUCH_WAKE** (0x00050009)
- Controls GPU touch-wake behavior.
- Policy: higher_is_better.
- LowThreshold: 0

**RES_TIMER_MIGRATION** (0x00800000)
- Writes directly to /proc/sys/kernel/timer_migration.
- Value 0 disables timer migration (reduces latency for RT tasks).
- Has a sysfs path; no custom callback required.

**RES_CPU_FREQ_GOV** (0x00800001)
- No sysfs path (Path: empty string); requires a custom applier callback.
- Used by CyclicTestsExt to set the CPU frequency governor (e.g., performance).
- Registered in PerApp.yaml for the cyclictest process.

**RES_IRQ_AFFINITY** (0x00800002)
- No sysfs path; requires a custom applier callback.
- Configures IRQ affinity masks for RT workloads.

**RES_CPU_WQ_AFFINITY** (0x00800003)
- No sysfs path; requires a custom applier callback.
- Configures CPU workqueue affinity for RT workloads.

**RES_IRQ_AFFINE_ALL** (0x00f00001)
- Affinizes all system IRQs to a specified set of CPU cores.
- Used by the GENIE_T2T_RUN signal for AI inference workloads.
- Modes: display_on only.
- No sysfs path; requires a custom applier callback.

### 4.1.2 Adding a New Resource

1. Choose a ResType and ResID that does not conflict with existing entries.
2. Add the entry to Configs/ResourcesConfig.yaml.
3. If Path is empty, register a custom applier via URM_REGISTER_RES_APPLIER_CB.
4. Reference the resource by ResCode or Name in SignalsConfig.yaml.
5. Reinstall configs: sudo cp Configs/ResourcesConfig.yaml /etc/urm/target/

See the Extension API Guide (06-extension-api-guide.md) for callback implementation details.

## 4.2. Resource Configs

Resource configs for qith qcom gpu.
|   Target   |    Resource Config                                         |
|------------|------------------------------------------------------------|
| Qcom gpu   |  Configs/ResourcesConfig.yaml                              |

## 4.3. Signals

The following signal codes are supported signals through extensions

|       Signal Code                     |  Code           |
|---------------------------------------|-----------------|
|   URM_SIG_APP_OPEN                    | 0x 00 02 0001   |
|   URM_SIG_BROWSER_APP_OPEN            | 0x 00 02 0002   |
|   URM_SIG_GAME_APP_OPEN               | 0x 00 02 0003   |
|   URM_SIG_MULTIMEDIA_APP_OPEN         | 0x 00 02 0004   |
|   URM_SIG_VIDEO_DECODE                | 0x 00 03 0001   |
|   URM_SIG_CAMERA_PREVIEW              | 0x 00 03 0002   |
|   URM_SIG_CAMERA_ENCODE               | 0x 00 03 0003   |
|   URM_SIG_CAMERA_ENCODE_MULTI_STREAMS | 0x 00 03 0004   |
|   URM_SIG_ENCODE_DECODE               | 0x 00 03 0005   |

The above mentioned list of enums are available in the interface file "UrmPlatformAL.h".

## 4.4. Signal Configs

|   Target   |    Signal Config                                           |
|------------|------------------------------------------------------------|
|   generic  |  Configs/SignalsConfig.yaml                                |
|   qcm6490  |  Configs/target-specific/qcm6490/SignalsConfig.yaml        |
|   qcs8300  |  Configs/target-specific/qcs8300/SignalsConfig.yaml        |
|   qcs9100  |  Configs/target-specific/qcs9100/SignalsConfig.yaml        |

# 5. Extensions Details

## 5.1. System Layers

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

## 5.2. Plugin Loading Sequence

When URM starts, it performs the following steps in order:

1. **Load base upstream resources and signals** from the core URM config.
2. **Discover extension plugins** - scans /usr/lib/urm/ for *.so files.
3. **Load *.so** - the shared library built from this project.
4. **Execute constructor functions** - __attribute__((constructor)) functions run automatically.
   - PostProcessingBlock registers WorkloadPostprocessCallback for gst-launch-1.0 and gst-camera-per-port.
5. **Load custom configs** from /etc/urm/target/ (generic) and /etc/urm/target/<target>/ (target-specific).
6. **Merge resources and signals** - custom definitions override or extend the upstream set.
7. **Start serving requests** with the fully extended resource/signal set.

---

## 5.3. Config Resolution Order

URM resolves configuration in the following priority order (highest wins):

```
Priority 1 (highest): /etc/urm/target/<detected-target>/SignalsConfig.yaml
Priority 1:           /etc/urm/target/<detected-target>/ResourcesConfig.yaml
Priority 2:           /etc/urm/target/SignalsConfig.yaml   (generic)
Priority 2:           /etc/urm/target/ResourcesConfig.yaml (generic)
Priority 3 (lowest):  URM core built-in defaults
```

The target name is detected at runtime from /sys/devices/soc0/machine and lowercased.

---

## 5.4. Signal Processing Pipeline

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

# 5. App Classification and Post Processing Callbacks
Post Processing Callbacks allow writing custom logic which will be hooked into when a certain process starts. More precisely, URM listens to proc events using a Netlink socket, when an event of the type: PROC_EVENT_EXEC is received, URM checks if a post processing callback has been registered for that particular process, if it has been, it gets executed.

Writing a PostProcessing Callback:

```cpp
static void workloadPostprocessCallback(void* context) {
    if(context == nullptr) {
        return;
    }

    PostProcessCBData* cbData = static_cast<PostProcessCBData*>(context);
    if(cbData == nullptr) {
        return;
    }

    // Main selection logic
    // Get the sigId and sigType for the signal which needs to be configured.
    // ........

    // Relay the information back to URM
    cbData->mSigId = sigId;
    cbData->mSigType = sigType;
}

__attribute__((constructor))
static void registerWithUrm() {
    // Post Processing Callback for process: "gst-launch-1.0"
    URM_REGISTER_POST_PROCESS_CB("gst-launch-1.0", workloadPostprocessCallback)
}
```

