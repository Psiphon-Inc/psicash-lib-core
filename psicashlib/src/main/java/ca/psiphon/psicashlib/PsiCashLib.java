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
            public String date;
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

    public String newExpiringPurchaseWrapper() {
        // TEMP
        String paramsJSON;
        try {
            JSONObject json = new JSONObject();
            json.put("class", "speed-boost");
            json.put("distinguisher", "1hr");
            json.put("expectedPrice", 100000000000L);

            paramsJSON = json.toString();
        } catch (JSONException e) {
            // Should never happen
            e.printStackTrace();
            return "Failed to create params JSON";
        }

        String res = this.NewExpiringPurchase(paramsJSON);
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

            reqParams.method = jsonReader.getString("method");

            uriBuilder.encodedPath(jsonReader.getString("path"));

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

        // Check for consistency in the result.
        // Ensure sanity if there's an error: status must be -1 iff there's an error message
        if ((res.status == -1) != (res.error != null && !res.error.isEmpty())) {
            return "bad"; // TODO: what?
        }

        String jsonResult;
        try {
            JSONObject json = new JSONObject();
            json.put("status", res.status);
            json.put("body", res.body != null ? res.body : "");
            json.put("date", res.date != null ? res.date : "");
            json.put("error", res.error != null ? res.error : "");
            jsonResult = json.toString();
        }
        catch (JSONException e) {
            e.printStackTrace();
            return "bad"; // TODO: what?
        }

        return jsonResult;
    }

    /*
     * Expose native (C++) functions.
     */
    private static native boolean NativeStaticInit();
    private native String NativeObjectInit(String fileStoreRoot);
    private native String NewExpiringPurchase(String params);
}
