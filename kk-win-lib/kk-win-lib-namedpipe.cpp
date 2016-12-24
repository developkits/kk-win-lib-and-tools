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

#include "kk-win-lib-namedpipe.h"


#include <assert.h>

namespace
{

static const char sNamePipePrefix[] = "\\\\.\\pipe\\";

static
bool
makeFullPipeName(
    char* buff
    , const size_t buffLength
    , const char* name
)
{
    //::strcpy_s( buff, sizeof(buff)/sizeof(buff[0]), sNamePipePrefix );
    //::strcat_s( buff, sizeof(buff)/sizeof(buff[0]), name );

    bool needPrefix = true;
    {
        const size_t prefixLength = ::lstrlenA( sNamePipePrefix );
        const size_t nameLength = ::lstrlenA( name );

        if ( (buffLength-1) < nameLength )
        {
            return false;
        }

        ::lstrcpyA( buff, name );
        buff[prefixLength] = '\0';

        if ( 0 == ::lstrcmpiA( buff, sNamePipePrefix ) )
        {
            needPrefix = false;
        }
    }

    {
        const size_t length = ::lstrlenA(sNamePipePrefix) + ::lstrlenA(name);
        if ( (buffLength-1) < length )
        {
            return false;
        }
    }

    buff[0] = '\0';
    if ( needPrefix )
    {
        ::lstrcpyA( buff, sNamePipePrefix );
    }
    ::lstrcatA( buff, name );

    return true;
}

} // namespace `anonymous`


namespace kk
{

NamedPipe::NamedPipe()
{
    mHandle = INVALID_HANDLE_VALUE;
}

NamedPipe::~NamedPipe()
{
    this->close();
}

bool
NamedPipe::close(void)
{
    bool result = false;

    if ( INVALID_HANDLE_VALUE == mHandle )
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
            mHandle = INVALID_HANDLE_VALUE;
        }
    }

    return result;
}

bool
NamedPipe::openServer(
    const char* name
    , const enumPipeMode pipeMode
    , const size_t nInstanceCount
    , const size_t nBufferSize
)
{
    this->close();

    char     buff[256];
    {
        const bool bRet = makeFullPipeName( buff, sizeof(buff), name );
        if ( !bRet )
        {
            return false;
        }
    }

    {
        DWORD dwOpenMode = 0;
        DWORD dwPipeMode = 0;
        DWORD nMaxInstance;
        DWORD nOutBufferSize;
        DWORD nInBufferSize;
        DWORD nDefaultTimeOut;
        const LPSECURITY_ATTRIBUTES pSecurityAttributes = NULL;

        {
            switch ( pipeMode )
            {
            case kModeFullDuplex:
                dwOpenMode = PIPE_ACCESS_DUPLEX;
                break;
            case kModeServerInClientOut:
                dwOpenMode = PIPE_ACCESS_INBOUND;
                break;
            case kModeServerOutClientIn:
                dwOpenMode = PIPE_ACCESS_OUTBOUND;
                break;
            default:
                return false;
                break;
            }
        }
        {
            dwPipeMode |= PIPE_TYPE_BYTE;
            dwPipeMode |= PIPE_READMODE_BYTE;
            dwPipeMode |= PIPE_WAIT;
            dwPipeMode |= PIPE_ACCEPT_REMOTE_CLIENTS;
        }
        {
            if ( 0 == PIPE_UNLIMITED_INSTANCES )
            {
                return false;
            }

            if ( PIPE_UNLIMITED_INSTANCES < nInstanceCount )
            {
                nMaxInstance = PIPE_UNLIMITED_INSTANCES;
            }
            else
            {
                nMaxInstance = static_cast<DWORD>(nInstanceCount);
            }
        }
        {
            switch ( pipeMode )
            {
            case kModeFullDuplex:
                nOutBufferSize = static_cast<DWORD>(nBufferSize);
                nInBufferSize  = static_cast<DWORD>(nBufferSize);
                break;
            case kModeServerInClientOut:
                nOutBufferSize = 0;
                nInBufferSize  = static_cast<DWORD>(nBufferSize);
                break;
            case kModeServerOutClientIn:
                nOutBufferSize = static_cast<DWORD>(nBufferSize);
                nInBufferSize  = 0;
                break;
            default:
                return false;
                break;
            }
        }
        nDefaultTimeOut = NMPWAIT_USE_DEFAULT_WAIT;

        const HANDLE handle = ::CreateNamedPipeA(
            buff
            , dwOpenMode
            , dwPipeMode
            , nMaxInstance
            , nOutBufferSize, nInBufferSize
            , nDefaultTimeOut
            , pSecurityAttributes
            );
        if ( INVALID_HANDLE_VALUE == handle )
        {
            return false;
        }

        this->mHandle = handle;
    }

    return true;
}

bool
NamedPipe::waitClient( void )
{
    if ( INVALID_HANDLE_VALUE == mHandle )
    {
        return false;
    }

    bool result = false;
    {
        const BOOL BRet = ::ConnectNamedPipe( mHandle, NULL );
        if ( !BRet )
        {
            const DWORD dwErr = ::GetLastError();
            if ( ERROR_PIPE_CONNECTED == dwErr )
            {
                result = true;
            }
        }
        else
        {
            result = true;
        }
    }

    return result;
}


bool
NamedPipe::openClient(
    const char* name
    , const enumPipeMode pipeMode
)
{
    this->close();

    char     buff[256];
    {
        const bool bRet = makeFullPipeName( buff, sizeof(buff), name );
        if ( !bRet )
        {
            return false;
        }
    }

    {
        DWORD dwDesiredAccess = 0;
        DWORD dwShareMode = 0;
        const LPSECURITY_ATTRIBUTES pSecurityAttributes = NULL;
        DWORD dwCreationDisposition = 0;
        DWORD dwFlagsAndAttributes = 0;
        const HANDLE hTemplateFile = NULL;

        {
            switch ( pipeMode )
            {
            case kModeFullDuplex:
                dwDesiredAccess |= GENERIC_READ;
                dwDesiredAccess |= GENERIC_WRITE;
                break;
            case kModeServerInClientOut:
                dwDesiredAccess |= GENERIC_WRITE;
                break;
            case kModeServerOutClientIn:
                dwDesiredAccess |= GENERIC_READ;
                break;
            default:
                return false;
                break;
            }
        }
        dwCreationDisposition = OPEN_EXISTING;

        for ( ; ; )
        {
            const HANDLE handle = ::CreateFileA(
                buff
                , dwDesiredAccess
                , dwShareMode
                , pSecurityAttributes
                , dwCreationDisposition
                , dwFlagsAndAttributes
                , hTemplateFile
                );

            if ( INVALID_HANDLE_VALUE == handle )
            {
                const DWORD dwErr = ::GetLastError();
                if ( ERROR_PIPE_BUSY == dwErr )
                {
                    const BOOL BRet = ::WaitNamedPipeA( buff, NMPWAIT_WAIT_FOREVER );
                    if ( !BRet )
                    {
                        const DWORD dwErr = ::GetLastError();
                        return false;
                    }
                    else
                    {
                        continue;
                    }
                }

                ::Sleep( 5 * 1000 );
            }
            else
            {
                this->mHandle = handle;
                break;
            }
        }
    }

    return true;
}





static
bool
isErrorPipe(const DWORD dwErr)
{
    bool result = false;

    if ( ERROR_PIPE_LISTENING == dwErr )
    {
        result = true;
    }
    else
    if ( ERROR_BROKEN_PIPE == dwErr )
    {
        result = true;
    }
    else
    if ( ERROR_PIPE_NOT_CONNECTED == dwErr )
    {
        result = true;
    }
    else
    {
    }

    return result;
}


bool
NamedPipe::send(
    const char* buff
    , const size_t length
    , size_t& sendedSize
    , const size_t retryCount
)
{
    if ( INVALID_HANDLE_VALUE == this->mHandle )
    {
        sendedSize = 0;
        return false;
    }

    {
        size_t index = 0;
        size_t sizeRemain = length;

        for ( size_t retry = 0; retry < retryCount; ++retry )
        {
            const LPOVERLAPPED pOverlapped = NULL;
            DWORD dwWrited = 0;
            const BOOL BRet = ::WriteFile(
                this->mHandle
                , &buff[index]
                , static_cast<DWORD>(sizeRemain)
                , &dwWrited
                , pOverlapped
                );
            if ( !BRet )
            {
                const DWORD dwErr = ::GetLastError();
                if ( ERROR_INVALID_HANDLE == dwErr )
                {
                }
                else
                if ( isErrorPipe( dwErr ) )
                {
                }
                else
                {
                    assert( false );
                }
                return false;
            }
            else
            {
                index += dwWrited;
                sizeRemain -= dwWrited;

                if ( length <= index )
                {
                    break;
                }
            }

        } // retry

        sendedSize = index;
    }

    return true;
}

bool
NamedPipe::recv(
    char* buff
    , const size_t length
    , size_t& recvedSize
    , const size_t retryCount
)
{
    if ( INVALID_HANDLE_VALUE == this->mHandle )
    {
        recvedSize = 0;
        return false;
    }

    {
        size_t index = 0;
        size_t sizeRemain = length;

        for ( size_t retry = 0; retry < retryCount; ++retry )
        {
            const LPOVERLAPPED pOverlapped = NULL;
            DWORD dwReaded = 0;
            const BOOL BRet = ::ReadFile(
                this->mHandle
                , &buff[index]
                , static_cast<DWORD>(sizeRemain)
                , &dwReaded
                , pOverlapped
                );
            if ( !BRet )
            {
                const DWORD dwErr = ::GetLastError();
                if ( ERROR_INVALID_HANDLE == dwErr )
                {
                }
                else
                if ( isErrorPipe( dwErr ) )
                {
                }
                else
                {
                    assert( false );
                }
                return false;
            }
            else
            {
                index += dwReaded;
                sizeRemain -= dwReaded;

                if ( length <= index )
                {
                    break;
                }
            }

        } // retry

        recvedSize = index;
    }

    return true;
}



} // namespace kk

