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

#include "kk-win-lib-injection-dll.h"


#include <assert.h>

namespace kk
{

InjectionDLL::InjectionDLL()
{
    mNeedCloseHandle = false;
    mHandle = NULL;
}

InjectionDLL::~InjectionDLL()
{
    this->term();
}

bool
InjectionDLL::term(void)
{
    bool result = false;

    if ( NULL == mHandle )
    {
        result = true;
    }
    else
    {
        if ( mNeedCloseHandle )
        {
            const BOOL BRet = ::CloseHandle( mHandle );
            if ( !BRet )
            {
                const DWORD dwErr = ::GetLastError();
                assert(false);
                result = false;
            }
            else
            {
                result = true;
                mHandle = NULL;
            }
        }
        else
        {
            mHandle = NULL;
        }
    }

    return result;
}


bool
InjectionDLL::init(const size_t processId)
{
    this->term();

    {
        DWORD dwDesiredAccess = 0;
        const BOOL bInheritHandle = FALSE;
        const DWORD dwProcessId = static_cast<DWORD>(processId);

        dwDesiredAccess |= PROCESS_CREATE_THREAD;
        dwDesiredAccess |= PROCESS_VM_OPERATION;
        dwDesiredAccess |= PROCESS_VM_READ;
        dwDesiredAccess |= PROCESS_VM_WRITE;
        dwDesiredAccess |= PROCESS_QUERY_INFORMATION;
        {
            const HANDLE handle = ::OpenProcess( dwDesiredAccess, bInheritHandle, dwProcessId );
            if ( NULL == handle )
            {
                const DWORD dwErr = ::GetLastError();
                assert( false );
                return false;
            }

            this->mNeedCloseHandle = true;
            this->mHandle = handle;
            //this->mProcessId = dwProcessId;
        }
    }

    return true;
}


bool
InjectionDLL::init( const HANDLE hProcess )
{
    this->term();

    if ( NULL == hProcess )
    {
        return false;
    }

    mNeedCloseHandle = false;
    mHandle = hProcess;

    return true;
}



bool
InjectionDLL::injection( const char* szPath )
{
    if ( NULL == szPath )
    {
        return false;
    }

    {
        const DWORD dwRet = ::GetFileAttributesA( szPath );
        if ( INVALID_FILE_ATTRIBUTES == dwRet )
        {
            return false;
        }
    }

    bool result = true;

    const size_t nPathLen = ::lstrlenA(szPath) + 1;

    LPVOID pRemoteAddr = NULL;
    {
        const DWORD flAllocationType = MEM_RESERVE | MEM_COMMIT;
        const DWORD flProtect = PAGE_EXECUTE_READWRITE;
        pRemoteAddr = ::VirtualAllocEx( mHandle, NULL, nPathLen, flAllocationType, flProtect );
        if ( NULL == pRemoteAddr )
        {
            result = false;
        }
    }

    if ( NULL != pRemoteAddr )
    {
        SIZE_T dwBytes = 0;
        const BOOL BRet = ::WriteProcessMemory( mHandle, pRemoteAddr, szPath, nPathLen, &dwBytes );
        if ( !BRet )
        {
            result = false;
        }

        if ( nPathLen != dwBytes )
        {
            result = false;
        }
    }

    // assume equal as my process's address of kernel32!LoadLibraryA and remote process's address of kernel32!LoadLibraryA
    //  will break feature windows
    const LPVOID pLoadLibraryAddr = ::GetProcAddress( ::GetModuleHandleA("kernel32.dll"), "LoadLibraryA" );
    if ( NULL != pLoadLibraryAddr )
    {
        LPTHREAD_START_ROUTINE pThread = reinterpret_cast<LPTHREAD_START_ROUTINE>(pLoadLibraryAddr);
        HANDLE hThread = ::CreateRemoteThread( mHandle, NULL, 0, pThread, pRemoteAddr, 0, NULL );
        if ( NULL == hThread )
        {
            result = false;
        }
        else
        {
            ::WaitForSingleObject( hThread, INFINITE );

            ::CloseHandle( hThread );
            hThread = NULL;
        }
    }

    if ( NULL != pRemoteAddr )
    {
        const BOOL BRet = ::VirtualFreeEx( mHandle, pRemoteAddr, 0, MEM_RELEASE );
        if ( !BRet )
        {
            return false;
        }
    }

    return result;
}





} // namespace kk

