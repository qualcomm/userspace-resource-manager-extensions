// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <string>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <memory>
#include <mutex>
#include <thread>

#include "Helpers.h"

static std::vector<std::pair<std::string, std::string>> gIrqAffBackup;

static void irqAffinityApplierCallback(void* context) {
    LOGD("RESTUNE_COCO_TABLE", "enter irqAffinityApplierCallback");

    if(context == nullptr) return;
    Resource* resource = static_cast<Resource*>(context);

    gIrqAffBackup.clear();
    uint64_t mask = 0;
    for(int32_t i = 0; i < resource->getValuesCount(); i++) {
        mask |= ((uint64_t)1 << (resource->getValueAt(i)));
    }

    std::string dirPath = "/proc/irq/";
    DIR* dir = opendir(dirPath.c_str());
    if(dir == nullptr) {
        return;
    }

    struct dirent* entry;
    while((entry = readdir(dir)) != nullptr) {
        std::string filePath = dirPath + std::string(entry->d_name) + "/";
        filePath.append("smp_affinity");

        if(AuxRoutines::fileExists(filePath)) {
            gIrqAffBackup.emplace_back(filePath, AuxRoutines::readFromFile(filePath));

            // Convert to hex
            std::ostringstream oss;
            oss << std::hex << std::nouppercase;
            if (mask == 0) {
                oss << "0";
            } else {
                oss << mask;
            }
            std::string hexMask = oss.str();
            TYPELOGV(NOTIFY_NODE_WRITE_S, filePath.c_str(), hexMask.c_str());
            AuxRoutines::writeToFile(filePath, hexMask);
        }
    }
    closedir(dir);
}

static void irqAffinityTearCallback(void* context) {
    if(context == nullptr) return;

    for(const auto& kv : gIrqAffBackup) {
        const std::string& path = kv.first;
        const std::string& oldVal = kv.second;
        TYPELOGV(NOTIFY_NODE_RESET, path.c_str(),oldVal.c_str());
        AuxRoutines::writeToFile(path, oldVal);
    }
    gIrqAffBackup.clear();
}

static void workloadPostprocessCallback(void* context) {
    if(context == nullptr) {
        return;
    }

    PostProcessCBData* cbData = static_cast<PostProcessCBData*>(context);
    if(cbData == nullptr) {
        return;
    }

    // Match to our usecase
    cbData->mSigId = CONSTRUCT_SIG_CODE(0xf1, 0x0123);
    cbData->mSigType = DEFAULT_SIGNAL_TYPE;
}

URM_REGISTER_RES_APPLIER_CB(0x00f00001, irqAffinityApplierCallback)
URM_REGISTER_RES_TEAR_CB(0x00f00001, irqAffinityTearCallback)

// genie-t2t-run
URM_REGISTER_POST_PROCESS_CB("genie-t2t-run", workloadPostprocessCallback);
