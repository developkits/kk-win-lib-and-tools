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

#include "kk-win-lib-remote-process.h"
#include "kk-win-lib-debug-symbol.h"


#define _NO_CVCONST_H
#include <dbghelp.h>
//#pragma comment(lib,"dbghelp.lib")


#include <map>

#include <assert.h>


namespace kk
{

typedef BOOL (WINAPI *PFN_SymGetOptions)(void);
typedef BOOL (WINAPI *PFN_SymSetOptions)(DWORD SymOptions);

typedef BOOL (WINAPI *PFN_SymInitialize)(HANDLE hProcess, PCSTR UserSearchPath, BOOL fInvadeProcess);
typedef BOOL (WINAPI *PFN_SymCleanup)(HANDLE hProcess);

typedef DWORD64 (WINAPI *PFN_SymLoadModule64)(HANDLE hProcess, HANDLE hFile, PCSTR ImageName, PCSTR ModuleName, DWORD64 BaseOfDll, DWORD SizeOfDll);

//typedef BOOL (WINAPI *PFN_SymFromName)(HANDLE hProcess, PCSTR Name, PSYMBOL_INFO Symbol);
//typedef BOOL (WINAPI *PFN_SymNext)(HANDLE hProcess, PSYMBOL_INFO Symbol);
//typedef DWORD (WINAPI *PFN_UnDecorateSymbolName)(PCSTR DecoratedName, PSTR UnDecoratedName, DWORD UndecoratedLength, DWORD Flags);

typedef BOOL (WINAPI *PFN_SymEnumSymbols)(HANDLE hProcess, ULONG64 BaseOfDll, PCSTR Mask, PSYM_ENUMERATESYMBOLS_CALLBACK EnumSymbolsCallback, PVOID UserContext);
typedef BOOL (WINAPI *PFN_SymGetTypeInfo)(HANDLE hProcess, DWORD64 ModBase, ULONG TypeId, IMAGEHLP_SYMBOL_TYPE_INFO GetType, PVOID pInfo);
typedef BOOL (WINAPI *PFN_SymGetModuleInfo64)(HANDLE hProcess, DWORD64 ModBase, PIMAGEHLP_MODULE64 ModuleInfo);
typedef BOOL (WINAPI *PFN_SymGetLineFromAddr64)(HANDLE hProcess, DWORD64 qwAddr, DWORD* pdwDisplacement, PIMAGEHLP_LINE64 line);
typedef BOOL (WINAPI *PFN_SymEnumLines)(HANDLE hProcess, DWORD64 qwAddr, PCSTR Obj, PCSTR File, PSYM_ENUMLINES_CALLBACK EnumLinesCallback, PVOID UserContext);


class DebugSymbol::DebugSymbolImpl
{
public:
    DebugSymbolImpl();
    virtual ~DebugSymbolImpl();

public:
    bool
    init( const HANDLE processId );

    bool
    term( void );

public:
    bool
    findGlobalReplacements( const DWORD64 moduleBase, const size_t moduleSize );

    bool
    getGlobalReplacementsCount( size_t count[kIndexOperationMax] ) const;

    bool
    getGlobalReplacementsRVA( const enumIndexOperation indexOperation, DebugSymbol::FuncInfo* funcArray ) const;

    bool
    isIncludeCRTNewArray( void ) const;

protected:
    bool                mCRTCPPisStaticLinked;

public:
    bool
    setCRTCPPisStaticLinked( const bool isStaticLinked );

    bool
    getCRTCPPisStaticLinked( void ) const;

protected:
    static
    BOOL
    CALLBACK
    symEnumSymbolsProc( PSYMBOL_INFO pSymInfo, ULONG SymbolSize, PVOID UserContext );

    bool
    matchSymTag( const ULONG Index, const enum SymTagEnum tag);


protected:
    struct SearchCRTSource
    {
        DWORD64         addr;
        const char*     Obj;
        const char*     File;
        bool            found;
    };

    static
    BOOL
    CALLBACK
    symEnumLinesProc( PSRCCODEINFO pLineInfo, PVOID UserContext );

protected:
    HANDLE          mHandleProcess;
    DWORD64         mModuleBase;
    size_t          mModuleSize;

    HMODULE         mDbgHelp;
    PFN_SymGetOptions       mSymGetOptions;
    PFN_SymSetOptions       mSymSetOptions;
    PFN_SymInitialize       mSymInitialize;
    PFN_SymCleanup          mSymCleanup;
    //PFN_SymFromName         mSymFromName;
    //PFN_SymNext             mSymNext;
    //PFN_UnDecorateSymbolName    mUnDecorateSymbolName;
    PFN_SymLoadModule64     mSymLoadModule64;
    PFN_SymEnumSymbols      mSymEnumSymbols;
    PFN_SymGetTypeInfo      mSymGetTypeInfo;
    PFN_SymGetModuleInfo64  mSymGetModuleInfo64;
    PFN_SymGetLineFromAddr64    mSymGetLineFromAddr64;
    PFN_SymEnumLines        mSymEnumLines;

    //DWORD64         mGlobalReplacements[4];

protected:

    std::map<DWORD64,DebugSymbol::FuncInfo >     mGlobalReplacements[DebugSymbol::kIndexOperationMax];
    bool                    mIncludeCRTNewArray;

    std::map<DWORD64,DebugSymbol::FuncInfo >     mClassReplacements[DebugSymbol::kIndexOperationMax];

private:
    explicit DebugSymbolImpl(const DebugSymbolImpl&);
    DebugSymbolImpl& operator=(const DebugSymbolImpl&);
};

DebugSymbol::DebugSymbolImpl::DebugSymbolImpl()
{
    mHandleProcess = NULL;
    mModuleBase = NULL;
    mModuleSize = 0;

    mDbgHelp = NULL;
    mSymGetOptions = NULL;
    mSymSetOptions = NULL;
    mSymInitialize = NULL;
    mSymCleanup = NULL;
    //mSymFromName = NULL;
    //mSymNext = NULL;
    //mUnDecorateSymbolName = NULL;
    mSymLoadModule64 = NULL;
    mSymEnumSymbols = NULL;
    mSymGetTypeInfo = NULL;
    mSymGetModuleInfo64 = NULL;
    mSymGetLineFromAddr64 = NULL;
    mSymEnumLines = NULL;

    //ZeroMemory( mGlobalReplacements, sizeof(mGlobalReplacements) );
    for ( size_t index = 0; index < kIndexOperationMax; ++index )
    {
        mGlobalReplacements[index].clear();
    }
    mIncludeCRTNewArray = false;
    mCRTCPPisStaticLinked = false;
    for ( size_t index = 0; index < kIndexOperationMax; ++index )
    {
        mClassReplacements[index].clear();
    }
}

DebugSymbol::DebugSymbolImpl::~DebugSymbolImpl()
{
    this->term();
}

bool
DebugSymbol::DebugSymbolImpl::term( void )
{
    if ( NULL != mHandleProcess )
    {
        if ( NULL != mSymCleanup )
        {
            const BOOL BRet = mSymCleanup( mHandleProcess );
        }
    }


    mSymGetOptions = NULL;
    mSymSetOptions = NULL;
    mSymInitialize = NULL;
    mSymCleanup = NULL;
    //mSymFromName = NULL;
    //mSymNext = NULL;
    //mUnDecorateSymbolName = NULL;
    mSymLoadModule64 = NULL;
    mSymEnumSymbols = NULL;
    mSymGetTypeInfo = NULL;
    mSymGetModuleInfo64 = NULL;
    mSymGetLineFromAddr64 = NULL;
    mSymEnumLines = NULL;

    if ( NULL != mDbgHelp )
    {
        const BOOL BRet = ::FreeLibrary( mDbgHelp );
        if ( !BRet )
        {
        }
        else
        {
            mDbgHelp = NULL;
        }
    }

    for ( size_t index = 0; index < kIndexOperationMax; ++index )
    {
        mGlobalReplacements[index].clear();
    }
    mIncludeCRTNewArray = false;
    mCRTCPPisStaticLinked = false;
    for ( size_t index = 0; index < kIndexOperationMax; ++index )
    {
        mClassReplacements[index].clear();
    }


    mHandleProcess = NULL;
    mModuleBase = NULL;
    mModuleSize = 0;

    return true;
}

bool
DebugSymbol::DebugSymbolImpl::init( const HANDLE hProcess )
{
    this->term();

    {
        HMODULE hModule = ::LoadLibraryW( L"dbghelp.dll" );
        if ( NULL != hModule )
        {
            mSymGetOptions = (PFN_SymGetOptions)::GetProcAddress( hModule, "SymGetOptions" );
            mSymSetOptions = (PFN_SymSetOptions)::GetProcAddress( hModule, "SymSetOptions" );

            mSymInitialize = (PFN_SymInitialize)::GetProcAddress( hModule, "SymInitialize" );
            mSymCleanup = (PFN_SymCleanup)::GetProcAddress( hModule, "SymCleanup" );

            //mSymFromName = (PFN_SymFromName)::GetProcAddress( hModule, "SymFromName" );
            //mSymNext = (PFN_SymNext)::GetProcAddress( hModule, "SymNext" );
            //mUnDecorateSymbolName = (PFN_UnDecorateSymbolName)::GetProcAddress( hModule, "UnDecorateSymbolName" );
            mSymLoadModule64 = (PFN_SymLoadModule64)::GetProcAddress( hModule, "SymLoadModule64" );
            mSymEnumSymbols = (PFN_SymEnumSymbols)::GetProcAddress( hModule, "SymEnumSymbols" );
            mSymGetTypeInfo = (PFN_SymGetTypeInfo)::GetProcAddress( hModule, "SymGetTypeInfo" );
            mSymGetModuleInfo64 = (PFN_SymGetModuleInfo64)::GetProcAddress( hModule, "SymGetModuleInfo64" );
            mSymGetLineFromAddr64 = (PFN_SymGetLineFromAddr64)::GetProcAddress( hModule, "SymGetLineFromAddr64" );
            mSymEnumLines = (PFN_SymEnumLines)::GetProcAddress( hModule, "SymEnumLines" );

        }

        mDbgHelp = hModule;
    }

    mHandleProcess = hProcess;

    {
        DWORD opt = mSymGetOptions();
        opt |= SYMOPT_DEBUG;
        opt |= SYMOPT_DEFERRED_LOADS;
        //opt &= (~SYMOPT_UNDNAME);
        opt |= SYMOPT_LOAD_LINES;
        mSymSetOptions( opt );
    }
    if ( NULL != this->mHandleProcess )
    {
        const BOOL BRet = mSymInitialize( this->mHandleProcess, NULL, TRUE );
        if ( !BRet )
        {
            const DWORD dwErr = ::GetLastError();
        }
    }

    return true;
}


bool
DebugSymbol::DebugSymbolImpl::findGlobalReplacements( const DWORD64 moduleBase, const size_t moduleSize )
{
    if ( NULL == mHandleProcess )
    {
        return false;
    }

    {
        for ( size_t index = 0; index < kIndexOperationMax; ++index )
        {
            mGlobalReplacements[index].clear();
        }
    }

    if ( NULL != this->mHandleProcess )
    {
        {
            const DWORD64 dwBaseOfDll = mSymLoadModule64( mHandleProcess, NULL, NULL, NULL, moduleBase, (DWORD)moduleSize );
            assert( dwBaseOfDll == moduleBase );
            mModuleBase = dwBaseOfDll;
            mModuleSize = moduleSize;
        }

        {
            const BOOL BRet = mSymEnumSymbols( mHandleProcess, mModuleBase, NULL, symEnumSymbolsProc, this );
            if ( !BRet )
            {
                const DWORD dwErr = ::GetLastError();
            }
        }

    }

    if ( NULL != this->mHandleProcess )
    {
        IMAGEHLP_MODULE64       info;
        ZeroMemory( &info, sizeof(info) );
        info.SizeOfStruct = sizeof(info);
        {
            const BOOL BRet = mSymGetModuleInfo64( mHandleProcess, mModuleBase, &info );
            if ( !BRet )
            {
                const DWORD dwErr = ::GetLastError();
            }
        }

        if ( ! this->getCRTCPPisStaticLinked() )
        {
            if (
                1 == mGlobalReplacements[DebugSymbol::kIndexOperationNewArray].size()
                //&& 0 == mGlobalReplacements[DebugSymbol::kIndexOperationNew].size()
            )
            {
                if ( !info.LineNumbers )
                {
                    {
                        ::OutputDebugStringA( "kk-win-lib-debug-symbol: debug symbol does not have line numbers info\n" );
                    }
                }
                else
                {
                    std::map<DWORD64,DebugSymbol::FuncInfo>::const_iterator   it = mGlobalReplacements[DebugSymbol::kIndexOperationNewArray].begin();
                    const DWORD64 qwAddr = reinterpret_cast<const DWORD64>( ((LPBYTE)mModuleBase) + it->second.dwAddr );

                    IMAGEHLP_LINE64     line;
                    ZeroMemory( &line, sizeof(line) );
                    line.SizeOfStruct = sizeof(line);
                    {
                        DWORD dwDisplacement = 0;
                        const BOOL BRet = mSymGetLineFromAddr64( mHandleProcess, qwAddr, &dwDisplacement, &line );
                        if ( !BRet )
                        {
                            const DWORD dwErr = ::GetLastError();
                        }
                    }

                    bool includeCRTNewArray = true;
                    if ( NULL == line.FileName )
                    {
                        includeCRTNewArray = false;
                    }
                    else
                    {
                        char    fileName[_MAX_PATH];
                        ::lstrcpyA( fileName, line.FileName );
                        ::_strlwr_s( fileName, sizeof(fileName)/sizeof(fileName[0]) );

                        if ( NULL == ::strstr( fileName, "\\crt" ) )
                        {
                            includeCRTNewArray = false;
                        }
                        if ( NULL == ::strstr( fileName, "\\newaop" ) )
                        {
                            includeCRTNewArray = false;
                        }
                    }
                    mIncludeCRTNewArray = includeCRTNewArray;
                }
            }
        }
        else
        {
            if ( !info.LineNumbers )
            {
                {
                    ::OutputDebugStringA( "kk-win-lib-debug-symbol: debug symbol does not have line numbers info\n" );
                }
            }
            else
            {
                for ( size_t index = DebugSymbol::kIndexOperationNew; index < DebugSymbol::kIndexOperationMax; ++index )
                {
                    for (
                        std::map<DWORD64,DebugSymbol::FuncInfo>::iterator   it = mGlobalReplacements[index].begin();
                        it != mGlobalReplacements[index].end();
                        ++it
                    )
                    {
                        const DWORD64 qwAddr = reinterpret_cast<const DWORD64>( ((LPBYTE)mModuleBase) + it->second.dwAddr );

                        // SymGetLineFromAddr64 not work, unified function.
                        SearchCRTSource     context;
                        ZeroMemory( &context, sizeof(context) );
                        context.addr = qwAddr;

                        const char* searchFile = NULL;
                        {
                            switch ( index )
                            {
                            case DebugSymbol::kIndexOperationNew:
                                context.File = "\\new_scalar.cpp";
                                searchFile = "*\\new_scalar.cpp";
                                break;
                            case DebugSymbol::kIndexOperationDelete:
                                context.File = "\\delete_scalar.cpp";
                                searchFile = "*\\delete_scalar.cpp";
                                break;
                            case DebugSymbol::kIndexOperationNewArray:
                                context.File = "\\new_array.cpp";
                                searchFile = "*\\new_array.cpp";
                                break;
                            case DebugSymbol::kIndexOperationDeleteArray:
                                context.File = "\\delete_array.cpp";
                                searchFile = "*\\delete_array.cpp";
                                break;

                            case DebugSymbol::kIndexOperationDeleteSize:
                                context.File = "\\delete_scalar_size.cpp";
                                searchFile = "*\\delete_scalar_size.cpp";
                                break;
                            case DebugSymbol::kIndexOperationDeleteArraySize:
                                context.File = "\\delete_array_size.cpp";
                                searchFile = "*\\delete_array_size.cpp";
                                break;

                            default:
                                assert( false );
                                break;
                            }
                        }

                        {
                            const BOOL BRet = mSymEnumLines( mHandleProcess, mModuleBase, NULL, searchFile, symEnumLinesProc, &context );
                            if ( !BRet )
                            {
                                const DWORD dwErr = ::GetLastError();
                            }
                        }

                        if ( context.found )
                        {
                            it->second.isCRT = true;
                        }
                        else
                        {
                            it->second.isCRT = false;
                        }

                    } // for it
                } // for index
            }
        }
    }


    return true;
}

bool
DebugSymbol::DebugSymbolImpl::getGlobalReplacementsCount( size_t count[kIndexOperationMax] ) const
{
    if ( NULL == count )
    {
        return false;
    }

    for ( size_t index = 0; index < kIndexOperationMax; ++index )
    {
        count[index] = mGlobalReplacements[index].size();
    }

    return true;
}

bool
DebugSymbol::DebugSymbolImpl::getGlobalReplacementsRVA( const enumIndexOperation indexOperation, DebugSymbol::FuncInfo* funcArray ) const
{
    if ( NULL == funcArray )
    {
        return false;
    }

    const size_t count = mGlobalReplacements[indexOperation].size();
    size_t index = 0;
    for (
        std::map<DWORD64,DebugSymbol::FuncInfo>::const_iterator it = mGlobalReplacements[indexOperation].begin();
        it != mGlobalReplacements[indexOperation].end();
        ++it
    )
    {
        assert( index < count );
        funcArray[index] = it->second;
        index += 1;
    }

    return true;
}

bool
DebugSymbol::DebugSymbolImpl::isIncludeCRTNewArray( void ) const
{
    return mIncludeCRTNewArray;
}

bool
DebugSymbol::DebugSymbolImpl::setCRTCPPisStaticLinked( const bool isStaticLinked )
{
    bool oldValue = mCRTCPPisStaticLinked;

    mCRTCPPisStaticLinked = isStaticLinked;

    return oldValue;
}

bool
DebugSymbol::DebugSymbolImpl::getCRTCPPisStaticLinked( void ) const
{
    return mCRTCPPisStaticLinked;
}



static
enum enumDataType
{
    kDataTypeNone = 0
    , kDataTypeVoid = 1
    , kDataTypeVoidPointer = 2
    , kDataTypeChar = 3
    , kDataTypeCharPointer = 4
    , kDataTypeInt = 5
    , kDataTypeUInt = 6
    , kDataTypeUDT = 7
    , kDataTypeUDTPointer = 8
};

static
enum enumOperation
{
    kOperationNone = 0
    , kOperationGlobalNew = 1
    , kOperationGlobalDelete = 2
    , kOperationGlobalNewArray = 3
    , kOperationGlobalDeleteArray = 4

    , kOperationClassNew = 5
    , kOperationClassDelete = 6
    , kOperationClassNewArray = 7
    , kOperationClassDeleteArray = 8
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
        result = kOperationGlobalNew;
    }
    else
    if ( 0 == ::lstrcmpA( name, "operator delete" ) )
    {
        result = kOperationGlobalDelete;
    }
    else
    if ( 0 == ::lstrcmpA( name, "operator new[]" ) )
    {
        result = kOperationGlobalNewArray;
    }
    else
    if ( 0 == ::lstrcmpA( name, "operator delete[]" ) )
    {
        result = kOperationGlobalDeleteArray;
    }
    else
    {
        const char* p = ::strstr(name, "operator " );
        if ( NULL == p )
        {
            result = kOperationNone;
        }
        else
        if ( 0 == ::lstrcmpA( p, "operator new" ) )
        {
            result = kOperationClassNew;
        }
        else
        if ( 0 == ::lstrcmpA( p, "operator delete" ) )
        {
            result = kOperationClassDelete;
        }
        else
        if ( 0 == ::lstrcmpA( p, "operator new[]" ) )
        {
            result = kOperationClassNewArray;
        }
        else
        if ( 0 == ::lstrcmpA( p, "operator delete[]" ) )
        {
            result = kOperationClassDeleteArray;
        }
        else
        {
            result = kOperationNone;
        }
    }

    return result;
}


bool
DebugSymbol::DebugSymbolImpl::matchSymTag(
    const ULONG Index
    , const enum SymTagEnum tag
)
{
    bool result = false;

    const HANDLE    hProcess = this->mHandleProcess;
    const DWORD64   dwModuleBase = this->mModuleBase;
    {
        DWORD dwType = 0;
        {
            const BOOL BRet = this->mSymGetTypeInfo( hProcess, dwModuleBase, Index, TI_GET_SYMTAG, &dwType );
            if ( !BRet )
            {
                const DWORD dwErr = ::GetLastError();
            }
            else
            {
                if ( tag == dwType )
                {
                    result = true;
                }
            }
        }
    }

    return result;
}

//static
BOOL
CALLBACK
DebugSymbol::DebugSymbolImpl::symEnumSymbolsProc(
    PSYMBOL_INFO pSymInfo
    , ULONG SymbolSize
    , PVOID UserContext
)
{
    if ( NULL == pSymInfo )
    {
        return TRUE;
    }
    if ( NULL == UserContext )
    {
        return TRUE;
    }



    if ( SymTagFunction != pSymInfo->Tag )
    {
        return TRUE;
    }

#if 0
    if (
        NULL != ::strstr( pSymInfo->Name, "operator new" )
        || NULL != ::strstr( pSymInfo->Name, "operator delete" )
    )
#else
    if (
        0 == ::strncmp( pSymInfo->Name, "operator new", strlen("operator new") )
        || 0 == ::strncmp( pSymInfo->Name, "operator delete", strlen("operator delete") )
    )
#endif
    {
        const enumOperation enmOperation = parseName( pSymInfo->Name );
        assert( kOperationNone != enmOperation );
        enumDataType    enmTypeReturn = kDataTypeNone;
        DWORD           dwCallConversion = 0;
        enumDataType    enmTypeArg0 = kDataTypeNone;
        enumDataType    enmTypeArg1 = kDataTypeNone;
        enumDataType    enmTypeArg2 = kDataTypeNone;
        enumDataType    enmTypeArg3 = kDataTypeNone;
        DWORD           dwTypeIndexArg1 = 0;
        size_t          numArg = 0;


#if 0
        {
            char    buff[1024];
            ::wsprintfA( buff, "%p %08x %4u %4u %s\n", (void *)pSymInfo->Address, pSymInfo->Index, pSymInfo->Tag, pSymInfo->Index, pSymInfo->Name );
            ::OutputDebugStringA( buff );
        }
#endif
        DebugSymbol::DebugSymbolImpl* pThis = reinterpret_cast<DebugSymbol::DebugSymbolImpl*>(UserContext);
        const HANDLE    hProcess = pThis->mHandleProcess;
        const DWORD64   dwModuleBase = pThis->mModuleBase;

        if ( pThis->matchSymTag( pSymInfo->TypeIndex, SymTagFunctionType ) )
        {
            DWORD   dwTypeIndex = 0;
            {
                const BOOL BRet = pThis->mSymGetTypeInfo( hProcess, dwModuleBase, pSymInfo->TypeIndex, TI_GET_TYPEID, &dwTypeIndex );
                if ( !BRet )
                {
                    const DWORD dwErr = ::GetLastError();
                }
            }
            {
                DWORD   dwSymTag = 0;
                {
                    const BOOL BRet = pThis->mSymGetTypeInfo( hProcess, dwModuleBase, dwTypeIndex, TI_GET_SYMTAG, &dwSymTag );
                    if ( !BRet )
                    {
                        const DWORD dwErr = ::GetLastError();
                    }
                }
                if ( SymTagPointerType == dwSymTag )
                {
                    DWORD   dwTypeIndexPointer = 0;
                    {
                        const BOOL BRet = pThis->mSymGetTypeInfo( hProcess, dwModuleBase, dwTypeIndex, TI_GET_TYPEID, &dwTypeIndexPointer );
                        if ( !BRet )
                        {
                            const DWORD dwErr = ::GetLastError();
                        }
                    }

                    DWORD   dwSymTagPointer = 0;
                    {
                        const BOOL BRet = pThis->mSymGetTypeInfo( hProcess, dwModuleBase, dwTypeIndexPointer, TI_GET_SYMTAG, &dwSymTagPointer );
                        if ( !BRet )
                        {
                            const DWORD dwErr = ::GetLastError();
                        }
                    }
                    if ( SymTagBaseType == dwSymTagPointer )
                    {
                        DWORD   dwBaseTypePointer = 0;
                        {
                            const BOOL BRet = pThis->mSymGetTypeInfo( hProcess, dwModuleBase, dwTypeIndexPointer, TI_GET_BASETYPE, &dwBaseTypePointer );
                            if ( !BRet )
                            {
                                const DWORD dwErr = ::GetLastError();
                            }
                        }

                        switch ( dwBaseTypePointer )
                        {
                        case 1: // btVoid
                            enmTypeReturn = kDataTypeVoidPointer;
                            break;
                        }
                    }
                }
                else
                if ( SymTagBaseType == dwSymTag )
                {
                    DWORD dwBaseType = 0;
                    {
                        const BOOL BRet = pThis->mSymGetTypeInfo( hProcess, dwModuleBase, dwTypeIndex, TI_GET_BASETYPE, &dwBaseType );
                        if ( !BRet )
                        {
                            const DWORD dwErr = ::GetLastError();
                        }
                    }
                    switch ( dwBaseType )
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

            {
                DWORD   dwCallingConvertion = 0;
                {
                    const BOOL BRet = pThis->mSymGetTypeInfo( hProcess, dwModuleBase, pSymInfo->TypeIndex, TI_GET_CALLING_CONVENTION, &dwCallingConvertion );
                    if ( !BRet )
                    {
                        const DWORD dwErr = ::GetLastError();
                    }
                }
                switch ( dwCallingConvertion )
                {
                case 0: // CV_CALL_NEAR_C
                    break;
                }
                dwCallConversion = dwCallingConvertion;
            }

            DWORD nCount = 0;
            DWORD nCountChild = 0;
            {
                const BOOL BRet = pThis->mSymGetTypeInfo( hProcess, dwModuleBase, pSymInfo->TypeIndex, TI_GET_COUNT, &nCount );
                if ( !BRet )
                {
                    const DWORD dwErr = ::GetLastError();
                }
            }
            {
                const BOOL BRet = pThis->mSymGetTypeInfo( hProcess, dwModuleBase, pSymInfo->TypeIndex, TI_GET_CHILDRENCOUNT, &nCountChild );
                if ( !BRet )
                {
                    const DWORD dwErr = ::GetLastError();
                }
            }
            const size_t nArgCount = 5;
            if ( (
                (1 == nCount )
                && (1 == nCountChild)
                )
            )
            {
                numArg = 1;
            }
            else
            if ( (
                (2 == nCount )
                && (2 == nCountChild)
                )
            )
            {
                numArg = 2;
            }
            else
            if ( (
                (3 == nCount )
                && (3 == nCountChild)
                )
            )
            {
                numArg = 3;
            }
            else
            if ( (
                (4 == nCount )
                && (4 == nCountChild)
                )
            )
            {
                numArg = 4;
            }

            if ( nCountChild < nArgCount )
            {
                char    buff[sizeof(TI_FINDCHILDREN_PARAMS)+nArgCount*sizeof(ULONG)*2];
                ZeroMemory( buff, sizeof(buff) );
                TI_FINDCHILDREN_PARAMS* pParam = reinterpret_cast<TI_FINDCHILDREN_PARAMS*>(buff);
                pParam->Count = nCountChild;
                pParam->Start = 0;
                {
                    const BOOL BRet = pThis->mSymGetTypeInfo( hProcess, dwModuleBase, pSymInfo->TypeIndex, TI_FINDCHILDREN, pParam );
                    if ( !BRet )
                    {
                        const DWORD dwErr = ::GetLastError();
                    }
                }

                for ( size_t index = 0; index < nCountChild; ++index )
                {
                    const ULONG typeIndex = pParam->ChildId[index];

                    DWORD   dwSymTag = 0;
                    {
                        const BOOL BRet = pThis->mSymGetTypeInfo( hProcess, dwModuleBase, typeIndex, TI_GET_SYMTAG, &dwSymTag );
                        if ( !BRet )
                        {
                            const DWORD dwErr = ::GetLastError();
                        }
                    }
                    if ( SymTagFunctionArgType == dwSymTag )
                    {
                        DWORD   dwTypeIndex = 0;
                        {
                            const BOOL BRet = pThis->mSymGetTypeInfo( hProcess, dwModuleBase, typeIndex, TI_GET_TYPEID, &dwTypeIndex );
                            if ( !BRet )
                            {
                                const DWORD dwErr = ::GetLastError();
                            }
                        }

                        DWORD   dwSymTagChild = 0;
                        {
                            const BOOL BRet = pThis->mSymGetTypeInfo( hProcess, dwModuleBase, dwTypeIndex, TI_GET_SYMTAG, &dwSymTagChild );
                            if ( !BRet )
                            {
                                const DWORD dwErr = ::GetLastError();
                            }
                        }
#if 1
                        if ( SymTagPointerType == dwSymTagChild )
                        {
                            DWORD   dwTypeIndexPointer = 0;
                            {
                                const BOOL BRet = pThis->mSymGetTypeInfo( hProcess, dwModuleBase, dwTypeIndex, TI_GET_TYPEID, &dwTypeIndexPointer );
                                if ( !BRet )
                                {
                                    const DWORD dwErr = ::GetLastError();
                                }
                            }

                            DWORD   dwSymTag = 0;
                            {
                                const BOOL BRet = pThis->mSymGetTypeInfo( hProcess, dwModuleBase, dwTypeIndexPointer, TI_GET_SYMTAG, &dwSymTag );
                                if ( !BRet )
                                {
                                    const DWORD dwErr = ::GetLastError();
                                }
                            }
                            if ( SymTagBaseType == dwSymTag )
                            {
                                DWORD   dwBaseType = 0;
                                {
                                    const BOOL BRet = pThis->mSymGetTypeInfo( hProcess, dwModuleBase, dwTypeIndexPointer, TI_GET_BASETYPE, &dwBaseType );
                                    if ( !BRet )
                                    {
                                        const DWORD dwErr = ::GetLastError();
                                    }
                                }

                                switch ( dwBaseType )
                                {
                                case 1: // btVoid
                                    if ( 0 == index )
                                    {
                                        enmTypeArg0 = kDataTypeVoidPointer;
                                    }
                                    else
                                    if ( 1 == index )
                                    {
                                        enmTypeArg1 = kDataTypeVoidPointer;
                                    }
                                    else
                                    if ( 2 == index )
                                    {
                                        enmTypeArg2 = kDataTypeVoidPointer;
                                    }
                                    else
                                    if ( 3 == index )
                                    {
                                        enmTypeArg3 = kDataTypeVoidPointer;
                                    }
                                    break;
                                case 2: // btChar
                                    if ( 0 == index )
                                    {
                                        enmTypeArg0 = kDataTypeCharPointer;
                                    }
                                    else
                                    if ( 1 == index )
                                    {
                                        enmTypeArg1 = kDataTypeCharPointer;
                                    }
                                    else
                                    if ( 2 == index )
                                    {
                                        enmTypeArg2 = kDataTypeCharPointer;
                                    }
                                    else
                                    if ( 3 == index )
                                    {
                                        enmTypeArg3 = kDataTypeCharPointer;
                                    }
                                    break;
                                }
                            }
                            else
                            if ( SymTagUDT == dwSymTag )
                            {
                                if ( 0 == index )
                                {
                                    enmTypeArg0 = kDataTypeUDTPointer;
                                }
                                else
                                if ( 1 == index )
                                {
                                    enmTypeArg1 = kDataTypeUDTPointer;
                                    dwTypeIndexArg1 = dwTypeIndexPointer;
                                }
                                else
                                if ( 2 == index )
                                {
                                    enmTypeArg2 = kDataTypeUDTPointer;
                                }
                                else
                                if ( 3 == index )
                                {
                                    enmTypeArg3 = kDataTypeUDTPointer;
                                }
                            }

                        }
                        else
                        if ( SymTagBaseType == dwSymTagChild )
                        {
                            DWORD   dwBaseType = 0;
                            {
                                const BOOL BRet = pThis->mSymGetTypeInfo( hProcess, dwModuleBase, dwTypeIndex, TI_GET_BASETYPE, &dwBaseType );
                                if ( !BRet )
                                {
                                    const DWORD dwErr = ::GetLastError();
                                }
                            }

                            switch ( dwBaseType )
                            {
                            case 1: // btVoid
                                if ( 0 == index )
                                {
                                    enmTypeArg0 = kDataTypeVoid;
                                }
                                else
                                if ( 1 == index )
                                {
                                    enmTypeArg1 = kDataTypeVoid;
                                }
                                else
                                if ( 2 == index )
                                {
                                    enmTypeArg2 = kDataTypeVoid;
                                }
                                else
                                if ( 3 == index )
                                {
                                    enmTypeArg3 = kDataTypeVoid;
                                }
                                break;
                            case 6: // btInt
                                if ( 0 == index )
                                {
                                    ULONG64 length = 0;
                                    {
                                        const BOOL BRet = pThis->mSymGetTypeInfo( hProcess, dwModuleBase, dwTypeIndex, TI_GET_LENGTH, &length );
                                        if ( !BRet )
                                        {
                                            const DWORD dwErr = ::GetLastError();
                                        }
                                    }

                                    enmTypeArg0 = kDataTypeInt;
                                }
                                else
                                if ( 1 == index )
                                {
                                    ULONG64 length = 0;
                                    {
                                        const BOOL BRet = pThis->mSymGetTypeInfo( hProcess, dwModuleBase, dwTypeIndex, TI_GET_LENGTH, &length );
                                        if ( !BRet )
                                        {
                                            const DWORD dwErr = ::GetLastError();
                                        }
                                    }

                                    enmTypeArg1 = kDataTypeInt;
                                }
                                else
                                if ( 2 == index )
                                {
                                    ULONG64 length = 0;
                                    {
                                        const BOOL BRet = pThis->mSymGetTypeInfo( hProcess, dwModuleBase, dwTypeIndex, TI_GET_LENGTH, &length );
                                        if ( !BRet )
                                        {
                                            const DWORD dwErr = ::GetLastError();
                                        }
                                    }

                                    enmTypeArg2 = kDataTypeInt;
                                }
                                else
                                if ( 3 == index )
                                {
                                    ULONG64 length = 0;
                                    {
                                        const BOOL BRet = pThis->mSymGetTypeInfo( hProcess, dwModuleBase, dwTypeIndex, TI_GET_LENGTH, &length );
                                        if ( !BRet )
                                        {
                                            const DWORD dwErr = ::GetLastError();
                                        }
                                    }

                                    enmTypeArg3 = kDataTypeInt;
                                }
                                break;
                            case 7: // btUInt
                                if ( 0 == index )
                                {
                                    ULONG64 length = 0;
                                    {
                                        const BOOL BRet = pThis->mSymGetTypeInfo( hProcess, dwModuleBase, dwTypeIndex, TI_GET_LENGTH, &length );
                                        if ( !BRet )
                                        {
                                            const DWORD dwErr = ::GetLastError();
                                        }
                                    }

                                    enmTypeArg0 = kDataTypeUInt;
                                }
                                else
                                if ( 1 == index )
                                {
                                    ULONG64 length = 0;
                                    {
                                        const BOOL BRet = pThis->mSymGetTypeInfo( hProcess, dwModuleBase, dwTypeIndex, TI_GET_LENGTH, &length );
                                        if ( !BRet )
                                        {
                                            const DWORD dwErr = ::GetLastError();
                                        }
                                    }

                                    enmTypeArg1 = kDataTypeUInt;
                                }
                                else
                                if ( 2 == index )
                                {
                                    ULONG64 length = 0;
                                    {
                                        const BOOL BRet = pThis->mSymGetTypeInfo( hProcess, dwModuleBase, dwTypeIndex, TI_GET_LENGTH, &length );
                                        if ( !BRet )
                                        {
                                            const DWORD dwErr = ::GetLastError();
                                        }
                                    }

                                    enmTypeArg2 = kDataTypeUInt;
                                }
                                else
                                if ( 3 == index )
                                {
                                    ULONG64 length = 0;
                                    {
                                        const BOOL BRet = pThis->mSymGetTypeInfo( hProcess, dwModuleBase, dwTypeIndex, TI_GET_LENGTH, &length );
                                        if ( !BRet )
                                        {
                                            const DWORD dwErr = ::GetLastError();
                                        }
                                    }

                                    enmTypeArg3 = kDataTypeUInt;
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
            if ( 1 <= numArg && numArg <= 4 )
            {
                const char*     strFuncReturn = "";
                switch ( enmTypeReturn )
                {
                case kDataTypeVoid:
                    strFuncReturn = "void";
                    break;
                case kDataTypeVoidPointer:
                    strFuncReturn = "void*";
                    break;
                case kDataTypeNone:
                case kDataTypeUInt:
                    break;
                default:
                    break;
                }

                const char*     strFuncArg0 = "";
                switch ( enmTypeArg0 )
                {
                case kDataTypeVoid:
                    strFuncArg0 = "void";
                    break;
                case kDataTypeVoidPointer:
                    strFuncArg0 = "void*";
                    break;
                case kDataTypeUInt:
                    strFuncArg0 = "size_t";
                    break;
                case kDataTypeNone:
                    break;
                default:
                    break;
                }

                const char*     strFuncArg1 = "";
                switch ( enmTypeArg1 )
                {
                case kDataTypeVoid:
                    strFuncArg1 = "void";
                    break;
                case kDataTypeVoidPointer:
                    strFuncArg1 = "void*";
                    break;
                case kDataTypeChar:
                    strFuncArg1 = "char";
                    break;
                case kDataTypeCharPointer:
                    strFuncArg1 = "char*";
                    break;
                case kDataTypeUInt:
                    strFuncArg1 = "size_t";
                    break;
                case kDataTypeUDTPointer:
                    strFuncArg1 = "";
                case kDataTypeNone:
                    break;
                default:
                    break;
                }

                const char*     strFuncArg2 = "";
                switch ( enmTypeArg2 )
                {
                case kDataTypeVoid:
                    strFuncArg2 = "void";
                    break;
                case kDataTypeVoidPointer:
                    strFuncArg2 = "void*";
                    break;
                case kDataTypeChar:
                    strFuncArg2 = "char";
                    break;
                case kDataTypeCharPointer:
                    strFuncArg2 = "char*";
                    break;
                case kDataTypeInt:
                    strFuncArg2 = "int";
                    break;
                case kDataTypeUInt:
                    strFuncArg2 = "size_t";
                    break;
                case kDataTypeUDTPointer:
                    strFuncArg2 = "";
                case kDataTypeNone:
                    break;
                default:
                    break;
                }

                const char*     strFuncArg3 = "";
                switch ( enmTypeArg3 )
                {
                case kDataTypeVoid:
                    strFuncArg3 = "void";
                    break;
                case kDataTypeVoidPointer:
                    strFuncArg3 = "void*";
                    break;
                case kDataTypeChar:
                    strFuncArg3 = "char";
                    break;
                case kDataTypeCharPointer:
                    strFuncArg3 = "char*";
                    break;
                case kDataTypeInt:
                    strFuncArg3 = "int";
                    break;
                case kDataTypeUInt:
                    strFuncArg3 = "size_t";
                    break;
                case kDataTypeUDTPointer:
                    strFuncArg3 = "";
                case kDataTypeNone:
                    break;
                default:
                    break;
                }

                char    buff[1024];
                buff[0] = '\0';
                if ( 1 == numArg )
                {
                    ::wsprintfA( buff, "%p %08x %4u %4u %s %s(%s)\n"
                        , (void *)pSymInfo->Address, pSymInfo->Index, pSymInfo->Tag, pSymInfo->Index
                        , strFuncReturn
                        , pSymInfo->Name
                        , strFuncArg0
                        );
                }
                else
                if ( 2 == numArg )
                {
                    if ( kDataTypeUDTPointer == enmTypeArg1 )
                    {
                        wchar_t*    pStrArg1 = NULL;
                        {
                            const BOOL BRet = pThis->mSymGetTypeInfo( hProcess, dwModuleBase, dwTypeIndexArg1, TI_GET_SYMNAME, &pStrArg1 );
                            if ( !BRet )
                            {
                                const DWORD dwErr = ::GetLastError();
                            }
                        }

                        ::wsprintfA( buff, "%p %08x %4u %4u %s %s(%s,%S*)\n"
                            , (void *)pSymInfo->Address, pSymInfo->Index, pSymInfo->Tag, pSymInfo->Index
                            , strFuncReturn
                            , pSymInfo->Name
                            , strFuncArg0
                            , pStrArg1
                            );

                        if ( NULL != pStrArg1 )
                        {
                            const HLOCAL pResult = ::LocalFree( pStrArg1 );
                            assert( NULL == pResult );
                        }

                    }
                    else
                    {
                        ::wsprintfA( buff, "%p %08x %4u %4u %s %s(%s,%s)\n"
                            , (void *)pSymInfo->Address, pSymInfo->Index, pSymInfo->Tag, pSymInfo->Index
                            , strFuncReturn
                            , pSymInfo->Name
                            , strFuncArg0
                            , strFuncArg1
                            );
                    }
                }
                else
                if ( 3 == numArg )
                {
                    ::wsprintfA( buff, "%p %08x %4u %4u %s %s(%s,%s,%s)\n"
                        , (void *)pSymInfo->Address, pSymInfo->Index, pSymInfo->Tag, pSymInfo->Index
                        , strFuncReturn
                        , pSymInfo->Name
                        , strFuncArg0
                        , strFuncArg1
                        , strFuncArg2
                        );
                }
                else
                if ( 4 == numArg )
                {
                    ::wsprintfA( buff, "%p %08x %4u %4u %s %s(%s,%s,%s,%s)\n"
                        , (void *)pSymInfo->Address, pSymInfo->Index, pSymInfo->Tag, pSymInfo->Index
                        , strFuncReturn
                        , pSymInfo->Name
                        , strFuncArg0
                        , strFuncArg1
                        , strFuncArg2
                        , strFuncArg3
                        );
                }

                ::OutputDebugStringA( buff );
            }
#endif

            if ( 1 == numArg )
            {
                const DWORD64   addr = pSymInfo->Address - dwModuleBase;
                switch ( enmOperation )
                {
                case kOperationGlobalNew:
                case kOperationClassNew:
                    if ( kDataTypeVoidPointer == enmTypeReturn )
                    {
                        if ( kDataTypeUInt == enmTypeArg0 )
                        {
                            DebugSymbol::FuncInfo   funcInfo;
                            funcInfo.dwAddr = addr;
                            funcInfo.size = pSymInfo->Size;
                            funcInfo.isCRT = false;

                            switch ( enmOperation )
                            {
                            case kOperationGlobalNew:
                                {
                                    std::pair<std::map<DWORD64,DebugSymbol::FuncInfo>::iterator,bool> ret = 
                                    pThis->mGlobalReplacements[DebugSymbol::kIndexOperationNew].insert(
                                        std::pair<DWORD64,DebugSymbol::FuncInfo>( addr, funcInfo )
                                        );
                                    assert( true == ret.second );
                                }
                                break;
                            case kOperationClassNew:
                                {
                                    std::pair<std::map<DWORD64,DebugSymbol::FuncInfo>::iterator,bool> ret = 
                                    pThis->mClassReplacements[DebugSymbol::kIndexOperationNew].insert(
                                        std::pair<DWORD64,DebugSymbol::FuncInfo>( addr, funcInfo )
                                        );
                                    assert( true == ret.second );
                                }
                                break;
                            default:
                                assert( false );
                                break;
                            }
                        }
                    }
                    break;
                case kOperationGlobalNewArray:
                case kOperationClassNewArray:
                    if ( kDataTypeVoidPointer == enmTypeReturn )
                    {
                        if ( kDataTypeUInt == enmTypeArg0 )
                        {
                            DebugSymbol::FuncInfo   funcInfo;
                            funcInfo.dwAddr = addr;
                            funcInfo.size = pSymInfo->Size;
                            funcInfo.isCRT = false;

                            switch ( enmOperation )
                            {
                            case kOperationGlobalNewArray:
                                {
                                    std::pair<std::map<DWORD64,DebugSymbol::FuncInfo >::iterator,bool> ret = 
                                    pThis->mGlobalReplacements[DebugSymbol::kIndexOperationNewArray].insert(
                                        std::pair<DWORD64,DebugSymbol::FuncInfo>( addr, funcInfo )
                                        );
                                    assert( true == ret.second );
                                }
                                break;
                            case kOperationClassNewArray:
                                {
                                    std::pair<std::map<DWORD64,DebugSymbol::FuncInfo >::iterator,bool> ret = 
                                    pThis->mClassReplacements[DebugSymbol::kIndexOperationNewArray].insert(
                                        std::pair<DWORD64,DebugSymbol::FuncInfo>( addr, funcInfo )
                                        );
                                    assert( true == ret.second );
                                }
                                break;
                            default:
                                assert( false );
                                break;
                            }
                        }
                    }
                    break;
                case kOperationGlobalDelete:
                case kOperationClassDelete:
                    if ( kDataTypeVoid == enmTypeReturn )
                    {
                        if ( kDataTypeVoidPointer == enmTypeArg0 )
                        {
                            DebugSymbol::FuncInfo   funcInfo;
                            funcInfo.dwAddr = addr;
                            funcInfo.size = pSymInfo->Size;
                            funcInfo.isCRT = false;

                            switch ( enmOperation )
                            {
                            case kOperationGlobalDelete:
                                {
                                    std::pair<std::map<DWORD64,DebugSymbol::FuncInfo >::iterator,bool> ret = 
                                    pThis->mGlobalReplacements[DebugSymbol::kIndexOperationDelete].insert(
                                        std::pair<DWORD64,DebugSymbol::FuncInfo>( addr, funcInfo )
                                        );
                                    assert( true == ret.second );
                                }
                                break;
                            case kOperationClassDelete:
                                {
                                    std::pair<std::map<DWORD64,DebugSymbol::FuncInfo >::iterator,bool> ret = 
                                    pThis->mClassReplacements[DebugSymbol::kIndexOperationDelete].insert(
                                        std::pair<DWORD64,DebugSymbol::FuncInfo>( addr, funcInfo )
                                        );
                                    assert( true == ret.second );
                                }
                                break;
                            default:
                                assert( false );
                                break;
                            }
                        }
                    }
                    break;
                case kOperationGlobalDeleteArray:
                case kOperationClassDeleteArray:
                    if ( kDataTypeVoid == enmTypeReturn )
                    {
                        if ( kDataTypeVoidPointer == enmTypeArg0 )
                        {
                            DebugSymbol::FuncInfo   funcInfo;
                            funcInfo.dwAddr = addr;
                            funcInfo.size = pSymInfo->Size;
                            funcInfo.isCRT = false;

                            switch ( enmOperation )
                            {
                            case kOperationGlobalDeleteArray:
                                {
                                    std::pair<std::map<DWORD64,DebugSymbol::FuncInfo >::iterator,bool> ret = 
                                    pThis->mGlobalReplacements[DebugSymbol::kIndexOperationDeleteArray].insert(
                                        std::pair<DWORD64,DebugSymbol::FuncInfo>( addr, funcInfo )
                                        );
                                    assert( true == ret.second );
                                }
                                break;
                            case kOperationClassDeleteArray:
                                {
                                    std::pair<std::map<DWORD64,DebugSymbol::FuncInfo >::iterator,bool> ret = 
                                    pThis->mClassReplacements[DebugSymbol::kIndexOperationDeleteArray].insert(
                                        std::pair<DWORD64,DebugSymbol::FuncInfo>( addr, funcInfo )
                                        );
                                    assert( true == ret.second );
                                }
                                break;
                            default:
                                assert( false );
                                break;
                            }
                        }
                    }
                    break;

                default:
                    assert( false );
                    break;
                }
            }

            if ( 2 == numArg )
            {
                const DWORD64   addr = pSymInfo->Address - dwModuleBase;
                switch ( enmOperation )
                {
                case kOperationGlobalNew:
                case kOperationGlobalNewArray:
                case kOperationClassNew:
                case kOperationClassNewArray:
                    // ignore
                    break;

                case kOperationGlobalDelete:
                case kOperationClassDelete:
                    if ( kDataTypeVoid == enmTypeReturn )
                    {
                        if (
                            kDataTypeVoidPointer == enmTypeArg0
                            && kDataTypeUInt == enmTypeArg1
                        )
                        {
                            DebugSymbol::FuncInfo   funcInfo;
                            funcInfo.dwAddr = addr;
                            funcInfo.size = pSymInfo->Size;
                            funcInfo.isCRT = false;

                            switch ( enmOperation )
                            {
                            case kOperationGlobalDelete:
                                {
                                    std::pair<std::map<DWORD64,DebugSymbol::FuncInfo >::iterator,bool> ret = 
                                    pThis->mGlobalReplacements[DebugSymbol::kIndexOperationDeleteSize].insert(
                                        std::pair<DWORD64,DebugSymbol::FuncInfo>( addr, funcInfo )
                                        );
                                    assert( true == ret.second );
                                }
                                break;
                            case kOperationClassDelete:
                                {
                                    std::pair<std::map<DWORD64,DebugSymbol::FuncInfo >::iterator,bool> ret = 
                                    pThis->mClassReplacements[DebugSymbol::kIndexOperationDeleteSize].insert(
                                        std::pair<DWORD64,DebugSymbol::FuncInfo>( addr, funcInfo )
                                        );
                                    assert( true == ret.second );
                                }
                                break;
                            default:
                                assert( false );
                                break;
                            }
                        }
                    }
                    break;

                case kOperationGlobalDeleteArray:
                case kOperationClassDeleteArray:
                    if ( kDataTypeVoid == enmTypeReturn )
                    {
                        if (
                            kDataTypeVoidPointer == enmTypeArg0
                            && kDataTypeUInt == enmTypeArg1
                        )
                        {
                            DebugSymbol::FuncInfo   funcInfo;
                            funcInfo.dwAddr = addr;
                            funcInfo.size = pSymInfo->Size;
                            funcInfo.isCRT = false;

                            switch ( enmOperation )
                            {
                            case kOperationGlobalDeleteArray:
                                {
                                    std::pair<std::map<DWORD64,DebugSymbol::FuncInfo >::iterator,bool> ret = 
                                    pThis->mGlobalReplacements[DebugSymbol::kIndexOperationDeleteArraySize].insert(
                                        std::pair<DWORD64,DebugSymbol::FuncInfo>( addr, funcInfo )
                                        );
                                    assert( true == ret.second );
                                }
                                break;
                            case kOperationClassDeleteArray:
                                {
                                    std::pair<std::map<DWORD64,DebugSymbol::FuncInfo >::iterator,bool> ret = 
                                    pThis->mClassReplacements[DebugSymbol::kIndexOperationDeleteArraySize].insert(
                                        std::pair<DWORD64,DebugSymbol::FuncInfo>( addr, funcInfo )
                                        );
                                    assert( true == ret.second );
                                }
                                break;
                            default:
                                assert( false );
                                break;
                            }
                        }
                    }
                    break;

                default:
                    assert( false );
                    break;
                }
            }

            if ( 3 == numArg )
            {
                const DWORD64   addr = pSymInfo->Address - dwModuleBase;
                switch ( enmOperation )
                {
                case kOperationGlobalNew:
                case kOperationClassNew:
                    if ( kDataTypeVoidPointer == enmTypeReturn )
                    {
                        if (
                            (kDataTypeUInt == enmTypeArg0)
                            && (kDataTypeUDTPointer != enmTypeArg1)
                        )
                        {
                            DebugSymbol::FuncInfo   funcInfo;
                            funcInfo.dwAddr = addr;
                            funcInfo.size = pSymInfo->Size;
                            funcInfo.isCRT = false;

                            switch ( enmOperation )
                            {
                            case kOperationGlobalNew:
                                {
                                    std::pair<std::map<DWORD64,DebugSymbol::FuncInfo>::iterator,bool> ret = 
                                    pThis->mGlobalReplacements[DebugSymbol::kIndexOperationNewArg3].insert(
                                        std::pair<DWORD64,DebugSymbol::FuncInfo>( addr, funcInfo )
                                        );
                                    assert( true == ret.second );
                                }
                                break;
                            case kOperationClassNew:
                                {
                                    std::pair<std::map<DWORD64,DebugSymbol::FuncInfo>::iterator,bool> ret = 
                                    pThis->mClassReplacements[DebugSymbol::kIndexOperationNewArg3].insert(
                                        std::pair<DWORD64,DebugSymbol::FuncInfo>( addr, funcInfo )
                                        );
                                    assert( true == ret.second );
                                }
                                break;
                            default:
                                assert( false );
                                break;
                            }
                        }
                    }
                    break;
                case kOperationGlobalNewArray:
                case kOperationClassNewArray:
                    if ( kDataTypeVoidPointer == enmTypeReturn )
                    {
                        if (
                            (kDataTypeUInt == enmTypeArg0)
                            && (kDataTypeUDTPointer != enmTypeArg1)
                        )
                        {
                            DebugSymbol::FuncInfo   funcInfo;
                            funcInfo.dwAddr = addr;
                            funcInfo.size = pSymInfo->Size;
                            funcInfo.isCRT = false;

                            switch ( enmOperation )
                            {
                            case kOperationGlobalNewArray:
                                {
                                    std::pair<std::map<DWORD64,DebugSymbol::FuncInfo >::iterator,bool> ret = 
                                    pThis->mGlobalReplacements[DebugSymbol::kIndexOperationNewArrayArg3].insert(
                                        std::pair<DWORD64,DebugSymbol::FuncInfo>( addr, funcInfo )
                                        );
                                    assert( true == ret.second );
                                }
                                break;
                            case kOperationClassNewArray:
                                {
                                    std::pair<std::map<DWORD64,DebugSymbol::FuncInfo >::iterator,bool> ret = 
                                    pThis->mClassReplacements[DebugSymbol::kIndexOperationNewArrayArg3].insert(
                                        std::pair<DWORD64,DebugSymbol::FuncInfo>( addr, funcInfo )
                                        );
                                    assert( true == ret.second );
                                }
                                break;
                            default:
                                assert( false );
                                break;
                            }
                        }
                    }
                    break;
                case kOperationGlobalDelete:
                case kOperationClassDelete:
                    if ( kDataTypeVoid == enmTypeReturn )
                    {
                        if (
                            (kDataTypeVoidPointer == enmTypeArg0)
                            && (kDataTypeUDTPointer != enmTypeArg1)
                        )
                        {
                            DebugSymbol::FuncInfo   funcInfo;
                            funcInfo.dwAddr = addr;
                            funcInfo.size = pSymInfo->Size;
                            funcInfo.isCRT = false;

                            switch ( enmOperation )
                            {
                            case kOperationGlobalDelete:
                                {
                                    std::pair<std::map<DWORD64,DebugSymbol::FuncInfo >::iterator,bool> ret = 
                                    pThis->mGlobalReplacements[DebugSymbol::kIndexOperationDeleteArg3].insert(
                                        std::pair<DWORD64,DebugSymbol::FuncInfo>( addr, funcInfo )
                                        );
                                    assert( true == ret.second );
                                }
                                break;
                            case kOperationClassDelete:
                                {
                                    std::pair<std::map<DWORD64,DebugSymbol::FuncInfo >::iterator,bool> ret = 
                                    pThis->mClassReplacements[DebugSymbol::kIndexOperationDeleteArg3].insert(
                                        std::pair<DWORD64,DebugSymbol::FuncInfo>( addr, funcInfo )
                                        );
                                    assert( true == ret.second );
                                }
                                break;
                            default:
                                assert( false );
                                break;
                            }
                        }
                    }
                    break;
                case kOperationGlobalDeleteArray:
                case kOperationClassDeleteArray:
                    if ( kDataTypeVoid == enmTypeReturn )
                    {
                        if (
                            (kDataTypeVoidPointer == enmTypeArg0)
                            && (kDataTypeUDTPointer != enmTypeArg1)
                        )
                        {
                            DebugSymbol::FuncInfo   funcInfo;
                            funcInfo.dwAddr = addr;
                            funcInfo.size = pSymInfo->Size;
                            funcInfo.isCRT = false;

                            switch ( enmOperation )
                            {
                            case kOperationGlobalDeleteArray:
                                {
                                    std::pair<std::map<DWORD64,DebugSymbol::FuncInfo >::iterator,bool> ret = 
                                    pThis->mGlobalReplacements[DebugSymbol::kIndexOperationDeleteArrayArg3].insert(
                                        std::pair<DWORD64,DebugSymbol::FuncInfo>( addr, funcInfo )
                                        );
                                    assert( true == ret.second );
                                }
                                break;
                            case kOperationClassDeleteArray:
                                {
                                    std::pair<std::map<DWORD64,DebugSymbol::FuncInfo >::iterator,bool> ret = 
                                    pThis->mClassReplacements[DebugSymbol::kIndexOperationDeleteArrayArg3].insert(
                                        std::pair<DWORD64,DebugSymbol::FuncInfo>( addr, funcInfo )
                                        );
                                    assert( true == ret.second );
                                }
                                break;
                            default:
                                assert( false );
                                break;
                            }
                        }
                    }
                    break;

                default:
                    assert( false );
                    break;
                }
            }

            if ( 4 == numArg )
            {
                const DWORD64   addr = pSymInfo->Address - dwModuleBase;
                switch ( enmOperation )
                {
                case kOperationGlobalNew:
                case kOperationClassNew:
                    if ( kDataTypeVoidPointer == enmTypeReturn )
                    {
                        if (
                            (kDataTypeUInt == enmTypeArg0)
                            && (kDataTypeUDTPointer != enmTypeArg1)
                        )
                        {
                            DebugSymbol::FuncInfo   funcInfo;
                            funcInfo.dwAddr = addr;
                            funcInfo.size = pSymInfo->Size;
                            funcInfo.isCRT = false;

                            switch ( enmOperation )
                            {
                            case kOperationGlobalNew:
                                {
                                    std::pair<std::map<DWORD64,DebugSymbol::FuncInfo>::iterator,bool> ret = 
                                    pThis->mGlobalReplacements[DebugSymbol::kIndexOperationNewArg4].insert(
                                        std::pair<DWORD64,DebugSymbol::FuncInfo>( addr, funcInfo )
                                        );
                                    assert( true == ret.second );
                                }
                                break;
                            case kOperationClassNew:
                                {
                                    std::pair<std::map<DWORD64,DebugSymbol::FuncInfo>::iterator,bool> ret = 
                                    pThis->mClassReplacements[DebugSymbol::kIndexOperationNewArg4].insert(
                                        std::pair<DWORD64,DebugSymbol::FuncInfo>( addr, funcInfo )
                                        );
                                    assert( true == ret.second );
                                }
                                break;
                            default:
                                assert( false );
                                break;
                            }
                        }
                    }
                    break;
                case kOperationGlobalNewArray:
                case kOperationClassNewArray:
                    if ( kDataTypeVoidPointer == enmTypeReturn )
                    {
                        if (
                            (kDataTypeUInt == enmTypeArg0)
                            && (kDataTypeUDTPointer != enmTypeArg1)
                        )
                        {
                            DebugSymbol::FuncInfo   funcInfo;
                            funcInfo.dwAddr = addr;
                            funcInfo.size = pSymInfo->Size;
                            funcInfo.isCRT = false;

                            switch ( enmOperation )
                            {
                            case kOperationGlobalNewArray:
                                {
                                    std::pair<std::map<DWORD64,DebugSymbol::FuncInfo >::iterator,bool> ret = 
                                    pThis->mGlobalReplacements[DebugSymbol::kIndexOperationNewArrayArg4].insert(
                                        std::pair<DWORD64,DebugSymbol::FuncInfo>( addr, funcInfo )
                                        );
                                    assert( true == ret.second );
                                }
                                break;
                            case kOperationClassNewArray:
                                {
                                    std::pair<std::map<DWORD64,DebugSymbol::FuncInfo >::iterator,bool> ret = 
                                    pThis->mClassReplacements[DebugSymbol::kIndexOperationNewArrayArg4].insert(
                                        std::pair<DWORD64,DebugSymbol::FuncInfo>( addr, funcInfo )
                                        );
                                    assert( true == ret.second );
                                }
                                break;
                            default:
                                assert( false );
                                break;
                            }
                        }
                    }
                    break;
                case kOperationGlobalDelete:
                case kOperationClassDelete:
                    if ( kDataTypeVoid == enmTypeReturn )
                    {
                        if (
                            (kDataTypeVoidPointer == enmTypeArg0)
                            && (kDataTypeUDTPointer != enmTypeArg1)
                        )
                        {
                            DebugSymbol::FuncInfo   funcInfo;
                            funcInfo.dwAddr = addr;
                            funcInfo.size = pSymInfo->Size;
                            funcInfo.isCRT = false;

                            switch ( enmOperation )
                            {
                            case kOperationGlobalDelete:
                                {
                                    std::pair<std::map<DWORD64,DebugSymbol::FuncInfo >::iterator,bool> ret = 
                                    pThis->mGlobalReplacements[DebugSymbol::kIndexOperationDeleteArg4].insert(
                                        std::pair<DWORD64,DebugSymbol::FuncInfo>( addr, funcInfo )
                                        );
                                    assert( true == ret.second );
                                }
                                break;
                            case kOperationClassDelete:
                                {
                                    std::pair<std::map<DWORD64,DebugSymbol::FuncInfo >::iterator,bool> ret = 
                                    pThis->mClassReplacements[DebugSymbol::kIndexOperationDeleteArg4].insert(
                                        std::pair<DWORD64,DebugSymbol::FuncInfo>( addr, funcInfo )
                                        );
                                    assert( true == ret.second );
                                }
                                break;
                            default:
                                assert( false );
                                break;
                            }
                        }
                    }
                    break;
                case kOperationGlobalDeleteArray:
                case kOperationClassDeleteArray:
                    if ( kDataTypeVoid == enmTypeReturn )
                    {
                        if (
                            (kDataTypeVoidPointer == enmTypeArg0)
                            && (kDataTypeUDTPointer != enmTypeArg1)
                        )
                        {
                            DebugSymbol::FuncInfo   funcInfo;
                            funcInfo.dwAddr = addr;
                            funcInfo.size = pSymInfo->Size;
                            funcInfo.isCRT = false;

                            switch ( enmOperation )
                            {
                            case kOperationGlobalDeleteArray:
                                {
                                    std::pair<std::map<DWORD64,DebugSymbol::FuncInfo >::iterator,bool> ret = 
                                    pThis->mGlobalReplacements[DebugSymbol::kIndexOperationDeleteArrayArg4].insert(
                                        std::pair<DWORD64,DebugSymbol::FuncInfo>( addr, funcInfo )
                                        );
                                    assert( true == ret.second );
                                }
                                break;
                            case kOperationClassDeleteArray:
                                {
                                    std::pair<std::map<DWORD64,DebugSymbol::FuncInfo >::iterator,bool> ret = 
                                    pThis->mClassReplacements[DebugSymbol::kIndexOperationDeleteArrayArg4].insert(
                                        std::pair<DWORD64,DebugSymbol::FuncInfo>( addr, funcInfo )
                                        );
                                    assert( true == ret.second );
                                }
                                break;
                            default:
                                assert( false );
                                break;
                            }
                        }
                    }
                    break;

                default:
                    assert( false );
                    break;
                }
            }

#if defined(_DEBUG)
            ::OutputDebugStringA( "" );
#endif // defined(_DEBUG)



        } // SymTagFunctionType

    }

    return TRUE;
}


//static
BOOL
CALLBACK
DebugSymbol::DebugSymbolImpl::symEnumLinesProc( PSRCCODEINFO pLineInfo, PVOID UserContext )
{
    SearchCRTSource* pSearch = reinterpret_cast<SearchCRTSource*>(UserContext);
    if ( NULL == pLineInfo )
    {
        return FALSE;
    }

#if 0
    {
        char    buff[2048];
        ::wsprintfA( buff, "%p %s(%u) %s\n", pLineInfo->Address, pLineInfo->FileName, pLineInfo->LineNumber, pLineInfo->Obj );
        ::OutputDebugStringA( buff );
    }
#endif

    if ( pLineInfo->Address == pSearch->addr )
    {
        if ( NULL != ::strstr( pLineInfo->FileName, "\\crt" ) )
        {
            if ( NULL != pSearch->File )
            {
                if ( NULL != ::strstr( pLineInfo->FileName, pSearch->File ) )
                {
                    pSearch->found = true;
#if 1
                    {
                        char    buff[2048];
                        ::wsprintfA( buff, " %p %s(%u) %s\n", pLineInfo->Address, pLineInfo->FileName, pLineInfo->LineNumber, pLineInfo->Obj );
                        ::OutputDebugStringA( buff );
                    }
#endif
                    return FALSE;
                }
            }
        }
    }

    return TRUE;
}





DebugSymbol::DebugSymbol()
{
    mImpl = NULL;
}

DebugSymbol::~DebugSymbol()
{
    this->term();
}

bool
DebugSymbol::term(void)
{
    this->mProcess.term();

    if ( NULL != mImpl )
    {
        delete mImpl;
        mImpl = NULL;
    }

    return true;
}


bool
DebugSymbol::init( const size_t processId )
{
    this->term();

    bool result = false;

    result = mProcess.init( processId );

    HANDLE hProcess = mProcess.getHandle();
    HMODULE hModule = mProcess.getHModule();

    if ( NULL != hProcess )
    {
        mImpl = new DebugSymbol::DebugSymbolImpl();
        if ( NULL != mImpl )
        {
            result = mImpl->init( hProcess );
        }
    }


    return result;
}


bool
DebugSymbol::findGlobalReplacements( void )
{
    if ( NULL == mImpl )
    {
        return false;
    }

    bool result = false;

    {
        const DWORD64   moduleBase = (DWORD64)mProcess.getModuleBase();
        const size_t    moduleSize = mProcess.getModuleSize();

        result = mImpl->findGlobalReplacements( moduleBase, moduleSize );
    }

    return result;
}


bool
DebugSymbol::getGlobalReplacementsCount( size_t count[kIndexOperationMax] ) const
{
    if ( NULL == mImpl )
    {
        return false;
    }

    const bool result = mImpl->getGlobalReplacementsCount( count );

    return result;
}

bool
DebugSymbol::getGlobalReplacementsRVA( const enumIndexOperation indexOperation, FuncInfo* funcArray ) const
{
    if ( NULL == mImpl )
    {
        return false;
    }

    const bool result = mImpl->getGlobalReplacementsRVA( indexOperation, funcArray );

    return result;
}

bool
DebugSymbol::isIncludeCRTNewArray( void ) const
{
    if ( NULL == mImpl )
    {
        return false;
    }

    const bool result = mImpl->isIncludeCRTNewArray();

    return result;
}

bool
DebugSymbol::getCRTNewArrayRVA( DebugSymbol::FuncInfo* funcInfo ) const
{
    if ( NULL == funcInfo )
    {
        return false;
    }

    bool result = true;

    if ( isIncludeCRTNewArray() )
    {
        const bool bRet = this->getGlobalReplacementsRVA( DebugSymbol::kIndexOperationNewArray, funcInfo );
        if ( !bRet )
        {
            result = false;
            ZeroMemory( funcInfo, sizeof(funcInfo) );
        }
    }

    return result;
}

bool
DebugSymbol::setCRTCPPisStaticLinked( const bool isStaticLinked )
{
    if ( NULL == mImpl )
    {
        return false;
    }

    const bool result = mImpl->setCRTCPPisStaticLinked( isStaticLinked );

    return result;
}

bool
DebugSymbol::getCRTCPPisStaticLinked( void ) const
{
    if ( NULL == mImpl )
    {
        return false;
    }

    const bool result = mImpl->getCRTCPPisStaticLinked();

    return result;
}



} // namespace kk

