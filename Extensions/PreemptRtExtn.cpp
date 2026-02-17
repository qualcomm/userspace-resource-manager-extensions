// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <fstream>
#include <dirent.h>
#include <algorithm>

#include <Urm/Extensions.h>
#include <Urm/UrmPlatformAL.h>

#define POLICY_DIR_PATH "/sys/devices/system/cpu/cpufreq/"
#define WORKQUEUE_DIR_PATH "/sys/devices/virtual/workqueue/"

static void writeLineToFile(const std::string& fileName, const std::string& value) {
    if(fileName.length() == 0) return;

    std::ofstream fileStream(fileName, std::ios::out | std::ios::trunc);
    if(!fileStream.is_open()) {
        return;
    }

    fileStream<<value;

    if(fileStream.fail()) {}

    fileStream.flush();
    fileStream.close();
}

static std::string readLineFromFile(const std::string& fileName) {
    if(fileName.length() == 0) return "";

    std::ifstream fileStream(fileName, std::ios::in);
    std::string value = "";

    if(!fileStream.is_open()) {
        return "";
    }

    if(!getline(fileStream, value)) {
        return "";
    }

    fileStream.close();
    return value;
}

// Lowercase utility
static inline void toLower(std::string &s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
}

static void getWqMask(std::string& wqMaskStr) {
    std::string machineNamePath = "/sys/devices/soc0/machine";
    std::string machineName = readLineFromFile(machineNamePath);
    toLower(machineName);

    if(machineName == "qcs9100") {
        wqMaskStr = "7F";
    } else if(machineName == "qcs8300") {
        wqMaskStr = "F7";
    } else if(machineName == "qcm6490") {
        wqMaskStr = "7F";
    }
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

static void workqueueApplierCallback(void* context) {
    DIR* dir = opendir(WORKQUEUE_DIR_PATH);
    if(dir == nullptr) {
        return;
    }

    struct dirent* entry;
    while((entry = readdir(dir)) != nullptr) {
        std::string filePath = std::string(POLICY_DIR_PATH) + "/cpumask";
        std::string wqMask = "";
        getWqMask(wqMask);
        writeLineToFile(filePath, wqMask);
    }
    closedir(dir);
}

static void governorTearCallback(void* context) {
    // Reset to original if needed, else no_op
    return;
}

URM_REGISTER_RES_APPLIER_CB(0x00800000, governorApplierCallback)
URM_REGISTER_RES_APPLIER_CB(0x00800002, workqueueApplierCallback)
URM_REGISTER_RES_TEAR_CB(0x00800000, governorTearCallback)

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

URM_REGISTER_POST_PROCESS_CB("cyclictest", postProcessCallback)
