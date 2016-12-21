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

#include <assert.h>

#if defined(_MSC_VER)
#include <crtdbg.h>
#endif // defined(_MSC_VER)


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
        kk::Event evt;
    }
    {
        kk::Event evt;
        evt.term();
    }
    {
        kk::Event evt;
        evt.init();
    }

  } // loop

	return 0;
}

