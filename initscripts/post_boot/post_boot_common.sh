#!/bin/sh
# Copyright (c) 2026-2027, Qualcomm Innovation Center, Inc. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause-Clear

for policy in `ls -d /sys/devices/system/cpu/cpufreq/policy*`;
    do echo schedutil > $policy/scaling_governor;
done

echo s2idle > /sys/power/mem_sleep
echo 100 > /proc/sys/vm/swappiness
