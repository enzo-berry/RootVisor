#include "Utils.h"

#include <ntddk.h>

#include "Msr.h"

SIZE_T
PowerOfTwo(SIZE_T Exponent)
{
    SIZE_T Result = 1;
    while ( Exponent > 0 )
    {
        Result <<= 1;
        Exponent--;
    }
    return Result;
}
