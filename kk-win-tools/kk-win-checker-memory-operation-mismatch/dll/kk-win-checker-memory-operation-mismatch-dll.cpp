
#include "stdafx.h"
#include "kk-win-checker-memory-operation-mismatch-dll.h"

#include "../../../kk-win-lib/kk-win-lib-namedpipe.h"
#include "../../../kk-win-lib/checker/kk-win-lib-checker-memory-operation-mismatch.h"
#include "../../../kk-win-lib/checker/kk-win-lib-checker-memory-operation-mismatch-client.h"

#include <stdlib.h>

static
kk::checker::MemoryOperationMismatchClient      sMemoryOperationMismatch;


extern "C"
{

bool
initMemoryOperationMismatch( const size_t processId )
{
    bool result = false;

    {
        const bool bRet = sMemoryOperationMismatch.init( true );
        if ( !bRet )
        {
        }
        result = bRet;
    }

    {
        const bool bRet = sMemoryOperationMismatch.sendProcessId( (const DWORD)processId );
        if ( !bRet )
        {
        }
        result = bRet;
    }

    return result;
}

bool
termMemoryOperationMismatch(void)
{
    bool result = false;

    {
        const bool bRet = sMemoryOperationMismatch.term();
        result = bRet;
    }

    return result;
}

} // extern "C"

