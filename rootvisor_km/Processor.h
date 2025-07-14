#ifndef _PROCESSOR_H_
#define _PROCESSOR_H_

#include <devioctl.h>
#include <ntddk.h>

#define SIOCTL_TYPE 40000
#define IOCTL_CUSTOM CTL_CODE(SIOCTL_TYPE, 0x900, METHOD_IN_DIRECT, FILE_ANY_ACCESS)

SIZE_T
PowerOfTwo(SIZE_T Exponent);

BOOLEAN
IsVmxSupported();

void
RunOnEachLogicalProcessor(void* (*FunctionPtr)());

#endif // _PROCESSOR_H_