/**
 * The MIT License
 * 
 * Copyright (C) 2017 Kiyofumi Kondoh
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */


#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "kk-win-lib-checker-memory-operation-mismatch-hook-crtnewaop.h"



#include "../kk-win-lib-namedpipe.h"
#include "kk-win-lib-checker-memory-operation-mismatch.h"
#include "kk-win-lib-checker-memory-operation-mismatch-client.h"


#include <stdlib.h>

#include <assert.h>

namespace kk
{

namespace checker
{

static
HMODULE
sModule = NULL;

static
DWORD64
sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationMAX];

static
MemoryOperationMismatchClient*
sMemoryOperationMismatch = NULL;


typedef void* (*PFN_new)(size_t size);
static
PFN_new             pfn_new = NULL;

typedef void* (*PFN_new_array)(size_t size);
static
PFN_new_array       pfn_new_array = NULL;


static
void*
my_new_array( size_t size )
{
    void* p = pfn_new(size);

    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationNewArray, (DWORD64)p );

    return p;
}


#if defined(_M_IX86)

#include <pshpack1.h>
struct HookJump {
    BYTE        opJmp;
    DWORD       relAddr;
};
#include <poppack.h>

static
HookJump    sHookJump = { 0xe9, 0 };
static
BYTE        sOrigCode[sizeof(sHookJump)];


#endif // defined(_M_IX86)

#if defined(_M_X64)
#include <pshpack1.h>
struct HookJump {
    BYTE        opPushR;

    WORD        opMovRim64;
    DWORD64     absAddr;

    DWORD       opXchgR;

    BYTE        opRet;
};
struct NearJump {
    BYTE        opJmp;
    DWORD       relAddr;
};
#include <poppack.h>

static
HookJump    sHookJump = { 0x50, 0xb848, 0, 0x24048748, 0xc3 };

static
BYTE        sOrigCode[sizeof(sHookJump)];

#endif // defined(_M_X64)



static
const DWORD
getPageSize(void)
{
    DWORD   value = 4*1024;

    SYSTEM_INFO     info;
    {
        ::GetSystemInfo( &info );
        if ( 0 != info.dwPageSize )
        {
            value = info.dwPageSize;
        }
    }

    return value;
}


static
bool
hookCRTNewAOP( const HMODULE hModule )
{
    if ( NULL == hModule )
    {
        return false;
    }

    const DWORD64   pageSize = getPageSize();

    DWORD64     minOffset = -1;
    DWORD64     maxOffset = 0;

    {
        minOffset = sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationCRTNewAOP];
        maxOffset = minOffset + sizeof(sHookJump);
    }

    {
        bool haveSpace = false;

        const DWORD64 funcSize = sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationCRTNewAOPSize];
        if ( sizeof(HookJump) < funcSize )
        {
            haveSpace = true;
        }

        if ( false == haveSpace )
        {
#if defined(_M_IX86)
            assert(false);
#endif // defined(_M_IX86)
#if defined(_M_X64)
            haveSpace = true;
            {
                const BYTE* p = reinterpret_cast<BYTE*>(hModule);
                const BYTE* pAddr = &p[minOffset];
                for ( size_t index = sizeof(NearJump); index < sizeof(HookJump); ++index )
                {
                    const BYTE  c = pAddr[index];
                    if ( 0x90 == c || 0xcc == c )
                    {
                        continue;
                    }

                    // no space
                    haveSpace = false;
                    break;
                }
            }
#endif // defined(_M_X64)
        }

        if ( false == haveSpace )
        {
            return false;
        }
    }



    {
        assert( sizeof(size_t) == sizeof(void*) );

#if defined(_M_IX86)
        sHookJump.relAddr = (DWORD)(((DWORD64)my_new_array) - (DWORD64)((LPBYTE)hModule + minOffset + sizeof(sHookJump)));
#endif // defined(_M_IX86)
#if defined(_M_X64)
        sHookJump.absAddr = ((size_t)my_new_array);
#endif // defined(_M_X64)
    }

    HANDLE hHeap = ::HeapCreate( 0, 0, 0 );
    if ( NULL != hHeap )
    {
        const DWORD64   minPageCount = minOffset / pageSize;
        const DWORD64   maxPageCount = (maxOffset + (pageSize-1)) / pageSize;

        const size_t    pageCount = static_cast<const size_t>( (maxPageCount - minPageCount) );

        const BYTE* pAddrBase = reinterpret_cast<const BYTE*>(hModule);
        const BYTE* pAddrMin = pAddrBase + pageSize*minPageCount;
        const BYTE* pAddrMax = pAddrBase + pageSize*maxPageCount;

        DWORD*   protect = reinterpret_cast<DWORD*>( ::HeapAlloc( hHeap, 0, sizeof(DWORD) * pageCount ) );
        if ( NULL != protect )
        {
            ZeroMemory( protect, sizeof(DWORD) * pageCount );

            // query protect
            {
                MEMORY_BASIC_INFORMATION    mbi;
                const BYTE* pAddr = pAddrMin;
                for ( size_t index = 0; index < pageCount; ++index, pAddr += pageSize )
                {
                    const size_t nRet = ::VirtualQuery( pAddr, &mbi, sizeof(mbi) );
                    if ( 0 == nRet )
                    {
                        const DWORD dwErr = ::GetLastError();
                    }
                    else
                    {
                        protect[index] = mbi.Protect;
                    }
                }
            }
            // change protect
            {
                const BYTE* pAddr = pAddrMin;
                for ( size_t index = 0; index < pageCount; ++index, pAddr += pageSize )
                {
                    if ( PAGE_EXECUTE_READWRITE != protect[index] )
                    {
                        DWORD   dwProtect;
                        const BOOL BRet = ::VirtualProtect( (LPVOID)pAddr, (size_t)pageSize, PAGE_EXECUTE_READWRITE, &dwProtect );
                        if ( !BRet )
                        {
                            const DWORD dwErr = ::GetLastError();
                        }
                        else
                        {
                            assert( dwProtect == protect[index] );
                            protect[index] = dwProtect;
                        }
                    }
                }
            }

            {
                assert( sizeof(size_t) == sizeof(void*) );
                BYTE*   p = reinterpret_cast<BYTE*>(hModule);

                if ( 0 != sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationCRTNewAOP] )
                {
                    BYTE* pCode = reinterpret_cast<BYTE*>(p + sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationCRTNewAOP]);
                    for ( size_t index = 0; index < sizeof(sHookJump); ++index )
                    {
                        sOrigCode[index] = pCode[index];
                    }
                    BYTE* pHookCode = reinterpret_cast<BYTE*>(&sHookJump);
                    for ( size_t index = 0; index < sizeof(sHookJump); ++index )
                    {
                        pCode[index] = pHookCode[index];
                    }

                    size_t* crtNew = reinterpret_cast<size_t*>(p + sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationNew]);
                    pfn_new = reinterpret_cast<PFN_new>(*crtNew);
                }


            }

            // restore protect
            {
                const BYTE* pAddr = pAddrMin;
                for ( size_t index = 0; index < pageCount; ++index, pAddr += pageSize )
                {
                    if ( PAGE_EXECUTE_READWRITE != protect[index] )
                    {
                        DWORD   dwProtect;
                        const BOOL BRet = ::VirtualProtect( (LPVOID)pAddr, (size_t)pageSize, protect[index], &dwProtect );
                        if ( !BRet )
                        {
                            const DWORD dwErr = ::GetLastError();
                        }
                        else
                        {
                        }
                    }
                }
            }
        }

        if ( NULL != protect )
        {
            const BOOL BRet = ::HeapFree( hHeap, 0, protect );
            if ( !BRet )
            {
                const DWORD dwErr = ::GetLastError();
            }
            else
            {
                protect = NULL;
            }
        }

        ::OutputDebugStringA("");
    }

    if ( NULL != hHeap )
    {
        const BOOL BRet = ::HeapDestroy( hHeap );
        if ( !BRet )
        {
            const DWORD dwErr = ::GetLastError();
        }
        else
        {
            hHeap = NULL;
        }
    }


    return true;
}

static
bool
unhookCRTNewAOP( void )
{
    if ( NULL == sModule )
    {
        return false;
    }

    const DWORD64   pageSize = getPageSize();

    DWORD64     minOffset = -1;
    DWORD64     maxOffset = 0;

    {
        minOffset = sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationCRTNewAOP];
        maxOffset = minOffset + sizeof(sHookJump);
    }

    HANDLE hHeap = ::HeapCreate( 0, 0, 0 );
    if ( NULL != hHeap )
    {
        const DWORD64   minPageCount = minOffset / pageSize;
        const DWORD64   maxPageCount = (maxOffset + (pageSize-1)) / pageSize;

        const size_t    pageCount = static_cast<const size_t>( (maxPageCount - minPageCount) );

        const BYTE* pAddrBase = reinterpret_cast<const BYTE*>(sModule);
        const BYTE* pAddrMin = pAddrBase + pageSize*minPageCount;
        const BYTE* pAddrMax = pAddrBase + pageSize*maxPageCount;

        DWORD*   protect = reinterpret_cast<DWORD*>( ::HeapAlloc( hHeap, 0, sizeof(DWORD) * pageCount ) );
        if ( NULL != protect )
        {
            ZeroMemory( protect, sizeof(DWORD) * pageCount );

            // query protect
            {
                MEMORY_BASIC_INFORMATION    mbi;
                const BYTE* pAddr = pAddrMin;
                for ( size_t index = 0; index < pageCount; ++index, pAddr += pageSize )
                {
                    const size_t nRet = ::VirtualQuery( pAddr, &mbi, sizeof(mbi) );
                    if ( 0 == nRet )
                    {
                        const DWORD dwErr = ::GetLastError();
                    }
                    else
                    {
                        protect[index] = mbi.Protect;
                    }
                }
            }
            // change protect
            {
                const BYTE* pAddr = pAddrMin;
                for ( size_t index = 0; index < pageCount; ++index, pAddr += pageSize )
                {
                    if ( PAGE_EXECUTE_READWRITE != protect[index] )
                    {
                        DWORD   dwProtect;
                        const BOOL BRet = ::VirtualProtect( (LPVOID)pAddr, (size_t)pageSize, PAGE_EXECUTE_READWRITE, &dwProtect );
                        if ( !BRet )
                        {
                            const DWORD dwErr = ::GetLastError();
                        }
                        else
                        {
                            assert( dwProtect == protect[index] );
                            protect[index] = dwProtect;
                        }
                    }
                }
            }

            {
                assert( sizeof(size_t) == sizeof(void*) );
                BYTE*   p = reinterpret_cast<BYTE*>(sModule);

                if ( 0 != sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationCRTNewAOP] )
                {
                    if ( NULL != pfn_new )
                    {
                        BYTE* pCode = reinterpret_cast<BYTE*>(p + sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationCRTNewAOP]);
                        for ( size_t index = 0; index < sizeof(sHookJump); ++index )
                        {
                            pCode[index] = sOrigCode[index];
                        }
                    }
                }

            }


            // restore protect
            {
                const BYTE* pAddr = pAddrMin;
                for ( size_t index = 0; index < pageCount; ++index, pAddr += pageSize )
                {
                    if ( PAGE_EXECUTE_READWRITE != protect[index] )
                    {
                        DWORD   dwProtect;
                        const BOOL BRet = ::VirtualProtect( (LPVOID)pAddr, (size_t)pageSize, protect[index], &dwProtect );
                        if ( !BRet )
                        {
                            const DWORD dwErr = ::GetLastError();
                        }
                        else
                        {
                        }
                    }
                }
            }
        }

        if ( NULL != protect )
        {
            const BOOL BRet = ::HeapFree( hHeap, 0, protect );
            if ( !BRet )
            {
                const DWORD dwErr = ::GetLastError();
            }
            else
            {
                protect = NULL;
            }
        }

        ::OutputDebugStringA("");
    }

    if ( NULL != hHeap )
    {
        const BOOL BRet = ::HeapDestroy( hHeap );
        if ( !BRet )
        {
            const DWORD dwErr = ::GetLastError();
        }
        else
        {
            hHeap = NULL;
        }
    }


    pfn_new_array = NULL;


    return true;
}


bool
hookMemoryOperationMismatchCRTNewAOP( const HMODULE hModule, MemoryOperationMismatchClient* pMOM )
{
    bool result = true;
    {
        const bool bRet = pMOM->getCRTOffsetIAT( sCRTOffsetIAT );
        if ( !bRet )
        {
            result = false;
        }
        else
        {
            sMemoryOperationMismatch = pMOM;
        }
    }

    if ( result )
    {
        const bool bRet = hookCRTNewAOP( hModule );
        if ( !bRet )
        {
            result = false;
        }
        else
        {
            sModule = hModule;
        }
    }

    return result;
}


bool
unhookMemoryOperationMismatchCRTNewAOP( void )
{
    bool result = true;
    {
        const bool bRet = unhookCRTNewAOP();
        if ( !bRet )
        {
            result = false;
        }
    }

    sMemoryOperationMismatch = NULL;
    sModule = NULL;

    return result;
}



} // namespace checker

} // namespace kk

