# 4. Resources Reference

All custom resources defined in Configs/ResourcesConfig.yaml.

---

## GPU Resources (ResType 0x05)

These resources control the Qualcomm KGSL (Kernel Graphics Support Layer) GPU driver via sysfs.

| Resource Name | ResCode | sysfs Path | Policy | Modes |
|---------------|---------|-----------|--------|-------|
| RES_KGSL_DEF_PWRLEVEL | 0x00050003 | /sys/class/kgsl/kgsl-3d0/default_pwrlevel | lower_is_better | display_on, doze |
| RES_KGSL_DEVFREQ_MAX | 0x00050004 | /sys/class/kgsl/kgsl-3d0/devfreq/max_freq | lower_is_better | display_on, doze |
| RES_KGSL_DEVFREQ_MIN | 0x00050005 | /sys/class/kgsl/kgsl-3d0/devfreq/min_freq | lower_is_better | display_on, doze |
| RES_KGSL_IDLE_TIMER | 0x00050006 | /sys/class/kgsl/kgsl-3d0/idle_timer | higher_is_better | display_on, doze |
| RES_KGSL_MAX_PWRLEVEL | 0x00050007 | /sys/class/kgsl/kgsl-3d0/max_pwrlevel | higher_is_better | display_on, doze |
| RES_KGSL_MIN_PWRLEVEL | 0x00050008 | /sys/class/kgsl/kgsl-3d0/min_pwrlevel | higher_is_better | display_on, doze |
| RES_KGSL_TOUCH_WAKE | 0x00050009 | /sys/class/kgsl/kgsl-3d0/touch_wake | higher_is_better | display_on, doze |

### GPU Resource Details

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

---

## RT Benchmark Resources (ResType 0x80)

These resources support real-time benchmarking (cyclictest) and require custom C++ callbacks.

| Resource Name | ResCode | sysfs Path | Policy | Description |
|---------------|---------|-----------|--------|-------------|
| RES_TIMER_MIGRATION | 0x00800000 | /proc/sys/kernel/timer_migration | pass_through | Kernel timer migration control |
| RES_CPU_FREQ_GOV | 0x00800001 | (callback) | pass_through | CPU frequency governor selector |
| RES_IRQ_AFFINITY | 0x00800002 | (callback) | pass_through | IRQ affinity configuration |
| RES_CPU_WQ_AFFINITY | 0x00800003 | (callback) | pass_through | CPU workqueue affinity |

### RT Resource Details

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

---

## Special Resources (ResType 0xf0)

| Resource Name | ResCode | sysfs Path | Policy | Description |
|---------------|---------|-----------|--------|-------------|
| RES_IRQ_AFFINE_ALL | 0x00f00001 | (callback) | pass_through | Affinize all IRQs to specified cores |

**RES_IRQ_AFFINE_ALL** (0x00f00001)
- Affinizes all system IRQs to a specified set of CPU cores.
- Used by the GENIE_T2T_RUN signal for AI inference workloads.
- Modes: display_on only.
- No sysfs path; requires a custom applier callback.

---

## Resource Code Quick Reference

| ResCode | Name | Category | Has sysfs Path |
|---------|------|----------|----------------|
| 0x00050003 | RES_KGSL_DEF_PWRLEVEL | GPU | Yes |
| 0x00050004 | RES_KGSL_DEVFREQ_MAX | GPU | Yes |
| 0x00050005 | RES_KGSL_DEVFREQ_MIN | GPU | Yes |
| 0x00050006 | RES_KGSL_IDLE_TIMER | GPU | Yes |
| 0x00050007 | RES_KGSL_MAX_PWRLEVEL | GPU | Yes |
| 0x00050008 | RES_KGSL_MIN_PWRLEVEL | GPU | Yes |
| 0x00050009 | RES_KGSL_TOUCH_WAKE | GPU | Yes |
| 0x00800000 | RES_TIMER_MIGRATION | RT Benchmark | Yes (/proc) |
| 0x00800001 | RES_CPU_FREQ_GOV | RT Benchmark | No (callback) |
| 0x00800002 | RES_IRQ_AFFINITY | RT Benchmark | No (callback) |
| 0x00800003 | RES_CPU_WQ_AFFINITY | RT Benchmark | No (callback) |
| 0x00f00001 | RES_IRQ_AFFINE_ALL | Special | No (callback) |

---

## Adding a New Resource

1. Choose a ResType and ResID that does not conflict with existing entries.
2. Add the entry to Configs/ResourcesConfig.yaml.
3. If Path is empty, register a custom applier via URM_REGISTER_RES_APPLIER_CB.
4. Reference the resource by ResCode or Name in SignalsConfig.yaml.
5. Reinstall configs: sudo cp Configs/ResourcesConfig.yaml /etc/urm/target/

See the Extension API Guide (06-extension-api-guide.md) for callback implementation details.
