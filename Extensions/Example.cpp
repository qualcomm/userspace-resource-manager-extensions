// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "Helpers.h"

static void setCpuMaxFreq(void* context) {
    if(context == nullptr) return;
    Resource* resource = static_cast<Resource*>(context);

    ResConfInfo* resConf =
        ResourceRegistry::getInstance()->getResConf(resource->getResCode());
    if(resConf == nullptr) {
        return;
    }

    int32_t clusterId = resource->getClusterValue();
    int32_t valueToWrite = resource->getValueAt(0);

    // Get Node path:
    std::string nodePath = resConf->mResourcePath;

    // Custom write to node logic follows:
}