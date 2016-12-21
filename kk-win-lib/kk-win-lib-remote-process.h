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

class RemoteProcess
{
public:
    RemoteProcess();
    virtual ~RemoteProcess();

public:
    bool
    init( const size_t processId );

    bool
    term( void );


public:
    HANDLE
    getHandle( void ) const
    {
        return this->mHandle;
    }
    HMODULE
    getHModule( void ) const
    {
        return this->mModule;
    }

    LPVOID
    getModuleBase( void ) const
    {
        return this->mModuleBase;
    }
    size_t
    getModuleSize( void ) const
    {
        return this->mModuleSize;
    }

public:
    HMODULE
    findModule( const char* pName ) const;

protected:
    DWORD               mProcessId;
    HANDLE              mHandle;
    HMODULE             mModule;
    LPVOID              mModuleBase;
    size_t              mModuleSize;

private:
    explicit RemoteProcess(const RemoteProcess&);
    RemoteProcess& operator=(const RemoteProcess&);
}; // class RemoteProcess


} // namespace kk

