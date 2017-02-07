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

#include "kk-win-lib-checker-memory-operation-mismatch-hook-usercpp.h"
#include "kk-win-lib-checker-memory-operation-mismatch-hook-util.h"



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

namespace hookusercpp
{

static
DWORD64
sUserStaticFunc[MemoryOperationMismatch::kIndexUserStaticFuncMAX];

static
MemoryOperationMismatchClient*
sMemoryOperationMismatch = NULL;

static
size_t
sHookIndex[MemoryOperationMismatch::kIndexUserStaticFuncMAX/2];

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

    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationUserStaticNew, (DWORD64)p );

    return p;
}

static
void*
my_new_array( size_t size )
{
    void* p = pfn_new_array(size);

    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationUserStaticNewArray, (DWORD64)p );

    return p;
}

static
void
my_delete( void* p )
{
    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationUserStaticDelete, (DWORD64)p );

    pfn_delete(p);

    return;
}

static
void
my_delete_array( void* p )
{
    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationUserStaticDeleteArray, (DWORD64)p );

    pfn_delete_array(p);

    return;
}

static
void
my_delete_size( void* p, size_t size )
{
    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationUserStaticDeleteSize, (DWORD64)p );

    pfn_delete_size(p,size);

    return;
}

static
void
my_delete_array_size( void* p, size_t size )
{
    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationUserStaticDeleteArraySize, (DWORD64)p );

    pfn_delete_array_size(p,size);

    return;
}


typedef void* (*PFN_new_arg3)(size_t size,const char* fn,size_t line);
static
PFN_new_arg3            pfn_new_arg3 = NULL;

typedef void* (*PFN_new_array_arg3)(size_t size,const char* fn,size_t line);
static
PFN_new_array_arg3      pfn_new_array_arg3 = NULL;

typedef void (*PFN_delete_arg3)(void* p,const char* fn,size_t line);
static
PFN_delete_arg3         pfn_delete_arg3 = NULL;

typedef void (*PFN_delete_array_arg3)(void* p,const char* fn,size_t line);
static
PFN_delete_array_arg3   pfn_delete_array_arg3 = NULL;

typedef void* (*PFN_new_arg4)(size_t size,const char* fn,size_t line,const char* tag);
static
PFN_new_arg4            pfn_new_arg4 = NULL;

typedef void* (*PFN_new_array_arg4)(size_t size,const char* fn,size_t line,const char* tag);
static
PFN_new_array_arg4      pfn_new_array_arg4 = NULL;

typedef void (*PFN_delete_arg4)(void* p,const char* fn,size_t line,const char* tag);
static
PFN_delete_arg4         pfn_delete_arg4 = NULL;

typedef void (*PFN_delete_array_arg4)(void* p,const char* fn,size_t line,const char* tag);
static
PFN_delete_array_arg4   pfn_delete_array_arg4 = NULL;

static
void*
my_new_arg3( size_t size, const char* fn, size_t line )
{
    void* p = pfn_new_arg3(size,fn,line);

    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationUserStaticNew, (DWORD64)p );

    return p;
}

static
void*
my_new_array_arg3( size_t size, const char* fn, size_t line )
{
    void* p = pfn_new_array_arg3(size,fn,line);

    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationUserStaticNewArray, (DWORD64)p );

    return p;
}

static
void
my_delete_arg3( void* p, const char* fn, size_t line )
{
    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationUserStaticDelete, (DWORD64)p );

    pfn_delete_arg3(p,fn,line);

    return;
}

static
void
my_delete_array_arg3( void* p, const char* fn, size_t line )
{
    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationUserStaticDeleteArray, (DWORD64)p );

    pfn_delete_array_arg3(p,fn,line);

    return;
}

static
void*
my_new_arg4( size_t size, const char* fn, size_t line, const char* tag )
{
    void* p = pfn_new_arg4(size,fn,line,tag);

    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationUserStaticNew, (DWORD64)p );

    return p;
}

static
void*
my_new_array_arg4( size_t size, const char* fn, size_t line, const char* tag )
{
    void* p = pfn_new_array_arg4(size,fn,line,tag);

    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationUserStaticNewArray, (DWORD64)p );

    return p;
}

static
void
my_delete_arg4( void* p, const char* fn, size_t line, const char* tag )
{
    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationUserStaticDelete, (DWORD64)p );

    pfn_delete_arg4(p,fn,line,tag);

    return;
}

static
void
my_delete_array_arg4( void* p, const char* fn, size_t line, const char* tag )
{
    sMemoryOperationMismatch->sendOperation( MemoryOperationMismatch::kOperationUserStaticDeleteArray, (DWORD64)p );

    pfn_delete_array_arg4(p,fn,line,tag);

    return;
}






static
bool
hookUserCPP( const HMODULE hModule )
{
    bool result = true;

    for ( size_t indexOperation = 0; indexOperation < MemoryOperationMismatch::kIndexUserStaticFuncMAX; indexOperation += 2 )
    {
        const size_t nOrigOffset = static_cast<const size_t>(sUserStaticFunc[indexOperation+0]);
        const size_t nOrigSize   = static_cast<const size_t>(sUserStaticFunc[indexOperation+1]);

        const void* hookFunc = NULL;
        void**      origFunc = NULL;

        {
            switch ( indexOperation )
            {
            case MemoryOperationMismatch::kIndexUserStaticFuncNew:
                hookFunc = reinterpret_cast<const void*>(my_new);
                break;
            case MemoryOperationMismatch::kIndexUserStaticFuncNewArray:
                hookFunc = reinterpret_cast<const void*>(my_new_array);
                break;
            case MemoryOperationMismatch::kIndexUserStaticFuncDelete:
                hookFunc = reinterpret_cast<const void*>(my_delete);
                break;
            case MemoryOperationMismatch::kIndexUserStaticFuncDeleteArray:
                hookFunc = reinterpret_cast<const void*>(my_delete_array);
                break;
            case MemoryOperationMismatch::kIndexUserStaticFuncDeleteSize:
                hookFunc = reinterpret_cast<const void*>(my_delete_size);
                break;
            case MemoryOperationMismatch::kIndexUserStaticFuncDeleteArraySize:
                hookFunc = reinterpret_cast<const void*>(my_delete_array_size);
                break;
            case MemoryOperationMismatch::kIndexUserStaticFuncNewArg3:
                hookFunc = reinterpret_cast<const void*>(my_new_arg3);
                break;
            case MemoryOperationMismatch::kIndexUserStaticFuncNewArrayArg3:
                hookFunc = reinterpret_cast<const void*>(my_new_array_arg3);
                break;
            case MemoryOperationMismatch::kIndexUserStaticFuncDeleteArg3:
                hookFunc = reinterpret_cast<const void*>(my_delete_arg3);
                break;
            case MemoryOperationMismatch::kIndexUserStaticFuncDeleteArrayArg3:
                hookFunc = reinterpret_cast<const void*>(my_delete_array_arg3);
                break;
            case MemoryOperationMismatch::kIndexUserStaticFuncNewArg4:
                hookFunc = reinterpret_cast<const void*>(my_new_arg4);
                break;
            case MemoryOperationMismatch::kIndexUserStaticFuncNewArrayArg4:
                hookFunc = reinterpret_cast<const void*>(my_new_array_arg4);
                break;
            case MemoryOperationMismatch::kIndexUserStaticFuncDeleteArg4:
                hookFunc = reinterpret_cast<const void*>(my_delete_arg4);
                break;
            case MemoryOperationMismatch::kIndexUserStaticFuncDeleteArrayArg4:
                hookFunc = reinterpret_cast<const void*>(my_delete_array_arg4);
                break;
            default:
                assert( false );
                break;
            }
        }

        {
            switch ( indexOperation )
            {
            case MemoryOperationMismatch::kIndexUserStaticFuncNew:
                origFunc = reinterpret_cast<void**>(&pfn_new);
                break;
            case MemoryOperationMismatch::kIndexUserStaticFuncNewArray:
                origFunc = reinterpret_cast<void**>(&pfn_new_array);
                break;
            case MemoryOperationMismatch::kIndexUserStaticFuncDelete:
                origFunc = reinterpret_cast<void**>(&pfn_delete);
                break;
            case MemoryOperationMismatch::kIndexUserStaticFuncDeleteArray:
                origFunc = reinterpret_cast<void**>(&pfn_delete_array);
                break;
            case MemoryOperationMismatch::kIndexUserStaticFuncDeleteSize:
                origFunc = reinterpret_cast<void**>(&pfn_delete_size);
                break;
            case MemoryOperationMismatch::kIndexUserStaticFuncDeleteArraySize:
                origFunc = reinterpret_cast<void**>(&pfn_delete_array_size);
                break;
            case MemoryOperationMismatch::kIndexUserStaticFuncNewArg3:
                origFunc = reinterpret_cast<void**>(&pfn_new_arg3);
                break;
            case MemoryOperationMismatch::kIndexUserStaticFuncNewArrayArg3:
                origFunc = reinterpret_cast<void**>(&pfn_new_array_arg3);
                break;
            case MemoryOperationMismatch::kIndexUserStaticFuncDeleteArg3:
                origFunc = reinterpret_cast<void**>(&pfn_delete_arg3);
                break;
            case MemoryOperationMismatch::kIndexUserStaticFuncDeleteArrayArg3:
                origFunc = reinterpret_cast<void**>(&pfn_delete_array_arg3);
                break;
            case MemoryOperationMismatch::kIndexUserStaticFuncNewArg4:
                origFunc = reinterpret_cast<void**>(&pfn_new_arg4);
                break;
            case MemoryOperationMismatch::kIndexUserStaticFuncNewArrayArg4:
                origFunc = reinterpret_cast<void**>(&pfn_new_array_arg4);
                break;
            case MemoryOperationMismatch::kIndexUserStaticFuncDeleteArg4:
                origFunc = reinterpret_cast<void**>(&pfn_delete_arg4);
                break;
            case MemoryOperationMismatch::kIndexUserStaticFuncDeleteArrayArg4:
                origFunc = reinterpret_cast<void**>(&pfn_delete_array_arg4);
                break;
            default:
                assert( false );
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
unhookUserCPP( void )
{
    if ( NULL == sMemoryOperationMismatch )
    {
        return false;
    }

    for ( size_t indexOperation = 0; indexOperation < MemoryOperationMismatch::kIndexUserStaticFuncMAX; indexOperation += 2 )
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
    pfn_new_arg3 = NULL;
    pfn_delete_arg3 = NULL;
    pfn_new_array_arg3 = NULL;
    pfn_delete_array_arg3 = NULL;
    pfn_new_arg4 = NULL;
    pfn_delete_arg4 = NULL;
    pfn_new_array_arg4 = NULL;
    pfn_delete_array_arg4 = NULL;

    return true;
}



} // namespace hookusercpp




bool
hookMemoryOperationMismatchUserCPP( const HMODULE hModule, MemoryOperationMismatchClient* pMOM )
{
    bool result = true;
    {
        const bool bRet = pMOM->getUserStaticFunc( hookusercpp::sUserStaticFunc );
        if ( !bRet )
        {
            result = false;
        }
        else
        {
            hookusercpp::sMemoryOperationMismatch = pMOM;
        }
    }

    if ( result )
    {
        const bool bRet = hookusercpp::hookUserCPP( hModule );
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
unhookMemoryOperationMismatchUserCPP( void )
{
    bool result = true;
    {
        const bool bRet = hookusercpp::unhookUserCPP();
        if ( !bRet )
        {
            result = false;
        }
    }

    hookusercpp::sMemoryOperationMismatch = NULL;

    return result;
}



} // namespace checker

} // namespace kk

