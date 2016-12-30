//
//  Handler.cc
//  LiteCore
//
//  Created by Jens Alfke on 12/24/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#include "Handler.hh"
#include "Request.hh"
#include "Listener.hh"
#include "c4.h"
#include <assert.h>


using namespace std;

namespace litecore {

    #define kVersionString "0.0"


    Handler::Handler(Listener &listener, const Request &req)
    :_listener(listener)
    ,_req(req)
    {
        if (_req.path.size() > 0) {
            _dbName = _req.path[0];
            if (_req.path.size() > 1) {
                _docID = _req.path[1];
            }
        }
    }


    static bool isValidDBName(const string &dbName) {
        if (dbName.empty() || !islower(dbName[0]))
            return false;
        for (char c : dbName) {
            if (!islower(c) && !isdigit(c) && !strchr("_$()+-", c))
                return false;
        }
        return true;
    }


    void Handler::run() {
        try {
            _response.set("Server", "LiteCore/" kVersionString);
            _response.status = 404;
            call_t call = nullptr;
            if (_docID.empty()) {
                // Look up route for root or db-only path:
                call = lookup(_dbName, kRootRoutes);
            } else if (isValidDBName(_dbName)) {
                // Look up route for doc path:
                call = lookup(_dbName, kDBRoutes);
            }
            if (call)
                _response.status = (this->*call)();

        } catch (const C4Error &err) {
            auto msg = c4error_getMessage(err);
            C4Warn("Handler caught C4Error(%d, %d): %.*s",
                   err.domain, err.code, (int)msg.size, (const char*)msg.buf);
            _response.status = 500;
        } catch (...) {
            C4Warn("Handler caught unknown exception");
            _response.status = 500;
        }
        _listener.sendResponse(this, _response);
    }


#pragma mark - ROUTING:


    static bool matchName(const char *name, const char *reference) {
        if (reference)
            return strcmp(name, reference) == 0;
        else
            return name[0] != '_' && name[0] != '\0';   // Any non-empty non-underscored name
    }


    Handler::call_t Handler::lookup(const string &name, const route routes[]) {
        bool nameMatched = false;
        for (const route *r = routes; r->call; ++r) {
            if (matchName(name.c_str(), r->name)) {
                nameMatched = true;
                if (r->method == _req.method)
                    return r->call;
            }
        }
        _response.status = nameMatched ? 405 : 404;
        return nullptr;
    }
    

    // Matches first component of path
    const Handler::route Handler::kRootRoutes[] = {
        {"",            Method::GET,     &Handler::doGetRoot},
        {"_all_dbs",    Method::GET,     &Handler::doAllDBs},
        {nullptr,       Method::GET,     &Handler::doGetDB},
        {nullptr,       Method::PUT,     &Handler::doPutDB},
        {}
    };

    // Matches second component of path
    const Handler::route Handler::kDBRoutes[] = {
        {"_all_docs",   Method::GET,     &Handler::doAllDocs},
        {nullptr,       Method::GET,     &Handler::doGetDoc},
        {nullptr,       Method::PUT,     &Handler::doPutDoc},
        {nullptr,       Method::POST,    &Handler::doPostDoc},
        {}
    };


#pragma mark - ROOT / DATABASE HANDLERS:

    // https://developer.couchbase.com/documentation/mobile/1.3/references/couchbase-lite/rest-api/index.html


    Status Handler::doGetRoot() {
        _response.beginObject()
                 .writeKey("couchdb").writeString("Welcome")
                 .writeKey("vendor")
                    .beginObject()
                    .writeKey("name").writeString("LiteCore")
                    .writeKey("version").writeString(kVersionString)
                    .endObject()
                 .writeKey("version").writeString(kVersionString)
                 .endObject();
        return 200;
    }


    Status Handler::doGetDB() {
        _db = _listener.getDatabase(_dbName.c_str());
        if (!_db)
            return 404;
        _response.beginObject()
                 .writeKey("db_name")
                 .writeString(_dbName)
                 .writeKey("doc_count")
                 .writeNumber(c4db_getDocumentCount(_db))
                 .writeKey("update_seq")
                 .writeNumber(c4db_getLastSequence(_db))
                 .endObject();
        return 200;
    }


    Status Handler::doPutDB() {
        _db = _listener.createDatabase(_dbName.c_str());
        return 201;
    }


    Status Handler::doAllDBs() {
        return 501;
    }


#pragma mark - DOCUMENT HANDLERS:


    Status Handler::doAllDocs() {
        return 501;
    }


    Status Handler::doGetDoc() {
        return 501;
    }


    Status Handler::doPutDoc() {
        return 501;
    }

    
    Status Handler::doPostDoc() {
        return 501;
    }

}
