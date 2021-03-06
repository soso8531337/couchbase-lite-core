//
//  Error.cc
//  Couchbase Lite Core
//
//  Created by Jens Alfke on 3/4/16.
//  Copyright (c) 2016 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#include "Error.hh"
#include "Logging.hh"
#include "FleeceException.hh"
#include <sqlite3.h>
#include <SQLiteCpp/Exception.h>
#include <errno.h>
#include <string>

#if __ANDROID__
#include <android/log.h>
#elif defined(_MSC_VER)
#include "asprintf.h"
#endif

#if defined(__clang__) && !defined(__ANDROID__) // For logBacktrace:
#include <execinfo.h>   // Not available in Windows?
#include <unistd.h>
#include <cxxabi.h>
#endif

namespace litecore {

    using namespace std;


#pragma mark ERROR CODES, NAMES, etc.


    struct codeMapping { int err; error::Domain domain; int code; };

    static const codeMapping kPOSIXMapping[] = {
        {ENOENT,                        error::LiteCore,    error::NotFound},
        {0, /*must end with err=0*/     error::LiteCore,    0},
    };

    static const codeMapping kSQLiteMapping[] = {
        {SQLITE_PERM,                   error::LiteCore,    error::NotWriteable},
        {SQLITE_BUSY,                   error::LiteCore,    error::Busy},
        {SQLITE_LOCKED,                 error::LiteCore,    error::Busy},
        {SQLITE_NOMEM,                  error::LiteCore,    error::MemoryError},
        {SQLITE_READONLY,               error::LiteCore,    error::NotWriteable},
        {SQLITE_IOERR,                  error::LiteCore,    error::IOError},
        {SQLITE_CORRUPT,                error::LiteCore,    error::CorruptData},
        {SQLITE_FULL,                   error::POSIX,       ENOSPC},
        {SQLITE_CANTOPEN,               error::LiteCore,    error::CantOpenFile},
        {SQLITE_NOTADB,                 error::LiteCore,    error::NotADatabaseFile},
        {SQLITE_PERM,                   error::LiteCore,    error::NotWriteable},
        {0, /*must end with err=0*/     error::LiteCore,    0},
    };
    //TODO: Map the SQLite 'extended error codes' that give more detail about file errors

    static const codeMapping kFleeceMapping[] = {
        {fleece::MemoryError,           error::LiteCore,    error::MemoryError},
        {fleece::JSONError,             error::LiteCore,    error::InvalidQuery},
        {fleece::PathSyntaxError,       error::LiteCore,    error::InvalidQuery},
        {0, /*must end with err=0*/     error::Fleece,      0},
    };

    static bool mapError(error::Domain &domain, int &code, const codeMapping table[]) {
        for (const codeMapping *row = &table[0]; row->err != 0; ++row) {
            if (row->err == code) {
                domain = row->domain;
                code = row->code;
                return true;
            }
        }
        return false;
    }


    // Indexed by C4ErrorDomain
    static const char* kDomainNames[] = {"LiteCore", "POSIX", "ForestDB", "SQLite"};

    static const char* litecore_errstr(error::LiteCoreError code) {
        static const char* kLiteCoreMessages[] = {
            // These must match up with the codes in the declaration of LiteCoreError
            "no error", // 0
            "assertion failed",
            "unimplemented function called",
            "database doesn't support sequences",
            "unsupported encryption algorithm",
            "call must be made in a transaction",
            "bad revision ID",
            "bad version vector",
            "corrupt revision data",
            "corrupt index",
            "text tokenizer error",
            "database not open",
            "not found",
            "deleted",
            "conflict",
            "invalid parameter",
            "database error",
            "unexpected exception",
            "can't open file",
            "file I/O error",
            "commit failed",
            "memory allocation failed",
            "not writeable",
            "file data is corrupted",
            "database busy/locked",
            "must be called during a transaction",
            "transaction not closed",
            "index busy; can't close view",
            "unsupported operation for this database type",
            "file is not a database (or encryption key is invalid/missing)",
            "file/data is not in the requested format",
            "encryption/decryption error",
            "query syntax error",
            "missing database index",
            "invalid query parameter name/number",
        };
        static_assert(sizeof(kLiteCoreMessages)/sizeof(kLiteCoreMessages[0]) ==
                        error::NumLiteCoreErrorsPlus1, "Incomplete error message table");
        const char *str = nullptr;
        if (code < sizeof(kLiteCoreMessages)/sizeof(char*))
            str = kLiteCoreMessages[code];
        if (!str)
            str = "(unknown LiteCoreError)";
        return str;
    }

    string error::_what(error::Domain domain, int code) noexcept {
        switch (domain) {
            case LiteCore:
                return litecore_errstr((LiteCoreError)code);
            case POSIX:
                return strerror(code);
            case SQLite:
                return sqlite3_errstr(code);
            default:
                return "unknown error domain";
        }
    }


#pragma mark - ERROR CLASS:


    bool error::sWarnOnError = true;

    
    error::error(error::Domain d, int c)
    :error(d, c, _what(d, c))
    { }


    error::error(error::Domain d, int c, const std::string &what)
    :runtime_error(what),
    domain(d),
    code(c)
    { }


    error error::standardized() const {
        Domain d = domain;
        int c = code;
        switch (domain) {
            case POSIX:
                mapError(d, c, kPOSIXMapping);
                break;
            case SQLite:
                mapError(d, c, kSQLiteMapping);
                break;
            case Fleece:
                mapError(d, c, kFleeceMapping);
                break;
            default:
                return *this;
        }
        return error(d, c);
    }


    static error unexpectedException(const std::exception &x) {
        // Get the actual exception class name using RTTI.
        // Unmangle it by skipping class name prefix like "St12" (may be compiler dependent)
        const char *name =  typeid(x).name();
        while (isalpha(*name)) ++name;
        while (isdigit(*name)) ++name;
        Warn("Caught unexpected C++ %s(\"%s\")", name, x.what());
        return error(error::LiteCore, error::UnexpectedError);
    }


    error error::convertRuntimeError(const std::runtime_error &re) {
        auto e = dynamic_cast<const error*>(&re);
        if (e)
            return *e;
        auto se = dynamic_cast<const SQLite::Exception*>(&re);
        if (se)
            return error(SQLite, se->getErrorCode());
        auto fe = dynamic_cast<const fleece::FleeceException*>(&re);
        if (fe)
            return error(Fleece, fe->code);
        return unexpectedException(re);
    }

    error error::convertException(const std::exception &x) {
        auto re = dynamic_cast<const std::runtime_error*>(&x);
        if (re)
            return convertRuntimeError(*re);
        return unexpectedException(x);
    }


    bool error::isUnremarkable() const {
        if (code == 0)
            return true;
        switch (domain) {
            case LiteCore:
                return code == NotFound || code == Deleted;
            case POSIX:
                return code == ENOENT;
            default:
                return false;
        }
    }

    
    void error::_throw(Domain domain, int code ) {
        DebugAssert(code != 0);
        error err{domain, code};
        if (sWarnOnError && !err.isUnremarkable()) {
            WarnError("LiteCore throwing %s error %d: %s",
                      kDomainNames[domain], code, err.what());
            if (WillLog(LogLevel::Error))
                logBacktrace(1);
        }
        throw err;
    }

    
    void error::_throw(error::LiteCoreError err) {
        _throw(LiteCore, err);
    }

    void error::_throwErrno() {
        _throw(POSIX, errno);
    }



    void error::assertionFailed(const char *fn, const char *file, unsigned line, const char *expr,
                                const char *message)
    {
        if (!message)
            message = expr;
        if (!WillLog(LogLevel::Error))
            fprintf(stderr, "Assertion failed: %s (%s:%u, in %s)", message, file, line, fn);
        WarnError("Assertion failed: %s (%s:%u, in %s)", message, file, line, fn);
        if (WillLog(LogLevel::Error))
            logBacktrace(1);
        throw error(error::AssertionFailed);
    }

#ifndef __ANDROID__
    /*static*/ void error::logBacktrace(unsigned skip) {
#ifdef __clang__
        ++skip;     // skip the logBacktrace frame itself
        void* addrs[50];
        int n = backtrace(addrs, 50) - skip;
        if (n <= 0)
            return;
        char** lines = backtrace_symbols(&addrs[skip], n);
        char* unmangled = nullptr;
        size_t unmangledLen = 0;

        for (int i = 0; i < n; ++i) {
            char library[101], functionBuf[201];
            size_t pc;
            int offset;
            if (sscanf(lines[i], "%*d %100s %zi %200s + %i",
                       library, &pc, functionBuf, &offset) == 4 ||
                sscanf(lines[i], "%100[^(](%200[^+]+%i) ""[""%zi""]",
                       library, functionBuf, &offset, &pc) == 4) {
                const char *function = functionBuf;
                int status;
                unmangled = abi::__cxa_demangle(function, unmangled, &unmangledLen, &status);
                if (unmangled && status == 0)
                    function = unmangled;
                fprintf(stderr, "%2d  %-25s %s + %d\n", i, library, function, offset);
            } else {
                fprintf(stderr, "%s\n", lines[i]);
            }
        }
        free(unmangled);
        free(lines);
#endif
    }
#endif
}
