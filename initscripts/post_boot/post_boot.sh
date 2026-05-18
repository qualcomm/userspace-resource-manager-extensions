#!/bin/sh
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: BSD-3-Clause-Clear

if [ -f /sys/devices/soc0/machine ]; then
    machine=`cat /sys/devices/soc0/machine`
fi

case "$machine" in
    QCM6490 | qcm6490 | QCS6490 | qcs6490)
        /etc/urm/initscripts/post_boot/post_boot_qcm6490.sh
        ;;
    QCM8300 | qcm8300)
        /etc/urm/initscripts/post_boot/post_boot_qcm8300.sh
        ;;
    QCS9100 | qcs9100)
        /etc/urm/initscripts/post_boot/post_boot_qcs9100.sh
        ;;
    QCS9075 | qcs9075)
        /etc/urm/initscripts/post_boot/post_boot_qcs9075.sh
        ;;
    *)
        # Empty default case
        ;;
esac
