//
//  Query.hh
//  LiteCore
//
//  Created by Jens Alfke on 10/5/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#pragma once
#include "KeyStore.hh"

namespace litecore {
    class Query;


    class QueryEnumerator {
    public:
        struct Options {
            Options()           :skip(0), limit(UINT64_MAX) { }
            uint64_t skip;
            uint64_t limit;
            slice paramBindings;
        };

        QueryEnumerator(Query*, const Options* =nullptr);

        bool next();
        void close()                    {_impl.reset();}

        slice recordID() const          {return _recordID;}
        sequence_t sequence() const     {return _sequence;}
        slice meta()                    {return _impl->meta();}

        /** Info about a match of a full-text query term */
        struct FullTextTerm {
            uint32_t termIndex;                 ///< Index of the search term in the tokenized query
            uint32_t start, length;             ///< *Byte* range of word in query string
        };

        bool hasFullText()              {return _impl->hasFullText();}

        const std::vector<FullTextTerm>& fullTextTerms() {
            _impl->getFullTextTerms(_fullTextTerms); return _fullTextTerms;
        }

        alloc_slice getMatchedText() const {
            return _impl->getMatchedText();
        }

        alloc_slice getCustomColumns()  {return _impl->getCustomColumns();}

        class Impl {
        public:
            virtual ~Impl() = default;
            virtual bool next(slice &docID, sequence_t &sequence) =0;
            virtual slice meta() =0;
            virtual bool hasFullText()                                  {return false;}
            virtual void getFullTextTerms(std::vector<FullTextTerm>& t) {}
            virtual alloc_slice getMatchedText()                        {return alloc_slice();}
            virtual alloc_slice getCustomColumns()                      {return alloc_slice();}
        };

    private:
        std::unique_ptr<Impl> _impl;
        slice _recordID;
        sequence_t _sequence;
        std::vector<FullTextTerm> _fullTextTerms;
    };


    /** Abstract base class of compiled database queries.
        These are created by the factory method KeyStore::compileQuery(). */
    class Query {
    public:
        virtual ~Query() = default;

        KeyStore& keyStore() const      {return _keyStore;}

        virtual alloc_slice getMatchedText(slice recordID, sequence_t) {return alloc_slice();}

        virtual std::string explain()   {return "";}

    protected:
        Query(KeyStore &keyStore) noexcept
        :_keyStore(keyStore)
        { }

        virtual QueryEnumerator::Impl* createEnumerator(const QueryEnumerator::Options*) =0;

    private:
        KeyStore &_keyStore;

        friend class QueryEnumerator;
    };

}
