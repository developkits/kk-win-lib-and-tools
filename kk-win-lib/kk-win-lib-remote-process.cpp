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

#include "kk-win-lib-remote-process.h"


#include <assert.h>

namespace kk
{

RemoteProcess::RemoteProcess()
{
    mProcessId = 0;
    mHandle = NULL;
    mModule = NULL;
    mModuleBase = NULL;
    mModuleSize = 0;
}

RemoteProcess::~RemoteProcess()
{
    this->term();
}

bool
RemoteProcess::term(void)
{
    bool result = false;

    mModuleSize = 0;
    mModuleBase = NULL;
    mModule = NULL;

    if ( NULL == mHandle )
    {
        result = true;
    }
    else
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
    mProcessId = 0;

    return result;
}


bool
RemoteProcess::init(const size_t processId)
{
    this->term();

    {
        DWORD dwDesiredAccess = 0;
        const BOOL bInheritHandle = FALSE;
        const DWORD dwProcessId = static_cast<DWORD>(processId);

        dwDesiredAccess |= PROCESS_QUERY_INFORMATION;
        dwDesiredAccess |= PROCESS_VM_READ;
        {
            const HANDLE handle = ::OpenProcess( dwDesiredAccess, bInheritHandle, dwProcessId );
            if ( NULL == handle )
            {
                const DWORD dwErr = ::GetLastError();
                assert( false );
                return false;
            }

            this->mHandle = handle;
            this->mProcessId = dwProcessId;
        }
    }

    {
        const DWORD dwFlags = TH32CS_SNAPMODULE;
        const DWORD dwProcessId = static_cast<DWORD>(processId);
        const HANDLE hSnapshot = ::CreateToolhelp32Snapshot( dwFlags, dwProcessId );

        if ( INVALID_HANDLE_VALUE == hSnapshot )
        {
            const DWORD dwErr = ::GetLastError();
            if ( ERROR_PARTIAL_COPY == dwErr )
            {
                // 32bit App, 64bit target NG
            }
        }
        else
        {
            MODULEENTRY32   moduleEntry;
            ZeroMemory( &moduleEntry, sizeof(moduleEntry) );
            moduleEntry.dwSize = sizeof(moduleEntry);

            const BOOL BRet = ::Module32First( hSnapshot, &moduleEntry );
            if ( !BRet )
            {
                const DWORD dwErr = ::GetLastError();
                if ( ERROR_NO_MORE_FILES == dwErr )
                {
                }
                else
                {
                    assert( false );
                }
            }
            else
            {
                BOOL BRet = FALSE;
                do
                {
#if 0//defined(_DEBUG)
                    {
                        char temp[128];
                        ::wsprintfA( temp, "%p %s\n", moduleEntry.hModule, moduleEntry.szModule );
                        ::OutputDebugStringA( temp );
                    }
#endif // defined(_DEBUG)
                    const size_t nameLength = ::lstrlenA( moduleEntry.szModule );
                    const size_t suffixLength = 4; // ::lstarlenA(".exe")
                    assert( 4 <= nameLength );
                    const char* pName = &moduleEntry.szModule[nameLength-4/*::lstrlenA(".exe"*/];
                    if ( 0 == ::lstrcmpiA( pName, ".exe" ) )
                    {
                        MEMORY_BASIC_INFORMATION mbi;
                        const size_t size = ::VirtualQueryEx( mHandle, moduleEntry.hModule, &mbi, sizeof(mbi) );
                        if ( 0 == size )
                        {
                            const DWORD dwErr = ::GetLastError();
                            assert( false );
                        }
                        else
                        {
                            this->mModule = moduleEntry.hModule;
                            this->mModuleBase = moduleEntry.modBaseAddr;
                            this->mModuleSize = moduleEntry.modBaseSize;
                        }
                    }
                } while( BRet = ::Module32Next( hSnapshot, &moduleEntry ) );
            }
        }


        if ( INVALID_HANDLE_VALUE != hSnapshot )
        {
            const BOOL BRet = ::CloseHandle( hSnapshot );
            if ( !BRet )
            {
                const DWORD dwErr = ::GetLastError();
                return false;
            }
            else
            {
                const_cast<HANDLE>(hSnapshot) = INVALID_HANDLE_VALUE;
            }
        }
    }

    return true;
}


HMODULE
RemoteProcess::findModule( const char* pName ) const
{
    if ( NULL == this->mHandle )
    {
        return false;
    }

    HMODULE     hModule = NULL;
    {
#if defined(_M_IX86)
        const DWORD dwFlags = TH32CS_SNAPMODULE;
#endif // defined(_M_X86)
#if defined(_M_X64)
        const DWORD dwFlags = TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32;
#endif // defined(_M_X64)
        const HANDLE hSnapshot = ::CreateToolhelp32Snapshot( dwFlags, this->mProcessId );

        if ( INVALID_HANDLE_VALUE == hSnapshot )
        {
            const DWORD dwErr = ::GetLastError();
            if ( ERROR_PARTIAL_COPY == dwErr )
            {
                // 32bit App, 64bit target NG
            }
        }
        else
        {
            MODULEENTRY32   moduleEntry;
            ZeroMemory( &moduleEntry, sizeof(moduleEntry) );
            moduleEntry.dwSize = sizeof(moduleEntry);

            const BOOL BRet = ::Module32First( hSnapshot, &moduleEntry );
            if ( !BRet )
            {
                const DWORD dwErr = ::GetLastError();
                if ( ERROR_NO_MORE_FILES == dwErr )
                {
                }
                else
                {
                    assert( false );
                }
            }
            else
            {
                BOOL BRet = FALSE;
                do
                {
#if 0//defined(_DEBUG)
                    {
                        char temp[128];
                        ::wsprintfA( temp, "%p %s\n", moduleEntry.hModule, moduleEntry.szModule );
                        ::OutputDebugStringA( temp );
                    }
#endif // defined(_DEBUG)

                    if ( 0 == ::lstrcmpiA( moduleEntry.szModule, pName ) )
                    {
                        hModule = moduleEntry.hModule;
                        break;
                    }


                } while( BRet = ::Module32Next( hSnapshot, &moduleEntry ) );
            }
        }


        if ( INVALID_HANDLE_VALUE != hSnapshot )
        {
            const BOOL BRet = ::CloseHandle( hSnapshot );
            if ( !BRet )
            {
                const DWORD dwErr = ::GetLastError();
                return false;
            }
            else
            {
                const_cast<HANDLE>(hSnapshot) = INVALID_HANDLE_VALUE;
            }
        }
    }

    return hModule;
}






} // namespace kk

