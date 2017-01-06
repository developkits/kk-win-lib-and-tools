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
deallocateTrampolinePage( LPVOID pTrampolinePage )
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

LPVOID
allocateTrampolinePage( LPVOID minAddr, LPVOID maxAddr )
{
    LPVOID pTrampolinePage = NULL;

    const DWORD pageSize = getPageSize();

    {
        const DWORD flAllocationType = MEM_COMMIT | MEM_RESERVE;
        const DWORD flProtect = PAGE_READWRITE;
        // todo: x64 limit -2G to +2G
        {
            for ( DWORD64 pAddr = reinterpret_cast<DWORD64>(getAlignedPage(maxAddr,pageSize)) - pageSize; pageSize < pAddr; pAddr -= pageSize )
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
            for ( DWORD64 pAddr = reinterpret_cast<DWORD64>(getAlignedPage(minAddr,pageSize)) + pageSize*16; pAddr < (-1)-pageSize; pAddr += pageSize )
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

