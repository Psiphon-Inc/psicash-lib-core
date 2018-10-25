package ca.psiphon.psicashlib;

import android.net.Uri;
import android.support.annotation.Nullable;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
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

        public boolean equals(int code) {
            return this.code == code;
        }
    }

    private static final String kErrorKey = "error";
    private static final String kErrorMessageKey = "message";
    private static final String kErrorInternalKey = "internal";
    private static final String kResultKey = "result";

    public static class Error {
        public String message;
        public boolean internal;

        public Error() {
        }

        public Error(String message, boolean internal) {
            this.message = message;
            this.internal = internal;
        }

        public Error(String message) {
            this(message, false);
        }

        @Nullable
        public static Error fromJSON(JSONObject json) throws JSONException {
            Error error = new Error();

            JSONObject errorObj = JSON.nullableObject(json, kErrorKey);
            if (errorObj == null) {
                return null;
            }

            error.message = JSON.nullableString(errorObj, kErrorMessageKey);
            if (error.message == null) {
                return null;
            }

            Boolean internal = JSON.nullableBoolean(errorObj, kErrorInternalKey);
            error.internal = internal == null ? false : internal.booleanValue();

            return error;
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

    /**
     * @returns null if no error; Error otherwise
     */
    @Nullable
    public Error setRequestMetadataItem(String key, String value) {
        String jsonStr = this.SetRequestMetadataItem(key, value);

        JNI.ErrorOrResult<JSON.Nothing> res = new JNI.ErrorOrResult<>(JSON.Nothing.class, jsonStr);
        return res.error();
    }



    /**
     * @returns true if the currently held tokens correspond to an Account, or false if a Tracker.
     */
    public boolean isAccount() {
        String jsonStr = this.NativeIsAccount();

        JNI.ErrorOrResult<JSON.Boolean> res = new JNI.ErrorOrResult<>(JSON.Boolean.class, jsonStr);
        if (res.error() != null) {
            // Not expected to happen normally
            return false;
        }

        if (res.result().isNull()) {
            // Not expected to happen normally
            return false;
        }

        return res.result().value();
    }

    public static class ValidTokenTypes extends ArrayList<String> implements JSON.Unmarshalable {
        public ValidTokenTypes() { super(); }

        public void fromJSON(JSONObject json, String key) {
            this.clear();
            List<String> vtt = JSON.nullableList(String.class, json, key);;
            if (vtt != null) {
                this.addAll(vtt);
            }
        }
    }

    public ValidTokenTypes validTokenTypes() {
        String jsonStr = this.NativeValidTokenTypes();

        JNI.ErrorOrResult<ValidTokenTypes> res = new JNI.ErrorOrResult<>(ValidTokenTypes.class, jsonStr);
        if (res.error() != null) {
            // Not expected to happen normally
            return null;
        }

        return res.result();
    }

    public static class Purchase implements JSON.Unmarshalable {
        public String id;
        public String transactionClass;
        public String distinguisher;
        public Date expiry;
        public String authorization;

        public Purchase() {}

        @Override
        public void fromJSON(JSONObject json, String key) throws JSONException {
            json = JSON.nullableObject(json, key);
            if (json == null) {
                return;
            }

            this.id = json.getString("id");
            this.transactionClass = json.getString("class");
            this.distinguisher = json.getString("distinguisher");
            this.expiry = JSON.nullableDate(json, "serverTimeExpiry");
            this.authorization = JSON.nullableString(json, "authorization");
        }
    }

    public static class NewExpiringPurchaseResult implements JSON.Unmarshalable {
        public Error error;
        public Status status;
        public Purchase purchase;

        public NewExpiringPurchaseResult() {}

        public void fromJSON(JSONObject json, String key) throws JSONException {
            json = JSON.nullableObject(json, key);
            if (json == null) {
                return;
            }

            this.error = null; // handled elsewhere
            this.status = Status.fromCode(json.getInt("status"));
            this.purchase = new Purchase();
            this.purchase.fromJSON(json, "purchase");
        }
    }

    public NewExpiringPurchaseResult newExpiringPurchase(
            String transactionClass, String distinguisher, long expectedPrice) {
        NewExpiringPurchaseResult errorResult = new NewExpiringPurchaseResult();
        errorResult.status = Status.INVALID;

        String paramsJSON;
        try {
            JSONObject json = new JSONObject();
            json.put("class", transactionClass);
            json.put("distinguisher", distinguisher);
            json.put("expectedPrice", expectedPrice);
            paramsJSON = json.toString();
        } catch (JSONException e) {
            // Should never happen
            errorResult.error = new Error("Failed to create params JSON: " + e.getMessage(), true);
            return errorResult;
        }

        String jsonStr = this.NativeNewExpiringPurchase(paramsJSON);

        JNI.ErrorOrResult<NewExpiringPurchaseResult> res = new JNI.ErrorOrResult<>(NewExpiringPurchaseResult.class, jsonStr);
        if (res.error() != null) {
            errorResult.error = res.error;
            return errorResult;
        }

        return res.result();
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

            Number port = JSON.nullableInt(json, "port");
            if (port != null) {
                hostname += ":" + port.intValue();
            }

            uriBuilder.encodedAuthority(hostname);

            reqParams.method = json.getString("method");

            uriBuilder.encodedPath(json.getString("path"));

            JSONObject jsonHeaders = JSON.nullableObject(json, "headers");
            if (jsonHeaders != null) {
                Iterator<?> headerKeys = jsonHeaders.keys();
                while (headerKeys.hasNext()) {
                    String key = (String) headerKeys.next();
                    String value = jsonHeaders.getString(key);
                    reqParams.headers.put(key, value);
                }
            }

            // Query params are an array of arrays of 2 strings.
            JSONArray jsonQueryParams = JSON.nullableArray(json, "query");
            if (jsonQueryParams != null) {
                for (int i = 0; i < jsonQueryParams.length(); i++) {
                    JSONArray param = jsonQueryParams.getJSONArray(i);
                    String key = param.getString(0);
                    String value = param.getString(1);
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

    //
    // JNI helpers class
    //

    private static class JNI {
        @Nullable
        private static Boolean resultBoolean(String jsonStr) {
            try {
                JSONObject json = new JSONObject(jsonStr);
                return JSON.nullableBoolean(json, kResultKey);
            } catch (JSONException e) {
                e.printStackTrace(); // TODO
                return null;
            }
        }

        @Nullable
        private static <T> T result(Class<T> clazz, String jsonStr) throws JSONException {
            JSONObject json = new JSONObject(jsonStr);
            return JSON.nullable(clazz, json, kResultKey);
        }

        @Nullable
        private static <T> List<T> resultList(Class<T> clazz, String jsonStr) throws JSONException {
            JSONObject json = new JSONObject(jsonStr);
            return JSON.nullableList(clazz, json, kResultKey);
        }


        private static class ErrorOrResult<T extends JSON.Unmarshalable> {
            public ErrorOrResult(Class<T> clazz, String jsonStr) {
                if (jsonStr == null) {
                    this.error = new Error("Result JSON string is null", true);
                    return;
                }

                JSONObject json;
                try {
                    json = new JSONObject(jsonStr);
                } catch (JSONException e) {
                    this.error = new Error("ErrorOrResult: Overall JSON parse failed: " + e.getMessage(), true);
                    return;
                }

                try {
                    this.error = Error.fromJSON(json);
                    if (this.error != null) {
                        return;
                    }
                } catch (JSONException e) {
                    this.error = new Error("ErrorOrResult: Error JSON parse failed: " + e.getMessage(), true);
                    return;
                }

                try {
                    this.result = clazz.newInstance();
                } catch (InstantiationException e) {
                    this.error = new Error("ErrorOrResult: Missing public default constructor for " + clazz.getSimpleName() + "; " + e.getMessage(), true);
                    return;
                } catch (IllegalAccessException e) {
                    this.error = new Error("ErrorOrResult: Missing public default constructor for " + clazz.getSimpleName() + "; " + e.getMessage(), true);
                    return;
                }

                try {
                    this.result.fromJSON(json, kResultKey);
                } catch (JSONException e) {
                    this.error = new Error("ErrorOrResult: Result JSON parse failed: " + e.getMessage(), true);
                    return;
                }
            }

            public Error error() {
                return this.error;
            }

            public T result() {
                if (this.error != null) {
                    return null;
                }
                return this.result;
            }

            private Error error;
            private T result;
        }
    }

    //
    // JSON helpers class
    //

    private static class JSON {
        @Nullable
        private static String nullableString(JSONObject json, String key) throws JSONException {
            if (!json.has(key) || json.isNull(key)) {
                return null;
            }
            return json.getString(key);
        }

        @Nullable
        private static Number nullableInt(JSONObject json, String key) throws JSONException {
            if (!json.has(key) || json.isNull(key)) {
                return null;
            }
            return json.getInt(key);
        }

        @Nullable
        private static java.lang.Boolean nullableBoolean(JSONObject json, String key) throws JSONException {
            if (!json.has(key) || json.isNull(key)) {
                return null;
            }
            return json.getBoolean(key);
        }

        @Nullable
        private static JSONObject nullableObject(JSONObject json, String key) throws JSONException {
            if (!json.has(key) || json.isNull(key)) {
                return null;
            }
            return json.getJSONObject(key);
        }

        @Nullable
        private static JSONArray nullableArray(JSONObject json, String key) throws JSONException {
            if (!json.has(key) || json.isNull(key)) {
                return null;
            }
            return json.getJSONArray(key);
        }

        @Nullable
        private static Date nullableDate(JSONObject json, String key) throws JSONException {
            String dateString = nullableString(json, key);
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

        @Nullable
        private static <T> T cast(Class<T> clazz, Object o) {
            if (o == null) {
                return null;
            }

            if (!clazz.isAssignableFrom(o.getClass())) {
                // TODO: Log?
                return null;
            }

            return clazz.cast(o);
        }

        @Nullable
        private static <T> T nullable(Class<T> clazz, JSONObject json, String key) {
            Object o = json.opt(key);
            return cast(clazz, o);
        }

        @Nullable
        private static <T> T nullable(Class<T> clazz, JSONArray json, int key) {
            Object o = json.opt(key);
            return cast(clazz, o);
        }

        @Nullable
        private static <T> List<T> nullableList(Class<T> clazz, JSONObject json, String key) {
            JSONArray jsonArray = json.optJSONArray(key);
            if (jsonArray == null) {
                return null;
            }

            ArrayList<T> result = new ArrayList<>();

            for (int i = 0; i < jsonArray.length(); i++) {
                result.add(nullable(clazz, jsonArray, i));
            }

            return result;
        }

        private interface Unmarshalable {
            // NOTE: Also requires public default constructor

            void fromJSON(JSONObject json, String key) throws JSONException;
        }

        private static class Nothing implements Unmarshalable {
            public Nothing() {}

            @Override
            public void fromJSON(JSONObject json, String key) {
            }
        }

        private static class Boolean implements Unmarshalable {
            public Boolean() {}

            @Override
            public void fromJSON(JSONObject json, String key) {
                this.value = JSON.nullable(java.lang.Boolean.class, json, key);
            }

            public boolean isNull() {
                return this.value == null;
            }

            public java.lang.Boolean value() { return value; }
            private java.lang.Boolean value;
        }

    }

    /*
     * Expose native (C++) functions.
     * NOTE: Full descriptions of what these methods do are in psicash.h and will not be repeated here.
     */

    /*
    All String return values have this basic JSON structure:
        {
            "error": {      null or absent iff no error
                "message":  string; nonempty (if error object present)
                "internal": boolean; true iff error is 'internal' and probably unrecoverable
            }

            "result":       type varies; actual result of the call
        }
    Any field may be absent or null if not applicable, but either "error" or "result" must be present.
    */

    private static native boolean NativeStaticInit();

    private native String NativeObjectInit(String fileStoreRoot, boolean test);

    /**
     * @return { "error": {...} }
     */
    private native String SetRequestMetadataItem(String key, String value);

    /**
     * @return {
     *  "error": {...},
     *  "result": boolean
     * }
     */
    private native String NativeIsAccount();

    /**
     * @return {
     *  "error": {...},
     *  "result": ["earner", "indicator", ...]
     * }
     */
    private native String NativeValidTokenTypes();

    private native String NativeNewExpiringPurchase(String params);
}
