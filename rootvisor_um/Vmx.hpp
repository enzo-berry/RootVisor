#pragma once

#include <string>

extern "C"
{
bool inline DetectVmxSupportAsm(void);
void inline GetCpuVendorAsm(char* buffer);
}

std::string
GetCpuVendor();

bool
DetectVmxSupport();