//
//  Request.hh
//  LiteCore
//
//  Created by Jens Alfke on 12/25/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#pragma once
#include "slice.hh"
#include "Writer.hh"
#include <string>
#include <unordered_map>


namespace litecore {

    /** An HTTP status code. */
    typedef unsigned Status;


    /** An HTTP request method. */
    enum class Method {
        GET,
        HEAD,
        POST,
        PUT,
        DELETE,
    };


    /** A key/value map, used for HTTP headers and query parameters. */
    class Map {
    public:
        Map() { }

        const std::string& operator[] (const char *name) const {
            auto i = _map.find(name);
            if (i == _map.end())
                return kEmptyString;
            return i->second;
        }

        template <typename K, typename V>
        void set(const K key, const V value) {
            _map[key] = value;
        }

        typedef std::unordered_map<std::string, std::string>::const_iterator const_iterator;

        const_iterator begin() const    {return _map.begin();}
        const_iterator end() const      {return _map.end();}

    private:
        std::unordered_map<std::string, std::string> _map;
        static const std::string kEmptyString;
    };


    /** An HTTP request. */
    struct Request {
        const Method method;
        const std::vector<std::string> path;
        Map queries;
        Map headers;
        const void* const impl;

        Request(Method m, const std::vector<std::string> &p, const void *i)
        :method(m)
        ,path(p)
        ,impl(i)
        { }

        Request(const Request &r)
        :method(r.method)
        ,path(r.path)
        ,queries(r.queries)
        ,headers(r.headers)
        ,impl(r.impl)
        { }
    };


    /** An HTTP response. */
    class Response : public Map {
    public:
        Status status {200};

        void setContentType(const char *type)       {set("Content-Type", type);}

        Response& operator<< (uint8_t byte)         {_out << byte; return *this;}
        Response& operator<< (fleece::slice s)      {_out << s; return *this;}

        void write(fleece::slice s)                 {_out << s;}
        void write(const char *s, size_t length)    {write({s, length});}
        void write(const char *s)                   {write(fleece::slice(s));}

        fleece::alloc_slice extractOutput();

        //////// JSON:

        Response& writeNull();
        Response& writeBool(bool);
        Response& writeNumber(long);
        Response& writeString(const char*);
        Response& writeString(const std::string&);
        Response& beginArray()                      {return nest('[');}
        Response& endArray()                        {return unnest(']');}
        Response& beginObject()                     {return nest('{');}
        Response& endObject()                       {return unnest('}');}
        Response& writeKey(const char*);

    private:
        Response& nest(char);
        Response& unnest(char);
        void comma();

        fleece::Writer _out;
        bool _jsonComma {false};
        int _jsonDepth {0};
    };

}
