﻿//
// Base.cs
//
// Author:
// 	Jim Borden  <jim.borden@couchbase.com>
//
// Copyright (c) 2016 Couchbase, Inc All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;

using LiteCore.Util;

namespace LiteCore.Interop
{
    public enum LiteCoreError {
        AssertionFailed = 1,    // Internal assertion failure
        Unimplemented,          // Oops, an unimplemented API call
        NoSequences,            // This KeyStore does not support sequences
        UnsupportedEncryption,  // Unsupported encryption algorithm
        NoTransaction,          // Function must be called within a transaction
        BadRevisionID,          // Invalid revision ID syntax
        BadVersionVector,       // Invalid version vector syntax
        CorruptRevisionData,    // Revision contains corrupted/unreadable data
        CorruptIndexData,       // Index contains corrupted/unreadable data
        TokenizerError, /*10*/  // can't create text tokenizer for FTS
        NotOpen,                // Database/KeyStore/index is not open
        NotFound,               // Document not found
        Deleted,                // Document has been deleted
        Conflict,               // Document update conflict
        InvalidParameter,       // Invalid function parameter or struct value
        DatabaseError,          // Lower-level database error (ForestDB or SQLite)
        UnexpectedError,        // Internal unexpected C++ exception
        CantOpenFile,           // Database file can't be opened; may not exist
        IOError,                // File I/O error
        CommitFailed, /*20*/    // Transaction commit failed
        MemoryError,            // Memory allocation failed (out of memory?)
        NotWriteable,           // File is not writeable
        CorruptData,            // Data is corrupted
        Busy,                   // Database is busy/locked
        NotInTransaction,       // Function cannot be called while in a transaction
        TransactionNotClosed,   // Database can't be closed while a transaction is open
        IndexBusy,              // View can't be closed while index is enumerating
        Unsupported,            // Operation not supported in this database
        NotADatabaseFile,       // File is not a database, or encryption key is wrong
        WrongFormat, /*30*/     // Database exists but not in the format/storage requested
        Crypto,                 // Encryption/decryption error
        InvalidQuery,           // Invalid query
        MissingIndex            // No such index, or query requires a nonexistent index
    };

    public unsafe partial struct C4Error
    {
        public C4Error(C4ErrorDomain domain, int code)
        {
            this.code = code;
            this.domain = domain;
        }

        public C4Error(SQLiteStatus code) : this(C4ErrorDomain.SQLiteDomain, (int)code)
        {
            
        }

        public C4Error(LiteCoreError code) : this(C4ErrorDomain.LiteCoreDomain, (int)code)
        {
        }

        public C4Error(FLError code) : this(C4ErrorDomain.FleeceDomain, (int)code)
        {

        }
    }

    public unsafe partial struct C4Slice : IEnumerable<byte>
    {
        public static readonly C4Slice Null = new C4Slice(null, 0);

        public C4Slice(void* buf, ulong size)
        {
            this.buf = buf;
            this._size = new UIntPtr(size);
        }

        public static C4Slice Constant(string input)
        {
            return (C4Slice)FLSlice.Constant(input);
        }

        public static C4Slice Allocate(string input)
        {
            return (C4Slice)FLSlice.Allocate(input);
        }

        public static void Free(C4Slice slice)
        {
            FLSlice.Free((FLSlice)slice);
            slice.buf = null;
            slice.size = 0;
        }

        private bool Equals(C4Slice other)
        {
            return Native.c4SliceEqual(this, other);
        }

        private bool Equals(string other)
        {
            var c4str = new C4String(other);
            return Equals(c4str.AsC4Slice());
        }

        public string CreateString()
        {
            if(buf == null) {
                return null;
            }

            var bytes = ToArrayFast();
            return Encoding.UTF8.GetString(bytes, 0, bytes.Length);
        }

        public byte[] ToArrayFast()
        {
            if(buf == null) {
                return null;
            }

            var tmp = new IntPtr(buf);
            var bytes = new byte[size];
            Marshal.Copy(tmp, bytes, 0, bytes.Length);
            return bytes;
        }

        public static explicit operator C4Slice(FLSlice input)
        {
            return new C4Slice(input.buf, input.size);
        }

        public static explicit operator FLSlice(C4Slice input)
        {
            return new FLSlice(input.buf, input.size);
        }

        #pragma warning disable 1591

        public override string ToString()
        {
            return String.Format("C4Slice[\"{0}\"]", CreateString());
        }

        public override bool Equals(object obj)
        {
            if(obj is C4Slice) {
                return Equals((C4Slice)obj);
            }

            var str = obj as string;
            return str != null && Equals(str);
        }

        public override int GetHashCode()
        {
            unchecked {
                int hash = 17;

                hash = hash * 23 + (int)size;
                var ptr = (byte*)buf;
                if(ptr != null) {
                    hash = hash * 23 + ptr[size - 1];
                }

                return hash;
            }
        }

        public IEnumerator<byte> GetEnumerator()
        {
            return new C4SliceEnumerator(buf, (int)size);
        }

        System.Collections.IEnumerator System.Collections.IEnumerable.GetEnumerator()
        {
            return GetEnumerator();
        }
#pragma warning restore 1591
    }

    public unsafe struct C4SliceResult : IDisposable
    {
        public void* buf;
        private UIntPtr _size;

        public ulong size
        {
            get {
                return _size.ToUInt64();
            }
            set {
                _size = (UIntPtr)value;
            }
        }

        public static implicit operator C4Slice(C4SliceResult input)
        {
            return new C4Slice(input.buf, input.size);
        }

        public void Dispose()
        {
            Native.c4slice_free(this);
        }
    }

    public static unsafe partial class Native
    {
        static Native()
        {
            if(RuntimeInformation.IsOSPlatform(OSPlatform.Windows)) {
                var codeBase = AppContext.BaseDirectory;
                if(!codeBase.EndsWith("\\"))
                {
                    codeBase = codeBase + "\\";
                }

                UriBuilder uri = new UriBuilder(codeBase);
                var directory = Path.GetDirectoryName(Uri.UnescapeDataString(uri.Path));

                Debug.Assert(Path.IsPathRooted(directory), "directory is not rooted.");
                var architecture = IntPtr.Size == 4
                    ? "x86"
                    : "x64";

                var dllPath = Path.Combine(directory, architecture, "LiteCore.dll");
                var dllPathUWP = Path.Combine(directory, "LiteCore-Interop", architecture, "LiteCore.dll");
                var dllPathASP = Path.Combine(directory, "bin", architecture, "LiteCore.dll");
                var foundPath = default(string);
                foreach(var path in new[] {  dllPath, dllPathUWP, dllPathASP }) {
                    foundPath = File.Exists(path) ? path : null; 
                    if(foundPath != null) {
                        break;
                    }
                }

                if(foundPath == null) {
                    Console.WriteLine("Could not find LiteCore.dll!  Nothing is going to work!");
                    throw new LiteCoreException(new C4Error(LiteCoreError.UnexpectedError));
                }

                const uint LOAD_WITH_ALTERED_SEARCH_PATH = 8;
                var ptr = LoadLibraryEx(foundPath, IntPtr.Zero, LOAD_WITH_ALTERED_SEARCH_PATH);
                if(ptr == IntPtr.Zero) {
                    Console.WriteLine("Could not load LiteCore.dll!  Nothing is going to work!");
                    throw new LiteCoreException(new C4Error(LiteCoreError.UnexpectedError));
                }
            }
        }

        [DllImport("kernel32")]
        private static extern IntPtr LoadLibraryEx(string lpFileName, IntPtr hFile, uint dwFlags);
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void C4LogCallback(C4LogDomain domain, C4LogLevel level, C4Slice message);
}
