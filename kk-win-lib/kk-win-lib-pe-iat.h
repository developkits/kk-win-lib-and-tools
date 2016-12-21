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

namespace kk
{

class PEIAT
{
public:
    PEIAT();
    virtual ~PEIAT();

public:
    bool
    init( const void* moduleBase, const size_t moduleSize, const void* memory );

    bool
    term( void );


public:
    struct IMPORT_FUNC
    {
        const char*     nameFunc;
        const void**    addrRef;
    };

public:
    DWORD
    getSection_idata_Offset(void) const { return mOffset_idata; };

    DWORD
    getSection_idata_Size(void) const { return mSize_idata; };


    const char**
    getImportFile(void) const { return mImportFile; }

    size_t
    getImportFileCount(void) const { return mImportFileCount; }


    const IMPORT_FUNC**
    getImportFunc(void) const { return (const IMPORT_FUNC**)mImportFunc; }

    size_t*
    getImportFuncCount(void) const { return mImportFuncCount; }

protected:
    const void*         mProcessMemory;
    size_t              mProcessMemorySize;

    DWORD               mOffset_idata;
    DWORD               mSize_idata;

    const char**        mImportFile;
    size_t              mImportFileCount;

    IMPORT_FUNC**       mImportFunc;
    size_t*             mImportFuncCount;

private:
    explicit PEIAT(const PEIAT&);
    PEIAT& operator=(const PEIAT&);
}; // class PEIAT


} // namespace kk

