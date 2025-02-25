package com.couchbase.litecore;

import com.couchbase.litecore.fleece.FLEncoder;
import com.couchbase.litecore.fleece.FLSharedKeys;
import com.couchbase.litecore.fleece.FLSliceResult;
import com.couchbase.litecore.fleece.FLValue;

public class C4Database implements C4Constants {
    //-------------------------------------------------------------------------
    // Member Variables
    //-------------------------------------------------------------------------
    private long handle = 0L; // hold pointer to C4Database

    //-------------------------------------------------------------------------
    // Constructor
    //-------------------------------------------------------------------------
    public C4Database(String path,
                      int flags, String storageEngine, int versioning,
                      int algorithm, byte[] encryptionKey)
            throws LiteCoreException {
        handle = open(path, flags, storageEngine, versioning, algorithm, encryptionKey);
    }

    private C4Database(long handle) {
        if (handle == 0)
            throw new IllegalArgumentException("handle is 0");
        this.handle = handle;
    }

    //-------------------------------------------------------------------------
    // public methods
    //-------------------------------------------------------------------------

    // - Lifecycle

    public C4Database openAgain() throws LiteCoreException {
        return new C4Database(openAgain(handle));
    }

    public C4Database retain() {
        return new C4Database(retain(handle));
    }

    public boolean free() {
        boolean result = true;
        if (handle != 0L && (result = free(handle)))
            handle = 0L;
        return result;
    }

    public void close() throws LiteCoreException {
        close(handle);
    }

    public void delete() throws LiteCoreException {
        delete(handle);
    }

    public void rekey(int keyType, byte[] newKey) throws LiteCoreException {
        rekey(handle, keyType, newKey);
    }

    public static void shutdown() throws LiteCoreException {
        c4shutdown();
    }

    // - Accessors

    public String getPath() {
        return getPath(handle);
    }

    public long getDocumentCount() {
        return getDocumentCount(handle);
    }

    public long getLastSequence() {
        return getLastSequence(handle);
    }

    public long nextDocExpiration() {
        return nextDocExpiration(handle);
    }

    public int getMaxRevTreeDepth() {
        return getMaxRevTreeDepth(handle);
    }

    public void setMaxRevTreeDepth(int maxRevTreeDepth) {
        setMaxRevTreeDepth(handle, maxRevTreeDepth);
    }

    public byte[] getPublicUUID() throws LiteCoreException {
        return getPublicUUID(handle);
    }

    public byte[] getPrivateUUID() throws LiteCoreException {
        return getPrivateUUID(handle);
    }

    // - Compaction

    public void compact() throws LiteCoreException {
        compact(handle);
    }

    // - Transactions
    public void beginTransaction() throws LiteCoreException {
        beginTransaction(handle);
    }

    public void endTransaction(boolean commit) throws LiteCoreException {
        endTransaction(handle, commit);
    }

    public boolean isInTransaction() {
        return isInTransaction(handle);
    }

    // - RawDocs Raw Documents

    public C4RawDocument rawGet(String storeName, String docID) throws LiteCoreException {
        return new C4RawDocument(rawGet(handle, storeName, docID));
    }

    public void rawPut(String storeName, String key, String meta, String body)
            throws LiteCoreException {
        rawPut(handle, storeName, key, meta, body);
    }

    // c4Document+Fleece.h

    // - Fleece-related

    public FLEncoder createFleeceEncoder() {
        return new FLEncoder(createFleeceEncoder(handle));
    }

    // NOTE: Should param be String instead of byte[]?
    public FLSliceResult encodeJSON(byte[] jsonData) throws LiteCoreException {
        return new FLSliceResult(encodeJSON(handle, jsonData));
    }

    public final FLSharedKeys getFLSharedKeys() {
        return new FLSharedKeys(getFLSharedKeys(handle));
    }

    ////////////////////////////////
    // C4DocEnumerator
    ////////////////////////////////

    public C4DocEnumerator enumerateChanges(long since,
                                            long skip,
                                            int flags)
            throws LiteCoreException {
        return new C4DocEnumerator(handle, since, skip, flags);
    }

    public C4DocEnumerator enumerateAllDocs(String startDocID,
                                            String endDocID,
                                            long skip,
                                            int flags)
            throws LiteCoreException {
        return new C4DocEnumerator(handle, startDocID, endDocID, skip, flags);
    }

    public C4DocEnumerator enumerateSomeDocs(String[] docIDs,
                                             long skip,
                                             int flags)
            throws LiteCoreException {
        return new C4DocEnumerator(handle, docIDs, skip, flags);
    }

    ////////////////////////////////
    // C4Document
    ////////////////////////////////

    public C4Document get(String docID, boolean mustExist) throws LiteCoreException {
        return new C4Document(handle, docID, mustExist);
    }

    public C4Document getBySequence(long sequence) throws LiteCoreException {
        return new C4Document(handle, sequence);
    }

    // - Purging and Expiration

    public void purgeDoc(String docID) throws LiteCoreException {
        C4Document.purgeDoc(handle, docID);
    }

    public void setExpiration(String docID, long timestamp)
            throws LiteCoreException {
        C4Document.setExpiration(handle, docID, timestamp);
    }

    public long getExpiration(String docID) {
        return C4Document.getExpiration(handle, docID);
    }

    // - Creating and Updating Documents

    public C4Document put(
            byte[] body,
            String docID,
            int revFlags,
            boolean existingRevision,
            boolean allowConflict,
            String[] history,
            boolean save,
            int maxRevTreeDepth)
            throws LiteCoreException {
        return new C4Document(C4Document.put(handle,
                body, docID, revFlags, existingRevision, allowConflict,
                history, save, maxRevTreeDepth));
    }

    public C4Document put(
            FLSliceResult body, // C4Slice*
            String docID,
            int revFlags,
            boolean existingRevision,
            boolean allowConflict,
            String[] history,
            boolean save,
            int maxRevTreeDepth)
            throws LiteCoreException {
        return new C4Document(C4Document.put2(handle,
                body.getHandle(), docID, revFlags, existingRevision, allowConflict,
                history, save, maxRevTreeDepth));
    }

    public C4Document create(String docID, byte[] body, int revisionFlags)
            throws LiteCoreException {
        return new C4Document(C4Document.create(handle, docID, body, revisionFlags));
    }

    public C4Document create(String docID, FLSliceResult body, int flags) throws LiteCoreException {
        return new C4Document(C4Document.create2(handle, docID, body != null ? body.getHandle() : 0, flags));
    }

    ////////////////////////////////////////////////////////////////
    // C4DatabaseObserver/C4DocumentObserver
    ////////////////////////////////////////////////////////////////

    public C4DatabaseObserver createDatabaseObserver(C4DatabaseObserverListener listener,
                                                     Object context) {
        return new C4DatabaseObserver(handle, listener, context);
    }

    public C4DocumentObserver createDocumentObserver(String docID,
                                                     C4DocumentObserverListener listener,
                                                     Object context) {
        return new C4DocumentObserver(handle, docID, listener, context);
    }

    ////////////////////////////////
    // C4BlobStore
    ////////////////////////////////
    public C4BlobStore getBlobStore() throws LiteCoreException {
        return new C4BlobStore(C4BlobStore.getBlobStore(handle), true);
    }

    ////////////////////////////////
    // C4Query
    ////////////////////////////////

    public C4Query createQuery(String expression) throws LiteCoreException {
        return new C4Query(handle, expression);
    }

    public boolean createIndex(String name, String expressionsJSON, int indexType, String language,
                               boolean ignoreDiacritics) throws LiteCoreException {
        return C4Query.createIndex(handle, name, expressionsJSON, indexType, language, ignoreDiacritics);
    }

    public void deleteIndex(String name) throws LiteCoreException {
        C4Query.deleteIndex(handle, name);
    }

    public FLValue getIndexes() throws LiteCoreException {
        return new FLValue(C4Query.getIndexes(handle));
    }

    ////////////////////////////////
    // C4Replicator
    ////////////////////////////////
    public C4Replicator createReplicator(String schema, String host, int port, String path,
                                         String remoteDatabaseName,
                                         C4Database otherLocalDB,
                                         int push, int pull,
                                         byte[] options,
                                         C4ReplicatorListener listener, Object context)
            throws LiteCoreException {
        return new C4Replicator(handle, schema, host, port, path, remoteDatabaseName,
                otherLocalDB != null ? otherLocalDB.getHandle() : 0,
                push, pull,
                options, listener, context);
    }

    //-------------------------------------------------------------------------
    // protected methods
    //-------------------------------------------------------------------------

    @Override
    protected void finalize() throws Throwable {
        free();
        super.finalize();
    }

    //-------------------------------------------------------------------------
    // package access
    //-------------------------------------------------------------------------

    long getHandle() {
        return handle;
    }

    //-------------------------------------------------------------------------
    // native methods
    //-------------------------------------------------------------------------

    // - Lifecycle

    static native long open(String path, int flags,
                            String storageEngine, int versioning,
                            int algorithm, byte[] encryptionKey)
            throws LiteCoreException;

    static native long openAgain(long db) throws LiteCoreException;

    public static native void copy(String sourcePath, String destinationPath,
                                   int flags,
                                   String storageEngine,
                                   int versioning,
                                   int algorithm,
                                   byte[] encryptionKey)
            throws LiteCoreException;

    static native long retain(long db);

    static native boolean free(long db);

    static native void close(long db) throws LiteCoreException;

    static native void delete(long db) throws LiteCoreException;

    public static native void deleteAtPath(String path, int flags,
                                           String storageEngine, int versioning)
            throws LiteCoreException;

    static native void rekey(long db, int keyType, byte[] newKey) throws LiteCoreException;

    static native void c4shutdown() throws LiteCoreException;

    // - Accessors

    static native String getPath(long db);

    static native long getDocumentCount(long db);

    static native long getLastSequence(long db);

    static native long nextDocExpiration(long db);

    static native int getMaxRevTreeDepth(long db);

    static native void setMaxRevTreeDepth(long db, int maxRevTreeDepth);

    static native byte[] getPublicUUID(long db) throws LiteCoreException;

    static native byte[] getPrivateUUID(long db) throws LiteCoreException;

    // - Compaction
    static native void compact(long db) throws LiteCoreException;

    // - Transactions
    static native void beginTransaction(long db) throws LiteCoreException;

    static native void endTransaction(long db, boolean commit) throws LiteCoreException;

    static native boolean isInTransaction(long db);

    //////// RAW DOCUMENTS (i.e. info or _local)

    // -RawDocs Raw Documents

    static native void rawFree(long rawDoc) throws LiteCoreException;

    static native long rawGet(long db, String storeName, String docID)
            throws LiteCoreException;

    static native void rawPut(long db, String storeName,
                              String key, String meta, String body)
            throws LiteCoreException;

    ////////////////////////////////
    // c4Document+Fleece.h
    ////////////////////////////////

    // - Fleece-related

    static native long createFleeceEncoder(long db);

    static native long encodeJSON(long db, byte[] jsonData) throws LiteCoreException;

    static native long getFLSharedKeys(long db);
}
