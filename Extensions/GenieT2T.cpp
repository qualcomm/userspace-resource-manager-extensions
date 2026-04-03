// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <string>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <memory>
#include <mutex>

#include <Urm/Extensions.h>
#include <Urm/UrmPlatformAL.h>

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

/* Read utime + stime for a thread */
static int64_t readThreadCpuTime(pid_t pid, pid_t tid) {
    std::string path = "/proc/" + std::to_string(pid) +
                       "/task/" + std::to_string(tid) + "/stat";
 
    std::ifstream file(path.c_str());
    if (!file.is_open()) {
        return 0;
    }
 
    std::string line;
    std::getline(file, line);
    file.close();
 
    std::istringstream iss(line);
    std::string token;
    int64_t utime = 0, stime = 0;
 
    for(int32_t i = 1; i <= 15; ++i) {
        iss >> token;
        if (i == 14) utime = atoll(token.c_str());
        if (i == 15) stime = atoll(token.c_str());
    }
 
    return utime;
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
    
    std::sort(cpuUtilization.begin(), cpuUtilization.end(), [](ThreadInfo& a, ThreadInfo& b) {
        return a.cpuUsage > b.cpuUsage;
    });

    cbData->mSigId = CONSTRUCT_SIG_CODE(0x80, 0x0002);
    cbData->mSigType = DEFAULT_SIGNAL_TYPE;

    int32_t actualArgCount = std::min((int32_t)cpuUtilization.size(), 6);
    int32_t* args = (int32_t*) calloc(actualArgCount, 0);

    for(int32_t i = 0; i < actualArgCount; i++) {
        args[i] = cpuUtilization[i].tid;
    }
    cbData->mNumArgs = actualArgCount;
    cbData->mArgs = args;
}

// genie-t2t-run
URM_REGISTER_POST_PROCESS_CB("vi", workloadPostprocessCallback);
