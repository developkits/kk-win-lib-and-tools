
#include "stdafx.h"

#include "../../../kk-win-lib/kk-win-lib-namedpipe.h"
#include "../../../kk-win-lib/checker/kk-win-lib-checker-memory-operation-mismatch.h"
#include "../../../kk-win-lib/checker/kk-win-lib-checker-memory-operation-mismatch-server.h"

int _tmain(int argc, _TCHAR* argv[])
{
    kk::checker::MemoryOperationMismatchServer  memoryOperationMismatch;

    memoryOperationMismatch.init( true );
    memoryOperationMismatch.serverStart();
    memoryOperationMismatch.serverWaitFinish();

    memoryOperationMismatch.term();


	return 0;
}

