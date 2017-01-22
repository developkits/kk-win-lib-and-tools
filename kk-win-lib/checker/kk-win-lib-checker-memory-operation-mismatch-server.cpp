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

#include "../kk-win-lib-namedpipe.h"
#include "../kk-win-lib-event.h"

#include "../kk-win-lib-remote-process.h"
#include "../kk-win-lib-remote-memory.h"
#include "../kk-win-lib-pe-iat.h"
#include "../kk-win-lib-debug-symbol.h"

#include "kk-win-lib-checker-memory-operation-mismatch.h"
#include "kk-win-lib-checker-memory-operation-mismatch-server.h"

#include "kk-win-lib-checker-memory-operation-mismatch-internal.h"

#include <process.h>
#include <assert.h>

#include <map>
#include <stack>

namespace kk
{

namespace checker
{



MemoryOperationMismatchServer::MemoryOperationMismatchServer()
{
    mDoBreak = true;
    mNeedBreakDeallocNull = false;
    mNeedBreakAllocNull = false;
}

MemoryOperationMismatchServer::~MemoryOperationMismatchServer()
{
    this->term();
}

bool
MemoryOperationMismatchServer::term(void)
{
    bool result = false;

    result = mNamedPipe.close();

    this->serverStop();

    mDoBreak = true;
    mNeedBreakDeallocNull = false;
    mNeedBreakAllocNull = false;

    return result;
}


bool
MemoryOperationMismatchServer::init(const bool actionDoBreak)
{
    this->term();

    mDoBreak = actionDoBreak;

    this->mNamedPipe.openServer( szNamedPiepeName, kk::NamedPipe::kModeFullDuplex, 1, 1024 );

    return true;
}



bool
MemoryOperationMismatchServer::setBreakDeallocNull( const bool needBreak )
{
    bool oldValue = mNeedBreakDeallocNull;

    mNeedBreakDeallocNull = needBreak;

    return oldValue;
}









static
kk::Event       eventStop;

static
HANDLE          hThread = NULL;

bool
MemoryOperationMismatchServer::serverStop( void )
{
    eventStop.set();

    if ( NULL != hThread )
    {
        const DWORD dwRet = ::WaitForSingleObject( hThread, 3*1000 );
        if ( WAIT_OBJECT_0 != dwRet )
        {
            const DWORD dwErr = ::GetLastError();
            {
                const BOOL BRet = ::TerminateThread( hThread, 1 );
                if ( !BRet )
                {
                    const DWORD dwErr = ::GetLastError();
                }
            }
        }
    }

    if ( NULL != hThread )
    {
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

    eventStop.term();

    return true;
}

bool
MemoryOperationMismatchServer::serverStart( void )
{
    this->serverStop();

    eventStop.init();

    hThread = (HANDLE)::_beginthreadex( NULL, 0, this->threadServer, this, 0, NULL );

    const bool result = (NULL != hThread)?(true):(false);
    return result;
}

bool
MemoryOperationMismatchServer::serverWaitFinish( void )
{
    bool result = false;

    {
        const DWORD dwRet = ::WaitForSingleObject( hThread, INFINITE );
        if ( WAIT_OBJECT_0 == dwRet )
        {
            result = true;
        }
    }

    return result;
}






typedef std::pair<DWORD64,MemoryOperationMismatch::enumMemoryOperation> pairRecord;
typedef std::map<DWORD64,MemoryOperationMismatch::enumMemoryOperation>  mapRecord;

static
mapRecord       mapMemory;


typedef std::pair<DWORD64,std::stack<MemoryOperationMismatch::enumMemoryOperation>> pairRecordUser;
typedef std::map<DWORD64,std::stack<MemoryOperationMismatch::enumMemoryOperation>>  mapRecordUser;

static
mapRecordUser   mapMemoryUser;


#if 0
x64
    ??2@YAPEAX_K@Z
    void * operator new(unsigned __int64)

    ??_U@YAPEAX_K@Z
    void * operator new[](unsigned __int64)

    ??3@YAXPEAX@Z
    void operator delete(void *)

    ??_V@YAXPEAX@Z
    void operator delete[](void *)

x86
    ??2@YAPAXI@Z
    void * operator new(unsigned int)

    ??3@YAXPAX@Z
    void operator delete(void *)

    ??_U@YAPAXI@Z
    void * operator new[](unsigned int)

    ??_V@YAXPAX@Z
    void operator delete[](void *)
#endif

static
const char*      sFuncName_New = "??2@YAPAXI@Z";
static
const char*      sFuncName_NewArray = "??_U@YAPAXI@Z";
static
const char*      sFuncName_Delete = "??3@YAXPAX@Z";
static
const char*      sFuncName_DeleteArray = "??_V@YAXPAX@Z";

static
const char*      sFuncName_New64 = "??2@YAPEAX_K@Z";
static
const char*      sFuncName_NewArray64 = "??_U@YAPEAX_K@Z";
static
const char*      sFuncName_Delete64 = "??3@YAXPEAX@Z";
static
const char*      sFuncName_DeleteArray64 = "??_V@YAXPEAX@Z";

static
void
fetchFunc( dataCRTModule& data, const size_t funcCount, const kk::PEIAT::IMPORT_FUNC* funcFile )
{
    for ( size_t indexFunc = 0; indexFunc < funcCount; ++indexFunc )
    {
        const kk::PEIAT::IMPORT_FUNC& func = funcFile[indexFunc];
        if ( 0 == ::lstrcmpA( func.nameFunc, "malloc" ) )
        {
            data.dwMalloc = (DWORD64)func.addrRef;
        }
        else
        if ( 0 == ::lstrcmpA( func.nameFunc, "free" ) )
        {
            data.dwFree = (DWORD64)func.addrRef;
        }
        else
        if ( 0 == ::lstrcmpA( func.nameFunc, "calloc" ) )
        {
            data.dwCalloc = (DWORD64)func.addrRef;
        }
        else
        if ( 0 == ::lstrcmpA( func.nameFunc, "realloc" ) )
        {
            data.dwRealloc = (DWORD64)func.addrRef;
        }
        else
        if ( 0 == ::lstrcmpA( func.nameFunc, "_strdup" ) )
        {
            data.dwStrdup = (DWORD64)func.addrRef;
        }
        else
        if ( 0 == ::lstrcmpA( func.nameFunc, "_wcsdup" ) )
        {
            data.dwWcsdup = (DWORD64)func.addrRef;
        }
        else
        if ( 0 == ::lstrcmpA( func.nameFunc, "_recalloc" ) )
        {
            data.dwReCalloc = (DWORD64)func.addrRef;
        }
        else
        if ( 0 == ::lstrcmpA( func.nameFunc, "_expand" ) )
        {
            data.dwExpand = (DWORD64)func.addrRef;
        }
        else
        if ( 0 == ::lstrcmpA( func.nameFunc, sFuncName_New ) )
        {
            data.dwNew = (DWORD64)func.addrRef;
        }
        else
        if ( 0 == ::lstrcmpA( func.nameFunc, sFuncName_Delete ) )
        {
            data.dwDelete = (DWORD64)func.addrRef;
        }
        else
        if ( 0 == ::lstrcmpA( func.nameFunc, sFuncName_NewArray ) )
        {
            data.dwNewArray = (DWORD64)func.addrRef;
        }
        else
        if ( 0 == ::lstrcmpA( func.nameFunc, sFuncName_DeleteArray ) )
        {
            data.dwDeleteArray = (DWORD64)func.addrRef;
        }
        else
        if ( 0 == ::lstrcmpA( func.nameFunc, sFuncName_New64 ) )
        {
            data.dwNew = (DWORD64)func.addrRef;
        }
        else
        if ( 0 == ::lstrcmpA( func.nameFunc, sFuncName_NewArray64 ) )
        {
            data.dwNewArray = (DWORD64)func.addrRef;
        }
        else
        if ( 0 == ::lstrcmpA( func.nameFunc, sFuncName_Delete64 ) )
        {
            data.dwDelete = (DWORD64)func.addrRef;
        }
        else
        if ( 0 == ::lstrcmpA( func.nameFunc, sFuncName_DeleteArray64 ) )
        {
            data.dwDeleteArray = (DWORD64)func.addrRef;
        }
        else
        if ( 0 == ::lstrcmpA( func.nameFunc, "_aligned_malloc" ) )
        {
            data.dwAlignedMalloc = (DWORD64)func.addrRef;
        }
        else
        if ( 0 == ::lstrcmpA( func.nameFunc, "_aligned_free" ) )
        {
            data.dwAlignedFree = (DWORD64)func.addrRef;
        }
        else
        if ( 0 == ::lstrcmpA( func.nameFunc, "_aligned_recalloc" ) )
        {
            data.dwAlignedReCalloc = (DWORD64)func.addrRef;
        }
        else
        if ( 0 == ::lstrcmpA( func.nameFunc, "_aligned_realloc" ) )
        {
            data.dwAlignedRealloc = (DWORD64)func.addrRef;
        }
    }
}

//static
unsigned int
MemoryOperationMismatchServer::threadServer( void* pVoid )
{
    if ( NULL == pVoid )
    {
        return 1;
    }

    MemoryOperationMismatchServer* pThis = reinterpret_cast<MemoryOperationMismatchServer*>(pVoid);

    mapMemory.clear();

    packetAction    action;
    packetCRTModule module;

    size_t recevedSize = 0;
    size_t sendedSize = 0;
    packetHeader    header;
    dataProcessId   processId;
    dataMemoryOperation memory;

    bool    luckNewArray = false;

    {
        const bool bRet = pThis->mNamedPipe.waitClient();
        if ( !bRet )
        {
            return 2;
        }
    }

    for ( ; ; )
    {
        if ( eventStop.wait(0) )
        {
            break;
        }

        {
            const bool bRet = pThis->mNamedPipe.recv( (char*)&header, sizeof(header), recevedSize );
            if ( !bRet )
            {
                break;
            }
        }
        switch ( header.mode )
        {
        case kModeAction:
            break;
        case kModeProcessId:
            if ( sizeof(processId) == header.size )
            {
                pThis->mNamedPipe.recv( (char*)&processId, sizeof(processId), recevedSize );

                module.header.size = sizeof(module);
                module.header.mode = kModeCRTModule;
                ZeroMemory( &module.data, sizeof(module.data) );
                ZeroMemory( &module.func, sizeof(module.func) );
                ZeroMemory( &module.user, sizeof(module.user) );
                {
                    kk::RemoteProcess   remoteProcess;
                    remoteProcess.init( processId.processId );
                    kk::RemoteMemory    remoteMemory;
                    const HANDLE hProcess = remoteProcess.getHandle();
                    const HMODULE hModule = remoteProcess.getHModule();
                    remoteMemory.init( hProcess, hModule );
                    const LPVOID moduleBase = remoteProcess.getModuleBase();
                    const size_t moduleSize = remoteProcess.getModuleSize();
                    remoteMemory.readFromRemote( moduleBase, moduleSize );
                    kk::PEIAT       peIAT;
                    const LPVOID pMemory = remoteMemory.getMemory();
                    peIAT.init( moduleBase, moduleSize, pMemory );
                    const char** file = peIAT.getImportFile();
                    const size_t fileCount = peIAT.getImportFileCount();
                    const kk::PEIAT::IMPORT_FUNC**  funcFile = peIAT.getImportFunc();
                    const size_t* funcCountFile = peIAT.getImportFuncCount();

                    for ( size_t indexFile = 0; indexFile < fileCount; ++indexFile )
                    {
                        const char* pFile = file[indexFile];
                        if ( 0 == ::lstrcmpiA(pFile, "msvcr90.dll" ) )
                        {
                            HMODULE hModule = remoteProcess.findModule( "msvcr90.dll" );
                            module.data.module = (DWORD64)hModule;
                            module.data.moduleInfo.info.version = enmVS2008;
                            module.data.moduleInfo.info.isDebug = false;
                            module.data.moduleInfo.info.isCRTStaticLinked = false;

                            const size_t funcCount = funcCountFile[indexFile];
                            fetchFunc( module.data, funcCount, funcFile[indexFile] );
                        }
#if defined(_DEBUG)
                        else
                        if ( 0 == ::lstrcmpiA(pFile, "msvcr90d.dll" ) )
                        {
                            HMODULE hModule = remoteProcess.findModule( "msvcr90d.dll" );
                            module.data.module = (DWORD64)hModule;
                            module.data.moduleInfo.info.version = enmVS2008;
                            module.data.moduleInfo.info.isDebug = true;
                            module.data.moduleInfo.info.isCRTStaticLinked = false;

                            const size_t funcCount = funcCountFile[indexFile];
                            fetchFunc( module.data, funcCount, funcFile[indexFile] );
                        }
#endif
                        else
                        if ( 0 == ::lstrcmpiA(pFile, "msvcr100.dll" ) )
                        {
                            HMODULE hModule = remoteProcess.findModule( "msvcr100.dll" );
                            module.data.module = (DWORD64)hModule;
                            module.data.moduleInfo.info.version = enmVS2010;
                            module.data.moduleInfo.info.isDebug = false;
                            module.data.moduleInfo.info.isCRTStaticLinked = false;

                            const size_t funcCount = funcCountFile[indexFile];
                            fetchFunc( module.data, funcCount, funcFile[indexFile] );
                        }
                        else
                        if ( 0 == ::lstrcmpiA(pFile, "msvcr110.dll" ) )
                        {
                            HMODULE hModule = remoteProcess.findModule( "msvcr110.dll" );
                            module.data.module = (DWORD64)hModule;
                            module.data.moduleInfo.info.version = enmVS2012;
                            module.data.moduleInfo.info.isDebug = false;
                            module.data.moduleInfo.info.isCRTStaticLinked = false;

                            const size_t funcCount = funcCountFile[indexFile];
                            fetchFunc( module.data, funcCount, funcFile[indexFile] );
                        }
                        else
                        if ( 0 == ::lstrcmpiA(pFile, "msvcr120.dll" ) )
                        {
                            HMODULE hModule = remoteProcess.findModule( "msvcr120.dll" );
                            module.data.module = (DWORD64)hModule;
                            module.data.moduleInfo.info.version = enmVS2013;
                            module.data.moduleInfo.info.isDebug = false;
                            module.data.moduleInfo.info.isCRTStaticLinked = false;

                            const size_t funcCount = funcCountFile[indexFile];
                            fetchFunc( module.data, funcCount, funcFile[indexFile] );
                        }
#if defined(_DEBUG)
                        else
                        if ( 0 == ::lstrcmpiA(pFile, "msvcr120d.dll" ) )
                        {
                            HMODULE hModule = remoteProcess.findModule( "msvcr120d.dll" );
                            module.data.module = (DWORD64)hModule;
                            module.data.moduleInfo.info.version = enmVS2013;
                            module.data.moduleInfo.info.isDebug = true;
                            module.data.moduleInfo.info.isCRTStaticLinked = false;

                            const size_t funcCount = funcCountFile[indexFile];
                            fetchFunc( module.data, funcCount, funcFile[indexFile] );
                        }
#endif
#if defined(_DEBUG)
                        else
                        if ( 0 == ::lstrcmpiA(pFile, "ucrtbased.dll" ) )
                        {
                            HMODULE hModule = remoteProcess.findModule( "ucrtbased.dll" );
                            module.data.module = (DWORD64)hModule;
                            module.data.moduleInfo.info.version = enmVS2015;
                            module.data.moduleInfo.info.isDebug = true;
                            module.data.moduleInfo.info.isCRTStaticLinked = true;

                            const size_t funcCount = funcCountFile[indexFile];
                            fetchFunc( module.data, funcCount, funcFile[indexFile] );
                        }
#endif
                        else
                        if ( 0 == ::lstrcmpiA(pFile, "api-ms-win-crt-heap-l1-1-0.dll" ) )
                        {
                            HMODULE hModule = remoteProcess.findModule( "api-ms-win-crt-heap-l1-1-0.dll" );
                            module.data.module = (DWORD64)hModule;
                            module.data.moduleInfo.info.version = enmVS2015;
                            module.data.moduleInfo.info.isDebug = false;
                            module.data.moduleInfo.info.isCRTStaticLinked = true;

                            const size_t funcCount = funcCountFile[indexFile];
                            fetchFunc( module.data, funcCount, funcFile[indexFile] );
                        }
                    }
                }

                bool includeCRTNewArray = false;
                {
                    kk::DebugSymbol debugSymbol;
                    debugSymbol.init( processId.processId );
                    switch ( module.data.moduleInfo.info.version )
                    {
                    case enmVS6:
                    case enmVS2002:
                    case enmVS2003:
                    case enmVS2005:
                    case enmVS2008:
                    case enmVS2010:
                    case enmVS2012:
                    case enmVS2013:
                        debugSymbol.setCRTCPPisStaticLinked( false );
                        break;

                    case enmVS2015:
                        debugSymbol.setCRTCPPisStaticLinked( true );
                        break;

                    default:
                        break;
                    }
                    debugSymbol.findGlobalReplacements();
                    includeCRTNewArray = debugSymbol.isIncludeCRTNewArray();
                    if ( includeCRTNewArray )
                    {
                        kk::DebugSymbol::FuncInfo   funcInfo;
                        ZeroMemory( &funcInfo, sizeof(funcInfo) );
                        const bool bRet = debugSymbol.getCRTNewArrayRVA( &funcInfo );
                        if ( bRet )
                        {
                            module.func.dwCRTStaticNewArray = funcInfo.dwAddr;
                            module.func.dwCRTStaticNewArrayLength = funcInfo.size;
                        }
                        else
                        {
                        }
                    }

                    {
                        size_t  countArray[DebugSymbol::kIndexOperationMax];
                        debugSymbol.getGlobalReplacementsCount( countArray );
                        for ( size_t indexOperation = 0; indexOperation < DebugSymbol::kIndexOperationMax; ++indexOperation )
                        {
                            const size_t count = countArray[indexOperation];
                            if ( 0 == count )
                            {
                                continue;
                            }

                            DebugSymbol::FuncInfo* funcInfo = new DebugSymbol::FuncInfo[ count ];

                            if ( NULL != funcInfo )
                            {
                                debugSymbol.getGlobalReplacementsRVA( (DebugSymbol::enumIndexOperation)indexOperation, funcInfo );

                                if ( 2 <= count )
                                {
                                    {
                                        ::OutputDebugStringA( "kk-win-lib-checker-memory-operation-mismatch-server: multiple instance. delete 'static' and re-compile your program\n" );
                                    }
                                }
                                for ( size_t index = 0; index < count; ++index )
                                {
                                    if ( funcInfo[index].isCRT )
                                    {
                                        switch( indexOperation )
                                        {
                                        case DebugSymbol::kIndexOperationNew:
                                            module.func.dwCRTStaticNew = funcInfo[index].dwAddr;
                                            module.func.dwCRTStaticNewLength = funcInfo[index].size;
                                            break;
                                        case DebugSymbol::kIndexOperationDelete:
                                            module.func.dwCRTStaticDelete = funcInfo[index].dwAddr;
                                            module.func.dwCRTStaticDeleteLength = funcInfo[index].size;
                                            break;
                                        case DebugSymbol::kIndexOperationNewArray:
                                            module.func.dwCRTStaticNewArray = funcInfo[index].dwAddr;
                                            module.func.dwCRTStaticNewArrayLength = funcInfo[index].size;
                                            break;
                                        case DebugSymbol::kIndexOperationDeleteArray:
                                            module.func.dwCRTStaticDeleteArray = funcInfo[index].dwAddr;
                                            module.func.dwCRTStaticDeleteArrayLength = funcInfo[index].size;
                                            break;

                                        case DebugSymbol::kIndexOperationDeleteSize:
                                            module.func.dwCRTStaticDeleteSize = funcInfo[index].dwAddr;
                                            module.func.dwCRTStaticDeleteSizeLength = funcInfo[index].size;
                                            break;
                                        case DebugSymbol::kIndexOperationDeleteArraySize:
                                            module.func.dwCRTStaticDeleteArraySize = funcInfo[index].dwAddr;
                                            module.func.dwCRTStaticDeleteArraySizeLength = funcInfo[index].size;
                                            break;

                                        default:
                                            assert( false );
                                            break;
                                        }
                                    }
                                    else
                                    {
                                        switch( indexOperation )
                                        {
                                        case DebugSymbol::kIndexOperationNew:
                                            module.user.dwUserStaticNew = funcInfo[index].dwAddr;
                                            module.user.dwUserStaticNewLength = funcInfo[index].size;
                                            break;
                                        case DebugSymbol::kIndexOperationDelete:
                                            module.user.dwUserStaticDelete = funcInfo[index].dwAddr;
                                            module.user.dwUserStaticDeleteLength = funcInfo[index].size;
                                            break;
                                        case DebugSymbol::kIndexOperationNewArray:
                                            module.user.dwUserStaticNewArray = funcInfo[index].dwAddr;
                                            module.user.dwUserStaticNewArrayLength = funcInfo[index].size;
                                            break;
                                        case DebugSymbol::kIndexOperationDeleteArray:
                                            module.user.dwUserStaticDeleteArray = funcInfo[index].dwAddr;
                                            module.user.dwUserStaticDeleteArrayLength = funcInfo[index].size;
                                            break;

                                        case DebugSymbol::kIndexOperationDeleteSize:
                                            module.user.dwUserStaticDeleteSize = funcInfo[index].dwAddr;
                                            module.user.dwUserStaticDeleteSizeLength = funcInfo[index].size;
                                            break;
                                        case DebugSymbol::kIndexOperationDeleteArraySize:
                                            module.user.dwUserStaticDeleteArraySize = funcInfo[index].dwAddr;
                                            module.user.dwUserStaticDeleteArraySizeLength = funcInfo[index].size;
                                            break;

                                        case DebugSymbol::kIndexOperationNewArg3:
                                            module.user.dwUserStaticNewArg3 = funcInfo[index].dwAddr;
                                            module.user.dwUserStaticNewArg3Length = funcInfo[index].size;
                                            break;
                                        case DebugSymbol::kIndexOperationDeleteArg3:
                                            module.user.dwUserStaticDeleteArg3 = funcInfo[index].dwAddr;
                                            module.user.dwUserStaticDeleteArg3Length = funcInfo[index].size;
                                            break;
                                        case DebugSymbol::kIndexOperationNewArrayArg3:
                                            module.user.dwUserStaticNewArrayArg3 = funcInfo[index].dwAddr;
                                            module.user.dwUserStaticNewArrayArg3Length = funcInfo[index].size;
                                            break;
                                        case DebugSymbol::kIndexOperationDeleteArrayArg3:
                                            module.user.dwUserStaticDeleteArrayArg3 = funcInfo[index].dwAddr;
                                            module.user.dwUserStaticDeleteArrayArg3Length = funcInfo[index].size;
                                            break;
                                        case DebugSymbol::kIndexOperationNewArg4:
                                            module.user.dwUserStaticNewArg4 = funcInfo[index].dwAddr;
                                            module.user.dwUserStaticNewArg4Length = funcInfo[index].size;
                                            break;
                                        case DebugSymbol::kIndexOperationDeleteArg4:
                                            module.user.dwUserStaticDeleteArg4 = funcInfo[index].dwAddr;
                                            module.user.dwUserStaticDeleteArg4Length = funcInfo[index].size;
                                            break;
                                        case DebugSymbol::kIndexOperationNewArrayArg4:
                                            module.user.dwUserStaticNewArrayArg4 = funcInfo[index].dwAddr;
                                            module.user.dwUserStaticNewArrayArg4Length = funcInfo[index].size;
                                            break;
                                        case DebugSymbol::kIndexOperationDeleteArrayArg4:
                                            module.user.dwUserStaticDeleteArrayArg4 = funcInfo[index].dwAddr;
                                            module.user.dwUserStaticDeleteArrayArg4Length = funcInfo[index].size;
                                            break;

                                        default:
                                            assert( false );
                                            break;
                                        }
                                    }
                                } // for index

                            }

                            if ( NULL != funcInfo )
                            {
                                delete [] funcInfo;
                            }
                            funcInfo = NULL;
                        } // for indexOperation

                        if (
                            module.func.dwCRTStaticDeleteArray == module.func.dwCRTStaticDeleteSize
                            && module.func.dwCRTStaticDeleteArrayLength == module.func.dwCRTStaticDeleteSizeLength
                        )
                        {
                            module.func.dwCRTStaticDeleteSize = 0;
                            module.func.dwCRTStaticDeleteSizeLength = 0;
                        }

                    } // includeCRTNewArray
                }

                if ( includeCRTNewArray )
                {
                    if ( 0 == module.func.dwCRTStaticNewArray )
                    {
                        luckNewArray = true;
                    }
                }
                else
                {
                    if (
                        0 != module.data.dwNew
                        && 0 != module.data.dwDelete
                        && 0 == module.data.dwNewArray
                        && 0 != module.data.dwDeleteArray
                    )
                    {
                        luckNewArray = true;
                        {
                            ::OutputDebugStringA( "kk-win-lib-checker-memory-operation-mismatch: assume new[] redirected.\n" );
                        }
                    }
                }

                pThis->mNamedPipe.send( (char*)&module, sizeof(module), sendedSize );


            }
            break;
        case kModeMemoryOperation:
            if ( sizeof(memory) == header.size )
            {
                pThis->mNamedPipe.recv( (char*)&memory, sizeof(memory), recevedSize );

                switch ( memory.funcMemoryOperation )
                {
                case kOperationMalloc:
                case kOperationCalloc:
                case kOperationRealloc:
                case kOperationStrdup:
                case kOperationWcsdup:
                case kOperationReCalloc:
                case kOperationExpand:
                    {
                        bool negative = false;

                        if ( 0 == memory.pointer && false == pThis->mNeedBreakAllocNull )
                        {
                            // skip
                        }
                        else
                        {
                            mapRecord::iterator it = mapMemory.find( memory.pointer );
                            if ( mapMemory.end() != it )
                            {
                                // error
                                action.header.size = sizeof(action);
                                action.header.mode = kModeAction;
                                if ( pThis->mDoBreak )
                                {
                                    action.data.action = kActionBreak;
                                }
                                else
                                {
                                    action.data.action = kActionNone;
                                }
                                pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                negative = true;
                            }
                            pairRecord  pair(memory.pointer, (enumMemoryOperation)memory.funcMemoryOperation);
                            std::pair<mapRecord::iterator,bool> ret = mapMemory.insert( pair );
                            if ( false == ret.second )
                            {
                                // error
                                action.header.size = sizeof(action);
                                action.header.mode = kModeAction;
                                if ( pThis->mDoBreak )
                                {
                                    action.data.action = kActionBreak;
                                }
                                else
                                {
                                    action.data.action = kActionNone;
                                }
                                pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                negative = true;
                            }
                        }

                        if ( negative )
                        {
                        }
                        else
                        {
                            action.header.size = sizeof(action);
                            action.header.mode = kModeAction;
                            action.data.action = kActionNone;
                            pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                        }
                    }
                    break;

                case kOperationFree:
                case kOperationReallocFree:
                case kOperationReCallocFree:
                case kOperationExpandFree:
                    {
                        bool negative = false;

                        if ( 0 == memory.pointer && false == pThis->mNeedBreakDeallocNull )
                        {
                            // skip
                        }
                        else
                        {
                            mapRecord::iterator it = mapMemory.find( memory.pointer );
                            if ( mapMemory.end() == it )
                            {
                                // error
                                action.header.size = sizeof(action);
                                action.header.mode = kModeAction;
                                if ( pThis->mDoBreak )
                                {
                                    action.data.action = kActionBreak;
                                }
                                else
                                {
                                    action.data.action = kActionNone;
                                }
                                pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                negative = true;
                            }
                            else
                            {
                                if (
                                    !
                                    (kOperationMalloc == it->second
                                    || kOperationCalloc == it->second
                                    || kOperationRealloc == it->second
                                    || kOperationStrdup == it->second
                                    || kOperationWcsdup == it->second
                                    || kOperationReCalloc == it->second
                                    || kOperationExpand == it->second
                                    )
                                )
                                {
                                    // error
                                    action.header.size = sizeof(action);
                                    action.header.mode = kModeAction;
                                    if ( pThis->mDoBreak )
                                    {
                                        action.data.action = kActionBreak;
                                    }
                                    else
                                    {
                                        action.data.action = kActionNone;
                                    }
                                    pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                    negative = true;
                                }
                                mapMemory.erase( it );

                                {
                                    mapRecordUser::iterator itUser = mapMemoryUser.find( memory.pointer );
                                    if ( mapMemoryUser.end() != itUser )
                                    {
                                        mapMemoryUser.erase( itUser );
                                    }
                                }

                            }
                        }

                        if ( negative )
                        {
                        }
                        else
                        {
                            action.header.size = sizeof(action);
                            action.header.mode = kModeAction;
                            action.data.action = kActionNone;
                            pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                        }
                    }
                    break;

                case kOperationNew:
                case kOperationNewArray:
                    {
                        bool negative = false;

                        if ( 0 == memory.pointer && false == pThis->mNeedBreakAllocNull )
                        {
                            // skip
                        }
                        else
                        {
                            mapRecord::iterator it = mapMemory.find( memory.pointer );
                            if ( mapMemory.end() != it )
                            {
                                // error
                                action.header.size = sizeof(action);
                                action.header.mode = kModeAction;
                                if ( pThis->mDoBreak )
                                {
                                    action.data.action = kActionBreak;
                                }
                                else
                                {
                                    action.data.action = kActionNone;
                                }
                                pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                negative = true;
                            }
                            pairRecord  pair(memory.pointer, (enumMemoryOperation)memory.funcMemoryOperation);
                            std::pair<mapRecord::iterator,bool> ret = mapMemory.insert( pair );
                            if ( false == ret.second )
                            {
                                // error
                                action.header.size = sizeof(action);
                                action.header.mode = kModeAction;
                                if ( pThis->mDoBreak )
                                {
                                    action.data.action = kActionBreak;
                                }
                                else
                                {
                                    action.data.action = kActionNone;
                                }
                                pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                negative = true;
                            }
                        }

                        if ( negative )
                        {
                        }
                        else
                        {
                            action.header.size = sizeof(action);
                            action.header.mode = kModeAction;
                            action.data.action = kActionNone;
                            pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                        }

                    }
                    break;

                case kOperationDelete:
                    {
                        bool negative = false;

                        if ( 0 == memory.pointer && false == pThis->mNeedBreakDeallocNull )
                        {
                            // skip
                        }
                        else
                        {
                            mapRecord::iterator it = mapMemory.find( memory.pointer );
                            if ( mapMemory.end() == it )
                            {
                                // error
                                action.header.size = sizeof(action);
                                action.header.mode = kModeAction;
                                if ( pThis->mDoBreak )
                                {
                                    action.data.action = kActionBreak;
                                }
                                else
                                {
                                    action.data.action = kActionNone;
                                }
                                pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                negative = true;
                            }
                            else
                            {
                                if ( kOperationNew != it->second )
                                {
                                    // error
                                    action.header.size = sizeof(action);
                                    action.header.mode = kModeAction;
                                    if ( pThis->mDoBreak )
                                    {
                                        action.data.action = kActionBreak;
                                    }
                                    else
                                    {
                                        action.data.action = kActionNone;
                                    }
                                    pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                    negative = true;
                                }
                                mapMemory.erase( it );
                            }
                        }

                        // check CERT DCL54-CPP
                        {
                            mapRecordUser::iterator itUser = mapMemoryUser.find( memory.pointer );
                            if ( mapMemoryUser.end() == itUser )
                            {
                            }
                            else
                            {
                                if ( false == negative )
                                {
                                    // error
                                    action.header.size = sizeof(action);
                                    action.header.mode = kModeAction;
                                    if ( pThis->mDoBreak )
                                    {
                                        action.data.action = kActionBreak;
                                    }
                                    else
                                    {
                                        action.data.action = kActionNone;
                                    }
                                    pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                    negative = true;
                                }

                                mapMemoryUser.erase( itUser );
                            }
                        }

                        if ( negative )
                        {
                        }
                        else
                        {
                            action.header.size = sizeof(action);
                            action.header.mode = kModeAction;
                            action.data.action = kActionNone;
                            pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                        }

                    }
                    break;

                case kOperationDeleteArray:
                    {
                        bool negative = false;

                        if ( 0 == memory.pointer && false == pThis->mNeedBreakDeallocNull )
                        {
                            // skip
                        }
                        else
                        {
                            mapRecord::iterator it = mapMemory.find( memory.pointer );
                            if ( mapMemory.end() == it )
                            {
                                // error
                                action.header.size = sizeof(action);
                                action.header.mode = kModeAction;
                                if ( pThis->mDoBreak )
                                {
                                    action.data.action = kActionBreak;
                                }
                                else
                                {
                                    action.data.action = kActionNone;
                                }
                                pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                negative = true;
                            }
                            else
                            {
                                if ( kOperationNewArray != it->second )
                                {
                                    if ( luckNewArray )
                                    {
                                        // warn?
                                    }
                                    else
                                    {
                                        // error
                                        action.header.size = sizeof(action);
                                        action.header.mode = kModeAction;
                                        if ( pThis->mDoBreak )
                                        {
                                            action.data.action = kActionBreak;
                                        }
                                        else
                                        {
                                            action.data.action = kActionNone;
                                        }
                                        pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                        negative = true;
                                    }
                                }
                                mapMemory.erase( it );
                            }
                        }

                        // check CERT DCL54-CPP
                        {
                            mapRecordUser::iterator itUser = mapMemoryUser.find( memory.pointer );
                            if ( mapMemoryUser.end() == itUser )
                            {
                            }
                            else
                            {
                                if ( false == negative )
                                {
                                    // error
                                    action.header.size = sizeof(action);
                                    action.header.mode = kModeAction;
                                    if ( pThis->mDoBreak )
                                    {
                                        action.data.action = kActionBreak;
                                    }
                                    else
                                    {
                                        action.data.action = kActionNone;
                                    }
                                    pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                    negative = true;
                                }

                                mapMemoryUser.erase( itUser );
                            }
                        }

                        if ( negative )
                        {
                        }
                        else
                        {
                            action.header.size = sizeof(action);
                            action.header.mode = kModeAction;
                            action.data.action = kActionNone;
                            pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                        }

                    }
                    break;

                case kOperationAlignedMalloc:
                case kOperationAlignedRecalloc:
                case kOperationAlignedRealloc:
                    {
                        bool negative = false;

                        if ( 0 == memory.pointer && false == pThis->mNeedBreakAllocNull )
                        {
                            // skip
                        }
                        else
                        {
                            mapRecord::iterator it = mapMemory.find( memory.pointer );
                            if ( mapMemory.end() != it )
                            {
                                // error
                                action.header.size = sizeof(action);
                                action.header.mode = kModeAction;
                                if ( pThis->mDoBreak )
                                {
                                    action.data.action = kActionBreak;
                                }
                                else
                                {
                                    action.data.action = kActionNone;
                                }
                                pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                negative = true;
                            }
                            pairRecord  pair(memory.pointer, (enumMemoryOperation)memory.funcMemoryOperation);
                            std::pair<mapRecord::iterator,bool> ret = mapMemory.insert( pair );
                            if ( false == ret.second )
                            {
                                // error
                                action.header.size = sizeof(action);
                                action.header.mode = kModeAction;
                                if ( pThis->mDoBreak )
                                {
                                    action.data.action = kActionBreak;
                                }
                                else
                                {
                                    action.data.action = kActionNone;
                                }
                                pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                negative = true;
                            }
                        }

                        if ( negative )
                        {
                        }
                        else
                        {
                            action.header.size = sizeof(action);
                            action.header.mode = kModeAction;
                            action.data.action = kActionNone;
                            pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                        }
                    }
                    break;

                case kOperationAlignedFree:
                case kOperationAlignedRecallocFree:
                case kOperationAlignedReallocFree:
                    {
                        bool negative = false;

                        if ( 0 == memory.pointer && false == pThis->mNeedBreakDeallocNull )
                        {
                            // skip
                        }
                        else
                        {
                            mapRecord::iterator it = mapMemory.find( memory.pointer );
                            if ( mapMemory.end() == it )
                            {
                                // error
                                action.header.size = sizeof(action);
                                action.header.mode = kModeAction;
                                if ( pThis->mDoBreak )
                                {
                                    action.data.action = kActionBreak;
                                }
                                else
                                {
                                    action.data.action = kActionNone;
                                }
                                pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                negative = true;
                            }
                            else
                            {
                                if (
                                    !(kOperationAlignedMalloc == it->second
                                    || kOperationAlignedRecalloc == it->second
                                    || kOperationAlignedRealloc == it->second
                                    )
                                )
                                {
                                    // error
                                    action.header.size = sizeof(action);
                                    action.header.mode = kModeAction;
                                    if ( pThis->mDoBreak )
                                    {
                                        action.data.action = kActionBreak;
                                    }
                                    else
                                    {
                                        action.data.action = kActionNone;
                                    }
                                    pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                    negative = true;
                                }
                                mapMemory.erase( it );

                                {
                                    mapRecordUser::iterator itUser = mapMemoryUser.find( memory.pointer );
                                    if ( mapMemoryUser.end() != itUser )
                                    {

                                        mapMemoryUser.erase( itUser );
                                    }
                                }
                            }
                        }

                        if ( negative )
                        {
                        }
                        else
                        {
                            action.header.size = sizeof(action);
                            action.header.mode = kModeAction;
                            action.data.action = kActionNone;
                            pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                        }
                    }
                    break;

                case kOperationCRTStaticNew:
                    {
                        bool negative = false;

                        if ( 0 == memory.pointer && false == pThis->mNeedBreakAllocNull )
                        {
                            // skip
                        }
                        else
                        {
                            mapRecord::iterator it = mapMemory.find( memory.pointer );
                            if ( mapMemory.end() != it )
                            {
                                if ( kOperationMalloc == it->second )
                                {
                                    // change
                                    it->second = kOperationCRTStaticNew;
                                }
                                else
                                {
                                    // error
                                    action.header.size = sizeof(action);
                                    action.header.mode = kModeAction;
                                    if ( pThis->mDoBreak )
                                    {
                                        action.data.action = kActionBreak;
                                    }
                                    else
                                    {
                                        action.data.action = kActionNone;
                                    }
                                    pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                    negative = true;
                                }
                            }
                            else
                            {
                                pairRecord  pair(memory.pointer, (enumMemoryOperation)memory.funcMemoryOperation);
                                std::pair<mapRecord::iterator,bool> ret = mapMemory.insert( pair );
                                if ( false == ret.second )
                                {
                                    // error
                                    action.header.size = sizeof(action);
                                    action.header.mode = kModeAction;
                                    if ( pThis->mDoBreak )
                                    {
                                        action.data.action = kActionBreak;
                                    }
                                    else
                                    {
                                        action.data.action = kActionNone;
                                    }
                                    pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                    negative = true;
                                }
                            }
                        }

                        if ( negative )
                        {
                        }
                        else
                        {
                            action.header.size = sizeof(action);
                            action.header.mode = kModeAction;
                            action.data.action = kActionNone;
                            pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                        }

                    }
                    break;

                case kOperationCRTStaticNewArray:
                    {
                        bool negative = false;

                        if ( 0 == memory.pointer && false == pThis->mNeedBreakAllocNull )
                        {
                            // skip
                        }
                        else
                        {
                            mapRecord::iterator it = mapMemory.find( memory.pointer );
                            if ( mapMemory.end() != it )
                            {
                                if ( kOperationCRTStaticNew == it->second )
                                {
                                    // change
                                    it->second = kOperationCRTStaticNewArray;
                                }
                                else
                                {
                                    // error
                                    action.header.size = sizeof(action);
                                    action.header.mode = kModeAction;
                                    if ( pThis->mDoBreak )
                                    {
                                        action.data.action = kActionBreak;
                                    }
                                    else
                                    {
                                        action.data.action = kActionNone;
                                    }
                                    pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                    negative = true;
                                }
                            }
                            else
                            {
                                pairRecord  pair(memory.pointer, (enumMemoryOperation)memory.funcMemoryOperation);
                                std::pair<mapRecord::iterator,bool> ret = mapMemory.insert( pair );
                                if ( false == ret.second )
                                {
                                    // error
                                    action.header.size = sizeof(action);
                                    action.header.mode = kModeAction;
                                    if ( pThis->mDoBreak )
                                    {
                                        action.data.action = kActionBreak;
                                    }
                                    else
                                    {
                                        action.data.action = kActionNone;
                                    }
                                    pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                    negative = true;
                                }
                            }
                        }

                        if ( negative )
                        {
                        }
                        else
                        {
                            action.header.size = sizeof(action);
                            action.header.mode = kModeAction;
                            action.data.action = kActionNone;
                            pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                        }

                    }
                    break;

                case kOperationCRTStaticDelete:
                    {
                        bool negative = false;

                        if ( 0 == memory.pointer && false == pThis->mNeedBreakDeallocNull )
                        {
                            // skip
                        }
                        else
                        {
                            mapRecord::iterator it = mapMemory.find( memory.pointer );
                            if ( mapMemory.end() == it )
                            {
                                // error
                                action.header.size = sizeof(action);
                                action.header.mode = kModeAction;
                                if ( pThis->mDoBreak )
                                {
                                    action.data.action = kActionBreak;
                                }
                                else
                                {
                                    action.data.action = kActionNone;
                                }
                                pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                negative = true;
                            }
                            else
                            {
                                if ( kOperationCRTStaticNew != it->second )
                                {
                                    // error
                                    action.header.size = sizeof(action);
                                    action.header.mode = kModeAction;
                                    if ( pThis->mDoBreak )
                                    {
                                        action.data.action = kActionBreak;
                                    }
                                    else
                                    {
                                        action.data.action = kActionNone;
                                    }
                                    pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                    negative = true;
                                }
                                it->second = kOperationMalloc;
                            }
                        }

                        if ( negative )
                        {
                        }
                        else
                        {
                            action.header.size = sizeof(action);
                            action.header.mode = kModeAction;
                            action.data.action = kActionNone;
                            pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                        }

                    }
                    break;

                case kOperationCRTStaticDeleteArray:
                    {
                        bool negative = false;

                        if ( 0 == memory.pointer && false == pThis->mNeedBreakDeallocNull )
                        {
                            // skip
                        }
                        else
                        {
                            mapRecord::iterator it = mapMemory.find( memory.pointer );
                            if ( mapMemory.end() == it )
                            {
                                // error
                                action.header.size = sizeof(action);
                                action.header.mode = kModeAction;
                                if ( pThis->mDoBreak )
                                {
                                    action.data.action = kActionBreak;
                                }
                                else
                                {
                                    action.data.action = kActionNone;
                                }
                                pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                negative = true;
                            }
                            else
                            {
                                if ( kOperationCRTStaticNewArray != it->second )
                                {
                                    if ( luckNewArray )
                                    {
                                        // warn?
                                    }
                                    else
                                    {
                                        // error
                                        action.header.size = sizeof(action);
                                        action.header.mode = kModeAction;
                                        if ( pThis->mDoBreak )
                                        {
                                            action.data.action = kActionBreak;
                                        }
                                        else
                                        {
                                            action.data.action = kActionNone;
                                        }
                                        pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                        negative = true;
                                    }
                                }
                                it->second = kOperationCRTStaticNew;
                            }
                        }

                        if ( negative )
                        {
                        }
                        else
                        {
                            action.header.size = sizeof(action);
                            action.header.mode = kModeAction;
                            action.data.action = kActionNone;
                            pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                        }

                    }
                    break;

                case kOperationCRTStaticDeleteSize:
                    {
                        bool negative = false;

                        if ( 0 == memory.pointer && false == pThis->mNeedBreakDeallocNull )
                        {
                            // skip
                        }
                        else
                        {
                            mapRecord::iterator it = mapMemory.find( memory.pointer );
                            if ( mapMemory.end() == it )
                            {
                                // error
                                action.header.size = sizeof(action);
                                action.header.mode = kModeAction;
                                if ( pThis->mDoBreak )
                                {
                                    action.data.action = kActionBreak;
                                }
                                else
                                {
                                    action.data.action = kActionNone;
                                }
                                pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                negative = true;
                            }
                            else
                            {
                                if ( kOperationCRTStaticNew != it->second )
                                {
                                    // error
                                    action.header.size = sizeof(action);
                                    action.header.mode = kModeAction;
                                    if ( pThis->mDoBreak )
                                    {
                                        action.data.action = kActionBreak;
                                    }
                                    else
                                    {
                                        action.data.action = kActionNone;
                                    }
                                    pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                    negative = true;
                                }
                                it->second = kOperationCRTStaticNew;
                            }
                        }

                        if ( negative )
                        {
                        }
                        else
                        {
                            action.header.size = sizeof(action);
                            action.header.mode = kModeAction;
                            action.data.action = kActionNone;
                            pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                        }

                    }
                    break;
                case kOperationCRTStaticDeleteArraySize:
                    {
                        bool negative = false;

                        if ( 0 == memory.pointer && false == pThis->mNeedBreakDeallocNull )
                        {
                            // skip
                        }
                        else
                        {
                            mapRecord::iterator it = mapMemory.find( memory.pointer );
                            if ( mapMemory.end() == it )
                            {
                                // error
                                action.header.size = sizeof(action);
                                action.header.mode = kModeAction;
                                if ( pThis->mDoBreak )
                                {
                                    action.data.action = kActionBreak;
                                }
                                else
                                {
                                    action.data.action = kActionNone;
                                }
                                pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                negative = true;
                            }
                            else
                            {
                                if ( kOperationCRTStaticNewArray != it->second )
                                {
                                    if ( luckNewArray )
                                    {
                                        // warn?
                                    }
                                    else
                                    {
                                        // error
                                        action.header.size = sizeof(action);
                                        action.header.mode = kModeAction;
                                        if ( pThis->mDoBreak )
                                        {
                                            action.data.action = kActionBreak;
                                        }
                                        else
                                        {
                                            action.data.action = kActionNone;
                                        }
                                        pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                        negative = true;
                                    }
                                }
                                it->second = kOperationCRTStaticNewArray;
                            }
                        }

                        if ( negative )
                        {
                        }
                        else
                        {
                            action.header.size = sizeof(action);
                            action.header.mode = kModeAction;
                            action.data.action = kActionNone;
                            pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                        }

                    }
                    break;

                case kOperationUserStaticNew:
                    {
                        bool negative = false;

                        if ( 0 == memory.pointer && false == pThis->mNeedBreakAllocNull )
                        {
                            // skip
                        }
                        else
                        {
                            mapRecord::iterator it = mapMemory.find( memory.pointer );
                            if ( mapMemory.end() != it )
                            {
                                std::stack<enumMemoryOperation>    chain;
                                chain.push( it->second );
                                chain.push( (enumMemoryOperation)memory.funcMemoryOperation );
                                pairRecordUser  pair(memory.pointer, chain);
                                std::pair<mapRecordUser::iterator,bool> ret = mapMemoryUser.insert( pair );
                                if ( false == ret.second )
                                {
                                    // error
                                    action.header.size = sizeof(action);
                                    action.header.mode = kModeAction;
                                    if ( pThis->mDoBreak )
                                    {
                                        action.data.action = kActionBreak;
                                    }
                                    else
                                    {
                                        action.data.action = kActionNone;
                                    }
                                    pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                    negative = true;
                                }
                            }
                            else
                            {
                                std::stack<enumMemoryOperation>    chain;
                                chain.push( (enumMemoryOperation)memory.funcMemoryOperation );
                                pairRecordUser  pair(memory.pointer, chain);
                                std::pair<mapRecordUser::iterator,bool> ret = mapMemoryUser.insert( pair );
                                if ( false == ret.second )
                                {
                                    // error
                                    action.header.size = sizeof(action);
                                    action.header.mode = kModeAction;
                                    if ( pThis->mDoBreak )
                                    {
                                        action.data.action = kActionBreak;
                                    }
                                    else
                                    {
                                        action.data.action = kActionNone;
                                    }
                                    pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                    negative = true;
                                }
                            }
                        }

                        if ( negative )
                        {
                        }
                        else
                        {
                            action.header.size = sizeof(action);
                            action.header.mode = kModeAction;
                            action.data.action = kActionNone;
                            pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                        }

                    }
                    break;

                case kOperationUserStaticNewArray:
                    {
                        bool negative = false;

                        if ( 0 == memory.pointer && false == pThis->mNeedBreakAllocNull )
                        {
                            // skip
                        }
                        else
                        {
                            mapRecord::iterator it = mapMemory.find( memory.pointer );
                            if ( mapMemory.end() != it )
                            {
                                mapRecordUser::iterator itUser = mapMemoryUser.find( memory.pointer );
                                if ( mapMemoryUser.end() != itUser )
                                {
                                    itUser->second.push( (enumMemoryOperation)memory.funcMemoryOperation );
                                }
                                else
                                {
                                    std::stack<enumMemoryOperation>    chain;
                                    chain.push( it->second );
                                    chain.push( (enumMemoryOperation)memory.funcMemoryOperation );
                                    pairRecordUser  pair(memory.pointer, chain);
                                    std::pair<mapRecordUser::iterator,bool> ret = mapMemoryUser.insert( pair );
                                    if ( false == ret.second )
                                    {
                                        // error
                                        action.header.size = sizeof(action);
                                        action.header.mode = kModeAction;
                                        if ( pThis->mDoBreak )
                                        {
                                            action.data.action = kActionBreak;
                                        }
                                        else
                                        {
                                            action.data.action = kActionNone;
                                        }
                                        pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                        negative = true;
                                    }
                                }
                            }
                            else
                            {
                                std::stack<enumMemoryOperation>    chain;
                                chain.push( (enumMemoryOperation)memory.funcMemoryOperation );
                                pairRecordUser  pair(memory.pointer, chain);
                                std::pair<mapRecordUser::iterator,bool> ret = mapMemoryUser.insert( pair );
                                if ( false == ret.second )
                                {
                                    // error
                                    action.header.size = sizeof(action);
                                    action.header.mode = kModeAction;
                                    if ( pThis->mDoBreak )
                                    {
                                        action.data.action = kActionBreak;
                                    }
                                    else
                                    {
                                        action.data.action = kActionNone;
                                    }
                                    pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                    negative = true;
                                }
                            }
                        }

                        if ( negative )
                        {
                        }
                        else
                        {
                            action.header.size = sizeof(action);
                            action.header.mode = kModeAction;
                            action.data.action = kActionNone;
                            pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                        }

                    }
                    break;

                case kOperationUserStaticDelete:
                    {
                        bool negative = false;

                        if ( 0 == memory.pointer && false == pThis->mNeedBreakDeallocNull )
                        {
                            // skip
                        }
                        else
                        {
                            mapRecordUser::iterator it = mapMemoryUser.find( memory.pointer );
                            if ( mapMemoryUser.end() == it )
                            {
                                // error
                                action.header.size = sizeof(action);
                                action.header.mode = kModeAction;
                                if ( pThis->mDoBreak )
                                {
                                    action.data.action = kActionBreak;
                                }
                                else
                                {
                                    action.data.action = kActionNone;
                                }
                                pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                negative = true;
                            }
                            else
                            {
                                if ( kOperationUserStaticNew != it->second.top() )
                                {
                                    // error
                                    action.header.size = sizeof(action);
                                    action.header.mode = kModeAction;
                                    if ( pThis->mDoBreak )
                                    {
                                        action.data.action = kActionBreak;
                                    }
                                    else
                                    {
                                        action.data.action = kActionNone;
                                    }
                                    pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                    negative = true;
                                }
                                it->second.pop();
                                if ( it->second.empty() )
                                {
                                    mapMemoryUser.erase( it );
                                }
                            }
                        }

                        if ( negative )
                        {
                        }
                        else
                        {
                            action.header.size = sizeof(action);
                            action.header.mode = kModeAction;
                            action.data.action = kActionNone;
                            pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                        }

                    }
                    break;

                case kOperationUserStaticDeleteArray:
                    {
                        bool negative = false;

                        if ( 0 == memory.pointer && false == pThis->mNeedBreakDeallocNull )
                        {
                            // skip
                        }
                        else
                        {
                            mapRecordUser::iterator it = mapMemoryUser.find( memory.pointer );
                            if ( mapMemoryUser.end() == it )
                            {
                                // error
                                action.header.size = sizeof(action);
                                action.header.mode = kModeAction;
                                if ( pThis->mDoBreak )
                                {
                                    action.data.action = kActionBreak;
                                }
                                else
                                {
                                    action.data.action = kActionNone;
                                }
                                pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                negative = true;
                            }
                            else
                            {
                                if ( kOperationUserStaticNewArray != it->second.top() )
                                {
                                    if ( luckNewArray )
                                    {
                                        // warn?
                                    }
                                    else
                                    {
                                        // error
                                        action.header.size = sizeof(action);
                                        action.header.mode = kModeAction;
                                        if ( pThis->mDoBreak )
                                        {
                                            action.data.action = kActionBreak;
                                        }
                                        else
                                        {
                                            action.data.action = kActionNone;
                                        }
                                        pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                        negative = true;
                                    }
                                }
                                it->second.pop();
                                if ( it->second.empty() )
                                {
                                    mapMemoryUser.erase( it );
                                }
                            }
                        }

                        if ( negative )
                        {
                        }
                        else
                        {
                            action.header.size = sizeof(action);
                            action.header.mode = kModeAction;
                            action.data.action = kActionNone;
                            pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                        }

                    }
                    break;

                case kOperationUserStaticDeleteSize:
                    {
                        bool negative = false;

                        if ( 0 == memory.pointer && false == pThis->mNeedBreakDeallocNull )
                        {
                            // skip
                        }
                        else
                        {
                            mapRecordUser::iterator it = mapMemoryUser.find( memory.pointer );
                            if ( mapMemoryUser.end() == it )
                            {
                                // error
                                action.header.size = sizeof(action);
                                action.header.mode = kModeAction;
                                if ( pThis->mDoBreak )
                                {
                                    action.data.action = kActionBreak;
                                }
                                else
                                {
                                    action.data.action = kActionNone;
                                }
                                pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                negative = true;
                            }
                            else
                            {
                                if ( kOperationUserStaticNew != it->second.top() )
                                {
                                    // error
                                    action.header.size = sizeof(action);
                                    action.header.mode = kModeAction;
                                    if ( pThis->mDoBreak )
                                    {
                                        action.data.action = kActionBreak;
                                    }
                                    else
                                    {
                                        action.data.action = kActionNone;
                                    }
                                    pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                    negative = true;
                                }
                                it->second.pop();
                                if ( it->second.empty() )
                                {
                                    mapMemoryUser.erase( it );
                                }
                            }
                        }

                        if ( negative )
                        {
                        }
                        else
                        {
                            action.header.size = sizeof(action);
                            action.header.mode = kModeAction;
                            action.data.action = kActionNone;
                            pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                        }

                    }
                    break;
                case kOperationUserStaticDeleteArraySize:
                    {
                        bool negative = false;

                        if ( 0 == memory.pointer && false == pThis->mNeedBreakDeallocNull )
                        {
                            // skip
                        }
                        else
                        {
                            mapRecordUser::iterator it = mapMemoryUser.find( memory.pointer );
                            if ( mapMemoryUser.end() == it )
                            {
                                // error
                                action.header.size = sizeof(action);
                                action.header.mode = kModeAction;
                                if ( pThis->mDoBreak )
                                {
                                    action.data.action = kActionBreak;
                                }
                                else
                                {
                                    action.data.action = kActionNone;
                                }
                                pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                negative = true;
                            }
                            else
                            {
                                if ( kOperationUserStaticNewArray != it->second.top() )
                                {
                                    if ( luckNewArray )
                                    {
                                        // warn?
                                    }
                                    else
                                    {
                                        // error
                                        action.header.size = sizeof(action);
                                        action.header.mode = kModeAction;
                                        if ( pThis->mDoBreak )
                                        {
                                            action.data.action = kActionBreak;
                                        }
                                        else
                                        {
                                            action.data.action = kActionNone;
                                        }
                                        pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                                        negative = true;
                                    }
                                }
                                it->second.pop();
                                if ( it->second.empty() )
                                {
                                    mapMemoryUser.erase( it );
                                }
                            }
                        }

                        if ( negative )
                        {
                        }
                        else
                        {
                            action.header.size = sizeof(action);
                            action.header.mode = kModeAction;
                            action.data.action = kActionNone;
                            pThis->mNamedPipe.send( (char*)&action, sizeof(action), sendedSize );
                        }

                    }
                    break;

                default:
                    assert( false );
                    break;
                } // switch ( memory.funcMemoryOperation )
            }
            break;

        default:
            assert( false );
            break;
        } // switch ( header.mode )

    }

    return 0;
}





} // namespace checker

} // namespace kk

