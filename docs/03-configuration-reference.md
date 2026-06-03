# 3. Configuration Reference

URM Extensions uses four YAML configuration files plus target-specific overrides.

## File Overview

| File | Purpose | Scope |
|------|---------|-------|
| ResourcesConfig.yaml | Define custom resources (sysfs paths, policies, thresholds) | Generic + target-specific |
| SignalsConfig.yaml | Define custom signals and their resource bundles | Generic + target-specific |
| PerApp.yaml | Map process names to cgroup identifiers and resource configs | Generic |
| InitConfig.yaml | IRQ affinity initialization settings | Generic |

---

## ResourcesConfig.yaml

**Installed to**: /etc/urm/target/ResourcesConfig.yaml

### Schema

Each entry under ResourceConfigs defines one resource:

    ResType:       Resource type byte (hex). 0x05=GPU, 0x80=custom/RT, 0xf0=special
    ResID:         Resource ID within type (hex)
    Name:          Human-readable name used in SignalsConfig
    Path:          sysfs path to write; empty string for callback-only resources
    Supported:     true/false - whether this resource is active
    HighThreshold: Maximum allowed value (optional)
    LowThreshold:  Minimum allowed value (optional)
    Permissions:   third_party or system
    Modes:         Display modes where resource is active (display_on, doze)
    Policy:        lower_is_better | higher_is_better | pass_through
    ApplyType:     global

### ResCode Calculation

    ResCode = (ResType << 16) | ResID
    Example: ResType=0x05, ResID=0x0003 => ResCode = 0x00050003

### Policy Values

| Policy | Behavior |
|--------|----------|
| lower_is_better | Lowest value wins (most conservative); used for frequency caps |
| higher_is_better | Highest value wins (most aggressive); used for minimum floors |
| pass_through | Most recent request wins; no arbitration; used for RT/special resources |

### Modes

| Mode | Meaning |
|------|---------|
| display_on | Resource active when display is on |
| doze | Resource active in doze/low-power display state |

---

## SignalsConfig.yaml

**Installed to**: /etc/urm/target/SignalsConfig.yaml (generic)
**Also**: /etc/urm/target/<target>/SignalsConfig.yaml (target-specific)

### Schema

Each entry under SignalConfigs defines one signal variant:

    SigId:          Signal ID (hex)
    Category:       Signal category byte (hex)
    SigType:        Signal type/variant (integer, default 0)
    Name:           Human-readable name matching URM_SIG_* enum
    Enable:         true/false
    TargetsEnabled: List of target names where this config applies
    Permissions:    List of permission levels (system, third_party)
    Timeout:        Duration in ms; -1 = indefinite hold
    Resources:      List of resource tuning actions

### Resource Entry Fields

| Field | Description |
|-------|-------------|
| ResCode | Resource code as hex (e.g. 0x00050003) or symbolic name from ResourcesConfig |
| ResInfo | Additional info (e.g., cluster selector for CPU freq resources) |
| Values | List of values. For cgroup resources, first value is cgroup ID |

### Full Signal Code

    Full Signal Code = (Category << 16) | SigId
    Example: Category=0x03, SigId=0x0001 => 0x00030001 (URM_SIG_VIDEO_DECODE)

---

## PerApp.yaml

**Installed to**: /etc/urm/target/PerApp.yaml

### Schema

    App:            Process name prefix to match (against /proc/PID/comm)
    Threads:        Map of thread name substrings to cgroup identifiers
    Configurations: List of resource codes configurable per-app

### Cgroup Identifiers

| Identifier | Meaning |
|------------|---------|
| FOCUSED_CGROUP_IDENTIFIER | Places thread in the focused (foreground) cgroup |

### Current Entries

| App | Thread Pattern | Cgroup | Configurations |
|-----|---------------|--------|----------------|
| gst-launch- | cam-server | FOCUSED_CGROUP_IDENTIFIER | - |
| gst-launch- | gst-launch- | FOCUSED_CGROUP_IDENTIFIER | - |
| cyclictest | cyclictest | FOCUSED_CGROUP_IDENTIFIER | 0x00800001 (RES_CPU_FREQ_GOV) |

---

## InitConfig.yaml

**Installed to**: /etc/urm/target/InitConfig.yaml

### Current Configuration

    InitConfigs:
      - IRQConfigs:
          - AffineIRQToCluster: [-1, 0]

This affinizes all IRQs (index -1 = all) to cluster 0 (little cores) at initialization.

---

## Target-Specific Config Directory Structure

    Configs/
      ResourcesConfig.yaml          # Generic resources (all targets)
      SignalsConfig.yaml            # Generic signals (all targets)
      PerApp.yaml                   # Per-app config (all targets)
      InitConfig.yaml               # IRQ init config (all targets)
      target-specific/
        qcm6490/SignalsConfig.yaml  # QCM6490-specific signal tuning
        qcs8300/SignalsConfig.yaml  # QCS8300-specific signal tuning
        qcs9100/SignalsConfig.yaml  # QCS9100/QCS9075-specific signal tuning

---

## Config Validation Tips

1. **ResCode must match** between ResourcesConfig.yaml and SignalsConfig.yaml.
2. **TargetsEnabled** must exactly match the lowercased machine name from /sys/devices/soc0/machine.
3. **Timeout: -1** means the resource is held indefinitely until the client releases it.
4. **Callback-only resources** (empty Path) require a registered URM_REGISTER_RES_APPLIER_CB.
5. **SigType** defaults to 0 if not specified; multiple entries with the same SigId but different SigType are all valid.