// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <sstream>
#include <fstream>
#include <algorithm>
#include <unistd.h>
#include <cerrno>
#include <cctype>

#include "Helpers.h"

// Lowercase utility
void toLower(std::string &s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
}

std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

// Check writability using access(2)
bool isWritable(const std::string& path) {
    if (path.empty()) return false;
    return ::access(path.c_str(), W_OK) == 0;
}


int writeLineToFile(const std::string& fileName, const std::string& value) {
    if (fileName.empty()) return EINVAL;

    std::ofstream fileStream(fileName, std::ios::out | std::ios::trunc);
    if (!fileStream.is_open()) {
        return EIO;
    }

    fileStream << value;

    if (!fileStream.good()) {
        fileStream.close();
        return EIO;
    }

    fileStream.flush();
    fileStream.close();
    return 0;
}

bool readLineFromFile(const std::string& fileName, std::string& line) {
    if (fileName.empty()) return false;

    std::ifstream fileStream(fileName, std::ios::in);
    std::string value;

    if(!fileStream.is_open()) {
        return false;
    }

    if(!getline(fileStream, value)) {
        fileStream.close();
        return false;
    }

    fileStream.close();
    line = value;
    return true;
}

void fetchMachineName(std::string& machineName) {
    std::string machineNamePath = "/sys/devices/soc0/machine";
    std::string v;
    if (!readLineFromFile(machineNamePath, v)) {
        machineName.clear();
        return;
    }
    v = trim(v);
    toLower(v);
    machineName = v;
}
