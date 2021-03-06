//
//  c4Key.h
//  Couchbase Lite Core
//
//  Created by Jens Alfke on 11/6/15.
//  Copyright (c) 2015-2016 Couchbase. All rights reserved.
//

#pragma once
#include "c4Base.h"

#ifdef __cplusplus
extern "C" {
#endif

    /** \defgroup Keys View and Query Keys
        @{ */


    //////// KEYS:


    /** \name Creating Keys
        @{ */


    /** An opaque value used as a key or value in a view index. The data types that can be stored
        in a C4Key are the same as JSON, but the actual data format is quite different. */
    typedef struct c4Key C4Key;

    /** Creates a new empty C4Key. */
    C4Key* c4key_new(void) C4API;

    /** Creates a C4Key by copying the data, which must be in the C4Key binary format. */
    C4Key* c4key_withBytes(C4Slice) C4API;

    /** Resets a C4Key to the empty state so it can be reused. */
    void c4key_reset(C4Key*) C4API;

    /** Frees a C4Key. */
    void c4key_free(C4Key*) C4API;

    void c4key_addNull(C4Key*) C4API;             ///< Adds a JSON null value to a C4Key. */
    void c4key_addBool(C4Key*, bool) C4API;       ///< Adds a boolean value to a C4Key. */
    void c4key_addNumber(C4Key*, double) C4API;   ///< Adds a number to a C4Key. */
    void c4key_addString(C4Key*, C4String) C4API;  ///< Adds a UTF-8 string to a C4Key. */

    /** Adds an array to a C4Key.
        Subsequent values added will go into the array, until c4key_endArray is called. */
    void c4key_beginArray(C4Key*) C4API;

    /** Closes an array opened by c4key_beginArray. (Every array must be closed.) */
    void c4key_endArray(C4Key*) C4API;

    /** Adds a map/dictionary/object to a C4Key.
        Subsequent keys and values added will go into the map, until c4key_endMap is called. */
    void c4key_beginMap(C4Key*) C4API;

    /** Closes a map opened by c4key_beginMap. (Every map must be closed.) */
    void c4key_endMap(C4Key*) C4API;

    /** Adds a map key, before the next value. When adding to a map, every value must be
        preceded by a key. */
    void c4key_addMapKey(C4Key*, C4String) C4API;


    /** @} */


    //////// KEY READERS:


    /** \name Parsing Keys
        @{ */


    /** A struct pointing to the raw data of an encoded key. The functions that operate
        on this allow it to be parsed by reading items one at a time (similar to SAX parsing.) */
    typedef struct {
        const void *bytes;
        size_t length;
    } C4KeyReader;

    /** The types of tokens in a key. */
    typedef C4_ENUM(uint8_t, C4KeyToken) {
        kC4Null,
        kC4Bool,
        kC4Number,
        kC4String,
        kC4Array,
        kC4Map,
        kC4EndSequence,
        kC4Special,
        kC4Error = 255
    };


    /** Returns a C4KeyReader that can parse the contents of a C4Key.
        Warning: Adding to the C4Key will invalidate the reader. */
    C4KeyReader c4key_read(const C4Key *key) C4API;

    /** Same as c4key_read, but allocates the C4KeyReader on the heap.
        (Used by the Java binding.) */
    C4KeyReader* c4key_newReader(const C4Key *key) C4API;

    /** Frees a C4KeyReader allocated by c4key_newReader. */
    void c4key_freeReader(C4KeyReader*) C4API;

    /** Returns the type of the next item in the key, or kC4Error at the end of the key or if the
        data is corrupt.
        To move on to the next item, you must call skipToken or one of the read___ functions. */
    C4KeyToken c4key_peek(const C4KeyReader*) C4API;

    /** Skips the current token in the key. If it was kC4Array or kC4Map, the reader will
        now be positioned at the first item of the collection. */
    void c4key_skipToken(C4KeyReader*) C4API;

    bool c4key_readBool(C4KeyReader*) C4API;              ///< Reads a boolean value.
    double c4key_readNumber(C4KeyReader*) C4API;          ///< Reads a numeric value.
    C4StringResult c4key_readString(C4KeyReader*) C4API;   ///< Reads a string (remember to free it!)

    /** Converts a C4KeyReader to JSON. Remember to free the result. */
    C4StringResult c4key_toJSON(const C4KeyReader*) C4API;

    /** @} */


    //////// KEY/VALUE LISTS:


    /** \name Key/Value Lists
        @{ */


    /** An opaque list of key/value pairs, used when indexing a view. */
    typedef struct c4KeyValueList C4KeyValueList;

    /** Creates a new empty list. */
    C4KeyValueList* c4kv_new(void) C4API;

    /** Adds a key/value pair to a list. The key and value are copied. */
    void c4kv_add(C4KeyValueList *kv, C4Key *key, C4String value) C4API;

    /** Removes all keys and values from a list. */
    void c4kv_reset(C4KeyValueList *kv) C4API;

    /** Frees all storage used by a list (including its copied keys and values.) */
    void c4kv_free(C4KeyValueList *kv) C4API;

    /* @} */
    /* @} */
#ifdef __cplusplus
    }
#endif
