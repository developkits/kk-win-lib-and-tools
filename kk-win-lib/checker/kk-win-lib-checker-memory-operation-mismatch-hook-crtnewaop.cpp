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
#include "kk-win-lib-checker-memory-operation-mismatch-hook-util.h"



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


static
void*
my_new_array( size_t size )
{
    //void* p = pfn_new_array(size);
    void* p = pfn_new(size);

    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationNewArray, (DWORD64)p );

    return p;
}



static
LPVOID      sPageTrampoline  = NULL;


#if defined(_M_IX86) || defined(_M_X64)

#include <pshpack1.h>
struct HookJump {
    BYTE        opJmp;
    DWORD       relAddr;
};
#include <poppack.h>

static
const
HookJump    sHookJump = { 0xe9, 0 };

#endif // defined(_M_IX86)

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



static
bool
hookCRTNewAOP( const HMODULE hModule )
{
    if ( NULL == hModule )
    {
        return false;
    }

    const DWORD64   pageSize = hookutil::getPageSize();

    sPageTrampoline = hookutil::trampolinePageAllocate( hModule, hModule );

    if ( NULL == sPageTrampoline )
    {
        return false;
    }

    DWORD64     minOffset = -1;
    DWORD64     maxOffset = 0;

    {
        minOffset = sCRTStaticFunc[MemoryOperationMismatch::kIndexCRTStaticFuncNewArray];
        maxOffset = minOffset + sizeof(sHookJump);
    }

    {
        bool haveSpace = false;

        const DWORD64 funcSize = sCRTStaticFunc[MemoryOperationMismatch::kIndexCRTStaticFuncNewArrayLength];
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

                if ( 0 != sCRTStaticFunc[MemoryOperationMismatch::kIndexCRTStaticFuncNewArray] )
                {
                    BYTE* pCode = reinterpret_cast<BYTE*>(p + sCRTStaticFunc[MemoryOperationMismatch::kIndexCRTStaticFuncNewArray]);
                    hookutil::Trampoline* pTrampoline = reinterpret_cast<hookutil::Trampoline*>((LPBYTE)sPageTrampoline + sizeof(hookutil::Trampoline)*0);
                    BYTE* pOrigCode = pTrampoline->origCode;
                    BYTE* pHookCode = pTrampoline->hookCode;

                    size_t indexPatch = 0;
                    size_t indexOrig = 0;
                    size_t indexHook = 0;
                    bool    lastInstJump = true;
                    for ( indexOrig = 0; indexOrig < sizeof(HookJump); )
                    {
                        const BYTE p = pCode[indexOrig];
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
                        case 0x51: // push ecx
                        case 0x55: // push ebp
                            pOrigCode[indexOrig] = p;
                            pHookCode[indexHook] = p;
                            indexOrig += 1;
                            indexHook += 1;
                            lastInstJump = false;
                            break;
                        case 0x5d: // pop ebp
                            pOrigCode[indexOrig] = p;
                            pHookCode[indexHook] = p;
                            indexOrig += 1;
                            indexHook += 1;
                            lastInstJump = false;
                            break;
                        case 0x89:
                            if (
                                (0x89 == pCode[indexOrig+1])
                                || (0x8b == pCode[indexOrig+1])
                            )
                            {
                                if (
                                    (0x4c == pCode[indexOrig+2])
                                    && (0x24 == pCode[indexOrig+3])
                                )
                                {
                                    // 89 4c 24: mov [rsp+imm8],rcx
                                    // 8b 4c 24: mov rcx,[rsp+imm8]
                                    pOrigCode[indexOrig+0] = p;
                                    pOrigCode[indexOrig+1] = pCode[indexOrig+1];
                                    pOrigCode[indexOrig+2] = pCode[indexOrig+2];
                                    pOrigCode[indexOrig+3] = pCode[indexOrig+3];
                                    pOrigCode[indexOrig+4] = pCode[indexOrig+4];
                                    pHookCode[indexHook+0] = p;
                                    pHookCode[indexHook+1] = pCode[indexOrig+1];
                                    pHookCode[indexHook+2] = pCode[indexOrig+2];
                                    pHookCode[indexHook+3] = pCode[indexOrig+3];
                                    pHookCode[indexHook+4] = pCode[indexOrig+4];
                                    indexOrig += 5;
                                    indexHook += 5;
                                    lastInstJump = false;
                                }
                                else
                                {
                                    assert( false );
                                }
                            }
                            else
                            {
                                assert( false );
                            }
                            break;
                        case 0x8b:
                            if (
                                (0xff == pCode[indexOrig+1]) // mov edi,edi
                                || (0xec == pCode[indexOrig+1]) // mov ebp, esp
                            )
                            {
                                pOrigCode[indexOrig+0] = p;
                                pOrigCode[indexOrig+1] = pCode[indexOrig+1];
                                pHookCode[indexHook+0] = p;
                                pHookCode[indexHook+1] = pCode[indexOrig+1];
                                indexOrig += 2;
                                indexHook += 2;
                                lastInstJump = false;
                            }
                            else
                            if (
                                (0x45 == pCode[indexOrig+1]) // mov eax,[ebp+imm8]
                            )
                            {
                                pOrigCode[indexOrig+0] = p;
                                pOrigCode[indexOrig+1] = pCode[indexOrig+1];
                                pOrigCode[indexOrig+2] = pCode[indexOrig+2];
                                pHookCode[indexHook+0] = p;
                                pHookCode[indexHook+1] = pCode[indexOrig+1];
                                pHookCode[indexHook+2] = pCode[indexOrig+2];
                                indexOrig += 3;
                                indexHook += 3;
                                lastInstJump = false;
                            }
                            else
                            {
                                assert( false );
                            }
                            break;

                        case 0xe9:
                            {
                                // jmp rel32
                                pOrigCode[indexOrig+0] = p;
                                pOrigCode[indexOrig+1] = pCode[indexOrig+1];
                                pOrigCode[indexOrig+2] = pCode[indexOrig+2];
                                pOrigCode[indexOrig+3] = pCode[indexOrig+3];
                                pOrigCode[indexOrig+4] = pCode[indexOrig+4];

#if defined(_M_IX86)
                                pHookCode[indexHook+0] = 0xe9;
                                DWORD* pAddr = reinterpret_cast<DWORD*>(&pHookCode[indexHook+1]);
                                const LPBYTE addr = reinterpret_cast<LPBYTE>(&pCode[indexOrig+sizeof(HookJump)] + *((LONG*)&pCode[indexOrig+1]));
                                const LPBYTE pAddrBase = reinterpret_cast<LPBYTE>(&pHookCode[indexHook+sizeof(HookJump)]);
                                *pAddr = addr - pAddrBase;
                                indexOrig += sizeof(HookJump);
                                indexHook += sizeof(HookJump);
#endif // defined(_M_IX86)
#if defined(_M_X64)
                                memcpy( &pHookCode[indexHook], &sLongJump, sizeof(sLongJump) );
                                LongJump* pAddrJump = reinterpret_cast<LongJump*>(&pHookCode[indexHook]);
                                const LPBYTE addr = reinterpret_cast<LPBYTE>(&pCode[indexOrig+sizeof(HookJump)] + *((LONG*)&pCode[indexOrig+1]));
                                pAddrJump->absAddr = reinterpret_cast<DWORD64>(addr);
                                indexOrig += sizeof(HookJump);
                                indexHook += sizeof(sLongJump);
#endif // defined(_M_X64)

                                if ( indexOrig == sCRTStaticFunc[MemoryOperationMismatch::kIndexCRTStaticFuncNewArrayLength] )
                                {
                                    indexPatch = indexOrig - sizeof(HookJump);
                                }
                            }
                            break;
                        case 0xcc:
                            // int 3
                            {
                                char temp[128];
                                ::wsprintfA( temp, "hook-crtnewaop: Detect Breakpoint of Debugger.\n" );
                                ::OutputDebugStringA( temp );
                            }
                            break;
                        default:
                            assert( false );
                            break;
                        } // switch
                    }

                    {
                        if ( lastInstJump )
                        {
                        }
                        else
                        {
#if defined(_M_IX86)
                            pHookCode[indexHook] = sHookJump.opJmp;
                            DWORD* pAddr = reinterpret_cast<DWORD*>(&pHookCode[indexHook+1]);
                            const LPBYTE addr = reinterpret_cast<LPBYTE>(&pCode[indexPatch+0+sizeof(HookJump)]);
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
                    }


                    {
#if defined(_M_IX86)
                        {
                            pCode[indexPatch+0] = sHookJump.opJmp;
                            DWORD* pAddr = reinterpret_cast<DWORD*>(&pCode[indexPatch+1]);
                            const LPBYTE pAddrBase = reinterpret_cast<const LPBYTE>(&pCode[indexPatch+0+sizeof(HookJump)]);
                            LPBYTE addr = reinterpret_cast<LPBYTE>(my_new_array);
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
                                *pAddr = static_cast<DWORD>(addr - pAddrBase);
                            }

                            {
                                memcpy( pTrampoline->longJump, &sLongJump, sizeof(sLongJump) );
                                LongJump* pAddrJump = reinterpret_cast<LongJump*>(pTrampoline->longJump);

                                LPBYTE addr = reinterpret_cast<LPBYTE>(my_new_array);
                                pAddrJump->absAddr = reinterpret_cast<DWORD64>(addr);
                            }
                        }
#endif // defined(_M_X64)
                    }

                    {
                        for ( size_t index = indexPatch + sizeof(HookJump); index < indexOrig; ++index )
                        {
                            pCode[index] = 0x90;
                        }
                    }

                    assert( 0 != sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationNew] );
                    size_t* crtNew = reinterpret_cast<size_t*>(p + sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationNew]);
                    pfn_new = reinterpret_cast<PFN_new>(*crtNew);
                    pfn_new_array = reinterpret_cast<PFN_new_array>(pHookCode);

                    hookutil::TrampolineHeader* pHeader = &(pTrampoline->uh.header);
                    pHeader->origSize = static_cast<WORD>(sCRTStaticFunc[MemoryOperationMismatch::kIndexCRTStaticFuncNewArrayLength]);
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

    hookutil::tranpolinePageDropWriteOperation( sPageTrampoline, (const DWORD)pageSize );

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

    const DWORD64   pageSize = hookutil::getPageSize();

    DWORD64     minOffset = -1;
    DWORD64     maxOffset = 0;

    {
        minOffset = sCRTStaticFunc[MemoryOperationMismatch::kIndexCRTStaticFuncNewArray];
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

                if ( 0 != sCRTStaticFunc[MemoryOperationMismatch::kIndexCRTStaticFuncNewArray] )
                {
                    BYTE* pCode = reinterpret_cast<BYTE*>(p + sCRTStaticFunc[MemoryOperationMismatch::kIndexCRTStaticFuncNewArray]);
                    hookutil::Trampoline* pTrampoline = reinterpret_cast<hookutil::Trampoline*>((LPBYTE)sPageTrampoline + sizeof(hookutil::Trampoline)*0/2);
                    BYTE* pOrigCode = pTrampoline->origCode;
                    BYTE* pHookCode = pTrampoline->hookCode;
                    hookutil::TrampolineHeader* pHeader = &(pTrampoline->uh.header);

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


    pfn_new_array = NULL;

    hookutil::trampolinePageDeallocate( sPageTrampoline );
    sPageTrampoline = NULL;

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

