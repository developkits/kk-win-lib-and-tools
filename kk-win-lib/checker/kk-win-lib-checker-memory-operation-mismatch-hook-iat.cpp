/**
 * The MIT License
 * 
 * Copyright (C) 2016 Kiyofumi Kondoh
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

#include "kk-win-lib-checker-memory-operation-mismatch-hook-iat.h"



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


typedef void* (*PFN_malloc)(size_t size);
PFN_malloc      pfn_malloc = NULL;

typedef void (*PFN_free)(void* p);
PFN_free        pfn_free = NULL;

typedef void* (*PFN_calloc)(size_t num, size_t size);
PFN_calloc      pfn_calloc = NULL;

typedef void* (*PFN_realloc)(void* p, size_t size);
PFN_realloc     pfn_realloc = NULL;

typedef char* (*PFN_strdup)(const char* str);
PFN_strdup      pfn_strdup = NULL;

typedef wchar_t* (*PFN_wcsdup)(const wchar_t* str);
PFN_wcsdup      pfn_wcsdup = NULL;

static
void*
my_malloc(size_t size)
{
    void* p= pfn_malloc(size);

    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationMalloc, (DWORD64)p );

    return p;
}

static
void
my_free(void* p)
{
    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationFree, (DWORD64)p );

    pfn_free(p);

    return;
}

static
void*
my_calloc(size_t num, size_t size)
{
    void* p = pfn_calloc( num, size );

    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationCalloc, (DWORD64)p );

    return p;
}

static
void*
my_realloc(void* p, size_t size)
{
    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationReallocFree, (DWORD64)p );

    void* result = pfn_realloc( p, size );

    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationRealloc, (DWORD64)result );

    return p;
}

static
char*
my_strdup(const char* str)
{
    char* p= pfn_strdup(str);

    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationStrdup, (DWORD64)p );

    return p;
}

static
wchar_t*
my_wcsdup(const wchar_t* str)
{
    wchar_t* p= pfn_wcsdup(str);

    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationWcsdup, (DWORD64)p );

    return p;
}

typedef void* (*PFN_new)(size_t size);
PFN_new             pfn_new = NULL;

typedef void (*PFN_delete)(void* p);
PFN_delete          pfn_delete = NULL;

typedef void* (*PFN_new_array)(size_t size);
PFN_new_array       pfn_new_array = NULL;

typedef void (*PFN_delete_array)(void* p);
PFN_delete_array    pfn_delete_array = NULL;


static
void*
my_new( size_t size )
{
    void* p = pfn_new(size);

    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationNew, (DWORD64)p );

    return p;
}

static
void
my_delete( void* p )
{
    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationDelete, (DWORD64)p );

    pfn_delete(p);

    return;
}

static
void*
my_new_array( size_t size )
{
    void* p = pfn_new_array(size);

    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationNewArray, (DWORD64)p );

    return p;
}

static
void
my_delete_array( void* p )
{
    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationDeleteArray, (DWORD64)p );

    pfn_delete_array(p);

    return;
}


typedef void* (*PFN_aligned_malloc)(size_t size,size_t align);
PFN_aligned_malloc      pfn_aligned_malloc = NULL;

typedef void (*PFN_aligned_free)(void* p);
PFN_aligned_free        pfn_aligned_free = NULL;

typedef void* (*PFN_aligned_realloc)(void* p, size_t size, size_t align);
PFN_aligned_realloc     pfn_aligned_realloc = NULL;

typedef void* (*PFN_aligned_recalloc)(void* p, size_t num, size_t size, size_t align);
PFN_aligned_recalloc    pfn_aligned_recalloc = NULL;



static
void*
my_aligned_malloc( size_t size, size_t align )
{
    void* p = pfn_aligned_malloc( size, align );

    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationAlignedMalloc, (DWORD64)p );

    return p;
}

static
void
my_aligned_free( void* p )
{
    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationAlignedFree, (DWORD64)p );

    pfn_aligned_free( p );

    return;
}

static
void*
my_aligned_realloc(void* p, size_t size, size_t align)
{
    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationAlignedReallocFree, (DWORD64)p );

    void* result = pfn_aligned_realloc( p, size, align );

    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationAlignedRealloc, (DWORD64)result );

    return result;
}

static
void*
my_aligned_recalloc(void* p, size_t num, size_t size, size_t align)
{
    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationAlignedRecallocFree, (DWORD64)p );

    void* result = pfn_aligned_recalloc( p, num, size, align );

    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationAlignedRecalloc, (DWORD64)result );

    return result;
}






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
hookCRTbyIAT( const HMODULE hModule )
{
    if ( NULL == hModule )
    {
        return false;
    }

    const DWORD64   pageSize = getPageSize();

    DWORD64     minOffset = -1;
    DWORD64     maxOffset = 0;

    {
        for ( size_t index = 0; index < MemoryOperationMismatch::kIndexOperationMAX; ++index )
        {
            const DWORD64 value = sCRTOffsetIAT[index];
            if ( 0 == value )
            {
                continue;
            }

            if ( value < minOffset )
            {
                minOffset = value;
            }

            if ( maxOffset < value )
            {
                maxOffset = value;
            }

        }
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
                    if ( PAGE_READWRITE != protect[index] )
                    {
                        DWORD   dwProtect;
                        const BOOL BRet = ::VirtualProtect( (LPVOID)pAddr, (size_t)pageSize, PAGE_READWRITE, &dwProtect );
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
                if ( 0 != sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationMalloc] )
                {
                    size_t* pIAT = reinterpret_cast<size_t*>(p + sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationMalloc]);
                    pfn_malloc = reinterpret_cast<kk::checker::PFN_malloc>(*pIAT);
                    *pIAT = reinterpret_cast<size_t>(my_malloc);
                }
                if ( 0 != sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationFree] )
                {
                    size_t* pIAT = reinterpret_cast<size_t*>(p + sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationFree]);
                    pfn_free= reinterpret_cast<kk::checker::PFN_free>(*pIAT);
                    *pIAT = reinterpret_cast<size_t>(my_free);
                }
                if ( 0 != sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationCalloc] )
                {
                    size_t* pIAT = reinterpret_cast<size_t*>(p + sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationCalloc]);
                    pfn_calloc = reinterpret_cast<kk::checker::PFN_calloc>(*pIAT);
                    *pIAT = reinterpret_cast<size_t>(my_calloc);
                }
                if ( 0 != sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationRealloc] )
                {
                    size_t* pIAT = reinterpret_cast<size_t*>(p + sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationRealloc]);
                    pfn_realloc = reinterpret_cast<kk::checker::PFN_realloc>(*pIAT);
                    *pIAT = reinterpret_cast<size_t>(my_realloc);
                }
                if ( 0 != sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationStrdup] )
                {
                    size_t* pIAT = reinterpret_cast<size_t*>(p + sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationStrdup]);
                    pfn_strdup = reinterpret_cast<kk::checker::PFN_strdup>(*pIAT);
                    *pIAT = reinterpret_cast<size_t>(my_strdup);
                }
                if ( 0 != sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationWcsdup] )
                {
                    size_t* pIAT = reinterpret_cast<size_t*>(p + sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationWcsdup]);
                    pfn_wcsdup = reinterpret_cast<kk::checker::PFN_wcsdup>(*pIAT);
                    *pIAT = reinterpret_cast<size_t>(my_wcsdup);
                }

                if ( 0 != sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationNew] )
                {
                    size_t* pIAT = reinterpret_cast<size_t*>(p + sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationNew]);
                    pfn_new = reinterpret_cast<kk::checker::PFN_new>(*pIAT);
                    *pIAT = reinterpret_cast<size_t>(my_new);
                }
                if ( 0 != sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationDelete] )
                {
                    size_t* pIAT = reinterpret_cast<size_t*>(p + sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationDelete]);
                    pfn_delete = reinterpret_cast<kk::checker::PFN_delete>(*pIAT);
                    *pIAT = reinterpret_cast<size_t>(my_delete);
                }
                if ( 0 != sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationNewArray] )
                {
                    size_t* pIAT = reinterpret_cast<size_t*>(p + sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationNewArray]);
                    pfn_new_array = reinterpret_cast<kk::checker::PFN_new_array>(*pIAT);
                    *pIAT = reinterpret_cast<size_t>(my_new_array);
                }
                if ( 0 != sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationDeleteArray] )
                {
                    size_t* pIAT = reinterpret_cast<size_t*>(p + sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationDeleteArray]);
                    pfn_delete_array = reinterpret_cast<kk::checker::PFN_delete_array>(*pIAT);
                    *pIAT = reinterpret_cast<size_t>(my_delete_array);
                }

                if ( 0 != sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationAlignedMalloc] )
                {
                    size_t* pIAT = reinterpret_cast<size_t*>(p + sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationAlignedMalloc]);
                    pfn_aligned_malloc = reinterpret_cast<kk::checker::PFN_aligned_malloc>(*pIAT);
                    *pIAT = reinterpret_cast<size_t>(my_aligned_malloc);
                }
                if ( 0 != sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationAlignedFree] )
                {
                    size_t* pIAT = reinterpret_cast<size_t*>(p + sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationAlignedFree]);
                    pfn_aligned_free= reinterpret_cast<kk::checker::PFN_aligned_free>(*pIAT);
                    *pIAT = reinterpret_cast<size_t>(my_aligned_free);
                }
                if ( 0 != sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationAlignedReCalloc] )
                {
                    size_t* pIAT = reinterpret_cast<size_t*>(p + sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationAlignedReCalloc]);
                    pfn_aligned_recalloc = reinterpret_cast<kk::checker::PFN_aligned_recalloc>(*pIAT);
                    *pIAT = reinterpret_cast<size_t>(my_aligned_recalloc);
                }
                if ( 0 != sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationAlignedRealloc] )
                {
                    size_t* pIAT = reinterpret_cast<size_t*>(p + sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationAlignedRealloc]);
                    pfn_aligned_realloc = reinterpret_cast<kk::checker::PFN_aligned_realloc>(*pIAT);
                    *pIAT = reinterpret_cast<size_t>(my_aligned_realloc);
                }

            }

            // restore protect
            {
                const BYTE* pAddr = pAddrMin;
                for ( size_t index = 0; index < pageCount; ++index, pAddr += pageSize )
                {
                    if ( PAGE_READWRITE != protect[index] )
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
unhookCRTbyIAT( void )
{
    if ( NULL == sModule )
    {
        return false;
    }

    const DWORD64   pageSize = getPageSize();

    DWORD64     minOffset = -1;
    DWORD64     maxOffset = 0;

    {
        for ( size_t index = 0; index < MemoryOperationMismatch::kIndexOperationMAX; ++index )
        {
            const DWORD64 value = sCRTOffsetIAT[index];
            if ( 0 == value )
            {
                continue;
            }

            if ( value < minOffset )
            {
                minOffset = value;
            }

            if ( maxOffset < value )
            {
                maxOffset = value;
            }

        }
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
                    if ( PAGE_READWRITE != protect[index] )
                    {
                        DWORD   dwProtect;
                        const BOOL BRet = ::VirtualProtect( (LPVOID)pAddr, (size_t)pageSize, PAGE_READWRITE, &dwProtect );
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
                if ( 0 != sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationMalloc] )
                {
                    size_t* pIAT = reinterpret_cast<size_t*>(p + sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationMalloc]);
                    *pIAT = reinterpret_cast<size_t>(pfn_malloc);
                    pfn_malloc = NULL;
                }
                if ( 0 != sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationFree] )
                {
                    size_t* pIAT = reinterpret_cast<size_t*>(p + sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationFree]);
                    *pIAT = reinterpret_cast<size_t>(pfn_free);
                    pfn_free = NULL;
                }
                if ( 0 != sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationCalloc] )
                {
                    size_t* pIAT = reinterpret_cast<size_t*>(p + sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationCalloc]);
                    *pIAT = reinterpret_cast<size_t>(pfn_calloc);
                    pfn_calloc = NULL;
                }
                if ( 0 != sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationRealloc] )
                {
                    size_t* pIAT = reinterpret_cast<size_t*>(p + sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationRealloc]);
                    *pIAT = reinterpret_cast<size_t>(pfn_realloc);
                    pfn_realloc = NULL;
                }
                if ( 0 != sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationStrdup] )
                {
                    size_t* pIAT = reinterpret_cast<size_t*>(p + sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationStrdup]);
                    *pIAT = reinterpret_cast<size_t>(pfn_strdup);
                    pfn_strdup = NULL;
                }
                if ( 0 != sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationWcsdup] )
                {
                    size_t* pIAT = reinterpret_cast<size_t*>(p + sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationWcsdup]);
                    *pIAT = reinterpret_cast<size_t>(pfn_wcsdup);
                    pfn_wcsdup = NULL;
                }

                if ( 0 != sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationNew] )
                {
                    size_t* pIAT = reinterpret_cast<size_t*>(p + sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationNew]);
                    *pIAT = reinterpret_cast<size_t>(pfn_new);
                    pfn_new = NULL;
                }
                if ( 0 != sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationDelete] )
                {
                    size_t* pIAT = reinterpret_cast<size_t*>(p + sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationDelete]);
                    *pIAT = reinterpret_cast<size_t>(pfn_delete);
                    pfn_delete = NULL;
                }
                if ( 0 != sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationNewArray] )
                {
                    size_t* pIAT = reinterpret_cast<size_t*>(p + sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationNewArray]);
                    *pIAT = reinterpret_cast<size_t>(pfn_new_array);
                    pfn_new_array = NULL;
                }
                if ( 0 != sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationDeleteArray] )
                {
                    size_t* pIAT = reinterpret_cast<size_t*>(p + sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationDeleteArray]);
                    *pIAT = reinterpret_cast<size_t>(pfn_delete_array);
                    pfn_delete_array = NULL;
                }

                if ( 0 != sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationAlignedMalloc] )
                {
                    size_t* pIAT = reinterpret_cast<size_t*>(p + sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationAlignedMalloc]);
                    *pIAT = reinterpret_cast<size_t>(pfn_aligned_malloc);
                    pfn_aligned_malloc = NULL;
                }
                if ( 0 != sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationAlignedFree] )
                {
                    size_t* pIAT = reinterpret_cast<size_t*>(p + sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationAlignedFree]);
                    *pIAT = reinterpret_cast<size_t>(pfn_aligned_free);
                    pfn_aligned_free = NULL;
                }
                if ( 0 != sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationAlignedReCalloc] )
                {
                    size_t* pIAT = reinterpret_cast<size_t*>(p + sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationAlignedReCalloc]);
                    *pIAT = reinterpret_cast<size_t>(pfn_aligned_recalloc);
                    pfn_aligned_recalloc = NULL;
                }
                if ( 0 != sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationAlignedRealloc] )
                {
                    size_t* pIAT = reinterpret_cast<size_t*>(p + sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationAlignedRealloc]);
                    *pIAT = reinterpret_cast<size_t>(pfn_aligned_realloc);
                    pfn_aligned_realloc = NULL;
                }

            }


            // restore protect
            {
                const BYTE* pAddr = pAddrMin;
                for ( size_t index = 0; index < pageCount; ++index, pAddr += pageSize )
                {
                    if ( PAGE_READWRITE != protect[index] )
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

    pfn_malloc = NULL;
    pfn_free= NULL;
    pfn_calloc= NULL;
    pfn_realloc = NULL;

    pfn_new = NULL;
    pfn_delete = NULL;
    pfn_new_array = NULL;
    pfn_delete_array = NULL;

    pfn_aligned_malloc = NULL;
    pfn_aligned_free = NULL;
    pfn_aligned_recalloc = NULL;
    pfn_aligned_realloc = NULL;


    return true;
}


bool
hookMemoryOperationMismatchIAT( const HMODULE hModule, MemoryOperationMismatchClient* pMOM )
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
        const bool bRet = hookCRTbyIAT( hModule );
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
unhookMemoryOperationMismatchIAT( void )
{
    bool result = true;
    {
        const bool bRet = unhookCRTbyIAT();
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

