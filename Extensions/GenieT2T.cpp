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

#include <Urm/Extensions.h>
#include <Urm/UrmPlatformAL.h>
#include <Urm/Logger.h>

struct ThreadInfo {
    int64_t cpuUsage;
    pid_t tid;
};
 
static void getThreadIds(pid_t pid, std::vector<int>& tids) {
    std::string taskPath = "/proc/" + std::to_string(pid) + "/task";
    DIR* dir = opendir(taskPath.c_str());
    if (dir == nullptr) {
        return;
    }
 
    struct dirent* entry;
    while((entry = readdir(dir)) != nullptr) {
        if(entry->d_type == DT_DIR) {
            pid_t tid = atoi(entry->d_name);
            if(tid > 0) {
                tids.push_back(tid);
            }
        }
    }
 
    closedir(dir);
}

static int64_t readThreadCpuTime(pid_t pid, pid_t tid)
{
    std::string path = "/proc/" + std::to_string(pid) +
                       "/task/" + std::to_string(tid) + "/stat";
 
    std::ifstream file(path);
    if (!file.is_open()) {
        return 0;
    }
 
    std::string line;
    std::getline(file, line);
    file.close();
 
    // The second field (comm) is inside parentheses and may contain spaces.
    // We must skip everything up to the closing ')'.
    auto closingParen = line.rfind(')');
    if (closingParen == std::string::npos) {
        return 0;
    }
 
    // Create a stream starting after ") "
    std::istringstream iss(line.substr(closingParen + 2));
 
    /*
     * Field numbers after comm:
     *  3  state
     *  4  ppid
     *  5  pgrp
     *  6  session
     *  7  tty_nr
     *  8  tpgid
     *  9  flags
     * 10  minflt
     * 11  cminflt
     * 12  majflt
     * 13  cmajflt
     * 14  utime  <-- what we want
     */
 
    std::string token;
    for (int i = 3; i < 14; ++i) {
        iss >> token; // skip fields 3..13
    }
 
    int64_t utime = 0;
    iss >> utime;
    int64_t stime = 0;
    iss >> stime;
 
    return utime + stime;  // in clock ticks (jiffies)
}

static void workloadPostprocessCallback(void* context) {
    if(context == nullptr) {
        return;
    }

    PostProcessCBData* cbData = static_cast<PostProcessCBData*>(context);
    if(cbData == nullptr) {
        return;
    }

    pid_t pid = cbData->mPid;
    uint32_t sigId = cbData->mSigId;
    uint32_t sigType = cbData->mSigType;
    
    // Get thread list
    std::vector<pid_t> tids;
    getThreadIds(pid, tids);
 
    // sort by utilization
    std::vector<ThreadInfo> cpuUtilization(tids.size());
    for(size_t i = 0; i < tids.size(); i++) {
        cpuUtilization[i] = ThreadInfo {
            .cpuUsage = readThreadCpuTime(pid, tids[i]),
            .tid = tids[i]
        };
    }

    for(size_t i = 0; i < tids.size(); i++) {
        LOGE("URM_EXT_LOGS", "capacity = " + std::to_string(cpuUtilization[i].cpuUsage));
    }
    
    std::sort(cpuUtilization.begin(), cpuUtilization.end(), [](ThreadInfo& a, ThreadInfo& b) {
        return a.cpuUsage > b.cpuUsage;
    });

    cbData->mSigId = CONSTRUCT_SIG_CODE(0x80, 0x0002);
    cbData->mSigType = DEFAULT_SIGNAL_TYPE;

    int32_t actualArgCount = std::min((int32_t)cpuUtilization.size(), 6);
    int32_t* args = (int32_t*) calloc(actualArgCount, 0);

    LOGE("URM_EXT_LOGS", "args count passed = " + std::to_string(actualArgCount));
    for(int32_t i = 0; i < actualArgCount; i++) {
        args[i] = cpuUtilization[i].tid;
        LOGE("URM_EXT_LOGS", "arg at i = " + std::to_string(args[i]) + " with capacity = " + std::to_string(cpuUtilization[i].cpuUsage));
    }
    cbData->mNumArgs = actualArgCount;
    cbData->mArgs = args;
}

// genie-t2t-run
URM_REGISTER_POST_PROCESS_CB("genie-t2t-run", workloadPostprocessCallback);
