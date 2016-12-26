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

        , kOperationNew = 10
        , kOperationDelete = 11
        , kOperationNewArray = 12
        , kOperationDeleteArray = 13

        , kOperationAlignedMalloc = 20
        , kOperationAlignedFree = 21
        , kOperationAlignedRealloc = 22
        , kOperationAlignedRecalloc = 23
        , kOperationAlignedReallocFree = 24
        , kOperationAlignedRecallocFree = 25

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

        , kIndexOperationMAX = 14
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

