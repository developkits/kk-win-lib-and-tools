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


namespace kk
{

namespace checker
{

#include <pshpack4.h>

struct packetHeader
{
    DWORD       size;
    DWORD       mode;
};

enum enumMode
{
    kModeAction = 1
    , kModeProcessId = 2
    , kModeCRTModule = 3
    , kModeMemoryOperation = 4
};


enum enumAction
{
    kActionNone = 1
    , kActionBreak = 2
};

struct dataAction
{
    DWORD   action;
};

struct packetAction
{
    packetHeader    header;
    dataAction      data;
};

struct dataProcessId
{
    DWORD   processId;
};

struct packetProcessId
{
    packetHeader    header;
    dataProcessId   data;
};

enum enumCRTVersion
{
    enmVS6      =  60
    , enmVS2002 =  70
    , enmVS2003 =  71
    , enmVS2005 =  80
    , enmVS2008 =  90
    , enmVS2010 = 100
    , enmVS2012 = 110
    , enmVS2013 = 120

    , enmVS2015 = 140
    , enmVS2017 = 150
};

struct dataCRTModuleInfo
{
    enumCRTVersion  version;
    bool            isDebug;

    bool            isCRTStaticLinked;
};

union unionCRTModuleInfo
{
    DWORD64             pack;
    dataCRTModuleInfo   info;
};

struct dataCRTModule
{
    DWORD64     module;
    unionCRTModuleInfo  moduleInfo;

    DWORD64     dwMalloc;
    DWORD64     dwFree;
    DWORD64     dwCalloc;
    DWORD64     dwRealloc;
    DWORD64     dwStrdup;
    DWORD64     dwWcsdup;
    DWORD64     dwReCalloc;
    DWORD64     dwExpand;

    DWORD64     dwNew;
    DWORD64     dwDelete;

    DWORD64     dwNewArray;
    DWORD64     dwDeleteArray;

    DWORD64     dwAlignedMalloc;
    DWORD64     dwAlignedFree;
    DWORD64     dwAlignedReCalloc;
    DWORD64     dwAlignedRealloc;
};

struct dataCRTStaticFunc
{
    DWORD64     dwCRTStaticNew;
    DWORD64     dwCRTStaticNewLength;
    DWORD64     dwCRTStaticDelete;
    DWORD64     dwCRTStaticDeleteLength;
    DWORD64     dwCRTStaticNewArray;
    DWORD64     dwCRTStaticNewArrayLength;
    DWORD64     dwCRTStaticDeleteArray;
    DWORD64     dwCRTStaticDeleteArrayLength;

    DWORD64     dwCRTStaticDeleteSize;
    DWORD64     dwCRTStaticDeleteSizeLength;
    DWORD64     dwCRTStaticDeleteArraySize;
    DWORD64     dwCRTStaticDeleteArraySizeLength;
};

struct dataUserStaticFunc
{
    DWORD64     dwUserStaticNew;
    DWORD64     dwUserStaticNewLength;
    DWORD64     dwUserStaticDelete;
    DWORD64     dwUserStaticDeleteLength;
    DWORD64     dwUserStaticNewArray;
    DWORD64     dwUserStaticNewArrayLength;
    DWORD64     dwUserStaticDeleteArray;
    DWORD64     dwUserStaticDeleteArrayLength;

    DWORD64     dwUserStaticDeleteSize;
    DWORD64     dwUserStaticDeleteSizeLength;
    DWORD64     dwUserStaticDeleteArraySize;
    DWORD64     dwUserStaticDeleteArraySizeLength;

    DWORD64     dwUserStaticNewArg3;
    DWORD64     dwUserStaticNewArg3Length;
    DWORD64     dwUserStaticDeleteArg3;
    DWORD64     dwUserStaticDeleteArg3Length;
    DWORD64     dwUserStaticNewArrayArg3;
    DWORD64     dwUserStaticNewArrayArg3Length;
    DWORD64     dwUserStaticDeleteArrayArg3;
    DWORD64     dwUserStaticDeleteArrayArg3Length;
    DWORD64     dwUserStaticNewArg4;
    DWORD64     dwUserStaticNewArg4Length;
    DWORD64     dwUserStaticDeleteArg4;
    DWORD64     dwUserStaticDeleteArg4Length;
    DWORD64     dwUserStaticNewArrayArg4;
    DWORD64     dwUserStaticNewArrayArg4Length;
    DWORD64     dwUserStaticDeleteArrayArg4;
    DWORD64     dwUserStaticDeleteArrayArg4Length;

};

struct packetCRTModule
{
    packetHeader    header;
    dataCRTModule   data;
    dataCRTStaticFunc   func;
    dataUserStaticFunc  user;
};

struct dataMemoryOperation
{
    DWORD       funcMemoryOperation;
    DWORD64     pointer;
};

struct packetMemoryOperation
{
    packetHeader        header;
    dataMemoryOperation data;
};


#include <poppack.h>

static
const char* szNamedPiepeName = "kk-win-checker-memory-operation-mismatch";







} // namespace checker

} // namespace kk

