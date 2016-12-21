
#include "stdafx.h"

extern "C" bool initMemoryOperationMismatch( const size_t processId );
extern "C" bool termMemoryOperationMismatch(void);

BOOL
APIENTRY
DllMain(
    HMODULE hModule
    , DWORD  ul_reason_for_call
    , LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        {
            const DWORD processId = ::GetCurrentProcessId();
            const HMODULE module = ::GetModuleHandle( NULL );
            {
                char temp[128];
                ::wsprintfA( temp, "%u\n", processId );
                ::OutputDebugStringA( temp );
            }
            initMemoryOperationMismatch( processId );
        }
        break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;

    case DLL_PROCESS_DETACH:
        {
            termMemoryOperationMismatch();
        }
        break;
    }

    return TRUE;
}

