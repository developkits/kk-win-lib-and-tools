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
#include "../kk-win-lib-hook-local.h"
#include "kk-win-lib-checker-memory-operation-mismatch-client.h"


#include <stdlib.h>

#include <assert.h>

namespace kk
{

namespace checker
{

static
DWORD64
sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationMAX];

static
DWORD64
sCRTStaticFunc[MemoryOperationMismatch::kIndexCRTStaticFuncMAX];

static
MemoryOperationMismatchClient*
sMemoryOperationMismatch = NULL;

static
size_t
sHookIndex;

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
bool
hookCRTNewAOP( const HMODULE hModule )
{
    const LPBYTE p = reinterpret_cast<const LPBYTE>(hModule);
    size_t* crtNew = reinterpret_cast<size_t*>(p + sCRTOffsetIAT[MemoryOperationMismatch::kIndexOperationNew]);
    pfn_new = reinterpret_cast<PFN_new>(*crtNew);

    const size_t nOrigOffset = static_cast<const size_t>(sCRTStaticFunc[MemoryOperationMismatch::kIndexCRTStaticFuncNewArray]);
    const size_t nOrigSize   = static_cast<const size_t>(sCRTStaticFunc[MemoryOperationMismatch::kIndexCRTStaticFuncNewArrayLength]);

    const void* hookFunc = my_new_array;
    void**      origFunc = reinterpret_cast<void**>(&pfn_new_array);

    const bool result = sMemoryOperationMismatch->hook( sHookIndex, hModule, nOrigOffset, nOrigSize, hookFunc, origFunc );

    return result;
}

static
bool
unhookCRTNewAOP( void )
{
    if ( NULL == sMemoryOperationMismatch )
    {
        return false;
    }

    bool result = true;
    {
        const bool bRet = sMemoryOperationMismatch->unhook( sHookIndex );

        if ( false == bRet )
        {
            result = false;
        }
        else
        {
            sHookIndex = (size_t)-1;
            pfn_new_array = NULL;
            pfn_new = NULL;
        }
    }

    return result;
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

    return result;
}



} // namespace checker

} // namespace kk

