// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <cstring>
#include <string>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <algorithm>

#include <Urm/Extensions.h>

static void SanitizeNulls(char *buf, int32_t len) {
    /* /proc/<pid>/cmdline contains null charaters instead of spaces
     * sanitize those null characters with spaces such that char*
     * can be treaded till line end.
     */
    for(int32_t i = 0; i < len; i++) {
        if (buf[i] == '\0') {
            buf[i] = ' ';
        }
    }
}

static inline int32_t ReadFirstLine(const std::string& filePath, std::string &line) {
    if(filePath.length() == 0) return "";

    std::ifstream fileStream(filePath, std::ios::in);

    if(!fileStream.is_open()) {
        return 0;
    }

    if(!getline(fileStream, line)) {
        return 0;
    }

    fileStream.close();
    return value;
}

int8_t CheckProcessCommSubstring(int pid, const std::string& target) {
    std::string processName = "";
    const fs::path commPath = "/proc/" + std::to_string(pid) + "/comm";

    if(ReadFirstLine(comm_path, processName) <= 0) {
        return false;
    }

    // Check if target is a substring of processName
    return processName.find(target) != std::string::npos;
}

// Lowercase utility (safe for unsigned char)
static inline void to_lower(std::string &s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return;
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

static int32_t countThreadsWithName(pid_t pid, const char* commSub) {
    std::string commSubStr = std::string(commSub);
    const std::string threadsListPath = "/proc" + std::to_string(pid) + "task/";

    DIR* dir = nullptr;
    if((dir = opendir(threadsListPath.c_str())) == nullptr) {
        return 0;
    }

    int32_t count = 0;
    struct dirent* entry;
    while((entry = readdir(dir)) != nullptr) {
        std::string threadNamePath = threadsListPath + std::string(entry->d_name) + "/comm";

        std::ifstream fileStream(threadNamePath, std::ios::in);
        if(!fileStream.is_open()) {
            return 0;
        }

        std::string value = "";
        if(!getline(fileStream, value)) {
            return 0;
        }

        value = to_lower(value);
        commSubStr = to_lower(commSubStr);
        if(value.find(commSubStr) != std::string::npos) {
            count++;
        }
    }

    closedir(dir);
    return count;
}

static int32_t FetchUsecaseDetails(int32_t pid, char *buf, size_t sz, uint32_t &sigId, uint32_t &sigType) {
    /* For encoder, width of encoding, v4l2h264enc in line
     * For decoder, v4l2h264dec, or may be 265 as well, decoder bit
     */
    int32_t ret = -1, numSrc = 0;
    int32_t encode = 0, decode = 0, preview = 0;
    int32_t height = 0;
    std::string target = "gst-camera-per";
    const char *e_str = "v4l2h264enc";
    const char *d_str = "v4l2h264dec";
    const char *qmm_str = "qtiqmmfsrc";
    const char *n_str = "name=";
    const char *h_str = "height=";
    char *e = buf;
    int32_t sigCat = URM_SIG_CAT_MULTIMEDIA;

    if((e = strstr(e, e_str)) != NULL) {
        encode += 1;
        sigId = CONSTRUCT_SIG_CODE(sigCat, URM_SIG_CAMERA_ENCODE);
        const char *name = buf;
        if((name = strstr(name, n_str)) != NULL) {
            name += strlen(n_str);
        }

        if(name == NULL) {
            name = (char*)"camsrc";
        }
        numSrc = CountThreadsWithName(pid, name);
    }

    int8_t multi = CheckProcessCommSubstring(pid, target);

    if ((numSrc > 1) || (multi)) {
        sigId = CONSTRUCT_SIG_CODE(sigCat, URM_SIG_CAMERA_ENCODE_MULTI_STREAMS);
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
        sigId = CONSTRUCT_SIG_CODE(sigCat, URM_SIG_VIDEO_DECODE);
        numSrc = CountThreadsWithName(pid, d_str);
        sigType = numSrc;
    }

    /*Preview case*/
    if (encode == 0 && decode == 0) {
        char *d = buf;
        size_t d_str_sz = strlen(qmm_str);
        if ((d = strstr(d, qmm_str)) != NULL) {
            preview += 1;
            sigId = CONSTRUCT_SIG_CODE(sigCat, URM_SIG_CAMERA_PREVIEW);
            ret = 0;
        }
    }

    if(encode > 0 && decode > 0) {
        sigId = CONSTRUCT_SIG_CODE(sigCat, URM_SIG_ENCODE_DECODE);
        ret = 0;
    }
    return ret;
}

void WorkloadPostprocessCallback(void *cbData) {
    PostProcessCBData *cbdata = static_cast<PostProcessCBData *>(cbData);
    if(cbdata == NULL) {
        return;
    }

    pid_t pid = cbdata->mPid;
    uint32_t sigId = 0;
    uint32_t sigType = 0;

    std::string cmdline;
    std::string cmdLinePath = "/proc/" + std::to_string(pid) + "/cmdline";

    if(ReadFirstLine(cmdLinePath, cmdline) <= 0) {
        return;
    }

    char* buf = cmdline.data();
    size_t sz = cmdline.size();

    SanitizeNulls(buf, sz);
    FetchUsecaseDetails(pid, buf, sz, sigId, sigType);

    if(sigId != 0) {
        cbdata->mSigId = sigId;
    }

    if(sigType != 0) {
        cbdata->mSigSubtype = sigType;
    }
}

__attribute__((constructor))
void registerWithUrm() {
    CLASSIFIER_REGISTER_POST_PROCESS_CB("gst-launch-", WorkloadPostprocessCallback)
}
