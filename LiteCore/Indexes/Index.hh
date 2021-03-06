//
//  Index.hh
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
#include "DataFile.hh"
#include "RecordEnumerator.hh"
#include "Collatable.hh"
#include "Encoder.hh"
#include <atomic>

namespace litecore {
    
    class Index;


    /** Struct representing range of Index keys. */
    struct KeyRange {
        Collatable start;
        Collatable end;
        bool inclusiveEnd;

        KeyRange(Collatable s, Collatable e, bool inclusive =true)
                                                :start(s), end(e), inclusiveEnd(inclusive) { }
        KeyRange(Collatable single)             :start(single), end(single), inclusiveEnd(true) { }
        KeyRange(const KeyRange &r)             :start(r.start), end(r.end),
                                                 inclusiveEnd(r.inclusiveEnd) { }
        bool isKeyPastEnd(slice key) const;

        bool operator== (const KeyRange &r)     {return start==r.start && end==r.end;}
    };

    
    /** A key/value index, stored in a KeyStore. */
    class Index {
    public:
        explicit Index(KeyStore&);
        ~Index();

        alloc_slice getEntry(slice recordID, sequence recordSequence,
                             Collatable key,
                             unsigned emitIndex) const;

        DataFile& dataFile() const              {return _store.dataFile();}
        bool isBusy() const                     {return _userCount > 0;}

        /** Used as a placeholder for an index value that's stored out of line, i.e. that
            represents the entire record being indexed. */
        static const slice kSpecialValue;

    protected:
        KeyStore &_store;

    private:
        friend class IndexWriter;
        friend class IndexEnumerator;

        void addUser()                          {++_userCount;}
        void removeUser()                       {--_userCount;}

        std::atomic_uint _userCount {0};
    };


    /** Updates an index, within a Transaction. */
    class IndexWriter {
    public:
        IndexWriter(Index& index, Transaction& t, bool wasEmpty =false);
        ~IndexWriter();

        /** Updates the index entry for a record with the given keys and values.
            Adjusts the value of rowCount by the number of rows added or removed.
            Returns true if the index may have changed as a result. */
        bool update(slice recordID,
                    sequence recordSequence,
                    const std::vector<Collatable> &keys,
                    const std::vector<alloc_slice> &values,
                    uint64_t &rowCount);

    private:
        void getKeysForDoc(slice recordID, std::vector<Collatable> &outKeys, uint32_t &outHash);
        void setKeysForDoc(slice recordID, const std::vector<Collatable> &keys, uint32_t hash);

        friend class Index;
        friend class MapReduceIndex;

        Index &         _index;             // The Index being written to
        Transaction &   _transaction;       // The Transaction enabling the write
        const bool      _wasEmpty;          // Was the index empty beforehand?
        fleece::Encoder _encoder;           // Reuseable encoder, an optimization for update()
        CollatableBuilder _realKey;         // Reuseable builder, an optimization for update()
    };


    class ReduceFunction {
    public:
        virtual ~ReduceFunction() { };
        virtual void operator() (CollatableReader key, slice value) =0;
        virtual slice reducedValue() =0;    // Result must remain valid until next call
    };


    /** Index query enumerator. */
    class IndexEnumerator {
    public:
        struct Options : public RecordEnumerator::Options {
            ReduceFunction *reduce  {nullptr};
            unsigned groupLevel     {0};

            Options() {}
        };

        IndexEnumerator(Index&,
                        Collatable startKey, slice startKeyDocID,
                        Collatable endKey, slice endKeyDocID,
                        const Options& =Options());

        IndexEnumerator(Index&,
                        std::vector<KeyRange> keyRanges,
                        const Options& =Options());

        virtual ~IndexEnumerator()              {_index.removeUser();}

        const Index& index() const              {return _index;}

        CollatableReader key() const            {return CollatableReader(_key);}
        slice value() const                     {return _value;}
        slice recordID() const                  {return _recordID;}
        sequence_t sequence() const             {return _sequence;}

        int currentKeyRangeIndex()              {return _currentKeyIndex;}

        bool next();

        void close() noexcept                   {_dbEnum.close();}

    protected:
        bool nextKeyRange();
        virtual bool approve(slice key)         {return true;}
        bool read();
        void setValue(slice value)              {_value = value;}
        RecordEnumerator enumeratorForIndex(int keyRangeIndex);

        void computeGroupedKey();
        bool accumulateRow();
        bool createReducedRow();

    private:
        friend class Index;

        Index&                  _index;                 // The index
        Options                 _options;               // Enumeration options
        alloc_slice             _startKey;              // Key to start at
        alloc_slice             _endKey;                // Key to end at
        bool                    _inclusiveStart;        // Include the startKey?
        bool                    _inclusiveEnd;          // Include the endKey?
        std::vector<KeyRange>   _keyRanges;             // Ranges of keys to traverse (optional)
        int                     _currentKeyIndex {-1};  // Current key range's index or -1

        RecordEnumerator           _dbEnum;                // The underlying KeyStore enumerator
        slice                   _key;                   // Current key
        slice                   _value;                 // Current value
        alloc_slice             _recordID;                 // Current recordID
        sequence_t              _sequence;              // Current sequence

        bool                    _reducing {false};      // Am I accumulating reduced rows?
        alloc_slice             _groupedKey;            // Current key prefix being grouped
        alloc_slice             _reducedKey;            // Owns _key for a reduced row
    };

}
