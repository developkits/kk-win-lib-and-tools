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

#include "kk-win-lib-checker-memory-operation-mismatch.h"
#include "kk-win-lib-checker-memory-operation-mismatch-server.h"

#include "kk-win-lib-checker-memory-operation-mismatch-internal.h"

#include <process.h>
#include <assert.h>

#include <map>

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

                            const size_t funcCount = funcCountFile[indexFile];
                            fetchFunc( module.data, funcCount, funcFile[indexFile] );
                        }
#if defined(_DEBUG)
                        else
                        if ( 0 == ::lstrcmpiA(pFile, "msvcr90d.dll" ) )
                        {
                            HMODULE hModule = remoteProcess.findModule( "msvcr90d.dll" );
                            module.data.module = (DWORD64)hModule;

                            const size_t funcCount = funcCountFile[indexFile];
                            fetchFunc( module.data, funcCount, funcFile[indexFile] );
                        }
#endif
                        else
                        if ( 0 == ::lstrcmpiA(pFile, "msvcr100.dll" ) )
                        {
                            HMODULE hModule = remoteProcess.findModule( "msvcr100.dll" );
                            module.data.module = (DWORD64)hModule;

                            const size_t funcCount = funcCountFile[indexFile];
                            fetchFunc( module.data, funcCount, funcFile[indexFile] );
                        }
                        else
                        if ( 0 == ::lstrcmpiA(pFile, "msvcr110.dll" ) )
                        {
                            HMODULE hModule = remoteProcess.findModule( "msvcr110.dll" );
                            module.data.module = (DWORD64)hModule;

                            const size_t funcCount = funcCountFile[indexFile];
                            fetchFunc( module.data, funcCount, funcFile[indexFile] );
                        }
                        else
                        if ( 0 == ::lstrcmpiA(pFile, "msvcr120.dll" ) )
                        {
                            HMODULE hModule = remoteProcess.findModule( "msvcr120.dll" );
                            module.data.module = (DWORD64)hModule;

                            const size_t funcCount = funcCountFile[indexFile];
                            fetchFunc( module.data, funcCount, funcFile[indexFile] );
                        }
#if defined(_DEBUG)
                        else
                        if ( 0 == ::lstrcmpiA(pFile, "msvcr120d.dll" ) )
                        {
                            HMODULE hModule = remoteProcess.findModule( "msvcr120d.dll" );
                            module.data.module = (DWORD64)hModule;

                            const size_t funcCount = funcCountFile[indexFile];
                            fetchFunc( module.data, funcCount, funcFile[indexFile] );
                        }
#endif
                        else
                        if ( 0 == ::lstrcmpiA(pFile, "ucrtbase.dll" ) )
                        {
                            HMODULE hModule = remoteProcess.findModule( "ucrtbase.dll" );
                            module.data.module = (DWORD64)hModule;

                            const size_t funcCount = funcCountFile[indexFile];
                            fetchFunc( module.data, funcCount, funcFile[indexFile] );
                        }
                    }
                }

                {
                    if (
                        0 != module.data.dwNew
                        && 0 != module.data.dwDelete
                        && 0 == module.data.dwNewArray
                        && 0 != module.data.dwDeleteArray
                    )
                    {
                        luckNewArray = true;
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
                                    !(kOperationMalloc == it->second
                                    || kOperationCalloc == it->second
                                    || kOperationRealloc == it->second
                                    || kOperationStrdup == it->second
                                    || kOperationWcsdup == it->second
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
                }
            }
            break;

        default:
            assert( false );
            break;
        }
    }

    return 0;
}





} // namespace checker

} // namespace kk

