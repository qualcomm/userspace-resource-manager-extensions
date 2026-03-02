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

void writeLineToFile(const std::string& fileName, const std::string& value);
void readLineFromFile(const std::string& fileName, std::string& line);
void fetchMachineName(std::string& machineName);

#endif
