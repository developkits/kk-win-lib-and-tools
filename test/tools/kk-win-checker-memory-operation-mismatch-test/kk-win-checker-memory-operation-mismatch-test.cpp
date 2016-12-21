
#include "stdafx.h"

#include <stdlib.h>

#include <map>

class A
{
public:
    A()
    {
        value = '@';
    }
protected:
    char    value;
};

class B : public A
{
public:
    B()
    {
        value.clear();
        A   a;
        value.insert( std::pair<size_t,A>(123,a) );
    }
protected:
    std::map<size_t,A>  value;
};

int _tmain(int argc, _TCHAR* argv[])
{
#if 1
    HMODULE hModule = ::LoadLibraryA( "kk-win-checker-memory-operation-mismatch-dll.dll" );

#if 0
    if ( NULL != hModule )
    {
        ::FreeLibrary( hModule );
        hModule = NULL;
    }
#endif
#endif

    ::OutputDebugStringA( "" );
    {
        int*    p = (int*)::malloc( 16 );
        ::free( p );
    }
#if 1
    {
        int*    p = (int*)::calloc( 4, sizeof(int) );
        ::free( p );
    }
#endif
#if 1
    {
        int*    p = (int*)::malloc( 16 );
        p = (int*)::realloc( p, 32 );
        ::free( p );
    }
    {
        int*    p = (int*)::malloc( 16 );
        p = (int*)::realloc( p, 8 );
        ::free( p );
    }
#endif
#if 0
    {
        int*    p = new int;
        delete p;
    }
#endif
#if 1
    {
        // oops. vc2008 compile to 'operator new(size_t)'...
        B*  p = new B[4];
        delete[] p;
    }
#endif

    {
        int*    p = (int*)::_aligned_malloc( 16, 8 );
        ::_aligned_free( p );
    }

    {
        int*    p = (int*)::_aligned_malloc( 16, 8 );
        p = (int*)::_aligned_recalloc( p, 4, 8, 8 );
        ::_aligned_free( p );
    }

    {
        int*    p = (int*)::_aligned_malloc( 16, 8 );
        p = (int*)::_aligned_realloc( p, 8, 8 );
        ::_aligned_free( p );
    }

    {
        int*    p = (int*)::_aligned_malloc( 16, 8 );
        p = (int*)::_aligned_realloc( p, 32, 8 );
        ::_aligned_free( p );
    }



    if ( NULL != hModule )
    {
        ::FreeLibrary( hModule );
        hModule = NULL;
    }

	return 0;
}

