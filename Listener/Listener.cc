//
//  Listener.cc
//  LiteCore
//
//  Created by Jens Alfke on 12/23/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#include "Listener.hh"
#include "Handler.hh"
#include <assert.h>
#include <errno.h>

using namespace std;


#ifdef _MSC_VER
static const char  kSeparatorChar = '\\';
#else
static const char  kSeparatorChar = '/';
#endif


namespace litecore {

    static C4Slice slice(const string &s) {
        return {s.data(), s.size()};
    }


    Listener::Listener(const std::string &dbsPath)
    :_dbsPath(dbsPath)
    { }

    Listener::~Listener()
    { }


    C4Database* Listener::getDatabase(const char *name, bool createOnly) {
        c4Database *db = _dbs[name];
        if (db) {
            if (createOnly)
                throw C4Error{POSIXDomain, EEXIST};
        } else {
            string path = _dbsPath + kSeparatorChar + name;
            C4DatabaseConfig config = {
                kC4DB_Bundled,
                kC4SQLiteStorageEngine,
                kC4RevisionTrees
            };
            if (createOnly)
                config.flags |= kC4DB_Create | kC4DB_CreateOnly;
            C4Error err;
            db = c4db_open(slice(path), &config, &err);
            if (!db)
                throw err;
            _dbs[name] = db;
        }
        return db;
    }

}
