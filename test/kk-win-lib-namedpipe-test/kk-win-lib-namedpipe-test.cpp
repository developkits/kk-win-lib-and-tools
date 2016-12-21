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

#include "../../kk-win-lib/kk-win-lib-event.h"
#include "../../kk-win-lib/kk-win-lib-namedpipe.h"

#include <assert.h>

#if defined(_MSC_VER)
#include <crtdbg.h>
#endif // defined(_MSC_VER)


struct ThreadParam
{
    kk::NamedPipe*  pipeServer;
    kk::NamedPipe*  pipeClient;
    kk::Event*      event;
};

static
unsigned int
WINAPI
threadServer(void* pArgVoid)
{
    ThreadParam* pParam = reinterpret_cast<ThreadParam *>(pArgVoid);

    char    buff[16];
    size_t  recvedSize = 0;
    const bool bRet = pParam->pipeServer->recv( buff, 16, recvedSize );
    assert( false != bRet );
    assert( 16 == recvedSize );

    return 0;
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
    {
        kk::NamedPipe pipe;
    }
    {
        kk::NamedPipe pipe;
        pipe.close();
    }
    {
        kk::NamedPipe pipe;
        const bool bRet = pipe.openServer( "test01", kk::NamedPipe::kModeServerInClientOut, 1, 1024 );
        assert( false != bRet );
    }
    {
        kk::NamedPipe pipe;
        const bool bRet = pipe.openServer( "\\\\.\\pipe\\test01", kk::NamedPipe::kModeServerInClientOut, 1, 1024 );
        assert( false != bRet );
    }
    {
        kk::NamedPipe pipe1;
        kk::NamedPipe pipe2;
        {
            const bool bRet = pipe1.openServer( "\\\\.\\pipe\\test01", kk::NamedPipe::kModeServerInClientOut, 1, 1024 );
            assert( false != bRet );
        }
        {
            const bool bRet = pipe2.openServer( "\\\\.\\pipe\\test01", kk::NamedPipe::kModeServerInClientOut, 1, 1024 );
            assert( false == bRet );
        }
    }
    {
        kk::NamedPipe pipe1;
        kk::NamedPipe pipe2;
        {
            const bool bRet = pipe1.openServer( "\\\\.\\pipe\\test01", kk::NamedPipe::kModeServerInClientOut, 2, 1024 );
            assert( false != bRet );
        }
        {
            const bool bRet = pipe2.openServer( "\\\\.\\pipe\\test01", kk::NamedPipe::kModeServerInClientOut, 2, 1024 );
            assert( false != bRet );
        }
    }

    {
        kk::NamedPipe pipe;
        const bool bRet = pipe.openServer( "\\\\.\\pipe\\test01", kk::NamedPipe::kModeServerOutClientIn, 1, 1024 );
        assert( false != bRet );
        {
            char buff[16];
            size_t sendedSize = 0;
            {
                const bool bRet = pipe.send( buff, 16, sendedSize );
                assert( 16 == sendedSize );
                assert( false != bRet );
            }
        }
    }

    {
        kk::NamedPipe   pipeServer;
        kk::NamedPipe   pipeClient;
        kk::Event       evt;

        ThreadParam     threadParam;
        threadParam.pipeServer = &pipeServer;
        threadParam.pipeClient = &pipeClient;
        threadParam.event = &evt;

        HANDLE hThread = reinterpret_cast<HANDLE>(::_beginthreadex( NULL, 0, threadServer, &threadParam, 0, NULL ));
        if ( NULL != hThread )
        {
            pipeServer.openServer( "test01", kk::NamedPipe::kModeServerInClientOut, 1, 1024 );
            pipeClient.openClient( "test01", kk::NamedPipe::kModeServerInClientOut );

            char buff[16];
            size_t sendedSize = 0;
            {
                const bool bRet = pipeClient.send( buff, 16, sendedSize );
                assert( false != bRet );
            }
            {
                const DWORD dwRet = ::WaitForSingleObject( hThread, INFINITE );
            }
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

    }
  } // loop

	return 0;
}

