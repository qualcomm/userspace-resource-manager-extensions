// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef URM_EXT_HELPERS_H
#define URM_EXT_HELPERS_H

#include <string>

#include <Urm/Extensions.h>
#include <Urm/UrmPlatformAL.h>
#include <Urm/Logger.h>
#include <Urm/Resource.h>
#include <Urm/ResourceRegistry.h>
#include <Urm/TargetRegistry.h>

std::string trim(const std::string& s);
void toLower(std::string& s);
bool isWritable(const std::string& path);
int writeLineToFile(const std::string& fileName, const std::string& value);
bool readLineFromFile(const std::string& fileName, std::string& line);
void fetchMachineName(std::string& machineName);
std::string cpuMaskToHex(uint64_t mask);

#endif
