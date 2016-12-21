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

#include "kk-win-lib-pe-iat.h"


#include <assert.h>

namespace kk
{

PEIAT::PEIAT()
{
    mProcessMemory = NULL;
    mProcessMemorySize = 0;

    mOffset_idata = 0;
    mSize_idata = 0;

    mImportFile = NULL;
    mImportFileCount = 0;

    mImportFunc = NULL;
    mImportFuncCount = NULL;
}

PEIAT::~PEIAT()
{
    this->term();
}

bool
PEIAT::term(void)
{
    bool result = false;

    for ( size_t index = 0; index < mImportFileCount; ++index )
    {
        if ( NULL != mImportFunc[index] )
        {
            delete [] mImportFunc[index];
            mImportFunc[index] = NULL;
        }
    }
    if ( NULL != mImportFuncCount )
    {
        delete [] mImportFuncCount;
        mImportFuncCount = NULL;
    }
    if ( NULL != mImportFunc )
    {
        delete [] mImportFunc;
        mImportFunc = NULL;
    }

    if ( NULL != mImportFile )
    {
        delete [] mImportFile;
        mImportFile = NULL;
    }
    mImportFileCount = 0;

    mOffset_idata = 0;
    mSize_idata = 0;

    mProcessMemory = NULL;
    mProcessMemorySize = 0;

    return true;
}


bool
PEIAT::init( const void* moduleBase, const size_t moduleSize, const void* memory )
{
    this->term();

    if ( NULL == moduleBase )
    {
        return false;
    }
    if ( 0 == moduleSize )
    {
        return false;
    }
    if ( 0 == memory )
    {
        return false;
    }

    const BYTE* pImageBase = reinterpret_cast<const BYTE*>(memory);
    const IMAGE_DOS_HEADER* pHeaderDOS = reinterpret_cast<const IMAGE_DOS_HEADER*>(pImageBase);
    {
        const BYTE* pMagic = reinterpret_cast<const BYTE*>(&(pHeaderDOS->e_magic));
        if ( !( 'M' == pMagic[0] && 'Z' == pMagic[1] ) )
        {
            return false;
        }
    }

    const IMAGE_NT_HEADERS32* pHeaderNT32 = reinterpret_cast<const IMAGE_NT_HEADERS32*>(pImageBase + pHeaderDOS->e_lfanew);
    const IMAGE_NT_HEADERS64* pHeaderNT64 = reinterpret_cast<const IMAGE_NT_HEADERS64*>(pImageBase + pHeaderDOS->e_lfanew);
    {
        const BYTE* pSignature = reinterpret_cast<const BYTE*>(&(pHeaderNT32->Signature));
        if ( !( 'P' == pSignature[0] && 'E' == pSignature[1] && '\0' == pSignature[2] && '\0' == pSignature[3] ) )
        {
            return false;
        }
    }

    bool is64bit = false;
    {
        if ( !
            (IMAGE_NT_OPTIONAL_HDR32_MAGIC == pHeaderNT32->OptionalHeader.Magic
            || IMAGE_NT_OPTIONAL_HDR64_MAGIC == pHeaderNT32->OptionalHeader.Magic
            )
        )
        {
            return false;
        }
    }
    {
        if ( IMAGE_NT_OPTIONAL_HDR64_MAGIC == pHeaderNT32->OptionalHeader.Magic )
        {
            is64bit = true;
        }
    }

    if ( is64bit )
    {
        const size_t countSections = pHeaderNT64->FileHeader.NumberOfSections;
        const IMAGE_SECTION_HEADER* pHeaderSectionTable = reinterpret_cast<const IMAGE_SECTION_HEADER*>(IMAGE_FIRST_SECTION(pHeaderNT64));

        for ( size_t index = 0; index < countSections; ++index )
        {
            const IMAGE_SECTION_HEADER& rHeaderSection = pHeaderSectionTable[index];
            char name[16];
            ::memcpy_s( name, sizeof(name)/sizeof(name[0]), rHeaderSection.Name, sizeof(rHeaderSection.Name) );
            name[IMAGE_SIZEOF_SHORT_NAME] = '\0';
#if 0//defined(_DEBUG)
            {
                char temp[128];
                ::wsprintfA( temp, "%s\n", name );
                ::OutputDebugStringA( temp );
            }
            {
                char temp[128];
                ::wsprintfA( temp, "  0x%08x 0x%08x 0x%08x\n", rHeaderSection.VirtualAddress, rHeaderSection.PointerToRawData, rHeaderSection.SizeOfRawData );
                ::OutputDebugStringA( temp );
            }
#endif // defined(_DEBUG)
            if ( 0 == ::lstrcmpA( name, ".idata" ) )
            {
                mOffset_idata = rHeaderSection.PointerToRawData;
                mSize_idata = rHeaderSection.SizeOfRawData;
            }
        }
    }
    else
    {
        const size_t countSections = pHeaderNT32->FileHeader.NumberOfSections;
        const IMAGE_SECTION_HEADER* pHeaderSectionTable = reinterpret_cast<const IMAGE_SECTION_HEADER*>(IMAGE_FIRST_SECTION(pHeaderNT32));

        for ( size_t index = 0; index < countSections; ++index )
        {
            const IMAGE_SECTION_HEADER& rHeaderSection = pHeaderSectionTable[index];
            char name[16];
            ::memcpy_s( name, sizeof(name)/sizeof(name[0]), rHeaderSection.Name, sizeof(rHeaderSection.Name) );
            name[IMAGE_SIZEOF_SHORT_NAME] = '\0';
#if 0//defined(_DEBUG)
            {
                char temp[128];
                ::wsprintfA( temp, "%s\n", name );
                ::OutputDebugStringA( temp );
            }
            {
                char temp[128];
                ::wsprintfA( temp, "  0x%08x 0x%08x 0x%08x\n", rHeaderSection.VirtualAddress, rHeaderSection.PointerToRawData, rHeaderSection.SizeOfRawData );
                ::OutputDebugStringA( temp );
            }
#endif // defined(_DEBUG)
            if ( 0 == ::lstrcmpA( name, ".idata" ) )
            {
                mOffset_idata = rHeaderSection.PointerToRawData;
                mSize_idata = rHeaderSection.SizeOfRawData;
            }
        }
    }

    if ( is64bit )
    {
        // IMAGE_DIRECTORY_ENTRY_IAT
        const DWORD dwImportRVA = pHeaderNT64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
        const DWORD dwImportSize = pHeaderNT64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;

        const IMAGE_IMPORT_DESCRIPTOR* pImportDescStart = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(pImageBase + dwImportRVA);
        const IMAGE_IMPORT_DESCRIPTOR* pImportDescEnd   = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(pImageBase + dwImportRVA + dwImportSize);

        const IMAGE_IMPORT_DESCRIPTOR* pImportDesc = NULL;

        size_t countImportFile = 0;
        {
            pImportDesc = pImportDescStart;
            for ( countImportFile = 0; pImportDesc < pImportDescEnd; ++pImportDesc, ++countImportFile )
            {
                if ( 0 == pImportDesc->Name )
                {
                    break;
                }
            }

            this->mImportFile = new const char*[countImportFile];
            for ( size_t index = 0; index < countImportFile; ++index )
            {
                this->mImportFile[index] = NULL;
            }
            this->mImportFileCount = countImportFile;

            this->mImportFunc = new IMPORT_FUNC*[countImportFile];
            for ( size_t index = 0; index < countImportFile; ++index )
            {
                this->mImportFunc[index] = NULL;
            }
            this->mImportFuncCount = new size_t[countImportFile];
            for ( size_t index = 0; index < countImportFile; ++index )
            {
                this->mImportFuncCount[index] = 0;
            }

            pImportDesc = pImportDescStart;
            for ( size_t indexImportFile = 0; pImportDesc < pImportDescEnd; ++pImportDesc, ++indexImportFile )
            {
                if ( 0 == pImportDesc->Name )
                {
                    break;
                }

                size_t countImportFunc = 0;
                {
                    const IMAGE_THUNK_DATA64* pImportThunk = ((const IMAGE_THUNK_DATA64*)(pImageBase + pImportDesc->OriginalFirstThunk));
                    for ( size_t index = 0; ; ++index )
                    {
                        if ( 0 == pImportThunk[index].u1.Ordinal )
                        {
                            countImportFunc = index;
                            break;
                        }
                    }
                }

                this->mImportFuncCount[indexImportFile] = countImportFunc;
                this->mImportFunc[indexImportFile] = new IMPORT_FUNC[countImportFunc];
                for ( size_t index = 0; index < countImportFunc; ++index )
                {
                    this->mImportFunc[indexImportFile][index].nameFunc = NULL;
                    this->mImportFunc[indexImportFile][index].addrRef  = NULL;
                }
            }

        }


        size_t indexImportFile = 0;
        pImportDesc = pImportDescStart;
        for ( ; pImportDesc < pImportDescEnd; ++pImportDesc, ++indexImportFile )
        {
            if ( 0 == pImportDesc->Name )
            {
                break;
            }

            const char* pImportFileName = reinterpret_cast<const char*>(pImageBase + pImportDesc->Name);
            this->mImportFile[indexImportFile] = pImportFileName;
#if 0//defined(_DEBUG)
            {
                char temp[128];
                ::wsprintfA( temp, "%s\n", pImportFileName );
                ::OutputDebugStringA( temp );
            }
#endif // defined(_DEBUG)
            const IMAGE_THUNK_DATA64* pImportThunk = ((const IMAGE_THUNK_DATA64*)(pImageBase + pImportDesc->OriginalFirstThunk));
            const IMAGE_THUNK_DATA64* pImportAddrTable = ((const IMAGE_THUNK_DATA64*)(pImageBase + pImportDesc->FirstThunk));
            for ( size_t index = 0; ; ++index )
            {
                if ( 0 == pImportThunk[index].u1.Ordinal )
                {
                    break;
                }

                const void* pImportAddr = (const void*)(pImportAddrTable[index].u1.Function);
                this->mImportFunc[indexImportFile][index].addrRef = (const void**)((const BYTE*)(&pImportAddrTable[index])-pImageBase);
                if ( IMAGE_SNAP_BY_ORDINAL64( pImportThunk[index].u1.Ordinal ) )
                {
                    const DWORD value = (const DWORD)((pImportThunk[index].u1.Ordinal) & (~IMAGE_ORDINAL_FLAG64));
#if 0//defined(_DEBUG)
                    {
                        char temp[128];
                        ::wsprintfA( temp, "  %p(%p) %6u\n", &pImportAddrTable[index], pImportAddr, value);
                        ::OutputDebugStringA( temp );
                    }
#endif // defined(_DEBUG)
                    this->mImportFunc[indexImportFile][index].nameFunc = reinterpret_cast<const char *>(pImportThunk[index].u1.Ordinal);
                }
                else
                {
                    const IMAGE_IMPORT_BY_NAME* pImport = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(pImageBase + pImportThunk[index].u1.AddressOfData);
                    const char* pImportName = reinterpret_cast<const char*>(pImport->Name);
#if 0//defined(_DEBUG)
                    {
                        char temp[128];
                        ::wsprintfA( temp, "  %p(%p) %6u %s\n", &pImportAddrTable[index], pImportAddr, (unsigned int)pImport->Hint, pImportName );
                        ::OutputDebugStringA( temp );
                    }
#endif // defined(_DEBUG)
                    this->mImportFunc[indexImportFile][index].nameFunc = pImportName;
                }
            }
        }
    }
    else
    {
        const DWORD dwImportRVA = pHeaderNT32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
        const DWORD dwImportSize = pHeaderNT32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;

        const IMAGE_IMPORT_DESCRIPTOR* pImportDescStart = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(pImageBase + dwImportRVA);
        const IMAGE_IMPORT_DESCRIPTOR* pImportDescEnd   = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(pImageBase + dwImportRVA + dwImportSize);

        const IMAGE_IMPORT_DESCRIPTOR* pImportDesc = NULL;

        size_t countImportFile = 0;
        {
            pImportDesc = pImportDescStart;
            for ( countImportFile = 0; pImportDesc < pImportDescEnd; ++pImportDesc, ++countImportFile )
            {
                if ( 0 == pImportDesc->Name )
                {
                    break;
                }
            }

            this->mImportFile = new const char*[countImportFile];
            for ( size_t index = 0; index < countImportFile; ++index )
            {
                this->mImportFile[index] = NULL;
            }
            this->mImportFileCount = countImportFile;

            this->mImportFunc = new IMPORT_FUNC*[countImportFile];
            for ( size_t index = 0; index < countImportFile; ++index )
            {
                this->mImportFunc[index] = NULL;
            }
            this->mImportFuncCount = new size_t[countImportFile];
            for ( size_t index = 0; index < countImportFile; ++index )
            {
                this->mImportFuncCount[index] = 0;
            }

            pImportDesc = pImportDescStart;
            for ( size_t indexImportFile = 0; pImportDesc < pImportDescEnd; ++pImportDesc, ++indexImportFile )
            {
                if ( 0 == pImportDesc->Name )
                {
                    break;
                }

                size_t countImportFunc = 0;
                {
                    const IMAGE_THUNK_DATA32* pImportThunk = ((const IMAGE_THUNK_DATA32*)(pImageBase + pImportDesc->OriginalFirstThunk));
                    for ( size_t index = 0; ; ++index )
                    {
                        if ( 0 == pImportThunk[index].u1.Ordinal )
                        {
                            countImportFunc = index;
                            break;
                        }
                    }
                }

                this->mImportFuncCount[indexImportFile] = countImportFunc;
                this->mImportFunc[indexImportFile] = new IMPORT_FUNC[countImportFunc];
                for ( size_t index = 0; index < countImportFunc; ++index )
                {
                    this->mImportFunc[indexImportFile][index].nameFunc = NULL;
                    this->mImportFunc[indexImportFile][index].addrRef  = NULL;
                }
            }

        }


        size_t indexImportFile = 0;
        pImportDesc = pImportDescStart;
        for ( ; pImportDesc < pImportDescEnd; ++pImportDesc, ++indexImportFile )
        {
            if ( 0 == pImportDesc->Name )
            {
                break;
            }

            const char* pImportFileName = reinterpret_cast<const char*>(pImageBase + pImportDesc->Name);
            this->mImportFile[indexImportFile] = pImportFileName;
#if 0//defined(_DEBUG)
            {
                char temp[128];
                ::wsprintfA( temp, "%s\n", pImportFileName );
                ::OutputDebugStringA( temp );
            }
#endif // defined(_DEBUG)
            const IMAGE_THUNK_DATA32* pImportThunk = ((const IMAGE_THUNK_DATA32*)(pImageBase + pImportDesc->OriginalFirstThunk));
            const IMAGE_THUNK_DATA32* pImportAddrTable = ((const IMAGE_THUNK_DATA32*)(pImageBase + pImportDesc->FirstThunk));
            for ( size_t index = 0; ; ++index )
            {
                if ( 0 == pImportThunk[index].u1.Ordinal )
                {
                    break;
                }
                const void* pImportAddr = (const void*)(pImportAddrTable[index].u1.Function);
                this->mImportFunc[indexImportFile][index].addrRef = (const void**)((const BYTE*)(&pImportAddrTable[index])-pImageBase);
                if ( IMAGE_SNAP_BY_ORDINAL32( pImportThunk[index].u1.Ordinal ) )
                {
                    const DWORD value = (pImportThunk[index].u1.Ordinal) & (~IMAGE_ORDINAL_FLAG32);
#if 0//defined(_DEBUG)
                    {
                        char temp[128];
                        ::wsprintfA( temp, "  %p(%p) %6u\n", &pImportAddrTable[index], pImportAddr, value);
                        ::OutputDebugStringA( temp );
                    }
#endif // defined(_DEBUG)
                    this->mImportFunc[indexImportFile][index].nameFunc = reinterpret_cast<const char *>(pImportThunk[index].u1.Ordinal);
                }
                else
                {
                    const IMAGE_IMPORT_BY_NAME* pImport = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(pImageBase + pImportThunk[index].u1.AddressOfData);
                    const char* pImportName = reinterpret_cast<const char*>(pImport->Name);
#if 0//defined(_DEBUG)
                    {
                        char temp[128];
                        ::wsprintfA( temp, "  %p(%p) %6u %s\n", &pImportAddrTable[index], pImportAddr, (unsigned int)pImport->Hint, pImportName );
                        ::OutputDebugStringA( temp );
                    }
#endif // defined(_DEBUG)
                    this->mImportFunc[indexImportFile][index].nameFunc = pImportName;
                }
            }
        }

    }

    mProcessMemory = moduleBase;
    mProcessMemorySize = moduleSize;

    return true;
}








} // namespace kk

