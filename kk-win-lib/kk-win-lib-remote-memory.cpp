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

#undef UNICODE

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

#include "kk-win-lib-remote-memory.h"


#include <assert.h>

namespace kk
{

RemoteMemory::RemoteMemory()
{
    mHandle = NULL;
    mModule = NULL;
    mMemory = NULL;
}

RemoteMemory::~RemoteMemory()
{
    this->term();
}

bool
RemoteMemory::term(void)
{
    bool result = true;

    this->termMemory();

    mModule = NULL;
    mHandle = NULL;

    return result;
}

bool
RemoteMemory::termMemory(void)
{
    bool result = true;

    if ( NULL == mMemory )
    {
    }
    else
    {
        const BOOL BRet = ::VirtualFree( mMemory, 0, MEM_RELEASE );
        if ( !BRet )
        {
            const DWORD dwErr = ::GetLastError();
            result = false;
        }
        else
        {
            mMemory = NULL;
        }
    }

    return result;
}

bool
RemoteMemory::init( const HANDLE hProcess, const HMODULE hModule )
{
    this->term();

    this->mHandle = hProcess;
    this->mModule = hModule;

    return true;
}


bool
RemoteMemory::readFromRemote(
    const LPVOID moduleBase
    , const size_t moduleSize
)
{

    this->termMemory();

    {
        const DWORD flAllocationType = MEM_COMMIT;
        const DWORD flProtect = PAGE_READWRITE;
        LPVOID pMemory = ::VirtualAlloc( NULL, moduleSize, flAllocationType, flProtect );
        if ( NULL == pMemory )
        {
            const DWORD dwErr = ::GetLastError();
            return false;
        }
        else
        {
#if defined(_DEBUG)
            {
                char temp[128];
                ::wsprintfA( temp, "%p VirtualAlloc\n", pMemory );
                ::OutputDebugStringA( temp );
            }
#endif // defined(_DEBUG)

            this->mMemory = pMemory;
        }
    }

    if ( NULL != this->mMemory )
    {
        SIZE_T nReadedBytes = 0;
        const BOOL BRet = ::ReadProcessMemory(
            this->mHandle
            , moduleBase
            , this->mMemory
            , moduleSize
            , &nReadedBytes
            );
        if ( !BRet )
        {
            const DWORD dwErr = ::GetLastError();
            return false;
        }
    }

    return true;
}





} // namespace kk

