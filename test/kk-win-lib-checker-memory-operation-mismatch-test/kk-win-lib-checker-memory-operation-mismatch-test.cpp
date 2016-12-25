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

#if 0
//#define MY_USE_IMPLIMENT_CRT_MALLOCFREE
#define MY_USE_IMPLIMENT_CRT_MALLOCFREE_ALIGN
#include "mynewdelete.h"
#endif

#include "../../kk-win-lib/kk-win-lib-event.h"
#include "../../kk-win-lib/kk-win-lib-namedpipe.h"
#include "../../kk-win-lib/checker/kk-win-lib-checker-memory-operation-mismatch.h"
#include "../../kk-win-lib/checker/kk-win-lib-checker-memory-operation-mismatch-client.h"
#include "../../kk-win-lib/checker/kk-win-lib-checker-memory-operation-mismatch-server.h"

#include "../../kk-win-lib/kk-win-lib-remote-process.h"
#include "../../kk-win-lib/kk-win-lib-debug-symbol.h"

#include <assert.h>

#if defined(_MSC_VER)
#include <crtdbg.h>
#endif // defined(_MSC_VER)

#define _NO_CVCONST_H
#include <dbghelp.h>
//#pragma comment(lib,"dbghelp.lib")

enum enumDataType
{
    kDataTypeNone = 0
    , kDataTypeVoid = 1
    , kDataTypeVoidPointer = 2
    , kDataTypeUInt = 3
};

enum enumOperation
{
    kOperationNone = 0
    , kOperationNew = 1
    , kOperationDelete = 2
    , kOperationNewArray = 3
    , kOperationDeleteArray = 4
};

static
enumOperation
parseName
(
    const char* name
)
{
    enumOperation   result = kOperationNone;

    if ( 0 == ::lstrcmpA( name, "operator new" ) )
    {
        result = kOperationNew;
    }
    else
    if ( 0 == ::lstrcmpA( name, "operator delete" ) )
    {
        result = kOperationDelete;
    }
    else
    if ( 0 == ::lstrcmpA( name, "operator new[]" ) )
    {
        result = kOperationNewArray;
    }
    else
    if ( 0 == ::lstrcmpA( name, "operator delete[]" ) )
    {
        result = kOperationDeleteArray;
    }
    else
    {
        result = kOperationNone;
    }

    return result;
}


static
bool
matchSymTag( const HANDLE hProcess, const DWORD64 moduleBase, const ULONG Index, const enum SymTagEnum tag)
{
    bool result = false;

    {
        DWORD dwType = 0;
        const BOOL BRet = ::SymGetTypeInfo(
            hProcess, moduleBase
            , Index, TI_GET_SYMTAG, &dwType );
        if ( BRet )
        {
            if ( tag == dwType )
            {
                result = true;
            }
        }
    }

    return result;
}

struct Context
{
    HANDLE      hProcess;
    DWORD64     moduleBase;
};

static
BOOL
CALLBACK
mySymEnumSymbolsProc(
    PSYMBOL_INFO        pSymInfo
    , ULONG             SymbolSize
    , PVOID             UserContext
)
{
    if ( SymTagFunction != pSymInfo->Tag )
    {
        return TRUE;
    }

#if 0
    {
        char    buff[4096];
        ::wsprintfA( buff, "  %p %08x %4u %4u %s\n", (void *)pSymInfo->Address, pSymInfo->Index, pSymInfo->Tag, pSymInfo->Index, pSymInfo->Name );
        ::OutputDebugStringA( buff );
    }
#endif

    if (
        0 == ::strncmp( pSymInfo->Name, "operator new", strlen("operator new") )
        || 0 == ::strncmp( pSymInfo->Name, "operator delete", strlen("operator delete") )
    )
    {
        const enumOperation enmOperation = parseName( pSymInfo->Name );
        assert( kOperationNone != enmOperation );
        enumDataType    enmTypeReturn = kDataTypeNone;
        DWORD           dwCallConversion = 0;
        enumDataType    enmTypeArg0 = kDataTypeNone;
        bool            isArgSingle = true;

#if 0
        {
            char    buff[1024];
            ::wsprintfA( buff, "%p %08x %4u %4u %s\n", (void *)pSymInfo->Address, pSymInfo->Index, pSymInfo->Tag, pSymInfo->Index, pSymInfo->Name );
            ::OutputDebugStringA( buff );
        }
#endif
        Context* context = reinterpret_cast<Context*>(UserContext);

        DWORD dwType = 0;
        if ( matchSymTag( context->hProcess, context->moduleBase, pSymInfo->TypeIndex, SymTagFunctionType ) )
        {
            //::SymGetTypeInfo( context->hProcess, context->moduleBase
            //    , pSymInfo->TypeIndex, TI_GET_TYPE, &dwType );
            ::SymGetTypeInfo( context->hProcess, context->moduleBase
                , pSymInfo->TypeIndex, TI_GET_TYPEID, &dwType );
            {
                DWORD dwType2 = 0;
                ::SymGetTypeInfo( context->hProcess, context->moduleBase
                    , dwType, TI_GET_SYMTAG, &dwType2 );
                if ( SymTagPointerType == dwType2 )
                {
                    ULONG64 length = 0;
                    ::SymGetTypeInfo( context->hProcess, context->moduleBase
                        , dwType, TI_GET_LENGTH, &length );
                    //::SymGetTypeInfo( context->hProcess, context->moduleBase
                    //    , dwType, TI_GET_TYPE, &dwType2 );
                    ::SymGetTypeInfo( context->hProcess, context->moduleBase
                        , dwType, TI_GET_TYPEID, &dwType2 );

                    DWORD dwType3 = 0;
                    ::SymGetTypeInfo( context->hProcess, context->moduleBase
                        , dwType2, TI_GET_SYMTAG, &dwType3 );
                    if ( SymTagBaseType == dwType3 )
                    {
                        ::SymGetTypeInfo( context->hProcess, context->moduleBase
                            , dwType2, TI_GET_BASETYPE, &dwType3 );
                    }

                    switch ( dwType3 )
                    {
                    case 1: // btVoid
                        enmTypeReturn = kDataTypeVoidPointer;
                        break;
                    }

                }
                else
                if ( SymTagBaseType == dwType2 )
                {
                    DWORD dwType3 = 0;
                    ::SymGetTypeInfo( context->hProcess, context->moduleBase
                        , dwType, TI_GET_BASETYPE, &dwType3 );
                    switch ( dwType3 )
                    {
                    case 1: // btVoid
                        enmTypeReturn = kDataTypeVoid;
                        break;
                    }
                }
                else
                {
                }
            }

            ::SymGetTypeInfo( context->hProcess, context->moduleBase
                , pSymInfo->TypeIndex, TI_GET_CALLING_CONVENTION, &dwType );
            switch ( dwType )
            {
            case 0: // CV_CALL_NEAR_C
                break;
            }
            dwCallConversion = dwType;

            DWORD nCount = 0;
            DWORD nCountChild = 0;
            ::SymGetTypeInfo( context->hProcess, context->moduleBase
                , pSymInfo->TypeIndex, TI_GET_COUNT, &nCount );
            ::SymGetTypeInfo( context->hProcess, context->moduleBase
                , pSymInfo->TypeIndex, TI_GET_CHILDRENCOUNT, &nCountChild );
            const size_t nArgCount = 4;
            if ( !(
                (1 == nCount )
                && (1 == nCountChild)
                )
            )
            {
                isArgSingle = false;
            }

            if ( nCountChild < nArgCount )
            {
                char    buff[sizeof(TI_FINDCHILDREN_PARAMS)+nArgCount*sizeof(ULONG)*2];
                ZeroMemory( buff, sizeof(buff) );
                TI_FINDCHILDREN_PARAMS* pParam = reinterpret_cast<TI_FINDCHILDREN_PARAMS*>(buff);
                pParam->Count = nCountChild;
                pParam->Start = 0;
                ::SymGetTypeInfo( context->hProcess, context->moduleBase
                    , pSymInfo->TypeIndex, TI_FINDCHILDREN, pParam );

                for ( size_t index = 0; index < nCountChild; ++index )
                {
                    const ULONG typeIndex = pParam->ChildId[index];

                    DWORD dwType2 = 0;
                    ::SymGetTypeInfo( context->hProcess, context->moduleBase
                        , typeIndex, TI_GET_SYMTAG, &dwType2 );
                    if ( SymTagFunctionArgType == dwType2 )
                    {
                        ::SymGetTypeInfo( context->hProcess, context->moduleBase
                            , typeIndex, TI_GET_TYPEID, &dwType2 );

                        DWORD dwType3 = 0;
                        ::SymGetTypeInfo( context->hProcess, context->moduleBase
                            , dwType2, TI_GET_SYMTAG, &dwType3 );
#if 1
                        if ( SymTagPointerType == dwType3 )
                        {
                            ::SymGetTypeInfo( context->hProcess, context->moduleBase
                                , dwType2, TI_GET_TYPEID, &dwType3 );

                            DWORD dwType4 = 0;
                            ::SymGetTypeInfo( context->hProcess, context->moduleBase
                                , dwType3, TI_GET_SYMTAG, &dwType4 );
                            if ( SymTagBaseType == dwType4 )
                            {
                                ::SymGetTypeInfo( context->hProcess, context->moduleBase
                                    , dwType3, TI_GET_BASETYPE, &dwType4 );

                                switch ( dwType4 )
                                {
                                case 1: // btVoid
                                    if ( 0 == index )
                                    {
                                        enmTypeArg0 = kDataTypeVoidPointer;
                                    }
                                    break;
                                }
                            }

                        }
                        else
                        if ( SymTagBaseType == dwType3 )
                        {
                            DWORD dwType4 = 0;
                            ::SymGetTypeInfo( context->hProcess, context->moduleBase
                                , dwType2, TI_GET_BASETYPE, &dwType4 );

                            switch ( dwType4 )
                            {
                            case 1: // btVoid
                                if ( 0 == index )
                                {
                                    enmTypeArg0 = kDataTypeVoid;
                                }
                                break;
                            case 7: // btUInt
                                if ( 0 == index )
                                {
                                    ULONG64 length = 0;
                                    ::SymGetTypeInfo( context->hProcess, context->moduleBase
                                        , dwType2, TI_GET_LENGTH, &length );

                                    enmTypeArg0 = kDataTypeUInt;
                                }
                                break;
                            default:
                                ::DebugBreak();
                                break;
                            }

                        }
#endif

                    }
                }

            }

#if 1
            if ( isArgSingle )
            {
                char    buff[1024];
                ::wsprintfA( buff, "%p %08x %4u %4u %s\n", (void *)pSymInfo->Address, pSymInfo->Index, pSymInfo->Tag, pSymInfo->Index, pSymInfo->Name );
                ::OutputDebugStringA( buff );
            }
#endif

#if 1
            if ( isArgSingle )
            {
                IMAGEHLP_LINE64     line;
                line.SizeOfStruct = sizeof(line);
                DWORD dwDisplacement = 0;

                const BOOL BRet = ::SymGetLineFromAddr64( context->hProcess, pSymInfo->Address, &dwDisplacement, &line );
                if ( !BRet )
                {
                    const DWORD dwErr = ::GetLastError();
                }
                else
                {
                    {
                        char    buff[1024];
                        ::wsprintfA( buff, "  %s(%u)\n", line.FileName, line.LineNumber );
                        ::OutputDebugStringA( buff );
                    }
                }
            }
#endif

            ::OutputDebugStringA( "" );


        } // SymTagFunctionType

#if 0
            DWORD dwType2 = 0;
            ::SymGetTypeInfo( context->hProcess, context->moduleBase
                , dwType, TI_GET_SYMTAG, &dwType2 );
            //assert( SymTagBaseType == dwType2 );
            ::SymGetTypeInfo( context->hProcess, context->moduleBase
                , dwType, TI_GET_BASETYPE, &dwType2 );
            ULONG64 length = 0;
            ::SymGetTypeInfo( context->hProcess, context->moduleBase
                , dwType2, TI_GET_LENGTH, &length );
            WCHAR** pName = NULL;
            ::SymGetTypeInfo( context->hProcess, context->moduleBase
                , dwType2, TI_GET_SYMNAME, &pName );
            ::SymGetTypeInfo( context->hProcess, context->moduleBase
                , dwType, TI_GET_LEXICALPARENT, &dwType2 );
#endif


        ::OutputDebugStringA( "" );
    }

    return TRUE;
}

static
void
testEnum(void)
{
    HANDLE hProcess = ::GetCurrentProcess();
    HMODULE hModule = ::GetModuleHandleA( NULL );

    DWORD opt = 0;
    opt = ::SymGetOptions();
    opt |= SYMOPT_DEBUG;
    opt |= SYMOPT_DEFERRED_LOADS;
    opt &= (~SYMOPT_UNDNAME);
    //opt |= SYMOPT_NO_CPP;
    opt |= SYMOPT_LOAD_LINES;
    ::SymSetOptions( opt );
    ::SymInitialize( hProcess, NULL, TRUE );

    DWORD64 dwBaseOfDll = ::SymLoadModule64( hProcess, NULL, NULL, NULL, (DWORD64)hModule, 0 );

    Context     context;
    context.hProcess = hProcess;
    context.moduleBase = (DWORD64)hModule;
    //::SymEnumSymbols( hProcess, 0, NULL, mySymEnumSymbolsProc, NULL );
    ::SymEnumSymbols( hProcess, (DWORD64)hModule, NULL, mySymEnumSymbolsProc, &context );
    //::SymEnumSymbols( hProcess, (DWORD64)hModule, "*", mySymEnumSymbolsProc, NULL );
    //::SymEnumSymbols( hProcess, (DWORD64)hModule, "*!*", mySymEnumSymbolsProc, NULL );

#if 0
    opt |= SYMOPT_UNDNAME;
    ::SymSetOptions( opt );
    ::SymEnumSymbols( hProcess, (DWORD64)hModule, NULL, mySymEnumSymbolsProc, NULL );
#endif

    ::SymCleanup( hProcess );
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

    //testEnum();

    {
        int*    temp = new int;
        delete temp;
    }

    {
        char*   temp = new char[16];
        delete [] temp;
    }

    {
        kk::DebugSymbol*    temp = ::new kk::DebugSymbol();
        ::delete temp;
    }

    {
        kk::DebugSymbol*    temp = ::new kk::DebugSymbol[2]();
        ::delete [] temp;
    }

    {
        kk::DebugSymbol     symbol;
        const DWORD processId = ::GetCurrentProcessId();
        symbol.init( processId );
    }

    {
        kk::DebugSymbol     symbol;
        const DWORD processId = ::GetCurrentProcessId();
        symbol.init( processId );
        symbol.findGlobalReplacements();
    }

  size_t loopCount = 1024*5;
  for ( size_t loop = 0; loop < loopCount; ++loop )
  {
    {
        kk::checker::MemoryOperationMismatchServer  server;
        kk::checker::MemoryOperationMismatchClient  client;
        kk::Event       evt;

        server.init(true);
        server.serverStart();
        client.init(true);
        client.disableHookIAT( true );

        {
            const DWORD processId = ::GetCurrentProcessId();
            client.sendProcessId( processId );

            client.sendOperation( kk::checker::MemoryOperationMismatch::kOperationMalloc, 0x4 );
            client.sendOperation( kk::checker::MemoryOperationMismatch::kOperationFree, 0x4 );
        }
    }

    {
        kk::checker::MemoryOperationMismatchServer  server;
        kk::checker::MemoryOperationMismatchClient  client;
        kk::Event       evt;

        server.init(false);
        server.serverStart();
        client.init(true);
        client.disableHookIAT( true );

        {
            const DWORD processId = ::GetCurrentProcessId();
            client.sendProcessId( processId );

            client.sendOperation( kk::checker::MemoryOperationMismatch::kOperationMalloc, 0x4 );
            client.sendOperation( kk::checker::MemoryOperationMismatch::kOperationFree, 0x8 );
        }
    }

    {
        kk::checker::MemoryOperationMismatchServer  server;
        kk::checker::MemoryOperationMismatchClient  client;
        kk::Event       evt;

        server.init(false);
        server.serverStart();
        client.init(true);

        {
            const DWORD processId = ::GetCurrentProcessId();
            client.sendProcessId( processId );

            client.sendOperation( kk::checker::MemoryOperationMismatch::kOperationFree, 0x0 );
            client.sendOperation( kk::checker::MemoryOperationMismatch::kOperationDelete, 0x0 );
            client.sendOperation( kk::checker::MemoryOperationMismatch::kOperationDeleteArray, 0x0 );
            client.sendOperation( kk::checker::MemoryOperationMismatch::kOperationAlignedFree, 0x0 );
        }
    }

  } // loop


    {
#pragma comment(lib,"ws2_32.lib")
        ::WSAStartup( MAKEWORD(2,2), NULL );
        ::send( NULL, NULL, 0, 0 );
        ::recv( NULL, NULL, 0, 0 );
    }

	return 0;
}

