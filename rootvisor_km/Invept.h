#pragma once
#include "Ept.h"
#include "Vmx.h"


//////////////////////////////////////////////////
//                   Structures		   			//
//////////////////////////////////////////////////

typedef struct _INVEPT_DESC
{
    EPTP EptPointer;
    UINT64 Reserveds;
} INVEPT_DESC, *PINVEPT_DESC;


//////////////////////////////////////////////////
//                    Enums		    			//
//////////////////////////////////////////////////

typedef enum _INVEPT_TYPE
{
    SINGLE_CONTEXT = 0x00000001,
    ALL_CONTEXTS   = 0x00000002
} INVEPT_TYPE;


//////////////////////////////////////////////////
//                 Functions	    			//
//////////////////////////////////////////////////

// Invept Functions
unsigned char
Invept(UINT32 Type, INVEPT_DESC* Descriptor);
unsigned char
InveptAllContexts();
unsigned char
InveptSingleContext(UINT64 EptPonter);