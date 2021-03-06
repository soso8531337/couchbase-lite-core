//
//  c4View.h
//  Couchbase Lite Core
//
//  C API for view and query access.
//
//  Created by Jens Alfke on 9/10/15.
//  Copyright (c) 2015-2016 Couchbase. All rights reserved.
//

#pragma once

#include "c4Base.h"
#include "c4Database.h"
#include "c4Document.h"
#include "c4Key.h"

#ifdef __cplusplus
extern "C" {
#endif

    /** \defgroup Views Views
        @{ */


    //////// VIEWS:


    /** Opaque handle to an opened view. */
    typedef struct c4View C4View;


    /** \name Lifecycle
        @{ */


    /** Opens a view, or creates it if the file doesn't already exist.
        @param database  The database the view is associated with.
        @param path  The filesystem path to the view index file. If this is null, a default path
                will be used that's next to the database file, with a name based on the view name.
        @param viewName  The name of the view.
        @param version  The version of the view's map function.
        @param config  The configuration of the view's own index database
        @param outError  On failure, error info will be stored here.
        @return  The new C4View, or NULL on failure. */
    C4View* c4view_open(C4Database *database,
                        C4String path,
                        C4String viewName,
                        C4String version,
                        const C4DatabaseConfig *config,
                        C4Error *outError) C4API;

    /** Frees a view handle, closing it if necessary. */
    void c4view_free(C4View* view) C4API;

    /** Closes the view. Does not free the handle, but calls to it will return errors. */
    bool c4view_close(C4View* view, C4Error*) C4API;

    /** Changes a view's encryption key (removing encryption if it's NULL.) */
    bool c4view_rekey(C4View*,
                      const C4EncryptionKey *newKey,
                      C4Error *outError) C4API;

    /** Erases the view index, but doesn't delete the database file. */
    bool c4view_eraseIndex(C4View*, C4Error *outError) C4API;

    /** Deletes the view's file(s) and closes/frees the C4View. */
    bool c4view_delete(C4View*, C4Error *outError) C4API;

    /** Deletes the file(s) for the view at the given path.
        All C4Views at that path should be closed first. */
    bool c4view_deleteAtPath(C4String dbPath, const C4DatabaseConfig *config, C4Error *outError) C4API;

    /** Deletes the file(s) for a view given its name and parent database.
        Its path is assumed to be the same default path as used by c4view_open when no explicit
        path is given. */
    bool c4view_deleteByName(C4Database *database, C4String viewName, C4Error *outError) C4API;

    /** @} */


    //////// ACCESSORS:


    /** \name Accessors
        @{ */


    /** Sets the persistent version string associated with the map function. If the new value is
        different from the one previously stored, the index is invalidated. */
    void c4view_setMapVersion(C4View *view, C4String version) C4API;

    /** Returns the total number of rows in the view index. */
    uint64_t c4view_getTotalRows(C4View*) C4API;

    /** Returns the last database sequence number that's been indexed.
        If this is less than the database's lastSequence, the view index is out of date. */
    C4SequenceNumber c4view_getLastSequenceIndexed(C4View*) C4API;

    /** Returns the last database sequence number that changed the view index. */
    C4SequenceNumber c4view_getLastSequenceChangedAt(C4View*) C4API;


    /** Sets a documentType filter on the view. If non-null, only documents whose
        documentType matches will be indexed by this view. */
    void c4view_setDocumentType(C4View*, C4String docType) C4API;

    /** Registers a callback to be invoked when the view's index db starts or finishes compacting.
        May be called on a background thread, so be careful of thread safety. */
    void c4view_setOnCompactCallback(C4View*, C4OnCompactCallback, void *context) C4API;

    /** @} */


    //////// INDEXING:


    /** \name Indexing
        @{ */


    /** Opaque reference to an indexing task. */
    typedef struct c4Indexer C4Indexer;

    /** Creates an indexing task on one or more views in a database.
        @param db  The database to index.
        @param views  An array of views whose indexes should be updated in parallel.
        @param viewCount  The number of views in the views[] array.
        @param outError  On failure, error info will be stored here.
        @return  A new C4Indexer, or NULL on failure. */
    C4Indexer* c4indexer_begin(C4Database *db,
                               C4View *views[],
                               size_t viewCount,
                               C4Error *outError) C4API;

    /** Instructs the indexer not to do any indexing if the given view is up-to-date.
        Typically this is used when the indexing occurs because this view is being queried. */
    void c4indexer_triggerOnView(C4Indexer *indexer, C4View *view) C4API;

    /** Creates an enumerator that will return all the documents that need to be (re)indexed.
        Returns NULL if no indexing is needed; you can distinguish this from an error by looking
        at the C4Error. */
    struct C4DocEnumerator* c4indexer_enumerateDocuments(C4Indexer *indexer,
                                                         C4Error *outError) C4API;

    /** Returns true if a view being indexed should index the given document.
        (This checks whether the document's current revision's sequence is greater than
        the view's last-indexed sequence.)
        If only one view is being indexed, you don't need to call this (assume it returns true.)
        If this function returns true, the caller should proceed to compute the key/value pairs,
        then call c4indexer_emit() to add them to this view's index.
        If this function returns false, the caller should skip to the next view.*/
    bool c4indexer_shouldIndexDocument(C4Indexer *indexer,
                                       unsigned viewNumber,
                                       C4Document *doc) C4API;

    /** Adds index rows for the keys/values derived from one document, for one view.
        This function needs to be called *exactly once* for each (document, view) pair during
        indexing. (Even if the view's map function didn't emit anything, the old keys/values need to
        be cleaned up.)
     
        Values are uninterpreted by LiteCore, but by convention are JSON. A special value "*"
        (a single asterisk) is used as a placeholder for the entire document.

        @param indexer  The indexer task.
        @param document  The document being indexed.
        @param viewNumber  The position of the view in the indexer's views[] array.
        @param emitCount  The number of emitted key/value pairs.
        @param emittedKeys  Array of keys being emitted.
        @param emittedValues  Array of values being emitted. (JSON by convention.)
        @param outError  On failure, error info will be stored here.
        @return  True on success, false on failure. */
    bool c4indexer_emit(C4Indexer *indexer,
                        C4Document *document,
                        unsigned viewNumber,
                        unsigned emitCount,
                        C4Key* const emittedKeys[],
                        C4String const emittedValues[],
                        C4Error *outError) C4API;

    /** Alternate form of c4indexer_emit that takes a C4KeyValueList instead of C arrays. */
    bool c4indexer_emitList(C4Indexer *indexer,
                            C4Document *doc,
                            unsigned viewNumber,
                            C4KeyValueList *kv,
                            C4Error *outError) C4API;

    /** Finishes an indexing task and frees the indexer reference.
        @param indexer  The indexer.
        @param commit  True to commit changes to the indexes, false to abort.
        @param outError  On failure, error info will be stored here.
        @return  True on success, false on failure. */
    bool c4indexer_end(C4Indexer *indexer,
                       bool commit,
                       C4Error *outError) C4API;

// A view value that represents a placeholder for the entire document
#ifdef _MSC_VER
#define kC4PlaceholderValue ({"*", 1})
#else
#define kC4PlaceholderValue ((C4Slice){"*", 1})
#endif

    /** @} */
    /** @} */

    //////// QUERYING:


    /** \defgroup QueryingViews Querying Views
        @{ */


    /** Defines a "reduce function" that aggregates multiple index rows into a single value. */
    typedef struct {
        /** Callback that receives a key/value pair from the index and accumulates it into the
            ongoing reduced result. */
        void (*accumulate)(void *context, C4Key *key, C4String value);
        /** Callback that returns reduced result as encoded data.
            The data must remain valid until the next call to the reduce() callback!
            This function should also clear the internal accumulation state, in preparation for
            subsequent calls to the `accumulate` callback. */
        C4String (*reduce)(void *context);
        /** Arbitrary pointer to caller-supplied storage space for the accumulation state.
            This value is passed to the callbacks for their use. */
        void *context;
    } C4ReduceFunction;


    /** Options for view queries. */
    typedef struct {
        uint64_t skip;          ///< Number of initial rows to skip
        uint64_t limit;         ///< Max number of rows to return (set to UINT_MAX for unlimited)
        bool descending;        ///< If true, iteration is by descending key
        bool inclusiveStart;    ///< If true, rows with key equal to startKey are included
        bool inclusiveEnd;      ///< If true, rows with key equal to endKey are included
        bool rankFullText;      ///< Should full-text results be ranked by relevance?

        C4Key *startKey;        ///< Key to start at (the minimum, or maximum if descending=true)
        C4Key *endKey;          ///< Key to end at (the maximum, or minimum if descending=true)
        C4String startKeyDocID;  ///< If multiple rows have startKey, start at one with this docID
        C4String endKeyDocID;    ///< If multiple rows have endKey, end at one with this docID
        
        const C4Key **keys;     ///< List of keys to iterate (overrides start/endKey)
        size_t keysCount;       ///< Number of keys pointed to by `keys`

        const C4ReduceFunction *reduce; ///< Reduce function, or NULL for no reducing
        uint32_t groupLevel;            ///< Key grouping level, or 0 for no grouping
    } C4QueryOptions;


    /** Default query options. Has skip=0, limit=UINT_MAX, inclusiveStart=true,
        inclusiveEnd=true, rankFullText=true; all others are 0/false/NULL. */
	CBL_CORE_API extern const C4QueryOptions kC4DefaultQueryOptions;

    
    /** Info about a match of a full-text query term */
    typedef struct {
        uint32_t termIndex;                 ///< Index of the search term in the tokenized query
        uint32_t start, length;             ///< *Byte* range of word in query string
    } C4FullTextTerm;


    /** A view query result enumerator. 
        Created by c4view_query or c4db_query. Must be freed with c4queryenum_free.
        The fields of this struct represent the current matched index row, and are replaced by the
        next call to c4queryenum_next or c4queryenum_free.
        The memory pointed to by slice fields is valid until the enumerator is advanced or freed. */
    typedef struct {
        // All query types:
        C4String docID;                       ///< ID of doc that emitted this row
        C4SequenceNumber docSequence;        ///< Sequence number of doc that emitted row

        // Map/reduce only:
        C4KeyReader key;                     ///< Encoded emitted key
        C4String value;                       ///< Encoded emitted value

        // Expression-based only:
        C4String revID;
        C4DocumentFlags docFlags;

        // Full-text only:
        uint32_t fullTextTermCount;          ///< The number of terms that were matched
        const C4FullTextTerm *fullTextTerms; ///< Array of terms that were matched
    } C4QueryEnumerator;


    /** Runs a regular map/reduce query and returns an enumerator for the results.
        The enumerator's fields are not valid until you call c4queryenum_next(), though.
        @param view  The view to query.
        @param options  Query options, or NULL for the default options.
        @param outError  On failure, error info will be stored here.
        @return  A new query enumerator. Fields are invalid until c4queryenum_next is called. */
    C4QueryEnumerator* c4view_query(C4View *view,
                                    const C4QueryOptions *options,
                                    C4Error *outError) C4API;

    /** In an expression-based query enumerator, returns the values of the custom columns of the
        query (the "WHAT" expressions), as a Fleece-encoded array. */
    C4SliceResult c4queryenum_customColumns(C4QueryEnumerator *e) C4API;

    /** In a full-text query enumerator, returns the string that was emitted during indexing that
        contained the search term(s). */
    C4StringResult c4queryenum_fullTextMatched(C4QueryEnumerator *e,
                                              C4Error *outError) C4API;

    /** Advances a query enumerator to the next row, populating its fields.
        Returns true on success, false at the end of enumeration or on error. */
    bool c4queryenum_next(C4QueryEnumerator *e,
                          C4Error *outError) C4API;

    /** Closes an enumerator without freeing it. This is optional, but can be used to free up
        resources if the enumeration has not reached its end, but will not be freed for a while. */
    void c4queryenum_close(C4QueryEnumerator *e) C4API;

    /** Frees a query enumerator. */
    void c4queryenum_free(C4QueryEnumerator *e) C4API;


    /** @} */
#ifdef __cplusplus
}
#endif
