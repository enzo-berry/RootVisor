#pragma once

#include <string>

extern "C" {
	bool inline DetectVmxSupport(void);
	void inline _GetCpuVendor(char* buffer);
}

std::string
GetCpuVendor();