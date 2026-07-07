// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <mutex>
#include <memory>
#include <string>
#include <fstream>
#include <sstream>
#include <cstring>
#include <thread>
#include <atomic>
#include <chrono>
#include <unistd.h>
#include <algorithm>

#include "BoostManager.h"
#include "CgroupController.h"
#include <Urm/UrmPlatformAL.h>
#include <Urm/SignalInternal.h>

// Configurations
// Timer Interval
#define CLASSIFY_TICK_MS        500

// DT Rotation Interval
#define DT_ROTATE_INTERVAL_MS   1000

// Boost Cluster
#define BOOST_CLUSTER_COUNT  6
static int boostCluster[] = {12,13,14,15,16,17};

// Preferred DT Cores
#define DT_FAVORED_CORE_COUNT  2
static int favoredCoresDT[] = {13, 16};

// Preferred ST Core
#define ST_CORE  14

#define ST_ENTER_THRESH 80
#define ST_RETAIN_THRESH 70
#define DT_ENTER_THRESH 85
#define DT_RETAIN_THRESH 75

#define URM_INVALID_HANDLE -1
static int64_t restuneHandle = URM_INVALID_HANDLE;

// Cgroup paths
static const char *focusedCgroup =
    "/sys/fs/cgroup/urm.slice/focused.apps";
static const char *boostCgroup =
    "/sys/fs/cgroup/urm.slice/boost.group";
static const char *systemSliceCgroup =
    "/sys/fs/cgroup/system.slice";
static const char *userSliceCgroup =
    "/sys/fs/cgroup/user.slice";

// State
static AppTrackerCtx gAppTracker;
static BoostManagerCtx gBoostMgr;
static enum WorkloadType prevClass = APP_IDLE;
static std::thread gWorkloadDetectionThread;
static std::atomic<bool> gWorkloadDetectionTimerRunning(false);

#define URM_SIG_ST_DETECTED 0x00800010
#define URM_SIG_DT_DETECTED 0x00800011

static uint32_t getSigCodeForWorkload(enum WorkloadType type) {
    switch (type) {
        case APP_ST: 
            return URM_SIG_ST_DETECTED;
        case APP_DT: 
            return URM_SIG_DT_DETECTED;
        case APP_IDLE:
        case APP_MT:
            return 0;
    }

    return 0;
}

static void workloadDetectionTick(void) {
    struct classify_result result;
    classifyCgroup(&gAppTracker, focusedCgroup, &result);

    /* Apply boost placement policy (move/pin/isolate or reset) */
    boostManagerApply(&gBoostMgr, &result);

    /* Rotate DT thread placement at configured interval */
    if(result.type == APP_DT) {
        boost_manager_rotate_tick(&gBoostMgr);
    }

    // Signal management: only on state transition
    if(result.type == prevClass) {
        return;
    }

    // Untune previous signal
    if (restuneHandle != URM_INVALID_HANDLE) {
        // untuneSignal(restuneHandle);
        restuneHandle = URM_INVALID_HANDLE;
    }

    // Tune new signal for ST or DT
    uint32_t sigCode = getSigCodeForWorkload(result.type);
    if(sigCode != 0) {
        restuneHandle = acquireSignal(
            sigCode,
            DEFAULT_SIGNAL_TYPE,
            getpid(),
            getpid()
        );
    }

    prevClass = result.type;
}

static void workloadDetectionTimerLoop() {
    while(gWorkloadDetectionTimerRunning.load()) {
        workloadDetectionTick();
        std::this_thread::sleep_for(std::chrono::milliseconds(CLASSIFY_TICK_MS));
    }
}

static void setCgroupCpuset(const char *cgPath, const char *cpuset) {
    std::string cpusetPath = std::string(cgPath) + "/cpuset.cpus";
    std::ofstream file(cpusetPath, std::ios::out | std::ios::trunc);
    if(file.is_open()) {
        file << cpuset;
        file.close();
    }
}

extern "C" void initFeature() {
    if(gWorkloadDetectionTimerRunning.load()) {
        return;
    }

    // initialize App Tracker
    initTracker(
        &gAppTracker,
        CLASSIFY_TICK_MS, // window_ms / timer-duration
        ST_ENTER_THRESH,  // thresh for being classifier as ST
        ST_RETAIN_THRESH, // thresh for retaining ST classification
        DT_ENTER_THRESH,  // thresh for being classified as DT
        DT_RETAIN_THRESH  // thresh for retaining DT classification
    ); 

    // Initialize boost manager
    BoostConfig bcfg;
    memset(&bcfg, 0, sizeof(bcfg));
    bcfg.boostCgroupPath        = boostCgroup;
    bcfg.focusedCgroupPath      = focusedCgroup;
    bcfg.favoredCoresDT[0]      = favoredCoresDT[0];
    bcfg.favoredCoresDT[1]      = favoredCoresDT[1];
    bcfg.dt_favored_core_count  = DT_FAVORED_CORE_COUNT;
    bcfg.st_core                = ST_CORE;
    bcfg.boostClusterCores[0]   = boostCluster[0];
    bcfg.boostClusterCores[1]   = boostCluster[1];
    bcfg.boostClusterCores[2]   = boostCluster[2];
    bcfg.boostClusterCores[3]   = boostCluster[3];
    bcfg.boostClusterCores[4]   = boostCluster[4];
    bcfg.boostClusterCores[5]   = boostCluster[5];
    bcfg.boostClusterCoreCount  = BOOST_CLUSTER_COUNT;
    bcfg.dt_rotate_interval_ms  = DT_ROTATE_INTERVAL_MS;
    bcfg.tick_interval_ms       = CLASSIFY_TICK_MS;

    initBoostManager(&gBoostMgr, &bcfg);

    /* Keep non-URM system/user work off the boost cores */
    setCgroupCpuset(systemSliceCgroup, "0-5");
    setCgroupCpuset(userSliceCgroup, "0-5");

    /* Start periodic workload detection timer */
    if(!gWorkloadDetectionTimerRunning.load()) {
        gWorkloadDetectionTimerRunning.store(true);
        gWorkloadDetectionThread = std::thread(workloadDetectionTimerLoop);
    }
}

extern "C" void tearFeature() {
    // Stop periodic workload detection timer
    gWorkloadDetectionTimerRunning.store(false);
    if(gWorkloadDetectionThread.joinable()) {
        gWorkloadDetectionThread.join();
    }

    // Untune any held signal
    if(restuneHandle != URM_INVALID_HANDLE) {
        // untuneSignal(restuneHandle);
        restuneHandle = URM_INVALID_HANDLE;
    }

    /* Boost reset: move threads back to focused-group, remove isolation */
    boostManagerReset(&gBoostMgr);

    /* Cleanup classifier */
    tearTracker(&gAppTracker);
}
