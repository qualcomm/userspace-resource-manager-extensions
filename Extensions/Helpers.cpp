// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <sstream>
#include <fstream>
#include <algorithm>

#include "Helpers.h"

// Lowercase utility
static inline void toLower(std::string &s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
}

void writeLineToFile(const std::string& fileName, const std::string& value) {
    if(fileName.length() == 0) return;

    std::ofstream fileStream(fileName, std::ios::out | std::ios::trunc);
    if(!fileStream.is_open()) {
        return;
    }

    fileStream<<value;

    if(fileStream.fail()) {}

    fileStream.flush();
    fileStream.close();
}

void readLineFromFile(const std::string& fileName, std::string& line) {
    if(fileName.length() == 0) {
        return;
    }

    std::ifstream fileStream(fileName, std::ios::in);
    std::string value = "";

    if(!fileStream.is_open()) {
        return;
    }

    if(!getline(fileStream, value)) {
        return;
    }

    fileStream.close();
    line = value;
}

void fetchMachineName(std::string& machineName) {
    std::string machineNamePath = "/sys/devices/soc0/machine";
    readLineFromFile(machineNamePath, machineName);
    toLower(machineName);
}
