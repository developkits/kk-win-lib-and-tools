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

#include "kk-win-lib-checker-memory-operation-mismatch-hook-util.h"



#include <assert.h>

namespace kk
{

namespace checker
{

namespace hookutil
{

static
LPVOID
getAlignedPage( LPVOID pAddr, const DWORD pageSize )
{
    LPVOID result = NULL;

    if ( 0 == pageSize )
    {
        assert( 0 != pageSize );
        return NULL;
    }

    assert( sizeof(size_t) == sizeof(LPVOID) );

    const size_t p = reinterpret_cast<size_t>(pAddr);

    const size_t count = p / pageSize;
    const size_t addr = count * pageSize;

    result = reinterpret_cast<LPVOID>(addr);

    return result;
}

static const DWORD64 limit31bit = 0x000000007fffffffULL;
static const DWORD64 limit32bit = 0x00000000ffffffffULL;
static const DWORD64 limit64bit = 0xffffffffffffffffULL;

static
const DWORD64
getLimitPageBackword( const DWORD64 pAddr, const DWORD64 pageSize )
{
    DWORD64 addr = 0;

#if defined(_M_IX86)
    addr = pageSize;
#endif // defined(_M_IX86)

#if defined(_M_X64)
    if ( pAddr < limit31bit )
    {
        addr = pageSize;
    }
    else
    {
        addr = (pAddr+pageSize)-limit31bit;
    }
#endif // defined(_M_X64)

    addr = (size_t)getAlignedPage( (LPVOID)addr, (const DWORD)pageSize );

    return addr;
}

static
const DWORD64
getLimitPageForward( const DWORD64 pAddr, const DWORD64 pageSize )
{
    DWORD64 addr = 0;

#if defined(_M_IX86)
    addr = limit32bit-pageSize;
#endif // defined(_M_IX86)

#if defined(_M_X64)
    if ( (limit64bit-limit31bit) < pAddr )
    {
        addr = limit64bit-pageSize;
    }
    else
    {
        addr = pAddr + limit31bit;
    }
#endif // defined(_M_X64)

    addr = (size_t)getAlignedPage( (LPVOID)addr, (const DWORD)pageSize );

    return addr;
}




const DWORD
getPageSize(void)
{
    DWORD   result = 4*1024;

    SYSTEM_INFO     info;
    {
        ::GetSystemInfo( &info );
        if ( 0 != info.dwPageSize )
        {
            result = info.dwPageSize;
        }
    }

    return result;
}

bool
trampolinePageDeallocate( LPVOID pTrampolinePage )
{
    bool result = true;

    {
        const DWORD dwFreeType = MEM_RELEASE;

        const BOOL BRet = ::VirtualFree( pTrampolinePage, 0, dwFreeType );
        if ( !BRet )
        {
            const DWORD dwErr = ::GetLastError();
            result = false;
        }
    }

    return result;
}

bool
tranpolinePageDropWriteOperation( LPVOID pTrampolinePage, const DWORD changeSize )
{
    bool result = true;

    const size_t size = static_cast<const size_t>(changeSize);
    {
        DWORD dwProtect = 0;

        const BOOL BRet = ::VirtualProtect( (LPVOID)pTrampolinePage, size, PAGE_EXECUTE_READ, &dwProtect );
        if ( !BRet )
        {
            const DWORD dwErr = ::GetLastError();
            result = false;
        }
    }

    {
        HANDLE hProcess = ::GetCurrentProcess();
        const BOOL BRet = ::FlushInstructionCache( hProcess, pTrampolinePage, size );
        if ( !BRet )
        {
            const DWORD dwErr = ::GetLastError();
        }
    }

    return result;
}

LPVOID
trampolinePageAllocate( LPVOID minAddr, LPVOID maxAddr )
{
    LPVOID pTrampolinePage = NULL;

    const DWORD pageSize = getPageSize();

    {
        const DWORD flAllocationType = MEM_COMMIT | MEM_RESERVE;
        const DWORD flProtect = PAGE_READWRITE;

        {
            const DWORD64 pAddrStart = reinterpret_cast<DWORD64>(getAlignedPage(maxAddr,pageSize));
            const DWORD64 pAddrEnd = getLimitPageBackword(pAddrStart, pageSize);

            for (
                DWORD64 pAddr = pAddrStart - pageSize;
                pAddrEnd < pAddr;
                pAddr -= pageSize
            )
            {
                LPVOID p = ::VirtualAlloc( (LPVOID)pAddr, (size_t)pageSize, flAllocationType, flProtect );
                if ( NULL != p )
                {
                    pTrampolinePage = p;
                    break;
                }
            }
        }

        if ( NULL == pTrampolinePage )
        {
            const DWORD64 pAddrStart = reinterpret_cast<DWORD64>(getAlignedPage(minAddr,pageSize));
            const DWORD64 pAddrEnd = getLimitPageForward(pAddrStart, pageSize);

            for (
                DWORD64 pAddr = pAddrStart + pageSize*16;
                pAddr < pAddrEnd;
                pAddr += pageSize
            )
            {
                LPVOID p = ::VirtualAlloc( (LPVOID)pAddr, (size_t)pageSize, flAllocationType, flProtect );
                if ( NULL != p )
                {
                    pTrampolinePage = p;
                    break;
                }
            }
        }
    }

    return pTrampolinePage;
}






} // namespace hookutil

} // namespace checker

} // namespace kk

