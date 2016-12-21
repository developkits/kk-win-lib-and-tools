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

#include "kk-win-lib-event.h"


#include <assert.h>

namespace kk
{

Event::Event()
{
    mHandle = NULL;
}

Event::~Event()
{
    this->term();
}

bool
Event::term(void)
{
    bool result = false;

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

    return result;
}


bool
Event::init(void)
{
    this->term();

    {
        const LPSECURITY_ATTRIBUTES pSecurityAttributes = NULL;
        const BOOL bManualReset = FALSE;
        const BOOL bInitialState = FALSE;
        const LPCSTR pName = NULL;
        {
            const HANDLE handle = ::CreateEventA( pSecurityAttributes, bManualReset, bInitialState, pName );
            if ( NULL == handle )
            {
                return false;
            }

            this->mHandle = handle;
        }
    }

    return true;
}



bool
Event::wait(const DWORD dwMilliSeconds)
{
    if ( NULL == this->mHandle )
    {
        return false;
    }

    {
        const DWORD dwRet = ::WaitForSingleObject( this->mHandle, dwMilliSeconds );
        if ( WAIT_OBJECT_0 != dwRet )
        {
            const DWORD dwErr = ::GetLastError();
            return false;
        }
    }

    return true;
}

bool
Event::set(void)
{
    if ( NULL == this->mHandle )
    {
        return false;
    }

    {
        const BOOL BRet = ::SetEvent( this->mHandle );
        if ( !BRet )
        {
            const DWORD dwErr = ::GetLastError();
            return false;
        }
    }

    return true;
}





} // namespace kk

