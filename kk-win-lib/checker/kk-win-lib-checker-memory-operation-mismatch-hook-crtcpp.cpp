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

#include "kk-win-lib-checker-memory-operation-mismatch-hook-crtcpp.h"



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
sCRTStaticFunc[MemoryOperationMismatch::kIndexCRTStaticFuncMAX];

static
MemoryOperationMismatchClient*
sMemoryOperationMismatch = NULL;

typedef void* (*PFN_new)(size_t size);
static
PFN_new             pfn_new = NULL;

typedef void* (*PFN_new_array)(size_t size);
static
PFN_new_array       pfn_new_array = NULL;

typedef void (*PFN_delete)(void* p);
static
PFN_delete          pfn_delete = NULL;

typedef void (*PFN_delete_array)(void* p);
static
PFN_delete_array    pfn_delete_array = NULL;

typedef void (*PFN_delete_size)(void* p,size_t size);
static
PFN_delete_size         pfn_delete_size = NULL;

typedef void (*PFN_delete_array_size)(void* p,size_t size);
static
PFN_delete_array_size   pfn_delete_array_size = NULL;

static
void*
my_new( size_t size )
{
    void* p = pfn_new(size);

    //sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationCRTStaticNew, (DWORD64)p );
    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationNew, (DWORD64)p );

    return p;
}

static
void*
my_new_array( size_t size )
{
    void* p = pfn_new(size);

    //sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationCRTStaticNewArray, (DWORD64)p );
    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationNewArray, (DWORD64)p );

    return p;
}

static
void
my_delete( void* p )
{
    //sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationCRTStaticDelete, (DWORD64)p );
    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationDelete, (DWORD64)p );

    pfn_delete(p);

    return;
}

static
void
my_delete_array( void* p )
{
    //sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationCRTStaticDeleteArray, (DWORD64)p );
    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationDeleteArray, (DWORD64)p );

    pfn_delete_array(p);

    return;
}

static
void
my_delete_size( void* p, size_t size )
{
    //sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationCRTStaticDeleteSize, (DWORD64)p );
    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationDelete, (DWORD64)p );

    pfn_delete_size(p,size);

    return;
}

static
void
my_delete_array_size( void* p, size_t size )
{
    //sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationCRTStaticDeleteArraySize, (DWORD64)p );
    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationDeleteArray, (DWORD64)p );

    pfn_delete_array_size(p,size);

    return;
}


static
LPVOID      sPageTrampoline  = NULL;


#include <pshpack1.h>
struct HookJump {
    BYTE        opJmp;
    DWORD       relAddr;
};
#include <poppack.h>

static
const
HookJump    sHookJump = { 0xe9, 0 };

#if defined(_M_X64)
#include <pshpack1.h>
struct LongJump {
    BYTE        opPushR;

    WORD        opMovRim64;
    DWORD64     absAddr;

    DWORD       opXchgR;

    BYTE        opRet;
};
#include <poppack.h>

static
const
LongJump    sLongJump = { 0x50, 0xb848, 0, 0x24048748, 0xc3 };

#endif // defined(_M_X64)

#include <pshpack1.h>
struct TrampolineHeader
{
    WORD    origSize;
    BYTE    origCodeSize;
    BYTE    hookCodeSize;
};


struct Trampoline
{
    union
    {
        BYTE                data[0x10];
        TrampolineHeader    header;
    } uh;
    BYTE    origCode[0x10];
    BYTE    hookCode[0x20];

    BYTE    longJump[0x20];
};
#include <poppack.h>


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
hookCRTCPP( const HMODULE hModule )
{
    if ( NULL == hModule )
    {
        return false;
    }

    const DWORD64   pageSize = getPageSize();

    {
        const DWORD flAllocationType = MEM_COMMIT | MEM_RESERVE;
        const DWORD flProtect = PAGE_READWRITE;
        {
            for ( DWORD64 pAddr = reinterpret_cast<DWORD64>(hModule) - pageSize; pageSize < pAddr; pAddr -= pageSize )
            {
                LPVOID p = ::VirtualAlloc( (LPVOID)pAddr, (size_t)pageSize, flAllocationType, flProtect );
                if ( NULL != p )
                {
                    sPageTrampoline = p;
                    break;
                }
            }
        }
        if ( NULL == sPageTrampoline )
        {
            for ( DWORD64 pAddr = reinterpret_cast<DWORD64>(hModule) + pageSize*16; pAddr < (-1)-pageSize; pAddr += pageSize )
            {
                LPVOID p = ::VirtualAlloc( (LPVOID)pAddr, (size_t)pageSize, flAllocationType, flProtect );
                if ( NULL != p )
                {
                    sPageTrampoline = p;
                    break;
                }
            }
        }
    }

    if ( NULL == sPageTrampoline )
    {
        return false;
    }

    assert( sizeof(size_t) == sizeof(void*) );

    for ( size_t indexOperation = 0; indexOperation < MemoryOperationMismatch::kIndexCRTStaticFuncMAX; indexOperation += 2 )
    {
        DWORD64     minOffset = -1;
        DWORD64     maxOffset = 0;

        {
            minOffset = sCRTStaticFunc[indexOperation+0];
            maxOffset = minOffset + sizeof(sHookJump);
        }

        {
            bool haveSpace = false;

            const DWORD64 funcSize = sCRTStaticFunc[indexOperation+1];
            if ( 0 == funcSize )
            {
                continue;
            }
            if ( sizeof(HookJump) <= funcSize )
            {
                haveSpace = true;
            }

            if ( false == haveSpace )
            {
                assert(false);
            }

            if ( false == haveSpace )
            {
                return false;
            }
        }


        HANDLE hHeap = ::HeapCreate( 0, 0, 0 );
        if ( NULL != hHeap )
        {
            const DWORD64   minPageCount = minOffset / pageSize;
            const DWORD64   maxPageCount = (maxOffset + (pageSize-1)) / pageSize;

            const size_t    pageCount = static_cast<const size_t>( (maxPageCount - minPageCount) );

            const BYTE* pAddrModule = reinterpret_cast<const BYTE*>(hModule);
            const BYTE* pAddrMin = pAddrModule + pageSize*minPageCount;
            const BYTE* pAddrMax = pAddrModule + pageSize*maxPageCount;

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

                    if ( 0 != sCRTStaticFunc[indexOperation+0] )
                    {
                        BYTE* pCode = reinterpret_cast<BYTE*>(p + sCRTStaticFunc[indexOperation+0]);
                        Trampoline* pTrampoline = reinterpret_cast<Trampoline*>((LPBYTE)sPageTrampoline + sizeof(Trampoline)*indexOperation/2);
                        BYTE* pOrigCode = pTrampoline->origCode;
                        BYTE* pHookCode = pTrampoline->hookCode;

                        size_t indexPatch = 0;
                        size_t indexOrig = 0;
                        size_t indexHook = 0;
                        bool    lastInstJump = false;
                        for ( indexOrig = 0; indexOrig < sizeof(HookJump); )
                        {
                            const BYTE p = pCode[indexOrig];
                            lastInstJump = true;
#if defined(_M_X64)
                            // REX prefix?
                            if ( 0x40 == (p & 0xf0) )
                            {
                                pOrigCode[indexOrig] = p;
                                pHookCode[indexHook] = p;
                                indexOrig += 1;
                                indexHook += 1;

                                continue;
                            }
#endif // defined(_M_X64)

                            switch ( p )
                            {
                            case 0x53: // push rbx
                            case 0x55: // push ebp
                                pOrigCode[indexOrig] = p;
                                pHookCode[indexHook] = p;
                                indexOrig += 1;
                                indexHook += 1;
                                lastInstJump = false;
                                break;
                            case 0x5d:
                                pOrigCode[indexOrig] = p;
                                pHookCode[indexHook] = p;
                                indexOrig += 1;
                                indexHook += 1;
                                lastInstJump = false;
                                break;
                            case 0x83:
                                pOrigCode[indexOrig+0] = p;
                                pOrigCode[indexOrig+1] = pCode[indexOrig+1];
                                pOrigCode[indexOrig+2] = pCode[indexOrig+2];
                                pHookCode[indexHook+0] = p;
                                pHookCode[indexHook+1] = pCode[indexOrig+1];
                                pHookCode[indexHook+2] = pCode[indexOrig+2];
                                indexOrig += 3;
                                indexHook += 3;
                                lastInstJump = false;
                                break;
                            case 0x8b:
                                pOrigCode[indexOrig+0] = p;
                                pOrigCode[indexOrig+1] = pCode[indexOrig+1];
                                pHookCode[indexHook+0] = p;
                                pHookCode[indexHook+1] = pCode[indexOrig+1];
                                indexOrig += 2;
                                indexHook += 2;
                                lastInstJump = false;
                                break;
                            case 0xeb:
                                {
                                    pOrigCode[indexOrig+0] = p;
                                    pOrigCode[indexOrig+1] = pCode[indexOrig+1];
                                    pHookCode[indexHook+0] = 0xe9;
                                    DWORD* pAddr = reinterpret_cast<DWORD*>(&pHookCode[indexHook+1]);
                                    const LPBYTE addr = reinterpret_cast<LPBYTE>(&pCode[indexOrig+2] + pCode[indexOrig+1]);
                                    const LPBYTE pAddrBase = reinterpret_cast<LPBYTE>(&pHookCode[indexHook+sizeof(HookJump)]);
                                    *pAddr = addr - pAddrBase;
                                    indexOrig += 2;
                                    indexHook += sizeof(HookJump);
                                }
                                break;
                            case 0xe9:
                                {
                                    pOrigCode[indexOrig+0] = p;
                                    pOrigCode[indexOrig+1] = pCode[indexOrig+1];
                                    pOrigCode[indexOrig+2] = pCode[indexOrig+2];
                                    pOrigCode[indexOrig+3] = pCode[indexOrig+3];
                                    pOrigCode[indexOrig+4] = pCode[indexOrig+4];

#if defined(_M_IX86)
                                    pHookCode[indexHook+0] = 0xe9;
                                    DWORD* pAddr = reinterpret_cast<DWORD*>(&pHookCode[indexHook+1]);
                                    const LPBYTE addr = reinterpret_cast<LPBYTE>(&pCode[indexOrig+sizeof(HookJump)] + *((DWORD*)&pCode[indexOrig+1]));
                                    const LPBYTE pAddrBase = reinterpret_cast<LPBYTE>(&pHookCode[indexHook+sizeof(HookJump)]);
                                    *pAddr = addr - pAddrBase;
                                    indexOrig += sizeof(HookJump);
                                    indexHook += sizeof(HookJump);
#endif // defined(_M_IX86)
#if defined(_M_X64)
                                    memcpy( &pHookCode[indexHook], &sLongJump, sizeof(sLongJump) );
                                    LongJump* pAddrJump = reinterpret_cast<LongJump*>(&pHookCode[indexHook]);
                                    const LPBYTE addr = reinterpret_cast<LPBYTE>(&pCode[indexOrig+sizeof(HookJump)] + *((DWORD*)&pCode[indexOrig+1]));
                                    pAddrJump->absAddr = reinterpret_cast<DWORD64>(addr);
                                    indexOrig += sizeof(HookJump);
                                    indexHook += sizeof(sLongJump);
#endif // defined(_M_X64)

                                    if ( indexOrig == sCRTStaticFunc[indexOperation+1] )
                                    {
                                        indexPatch = indexOrig - sizeof(HookJump);
                                    }
                                }
                                break;
                            default:
                                assert( false );
                                break;
                            }
                        }
                        for ( size_t index = indexOrig; index < sizeof(pTrampoline->origCode); ++index )
                        {
                            pOrigCode[index] = 0xcc;
                        }

                        if ( lastInstJump )
                        {
                        }
                        else
                        {
#if defined(_M_IX86)
                            pHookCode[indexHook] = sHookJump.opJmp;
                            size_t* pAddr = reinterpret_cast<size_t*>(&pHookCode[indexHook+1]);
                            const LPBYTE addr = reinterpret_cast<LPBYTE>(&pCode[indexOrig+sizeof(HookJump)]);
                            const LPBYTE pAddrBase = reinterpret_cast<LPBYTE>(&pHookCode[indexHook+sizeof(HookJump)]);
                            *pAddr = addr - pAddrBase;
                            indexHook += sizeof(HookJump);
#endif defined(_M_IX86)
#if defined(_M_X64)
                            memcpy( &pHookCode[indexHook], &sLongJump, sizeof(sLongJump) );
                            LongJump* pAddrJump = reinterpret_cast<LongJump*>(&pHookCode[indexHook]);
                            pAddrJump->absAddr = reinterpret_cast<DWORD64>(&pCode[indexOrig]);
                            indexHook += sizeof(sLongJump);
#endif // defined(_M_X64)

                        }
                        for ( size_t index = indexHook; index < sizeof(pTrampoline->hookCode); ++index )
                        {
                            pHookCode[index] = 0xcc;
                        }

#if defined(_M_IX86)
                        {
                            pCode[indexPatch+0] = sHookJump.opJmp;
                            size_t* pAddr = reinterpret_cast<size_t*>(&pCode[indexPatch+1]);
                            const LPBYTE pAddrBase = reinterpret_cast<const LPBYTE>(&pCode[indexPatch+0+sizeof(HookJump)]);
                            LPBYTE addr = NULL;
                            switch ( indexOperation )
                            {
                            case MemoryOperationMismatch::kIndexCRTStaticFuncNew:
                                addr = reinterpret_cast<LPBYTE>(my_new);
                                break;
                            case MemoryOperationMismatch::kIndexCRTStaticFuncNewArray:
                                addr = reinterpret_cast<LPBYTE>(my_new_array);
                                break;
                            case MemoryOperationMismatch::kIndexCRTStaticFuncDelete:
                                addr = reinterpret_cast<LPBYTE>(my_delete);
                                break;
                            case MemoryOperationMismatch::kIndexCRTStaticFuncDeleteArray:
                                addr = reinterpret_cast<LPBYTE>(my_delete_array);
                                break;
                            case MemoryOperationMismatch::kIndexCRTStaticFuncDeleteSize:
                                addr = reinterpret_cast<LPBYTE>(my_delete_size);
                                break;
                            case MemoryOperationMismatch::kIndexCRTStaticFuncDeleteArraySize:
                                addr = reinterpret_cast<LPBYTE>(my_delete_array_size);
                                break;
                            default:
                                assert( false );
                                break;
                            }
                            *pAddr = addr - pAddrBase;
                        }
#endif // defined(_M_IX86)

#if defined(_M_X64)
                        {
                            {
                                pCode[indexPatch+0] = sHookJump.opJmp;
                                DWORD* pAddr = reinterpret_cast<DWORD*>(&pCode[indexPatch+1]);
                                const LPBYTE pAddrBase = reinterpret_cast<const LPBYTE>(&pCode[indexPatch+0+sizeof(HookJump)]);
                                const LPBYTE addr = reinterpret_cast<const LPBYTE>(pTrampoline->longJump);
                                *pAddr = addr - pAddrBase;
                            }

                            {
                                memcpy( pTrampoline->longJump, &sLongJump, sizeof(sLongJump) );
                                LongJump* pAddrJump = reinterpret_cast<LongJump*>(pTrampoline->longJump);

                                LPBYTE addr = NULL;
                                switch ( indexOperation )
                                {
                                case MemoryOperationMismatch::kIndexCRTStaticFuncNew:
                                    addr = reinterpret_cast<LPBYTE>(my_new);
                                    break;
                                case MemoryOperationMismatch::kIndexCRTStaticFuncNewArray:
                                    addr = reinterpret_cast<LPBYTE>(my_new_array);
                                    break;
                                case MemoryOperationMismatch::kIndexCRTStaticFuncDelete:
                                    addr = reinterpret_cast<LPBYTE>(my_delete);
                                    break;
                                case MemoryOperationMismatch::kIndexCRTStaticFuncDeleteArray:
                                    addr = reinterpret_cast<LPBYTE>(my_delete_array);
                                    break;
                                case MemoryOperationMismatch::kIndexCRTStaticFuncDeleteSize:
                                    addr = reinterpret_cast<LPBYTE>(my_delete_size);
                                    break;
                                case MemoryOperationMismatch::kIndexCRTStaticFuncDeleteArraySize:
                                    addr = reinterpret_cast<LPBYTE>(my_delete_array_size);
                                    break;
                                default:
                                    assert( false );
                                    break;
                                }
                                pAddrJump->absAddr = reinterpret_cast<DWORD64>(addr);
                            }
                        }
#endif // defined(_M_X64)

                        {
                            for ( size_t index = sizeof(HookJump); index < indexOrig; ++index )
                            {
                                pCode[index] = 0x90;
                            }
                        }

                        switch ( indexOperation )
                        {
                        case MemoryOperationMismatch::kIndexCRTStaticFuncNew:
                            pfn_new = reinterpret_cast<PFN_new>(pHookCode);
                            break;
                        case MemoryOperationMismatch::kIndexCRTStaticFuncNewArray:
                            pfn_new_array = reinterpret_cast<PFN_new_array>(pHookCode);
                            break;
                        case MemoryOperationMismatch::kIndexCRTStaticFuncDelete:
                            pfn_delete = reinterpret_cast<PFN_delete>(pHookCode);
                            break;
                        case MemoryOperationMismatch::kIndexCRTStaticFuncDeleteArray:
                            pfn_delete_array = reinterpret_cast<PFN_delete_array>(pHookCode);
                            break;
                        case MemoryOperationMismatch::kIndexCRTStaticFuncDeleteSize:
                            pfn_delete_size = reinterpret_cast<PFN_delete_size>(pHookCode);
                            break;
                        case MemoryOperationMismatch::kIndexCRTStaticFuncDeleteArraySize:
                            pfn_delete_array_size = reinterpret_cast<PFN_delete_array_size>(pHookCode);
                            break;
                        }

                        TrampolineHeader* pHeader = &(pTrampoline->uh.header);
                        pHeader->origSize = static_cast<WORD>(sCRTStaticFunc[indexOperation+1]);
                        pHeader->origCodeSize = static_cast<BYTE>(indexOrig);
                        pHeader->hookCodeSize = static_cast<BYTE>(indexHook);

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
    }

    {
        DWORD dwProtect = 0;
        const BOOL BRet = ::VirtualProtect( (LPVOID)sPageTrampoline, (size_t)pageSize, PAGE_EXECUTE_READ, &dwProtect );
    }

    return true;
}

static
bool
unhookCRTCPP( void )
{
    if ( NULL == sModule )
    {
        return false;
    }

    const DWORD64   pageSize = getPageSize();

    assert( sizeof(size_t) == sizeof(void*) );

    for ( size_t indexOperation = 0; indexOperation < MemoryOperationMismatch::kIndexCRTStaticFuncMAX; indexOperation += 2 )
    {
        DWORD64     minOffset = -1;
        DWORD64     maxOffset = 0;

        {
            minOffset = sCRTStaticFunc[indexOperation+0];
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

                    if ( 0 != sCRTStaticFunc[indexOperation+0] )
                    {
                        BYTE* pCode = reinterpret_cast<BYTE*>(p + sCRTStaticFunc[indexOperation+0]);
                        Trampoline* pTrampoline = reinterpret_cast<Trampoline*>((LPBYTE)sPageTrampoline + sizeof(Trampoline)*indexOperation/2);
                        BYTE* pOrigCode = pTrampoline->origCode;
                        BYTE* pHookCode = pTrampoline->hookCode;
                        TrampolineHeader* pHeader = &(pTrampoline->uh.header);

                        for ( size_t index = 0; index < pHeader->origCodeSize; ++index )
                        {
                            pCode[index] = pOrigCode[index];
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
    }


    pfn_new = NULL;
    pfn_delete = NULL;
    pfn_new_array = NULL;
    pfn_delete_array = NULL;
    pfn_delete_size = NULL;
    pfn_delete_array_size = NULL;

    {
        const DWORD dwFreeType = MEM_RELEASE;
        const BOOL BRet = ::VirtualFree( sPageTrampoline, 0, dwFreeType );
        if ( !BRet )
        {
            const DWORD dwErr = ::GetLastError();
        }

        sPageTrampoline = NULL;
    }


    return true;
}



bool
hookMemoryOperationMismatchCRTCPP( const HMODULE hModule, MemoryOperationMismatchClient* pMOM )
{
    bool result = true;
    {
        const bool bRet = pMOM->getCRTStaticFunc( sCRTStaticFunc );
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
        const bool bRet = hookCRTCPP( hModule );
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
unhookMemoryOperationMismatchCRTCPP( void )
{
    bool result = true;
    {
        const bool bRet = unhookCRTCPP();
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

