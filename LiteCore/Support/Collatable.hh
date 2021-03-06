//
//  Collatable.hh
//  Couchbase Lite Core
//
//  Created by Jens Alfke on 5/14/14.
//  Copyright (c) 2014-2016 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#pragma once
#include <stdint.h>
#include <string>
#include <iostream>
#include "Base.hh"

namespace litecore {

    class CollatableTypes {
    public:
        typedef enum {
            kEndSequence = 0,   // Returned to indicate the end of an array/dict
            kNull,
            kFalse,
            kTrue,
            kNegative,
            kPositive,
            kString,
            kArray,
            kMap,
            kSpecial,           // Placeholder for doc (Only used in values, not keys)
            kError = 255        // Something went wrong. (Never stored, only returned from peekTag)
        } Tag;
    };

    class CollatableBuilder;

    class Collatable : public alloc_slice, public CollatableTypes {
    public:
        Collatable()                :alloc_slice()  { }
        explicit inline Collatable(CollatableBuilder&& b);

        static Collatable withData(slice s)         {return Collatable(s, true);}
        static Collatable withData(alloc_slice s)   {return Collatable(s, true);}

        slice data() const          {return (slice)*this;}

        bool empty() const          {return size == 0;}

        std::string toJSON() const;

    private:
        Collatable(alloc_slice s, bool)     :alloc_slice(s) { }
        Collatable(slice s, bool)           :alloc_slice(s) { }
    };

    
    /** A binary encoding of JSON-compatible data, that collates with CouchDB-compatible semantics
        using a dumb binary compare (like memcmp).
        Data format spec: https://github.com/couchbaselabs/litecore/wiki/Collatable-Data-Format
        Collatable owns its data, in the form of a C++ string object. */
    class CollatableBuilder : public CollatableTypes {
    public:
        CollatableBuilder();
        explicit CollatableBuilder(Collatable c); // Imports data previously saved in collatable format
        CollatableBuilder(slice, bool);        // Imports data previously saved in collatable format
        ~CollatableBuilder();

        template<typename T> explicit CollatableBuilder(const T &t)
        :_buf(slice::newBytes(kDefaultSize), kDefaultSize),
         _available(_buf)
        {
            *this << t;
        }

        CollatableBuilder& addNull()                       {addTag(kNull); return *this;}
        CollatableBuilder& addBool (bool); // overriding <<(bool) is dangerous due to implicit conversion

        CollatableBuilder& operator<< (double);

        CollatableBuilder& operator<< (const Collatable&);
        CollatableBuilder& operator<< (const CollatableBuilder&);
        CollatableBuilder& operator<< (const std::string &s)  {return operator<<(slice(s));}
        CollatableBuilder& operator<< (const char* cstr)   {return operator<<(slice(cstr));}
        CollatableBuilder& operator<< (slice s)            {addString(kString, s); return *this;}

        CollatableBuilder& beginArray()                    {addTag(kArray); return *this;}
        CollatableBuilder& endArray()                      {addTag(kEndSequence); return *this;}

        CollatableBuilder& beginMap()                      {addTag(kMap); return *this;}
        CollatableBuilder& endMap()                        {addTag(kEndSequence); return *this;}

        CollatableBuilder& addSpecial()                    {addTag(kSpecial); return *this;}

        size_t size() const                         {return _buf.size - _available.size;}
        bool empty() const                          {return size() == 0;}

        std::string toJSON() const;

        slice data() const                          {return slice(_buf.buf, size());}
        operator slice() const                      {return data();}

        operator Collatable () const                {return Collatable::withData(data());}

        alloc_slice extractOutput() {
            auto result = data();
#if 0
            //TODO: Restore this optimization by using alloc_slice internally
            _buf = _available = nullslice;
            return alloc_slice::adopt(result);
#else
            return alloc_slice(result);
#endif
        }

        void reset();

        CollatableBuilder(CollatableBuilder&& c) {
            _buf = c._buf;
            _available = c._available;
            c._buf.buf = nullptr;
        }
        CollatableBuilder& operator= (CollatableBuilder &&c) {
            _buf = c._buf;
            _available = c._available;
            c._buf.buf = nullptr;
            return *this;
        }

    private:
        static const size_t kMinSize = 32;
        static const size_t kDefaultSize = 128;

        CollatableBuilder(const CollatableBuilder& c) =delete;
        CollatableBuilder& operator= (const CollatableBuilder &c) =delete;

        uint8_t* reserve(size_t amt);
        void add(slice);
        void addTag(Tag t)                          {uint8_t c = t; add(slice(&c,1));}
        void addString(Tag, slice);

        slice _buf;
        slice _available;
    };


    /** A decoder of Collatable-format data. Does _not_ own its data (reads from a slice.) */
    class CollatableReader : public CollatableTypes {
    public:
        CollatableReader(slice s);

        slice data() const                  {return _data;}
        bool atEnd() const                  {return _data.size == 0;}
        
        Tag peekTag() const noexcept;
        void skipTag() noexcept             {if (_data.size > 0) _skipTag();}

        int64_t readInt();
        double readDouble();
        alloc_slice readString()            {return readString(kString);}

        std::pair<alloc_slice, alloc_slice> readFullTextKey();  // pair is <text, langCode>

        /** Reads (skips) an entire object of any type, returning its data in Collatable form. */
        slice read();

        void beginArray();
        void endArray();
        void beginMap();
        void endMap();

        void writeJSONTo(std::ostream &out);
        std::string toJSON();

        static uint8_t* getInverseCharPriorityMap();

    private:
        void expectTag(Tag tag);
        void _skipTag()                     {_data.moveStart(1);} // like skipTag but unsafe
        alloc_slice readString(Tag);

        slice _data;
    };


    Collatable::Collatable(CollatableBuilder&& b) :alloc_slice(b.extractOutput()) { }


}
