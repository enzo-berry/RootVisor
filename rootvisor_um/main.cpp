#include <Windows.h>
#include <stdio.h>

#include "Utils.hpp"
#include "Vmx.hpp"

int
main()
{
    print_header();

    std::string cpu_vendor = GetCpuVendor();

    if ( cpu_vendor != "GenuineIntel" )
    {
        printf("[*] Processor vendor needs to be GenuineIntel.\n");
        return 1;
    }
    else
    {
        printf("[*] Intel processor detected.\n");
    }

    if ( !DetectVmxSupport() )
    {
        printf("[*] Vmx is not supported.\n");
        return 1;
    }
    else
    {
        printf("[*] VMX is supported\n");
    }

    // This will launch the Hypervisor initialization routine. //
    HANDLE handle = CreateFile(
        L"\\\\.\\rootvisor",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
        NULL);
    /////////////////////////////////////////////////////////////

    if ( handle == INVALID_HANDLE_VALUE )
    {
        printf("[*] Could not open handle to KernelMode device.\n");
        return 1;
    }

    printf("[*] Opened handle to KernelMode device.\n");

    // Wait for user input before closing the handle
    printf("[*] Press Enter to close the handle and exit.\n");
    getchar();

    // This will launch the devirtualization termination routine. //
    CloseHandle(handle);

    return 0;
}