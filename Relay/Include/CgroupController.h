// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef DYN_WL_CGROUP_CONTROLLER_H
#define DYN_WL_CGROUP_CONTROLLER_H

#include <stdint.h>
#include <sys/types.h>

enum WorkloadType {
    APP_IDLE = 0,
    APP_ST,         /* single-threaded: 1 thread > 80% runtime     */
    APP_DT,         /* dual-threaded:   top-2 > 85%, each > 30%    */
    APP_MT,         /* mixed/multi-threaded                         */
};

#define MAX_THREADS 512
#define MAX_HOT_THREADS 2

/* Per-thread snapshot using schedstat (nanosecond CPU run_time) */
struct thread_sample {
    pid_t    tid;
    uint64_t run_ns;   /* cumulative on-CPU time from schedstat field 1 */
};

/* Classification result with identified hot thread TIDs */
struct classify_result {
    enum WorkloadType  type;
    pid_t               hot_tids[MAX_HOT_THREADS]; /* top-1 or top-2 TIDs */
    int                 hot_count;                  /* 1 for ST, 2 for DT  */
};

typedef struct {
    uint32_t window_ms;
    uint32_t st_enter_pct;
    uint32_t st_exit_pct;
    uint32_t dt_enter_pct;
    uint32_t dt_exit_pct;
    uint32_t dt_min_per_thread_pct;
    struct thread_sample prev[MAX_THREADS];
    int32_t prev_count;
    enum WorkloadType   current_class;
} AppTrackerCtx;

void initTracker(AppTrackerCtx *ctx,
                 uint32_t window_ms,
                 uint32_t st_enter_pct,
                 uint32_t st_exit_pct,
                 uint32_t dt_enter_pct,
                 uint32_t dt_exit_pct);

enum WorkloadType classifyCgroup(AppTrackerCtx *ctx,
                                 const char *cg_path,
                                 struct classify_result *result);

void tearTracker(AppTrackerCtx *ctx);

#endif
