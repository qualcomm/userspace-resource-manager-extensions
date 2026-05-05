// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <thread>
#include <string>
#include <mutex>
#include <memory>
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <algorithm>

#include "Helpers.h"
#include "PredefCallbacks.h"

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

URM_REGISTER_RES_APPLIER_CB(0x00f00001, getApplyCb(IRQ_AFFINE_ALL))
URM_REGISTER_RES_TEAR_CB(0x00f00001, getTearCb(IRQ_AFFINE_ALL))

// genie-t2t-run
URM_REGISTER_POST_PROCESS_CB("genie-t2t-run", workloadPostprocessCallback);
