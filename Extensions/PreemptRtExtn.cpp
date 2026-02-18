// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <cctype>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <strings.h>
#include <cstdio>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/utsname.h>

#include <Urm/Extensions.h>
#include <Urm/UrmPlatformAL.h>

#include <fcntl.h>
#include <sys/time.h>
#include <syslog.h>

#define POLICY_DIR_PATH "/sys/devices/system/cpu/cpufreq/"
#define IRQ_DIR_PATH    "/proc/irq/"
#define WQ_DIR_PATH     "/sys/devices/virtual/workqueue/"

// ---------------------------
// Conditional logging (URM_EXT_LOG_RT)
// ---------------------------
static bool gLogInit = false;
static bool gLogEnabled = false;

static bool parseBoolEnv(const char* v) {
    if (!v) return false;
    return (!strcasecmp(v, "1") || !strcasecmp(v, "true") ||
            !strcasecmp(v, "on") || !strcasecmp(v, "yes") ||
            !strcasecmp(v, "y"));
}

static inline bool isLogEnabled() {
    if (!gLogInit) {
        gLogEnabled = parseBoolEnv(std::getenv("URM_EXT_RT"));
        gLogInit = true;
    }
    return gLogEnabled;
}

static void logLine(const std::string& msg) {
    if (!isLogEnabled()) return;
    static bool opened = false;
    if (!opened) {
        openlog("URM-EXT-RT", LOG_PID | LOG_CONS, LOG_DAEMON);
        opened = true;
    }

    syslog(LOG_INFO, "%s", msg.c_str());
}

// ---------------------------
// Basic I/O helpers
// ---------------------------
static int writeAttr(const std::string& path, const std::string& value) {
    int fd = ::open(path.c_str(), O_WRONLY | O_TRUNC);
    if (fd < 0) {
        if (isLogEnabled()) {
            logLine("open fail: " + path + " errno=" + std::to_string(errno) + " (" + std::string(strerror(errno)) + ")");
        }
        return errno;
    }
    std::string buf = value;
    if (buf.empty() || buf.back() != '\n') buf.push_back('\n');
    ssize_t n = ::write(fd, buf.c_str(), buf.size());
    if (n < 0) {
        if (isLogEnabled()) {
            logLine("write fail: " + path + " errno=" + std::to_string(errno) + " (" + std::string(strerror(errno)) + ")");
        }
        ::close(fd);
        return errno;
    }
    ::close(fd);
    if (isLogEnabled()) {
        logLine("wrote: " + path + " <= \"" + value + "\"");
    }
    return 0;
}

static bool readAttr(const std::string& path, std::string* out) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) return false;
    std::stringstream ss; ss << ifs.rdbuf();
    *out = ss.str();
    while (!out->empty() && (out->back() == '\n' || out->back() == '\r')) out->pop_back();
    return true;
}

static bool isWritable(const std::string& path) {
    return access(path.c_str(), W_OK) == 0;
}

// ---------------------------
// Small string helpers
// ---------------------------
static std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

static std::string toLower(std::string s) {
    for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// ---------------------------
// CPU list parsing & mask building
// ---------------------------
static std::vector<int> parseCpuList(const std::string& cpuListCsv) {
    std::vector<int> cpus;
    std::string token;
    std::stringstream ss(cpuListCsv);
    while (std::getline(ss, token, ',')) {
        token = trim(token);
        if (token.empty()) continue;
        auto dash = token.find('-');
        if (dash != std::string::npos) {
            std::string a = trim(token.substr(0, dash));
            std::string b = trim(token.substr(dash + 1));
            char* endp1 = nullptr; char* endp2 = nullptr;
            long lo = std::strtol(a.c_str(), &endp1, 10);
            long hi = std::strtol(b.c_str(), &endp2, 10);
            if (*endp1 == '\0' && *endp2 == '\0' && lo >= 0 && hi >= lo) {
                for (long v = lo; v <= hi; ++v) cpus.push_back(static_cast<int>(v));
            }
        } else {
            char* endp = nullptr;
            long v = std::strtol(token.c_str(), &endp, 10);
            if (*endp == '\0' && v >= 0) {
                cpus.push_back(static_cast<int>(v));
            }
        }
    }
    return cpus;
}

static std::string cpuListToHexMask64(const std::vector<int>& cpus) {
    uint64_t mask = 0;
    for (int cpu : cpus) {
        if (cpu >= 0 && cpu < 64) {
            mask |= (1ULL << static_cast<unsigned>(cpu));
        }
    }
    std::ostringstream oss;
    oss << std::hex << std::nouppercase;
    if (mask == 0) {
        oss << "0";
    } else {
        oss << mask;
    }
    return oss.str();
}

static std::string csvToHexMask(const std::string& csv) {
    return cpuListToHexMask64(parseCpuList(csv));
}

// ---------------------------
// Hostname / Machine (exact, case-sensitive)
// ---------------------------
static std::string readFileIfExists(const std::string& path) {
    std::string s;
    if (readAttr(path, &s)) return s;
    return {};
}

static std::string getHostname() {
    char buf[256] = {0};
    if (gethostname(buf, sizeof(buf) - 1) == 0 && buf[0] != '\0') {
        return std::string(buf);
    }
    std::string h = readFileIfExists("/etc/hostname");
    if (!h.empty()) return h;
    struct utsname u{};
    if (uname(&u) == 0) return std::string(u.nodename);
    return {};
}

// ---------------------------
//
// Key = exact hostname / machine;
// Value = { irqMaskSrc, irqIsHex, wqMaskSrc, wqIsHex }
//
// ---------------------------
struct MaskEntry {
    const char* irqSrc;  // CSV (e.g. "0-6") or HEX (e.g. "f7")
    bool        irqIsHex;
    const char* wqSrc;   // CSV or HEX
    bool        wqIsHex;
};

// Hostname-based policy
static const std::map<std::string, MaskEntry> kHostPolicyMap = {
    // Hostnames that use IRQ: 0,1,2,4,5,6,7 and WQ: f7
    { "iq-8275-evk",      { "0,1,2,4,5,6,7", false, "f7", true } },
    { "qcs8300-ride-sx",  { "0,1,2,4,5,6,7", false, "f7", true } },

    // Hostnames that use IRQ: 0-6 and WQ: 7f
    { "iq-9075-qvk",      { "0-6", false, "7f", true } },
    { "qcs9100-ride-sx",  { "0-6", false, "7f", true } },
    { "qcm6490-idp",      { "0-6", false, "7f", true } },
    { "rb3gen2-core-kit", { "0-6", false, "7f", true } },
};

// Machine-based policy
static const std::map<std::string, MaskEntry> kMachinePolicyMap = {
    { "QCS8275", { "0,1,2,4,5,6,7", false, "f7", true } },
    { "QCS8300", { "0,1,2,4,5,6,7", false, "f7", true } },

    { "QCS9075", { "0-6", false, "7f", true } },
    { "QCM6490", { "0-6", false, "7f", true } },
    { "QCS9100", { "0-6", false, "7f", true } },
    { "QCS6490", { "0-6", false, "7f", true } },
};

// Default masks when no map entry matches
static const MaskEntry kDefaultMask = { "0-6", false, "7f", true };

// ---------------------------
// Resolve final masks (hex strings) using the maps
// ---------------------------
struct ResolvedMasks {
    std::string irqHex;
    std::string wqHex;
    std::string matchedKey;
    std::string matchedScope;
};

static std::string strip0xLower(std::string s) {
    s = trim(s);
    if (s.size() >= 2 && s[0]=='0' && (s[1]=='x' || s[1]=='X')) s = s.substr(2);
    return toLower(s);
}

static ResolvedMasks resolveMasks() {
    const std::string host    = trim(getHostname());
    const std::string machine = trim(readFileIfExists("/sys/devices/soc0/machine"));

    // 1) Hostname exact match
    {
        auto it = kHostPolicyMap.find(host);
        if (it != kHostPolicyMap.end()) {
            const MaskEntry& e = it->second;
            ResolvedMasks r;
            r.irqHex = e.irqIsHex ? strip0xLower(e.irqSrc) : csvToHexMask(e.irqSrc);
            r.wqHex  = e.wqIsHex  ? strip0xLower(e.wqSrc ) : csvToHexMask(e.wqSrc );
            r.matchedKey = host;
            r.matchedScope = "host";
            if (isLogEnabled()) {
                logLine("Resolved by host: '" + host + "' -> irq=" + r.irqHex + ", wq=" + r.wqHex);
            }
            return r;
        }
    }

    // 2) Machine exact match
    {
        auto it = kMachinePolicyMap.find(machine);
        if (it != kMachinePolicyMap.end()) {
            const MaskEntry& e = it->second;
            ResolvedMasks r;
            r.irqHex = e.irqIsHex ? strip0xLower(e.irqSrc) : csvToHexMask(e.irqSrc);
            r.wqHex  = e.wqIsHex  ? strip0xLower(e.wqSrc ) : csvToHexMask(e.wqSrc );
            r.matchedKey = machine;
            r.matchedScope = "machine";
            if (isLogEnabled()) {
                logLine("Resolved by machine: '" + machine + "' -> irq=" + r.irqHex + ", wq=" + r.wqHex);
            }
            return r;
        }
    }

    // 3) Default
    ResolvedMasks r;
    r.irqHex = kDefaultMask.irqIsHex ? strip0xLower(kDefaultMask.irqSrc) : csvToHexMask(kDefaultMask.irqSrc);
    r.wqHex  = kDefaultMask.wqIsHex  ? strip0xLower(kDefaultMask.wqSrc ) : csvToHexMask(kDefaultMask.wqSrc );
    r.matchedKey = "<default>";
    r.matchedScope = "default";
    if (isLogEnabled()) {
        logLine("Resolved by default -> irq=" + r.irqHex + ", wq=" + r.wqHex +
                " (host='" + host + "', machine='" + machine + "')");
    }
    return r;
}

// ---------------------------
// PREEMPT_RT detection for cyclictest
// ---------------------------
static bool isPreemptRtActive() {
    std::string rt;
    if (readAttr("/sys/kernel/realtime", &rt)) {
        rt = trim(rt);
        if (isLogEnabled()) logLine(std::string("/sys/kernel/realtime = '") + rt + "'");
        if (rt == "1") return true;
        if (rt == "0") return false;
    }
    struct utsname u{};
    if (uname(&u) == 0) {
        std::string ver = toLower(u.version);
        if (isLogEnabled()) logLine(std::string("uname -v: ") + u.version);
        if (ver.find("preempt rt") != std::string::npos || ver.find("preempt_rt") != std::string::npos) {
            return true;
        }
    }
    return false;
}

// ---------------------------
// cpufreq: apply/tear
// ---------------------------
static bool gCpufreqApplied = false;
static std::vector<std::pair<std::string, std::string>> gCpufreqGovBackup;

static void cpufreqApplierCallback(void* /*context*/) {
    if (isLogEnabled()) logLine("enter cpufreqApplierCallback");

    if (gCpufreqApplied) return;

    gCpufreqGovBackup.clear();

    DIR* dir = opendir(POLICY_DIR_PATH);
    if (!dir) {
        if (isLogEnabled()) logLine(std::string("opendir fail: ") + POLICY_DIR_PATH + " errno=" + std::to_string(errno));
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strncmp(entry->d_name, "policy", 6) != 0) continue;

        std::string base = std::string(POLICY_DIR_PATH) + entry->d_name;
        std::string govFile = base + "/scaling_governor";
        if (!isWritable(govFile)) continue;

        std::string oldVal;
        if (readAttr(govFile, &oldVal)) {
            gCpufreqGovBackup.emplace_back(govFile, oldVal);
            if (isLogEnabled()) logLine("[" + std::string(entry->d_name) + "] old governor: " + oldVal);
            int rc = writeAttr(govFile, "performance");
            if (rc == 0 && isLogEnabled()) {
                std::string now;
                if (readAttr(govFile, &now)) logLine("verify " + govFile + " -> " + now);
            }
        }
    }
    closedir(dir);
    gCpufreqApplied = !gCpufreqGovBackup.empty();
}

static void cpufreqTearCallback(void* /*context*/) {
    if (!gCpufreqApplied) return;
    if (isLogEnabled()) logLine("enter cpufreqTearCallback");

    for (const auto& kv : gCpufreqGovBackup) {
        const std::string& path = kv.first;
        const std::string& oldVal = kv.second;
        if (isWritable(path)) writeAttr(path, oldVal);
    }
    gCpufreqGovBackup.clear();
    gCpufreqApplied = false;
}

// ---------------------------
// IRQ affinity: apply/tear
// ---------------------------
static bool gIrqApplied = false;
static std::vector<std::pair<std::string, std::string>> gIrqAffBackup;

static void irqAffinityApplierCallback(void* /*context*/) {
    if (isLogEnabled()) logLine("enter irqAffinityApplierCallback");

    if (gIrqApplied) return;

    gIrqAffBackup.clear();

    const ResolvedMasks m = resolveMasks();
    const std::string hexMask = m.irqHex.empty() ? std::string("7f") : m.irqHex;

    DIR* dir = opendir(IRQ_DIR_PATH);
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // numeric directories only
        bool numeric = true;
        for (const char* p = entry->d_name; *p; ++p) {
            if (!std::isdigit(static_cast<unsigned char>(*p))) { numeric = false; break; }
        }
        if (!numeric) continue;

        std::string smpFile = std::string(IRQ_DIR_PATH) + entry->d_name + "/smp_affinity";
        if (!isWritable(smpFile)) continue;

        std::string oldVal;
        if (readAttr(smpFile, &oldVal)) {
            gIrqAffBackup.emplace_back(smpFile, oldVal);
            writeAttr(smpFile, hexMask);
        }
    }
    closedir(dir);
    gIrqApplied = !gIrqAffBackup.empty();
}

static void irqAffinityTearCallback(void* /*context*/) {
    if (!gIrqApplied) return;
    if (isLogEnabled()) logLine("enter irqAffinityTearCallback");

    for (const auto& kv : gIrqAffBackup) {
        const std::string& path = kv.first;
        const std::string& oldVal = kv.second;
        if (isWritable(path)) writeAttr(path, oldVal);
    }
    gIrqAffBackup.clear();
    gIrqApplied = false;
}

// ---------------------------
// Workqueue cpumask: apply/tear
// ---------------------------
static bool gWqApplied = false;
static std::vector<std::pair<std::string, std::string>> gWqMaskBackup;

static void workqueueApplierCallback(void* /*context*/) {
    if (isLogEnabled()) logLine("enter workqueueApplierCallback");

    if (gWqApplied) return;

    gWqMaskBackup.clear();

    const ResolvedMasks m = resolveMasks();
    const std::string hexMask = m.wqHex.empty() ? std::string("7f") : m.wqHex;

    DIR* dir = opendir(WQ_DIR_PATH);
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') continue; // skip . and ..
        std::string cpumaskFile = std::string(WQ_DIR_PATH) + entry->d_name + "/cpumask";
        if (!isWritable(cpumaskFile)) continue;

        std::string oldVal;
        if (readAttr(cpumaskFile, &oldVal)) {
            gWqMaskBackup.emplace_back(cpumaskFile, oldVal);
            writeAttr(cpumaskFile, hexMask);
        }
    }
    closedir(dir);
    gWqApplied = !gWqMaskBackup.empty();
}

static void workqueueTearCallback(void* /*context*/) {
    if (!gWqApplied) return;
    if (isLogEnabled()) logLine("enter workqueueTearCallback");

    for (const auto& kv : gWqMaskBackup) {
        const std::string& path = kv.first;
        const std::string& oldVal = kv.second;
        if (isWritable(path)) writeAttr(path, oldVal);
    }
    gWqMaskBackup.clear();
    gWqApplied = false;
}

// ---------------------------
// Post-process callback (only when PREEMPT_RT is active)
// ---------------------------
static void postProcessCallback(void* context) {
    if (isLogEnabled()) logLine("enter postProcessCallback");
    if (context == nullptr) return;

    PostProcessCBData* cbData = static_cast<PostProcessCBData*>(context);
    if (cbData == nullptr) return;

    // Match to our usecase
    cbData->mSigId   = CONSTRUCT_SIG_CODE(0x80, 0x0001);
    cbData->mSigType = DEFAULT_SIGNAL_TYPE;
}

__attribute__((constructor))
static void registerCyclictestIfRt() {
    if (isPreemptRtActive()) {
        if (isLogEnabled()) logLine("PREEMPT_RT active: registering post-process 'cyclictest'");
        URM_REGISTER_POST_PROCESS_CB("cyclictest", postProcessCallback)
    } else {
        if (isLogEnabled()) logLine("PREEMPT_RT not active: skipping post-process 'cyclictest'");
    }
}

// ---------------------------
// URM registrations
// ---------------------------

// IDs:
//   0x00800001 -> cpufreq
//   0x00800002 -> irqaffinity
//   0x00800003 -> workqueue

URM_REGISTER_RES_APPLIER_CB(0x00800001, cpufreqApplierCallback)
URM_REGISTER_RES_TEAR_CB   (0x00800001, cpufreqTearCallback)

URM_REGISTER_RES_APPLIER_CB(0x00800002, irqAffinityApplierCallback)
URM_REGISTER_RES_TEAR_CB   (0x00800002, irqAffinityTearCallback)

URM_REGISTER_RES_APPLIER_CB(0x00800003, workqueueApplierCallback)
URM_REGISTER_RES_TEAR_CB   (0x00800003, workqueueTearCallback)
