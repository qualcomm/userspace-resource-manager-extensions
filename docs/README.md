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