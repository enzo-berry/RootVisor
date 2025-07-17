#include "Vmx.hpp"

std::string
GetCpuVendor()
{
    char cpuVendor[13];
    GetCpuVendorAsm(cpuVendor);
    std::string cpuID(cpuVendor);
    return cpuID;
}


bool
DetectVmxSupport()
{
    return DetectVmxSupportAsm();
}