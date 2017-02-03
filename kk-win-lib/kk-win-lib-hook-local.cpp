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

#include "kk-win-lib-hook-local.h"


#include <assert.h>

namespace kk
{

namespace hookutil
{

static
DWORD
nPageSize;

static
const DWORD
getPageSize( void );

static
void*
trampolinePageAllocate( const void* minAddr, const void* maxAddr, const size_t nSize );

static
bool
trampolinePageDropWriteOperation( void* pTrampolinePage, const DWORD changeSize );

static
bool
trampolinePageGrantWriteOperation( void* pTrampolinePage, const DWORD changeSize );

static
bool
trampolinePageDeallocate( void* pTrampolinePage );

static
bool
trampolinePageHook( void* pTrampolineAddr, const void* moduleBase, const size_t nOrigOffset, const size_t nOrigSize, const void* hookFunc, void** origFunc );

static
bool
trampolinePageUnhook( void* pTrampolineAddr );

#include <pshpack1.h>
struct TrampolineHeader
{
    WORD    origSize;
    BYTE    origCodeSize;
    BYTE    hookCodeSize;

    bool    isUsed;
    BYTE    padding[3];

    LPVOID  origAddr;
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
#if defined(_M_X64)
    BYTE    longJump[0x20];
#endif // defined(_M_X64)
};
#include <poppack.h>



} // namespace hookutil

} // namespace kk


namespace kk
{


HookLocal::HookLocal()
{
    mPageTrampoline = NULL;
    mHookIndexMax = 0;
    mHookIndexCurrent = 0;
}

HookLocal::~HookLocal()
{
    this->term();
}

bool
HookLocal::term(void)
{
    bool result = false;

    if ( NULL == mPageTrampoline )
    {
        result = true;
    }
    else
    {
        result = hookutil::trampolinePageDeallocate( mPageTrampoline );
    }

    mPageTrampoline = NULL;
    mHookIndexMax = 0;
    mHookIndexCurrent = 0;

    return result;
}


bool
HookLocal::init( const void* minAddr, const void* maxAddr, const size_t countHook )
{
    this->term();

    hookutil::nPageSize = hookutil::getPageSize();
    assert( 0 != hookutil::nPageSize );

    size_t requireSize = hookutil::nPageSize;
    if ( 0 < countHook )
    {
        const size_t rawSize = countHook * sizeof(hookutil::Trampoline);
        const size_t requireCount = (rawSize+(hookutil::nPageSize-1))/hookutil::nPageSize;
        requireSize = requireCount * hookutil::nPageSize;
    }

    bool result = true;
    {
        void* pPage = hookutil::trampolinePageAllocate( minAddr, maxAddr, requireSize );
        if ( NULL == pPage )
        {
            result = false;
        }
        else
        {
            this->mPageTrampoline = pPage;
            this->mPageTrampolineSize = requireSize;
            this->mHookIndexCurrent = 0;
            this->mHookIndexMax = requireSize / sizeof(hookutil::Trampoline);
        }
    }

    return result;
}

bool
HookLocal::dropWriteOperation( void )
{
    if ( NULL == this->mPageTrampoline )
    {
        return false;
    }

    const bool result = hookutil::trampolinePageDropWriteOperation( this->mPageTrampoline, static_cast<DWORD>(this->mPageTrampolineSize) );

    return result;
}

bool
HookLocal::grantWriteOperation( void )
{
    if ( NULL == this->mPageTrampoline )
    {
        return false;
    }

    const bool result = hookutil::trampolinePageGrantWriteOperation( this->mPageTrampoline, static_cast<DWORD>(this->mPageTrampolineSize) );

    return result;
}


bool
HookLocal::hook( size_t& indexHook, const void* moduleBase, const size_t nOrigOffset, const size_t nOrigSize, const void* hookFunc, void** origFunc )
{
    if ( NULL == this->mPageTrampoline )
    {
        return false;
    }
    if ( this->mHookIndexMax <= this->mHookIndexCurrent )
    {
        return false;
    }

    bool result = true;
    indexHook = (size_t)-1;
    {
        hookutil::Trampoline* pTrampolineBase = reinterpret_cast<hookutil::Trampoline*>(this->mPageTrampoline);
        hookutil::Trampoline* pTrampoline = &(pTrampolineBase[this->mHookIndexCurrent]);

        const bool bRet = hookutil::trampolinePageHook( pTrampoline, moduleBase, nOrigOffset, nOrigSize, hookFunc, origFunc );
        if ( bRet )
        {
            indexHook = this->mHookIndexCurrent;

            this->mHookIndexCurrent += 1;
        }
        else
        {
            result = false;
        }
    }

    return result;
}

bool
HookLocal::unhook( const size_t indexHook )
{
    if ( NULL == this->mPageTrampoline )
    {
        return false;
    }
    if ( this->mHookIndexMax <= indexHook )
    {
        return false;
    }

    bool result = true;
    {
        hookutil::Trampoline* pTrampolineBase = reinterpret_cast<hookutil::Trampoline*>(this->mPageTrampoline);
        hookutil::Trampoline* pTrampoline = &(pTrampolineBase[indexHook]);

        const bool bRet = hookutil::trampolinePageUnhook( pTrampoline );
        if ( false == bRet )
        {
            result = false;
        }
    }

    return result;
}







namespace hookutil
{

static
void*
getAlignedPage( const void* pAddr, const DWORD pageSize )
{
    LPVOID result = NULL;

    if ( 0 == pageSize )
    {
        assert( 0 != pageSize );
        return NULL;
    }

    assert( sizeof(size_t) == sizeof(void*) );

    const size_t p = reinterpret_cast<size_t>(pAddr);

    const size_t count = p / pageSize;
    const size_t addr = count * pageSize;

    result = reinterpret_cast<void*>(addr);

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

static
bool
trampolinePageDropWriteOperation( void* pTrampolinePage, const DWORD changeSize )
{
    bool result = true;

    const size_t size = static_cast<const size_t>(changeSize);
    {
        DWORD dwProtect = 0;

        const BOOL BRet = ::VirtualProtect( pTrampolinePage, size, PAGE_EXECUTE_READ, &dwProtect );
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

static
bool
trampolinePageGrantWriteOperation( void* pTrampolinePage, const DWORD changeSize )
{
    bool result = true;

    const size_t size = static_cast<const size_t>(changeSize);
    {
        DWORD dwProtect = 0;

        const BOOL BRet = ::VirtualProtect( pTrampolinePage, size, PAGE_EXECUTE_READWRITE, &dwProtect );
        if ( !BRet )
        {
            const DWORD dwErr = ::GetLastError();
            result = false;
        }
    }

    return result;
}

static
void*
trampolinePageAllocate( const void* minAddr, const void* maxAddr, const size_t nSize )
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
                LPVOID p = ::VirtualAlloc( (LPVOID)pAddr, (size_t)nSize, flAllocationType, flProtect );
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
                LPVOID p = ::VirtualAlloc( (LPVOID)pAddr, (size_t)nSize, flAllocationType, flProtect );
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


static
const size_t    pageCountConst = 2;

static
bool
trampolinePageUnhook( void* pTrampolineAddr )
{
    hookutil::Trampoline* pTrampoline = reinterpret_cast<hookutil::Trampoline*>(pTrampolineAddr);

    const size_t pageSize = hookutil::nPageSize;
    {

        const BYTE* pAddrStart = reinterpret_cast<const BYTE*>(pTrampoline->uh.header.origAddr);
        const BYTE* pAddrMin = reinterpret_cast<const BYTE*>( getAlignedPage( pAddrStart, hookutil::nPageSize ) );
        const BYTE* pAddrMax = reinterpret_cast<const BYTE*>( getAlignedPage( pAddrStart+pTrampoline->uh.header.origSize, hookutil::nPageSize ) );

        const DWORD64   minPageCount = reinterpret_cast<const size_t>(pAddrMin) / pageSize;
        const DWORD64   maxPageCount = reinterpret_cast<const size_t>(pAddrMax) / pageSize;

        const size_t    pageCount = static_cast<const size_t>( (maxPageCount - minPageCount + 1) );

        DWORD   aProtect[pageCountConst];
        assert( pageCount <= pageCountConst );
        {
            ZeroMemory( aProtect, sizeof(DWORD) * pageCount );

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
                        aProtect[index] = mbi.Protect;
                    }
                }
            }
            // change protect
            {
                const BYTE* pAddr = pAddrMin;
                for ( size_t index = 0; index < pageCount; ++index, pAddr += pageSize )
                {
                    if ( PAGE_EXECUTE_READWRITE != aProtect[index] )
                    {
                        DWORD   dwProtect;
                        const BOOL BRet = ::VirtualProtect( (LPVOID)pAddr, (size_t)pageSize, PAGE_EXECUTE_READWRITE, &dwProtect );
                        if ( !BRet )
                        {
                            const DWORD dwErr = ::GetLastError();
                        }
                        else
                        {
                            assert( dwProtect == aProtect[index] );
                            aProtect[index] = dwProtect;
                        }
                    }
                }
            }

            {
                assert( sizeof(size_t) == sizeof(void*) );

                hookutil::TrampolineHeader* pHeader = &(pTrampoline->uh.header);
                if ( 0 != pHeader->origSize )
                {
                    BYTE* pCode = reinterpret_cast<BYTE*>(pHeader->origAddr);
                    BYTE* pOrigCode = pTrampoline->origCode;
                    BYTE* pHookCode = pTrampoline->hookCode;

                    for ( size_t index = 0; index < pHeader->origCodeSize; ++index )
                    {
                        pCode[index] = pOrigCode[index];
                    }

                    pHeader->isUsed = false;
                    pHeader->origSize = 0;
                }
            }


            // restore protect
            {
                const BYTE* pAddr = pAddrMin;
                for ( size_t index = 0; index < pageCount; ++index, pAddr += pageSize )
                {
                    if ( PAGE_EXECUTE_READWRITE != aProtect[index] )
                    {
                        DWORD   dwProtect;
                        const BOOL BRet = ::VirtualProtect( (LPVOID)pAddr, (size_t)pageSize, aProtect[index], &dwProtect );
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
    }

    return true;
}









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

static
bool
trampolinePageHook( void* pTrampolineAddr, const void* moduleBase, const size_t nOrigOffset, const size_t nOrigSize, const void* hookFunc, void** origFunc )
{
    {
        if ( NULL == moduleBase )
        {
            return false;
        }
        if ( 0 == nOrigOffset )
        {
            return false;
        }
        if ( 0 == nOrigSize )
        {
            return false;
        }
        if ( NULL == hookFunc )
        {
            return false;
        }
        if ( NULL == origFunc )
        {
            return false;
        }

        {
            bool haveSpace = false;

            if ( sizeof(HookJump) <= nOrigSize )
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

    }


    const DWORD64   minOffset = nOrigOffset;
    const DWORD64   maxOffset = minOffset + sizeof(sHookJump);
    const DWORD     pageSize = hookutil::nPageSize;

    {
        const DWORD64   minPageCount = minOffset / pageSize;
        const DWORD64   maxPageCount = (maxOffset + (pageSize-1)) / pageSize;

        const size_t    pageCountConst = 2;
        const size_t    pageCount = static_cast<const size_t>( (maxPageCount - minPageCount) );

        const BYTE* pAddrModule = reinterpret_cast<const BYTE*>(moduleBase);
        const BYTE* pAddrMin = pAddrModule + pageSize*minPageCount;
        const BYTE* pAddrMax = pAddrModule + pageSize*maxPageCount;

        DWORD   aProtect[pageCountConst];
        assert( pageCount <= pageCountConst );
        {
            ZeroMemory( aProtect, sizeof(DWORD) * pageCount );

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
                        aProtect[index] = mbi.Protect;
                    }
                }
            }
            // change protect
            {
                const BYTE* pAddr = pAddrMin;
                for ( size_t index = 0; index < pageCount; ++index, pAddr += pageSize )
                {
                    if ( PAGE_EXECUTE_READWRITE != aProtect[index] )
                    {
                        DWORD   dwProtect;
                        const BOOL BRet = ::VirtualProtect( (LPVOID)pAddr, (size_t)pageSize, PAGE_EXECUTE_READWRITE, &dwProtect );
                        if ( !BRet )
                        {
                            const DWORD dwErr = ::GetLastError();
                        }
                        else
                        {
                            assert( dwProtect == aProtect[index] );
                            aProtect[index] = dwProtect;
                        }
                    }
                }
            }

            {
                assert( sizeof(size_t) == sizeof(void*) );
                const LPBYTE p = reinterpret_cast<const LPBYTE>((const LPBYTE)moduleBase);

                if ( 0 != nOrigOffset )
                {
                    const LPBYTE pCode = reinterpret_cast<const LPBYTE>(p + nOrigOffset);
                    hookutil::Trampoline* pTrampoline = reinterpret_cast<hookutil::Trampoline*>((LPBYTE)pTrampolineAddr);
                    const LPBYTE pOrigCode = pTrampoline->origCode;
                    const LPBYTE pHookCode = pTrampoline->hookCode;

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
                        case 0x50: // push eax
                        case 0x51: // push ecx
                        case 0x52: // push edx
                        case 0x53: // push rbx
                        case 0x54: // push esp
                        case 0x55: // push ebp
                        case 0x56: // push esi
                        case 0x57: // push edi
                            pOrigCode[indexOrig] = p;
                            pHookCode[indexHook] = p;
                            indexOrig += 1;
                            indexHook += 1;
                            lastInstJump = false;
                            break;
                        case 0x58: // pop eax
                        case 0x59: // pop ecx
                        case 0x5a: // pop edx
                        case 0x5b: // pop ebx
                        case 0x5c: // pop esp
                        case 0x5d: // pop ebp
                        case 0x5e: // pop esi
                        case 0x5f: // pop edi
                            pOrigCode[indexOrig] = p;
                            pHookCode[indexHook] = p;
                            indexOrig += 1;
                            indexHook += 1;
                            lastInstJump = false;
                            break;
                        case 0x6a: // push imm8
                            pOrigCode[indexOrig+0] = p;
                            pOrigCode[indexOrig+1] = pCode[indexOrig+1];
                            pHookCode[indexHook+0] = p;
                            pHookCode[indexHook+1] = pCode[indexOrig+1];
                            indexOrig += 2;
                            indexHook += 2;
                            lastInstJump = false;
                            break;
                        case 0x81:
                            if ( 0xec == pCode[indexOrig+1] )
                            {
                                // sub esp, imm32
                                pOrigCode[indexOrig+0] = p;
                                pOrigCode[indexOrig+1] = pCode[indexOrig+1];
                                pOrigCode[indexOrig+2] = pCode[indexOrig+2];
                                pOrigCode[indexOrig+3] = pCode[indexOrig+3];
                                pOrigCode[indexOrig+4] = pCode[indexOrig+4];
                                pOrigCode[indexOrig+5] = pCode[indexOrig+5];
                                pHookCode[indexHook+0] = p;
                                pHookCode[indexHook+1] = pCode[indexOrig+1];
                                pHookCode[indexHook+2] = pCode[indexOrig+2];
                                pHookCode[indexHook+3] = pCode[indexOrig+3];
                                pHookCode[indexHook+4] = pCode[indexOrig+4];
                                pHookCode[indexHook+5] = pCode[indexOrig+5];
                                indexOrig += 6;
                                indexHook += 6;
                                lastInstJump = false;
                            }
                            else
                            {
                                assert( false );
                            }
                            break;
                        case 0x83:
                            if ( 0xec == pCode[indexOrig+1] )
                            {
                                // sub esp, imm8
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
                        case 0x89:
                            if (
                                (
                                    (0x4c == pCode[indexOrig+1])
                                    || (0x54 == pCode[indexOrig+1])
                                    || (0x44 == pCode[indexOrig+1])
                                )
                                && (0x24 == pCode[indexOrig+2])
                            )
                            {
                                // 89 44 24: mov [rsp+imm8],r8
                                // 89 54 24: mov [rsp+imm8],rdx
                                // 89 4c 24: mov [rsp+imm8],rcx
                                pOrigCode[indexOrig+0] = p;
                                pOrigCode[indexOrig+1] = pCode[indexOrig+1];
                                pOrigCode[indexOrig+2] = pCode[indexOrig+2];
                                pOrigCode[indexOrig+3] = pCode[indexOrig+3];
                                pHookCode[indexHook+0] = p;
                                pHookCode[indexHook+1] = pCode[indexOrig+1];
                                pHookCode[indexHook+2] = pCode[indexOrig+2];
                                pHookCode[indexHook+3] = pCode[indexOrig+3];
                                indexOrig += 4;
                                indexHook += 4;
                                lastInstJump = false;
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
                            if (
                                (0x74 == pCode[indexOrig+1])
                                && (0x24 == pCode[indexOrig+2])
                            )
                            {
                                // mov esi,[esp+imm8]
                                pOrigCode[indexOrig+0] = p;
                                pOrigCode[indexOrig+1] = pCode[indexOrig+1];
                                pOrigCode[indexOrig+2] = pCode[indexOrig+2];
                                pOrigCode[indexOrig+3] = pCode[indexOrig+3];
                                pHookCode[indexHook+0] = p;
                                pHookCode[indexHook+1] = pCode[indexOrig+1];
                                pHookCode[indexHook+2] = pCode[indexOrig+2];
                                pHookCode[indexHook+3] = pCode[indexOrig+3];
                                indexOrig += 4;
                                indexHook += 4;
                                lastInstJump = false;
                            }
                            else
                            if (
                                (0x4c == pCode[indexOrig+1])
                                && (0x24 == pCode[indexOrig+2])
                            )
                            {
                                // 8b 4c 24: mov rcx,[rsp+imm8]
                                pOrigCode[indexOrig+0] = p;
                                pOrigCode[indexOrig+1] = pCode[indexOrig+1];
                                pOrigCode[indexOrig+2] = pCode[indexOrig+2];
                                pOrigCode[indexOrig+3] = pCode[indexOrig+3];
                                pHookCode[indexHook+0] = p;
                                pHookCode[indexHook+1] = pCode[indexOrig+1];
                                pHookCode[indexHook+2] = pCode[indexOrig+2];
                                pHookCode[indexHook+3] = pCode[indexOrig+3];
                                indexOrig += 4;
                                indexHook += 4;
                                lastInstJump = false;
                            }
                            else
                            {
                                assert( false );
                            }
                            break;
                        case 0xeb:
                            {
                                // jmp rel8
                                pOrigCode[indexOrig+0] = p;
                                pOrigCode[indexOrig+1] = pCode[indexOrig+1];
                                pHookCode[indexHook+0] = sHookJump.opJmp;
                                DWORD* pAddr = reinterpret_cast<DWORD*>(&pHookCode[indexHook+1]);
                                const LPBYTE addr = reinterpret_cast<const LPBYTE>(&pCode[indexOrig+2] + *(char*)(&pCode[indexOrig+1]));
                                const LPBYTE pAddrBase = reinterpret_cast<const LPBYTE>(&pHookCode[indexHook+sizeof(HookJump)]);
                                *pAddr = static_cast<DWORD>(addr - pAddrBase);
                                indexOrig += 2;
                                indexHook += sizeof(HookJump);
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
                                pHookCode[indexHook+0] = sHookJump.opJmp;
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

                                if ( indexOrig == nOrigSize )
                                {
                                    indexPatch = indexOrig - sizeof(HookJump);
                                }
                            }
                            break;
                        case 0xcc:
                            // int 3
                            {
                                char temp[128];
                                ::wsprintfA( temp, "hook-local: Detect Breakpoint of Debugger.\n" );
                                ::OutputDebugStringA( temp );
                            }
                            break;
                        case 0xff:
                            if ( 0x75 == pCode[indexOrig+1] )
                            {
                                // push [ebp+imm8]
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
                            if ( 0x15 == pCode[indexOrig+1] )
                            {
                                // call imm32
                                pOrigCode[indexOrig+0] = p;
                                pOrigCode[indexOrig+1] = pCode[indexOrig+1];
                                pOrigCode[indexOrig+2] = pCode[indexOrig+2];
                                pOrigCode[indexOrig+3] = pCode[indexOrig+3];
                                pOrigCode[indexOrig+4] = pCode[indexOrig+4];
                                pOrigCode[indexOrig+5] = pCode[indexOrig+5];
                                pHookCode[indexHook+0] = p;
                                pHookCode[indexHook+1] = pCode[indexOrig+1];
                                pHookCode[indexHook+2] = pCode[indexOrig+2];
                                pHookCode[indexHook+3] = pCode[indexOrig+3];
                                pHookCode[indexHook+4] = pCode[indexOrig+3];
                                pHookCode[indexHook+5] = pCode[indexOrig+4];
                                indexOrig += 6;
                                indexHook += 6;
                                lastInstJump = false;
                            }
                            else
                            {
                                assert( false );
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
                    for ( size_t index = indexHook; index < sizeof(pTrampoline->hookCode); ++index )
                    {
                        pHookCode[index] = 0xcc;
                    }

#if defined(_M_IX86)
                    {
                        pCode[indexPatch+0] = sHookJump.opJmp;
                        DWORD* pAddr = reinterpret_cast<DWORD*>(&pCode[indexPatch+1]);
                        const LPBYTE pAddrBase = reinterpret_cast<const LPBYTE>(&pCode[indexPatch+0+sizeof(HookJump)]);
                        const LPBYTE addr = reinterpret_cast<const LPBYTE>((const LPBYTE)hookFunc);
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

                            const LPBYTE addr = reinterpret_cast<const LPBYTE>((const LPBYTE)hookFunc);
                            pAddrJump->absAddr = reinterpret_cast<DWORD64>(addr);
                        }
                    }
#endif // defined(_M_X64)

                    {
                        for ( size_t index = indexPatch + sizeof(HookJump); index < indexOrig; ++index )
                        {
                            pCode[index] = 0x90;
                        }
                    }

                    *origFunc = pHookCode;

                    hookutil::TrampolineHeader* pHeader = &(pTrampoline->uh.header);
                    pHeader->origSize = static_cast<WORD>(nOrigSize);
                    pHeader->origCodeSize = static_cast<BYTE>(indexOrig);
                    pHeader->hookCodeSize = static_cast<BYTE>(indexHook);
                    pHeader->origAddr = static_cast<void*>(pCode);
                    pHeader->isUsed = true;

                }
            }

            // restore protect
            {
                const BYTE* pAddr = pAddrMin;
                for ( size_t index = 0; index < pageCount; ++index, pAddr += pageSize )
                {
                    if ( PAGE_EXECUTE_READWRITE != aProtect[index] )
                    {
                        DWORD   dwProtect;
                        const BOOL BRet = ::VirtualProtect( (LPVOID)pAddr, (size_t)pageSize, aProtect[index], &dwProtect );
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
    }

    return true;
}


} // namepsace hookutil



} // namespace kk

