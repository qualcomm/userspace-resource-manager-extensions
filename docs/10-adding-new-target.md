# 10. Adding a New Target

This guide walks through the complete process of onboarding a new hardware target
into the URM Extensions framework.

---

## Overview

Adding a new target requires:
1. Creating a target-specific SignalsConfig.yaml
2. Creating a target-specific post-boot script
3. Updating CMakeLists.txt to install the new files
4. Verifying target detection

---

## Step 1: Determine the Target Name

The target name is read from /sys/devices/soc0/machine and lowercased.

    cat /sys/devices/soc0/machine | tr upper lower
    # Example output: qcs9200

This string is used as:
- The directory name under Configs/target-specific/
- The TargetsEnabled value in SignalsConfig.yaml
- The suffix of the post-boot script filename

---

## Step 2: Create Target-Specific SignalsConfig.yaml

    mkdir -p Configs/target-specific/qcs9200
    cp Configs/target-specific/qcs9100/SignalsConfig.yaml Configs/target-specific/qcs9200/SignalsConfig.yaml

Edit the new file:
1. Change all TargetsEnabled entries to the new target name
2. Adjust CPU frequency values for the new SoC
3. Adjust cgroup core assignments for the new CPU topology

---

## Step 3: Create Post-Boot Script

    cp initscripts/post_boot/post_boot_qcs9100.sh initscripts/post_boot/post_boot_qcs9200.sh

Edit the new script if the new target requires different tuning.

---

## Step 4: Update CMakeLists.txt

Add the new post-boot script to the install list in CMakeLists.txt.
The target-specific config directory is automatically picked up by the existing
DIRECTORY glob install rule - no changes needed for the config.

---

## Step 5: Build and Install

    rm -rf build && mkdir build && cd build
    cmake .. -DCMAKE_INSTALL_PREFIX=/
    cmake --build .
    sudo cmake --install .

---

## Step 6: Verify

    ls /etc/urm/target/qcs9200/
    ls /etc/urm/initscripts/post_boot/post_boot_qcs9200.sh
    urmserver &
    journalctl -u urmserver | grep -i qcs9200

---

## CPU Topology Reference

    cat /sys/devices/system/cpu/cpu*/topology/cluster_id
    cat /sys/devices/system/cpu/cpufreq/policy0/scaling_available_frequencies

| ResInfo | Cluster |
|---------|--------|
| CLUSTER_LITTLE_ALL_CORES | Little cores (typically 0-3) |
| CLUSTER_BIG_ALL_CORES | Big cores (typically 4-7) |
| CLUSTER_PLUS_ALL_CORES | Prime/plus cores (if present) |

---

## SigType Thresholds

| Signal | SigType | Condition |
|--------|---------|-----------|
| URM_SIG_VIDEO_DECODE | 0 | Default (0-4 sessions) |
| URM_SIG_VIDEO_DECODE | 5 | Medium load (5-20 sessions) |
| URM_SIG_VIDEO_DECODE | 20 | High load (20+ sessions) |
| URM_SIG_CAMERA_ENCODE_MULTI_STREAMS | 0 | Normal (0-12 streams) |
| URM_SIG_CAMERA_ENCODE_MULTI_STREAMS | 13 | High load (12+ streams) |

These thresholds are determined by PostProcessingBlock.cpp and must match
the SigType values in your SignalsConfig.yaml.