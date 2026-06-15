# 8. Post-Boot Init Scripts

**Directory**: initscripts/post_boot/

---

## Purpose

Post-boot scripts apply target-specific kernel and sysfs tuning at system startup, for example to configure CPU governor, RT scheduling, memory management, etc.

---

## Script Inventory

| Script | Purpose |
|--------|--------|
| post_boot.sh | Dispatcher: runs common + target-specific scripts |
| post_boot_common.sh | Common kernel tuning applied to all targets |
| post_boot_alorp.sh | ALORP kernel tuning |
| post_boot_qcm6490.sh | QCM6490 kernel tuning |
| post_boot_qcs615.sh | QCS615 kernel tuning |
| post_boot_qcs8300.sh | QCS8300 kernel tuning |
| post_boot_qcs9075.sh | QCS9075 kernel tuning |
| post_boot_qcs9100.sh | QCS9100 kernel tuning |

All scripts are installed to /etc/urm/initscripts/post_boot/ with execute permissions (755).

---

## Dispatcher Script (post_boot.sh)

The dispatcher script:
1. Reads /proc/meminfo to compute RAM_MB and exports it for use by child scripts.
2. Runs post_boot_common.sh (if it exists and is executable).
3. Reads /sys/devices/soc0/machine to detect the target name.
4. Lowercases the machine name using `tr`.
5. Applies machine name aliases:
   - `sa8775p` → `qcs9100`
   - `sa7255p` → `qcs8300`
   - `qcs6490` → `qcm6490`
6. Runs post_boot_${machine}.sh if it exists and is executable.

Target detection is fully automatic - no manual configuration needed.

---

## Common Script (post_boot_common.sh)

Applied to all targets before the target-specific script. Settings:

| Setting | Value | Rationale |
|---------|-------|----------|
| CPU governor | schedutil | Energy-aware; tracks actual CPU load |
| mem_sleep | s2idle | Fast resume for embedded/always-on use |
| vm.swappiness | 100 | Aggressive swap to free RAM for workloads |
| THP enabled | never (if RAM < 15 GB) | Disable transparent huge pages on low-RAM targets |
| ZRAM mem_limit | RAM_MB / 2 | Cap ZRAM usage to half of physical RAM |

---

## Target-Specific Script Contents

All target-specific scripts (alorp, qcm6490, qcs615, qcs8300, qcs9075, qcs9100) apply the following settings:

| Setting | Value | Rationale |
|---------|-------|----------|
| sched_util_clamp_min_rt_default | 0 | Disable minimum utilization clamp for RT tasks |
| vm.compaction_proactiveness | 0 | Disable proactive THP compaction (THP not used) |

Scripts for qcm6490, qcs615, qcs8300, qcs9075, and qcs9100 additionally apply:

| Setting | Value | Rationale |
|---------|-------|----------|
| system.slice cpuset.cpus | 0-3 | Restrict system slice to little cores |
| kernel.printk | 4 | Suppress debug/info kernel messages |

The alorp script does not restrict system.slice or printk level.

---

## Adding a New Target Script

1. Create initscripts/post_boot/post_boot_NEWTARGET.sh
2. The dispatcher will automatically pick it up based on /sys/devices/soc0/machine
3. If the hardware reports a different machine name, add an alias case to post_boot.sh
