// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <dirent.h>
#include <algorithm>

#include "CgroupController.h"

/**
 * Read on-CPU run_time (nanoseconds) from /proc/<pid>/task/<tid>/schedstat.
 * schedstat format: "<run_time_ns> <wait_time_ns> <nr_switches>"
 * Returns 0 on success, -1 on failure.
 */
static int read_thread_schedstat(pid_t tid, pid_t pid, uint64_t *run_ns)
{
    char path[128];
    snprintf(path, sizeof(path), "/proc/%d/task/%d/schedstat", (int)pid, (int)tid);

    FILE *fp = fopen(path, "r");
    if (!fp)
        return -1;

    char buf[128];
    if (!fgets(buf, sizeof(buf), fp)) {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    /* Parse first field: run_time in nanoseconds */
    char *endptr = NULL;
    uint64_t rt = std::strtoull(buf, &endptr, 10);
    if (endptr == buf)
        return -1;

    *run_ns = rt;
    return 0;
}

/**
 * Snapshot all threads of all PIDs in the given cgroup using schedstat.
 */
static int snapshot_cgroup_threads(const char *cg_path,
                                   struct thread_sample *out,
                                   int max_threads)
{
    char procs_path[256];
    snprintf(procs_path, sizeof(procs_path), "%s/cgroup.procs", cg_path);

    FILE *fp = fopen(procs_path, "r");
    if (!fp)
        return 0;

    int count = 0;
    char line[64];

    while (fgets(line, sizeof(line), fp) && count < max_threads) {
        pid_t pid = (pid_t)std::atoi(line);
        if (pid <= 0)
            continue;

        char task_dir[128];
        snprintf(task_dir, sizeof(task_dir), "/proc/%d/task", (int)pid);

        DIR *dir = opendir(task_dir);
        if (!dir)
            continue;

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL && count < max_threads) {
            if (!std::isdigit((unsigned char)entry->d_name[0]))
                continue;

            pid_t tid = (pid_t)std::atoi(entry->d_name);
            uint64_t rns = 0;
            if (read_thread_schedstat(tid, pid, &rns) == 0) {
                out[count].tid    = tid;
                out[count].run_ns = rns;
                count++;
            }
        }
        closedir(dir);
    }
    fclose(fp);
    return count;
}

/* Sort helper for delta entries */
struct delta_entry {
    pid_t    tid;
    uint64_t delta;
};

static int cmp_delta_desc(const void *a, const void *b) {
    const struct delta_entry *da = (const struct delta_entry *)a;
    const struct delta_entry *db = (const struct delta_entry *)b;
    if (db->delta > da->delta) return  1;
    if (db->delta < da->delta) return -1;
    return 0;
}

/* ---- Public API -------------------------------------------------------- */

void initTracker(AppTrackerCtx *ctx,
                 uint32_t window_ms,
                 uint32_t st_enter_pct,
                 uint32_t st_exit_pct,
                 uint32_t dt_enter_pct,
                 uint32_t dt_exit_pct) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->window_ms             = window_ms;
    ctx->st_enter_pct          = st_enter_pct;
    ctx->st_exit_pct           = st_exit_pct;
    ctx->dt_enter_pct          = dt_enter_pct;
    ctx->dt_exit_pct           = dt_exit_pct;
    ctx->dt_min_per_thread_pct = 30;
    ctx->current_class         = APP_IDLE;
    ctx->prev_count            = 0;
}

enum WorkloadType classifyCgroup(AppTrackerCtx *ctx,
                                 const char *cg_path,
                                 struct classify_result *result) {
    struct thread_sample curr[MAX_THREADS];
    int curr_count = snapshot_cgroup_threads(cg_path, curr, MAX_THREADS);

    /* Clear result */
    result->type = APP_IDLE;
    result->hot_count = 0;
    result->hot_tids[0] = 0;
    result->hot_tids[1] = 0;

    if (curr_count == 0) {
        ctx->current_class = APP_IDLE;
        goto store;
    }

    /* Need a previous snapshot to compute deltas */
    if (ctx->prev_count == 0) {
        ctx->current_class = APP_IDLE;
        goto store;
    }

    /* Compute per-thread run_time deltas */
    {
        struct delta_entry entries[MAX_THREADS];
        int delta_count = 0;
        uint64_t total_delta = 0;

        for (int i = 0; i < curr_count; i++) {
            uint64_t prev_run = 0;
            int found = 0;

            for (int j = 0; j < ctx->prev_count; j++) {
                if (ctx->prev[j].tid == curr[i].tid) {
                    prev_run = ctx->prev[j].run_ns;
                    found = 1;
                    break;
                }
            }

            uint64_t d = 0;
            if (found && curr[i].run_ns >= prev_run)
                d = curr[i].run_ns - prev_run;

            entries[delta_count].tid   = curr[i].tid;
            entries[delta_count].delta = d;
            delta_count++;
            total_delta += d;
        }

        if (total_delta == 0) {
            ctx->current_class = APP_IDLE;
            goto store;
        }

        /* Sort descending by delta to identify top threads */
        std::qsort(entries, (size_t)delta_count, sizeof(struct delta_entry),
              cmp_delta_desc);

        uint64_t top1 = (delta_count >= 1) ? entries[0].delta : 0;
        uint64_t top2 = (delta_count >= 2) ? entries[1].delta : 0;

        uint32_t top1_pct     = (uint32_t)((top1 * 100) / total_delta);
        uint32_t top2_pct     = (uint32_t)((top2 * 100) / total_delta);
        uint32_t combined_pct = (uint32_t)(((top1 + top2) * 100) / total_delta);

        /* ST check: single thread dominates */
        uint32_t st_thresh = (ctx->current_class == APP_ST)
                                 ? ctx->st_exit_pct
                                 : ctx->st_enter_pct;
        if (top1_pct > st_thresh) {
            ctx->current_class   = APP_ST;
            result->type         = APP_ST;
            result->hot_tids[0]  = entries[0].tid;
            result->hot_count    = 1;
            goto store;
        }

        /* DT check: two threads dominate, each contributing meaningfully */
        uint32_t dt_thresh = (ctx->current_class == APP_DT)
                                 ? ctx->dt_exit_pct
                                 : ctx->dt_enter_pct;
        if (combined_pct > dt_thresh &&
            top1_pct > ctx->dt_min_per_thread_pct &&
            top2_pct > ctx->dt_min_per_thread_pct) {
            ctx->current_class   = APP_DT;
            result->type         = APP_DT;
            result->hot_tids[0]  = entries[0].tid;
            result->hot_tids[1]  = entries[1].tid;
            result->hot_count    = 2;
            goto store;
        }

        /* Otherwise: mixed threaded */
        ctx->current_class = APP_MT;
        result->type       = APP_MT;
    }

store:
    memcpy(ctx->prev, curr,
           sizeof(struct thread_sample) * (size_t)curr_count);
    ctx->prev_count = curr_count;
    return ctx->current_class;
}

void tearTracker(AppTrackerCtx *ctx) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->current_class = APP_IDLE;
}
