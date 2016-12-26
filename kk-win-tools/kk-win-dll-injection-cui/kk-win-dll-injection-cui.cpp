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

#include "../../kk-win-lib/kk-win-lib-injection-dll.h"


#include <shlwapi.h>
#pragma comment(lib,"shlwapi.lib")

#include <assert.h>

static
bool
searchFile(char* szBuff, const size_t nBuffLen, const char* szPath )
{
    char    result[_MAX_PATH*2];

    bool found = false;
    szBuff[0] = '\0';

    for ( size_t method= 0; method< 3; ++method)
    {
        result[0] = '\0';

        switch( method )
        {
        case 0:
            {
                const DWORD dwRet = ::GetFileAttributesA( szPath );
                if ( INVALID_FILE_ATTRIBUTES != dwRet )
                {
                    found = true;
                    ::lstrcpyA( result, szPath );
                }
            }
            break;
        case 1:
            {
                char    buff[_MAX_PATH*2];
                {
                    const DWORD dwRet = ::GetCurrentDirectoryA( sizeof(buff), buff );
                    ::lstrcatA( buff, "\\" );
                    ::lstrcatA( buff, szPath );
                }
                {
                    const size_t len = ::lstrlenA( buff );
                    for ( size_t index = 0; index < len; ++index )
                    {
                        if ( '/' == buff[index] )
                        {
                            buff[index] = '\\';
                        }
                    }
                }
                {
                    const BOOL BRet = ::PathCanonicalizeA( result, buff );
                    if ( BRet )
                    {
                        const DWORD dwRet = ::GetFileAttributesA( result );
                        if ( INVALID_FILE_ATTRIBUTES != dwRet )
                        {
                            found = true;
                        }
                    }
                }
            }
            break;
        case 2:
            {
                char    buff[_MAX_PATH*2];
                const DWORD dwRet = ::GetModuleFileNameA( NULL, buff, sizeof(buff) );
                char* p = ::strrchr( buff, '\\' );
                if ( NULL != p )
                {
                    *p = '\0';
                }
                ::lstrcatA( buff, "\\" );
                ::lstrcatA( buff, szPath );
                {
                    const size_t len = ::lstrlenA( buff );
                    for ( size_t index = 0; index < len; ++index )
                    {
                        if ( '/' == buff[index] )
                        {
                            buff[index] = '\\';
                        }
                    }
                }
                {
                    const BOOL BRet = ::PathCanonicalizeA( result, buff );
                    if ( BRet )
                    {
                        const DWORD dwRet = ::GetFileAttributesA( result );
                        if ( INVALID_FILE_ATTRIBUTES != dwRet )
                        {
                            found = true;
                        }
                    }
                }
            }
            break;
        default:
            assert( false );
            break;
        }

        if ( found )
        {
            break;
        }
    }

    if ( found )
    {
        if ( nBuffLen < (size_t)::lstrlenA( result ) )
        {
            return false;
        }
        ::lstrcpyA( szBuff, result );
    }

    return found;
}

static
DWORD
createProcessSuspended(char* szCmd, STARTUPINFOA* pSI, PROCESS_INFORMATION* pPI)
{
    DWORD result = 0;
    {
        const LPCSTR pApplicationName = NULL;
        const LPSTR pCommandLine = szCmd;
        const LPSECURITY_ATTRIBUTES     pProcessAttributes = NULL;
        const LPSECURITY_ATTRIBUTES     pThreadAttributes = NULL;
        const BOOL isInheritHandles = FALSE;
        const DWORD dwCreationFlags = CREATE_SUSPENDED;
        const LPVOID pEnvironment = NULL;
        LPCSTR  pCurrentDirectory = NULL;

        const BOOL BRet = ::CreateProcessA(
            pApplicationName
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
launchProcess( HANDLE& hProcess, HANDLE& hThread, char* szCmd )
{
    DWORD   result = 0;

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    ZeroMemory( &si, sizeof(si) );
    ZeroMemory( &pi, sizeof(pi) );

    si.cb = sizeof(si);

    {
        result = createProcessSuspended( szCmd, &si, &pi );
    }

    hProcess = pi.hProcess;
    hThread = pi.hThread;

    return result;
}

int main(int argc, char* argv[])
{
    if ( argc < 2 )
    {
        printf( "argv[1]: injection-dll\n" );
        printf( "argv[2]: launch-exe\n" );
        printf( "argv[3..]: launch-exe's arg\n" );
        return 1;
    }

    {
        if ( NULL == argv[1] )
        {
            return 1;
        }
        if ( NULL == argv[2] )
        {
            return 1;
        }
    }

    char    szDllPath[_MAX_PATH*2];
    {
        const bool bRet = searchFile( szDllPath, sizeof(szDllPath), argv[1] );
        if( !bRet )
        {
            printf( " dll not found '%s'\n", argv[1] );
            return 2;
        }
    }

    char    szCmd[64*1024];
    szCmd[0] = '\0';
    // avoid security risk. see ref CreateProcess
    {
        const char* pArg = argv[2];
        if ( NULL == pArg )
        {
            return 1;
        }

        if ( NULL != ::strchr( pArg, ' ' ) )
        {
            ::lstrcpyA( szCmd, "\"" );
            ::lstrcatA( szCmd, pArg );
            ::lstrcatA( szCmd, "\"" );
        }
        else
        {
            ::lstrcpyA( szCmd, pArg );
        }
    }

    for ( size_t index = 3; index < (size_t)argc; ++index )
    {
        const char* pArg = argv[index];
        if ( NULL == pArg )
        {
            continue;
        }

        ::lstrcatA( szCmd, " " );
        if ( NULL == ::strchr( pArg, ' ' ) )
        {
            ::lstrcatA( szCmd, pArg );
        }
        else
        {
            ::lstrcatA( szCmd, "\"" );
            ::lstrcatA( szCmd, pArg );
            ::lstrcatA( szCmd, "\"" );
        }
    }
    //::OutputDebugStringA( ::GetCommandLineA() );
    //::OutputDebugStringA( "\n" );

    HANDLE  hProcess = NULL;
    HANDLE  hThread = NULL;

    const DWORD dwProcessId = launchProcess( hProcess, hThread, szCmd );

    if ( 0 != dwProcessId )
    {
        kk::InjectionDLL    injection;
        injection.init( hProcess );

        injection.injection( szDllPath );
    }

    if ( NULL != hThread )
    {
        ::ResumeThread( hThread );
    }

    if ( NULL != hThread )
    {
        const BOOL BRet = ::CloseHandle( hThread );
        if ( !BRet )
        {
            const DWORD dwErr = ::GetLastError();
        }
        else
        {
            hThread = NULL;
        }
    }
    if ( NULL != hProcess )
    {
        const BOOL BRet = ::CloseHandle( hProcess );
        if ( !BRet )
        {
            const DWORD dwErr = ::GetLastError();
        }
        else
        {
            hProcess = NULL;
        }
    }

	return 0;
}

