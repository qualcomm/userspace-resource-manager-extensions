// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <fstream>
#include <dirent.h>
#include <algorithm>

#include <Urm/Extensions.h>
#include <Urm/UrmPlatformAL.h>

#include "Helpers.h"

#define POLICY_DIR_PATH "/sys/devices/system/cpu/cpufreq/"

static void cpuFreqApplierCallback(void* context) {
	/*Resource* res = static_cast<Resource*>(context);
	int32_t cluster = res->getClusterValue();
	int64_t value = res->getValueAt(0);
	//Get Path 
	std::string path = "/sys/kernel/msm_performance/parameters/cpu_max_freq";

    / # cat /sys/kernel/msm_performance/parameters/cpu_max_freq
        0:2147483647 1:2147483647 2:2147483647 3:2147483647 4:2147483647 5:2147483647 6:2147483647 7:2147483647
    / # cat /sys/kernel/msm_performance/parameters/cpu_min_freq
        0:0 1:0 2:0 3:0 4:0 5:0 6:0 7:0
   / #
	//Save the value of this Node 
	//write the requested value
	*/
	printf("URM-ext: cpuFreqApplierCallback");
	return;
}

static void cpuFreqTearCallback(void* context) {
    // Reset to original if needed, else no_op
    printf("URM-ext: cpuFreqTearCallback");
    return;
}

URM_REGISTER_RES_APPLIER_CB(0x00900001, cpuFreqApplierCallback)
URM_REGISTER_RES_TEAR_CB(0x00900001, cpuFreqTearCallback)

static void postProcessCallback(void* context) {
    if(context == nullptr) {
        return;
    }

    PostProcessCBData* cbData = static_cast<PostProcessCBData*>(context);
    if(cbData == nullptr) {
        return;
    }

    // Match to our usecase
    cbData->mSigId = CONSTRUCT_SIG_CODE(0x81, 0x0001);
    cbData->mSigType = DEFAULT_SIGNAL_TYPE;
}

URM_REGISTER_POST_PROCESS_CB("gst-launch-1.0", postProcessCallback)
