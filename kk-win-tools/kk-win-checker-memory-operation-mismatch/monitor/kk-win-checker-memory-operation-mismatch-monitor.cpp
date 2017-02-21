
#include "stdafx.h"

#include "../../../kk-win-lib/kk-win-lib-namedpipe.h"
#include "../../../kk-win-lib/checker/kk-win-lib-checker-memory-operation-mismatch.h"
#include "../../../kk-win-lib/checker/kk-win-lib-checker-memory-operation-mismatch-server.h"

static
bool    s_optBreakTargetMismatch = true;

static
bool    s_optBreakMonitorMismatch = false;
static
bool    s_optBreakMonitorDeallocNull = false;
static
bool    s_optBreakMonitorAllocNull = false;


static
bool
parseOpt(const int argc, _TCHAR* argv[])
{
    {
        if ( 0 == argc )
        {
            return true;
        }

        if ( NULL == argv )
        {
            return false;
        }
    }

    for ( int index = 1; index < argc; ++index )
    {
        _TCHAR*     arg = argv[index];
        if ( NULL == arg )
        {
            continue;
        }

        if ( 0 == ::_tcsicmp( arg, _T("--disable-break-target-mismatch") ) )
        {
            s_optBreakTargetMismatch = false;
        }
        else
        if ( 0 == ::_tcsicmp( arg, _T("--enable-break-target-mismatch") ) )
        {
            s_optBreakTargetMismatch = true;
        }

        else
        if ( 0 == ::_tcsicmp( arg, _T("--disable-break-monitor-mismatch") ) )
        {
            s_optBreakMonitorMismatch = false;
        }
        else
        if ( 0 == ::_tcsicmp( arg, _T("--enable-break-monitor-mismatch") ) )
        {
            s_optBreakMonitorMismatch = true;
        }

        else
        if ( 0 == ::_tcsicmp( arg, _T("--disable-break-monitor-dealloc-null") ) )
        {
            s_optBreakMonitorDeallocNull = false;
        }
        else
        if ( 0 == ::_tcsicmp( arg, _T("--enable-break-monitor-dealloc-null") ) )
        {
            s_optBreakMonitorDeallocNull = true;
        }
        else
        if ( 0 == ::_tcsicmp( arg, _T("--disable-break-monitor-alloc-null") ) )
        {
            s_optBreakMonitorAllocNull = false;
        }
        else
        if ( 0 == ::_tcsicmp( arg, _T("--enable-break-monitor-alloc-null") ) )
        {
            s_optBreakMonitorAllocNull = true;
        }
    }

    return true;
}

int _tmain(int argc, _TCHAR* argv[])
{
    {
        parseOpt(argc, argv);

        if ( false == s_optBreakTargetMismatch && false == s_optBreakMonitorMismatch )
        {
            fprintf( stderr, "warning: no break! target and monitor\n" );
        }
    }

    kk::checker::MemoryOperationMismatchServer  memoryOperationMismatch;

    memoryOperationMismatch.init( s_optBreakTargetMismatch );
    memoryOperationMismatch.setBreakMismatch( s_optBreakMonitorMismatch );
    memoryOperationMismatch.serverStart();
    memoryOperationMismatch.serverWaitFinish();

    memoryOperationMismatch.term();


	return 0;
}

