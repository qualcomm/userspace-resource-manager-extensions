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

#include "Helpers.h"

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
    // rely on daemon's logging setup; no openlog() here
    syslog(LOG_INFO, "%s", msg.c_str());
}


static inline void logWriteFailure(const std::string& path, int rc) {
    if (!isLogEnabled()) return;
    logLine("write failed for " + path + " rc=" + std::to_string(rc) +
            " err='" + std::string(strerror(rc)) + "'");
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
    if (readLineFromFile(path, s)) return s;
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
    { "iq-9075-evk",      { "0-6", false, "7f", true } },
    { "qcs9100-ride-sx",  { "0-6", false, "7f", true } },
    { "qcm6490-idp",      { "0-6", false, "7f", true } },
    { "rb3gen2-core-kit", { "0-6", false, "7f", true } },
};

// Machine-based policy
static const std::map<std::string, MaskEntry> kMachinePolicyMap = {
    { "qcs8275", { "0,1,2,4,5,6,7", false, "f7", true } },
    { "qcs8300", { "0,1,2,4,5,6,7", false, "f7", true } },

    { "qcs9075", { "0-6", false, "7f", true } },
    { "qcm6490", { "0-6", false, "7f", true } },
    { "qcs9100", { "0-6", false, "7f", true } },
    { "qcs6490", { "0-6", false, "7f", true } },
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
    toLower(s);
    return s;
}

static ResolvedMasks resolveMasks() {
    const std::string host = trim(getHostname());

    std::string machine;
    fetchMachineName(machine);

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
    if (readLineFromFile("/sys/kernel/realtime", rt)) {
        rt = trim(rt);
        if (isLogEnabled()) logLine(std::string("/sys/kernel/realtime = '") + rt + "'");
        if (rt == "1") return true;
        if (rt == "0") return false;
    }
    struct utsname u{};
    if (uname(&u) == 0) {
        std::string ver(u.version);
        toLower(ver);
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

static void cpufreqGovApplierCallback(void* /*context*/) {
    if (isLogEnabled()) logLine("enter cpufreqGovApplierCallback");

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
        if (readLineFromFile(govFile, oldVal)) {
            gCpufreqGovBackup.emplace_back(govFile, oldVal);
            if (isLogEnabled()) logLine("[" + std::string(entry->d_name) + "] old governor: " + oldVal);
            int rc = writeLineToFile(govFile, "performance");
            if (rc != 0) {
               logWriteFailure(govFile, rc);
            }
            
            if (rc == 0 && isLogEnabled()) {
                std::string now;
                if (readLineFromFile(govFile, now)) {
                    logLine("verify " + govFile + " -> " + now);
                }
            }
        }
    }
    closedir(dir);
    gCpufreqApplied = !gCpufreqGovBackup.empty();
}

static void cpufreqGovTearCallback(void* /*context*/) {
    if (!gCpufreqApplied) return;
    if (isLogEnabled()) logLine("enter cpufreqTearCallback");

    for (const auto& kv : gCpufreqGovBackup) {
        const std::string& path = kv.first;
        const std::string& oldVal = kv.second;
        if (isWritable(path)) {
            int rc = writeLineToFile(path, oldVal);
            if (rc != 0) logWriteFailure(path, rc);
        }
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

        if (readLineFromFile(smpFile, oldVal)) {
            gIrqAffBackup.emplace_back(smpFile, oldVal);

            int rc = writeLineToFile(smpFile, hexMask);
            if (rc != 0) {
                logWriteFailure(smpFile, rc);
            }
            if (rc == 0 && isLogEnabled()) {
                std::string now;
                if (readLineFromFile(smpFile, now)) {
                    logLine("verify " + smpFile + " -> " + now);
                }
            }
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
        if (isWritable(path)) {
            int rc = writeLineToFile(path, oldVal);
            if (rc != 0) logWriteFailure(path, rc);
        }
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
        if (readLineFromFile(cpumaskFile, oldVal)) {
            gWqMaskBackup.emplace_back(cpumaskFile, oldVal);

            int rc = writeLineToFile(cpumaskFile, hexMask);
            if (rc != 0) {
                logWriteFailure(cpumaskFile, rc);
            }
            if (rc == 0 && isLogEnabled()) {
                std::string now;
                if (readLineFromFile(cpumaskFile, now)) {
                    logLine("verify " + cpumaskFile + " -> " + now);
                }
            }
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
        
        if (isWritable(path)) {
            int rc = writeLineToFile(path, oldVal);
            if (rc != 0) logWriteFailure(path, rc);
        }
    }
    gWqMaskBackup.clear();
    gWqApplied = false;
}

// ---------------------------
// URM registrations
// ---------------------------

// IDs:
//   0x00800001 -> cpufreq
//   0x00800002 -> irqaffinity
//   0x00800003 -> workqueue

URM_REGISTER_RES_APPLIER_CB(0x00800001, cpufreqGovApplierCallback)
URM_REGISTER_RES_TEAR_CB   (0x00800001, cpufreqGovTearCallback)

URM_REGISTER_RES_APPLIER_CB(0x00800002, irqAffinityApplierCallback)
URM_REGISTER_RES_TEAR_CB   (0x00800002, irqAffinityTearCallback)

URM_REGISTER_RES_APPLIER_CB(0x00800003, workqueueApplierCallback)
URM_REGISTER_RES_TEAR_CB   (0x00800003, workqueueTearCallback)
