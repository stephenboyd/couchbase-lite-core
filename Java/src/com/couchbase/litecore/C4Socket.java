package com.couchbase.litecore;

import android.util.Log;

import com.couchbase.litecore.fleece.FLValue;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

import static com.couchbase.litecore.C4Replicator.kC4Replicator2Scheme;
import static com.couchbase.litecore.C4Replicator.kC4Replicator2TLSScheme;


public abstract class C4Socket {

    //-------------------------------------------------------------------------
    // Constants
    //-------------------------------------------------------------------------
    private static final String TAG = C4Socket.class.getSimpleName();

    public static final String WEBSOCKET_SCHEME = "ws";
    public static final String WEBSOCKET_SECURE_CONNECTION_SCHEME = "wss";

    // Replicator option dictionary keys:
    public static final String kC4ReplicatorOptionExtraHeaders = "headers";  // Extra HTTP headers; string[]
    public static final String kC4ReplicatorOptionCookies = "cookies";  // HTTP Cookie header value; string
    public static final String kC4ReplicatorOptionAuthentication = "auth";     // Auth settings; Dict
    public static final String kC4ReplicatorOptionPinnedServerCert = "pinnedCert";  // Cert or public key [data]
    public static final String kC4ReplicatorOptionChannels = "channels"; // SG channel names; string[]
    public static final String kC4ReplicatorOptionFilter = "filter";   // Filter name; string
    public static final String kC4ReplicatorOptionFilterParams = "filterParams";  // Filter params; Dict[string]
    public static final String kC4ReplicatorOptionSkipDeleted = "skipDeleted"; // Don't push/pull tombstones; bool
    public static final String kC4ReplicatorOptionNoConflicts = "noConflicts"; // Puller rejects conflicts; bool

    // Auth dictionary keys:
    public static final String kC4ReplicatorAuthType = "type";// Auth property; string
    public static final String kC4ReplicatorAuthUserName = "username";// Auth property; string
    public static final String kC4ReplicatorAuthPassword = "password";// Auth property; string

    //-------------------------------------------------------------------------
    // Static Variables
    //-------------------------------------------------------------------------

    protected static String IMPLEMENTATION_CLASS_NAME;

    // Long: handle of C4Socket native address
    // C4Socket: Java class holds handle
    private static Map<Long, C4Socket> reverseLookupTable
            = Collections.synchronizedMap(new HashMap<Long, C4Socket>());

    //-------------------------------------------------------------------------
    // Member Variables
    //-------------------------------------------------------------------------
    protected long handle = 0L; // hold pointer to C4Socket
    private static C4Socket c4sock = null;

    //-------------------------------------------------------------------------
    // constructor
    //-------------------------------------------------------------------------
    protected C4Socket(long handle) {
        this.handle = handle;
    }

    //-------------------------------------------------------------------------
    // Abstract methods
    //-------------------------------------------------------------------------
    protected abstract void start();

    protected abstract void send(byte[] allocatedData);

    // NOTE: Not used
    protected abstract void completedReceive(long byteCount);

    // NOTE: Not used
    protected abstract void close();

    protected abstract void requestClose(int status, String message);

    //-------------------------------------------------------------------------
    // callback methods from JNI
    //-------------------------------------------------------------------------

    private static void open(long socket, String scheme, String hostname, int port, String path, byte[] optionsFleece) {
        Log.e(TAG, "C4Socket.callback.open() socket -> 0x" + Long.toHexString(socket) + ", scheme -> " + scheme + ", hostname -> " + hostname + ", port -> " + port + ", path -> " + path);
        Log.e(TAG, "optionsFleece: " + (optionsFleece != null ? "not null" : "null"));

        Map<String, Object> options = null;
        if (optionsFleece != null) {
            options = FLValue.fromData(optionsFleece).asDict();
            Log.e(TAG, "options = " + options);
        }

        // NOTE: OkHttp can not understand blip/blips
        if (scheme.equalsIgnoreCase(kC4Replicator2Scheme))
            scheme = WEBSOCKET_SCHEME;
        else if (scheme.equalsIgnoreCase(kC4Replicator2TLSScheme))
            scheme = WEBSOCKET_SECURE_CONNECTION_SCHEME;

        URI uri;
        try {
            uri = new URI(scheme, null, hostname, port, path, null, null);
        } catch (URISyntaxException e) {
            Log.e(TAG, "Error with instantiating URI", e);
            return;
        }

        try {
            //c4sock = new CBLWebSocket(socket, uri, options);
            c4sock = newInstance(socket, uri, options);
        } catch (Exception e) {
            Log.e(TAG, "Failed to instantiate C4Socket: " + e);
            e.printStackTrace();
            return;
        }

        reverseLookupTable.put(socket, c4sock);

        c4sock.start();
    }

    private static void write(long handle, byte[] allocatedData) {
        if (handle == 0 || allocatedData == null) {
            Log.e(TAG, "C4Socket.callback.write() parameter error");
            return;
        }

        Log.e(TAG, "C4Socket.callback.write() handle -> 0x" + Long.toHexString(handle) + ", allocatedData.length -> " + allocatedData.length);

        C4Socket socket = reverseLookupTable.get(handle);
        if (socket != null)
            socket.send(allocatedData);
    }

    private static void completedReceive(long handle, long byteCount) {
        Log.e(TAG, "C4Socket.callback.completedReceive() socket -> 0x" + Long.toHexString(handle) + ", byteCount -> " + byteCount);

        // NOTE: No further action is not required?
    }

    private static void close(long handle) {
        Log.e(TAG, "C4Socket.callback.close() socket -> 0x" + Long.toHexString(handle));

        // NOTE: close(long) method should not be called.
    }

    private static void requestClose(long handle, int status, String message) {
        Log.e(TAG, "C4Socket.callback.requestClose() socket -> 0x" + Long.toHexString(handle) + ", status -> " + status + ", message -> " + message);

        C4Socket socket = reverseLookupTable.get(handle);
        if (socket != null)
            socket.requestClose(status, message);
    }

    //-------------------------------------------------------------------------
    // native methods
    //-------------------------------------------------------------------------
    protected static native void registerFactory();

    protected static native void gotHTTPResponse(long socket, int httpStatus, byte[] responseHeadersFleece);

    protected static native void opened(long socket);

    protected static native void closed(long socket, int errorDomain, int errorCode);

    protected static native void closeRequested(long socket, int status, String message);

    protected static native void completedWrite(long socket, long byteCount);

    protected static native void received(long socket, byte[] data);

    //-------------------------------------------------------------------------
    // Protected methods
    //-------------------------------------------------------------------------
    protected void gotHTTPResponse(int httpStatus, byte[] responseHeadersFleece) {
        gotHTTPResponse(handle, httpStatus, responseHeadersFleece);
    }

    protected void completedWrite(long byteCount) {
        completedWrite(handle, byteCount);
    }

    private static C4Socket newInstance(long handle, URI uri, Map<String, Object> options)
            throws Exception {
        return (C4Socket) Class.forName(C4Socket.IMPLEMENTATION_CLASS_NAME)
                .getConstructor(Long.TYPE, URI.class, Map.class)
                .newInstance(handle, uri, options);
    }
}
