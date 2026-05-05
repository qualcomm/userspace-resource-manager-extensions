// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <string>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <memory>
#include <mutex>

#include <Urm/Logger.h>
#include <Urm/Extensions.h>
#include <Urm/UrmPlatformAL.h>

class PostProcessingBlock {
private:
    static std::once_flag mInitFlag;
    static std::unique_ptr<PostProcessingBlock> mInstance;

private:
    inline void    SanitizeNulls(char *buf, int32_t len);
    inline int32_t ReadFirstLine(const std::string& filePath, std::string &line);
    inline void    to_lower(std::string &s);
    int32_t        CountThreadsWithName(pid_t pid, const std::string& commSub);
    int32_t        FetchUsecaseDetails(int32_t pid, char *buf, uint32_t &sigId, uint32_t &sigType);
    int32_t        countEncoders(const char* buffer, const char* encoderStr);
    std::string    extractSourceName(const char* buffer, const char* namePrefix, const char* defaultName);

    uint32_t       calculateEncoderSigType(int32_t count);
    uint32_t       calculateDecoderSigType(int32_t threadCount);

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

// Lowercase utility
inline void PostProcessingBlock::to_lower(std::string &s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return;
}

// Count threads under /proc/<pid>/task whose names contain `substring`.
int32_t PostProcessingBlock::CountThreadsWithName(pid_t pid, const std::string& commSub) {
    std::string commSubStr = std::string(commSub);
    const std::string threadsListPath = "/proc/" + std::to_string(pid) + "task/";

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

int32_t PostProcessingBlock::countEncoders(const char* buffer, const char* encoderStr) {
    if (buffer == nullptr || encoderStr == nullptr) {
        return 0;
    }

    int32_t count = 0;
    const char* current = buffer;
    const size_t encoderStrLen = strlen(encoderStr);

    while ((current = strstr(current, encoderStr)) != nullptr) {
        count++;
        current += encoderStrLen;
    }

    return count;
}

std::string PostProcessingBlock::extractSourceName(const char* buffer,
                                                   const char* namePrefix,
                                                   const char* defaultName) {
    if (buffer == nullptr || namePrefix == nullptr) {
        return std::string(defaultName);
    }

    const char* namePtr = strstr(buffer, namePrefix);
    if (namePtr != nullptr) {
        namePtr += strlen(namePrefix);

        // Find the end of the name
        const char* nameEnd = namePtr;
        while (*nameEnd && *nameEnd != ' ' && *nameEnd != '\t' &&
               *nameEnd != '\n' && *nameEnd != '\r' && *nameEnd != '!') {
            nameEnd++;
        }

        // Return extracted name as string
        if (nameEnd > namePtr) {
            return std::string(namePtr, nameEnd - namePtr);
        }
    }

    return std::string(defaultName);
}

/**
 * @brief Calculate sigType value for encoder based on count thresholds
 *
 * @return sigType value based on encoder load classification:
 *         0 for normal load (≤12 encoders/threads)
 *         13 for high load (>12 encoders/threads)
 */
uint32_t PostProcessingBlock::calculateEncoderSigType(int32_t count) {
    return (count <= 12) ? 0 : 13;
}

/**
 * @brief Calculate sigType value for decoder based on thread count thresholds
 *
 * @return sigType value based on decoder load classification:
 *         0 for low load (0-4 threads)
 *         5 for medium load (5-20 threads)
 *         21 for high load (20+ threads)
 */
uint32_t PostProcessingBlock::calculateDecoderSigType(int32_t threadCount) {
    if (threadCount < 5){
        return 0;
    }
    else if (threadCount >= 5 && threadCount <= 20) {
        return 5;
    }
    else {
        return 21;
    }
}

int32_t PostProcessingBlock::FetchUsecaseDetails(int32_t pid,
                                                 char *buf,
                                                 uint32_t& sigId,
                                                 uint32_t& sigType) {
    /**
     * For encoder, width of encoding, v4l2h264enc in line
     * For decoder, v4l2h264dec, or may be 265 as well, decoder bit
     */

    // GStreamer element identifiers for different video operations
    const char* encoderStr  = "v4l2h264enc";    // Hardware H.264 encoder element
    const char* decoderStr  = "v4l2h264dec";    // Hardware H.264 decoder element
    const char* qmmSrcStr   = "qtiqmmfsrc";     // Qualcomm multimedia source element
    const char* namePrefix  = "name=";          // GStreamer element name attribute
    const char* defaultName = "camsrc";         // Default camera source name

    int32_t result = -1;  // Return -1 if no use case detected

    // Check for encoder
    if (strstr(buf, encoderStr) != nullptr) {
        int32_t encoderCount   = countEncoders(buf, encoderStr);
        std::string sourceName = extractSourceName(buf, namePrefix, defaultName);
        int32_t numSrc         = CountThreadsWithName(pid, sourceName);

        // Encode Multi stream case
        if (encoderCount > 1) {
            sigId   = URM_SIG_CAMERA_ENCODE_MULTI_STREAMS;
            sigType = calculateEncoderSigType(encoderCount);;
            LOGE("FetchUsecaseDetails - varun", std::string("ENCODE_MULTI_STREAM"));
        }
        // Encode single stream case
        else {
            sigId   = URM_SIG_CAMERA_ENCODE;
            sigType = calculateEncoderSigType(numSrc);
            LOGE("FetchUsecaseDetails - varun", std::string("ENCODE"));
        }
        result = 0;
    }
    // Check for decoder
    else if (strstr(buf, decoderStr) != nullptr) {
        int32_t numSources = CountThreadsWithName(pid, decoderStr);
        sigId   = URM_SIG_VIDEO_DECODE;
        sigType = calculateDecoderSigType(numSources);
        result  = 0;
        LOGE("FetchUsecaseDetails - varun", std::string("DECODE"));
    }
    // Check for preview
    else if (strstr(buf, qmmSrcStr) != nullptr) {
        sigId = URM_SIG_CAMERA_PREVIEW;
        sigType = 0;
        result = 0;
    }

    return result;
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
    FetchUsecaseDetails(pid, buf, sigId, sigType);
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
static void registerWithUrm() {
    URM_REGISTER_POST_PROCESS_CB("gst-launch-1.0", WorkloadPostprocessCallback)
    URM_REGISTER_POST_PROCESS_CB("gst-camera-per-port-example", WorkloadPostprocessCallback)
}
