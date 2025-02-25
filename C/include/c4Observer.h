//
//  c4Observer.h
//  LiteCore
//
//  Created by Jens Alfke on 11/4/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#pragma once

#include "c4Database.h"

#ifdef __cplusplus
extern "C" {
#endif


    /** \defgroup Observer  Database and Document Observers
        @{ */

    typedef struct {
        C4String docID;
        C4String revID;
        C4SequenceNumber sequence;
        uint32_t bodySize;
    } C4DatabaseChange;

    /** A database-observer reference. */
    typedef struct c4DatabaseObserver C4DatabaseObserver;

    /** Callback invoked by a database observer.
        @param observer  The observer that initiated the callback.
        @param context  user-defined parameter given when registering the callback. */
    typedef void (*C4DatabaseObserverCallback)(C4DatabaseObserver* observer C4NONNULL,
                                               void *context);

    /** Creates a new database observer, with a callback that will be invoked after the database
        changes. The callback will be called _once_, after the first change. After that it won't
        be called again until all of the changes have been read by calling `c4dbobs_getChanges`.
        @param database  The database to observer.
        @param callback  The function to call after the database changes.
        @param context  An arbitrary value that will be passed to the callback.
        @return  The new observer reference. */
    C4DatabaseObserver* c4dbobs_create(C4Database* database C4NONNULL,
                                       C4DatabaseObserverCallback callback C4NONNULL,
                                       void *context) C4API;

    /** Identifies which documents have changed since the last time this function was called, or
        since the observer was created. This function effectively "reads" changes from a stream,
        in whatever quantity the caller desires. Once all of the changes have been read, the
        observer is reset and ready to notify again.
        @param observer  The observer.
        @param outChanges  A caller-provided buffer of structs into which changes will be
                            written.
        @param maxChanges  The maximum number of changes to return, i.e. the size of the caller's
                            outChanges buffer.
        @param outExternal  Will be set to true if the changes were made by a different C4Database.
        @return  The number of changes written to `outDocIDs`. If this is less than `maxChanges`,
                            the end has been reached and the observer is reset. */
    uint32_t c4dbobs_getChanges(C4DatabaseObserver *observer C4NONNULL,
                                C4DatabaseChange outChanges[] C4NONNULL,
                                uint32_t maxChanges,
                                bool *outExternal C4NONNULL) C4API;

    /** Stops an observer and frees the resources it's using.
        It is safe to pass NULL to this call. */
    void c4dbobs_free(C4DatabaseObserver*) C4API;


    /** A document-observer reference. */
    typedef struct c4DocumentObserver C4DocumentObserver;

    /** Callback invoked by a document observer.
        @param observer  The observer that initiated the callback.
        @param docID  The ID of the document that changed.
        @param sequence  The sequence number of the change.
        @param context  user-defined parameter given when registering the callback. */
    typedef void (*C4DocumentObserverCallback)(C4DocumentObserver* observer C4NONNULL,
                                               C4String docID,
                                               C4SequenceNumber sequence,
                                               void *context);

    /** Creates a new document observer, with a callback that will be invoked when the document
        changes. The callback will be called every time the document changes.
        @param database  The database to observer.
        @param docID  The ID of the document to observe.
        @param callback  The function to call after the database changes.
        @param context  An arbitrary value that will be passed to the callback.
        @return  The new observer reference. */
    C4DocumentObserver* c4docobs_create(C4Database* database C4NONNULL,
                                        C4String docID,
                                        C4DocumentObserverCallback callback,
                                        void *context) C4API;

    /** Stops an observer and frees the resources it's using.
        It is safe to pass NULL to this call. */
    void c4docobs_free(C4DocumentObserver*) C4API;

    /** @} */
#ifdef __cplusplus
}
#endif
