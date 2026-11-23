#pragma once

#include "core/Types.h"

#include <string>
#include <string_view>

namespace er {

class FeatureManager;

void installCrashDebug();
void debugNote(std::string_view msg);
void writeCrashReport(std::string_view title, std::string_view body);
void openTrainerLogFolder();
std::string featureDebugDump(const FeatureManager& features);
std::string hooksDebugDump();
std::string formatExitCode(u32 code);

}  // namespace er
