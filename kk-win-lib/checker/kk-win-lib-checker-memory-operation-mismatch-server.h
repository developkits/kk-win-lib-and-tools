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

class MemoryOperationMismatchServer : public MemoryOperationMismatch
{
public:
    MemoryOperationMismatchServer();
    virtual ~MemoryOperationMismatchServer();

public:
    virtual
    bool
    init( const bool actionDoBreak );

    virtual
    bool
    term( void );

public:
    bool
    serverStop( void );

    bool
    serverStart( void );

    bool
    serverWaitFinish( void );

protected:
    static
    unsigned int
    __stdcall
    threadServer( void* pVoid );

    bool
    responseError( void* pBuff, size_t& sendedSize );

protected:
    DWORD64             mCRTOffsetIAT[kIndexOperationMAX];


public:
    bool
    setBreakDeallocNull( const bool needBreak );

    bool
    setBreakAllocNull( const bool needBreak );

    bool
    setBreakMismatch( const bool needBreak );

protected:
    bool                mNeedBreakDeallocNull;
    bool                mNeedBreakAllocNull;
    bool                mNeedBreakMismatch;

private:
    explicit MemoryOperationMismatchServer(const MemoryOperationMismatchServer&);
    MemoryOperationMismatchServer& operator=(const MemoryOperationMismatchServer&);
}; // class MemoryOperationMismatchServer


} // namespace checker

} // namespace kk

