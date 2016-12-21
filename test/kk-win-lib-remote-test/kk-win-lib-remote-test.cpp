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

#include "stdafx.h"

#include "../../kk-win-lib/kk-win-lib-pe-iat.h"
#include "../../kk-win-lib/kk-win-lib-remote-process.h"
#include "../../kk-win-lib/kk-win-lib-remote-memory.h"

#include <assert.h>

#if defined(_MSC_VER)
#include <crtdbg.h>
#endif // defined(_MSC_VER)


static
DWORD
launchNotepad(const char* szPath, STARTUPINFOA* pSI, PROCESS_INFORMATION* pPI)
{
    DWORD result = 0;
    {
        const LPSTR pCommandLine = NULL;
        const LPSECURITY_ATTRIBUTES     pProcessAttributes = NULL;
        const LPSECURITY_ATTRIBUTES     pThreadAttributes = NULL;
        const BOOL isInheritHandles = FALSE;
        const DWORD dwCreationFlags = 0;
        const LPVOID pEnvironment = NULL;
        LPCSTR  pCurrentDirectory = NULL;

        const BOOL BRet = ::CreateProcessA(
            szPath
            , pCommandLine
            , pProcessAttributes, pThreadAttributes
            , isInheritHandles, dwCreationFlags, pEnvironment, pCurrentDirectory, pSI, pPI );
        if ( !BRet )
        {
            const DWORD dwErr = ::GetLastError();
        }
        else
        {
            result = pPI->dwProcessId;
        }
    }

    return result;
}

static
DWORD
launchNotepad(const bool is64bit)
{
    DWORD   result = 0;

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    ZeroMemory( &si, sizeof(si) );
    ZeroMemory( &pi, sizeof(pi) );

    si.cb = sizeof(si);

    char    szPath[_MAX_PATH+1];

#if defined(_M_X64)
    if ( is64bit )
    {
        {
            const UINT ret = ::GetSystemDirectoryA( szPath, sizeof(szPath) );
            if ( 0 == ret )
            {
                ::lstrcpyA( szPath, "C:\\Windows\\System32" );
            }
        }
        ::lstrcatA( szPath, "\\notepad.exe" );
        result = launchNotepad( szPath, &si, &pi );
    }
    else
    {
        {
            const UINT ret = ::GetSystemWow64DirectoryA( szPath, sizeof(szPath) );
            if ( 0 == ret )
            {
                ::lstrcpyA( szPath, "C:\\Windows\\SysWOW64" );
            }
        }
        ::lstrcatA( szPath, "\\notepad.exe" );

        result = launchNotepad( szPath, &si, &pi );

    }
#endif //  defined(_M_X64)
#if defined(_M_IX86)

    {
        const UINT ret = ::GetSystemDirectoryA( szPath, sizeof(szPath) );
        if ( 0 == ret )
        {
            ::lstrcpyA( szPath, "C:\\Windows\\System32" );
        }
    }
    ::lstrcatA( szPath, "\\notepad.exe" );

    if ( is64bit )
    {
        bool needRevert = false;
        PVOID  oldValue = NULL;
        {
            const BOOL BRet = ::Wow64DisableWow64FsRedirection(&oldValue);
            if ( !BRet )
            {
                const DWORD dwErr = ::GetLastError();
            }
            else
            {
                needRevert = true;
            }
        }

        result = launchNotepad( szPath, &si, &pi );

        if ( needRevert )
        {
            const BOOL BRet = ::Wow64RevertWow64FsRedirection(oldValue);
            if ( !BRet )
            {
                const DWORD dwErr = ::GetLastError();
            }
        }
    }
    else
    {
        result = launchNotepad( szPath, &si, &pi );
    }

#endif //  defined(_M_IX86)

#if 0
    if ( NULL != pi.hProcess )
    {
        ::LockSetForegroundWindow( LSFW_LOCK );
        ::WaitForInputIdle( pi.hProcess, 500 );
    }
#endif

    if ( NULL != pi.hThread )
    {
        const BOOL BRet = ::CloseHandle( pi.hThread );
        if ( !BRet )
        {
            const DWORD dwErr = ::GetLastError();
        }
    }
    if ( NULL != pi.hProcess )
    {
        const BOOL BRet = ::CloseHandle( pi.hProcess );
        if ( !BRet )
        {
            const DWORD dwErr = ::GetLastError();
        }
    }

    return result;
}

static
void
terminateProcess(const DWORD dwProcessId)
{
    const DWORD dwDesiredAccess =
        PROCESS_TERMINATE
        ;
    const BOOL isInheritHandle = FALSE;
    HANDLE handle = ::OpenProcess( dwDesiredAccess, isInheritHandle, dwProcessId );
    if ( NULL != handle )
    {
        const BOOL BRet = ::TerminateProcess( handle, 0 );
        if ( !BRet )
        {
            const DWORD dwErr = ::GetLastError();
        }
    }


    if ( NULL != handle )
    {
        const BOOL BRet = ::CloseHandle( handle );
        if ( !BRet )
        {
            const DWORD dwErr = ::GetLastError();
        }
        else
        {
            handle = NULL;
        }
    }
}


int _tmain(int argc, _TCHAR* argv[])
{
#if defined(_MSC_VER)
#if defined(_DEBUG)
    {
        int value = ::_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
        value |= _CRTDBG_ALLOC_MEM_DF;
        value |= _CRTDBG_LEAK_CHECK_DF;
        ::_CrtSetDbgFlag(value);
    }
#endif // defined(_DEBUG)
#endif // defined(_MSC_VER)

    {
        int* p = new int[16];
        memset( p, 0, 16 );
        delete [] p;
    }

  size_t loopCount = 1024*5;
  for ( size_t loop = 0; loop < loopCount; ++loop )
  {
    const bool param[2] = { false, true };
    for ( size_t index = 0; index < sizeof(param)/sizeof(param[0]); ++index )
    {
        const DWORD processID = launchNotepad(param[index]);
        {
            kk::RemoteProcess   remoteProcess;
            remoteProcess.init(processID);
        }

        {
            kk::RemoteProcess   remoteProcess;
            remoteProcess.init(processID);
            kk::RemoteMemory    remoteMemory;

            const HANDLE hProcess = remoteProcess.getHandle();
            const HMODULE hModule = remoteProcess.getHModule();
            remoteMemory.init( hProcess, hModule );
            const LPVOID pModuleBase = remoteProcess.getModuleBase();
            const size_t nModuleSize = remoteProcess.getModuleSize();
            remoteMemory.readFromRemote( pModuleBase, nModuleSize );
        }

        {
            kk::RemoteProcess   remoteProcess;
            remoteProcess.init(processID);
            kk::RemoteMemory    remoteMemory;

            const HANDLE hProcess = remoteProcess.getHandle();
            const HMODULE hModule = remoteProcess.getHModule();
            remoteMemory.init( hProcess, hModule );
            const LPVOID pModuleBase = remoteProcess.getModuleBase();
            const size_t nModuleSize = remoteProcess.getModuleSize();
            remoteMemory.readFromRemote( pModuleBase, nModuleSize );

            kk::PEIAT   peIAT;
            LPVOID pMemory = remoteMemory.getMemory();
            peIAT.init( pModuleBase, nModuleSize, pMemory );
        }

        terminateProcess( processID );
    }


  } // loop


	return 0;
}

