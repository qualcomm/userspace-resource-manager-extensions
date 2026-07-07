// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef BOOST_DETECTION_H
#define BOOST_DETECTION_H

#include <cstdint>
#include <sys/types.h>

#define MAX_HOT_THREADS 2
#define BOOST_MAX_CORES 18

struct classify_result;

typedef struct {
    /* Boost-group cgroup: hot threads live here during boost */
    const char *boostCgroupPath;

    /* Focused-group cgroup: source of workload threads; teardown target */
    const char *focusedCgroupPath;

    /* Favored cores for DT rotation (e.g., core 6 and core 7) */
    int32_t favoredCoresDT[BOOST_MAX_CORES];
    int32_t dt_favored_core_count;

    /* Core for ST pinning (e.g., core 7 - the fastest) */
    int32_t st_core;

    /* All cores in the boost cluster (for exclusive isolation) */
    int32_t boostClusterCores[BOOST_MAX_CORES];
    int32_t boostClusterCoreCount;

    /* DT thread rotation interval in milliseconds (configurable) */
    uint32_t dt_rotate_interval_ms;

    /* Classification tick interval in ms (for rotation counter math) */
    uint32_t tick_interval_ms;
} BoostConfig;

/* -------------------------------------------------------------------------
 * Runtime state
 * ------------------------------------------------------------------------- */
typedef struct {
    BoostConfig cfg;

    /* Currently boosted thread TIDs */
    int32_t boosted_count;
    pid_t boosted_tids[MAX_HOT_THREADS];

    /* DT rotation state */
    int32_t dt_rotation_phase;
    uint32_t dt_rotation_counter_ms;

    /* Whether cluster isolation is currently active */
    int32_t isolation_active;
} BoostManagerCtx;

/* -------------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------------- */

void initBoostManager(BoostManagerCtx *ctx, const BoostConfig *cfg);

/**
 * @brief Apply boost based on classification.
 *
 * If ST or DT:
 *   - Move hot threads from focused-group to boost-group
 *   - Pin to favored cores
 *   - Isolate boost cluster (no other threads allowed)
 *
 * If MT or IDLE (boost reset):
 *   - Move boost-group threads back to focused-group
 *   - Remove isolation
 */
void boostManagerApply(BoostManagerCtx *ctx,
                       const struct classify_result *result);

/**
 * @brief DT rotation tick. Call every classification tick.
 *        Internally counts up to dt_rotate_interval_ms before rotating.
 */
void boost_manager_rotate_tick(BoostManagerCtx *ctx);

/**
 * @brief Boost reset: move all boost-group threads back to focused-group,
 *        remove cluster isolation, reset affinities.
 */
void boostManagerReset(BoostManagerCtx *ctx);

#endif
