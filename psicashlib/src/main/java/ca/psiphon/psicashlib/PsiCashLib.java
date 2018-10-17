package ca.psiphon.psicashlib;

import android.net.Uri;

import org.json.JSONException;
import org.json.JSONObject;

import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

public class PsiCashLib {
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

            public String toJSON() {
                JSONObject json = new JSONObject();
                try {
                    json.put("status", this.status);
                    json.put("body", this.body);
                    json.put("date", this.date);
                    json.put("error", this.error);
                    return json.toString();
                } catch (JSONException e) {
                    e.printStackTrace();
                }

                // Should never happen, and no sane recovery.
                return null;
            }
        }

        Result httpRequest(ReqParams reqParams);
    }

    private HTTPRequester httpRequester;

    public enum Status {
        INVALID(-1),
        SUCCESS(0),
        EXISTING_TRANSACTION(1),
        INSUFFICIENT_BALANCE(2),
        TRANSACTION_AMOUNT_MISMATCH(3),
        TRANSACTION_TYPE_NOT_FOUND(4),
        INVALID_TOKENS(5),
        SERVER_ERROR(6);

        private final int code;

        Status(int code) {
            this.code = code;
        }

        public static Status fromCode(int code) {
            for (Status s : Status.values()) {
                if (s.code == code) return s;
            }
            throw new IllegalArgumentException("Status not found");
        }
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

        String err = this.NativeObjectInit(fileStoreRoot, true);

        if (err != null) {
            return "PsiCashLib NativeObjectInit failed: " + err;
        }

        return null;
    }

    //
    // JSON helpers
    //

    private static String jsonNullableString(JSONObject json, String key) throws JSONException {
        if (!json.has(key) || json.isNull(key)) {
            return null;
        }
        return json.getString(key);
    }

    private static Number jsonNullableInt(JSONObject json, String key) throws JSONException {
        if (!json.has(key) || json.isNull(key)) {
            return null;
        }
        return json.getInt(key);
    }

    private static JSONObject jsonNullableObject(JSONObject json, String key) throws JSONException {
        if (!json.has(key) || json.isNull(key)) {
            return null;
        }
        return json.getJSONObject(key);
    }

    private static Date jsonNullableDate(JSONObject json, String key) throws JSONException {
        String dateString = jsonNullableString(json, key);
        if (dateString == null) {
            return null;
        }

        Date date;

        // We need to try different formats depending on the presence of milliseconds.
        SimpleDateFormat isoFormatWithMS = new SimpleDateFormat("yyyy-MM-dd'T'hh:mm:ss.SSS'Z'");
        try {
            date = isoFormatWithMS.parse(dateString);
        } catch (ParseException e1) {
            SimpleDateFormat isoFormatWithoutMS = new SimpleDateFormat("yyyy-MM-dd'T'hh:mm:ss'Z'");
            try {
                date = isoFormatWithoutMS.parse(dateString);
            } catch (ParseException e2) {
                // Should not happen. No way to recover.
                throw new JSONException("Failed to parse date with key " + key + "; error: " + e2.toString());
            }
        }

        return date;
    }

    public static class Purchase {
        public String id;
        public String transactionClass;
        public String distinguisher;
        public Date expiry;
        public String authorization;

        public static Purchase fromJSON(JSONObject json) throws JSONException {
            if (json == null) {
                return null;
            }

            Purchase p = new Purchase();
            p.id = json.getString("id");
            p.transactionClass = json.getString("class");
            p.distinguisher = json.getString("distinguisher");
            p.expiry = jsonNullableDate(json, "serverTimeExpiry");
            p.authorization = jsonNullableString(json, "authorization");

            return p;
        }
    }

    public static class NewExpiringPurchaseResult {
        public Status status;
        public String error;
        public Purchase purchase;

        public static NewExpiringPurchaseResult fromJSON(JSONObject json) throws JSONException {
            if (json == null) {
                return null;
            }

            NewExpiringPurchaseResult n = new NewExpiringPurchaseResult();
            n.status = Status.fromCode(json.getInt("status"));
            n.error = jsonNullableString(json, "error");
            n.purchase = Purchase.fromJSON(jsonNullableObject(json, "purchase"));
            return n;
        }
    }

    public NewExpiringPurchaseResult newExpiringPurchase(
            String transactionClass, String distinguisher, long expectedPrice) {
        NewExpiringPurchaseResult result = new NewExpiringPurchaseResult();
        result.status = Status.INVALID;

        String paramsJSON;
        try {
            JSONObject json = new JSONObject();
            json.put("class", transactionClass);
            json.put("distinguisher", distinguisher);
            json.put("expectedPrice", expectedPrice);
            paramsJSON = json.toString();
        } catch (JSONException e) {
            // Should never happen
            e.printStackTrace();
            // TODO
            result.error = "Failed to create params JSON";
            return result;
        }

        String resultJSON = this.NewExpiringPurchase(paramsJSON);
        if (resultJSON == null) {
            // TODO: what?
            result.error = "catastrophic";
            return result;
        }

        try {
            JSONObject j = new JSONObject(resultJSON);

            result = NewExpiringPurchaseResult.fromJSON(j);
        } catch (JSONException e) {
            // TODO
            e.printStackTrace();
            result.error = "catastrophic";
            return result;
        }

        return result;
    }

    public String makeHTTPRequest(String jsonReqParams) {
        HTTPRequester.Result result = new HTTPRequester.Result();

        HTTPRequester.ReqParams reqParams = new HTTPRequester.ReqParams();
        Uri.Builder uriBuilder = new Uri.Builder();
        reqParams.headers = new HashMap<>();

        try {
            JSONObject json = new JSONObject(jsonReqParams);

            uriBuilder.scheme(json.getString("scheme"));

            String hostname = json.getString("hostname");

            Number port = jsonNullableInt(json, "port");
            if (port != null) {
                hostname += ":" + port.intValue();
            }

            uriBuilder.encodedAuthority(hostname);

            reqParams.method = json.getString("method");

            uriBuilder.encodedPath(json.getString("path"));

            JSONObject jsonHeaders = jsonNullableObject(json, "headers");
            if (jsonHeaders != null) {
                Iterator<?> headerKeys = jsonHeaders.keys();
                while (headerKeys.hasNext()) {
                    String key = (String) headerKeys.next();
                    String value = jsonHeaders.getString(key);
                    reqParams.headers.put(key, value);
                }
            }

            JSONObject jsonQueryParams = jsonNullableObject(json, "query");
            if (jsonQueryParams != null) {
                Iterator<?> queryParamKeys = jsonQueryParams.keys();
                while (queryParamKeys.hasNext()) {
                    String key = (String) queryParamKeys.next();
                    String value = jsonQueryParams.getString(key);
                    uriBuilder.appendQueryParameter(key, value);
                }
            }
        } catch (JSONException e) {
            result.error = "Parsing request object failed: " + e.toString();
            return result.toJSON();
        }

        reqParams.uri = uriBuilder.build();

        result = httpRequester.httpRequest(reqParams);

        // Check for consistency in the result.
        // Ensure sanity if there's an error: status must be -1 iff there's an error message
        if ((result.status == -1) != (result.error != null && !result.error.isEmpty())) {
            result.error = "Request result is not in sane error state: " + result.toString();
            return result.toJSON();
        }

        return result.toJSON();
    }

    /*
     * Expose native (C++) functions.
     */
    private static native boolean NativeStaticInit();

    private native String NativeObjectInit(String fileStoreRoot, boolean test);

    private native String NewExpiringPurchase(String params);
}
