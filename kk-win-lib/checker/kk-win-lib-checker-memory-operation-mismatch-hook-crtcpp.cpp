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
#include "../kk-win-lib-hook-local.h"
#include "kk-win-lib-checker-memory-operation-mismatch-client.h"


#include <stdlib.h>

#include <assert.h>

namespace kk
{

namespace checker
{

namespace hookcrtcpp
{

static
DWORD64
sCRTStaticFunc[MemoryOperationMismatch::kIndexCRTStaticFuncMAX];

static
MemoryOperationMismatchClient*
sMemoryOperationMismatch = NULL;

static
size_t
sHookIndex[MemoryOperationMismatch::kIndexCRTStaticFuncMAX/2];



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

    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationCRTStaticNew, (DWORD64)p );

    return p;
}

static
void*
my_new_array( size_t size )
{
    void* p = pfn_new_array(size);

    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationCRTStaticNewArray, (DWORD64)p );

    return p;
}

static
void
my_delete( void* p )
{
    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationCRTStaticDelete, (DWORD64)p );

    pfn_delete(p);

    return;
}

static
void
my_delete_array( void* p )
{
    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationCRTStaticDeleteArray, (DWORD64)p );

    pfn_delete_array(p);

    return;
}

static
void
my_delete_size( void* p, size_t size )
{
    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationCRTStaticDeleteSize, (DWORD64)p );

    pfn_delete_size(p,size);

    return;
}

static
void
my_delete_array_size( void* p, size_t size )
{
    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationCRTStaticDeleteArraySize, (DWORD64)p );

    pfn_delete_array_size(p,size);

    return;
}





static
bool
hookCRTCPP( const HMODULE hModule )
{
    if ( NULL == hModule )
    {
        return false;
    }

    bool result = true;

    for ( size_t indexOperation = 0; indexOperation < MemoryOperationMismatch::kIndexCRTStaticFuncMAX; indexOperation += 2 )
    {
        const size_t nOrigOffset = static_cast<const size_t>(sCRTStaticFunc[indexOperation+0]);
        const size_t nOrigSize   = static_cast<const size_t>(sCRTStaticFunc[indexOperation+1]);

        const void* hookFunc = NULL;
        void**      origFunc = NULL;

        {
            switch ( indexOperation )
            {
            case MemoryOperationMismatch::kIndexCRTStaticFuncNew:
                hookFunc = reinterpret_cast<const void*>(my_new);
                break;
            case MemoryOperationMismatch::kIndexCRTStaticFuncNewArray:
                hookFunc = reinterpret_cast<const void*>(my_new_array);
                break;
            case MemoryOperationMismatch::kIndexCRTStaticFuncDelete:
                hookFunc = reinterpret_cast<const void*>(my_delete);
                break;
            case MemoryOperationMismatch::kIndexCRTStaticFuncDeleteArray:
                hookFunc = reinterpret_cast<const void*>(my_delete_array);
                break;
            case MemoryOperationMismatch::kIndexCRTStaticFuncDeleteSize:
                hookFunc = reinterpret_cast<const void*>(my_delete_size);
                break;
            case MemoryOperationMismatch::kIndexCRTStaticFuncDeleteArraySize:
                hookFunc = reinterpret_cast<const void*>(my_delete_array_size);
                break;
            default:
                assert( false );
                break;
            }
        }

        {
            switch ( indexOperation )
            {
            case MemoryOperationMismatch::kIndexCRTStaticFuncNew:
                origFunc = reinterpret_cast<void**>(&pfn_new);
                break;
            case MemoryOperationMismatch::kIndexCRTStaticFuncNewArray:
                origFunc = reinterpret_cast<void**>(&pfn_new_array);
                break;
            case MemoryOperationMismatch::kIndexCRTStaticFuncDelete:
                origFunc = reinterpret_cast<void**>(&pfn_delete);
                break;
            case MemoryOperationMismatch::kIndexCRTStaticFuncDeleteArray:
                origFunc = reinterpret_cast<void**>(&pfn_delete_array);
                break;
            case MemoryOperationMismatch::kIndexCRTStaticFuncDeleteSize:
                origFunc = reinterpret_cast<void**>(&pfn_delete_size);
                break;
            case MemoryOperationMismatch::kIndexCRTStaticFuncDeleteArraySize:
                origFunc = reinterpret_cast<void**>(&pfn_delete_array_size);
                break;
            }
        }

        const bool bRet = sMemoryOperationMismatch->hook( sHookIndex[indexOperation/2], hModule, nOrigOffset, nOrigSize, hookFunc, origFunc );
        if ( false == bRet )
        {
            result = false;
        }

    }

    return result;
}

static
bool
unhookCRTCPP( void )
{
    if ( NULL == sMemoryOperationMismatch )
    {
        return false;
    }


    for ( size_t indexOperation = 0; indexOperation < MemoryOperationMismatch::kIndexCRTStaticFuncMAX; indexOperation += 2 )
    {
        const size_t indexHook = sHookIndex[indexOperation/2];

        const bool bRet = sMemoryOperationMismatch->unhook( indexHook );
    }



    pfn_new = NULL;
    pfn_delete = NULL;
    pfn_new_array = NULL;
    pfn_delete_array = NULL;
    pfn_delete_size = NULL;
    pfn_delete_array_size = NULL;

    return true;
}


} // namespace hookcrtcpp




bool
hookMemoryOperationMismatchCRTCPP( const HMODULE hModule, MemoryOperationMismatchClient* pMOM )
{
    bool result = true;
    {
        const bool bRet = pMOM->getCRTStaticFunc( hookcrtcpp::sCRTStaticFunc );
        if ( !bRet )
        {
            result = false;
        }
        else
        {
            hookcrtcpp::sMemoryOperationMismatch = pMOM;
        }
    }

    if ( result )
    {
        const bool bRet = hookcrtcpp::hookCRTCPP( hModule );
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
unhookMemoryOperationMismatchCRTCPP( void )
{
    bool result = true;
    {
        const bool bRet = hookcrtcpp::unhookCRTCPP();
        if ( !bRet )
        {
            result = false;
        }
    }

    hookcrtcpp::sMemoryOperationMismatch = NULL;

    return result;
}



} // namespace checker

} // namespace kk

