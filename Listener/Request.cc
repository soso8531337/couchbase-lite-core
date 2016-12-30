//
//  Request.cc
//  LiteCore
//
//  Created by Jens Alfke on 12/25/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#include "Request.hh"
#include "Writer.hh"
#include <assert.h>

namespace litecore {

    fleece::alloc_slice Response::extractOutput() {
        assert(_jsonDepth == 0);
        return _out.extractOutput();
    }

    Response& Response::writeNull() {
        comma();
        write("null");
        return *this;
    }

    Response& Response::writeBool(bool b) {
        comma();
        _out.writeJSONBool(b);
        return *this;
    }

    Response& Response::writeNumber(long n) {
        comma();
        _out.writeJSONInt(n);
        return *this;
    }

    Response& Response::writeString(const char *str) {
        comma();
        _out.writeJSONString(fleece::slice(str));
        return *this;
    }

    Response& Response::writeString(const std::string &str) {
        return writeString(str.c_str());
    }

    Response& Response::writeKey(const char *key) {
        writeString(key);
        write(":", 1);
        _jsonComma = false; // no comma before the value
        return *this;
    }

    Response& Response::nest(char bracket) {
        if (_jsonDepth++ == 0)
            setContentType("application/json");
        else if (_jsonComma)
            write(",", 1);
        write(&bracket, 1);
        _jsonComma = false;
        return *this;
    }

    Response& Response::unnest(char bracket) {
        assert(_jsonDepth > 0);
        --_jsonDepth;
        write(&bracket, 1);
        _jsonComma = true;
        return *this;
    }

    void Response::comma() {
        assert(_jsonDepth > 0);
        if (_jsonComma)
            write(",", 1);
        _jsonComma = true;
    }

}
