#include "utils.hpp"
#include "asm.hpp"

#include <stdio.h>
#include <Windows.h>

int main()
{
	print_header();

	std::string cpu_vendor = GetCpuVendor();

	if (cpu_vendor != "GenuineIntel")
	{
		printf("[*] Processor vendor needs to be GenuineIntel.\n");
		return 1;
	}
	else
	{
		printf("[*] Intel processor detected.\n");
	}

	if (!DetectVmxSupport())
	{
		printf("[*] Vmx is not supported.\n");
		return 1;
	}
	else
	{
		printf("[*] VMX is supported\n");

	}

	HANDLE handle = CreateFile(L"\\\\.\\rootvisor",
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ |
		FILE_SHARE_WRITE,
		NULL, /// lpSecurityAttirbutes
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL |
		FILE_FLAG_OVERLAPPED,
		NULL); /// lpTemplateFile


	if (handle == INVALID_HANDLE_VALUE)
	{
		printf("[*] Could not open handle to KernelMode device.\n");
		return 1;
	}

	printf("[*] Opened handle to KernelMode device.\n");

	BOOL Result = DeviceIoControl(handle,
		(DWORD)IOCTL_CUSTOM,
		nullptr,
		0,
		nullptr,
		0,
		nullptr,
		NULL);

	printf("[*] IOCTL res: %d\n", Result);

	CloseHandle(handle);

	return 0;
}