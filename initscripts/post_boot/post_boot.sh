#!/bin/sh
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: BSD-3-Clause-Clear

check_and_execute() {
    script_path="$1"
    if [ -x "$script_path" ]; then
        "$script_path"
    fi
}

POST_BOOT_DIR="/etc/urm/initscripts/post_boot"

# Try postboot common
script="$POST_BOOT_DIR/post_boot_common.sh"
check_and_execute "$script"

# Check if there exists any target-specific post boot script
if [ -f /sys/devices/soc0/machine ]; then
    machine=$(cat /sys/devices/soc0/machine 2>/dev/null | tr '[:upper:]' '[:lower:]')
    case "$machine" in
        sa8775p)
            machine="qcs9100"
            ;;
        sa7255p)
            machine="qcs8300"
            ;;
        qcs6490)
            machine="qcm6490"
            ;;
        *)
            # Empty default case
            ;;
    esac

    # Try dynamic script resolution
    script="$POST_BOOT_DIR/post_boot_${machine}.sh"
    check_and_execute "$script"
fi
