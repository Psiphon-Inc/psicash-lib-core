package ca.psiphon.psicashlib;

import android.content.Context;
import android.net.Uri;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

public class PsiCashLib {
    private HTTPRequester httpRequester;

    /**
     * The library user must implement this interface. It provides HTTP request functionality to the
     * library.
     * It is up to the implementer to decide which thread the request should be executed on, and
     * whether the request should be proxied, etc.
     */
    public interface HTTPRequester {
        class ReqParams {
            public String method;
            public Uri uri;
            public Map<String, String> headers;
        }

        class Result {
            public int status = -1; // Negative indicates error trying to make the request
            public String body;
            public String error;
        }

        Result httpRequest(ReqParams reqParams);
    }

    static {
        // Load the C++ library.
        System.loadLibrary("psicash");

        // Call the C++ init function each time the library loads.
        if (!NativeStaticInit()) {
            // This shouldn't happen, unless the apk is misconfigured.
            throw new AssertionError("psicash library init failed");
        }
    }

    public PsiCashLib() {
    }

    // TODO: Figure out return type
    public String init(String fileStoreRoot, HTTPRequester httpRequester) {
        this.httpRequester = httpRequester;

        String err = this.NativeObjectInit(fileStoreRoot);

        if (err != null) {
            return "PsiCashLib NativeObjectInit failed: " + err;
        }

        return null;
    }

    public String newTrackerWrapper() {
        String res = this.NewTracker("my params");
        Log.i("temptag", res);
        return res;
    }

    public String makeHTTPRequest(String jsonReqParams) {
        HTTPRequester.ReqParams reqParams = new HTTPRequester.ReqParams();
        Uri.Builder uriBuilder = new Uri.Builder();
        reqParams.headers = new HashMap<>();

        try {
            JSONObject jsonReader = new JSONObject(jsonReqParams);

            uriBuilder.scheme(jsonReader.getString("scheme"));

            String hostname = jsonReader.getString("hostname");
            int port = jsonReader.getInt("port");
            uriBuilder.encodedAuthority(hostname + ":" + port);

            uriBuilder.encodedPath(jsonReader.getString("path"));

            reqParams.method = jsonReader.getString("method");

            JSONObject jsonHeaders = jsonReader.getJSONObject("headers");
            Iterator<?> headerKeys = jsonHeaders.keys();
            while (headerKeys.hasNext()) {
                String key = (String)headerKeys.next();
                String value = jsonHeaders.getString(key);
                reqParams.headers.put(key, value);
            }

            JSONObject jsonQueryParams = jsonReader.getJSONObject("query");
            Iterator<?> queryParamKeys = jsonQueryParams.keys();
            while (queryParamKeys.hasNext()) {
                String key = (String)queryParamKeys.next();
                String value = jsonQueryParams.getString(key);
                uriBuilder.appendQueryParameter(key, value);
            }
        }
        catch (JSONException e) {
            e.printStackTrace();
            return "bad"; // TODO: what?
        }

        reqParams.uri = uriBuilder.build();

        HTTPRequester.Result res = httpRequester.httpRequest(reqParams);
        return "" + res.status + ":" + res.body;
    }

    /*
     * Expose native (C++) functions.
     */
    private static native boolean NativeStaticInit();
    private native String NativeObjectInit(String fileStoreRoot);
    private native String NewTracker(String params);
}
