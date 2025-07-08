#ifndef UTILS_H
#define UTILS_H

#include <devioctl.h>

#define SIOCTL_TYPE 40000

#define IOCTL_CUSTOM CTL_CODE(SIOCTL_TYPE, 0x900, METHOD_IN_DIRECT, FILE_ANY_ACCESS)

#endif // UTILS_H