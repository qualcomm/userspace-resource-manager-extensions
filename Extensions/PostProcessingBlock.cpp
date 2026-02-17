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

class PostProcessingBlock {
private:
    static std::once_flag mInitFlag;
    static std::unique_ptr<PostProcessingBlock> mInstance;

private:
	inline void SanitizeNulls(char *buf, int32_t len);
	inline int32_t ReadFirstLine(const std::string& filePath, std::string &line);
	int8_t CheckProcessCommSubstring(int pid, const std::string& target);
	inline void to_lower(std::string &s);
	int32_t CountThreadsWithName(pid_t pid, const std::string& commSub);
	int32_t FetchUsecaseDetails(int32_t pid, char *buf, size_t sz,
                                uint32_t &sigId, uint32_t &sigType);

    PostProcessingBlock() = default;

    PostProcessingBlock(const PostProcessingBlock&) = delete;
    PostProcessingBlock& operator=(const PostProcessingBlock&) = delete;

public:
    static PostProcessingBlock& getInstance() {
        std::call_once(mInitFlag, [] {
            mInstance.reset(new PostProcessingBlock());
        });
        return *mInstance;
    }

    ~PostProcessingBlock() = default;
    void PostProcess(pid_t pid, uint32_t &sigId, uint32_t &sigType);
};

inline void PostProcessingBlock::SanitizeNulls(char *buf, int32_t len) {
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

inline int32_t PostProcessingBlock::ReadFirstLine(const std::string& filePath, std::string &line) {
    if(filePath.length() == 0) return 0;

    std::ifstream fileStream(filePath, std::ios::in);

    if(!fileStream.is_open()) {
        return 0;
    }

    if(!getline(fileStream, line)) {
        return 0;
    }

    fileStream.close();
    return line.size();
}

int8_t PostProcessingBlock::CheckProcessCommSubstring(int pid, const std::string& target) {
    std::string processName = "";
    std::string commPath = "/proc/" + std::to_string(pid) + "/comm";

    if(ReadFirstLine(commPath, processName) <= 0) {
        return false;
    }

    // Check if target is a substring of processName
    return processName.find(target) != std::string::npos;
}

// Lowercase utility
inline void PostProcessingBlock::to_lower(std::string &s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return;
}

// Count threads under /proc/<pid>/task whose names contain `substring`.
int32_t PostProcessingBlock::CountThreadsWithName(pid_t pid, const std::string& commSub) {
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
            closedir(dir);
            return 0;
        }

        std::string value = "";
        if(!getline(fileStream, value)) {
            closedir(dir);
            return 0;
        }

        to_lower(value);
        to_lower(commSubStr);
        if(value.find(commSubStr) != std::string::npos) {
            count++;
        }
    }

    closedir(dir);
    return count;
}

int32_t PostProcessingBlock::FetchUsecaseDetails(int32_t pid,
                                                 char *buf,
                                                 size_t sz,
                                                 uint32_t& sigId,
                                                 uint32_t& sigType) {
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

    if((e = strstr(e, e_str)) != nullptr) {
        encode += 1;
        sigId = URM_SIG_CAMERA_ENCODE;
        const char *name = buf;
        if((name = strstr(name, n_str)) != nullptr) {
            name += strlen(n_str);
        }

        if(name == nullptr) {
            name = (char*)"camsrc";
        }
        numSrc = CountThreadsWithName(pid, name);
    }

    int8_t multi = CheckProcessCommSubstring(pid, target);

    if ((numSrc > 1) || (multi)) {
        sigId = URM_SIG_CAMERA_ENCODE_MULTI_STREAMS;
        sigType = numSrc;
    }

    char *h = buf;
    size_t h_str_sz = strlen(h_str);
    h = strstr(h, h_str);
    if (h != nullptr) {
        height = strtol(h + h_str_sz, nullptr, 10);
    }

    char *d = buf;
    if ((d = strstr(d, d_str)) != nullptr) {
        decode += 1;
        sigId = URM_SIG_VIDEO_DECODE;
        numSrc = CountThreadsWithName(pid, d_str);
        sigType = numSrc;
    }

    /*Preview case*/
    if (encode == 0 && decode == 0) {
        char *d = buf;
        size_t d_str_sz = strlen(qmm_str);
        if ((d = strstr(d, qmm_str)) != nullptr) {
            preview += 1;
            sigId = URM_SIG_CAMERA_PREVIEW;
            ret = 0;
        }
    }

    if(encode > 0 && decode > 0) {
        sigId = URM_SIG_ENCODE_DECODE;
        ret = 0;
    }

    return ret;
}

void PostProcessingBlock::PostProcess(pid_t pid, uint32_t &sigId, uint32_t &sigType) {
	std::string cmdline;
    std::string cmdLinePath = "/proc/" + std::to_string(pid) + "/cmdline";

    if(ReadFirstLine(cmdLinePath, cmdline) <= 0) {
        return;
    }

    char* buf = (char*)cmdline.data();
    size_t sz = cmdline.size();

    SanitizeNulls(buf, sz);
    FetchUsecaseDetails(pid, buf, sz, sigId, sigType);
}

std::once_flag PostProcessingBlock::mInitFlag;
std::unique_ptr<PostProcessingBlock> PostProcessingBlock::mInstance = nullptr;

static void WorkloadPostprocessCallback(void* context) {
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

    PostProcessingBlock::getInstance().PostProcess(pid, sigId, sigType);

    cbData->mSigId = sigId;
    cbData->mSigType = sigType;
}

__attribute__((constructor))
void registerWithUrm() {
    URM_REGISTER_POST_PROCESS_CB("gst-launch-", WorkloadPostprocessCallback)
}
