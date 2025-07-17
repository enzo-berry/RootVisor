#include "Common.h"

#include <ntddk.h>
#include <wdf.h>

#include "VmxRoutines.h"

/* Set Bits for a special address (used on MSR Bitmaps) */
void
SetBit(PVOID Addr, UINT64 bit, BOOLEAN Set)
{

    UINT64 byte;
    UINT64 n;
    BYTE* Addr2;

    byte = bit / 8;
    n    = bit % 8;

    Addr2 = Addr;

    if ( Set )
    {
        Addr2[byte] |= (1 << n);
    }
    else
    {
        Addr2[byte] &= ~(1 << n);
    }
}

/* Get Bits of a special address (used on MSR Bitmaps) */
BYTE
GetBit(PVOID Addr, UINT64 bit)
{

    UINT64 byte, k;
    BYTE* Addr2;

    byte = 0;
    k    = 0;
    byte = bit / 8;
    k    = 7 - bit % 8;

    Addr2 = Addr;

    return Addr2[byte] & (1 << k);
}

/* Converts Virtual Address to Physical Address */
UINT64
VirtualAddressToPhysicalAddress(PVOID VirtualAddress)
{
    return MmGetPhysicalAddress(VirtualAddress).QuadPart;
}

/* Converts Physical Address to Virtual Address */
UINT64
PhysicalAddressToVirtualAddress(UINT64 PhysicalAddress)
{
    PHYSICAL_ADDRESS PhysicalAddr;
    PhysicalAddr.QuadPart = PhysicalAddress;

    return MmGetVirtualForPhysical(PhysicalAddr);
}
