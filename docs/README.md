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

# 4. Fetching Target Information

Plugins can fetch target information using the "getTargetInfo" helper utility provided by URM. The following information can be retrieved:
- CPU Masks
- Logical to Physical Mappings
- Number of cores / clusters on the target

```cpp
// Utility to fetch target-specific information
uint64_t getTargetInfo(int32_t option,
                       int32_t numArgs,
                       int32_t* args);
```

Usage:

For example, to get a cpu mask (which can be used, for example, for IRQ affinity use cases):
```cpp
    int32_t args[2] = {0, 0};
    uint64_t mask = getTargetInfo(GET_MASK, 2, args); // all cores in silver cluster
```

- The first parameter in the args array, in this case is the logical cluster ID.
Here, we use the logical identifier for the silver cluster, using logical identifiers in the code makes it target-architecture independent, hence portable.
The clusters logical id's are ordered according to the cluster capacities. Hence: 0 represents silver / little, 1 represents gold / big and 2 represents prime.

- The second parameter in the args array, in this case is the number of cores in the cluster to be considered for mask creation. 0 represents all cores within the cluster, a non-zero positive value represents the exact number of cores to be considered.

To simply get a mask corresponding to the highest capacity cluster on the target, for all cores, the GET_MAX_CLUSTER query can be used, as follows:
```cpp
    int32_t args[2] = {GET_MAX_CLUSTER, 0};
    uint64_t mask = getTargetInfo(GET_MASK, 2, args); // all cores in the highest capacity cluster
```
