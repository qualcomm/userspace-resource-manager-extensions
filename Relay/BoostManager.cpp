// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <dirent.h>
#include <ctype.h>
#include <errno.h>

#include "BoostManager.h"
#include "CgroupController.h"

/* ---- Internal helpers --------------------------------------------------- */

static int write_to_file(const char *path, const char *value)
{
    FILE *fp = fopen(path, "w");
    if (!fp)
        return -1;
    if (fputs(value, fp) == EOF) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

static void get_thread_cgroup_file(const char *cg_path, char *path, size_t path_size)
{
    snprintf(path, path_size, "%s/cgroup.threads", cg_path);
    if (access(path, F_OK) == 0)
        return;

    snprintf(path, path_size, "%s/tasks", cg_path);
    if (access(path, F_OK) == 0)
        return;

    snprintf(path, path_size, "%s/cgroup.procs", cg_path);
}

static int move_tid_to_cgroup(pid_t tid, const char *cg_path)
{
    char cgroup_path[256];
    char tid_str[32];
    get_thread_cgroup_file(cg_path, cgroup_path, sizeof(cgroup_path));
    snprintf(tid_str, sizeof(tid_str), "%d", (int)tid);
    return write_to_file(cgroup_path, tid_str);
}

static int pin_tid_to_core(pid_t tid, int core)
{
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(core, &mask);
    return sched_setaffinity(tid, sizeof(mask), &mask);
}

static void reset_tid_affinity(pid_t tid)
{
    cpu_set_t all;
    CPU_ZERO(&all);
	//fetch target topology
    long ncpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (ncpus <= 0) ncpus = 8;
    for (int c = 0; c < (int)ncpus; c++)
        CPU_SET(c, &all);
    sched_setaffinity(tid, sizeof(all), &all);
}

static void build_cpuset_str(const int *cores, int count, char *buf, int bufsz)
{
    if (bufsz <= 0)
        return;

    buf[0] = '\0';
    int pos = 0;
    for (int i = 0; i < count && pos < bufsz - 4; i++) {
        if (i > 0)
            pos += snprintf(buf + pos, (size_t)(bufsz - pos), ",");
        pos += snprintf(buf + pos, (size_t)(bufsz - pos), "%d", cores[i]);
    }
}

static void build_all_cpuset_str(char *buf, int bufsz)
{
    if (bufsz <= 0)
        return;

    buf[0] = '\0';
	//todo: use target topology
    long ncpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (ncpus <= 0) ncpus = 8;
    int pos = 0;
    for (int c = 0; c < (int)ncpus && pos < bufsz - 4; c++) {
        if (c > 0)
            pos += snprintf(buf + pos, (size_t)(bufsz - pos), ",");
        pos += snprintf(buf + pos, (size_t)(bufsz - pos), "%d", c);
    }
}

static void build_cpuset_excluding(const int *excl, int excl_count,
                                   char *buf, int bufsz)
{
    if (bufsz <= 0)
        return;

    buf[0] = '\0';
	//todo: use target topology
    long ncpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (ncpus <= 0) ncpus = 8;
    int pos = 0;
    int first = 1;
    for (int c = 0; c < (int)ncpus && pos < bufsz - 4; c++) {
        int skip = 0;
        for (int j = 0; j < excl_count; j++) {
            if (excl[j] == c) { skip = 1; break; }
        }
        if (skip) continue;
        if (!first)
            pos += snprintf(buf + pos, (size_t)(bufsz - pos), ",");
        pos += snprintf(buf + pos, (size_t)(bufsz - pos), "%d", c);
        first = 0;
    }
}

static int set_cgroup_cpuset(const char *cg_path, const char *cpuset_str)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/cpuset.cpus", cg_path);
    return write_to_file(path, cpuset_str);
}

/**
 * Move ALL threads currently in a cgroup to a destination cgroup,
 * except those in the keep list.
 */
static void evict_from_cgroup(const char *src_cg, const char *dst_cg,
                              const pid_t *keep_tids, int keep_count)
{
    char cgroup_path[256];
    get_thread_cgroup_file(src_cg, cgroup_path, sizeof(cgroup_path));

    FILE *fp = fopen(cgroup_path, "r");
    if (!fp)
        return;

    pid_t pids[MAX_THREADS];
    int pid_count = 0;
    char line[64];

    while (fgets(line, sizeof(line), fp) && pid_count < MAX_THREADS) {
        pid_t p = (pid_t)atoi(line);
        if (p <= 0)
            continue;
        int keep = 0;
        for (int i = 0; i < keep_count; i++) {
            if (keep_tids[i] == p) { keep = 1; break; }
        }
        if (!keep)
            pids[pid_count++] = p;
    }
    fclose(fp);

    for (int i = 0; i < pid_count; i++)
        move_tid_to_cgroup(pids[i], dst_cg);
}

void initBoostManager(BoostManagerCtx *ctx, const BoostConfig *cfg) {
    memset(ctx, 0, sizeof(*ctx));
    memcpy(&ctx->cfg, cfg, sizeof(BoostConfig));
    ctx->boosted_count          = 0;
    ctx->dt_rotation_phase      = 0;
    ctx->dt_rotation_counter_ms = 0;
    ctx->isolation_active       = 0;
}

void boostManagerApply(BoostManagerCtx *ctx,
                       const struct classify_result *result) {
    const BoostConfig *cfg = &ctx->cfg;

    /* --- Boost reset: MT or IDLE -> move back to focused-group --- */
    if (result->type == APP_MT || result->type == APP_IDLE) {
        if (ctx->boosted_count > 0 || ctx->isolation_active)
            boostManagerReset(ctx);
        return;
    }

    /* --- Check if hot TIDs changed --- */
    int tids_changed = 0;
    if (result->hot_count != ctx->boosted_count) {
        tids_changed = 1;
    } else {
        for (int i = 0; i < result->hot_count; i++) {
            if (result->hot_tids[i] != ctx->boosted_tids[i]) {
                tids_changed = 1;
                break;
            }
        }
    }

    /* If TIDs changed and we had a prior boost, reset first */
    if (tids_changed && ctx->boosted_count > 0)
        boostManagerReset(ctx);

    /* --- Activate cluster isolation --- */
    if (!ctx->isolation_active) {
        /* Boost cgroup gets only the boost cluster cores */
        char boost_cpuset[64];
        build_cpuset_str(cfg->boostClusterCores,
                         cfg->boostClusterCoreCount,
                         boost_cpuset, sizeof(boost_cpuset));
        set_cgroup_cpuset(cfg->boostCgroupPath, boost_cpuset);

        /* Focused cgroup loses the boost cluster cores (exclusivity) */
        char focused_cpuset[128];
        build_cpuset_excluding(cfg->boostClusterCores,
                               cfg->boostClusterCoreCount,
                               focused_cpuset, sizeof(focused_cpuset));
        set_cgroup_cpuset(cfg->focusedCgroupPath, focused_cpuset);

        ctx->isolation_active = 1;
    }

    /* --- Move hot threads from focused-group to boost-group --- */
    for (int i = 0; i < result->hot_count; i++) {
        move_tid_to_cgroup(result->hot_tids[i], cfg->boostCgroupPath);
        ctx->boosted_tids[i] = result->hot_tids[i];
    }
    ctx->boosted_count = result->hot_count;

    /* --- Evict any stale/non-hot threads from boost-group --- */
    evict_from_cgroup(cfg->boostCgroupPath,
                      cfg->focusedCgroupPath,
                      ctx->boosted_tids,
                      ctx->boosted_count);

    /* --- Pin threads to favored cores --- */
    if (result->type == APP_ST) {
        /* ST: keep running on the single fast core */
        pin_tid_to_core(ctx->boosted_tids[0], cfg->st_core);

    } else if (result->type == APP_DT) {
        /* DT: place on favored dual cores */
        if (cfg->dt_favored_core_count >= 2) {
            int phase = ctx->dt_rotation_phase;
            pin_tid_to_core(ctx->boosted_tids[0],
                            cfg->favoredCoresDT[phase % cfg->dt_favored_core_count]);
            pin_tid_to_core(ctx->boosted_tids[1],
                            cfg->favoredCoresDT[(phase + 1) % cfg->dt_favored_core_count]);
        }
        if (tids_changed)
            ctx->dt_rotation_counter_ms = 0;
    }
}

void boost_manager_rotate_tick(BoostManagerCtx *ctx) {
    const BoostConfig *cfg = &ctx->cfg;

    /* Only rotate when DT with 2 boosted threads on 2+ cores */
    if (ctx->boosted_count != 2 || cfg->dt_favored_core_count < 2)
        return;

    ctx->dt_rotation_counter_ms += cfg->tick_interval_ms;

    if (ctx->dt_rotation_counter_ms < cfg->dt_rotate_interval_ms)
        return;

    /* Time to rotate: swap thread-to-core assignment */
    ctx->dt_rotation_counter_ms = 0;
    ctx->dt_rotation_phase =
        (ctx->dt_rotation_phase + 1) % cfg->dt_favored_core_count;

    int phase = ctx->dt_rotation_phase;
    pin_tid_to_core(ctx->boosted_tids[0],
                    cfg->favoredCoresDT[phase % cfg->dt_favored_core_count]);
    pin_tid_to_core(ctx->boosted_tids[1],
                    cfg->favoredCoresDT[(phase + 1) % cfg->dt_favored_core_count]);
}

void boostManagerReset(BoostManagerCtx *ctx) {
    const BoostConfig *cfg = &ctx->cfg;

    /* Move boosted threads back to focused-group */
    for (int i = 0; i < ctx->boosted_count; i++) {
        move_tid_to_cgroup(ctx->boosted_tids[i], cfg->focusedCgroupPath);
        reset_tid_affinity(ctx->boosted_tids[i]);
    }
    ctx->boosted_count = 0;

    /* Also move any remaining threads in boost-group back to focused */
    pid_t empty_keep[1] = {0};
    evict_from_cgroup(cfg->boostCgroupPath,
                      cfg->focusedCgroupPath,
                      empty_keep, 0);

    /* Remove cluster isolation: restore all cpusets */
    if (ctx->isolation_active) {
        char all_cpuset[128];
        build_all_cpuset_str(all_cpuset, sizeof(all_cpuset));
        set_cgroup_cpuset(cfg->focusedCgroupPath, all_cpuset);
        set_cgroup_cpuset(cfg->boostCgroupPath, all_cpuset);
        ctx->isolation_active = 0;
    }

    ctx->dt_rotation_phase      = 0;
    ctx->dt_rotation_counter_ms = 0;
}
