# Userspace Resource Manager: A System Resource Tuning Framework

## Table of Contents

# 1. Overview
The Userspace Resource Manager (uRM) extensions offers extension configs and plugins (extensions)

# 2. Getting Started

To get started with the project:
[Build and install](../README.md#build-and-install-instructions)

# 3. Extended Configurable Resources and Signals

## 3.1. Resources

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

## 3.2. Resource Configs

Resource configs for qith qcom gpu.
|   Target   |    Resource Config                                         |
|------------|------------------------------------------------------------|
| Qcom gpu   |  Configs/ResourcesConfig.yaml                              |

## 3.2. Signals

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

## 3.3 Signal Configs

|   Target   |    Signal Config                                           |
|------------|------------------------------------------------------------|
|   generic  |  Configs/SignalsConfig.yaml                                |
|   qcm6490  |  Configs/target-specific/qcm6490/SignalsConfig.yaml        |
|   qcs8300  |  Configs/target-specific/qcs8300/SignalsConfig.yaml        |
|   qcs9100  |  Configs/target-specific/qcs9100/SignalsConfig.yaml        |

# 4. App Classification and Post Processing Callbacks
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

# 5. Post Boot Configurations
Post Boot scripts are maintained in the directory <project-root>/initscripts/post_boot. These scripts are installed to /etc/urm/initscripts/post_boot/.

Common post-boot configurations are stored in the script "post_boot_common.sh". In addition if target-specific configurations are needed, they can be applied through the script "post_boot_<target_name>.sh". Where "target_name" is obtained by reading the node "/sys/devices/soc0/machine".

The post boot configurations are applied during service startup.

```txt
[Service]
...
ExecStart=@CMAKE_INSTALL_FULL_BINDIR@/urm
ExecStartPost=-/etc/urm/initscripts/post_boot/post_boot.sh
```
