// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "PredefCallbacks.h"

static std::vector<std::pair<std::string, std::string>> gIrqAffBackup;

void irqAffinityApplierCallback(void* context) {
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
            oss<<std::hex<<std::nouppercase;
            if(mask == 0) {
                oss<<"0";
            } else {
                oss<<mask;
            }
            std::string hexMask = oss.str();
            TYPELOGV(NOTIFY_NODE_WRITE_S, filePath.c_str(), hexMask.c_str());
            AuxRoutines::writeToFile(filePath, hexMask);
        }
    }
    closedir(dir);
}

void irqAffinityTearCallback(void* context) {
    if(context == nullptr) return;

    for(const auto& kv : gIrqAffBackup) {
        const std::string& path = kv.first;
        const std::string& oldVal = kv.second;
        TYPELOGV(NOTIFY_NODE_RESET, path.c_str(), oldVal.c_str());
        AuxRoutines::writeToFile(path, oldVal);
    }
    gIrqAffBackup.clear();
}
