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

#pragma once

//#include <new>
#include <malloc.h>
#include <assert.h>

#define     MY_NEWDELETE_SCOPE      static
//#define     MY_NEWDELETE_SCOPE

#if defined(MY_USE_IMPLIMENT_CRT_MALLOCFREE)
static
inline
void* operator new(size_t size)
{
    return ::malloc(size);
}

static
inline
void operator delete(void* p)
{
#if defined(_MSC_VER)
    {
        const size_t size = ::_msize( p );
        if ( (size_t)-1 == size )
        {
            assert(0);
        }
    }
#endif // defined(_MSC_VER)
    ::free(p);
}

static
inline
void* operator new[](size_t size)
{
    return ::malloc(size);
}

static
inline
void operator delete[](void* p)
{
#if defined(_MSC_VER)
    {
        const size_t size = ::_msize( p );
        if ( (size_t)-1 == size )
        {
            assert(0);
        }
    }
#endif // defined(_MSC_VER)
    ::free(p);
}

#endif // defined(MY_USE_IMPLIMENT_CRT_MALLOCFREE)


#if defined(MY_USE_IMPLIMENT_CRT_MALLOCFREE_ALIGN)
#define     MY_ALLOC_ALIGN      8

#if 1
MY_NEWDELETE_SCOPE
inline
void* operator new(size_t size)
{
    void* p;
#if defined(_MSC_VER)
    p = ::_aligned_malloc( size, MY_ALLOC_ALIGN );
#if defined(_WIN32) && defined(MY_DISP_LOG)
    {
        char buff[256];
        ::wsprintfA( buff, "op new %p %u\n", p, size );
        ::OutputDebugStringA( buff );
    }
#endif // defined(_WIN32) && defined(MY_DISP_LOG)
#endif // defined(_MSC_VER)

    return p;
}

MY_NEWDELETE_SCOPE
inline
void operator delete(void* p)
{
#if defined(_MSC_VER)
    {
        const size_t size = ::_aligned_msize( p, MY_ALLOC_ALIGN, 0 );
#if defined(_WIN32) && defined(MY_DISP_LOG)
        {
            char buff[256];
            ::wsprintfA( buff, "op del %p %u\n", p, size );
            ::OutputDebugStringA( buff );
        }
#endif // defined(_WIN32) && defined(MY_DISP_LOG)
        if ( (size_t)-1 == size )
        {
            assert(0);
        }
    }
    ::_aligned_free( p );
#endif // defined(_MSC_VER)
}
#endif

#if 0
MY_NEWDELETE_SCOPE
inline
void* operator new(size_t size) throw(std::bad_alloc)
{
    void* p;
#if defined(_MSC_VER)
    p = ::_aligned_malloc( size, MY_ALLOC_ALIGN );
#if defined(_WIN32) && defined(MY_DISP_LOG)
    {
        char buff[256];
        ::wsprintfA( buff, "op new %p %u\n", p, size );
        ::OutputDebugStringA( buff );
    }
#endif // defined(_WIN32) && defined(MY_DISP_LOG)
#endif // defined(_MSC_VER)

    return p;
}

MY_NEWDELETE_SCOPE
inline
void operator delete(void* p) throw()
{
#if defined(_MSC_VER)
    {
        const size_t size = ::_aligned_msize( p, MY_ALLOC_ALIGN, 0 );
#if defined(_WIN32) && defined(MY_DISP_LOG)
        {
            char buff[256];
            ::wsprintfA( buff, "op del %p %u\n", p, size );
            ::OutputDebugStringA( buff );
        }
#endif // defined(_WIN32) && defined(MY_DISP_LOG)
        if ( (size_t)-1 == size )
        {
            assert(0);
        }
    }
    ::_aligned_free( p );
#endif // defined(_MSC_VER)
}
#endif


#if 0
MY_NEWDELETE_SCOPE
inline
void* operator new[](size_t size) throw(std::bad_alloc)
{
    void* p;
#if defined(_MSC_VER)
    p = ::_aligned_malloc( size, MY_ALLOC_ALIGN );
#if defined(_WIN32) && defined(MY_DISP_LOG)
    {
        char buff[256];
        ::wsprintfA( buff, "op new[] %p %u\n", p, size );
        ::OutputDebugStringA( buff );
    }
#endif // defined(_WIN32) && defined(MY_DISP_LOG)
#endif // defined(_MSC_VER)

    return p;
}

MY_NEWDELETE_SCOPE
inline
void operator delete[](void* p) throw()
{
#if defined(_MSC_VER)
    {
        const size_t size = ::_aligned_msize( p, MY_ALLOC_ALIGN, 0 );
#if defined(_WIN32) && defined(MY_DISP_LOG)
        {
            char buff[256];
            ::wsprintfA( buff, "op del[] %p %u\n", p, size );
            ::OutputDebugStringA( buff );
        }
#endif // defined(_WIN32) && defined(MY_DISP_LOG)
        if ( (size_t)-1 == size )
        {
            assert(0);
        }
    }
    ::_aligned_free( p );
#endif // defined(_MSC_VER)
}
#endif

#if 1
MY_NEWDELETE_SCOPE
inline
void* operator new[](size_t size)
{
    void* p;
#if defined(_MSC_VER)
    p = ::_aligned_malloc( size, MY_ALLOC_ALIGN );
#endif // defined(_MSC_VER)
#if defined(_WIN32) && defined(MY_DISP_LOG)
    {
        char buff[256];
        ::wsprintfA( buff, "op new[] %p %u\n", p, size );
        ::OutputDebugStringA( buff );
    }
#endif // defined(_WIN32) && defined(MY_DISP_LOG)

    return p;
}


MY_NEWDELETE_SCOPE
inline
void operator delete[](void* p)
{
#if defined(_MSC_VER)
    {
        const size_t size = ::_aligned_msize( p, MY_ALLOC_ALIGN, 0 );
#if defined(_WIN32) && defined(MY_DISP_LOG)
        {
            char buff[256];
            ::wsprintfA( buff, "op del[] %p %u\n", p, size );
            ::OutputDebugStringA( buff );
        }
#endif // defined(_WIN32) && defined(MY_DISP_LOG)
        if ( (size_t)-1 == size )
        {
            assert(0);
        }
    }
    ::_aligned_free( p );
#endif // defined(_MSC_VER)
}
#endif



#endif // defined(MY_USE_IMPLIMENT_CRT_MALLOCFREE_ALIGN)

