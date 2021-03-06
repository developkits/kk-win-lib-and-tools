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

namespace checker
{


class MemoryOperationMismatch
{
public:
    MemoryOperationMismatch();
    virtual ~MemoryOperationMismatch();

public:
    virtual
    bool
    init( const bool actionDoBreak );

    virtual
    bool
    term( void );

public:
    enum enumMemoryOperation
    {
        kOperationMalloc = 1
        , kOperationFree = 2
        , kOperationCalloc = 3
        , kOperationRealloc = 4
        , kOperationReallocFree = 5
        , kOperationStrdup = 6
        , kOperationWcsdup = 7
        , kOperationReCalloc = 8
        , kOperationReCallocFree = 9
        , kOperationExpand = 10
        , kOperationExpandFree = 11

        , kOperationNew = 0x20+0
        , kOperationDelete = 0x20+1
        , kOperationNewArray = 0x20+2
        , kOperationDeleteArray = 0x20+3

        , kOperationAlignedMalloc = 0x40+0
        , kOperationAlignedFree = 0x40+1
        , kOperationAlignedRealloc = 0x40+2
        , kOperationAlignedRecalloc = 0x40+3
        , kOperationAlignedReallocFree = 0x40+4
        , kOperationAlignedRecallocFree = 0x40+5


        , kOperationCRTStaticNew = 0x60+0
        , kOperationCRTStaticDelete = 0x60+1
        , kOperationCRTStaticNewArray = 0x60+2
        , kOperationCRTStaticDeleteArray = 0x60+3

        , kOperationCRTStaticDeleteSize = 0x60+4
        , kOperationCRTStaticDeleteArraySize = 0x60+5

        , kOperationUserStaticNew = 0x80+0
        , kOperationUserStaticDelete = 0x80+1
        , kOperationUserStaticNewArray = 0x80+2
        , kOperationUserStaticDeleteArray = 0x80+3

        , kOperationUserStaticDeleteSize = 0x80+4
        , kOperationUserStaticDeleteArraySize = 0x80+5

    };


    enum enumIndexMemoryOperation
    {
        kIndexOperationMalloc = 0
        , kIndexOperationFree = 1
        , kIndexOperationCalloc = 2
        , kIndexOperationRealloc = 3

        , kIndexOperationNew = 4
        , kIndexOperationDelete = 5
        , kIndexOperationNewArray = 6
        , kIndexOperationDeleteArray = 7

        , kIndexOperationAlignedMalloc = 8
        , kIndexOperationAlignedFree = 9
        , kIndexOperationAlignedReCalloc = 10
        , kIndexOperationAlignedRealloc = 11

        , kIndexOperationStrdup = 12
        , kIndexOperationWcsdup = 13
        , kIndexOperationReCalloc = 14
        , kIndexOperationExpand = 15

        , kIndexOperationMAX = 16
    };

    enum enumIndexCRTStaticFunc
    {
        kIndexCRTStaticFuncNew = 0
        , kIndexCRTStaticFuncNewLength = 1
        , kIndexCRTStaticFuncDelete = 2
        , kIndexCRTStaticFuncDeleteLength = 3
        , kIndexCRTStaticFuncNewArray = 4
        , kIndexCRTStaticFuncNewArrayLength = 5
        , kIndexCRTStaticFuncDeleteArray = 6
        , kIndexCRTStaticFuncDeleteArrayLength = 7

        // since C++14
        , kIndexCRTStaticFuncDeleteSize = 8
        , kIndexCRTStaticFuncDeleteSizeLength = 9
        , kIndexCRTStaticFuncDeleteArraySize = 10
        , kIndexCRTStaticFuncDeleteArraySizeLength = 11

        , kIndexCRTStaticFuncMAX = 12
    };

    enum enumIndexUserStaticFunc
    {
        kIndexUserStaticFuncNew = 0
        , kIndexUserStaticFuncNewLength = 1
        , kIndexUserStaticFuncDelete = 2
        , kIndexUserStaticFuncDeleteLength = 3
        , kIndexUserStaticFuncNewArray = 4
        , kIndexUserStaticFuncNewArrayLength = 5
        , kIndexUserStaticFuncDeleteArray = 6
        , kIndexUserStaticFuncDeleteArrayLength = 7

        // since C++14
        , kIndexUserStaticFuncDeleteSize = 8
        , kIndexUserStaticFuncDeleteSizeLength = 9
        , kIndexUserStaticFuncDeleteArraySize = 10
        , kIndexUserStaticFuncDeleteArraySizeLength = 11


        , kIndexUserStaticFuncNewArg3 = 12
        , kIndexUserStaticFuncNewArg3Length = 13
        , kIndexUserStaticFuncDeleteArg3 = 14
        , kIndexUserStaticFuncDeleteArg3Length = 15
        , kIndexUserStaticFuncNewArrayArg3 = 16
        , kIndexUserStaticFuncNewArrayArg3Length = 17
        , kIndexUserStaticFuncDeleteArrayArg3 = 18
        , kIndexUserStaticFuncDeleteArrayArg3Length = 19
        , kIndexUserStaticFuncNewArg4 = 20
        , kIndexUserStaticFuncNewArg4Length = 21
        , kIndexUserStaticFuncDeleteArg4 = 22
        , kIndexUserStaticFuncDeleteArg4Length = 23
        , kIndexUserStaticFuncNewArrayArg4 = 24
        , kIndexUserStaticFuncNewArrayArg4Length = 25
        , kIndexUserStaticFuncDeleteArrayArg4 = 26
        , kIndexUserStaticFuncDeleteArrayArg4Length = 27

        , kIndexUserStaticFuncMAX = 28
    };

protected:
    kk::NamedPipe       mNamedPipe;
    bool                mDoBreak;

private:
    explicit MemoryOperationMismatch(const MemoryOperationMismatch&);
    MemoryOperationMismatch& operator=(const MemoryOperationMismatch&);
}; // class MemoryOperationMismatch


} // namespace checker

} // namespace kk

