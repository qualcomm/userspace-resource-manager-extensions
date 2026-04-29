// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef PREDEF_CALLBACKS_H
#define PREDEF_CALLBACKS_H

#include "Helpers.h"

void irqAffinityApplierCallback(void* context);
void irqAffinityTearCallback(void* context);

typedef struct {
    ResourceLifecycleCallback mApply;
    ResourceLifecycleCallback mTear;
} LifecycleCbSet;

enum PredefCallbackId : int32_t {
    IRQ_AFFINE_ALL = 0,
};

static LifecycleCbSet predefCallbacks[] = {
    {irqAffinityApplierCallback, irqAffinityTearCallback},
};

inline ResourceLifecycleCallback getApplyCb(int32_t id) {
    return predefCallbacks[id].mApply;
}

inline ResourceLifecycleCallback getTearCb(int32_t id) {
    return predefCallbacks[id].mTear;
}

#endif
