#include <cstring>
#include <string>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <algorithm>

#include <Urm/Extensions.h>

enum USECASE {
    UNDETERMINED,
    URM_DECODE,
    URM_ENCODE_720,
    URM_ENCODE_1080,
    URM_ENCODE_2160,
    URM_ENCODE_MANY,
    URM_ENCODE_DECODE,
    URM_CAMERA_PREVIEW,
    URM_VIDEO_DECODE,
    URM_CAMERA_ENCODE,
    URM_CAMERA_ENCODE_MULTI_STREAMS,
};

typedef struct {
    pid_t mPid;
	uint32_t mSigId;
	uint32_t mSigSubtype;
} PostProcessCBData;

static const std::string readFromFile(const std::string& fileName) {
    if(fileName.length() == 0) return "";

    std::ifstream fileStream(fileName, std::ios::in);
    std::string value = "";

    if(!fileStream.is_open()) {
        return "";
    }

    if(!getline(fileStream, value)) {
        return "";
    }

    fileStream.close();
    return value;
}

static int8_t checkProcessCommSubstring(int32_t pid, const std::string& target) {
    std::string path = "/proc/" + std::to_string(pid) + "/comm";
    std::ifstream file(path);

    if(!file.is_open()) {
        throw std::runtime_error("Failed to open " + path + ". Process may not exist.");
    }

    std::string processName;
    std::getline(file, processName); // Read the process name
    file.close();

    // Check if target is a substring of processName
    return processName.find(target) != std::string::npos;
}

static std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

static int32_t countThreadsWithName(pid_t pid, const char* commSub) {
    std::string commSubStr = std::string(commSub);
    const std::string threadsListPath = "/proc" + std::to_string(pid) + "task/";

    DIR* dir = opendir(threadsListPath.c_str());
    if(dir == nullptr) {
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

static enum USECASE fetchUsecaseDetails(pid_t pid, char *buf, size_t sz, uint32_t &sigId, uint32_t &sigType) {
    enum USECASE useCase = UNDETERMINED;

    int32_t encode = 0, decode = 0;
    int32_t height = 0;
    int32_t numSrc = 0;

    std::string target = "gst-camera-per";
    const char *e_str = "v4l2h264enc";
    const char *d_str = "v4l2h264dec";
    const char *qmm_str = "qtiqmmfsrc";
    const char *n_str = "name=";
    const char *h_str = "height=";
    char *e = buf;

    if((e = strstr(e, e_str)) != NULL) {
        encode += 1;
        sigId = URM_CAMERA_ENCODE;
        char *name = buf;
        if((name = strstr(name, n_str)) != NULL) {
            name += strlen(n_str);
        }

        if(name == NULL) {
            name = (char*)"camsrc";
        }
        numSrc = countThreadsWithName(pid, name);
    }

    int8_t multi = checkProcessCommSubstring(pid, target);

    if((numSrc > 1) || (multi)) {
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
    if((d = strstr(d, d_str)) != NULL) {
        decode += 1;
        sigId = URM_VIDEO_DECODE;
        numSrc = countThreadsWithName(pid, d_str);
        sigType = numSrc;
    }

    /*Preview case*/
    if(encode == 0 && decode == 0) {
        char *d = buf;
        size_t d_str_sz = strlen(qmm_str);
        if((d = strstr(d, qmm_str)) != NULL) {
            sigId = URM_CAMERA_PREVIEW;
        }
    }

    if(encode > 0 && decode > 0) {
        sigId = URM_ENCODE_DECODE;
    }

    if(decode > 0) {
        useCase = URM_DECODE;
    }

    if(encode > 1) {
        useCase = URM_ENCODE_MANY;
    } else if (encode == 1) {
        if(height <= 720) {
            useCase = URM_ENCODE_720;
        } else if (height <= 1080) {
            useCase = URM_ENCODE_1080;
        } else {
            useCase = URM_ENCODE_2160;
        }
    }

    if(encode > 0 && decode > 0) {
        useCase = URM_ENCODE_DECODE;
    }

    return useCase;
}

static void sanitize_nulls(char *buf, int len) {
    /* /proc/<pid>/cmdline contains null charaters instead of spaces
     * sanitize those null characters with spaces such that char*
     * can be treaded till line end.
     */
    for(int i = 0; i < len; i++) {
        if(buf[i] == '\0') {
            buf[i] = ' ';
        }
    }
}

static void gstCamPostProcess(void* context) {
    if(context == nullptr) {
        return;
    }

    PostProcessCBData* postProcessInfo = static_cast<PostProcessCBData*>(context);
    std::string cmdLine = "";
    // Read cmdline
    cmdLine = readFromFile("/proc/" + std::to_string(postProcessInfo->mPid) + "/cmdline");

    uint32_t sigID = postProcessInfo->mSigId;
    uint32_t sigType = postProcessInfo->mSigSubtype;

    char cmdline[1024];
    snprintf(cmdline, 1024, "/proc/%d/cmdline", postProcessInfo->mPid);
    FILE *fp = fopen(cmdline, "r");
    if(fp) {
        char *buf = NULL;
        size_t sz = 0;
        int len = 0;
        while((len = getline(&buf, &sz, fp)) > 0) {
            sanitize_nulls(buf, len);
            enum USECASE type = fetchUsecaseDetails(postProcessInfo->mPid, buf, sz, sigID, sigType);
            if(type != UNDETERMINED) {
                // Update:
                postProcessInfo->mSigId = sigID;
                postProcessInfo->mSigSubtype = sigType;
                break;
            }
        }
    } else {
        printf("Failed to open file:%d\n", postProcessInfo->mPid);
    }
}

// Register post processing block for gst-camera
CLASSIFIER_REGISTER_POST_PROCESS_CB("gst-camera-per", gstCamPostProcess);

__attribute__((constructor))
void registerWithUrm() {
    // Useful if the user wants to maintain app config in a separate location
    // For now we expect it will be placed in /etc/urm/custom/ directly
    // RESTUNE_REGISTER_CONFIG(APP_CONFIG, "/etc/urm/tests/configs/ResourcesConfig.yaml")
}
