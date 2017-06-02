//
// Replicator_native.cs
//
// Author:
// 	Jim Borden  <jim.borden@couchbase.com>
//
// Copyright (c) 2017 Couchbase, Inc All rights reserved.
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
using System.Linq;
using System.Runtime.InteropServices;

using LiteCore.Util;

namespace LiteCore.Interop
{
#if LITECORE_PACKAGED
    internal
#else
    public
#endif 
    unsafe static partial class Native
    {
        public static bool c4repl_isValidDatabaseName(string dbName)
        {
            using(var dbName_ = new C4String(dbName)) {
                return NativeRaw.c4repl_isValidDatabaseName(dbName_.AsC4Slice());
            }
        }

        public static bool c4repl_parseURL(string url, C4Address* address, C4String* dbName)
        {
            using(var url_ = new C4String(url)) {
                return NativeRaw.c4repl_parseURL(url_.AsC4Slice(), address, dbName);
            }
        }

        [DllImport(Constants.DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern C4Replicator* c4repl_new(C4Database* db, C4Address remoteAddress, C4Slice remoteDatabaseName, C4Database* otherLocalDB, C4ReplicatorMode push, C4ReplicatorMode pull, C4Slice optionsDictFleece, C4ReplicatorStatusChangedCallback onStatusChanged, void* callbackContext, C4Error* err);

        [DllImport(Constants.DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void c4repl_free(C4Replicator* repl);

        [DllImport(Constants.DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void c4repl_stop(C4Replicator* repl);

        [DllImport(Constants.DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern C4ReplicatorStatus c4repl_getStatus(C4Replicator* repl);

        [DllImport(Constants.DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern C4Slice c4repl_getResponseHeaders(C4Replicator* repl);

        [DllImport(Constants.DllName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.U1)]
        public static extern bool c4error_mayBeTransient(C4Error err);

        [DllImport(Constants.DllName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.U1)]
        public static extern bool c4error_mayBeNetworkDependent(C4Error err);


    }
    
#if LITECORE_PACKAGED
    internal
#else
    public
#endif 
    unsafe static partial class NativeRaw
    {
        [DllImport(Constants.DllName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.U1)]
        public static extern bool c4repl_isValidDatabaseName(C4Slice dbName);

        [DllImport(Constants.DllName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.U1)]
        public static extern bool c4repl_parseURL(C4Slice url, C4Address* address, C4String* dbName);


    }
}
