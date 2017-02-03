/**
 * The MIT License
 * 
 * Copyright (C) 2017 Kiyofumi Kondoh
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

class HookLocal
{
public:
    HookLocal();
    virtual ~HookLocal();

public:
    bool
    init( const void* minAddr, const void* maxAddr, const size_t countHook );

    bool
    term( void );

public:
    bool
    dropWriteOperation( void );

    bool
    grantWriteOperation( void );

    bool
    hook( size_t& indexHook, const void* moduleBase, const size_t nOrigOffset, const size_t nOrigSize, const void* hookFunc, void** origFunc );

    bool
    unhook( const size_t indexHook );

protected:
    void*               mPageTrampoline;
    size_t              mPageTrampolineSize;
    size_t              mHookIndexMax;
    size_t              mHookIndexCurrent;

private:
    explicit HookLocal(const HookLocal&);
    HookLocal& operator=(const HookLocal&);
}; // class HookLocal


} // namespace kk

