#include "Asm.hpp"

std::string GetCpuVendor()
{
	char cpuVendor[13];
	_GetCpuVendor(cpuVendor);
	std::string cpuID(cpuVendor);
	return cpuID;
}