// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <fstream>
#include <dirent.h>

#include <Urm/Extensions.h>
#include <Urm/UrmPlatformAL.h>

#define POLICY_DIR_PATH "/sys/devices/system/cpu/cpufreq/"

static void writeLineToFile(const std::string& fileName, const std::string& value) {
    if(fileName.length() == 0) return;

    std::ofstream fileStream(fileName, std::ios::out | std::ios::trunc);
    if(!fileStream.is_open()) {
        return;
    }

    fileStream<<value;

    if(fileStream.fail()) {
    }

    fileStream.flush();
    fileStream.close();
}

static void governorApplierCallback(void* context) {
    DIR* dir = opendir(POLICY_DIR_PATH);
    if(dir == nullptr) {
        return;
    }

    struct dirent* entry;
    while((entry = readdir(dir)) != nullptr) {
        if(strncmp(entry->d_name, "policy", 6) == 0) {
            std::string filePath = std::string(POLICY_DIR_PATH) + "/" + entry->d_name + "/governor";
            writeLineToFile(filePath, "performance");
        }
    }
    closedir(dir);
}

static void governorTearCallback(void* context) {
    // Reset to original if needed, else no_op
    return;
}

URM_REGISTER_RES_APPLIER_CB(0x00800000, governorApplierCallback)
URM_REGISTER_RES_TEAR_CB(0x00800001, governorTearCallback)

static void postProcessCallback(void* context) {
    if(context == nullptr) {
        return;
    }

    PostProcessCBData* cbData = static_cast<PostProcessCBData*>(context);
    if(cbData == nullptr) {
        return;
    }

    // Match to our usecase
    cbData->mSigId = CONSTRUCT_SIG_CODE(0x80, 0x0001);
    cbData->mSigType = DEFAULT_SIGNAL_TYPE;
}

URM_REGISTER_POST_PROCESS_CB("<comm-name>", postProcessCallback)
