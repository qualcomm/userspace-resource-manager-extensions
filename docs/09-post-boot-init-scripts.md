# 9. Post-Boot Init Scripts

**Directory**: initscripts/post_boot/

---

## Purpose

Post-boot scripts apply target-specific kernel and sysfs tuning at system startup, for example to configure CPU governor, RT scheduling, memory management, etc.

---

## Script Inventory

| Script | Purpose |
|--------|--------|
| post_boot.sh | Dispatcher: runs common + target-specific scripts |
| post_boot_qcm6490.sh | QCM6490 kernel tuning |
| post_boot_qcs8300.sh | QCS8300 kernel tuning |
| post_boot_qcs9075.sh | QCS9075 kernel tuning |
| post_boot_qcs9100.sh | QCS9100 kernel tuning |

All scripts are installed to /etc/urm/initscripts/post_boot/ with execute permissions.

---

## Dispatcher Script (post_boot.sh)

The dispatcher script:
1. Runs post_boot_common.sh (if it exists and is executable).
2. Reads /sys/devices/soc0/machine to detect the target name.
3. Lowercases the machine name using tr.
4. Runs post_boot_MACHINE.sh if it exists and is executable.

Target detection is fully automatic - no manual configuration needed.

---

## Target-Specific Script Contents

All target-specific scripts (qcm6490, qcs8300, qcs9100, qcs9075) apply the target specific tuning based on machine name.

---

## Tuning Summary Table

| Setting | Value | Rationale |
|---------|-------|----------|
| CPU governor | schedutil | Energy-aware; tracks actual CPU load |
| sched_util_clamp_min_rt_default | 128 | Prevents RT task throttling |
| mem_sleep | s2idle | Fast resume for embedded/always-on use |
| vm.swappiness | 100 | Aggressive swap to free RAM for workloads |
| vm.compaction_proactiveness | 0 | Disable THP compaction (THP not used) |
| camera-framework cgroup | created | Required for camera workload isolation |

---

## Adding a New Target Script

1. Create initscripts/post_boot/post_boot_NEWTARGET.sh
2. The dispatcher will automatically pick it up based on /sys/devices/soc0/machine
