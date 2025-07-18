#pragma once

#include <ntddk.h>

#include "VmxRoutines.h"

//////////////////////////////////////////////////
//				Global Variables				//
//////////////////////////////////////////////////

// Save the state and variables related to each to logical core
VIRTUAL_MACHINE_STATE* GuestState;

// Save the state and variables related to EPT
// Global EPT state for all logical cores.
EPT_STATE* EptState;

// Used for managing CR3_TARGET_VALUEx values
UINT64 TargerCr3Count;

// Because we may be executing in an arbitrary user-mode
// process as part of the DPC interrupt we execute in
// We have to save Cr3, for HOST_CR3
UINT64 InitiateCr3;
