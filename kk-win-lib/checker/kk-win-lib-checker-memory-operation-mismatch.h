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

        , kIndexOperationCRTNewAOP = 16
        , kIndexOperationCRTNewAOPSize = 17

        , kIndexOperationMAX = 18
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

