#!/bin/sh
# Copyright (c) 2026-2027, Qualcomm Innovation Center, Inc. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause-Clear

THP_PATH="/sys/kernel/mm/transparent_hugepage"

for policy in `ls -d /sys/devices/system/cpu/cpufreq/policy*`;
    do echo schedutil > $policy/scaling_governor;
done

echo s2idle > /sys/power/mem_sleep
echo 100 > /proc/sys/vm/swappiness

# 15GB threshold = 15360 MB
if [ "$RAM_MB" -lt 15360 ]; then
    echo "[URM] RAM < 15GB (${RAM_MB} MB). Disabling THP"
    [ -f "$THP_PATH/enabled" ] && echo never > "$THP_PATH/enabled"
fi
