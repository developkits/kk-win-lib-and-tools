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


#include <set>

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
typedef DWORD64 (WINAPI *PFN_SymGetModuleInfo64)(HANDLE hProcess, DWORD64 ModBase, PIMAGEHLP_MODULE64 ModuleInfo);


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
    getGlobalReplacementsRVA( const enumIndexOperation indexOperation, DWORD64* funcArray ) const;

protected:
    static
    BOOL
    CALLBACK
    symEnumSymbolsProc( PSYMBOL_INFO pSymInfo, ULONG SymbolSize, PVOID UserContext );

    bool
    matchSymTag( const ULONG Index, const enum SymTagEnum tag);

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

    //DWORD64         mGlobalReplacements[4];

protected:

    std::set<DWORD64>       mGlobalReplacements[DebugSymbol::kIndexOperationMax];

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

    //ZeroMemory( mGlobalReplacements, sizeof(mGlobalReplacements) );
    for ( size_t index = 0; index < kIndexOperationMax; ++index )
    {
        mGlobalReplacements[index].clear();
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
            const BOOL BRet = mSymEnumSymbols( mHandleProcess, moduleBase, NULL, symEnumSymbolsProc, this );
            if ( !BRet )
            {
                const DWORD dwErr = ::GetLastError();
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
DebugSymbol::DebugSymbolImpl::getGlobalReplacementsRVA( const enumIndexOperation indexOperation, DWORD64* funcArray ) const
{
    if ( NULL == funcArray )
    {
        return false;
    }

    const size_t count = mGlobalReplacements[indexOperation].size();
    size_t index = 0;
    for (
        std::set<DWORD64>::const_iterator it = mGlobalReplacements[indexOperation].begin();
        it != mGlobalReplacements[indexOperation].end();
        ++it
    )
    {
        assert( index < count );
        funcArray[index] = *it;
        index += 1;
    }

    return true;
}






static
enum enumDataType
{
    kDataTypeNone = 0
    , kDataTypeVoid = 1
    , kDataTypeVoidPointer = 2
    , kDataTypeUInt = 3
};

static
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
                                    break;
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

            if ( isArgSingle )
            {

                switch ( enmOperation )
                {
                case kOperationNew:
                    if ( kDataTypeVoidPointer == enmTypeReturn )
                    {
                        if ( kDataTypeUInt == enmTypeArg0 )
                        {
                            std::pair<std::set<DWORD64>::iterator,bool> ret = 
                            pThis->mGlobalReplacements[DebugSymbol::kIndexOperationNew].insert( pSymInfo->Address );
                            assert( true == ret.second );
                        }
                    }
                    break;
                case kOperationNewArray:
                    if ( kDataTypeVoidPointer == enmTypeReturn )
                    {
                        if ( kDataTypeUInt == enmTypeArg0 )
                        {
                            std::pair<std::set<DWORD64>::iterator,bool> ret = 
                            pThis->mGlobalReplacements[DebugSymbol::kIndexOperationNewArray].insert( pSymInfo->Address );
                            assert( true == ret.second );
                        }
                    }
                    break;
                case kOperationDelete:
                    if ( kDataTypeVoid == enmTypeReturn )
                    {
                        if ( kDataTypeVoidPointer == enmTypeArg0 )
                        {
                            std::pair<std::set<DWORD64>::iterator,bool> ret = 
                            pThis->mGlobalReplacements[DebugSymbol::kIndexOperationDelete].insert( pSymInfo->Address );
                            assert( true == ret.second );
                        }
                    }
                    break;
                case kOperationDeleteArray:
                    if ( kDataTypeVoid == enmTypeReturn )
                    {
                        if ( kDataTypeVoidPointer == enmTypeArg0 )
                        {
                            std::pair<std::set<DWORD64>::iterator,bool> ret = 
                            pThis->mGlobalReplacements[DebugSymbol::kIndexOperationDeleteArray].insert( pSymInfo->Address );
                            assert( true == ret.second );
                        }
                    }
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
DebugSymbol::getGlobalReplacementsRVA( const enumIndexOperation indexOperation, DWORD64* funcArray ) const
{
    if ( NULL == mImpl )
    {
        return false;
    }

    const bool result = mImpl->getGlobalReplacementsRVA( indexOperation, funcArray );

    return result;
}






} // namespace kk

