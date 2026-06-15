# 5. Signals Reference

Custom signals defined in Configs/SignalsConfig.yaml and target-specific overrides.

---

## Signal Code Reference

| Signal Name | Full Code | Category | SigId | Description |
|-------------|-----------|----------|-------|-------------|
| URM_APP_OPEN | 0x00010001 | 0x01 App Lifecycle | 0x0001 | Generic app open event |
| URM_SIG_VIDEO_DECODE | 0x00030001 | 0x03 Multimedia | 0x0001 | Video decode workload |
| URM_SIG_CAMERA_PREVIEW | 0x00030002 | 0x03 Multimedia | 0x0002 | Camera preview workload |
| URM_SIG_CAMERA_ENCODE | 0x00030003 | 0x03 Multimedia | 0x0003 | Camera encode workload |
| URM_SIG_CAMERA_ENCODE_MULTI_STREAMS | 0x00030004 | 0x03 Multimedia | 0x0004 | Multi-stream camera encode |
| RT_TRIGGER | 0x00800001 | 0x80 RT Workload | 0x0001 | Real-time workload trigger |
| GENIE_T2T_RUN | 0x00f10123 | 0xf1 Special | 0x0123 | AI inference (token-to-token) run |

---

## Generic Signals (Configs/SignalsConfig.yaml)

### URM_APP_OPEN (Category 0x01, SigId 0x0001)

Fired when any application opens. Applies cgroup tuning for the foreground app.
Permissions: system, third_party

Resources applied:
| ResCode | Values | Effect |
|---------|--------|--------|
| RES_CGRP_RUN_CORES | [2, 0,1,2,3] | Restrict background cgroup (id=2) to little cores |
| RES_CGRP_CPU_LATENCY | [4, -20] | Set CPU latency hint for focused cgroup (id=4) |

### RT_TRIGGER (Category 0x80, SigId 0x0001)

Fired to trigger real-time workload mode. Timeout: -1 (indefinite).
Permissions: system, third_party

Resources applied:
| ResCode | ResInfo | Values | Effect |
|---------|---------|--------|--------|
| 0x00030003 | - | [-1] | Disable CPU scaling |
| 0x00800000 | - | [0] | Disable timer migration |
| 0x00800001 | - | [0] | Set CPU freq governor (callback) |
| 0x00800002 | - | [0] | Configure IRQ affinity (callback) |
| 0x00800003 | - | [0] | Configure WQ affinity (callback) |
| 0x00040003 | 0x00000000 | [1] | Isolate little cores (cluster 0) |
| 0x00040004 | 0x00000100 | [1] | Isolate big cores (cluster 1) |
| 0x00040005 | 0x00000200 | [1] | Isolate prime cores (cluster 2) |

### GENIE_T2T_RUN (Category 0xf1, SigId 0x0123)

Fired for AI inference (token-to-token) workloads. Timeout: -1 (indefinite).
Permissions: system, third_party

Resources applied:
| ResCode | ResInfo | Values | Effect |
|---------|---------|--------|--------|
| 0x00f00001 | 0x00000000 | [0,1,2,3,4,5] | Affinize all IRQs to cores 0-5 |
| 0x00090002 | 0x00000000 | [2,0,1,2,3,4,5] | CPU affinity for inference threads |

---

## Target-Specific Signals

### Signal Type (SigType) Explained

SigType is a variant selector. When the post-processor detects a workload, it sets SigType
based on load intensity. URM then selects the matching signal config entry.

For video decode:

| SigType | Meaning | Trigger Condition |
|---------|---------|-------------------|
| 0 | Default (low load) | 0-4 concurrent decode sessions |
| 5 | Medium load | 5-20 concurrent decode sessions |
| 20 | High load | 20+ concurrent decode sessions |

For camera encode multi-stream:

| SigType | Meaning | Trigger Condition |
|---------|---------|-------------------|
| 0 | Normal load | 0-12 encoder instances |
| 8 | Medium load (qcm6490 only) | 8-12 encoder instances |
| 12 or 13 | High load | 12+ encoder instances |

---

## QCM6490 Signal Tuning (Configs/target-specific/qcm6490/SignalsConfig.yaml)

CPU cluster layout: cores 0-3 (little), cores 4-6 (big)

| Signal | SigType | Little Max Freq | Big Max Freq | Plus Max Freq |
|--------|---------|----------------|--------------|---------------|
| URM_SIG_VIDEO_DECODE | 0 (default) | 940800 | 940800 | 940800 |
| URM_SIG_VIDEO_DECODE | 5 (5-20 sessions) | 940800 | 1900800 | 940800 |
| URM_SIG_VIDEO_DECODE | 20 (20+ sessions) | (no freq cap) | (no freq cap) | (no freq cap) |
| URM_SIG_CAMERA_PREVIEW | 0 (default) | 940800 | 940800 | 940800 |
| URM_SIG_CAMERA_ENCODE | 0 (default) | 1804800 | 1900800 | 940800 |
| URM_SIG_CAMERA_ENCODE_MULTI_STREAMS | 0 (0-12 streams) | 1804800 | 940800 | 940800 |
| URM_SIG_CAMERA_ENCODE_MULTI_STREAMS | 8 (8-12 streams) | 1804800 | 1900800 | 1900800 |
| URM_SIG_CAMERA_ENCODE_MULTI_STREAMS | 12 (12+ streams) | (no freq cap) | (no freq cap) | (no freq cap) |

All signals also apply cgroup tuning:

| Cgroup | Cores | CPU Weight | Memory |
|--------|-------|-----------|--------|
| Background (cgroup 2) | 0,1,2,3 | - | - |
| System (cgroup 3) | 4,5,6 | 90 | HIGH: 1048576 |
| Focused (cgroup 4) | 0,1,2,3,4,5,6 | 150 | LOW: 507256, MIN: 116631 |

Note: For SigType 20 (20+ decode sessions) and SigType 12 (12+ encode streams),
no CPU frequency cap is applied - all cores are available.

---

## QCS8300 Signal Tuning (Configs/target-specific/qcs8300/SignalsConfig.yaml)

CPU cluster layout: cores 0-3 (little), cores 4-7 (big)

| Signal | SigType | Little Max Freq | Big Max Freq | Plus Max Freq |
|--------|---------|----------------|--------------|---------------|
| URM_SIG_VIDEO_DECODE | 0 (default) | 1200000 | 1200000 | 1200000 |
| URM_SIG_VIDEO_DECODE | 5 (5-20 sessions) | 1500000 | 1500000 | 1500000 |
| URM_SIG_VIDEO_DECODE | 20 (20+ sessions) | (no freq cap) | (no freq cap) | (no freq cap) |
| URM_SIG_CAMERA_PREVIEW | 0 (default) | 1200000 | 1200000 | 1200000 |
| URM_SIG_CAMERA_ENCODE | 0 (default) | 1200000 | 1200000 | 1200000 |
| URM_SIG_CAMERA_ENCODE_MULTI_STREAMS | 0 (0-12 streams) | 1500000 | 1500000 | 1500000 |
| URM_SIG_CAMERA_ENCODE_MULTI_STREAMS | 12 (12+ streams) | (no freq cap) | (no freq cap) | (no freq cap) |

All signals apply the same cgroup tuning as QCM6490 but with 8 cores (0-7).

---

## QCS9100/QCS9075 Signal Tuning (Configs/target-specific/qcs9100/SignalsConfig.yaml)

CPU cluster layout: cores 0-3 (little), cores 4-7 (big)
Note: qcs9075 uses the same config as qcs9100 (TargetsEnabled includes both).
All signals have Timeout: -1 (indefinite hold).

| Signal | SigType | Little Max Freq | Big Max Freq |
|--------|---------|----------------|--------------|
| URM_SIG_VIDEO_DECODE | 0 (default) | 1267200 | 1267200 |
| URM_SIG_VIDEO_DECODE | 5 (5+ sessions) | 1536000 | 1536000 |
| URM_SIG_VIDEO_DECODE | 20 (20+ sessions) | (no freq cap) | (no freq cap) |
| URM_SIG_CAMERA_PREVIEW | 0 (default) | 1267200 | 1267200 |
| URM_SIG_CAMERA_ENCODE | 0 (default) | 1267200 | 1267200 |
| URM_SIG_CAMERA_ENCODE_MULTI_STREAMS | 0 (0-12 streams) | 1267200 | 1267200 |
| URM_SIG_CAMERA_ENCODE_MULTI_STREAMS | 13 (12+ streams) | (no freq cap) | (no freq cap) |

Note: QCS9100/9075 does not have a CLUSTER_PLUS (third cluster); only little and big.

---

## Cross-Target Comparison

| Signal | SigType | QCM6490 Little | QCS8300 Little | QCS9100 Little |
|--------|---------|----------------|----------------|----------------|
| VIDEO_DECODE | 0 | 940800 | 1200000 | 1267200 |
| VIDEO_DECODE | 5 | 940800 | 1500000 | 1536000 |
| CAMERA_PREVIEW | 0 | 940800 | 1200000 | 1267200 |
| CAMERA_ENCODE | 0 | 1804800 | 1200000 | 1267200 |
