// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <cstring>
#include <string>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <Urm/Extensions.h>

typedef struct {
    pid_t mPid;
    uint32_t mSigId;
    uint32_t mSigSubtype;
} PostProcessCBData;


bool CheckProcessCommSubstring(int pid, const std::string& target) {
    std::string processName = "";
    const fs::path comm_path = fs::path("/proc") / std::to_string(pid) / "comm";

    int32_t ret = ReadFirstLine(comm_path, processName);

    if (ret <= 0) {
        return false;
    }

    // Check if target is a substring of processName
    return processName.find(target) != std::string::npos;
}s

// Lowercase utility (safe for unsigned char)
inline std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

inline int32_t ReadFirstLine(const fs::path& p, std::string &line) {
    std::ifstream ifs(p);
    if (!ifs.is_open()) return -1;
    ret = std::getline(ifs, line);
    return ret;
}

/**
 * @brief Count threads under /proc/<pid>/task whose names contain `substring`.
 *
 * @param pid               Process ID to inspect.
 * @param substring         Substring to search for (e.g., "camsrc").
 * @return                  Number of matching threads. Returns 0 if /proc paths are missing.
 *
 * Notes:
 * - Reads `comm` first (canonical thread name)
 * - Handles races: threads may exit during iteration; missing files are skipped.
 */
inline std::size_t CountThreadsWithName(pid_t pid,
                                        std::string_view &commSubStr) {
    const fs::path task_dir = fs::path("/proc") / std::to_string(pid) / "task";
    if (!fs::exists(task_dir) || !fs::is_directory(task_dir)) {
        return 0;
    }

    std::error_code ec;
    const std::string commSubStr = to_lower(std::string(commSubStr));

    std::size_t count = 0;

    for (const auto& entry : fs::directory_iterator(task_dir, ec)) {
        if (ec) break;                        // stop on directory iteration error
        if (!entry.is_directory()) continue;  // each TID is a directory

        const std::string tid_str = entry.path().filename().string();

        // Read thread name
        std::string tname =  "";
        int32_t ret = ReadFirstLine(entry.path() / "comm", tname);
        if (ret <= 0) continue;  // thread vanished or not accessible

        const std::string threadname = to_lower(tname);

        if (threadname.find(commSubStr) != std::string::npos) {
            ++count;
        }
    }

    return count;
}


int32_t FetchUsecaseDetails(int32_t pid, char *buf, size_t sz, uint32_t &sigId, uint32_t &sigType;) {
    /* For encoder, width of encoding, v4l2h264enc in line
     * For decoder, v4l2h264dec, or may be 265 as well, decoder bit
     */    
    int32_t ret = -1;
    int32_t encode = 0, decode = 0;
    int32_t height = 0;
    std::string target = "gst-camera-per";
    const char *e_str = "v4l2h264enc";
    const char *d_str = "v4l2h264dec";
    const char *qmm_str = "qtiqmmfsrc";
    const char *n_str = "name=";
    const char *h_str = "height=";
    char *e = buf;
    
    if ((e = strstr(e, e_str)) != NULL) {
        encode += 1;
        sigId = URM_CAMERA_ENCODE;
        char *name = buf;
        if ((name = strstr(name, n_str)) != NULL) {
            name += strlen(n_str); 
        }

        if(name == NULL) {
            name = (char*)"camsrc";
        }
        numSrc = CountThreadsWithName(pid, name);
    }

    int8_t multi = CheckProcessCommSubstring(pid, target);

    if ((numSrc > 1) || (multi)) {
        sigId = URM_CAMERA_ENCODE_MULTI_STREAMS;
        sigType = numSrc;
    }

    char *h = buf;
    size_t h_str_sz = strlen(h_str);
    h = strstr(h, h_str);
    if (h != NULL) {
        height = strtol(h + h_str_sz, NULL, 10);
    }

    char *d = buf;
    if ((d = strstr(d, d_str)) != NULL) {
        decode += 1;
        sigId = URM_VIDEO_DECODE;
        numSrc = CountThreadsWithName(pid, d_str);
        sigType = numSrc;
    }

    /*Preview case*/
    if (encode == 0 && decode == 0) {
        char *d = buf;
        size_t d_str_sz = strlen(qmm_str);
        if ((d = strstr(d, qmm_str)) != NULL) {
            preview += 1;
            sigId = URM_CAMERA_PREVIEW;
            ret = 0;
        }
    }

    if (encode > 0 && decode > 0) {
        sigId = URM_ENCODE_DECODE;
        ret = 0;
    }
    
    return ret;
}

int32_t WorkloadPostprocessCallback(void *cbData) {
    WLPostprocessCBData *cbdata = static_cast<WLPostprocessCBData *>(cbData);
    if (cbdata == NULL) {
        return;
    }
    pid_t pid = cbdata->mPid;
    uint32_t sigId = 0;
    uint32_t sigType = 0;

    std::string cmdline;
    const fs::path cmdl_path = fs::path("/proc") / std::to_string(pid) / "cmdline";

    int32_t ret = ReadFirstLine(cmdl_path, cmdline);

    if (ret <= 0) {
        return;
    }

    const char* buf = cmdline.data();
    size_t sz = cmdline.size();
    sanitize_nulls(buf, len);
    FetchUsecaseDetails(pid, buf, sz, sigId, sigType);
    if(sigId != 0) {
        cbdata->mSigId = sigId;
    }
    if(sigType != 0) {
        cbdata->mSigSubtype = sigType;
    }
    return 0;
}

__attribute__((constructor))
void registerWithUrm() {
    URM_REGISTER_WORKLOAD_POSTPROCESS_CB("gst-launch-", WorkloadPostprocessCallback)
}

/*
 * Compilation Notes:
 * To build the above code, it needs to be linked with UrmExtAPIs lib exposed by Resource Tuner,
 * and built as a shared lib:
 * => Create the sharesd lib:
 *    "g++ -fPIC -shared -o libplugin.so Plugin.cpp -lExtAPIs"
 *    This creates a shared lib, libplugin.so
 * => Copy this lib to "/etc/urm/custom", the location where Resource Tuner expects
 *    the custom Extensions lib to be placed.
 * => Make sure the lib file has appropriate permissions:
 *    "sudo chmod o+r /etc/urm/custom/libplugin.so"
 */
