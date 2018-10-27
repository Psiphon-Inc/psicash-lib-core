package ca.psiphon.psicashlib;

import android.net.Uri;
import android.support.annotation.NonNull;
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
import java.util.Locale;
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
    private static final String kStatusKey = "status";

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
        public static Error fromJSON(JSONObject json) {
            // We don't know for sure that the JSON contains an Error at this point.

            JSONObject errorObj = JSON.nullableObject(json, kErrorKey);
            if (errorObj == null) {
                // The object will be absent if this isn't actually an error
                return null;
            }

            Error error = new Error();
            error.message = JSON.nullableString(errorObj, kErrorMessageKey);
            if (error.message == null) {
                // Message is required for this to be considered an error
                return null;
            }

            Boolean internal = JSON.nullableBoolean(errorObj, kErrorInternalKey);
            error.internal = internal != null && internal;

            return error;
        }

        // Returns null if string is null
        @Nullable
        public static Error fromString(String message, boolean internal) {
            if (message == null) {
                return null;
            }
            return new Error(message, internal);
        }
    }

    public static class ValidTokenTypes extends ArrayList<String> {
        public ValidTokenTypes(List<String> src) {
            super(src);
        }
    }

    public static class PurchasePrice {
        public String transactionClass;
        public String distinguisher;
        public long price;
    }

    public static class PurchasePrices extends ArrayList<PurchasePrice> {
        public PurchasePrices() { }
        public PurchasePrices(List<PurchasePrice> src) {
            super(src);
        }
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
            p.id = JSON.nonnullString(json, "id");
            p.transactionClass = JSON.nonnullString(json, "class");
            p.distinguisher = JSON.nonnullString(json, "distinguisher");
            p.expiry = JSON.nullableDate(json, "serverTimeExpiry");
            p.authorization = JSON.nullableString(json, "authorization");

            return p;
        }
    }

    public static class Purchases extends ArrayList<Purchase> {
        public Purchases() { }
        public Purchases(List<Purchase> src) {
            super(src);
        }

        public static Purchases fromJSON(JSONArray jsonArray) throws JSONException {
            if (jsonArray == null) {
                return null;
            }

            Purchases purchases = new Purchases();

            for (int i = 0; i < jsonArray.length(); i++) {
                JSONObject j = JSON.nonnullObject(jsonArray, i);
                purchases.add(Purchase.fromJSON(j));
            }

            return purchases;
        }
    }

    public PsiCashLib() {
    }

    /*
     * Init
     */

    /**
     * @returns null if no error; Error otherwise
     */
    @Nullable
    public Error init(String fileStoreRoot, HTTPRequester httpRequester) {
        return init(fileStoreRoot, httpRequester, false);
    }

    @Nullable
    protected Error init(String fileStoreRoot, HTTPRequester httpRequester, boolean test) {
        this.httpRequester = httpRequester;

        String jsonStr = this.NativeObjectInit(fileStoreRoot, test);
        JNI.Result.ErrorOnly res = new JNI.Result.ErrorOnly(jsonStr);
        return res.error;
    }

    /*
     * SetRequestMetadataItem
     */

    /**
     * @returns null if no error; Error otherwise
     */
    @Nullable
    public Error setRequestMetadataItem(String key, String value) {
        String jsonStr = this.SetRequestMetadataItem(key, value);

        JNI.Result.ErrorOnly res = new JNI.Result.ErrorOnly(jsonStr);
        return res.error;
    }

    /*
     * IsAccount
     */

    public static class IsAccountResult {
        public Error error; // should never happen
        public boolean isAccount;

        public IsAccountResult(JNI.Result.IsAccount res) {
            this.error = res.error;
            if (this.error != null) {
                return;
            }
            this.isAccount = res.isAccount;
        }
    }

    @NonNull
    public IsAccountResult isAccount() {
        String jsonStr = this.NativeIsAccount();
        JNI.Result.IsAccount res = new JNI.Result.IsAccount(jsonStr);
        return new IsAccountResult(res);
    }

    /*
     * ValidTokenTypes
     */

    public static class ValidTokenTypesResult {
        @Nullable // and expected to always be null; indicates glue problem
        public Error error;

        @Nullable // but never expected to be null
        public ValidTokenTypes validTokenTypes;

        public ValidTokenTypesResult(JNI.Result.ValidTokenTypes res) {
            this.error = res.error;
            if (this.error != null) {
                return;
            }
            this.validTokenTypes = new ValidTokenTypes(res.validTokenTypes);
        }
    }

    @NonNull
    public ValidTokenTypesResult validTokenTypes() {
        String jsonStr = this.NativeValidTokenTypes();
        JNI.Result.ValidTokenTypes res = new JNI.Result.ValidTokenTypes(jsonStr);
        return new ValidTokenTypesResult(res);
    }

    /*
     * Balance
     */

    public static class BalanceResult {
        @Nullable // and expected to always be null; indicates glue problem
        public Error error;
        @Nullable // but never expected to be null
        public long balance;

        public BalanceResult(JNI.Result.Balance res) {
            this.error = res.error;
            if (this.error != null) {
                return;
            }
            this.balance = res.balance;
        }
    }

    @NonNull
    public BalanceResult balance() {
        String jsonStr = this.NativeBalance();
        JNI.Result.Balance res = new JNI.Result.Balance(jsonStr);
        return new BalanceResult(res);
    }

    /*
     * GetPurchasePrices
     */

    public static class GetPurchasePricesResult {
        @Nullable // and expected to always be null; indicates glue problem
        public Error error;
        @Nullable // but never expected to be null
        public PurchasePrices purchasePrices;

        public GetPurchasePricesResult(JNI.Result.GetPurchasePrices res) {
            this.error = res.error;
            if (this.error != null) {
                return;
            }
            this.purchasePrices = res.purchasePrices;
        }
    }

    @Nullable
    public GetPurchasePricesResult getPurchasePrices() {
        String jsonStr = this.NativeGetPurchasePrices();
        JNI.Result.GetPurchasePrices res = new JNI.Result.GetPurchasePrices(jsonStr);
        return new GetPurchasePricesResult(res);
    }

    /*
     * GetPurchases
     */

    public static class GetPurchasesResult {
        @Nullable // and expected to always be null; indicates glue problem
        public Error error;
        @Nullable // but never expected to be null
        public Purchases purchases;

        public GetPurchasesResult(JNI.Result.GetPurchases res) {
            this.error = res.error;
            if (this.error != null) {
                return;
            }
            this.purchases = res.purchases;
        }
    }

    @NonNull
    public GetPurchasesResult getPurchases() {
        String jsonStr = this.NativeGetPurchases();
        JNI.Result.GetPurchases res = new JNI.Result.GetPurchases(jsonStr);
        return new GetPurchasesResult(res);
    }

    /*
     * ValidPurchases
     */

    public static class ValidPurchasesResult {
        @Nullable // and expected to always be null; indicates glue problem
        public Error error;
        @Nullable // but never expected to be null
        public Purchases purchases;

        public ValidPurchasesResult(JNI.Result.ValidPurchases res) {
            this.error = res.error;
            if (this.error != null) {
                return;
            }
            this.purchases = res.purchases;
        }
    }

    @NonNull
    public ValidPurchasesResult validPurchases() {
        String jsonStr = this.NativeValidPurchases();
        JNI.Result.ValidPurchases res = new JNI.Result.ValidPurchases(jsonStr);
        return new ValidPurchasesResult(res);
    }

    /*
     * NextExpiringPurchase
     */

    public static class NextExpiringPurchaseResult {
        @Nullable // and expected to always be null; indicates glue problem
        public Error error;
        @Nullable // will be null if no such purchase
        public Purchase purchase;

        public NextExpiringPurchaseResult(JNI.Result.NextExpiringPurchase res) {
            this.error = res.error;
            if (this.error != null) {
                return;
            }
            this.purchase = res.purchase;
        }
    }

    @NonNull
    public NextExpiringPurchaseResult nextExpiringPurchase() {
        String jsonStr = this.NativeNextExpiringPurchase();
        JNI.Result.NextExpiringPurchase res = new JNI.Result.NextExpiringPurchase(jsonStr);
        return new NextExpiringPurchaseResult(res);
    }

    /*
     * ExpirePurchases
     */

    public static class ExpirePurchasesResult {
        @Nullable
        public Error error;
        @Nullable // may be null if no purchases were expired
        public Purchases purchases;

        public ExpirePurchasesResult(JNI.Result.ExpirePurchases res) {
            this.error = res.error;
            if (this.error != null) {
                return;
            }
            this.purchases = res.purchases;
        }
    }

    @NonNull
    public ExpirePurchasesResult expirePurchases() {
        String jsonStr = this.NativeExpirePurchases();
        JNI.Result.ExpirePurchases res = new JNI.Result.ExpirePurchases(jsonStr);
        return new ExpirePurchasesResult(res);
    }

    /*
     * RemovePurchases
     */

    /**
     * @returns null if no error; Error otherwise
     */
    @Nullable
    public Error removePurchases(List<String> transactionIDs) {
        if (transactionIDs == null) {
            return null;
        }
        String [] idsArray = transactionIDs.toArray(new String[0]);

        String jsonStr = this.NativeRemovePurchases(idsArray);
        JNI.Result.ErrorOnly res = new JNI.Result.ErrorOnly(jsonStr);
        return res.error;
    }

    /*
     * ModifyLandingPage
     */

    public static class ModifyLandingPageResult {
        @Nullable
        public Error error;
        @Nullable // null iff error
        public String url;

        public ModifyLandingPageResult(JNI.Result.ModifyLandingPage res) {
            this.error = res.error;
            if (this.error != null) {
                return;
            }
            this.url = res.url;
        }
    }

    @NonNull
    public ModifyLandingPageResult modifyLandingPage(String url) {
        String jsonStr = this.NativeModifyLandingPage(url);
        JNI.Result.ModifyLandingPage res = new JNI.Result.ModifyLandingPage(jsonStr);
        return new ModifyLandingPageResult(res);
    }

    /*
     * RefreshState
     */

    public static class RefreshStateResult {
        @Nullable
        public Error error;
        @Nullable // null iff error is non-null
        public Status status;

        public RefreshStateResult(JNI.Result.RefreshState res) {
            this.error = res.error;
            if (this.error != null) {
                return;
            }
            this.status = res.status;
        }
    }

    @NonNull
    public RefreshStateResult refreshState(List<String> purchaseClasses) {
        if (purchaseClasses == null) {
            purchaseClasses = new ArrayList<>();
        }

        String jsonStr = this.NativeRefreshState(purchaseClasses.toArray(new String[0]));

        JNI.Result.RefreshState res = new JNI.Result.RefreshState(jsonStr);
        return new RefreshStateResult(res);
    }

    /*
     * NewExpiringPurchase
     */

    public static class NewExpiringPurchaseResult {
        @Nullable
        public Error error;
        @Nullable
        public Status status;
        @Nullable
        public Purchase purchase;

        public NewExpiringPurchaseResult(JNI.Result.NewExpiringPurchase res) {
            this.error = res.error;
            if (this.error != null) {
                return;
            }
            this.status = res.status;
            this.purchase = res.purchase;
        }
    }

    @NonNull
    public NewExpiringPurchaseResult newExpiringPurchase(
            String transactionClass, String distinguisher, long expectedPrice) {
        String jsonStr = this.NativeNewExpiringPurchase(transactionClass, distinguisher, expectedPrice);
        JNI.Result.NewExpiringPurchase res = new JNI.Result.NewExpiringPurchase(jsonStr);
        return new NewExpiringPurchaseResult(res);
    }

    @SuppressWarnings("unused") // used as a native callback
    public String makeHTTPRequest(String jsonReqParams) {
        HTTPRequester.Result result = new HTTPRequester.Result();

        HTTPRequester.ReqParams reqParams = new HTTPRequester.ReqParams();
        Uri.Builder uriBuilder = new Uri.Builder();
        reqParams.headers = new HashMap<>();

        try {
            JSONObject json = new JSONObject(jsonReqParams);

            uriBuilder.scheme(JSON.nonnullString(json, "scheme"));

            String hostname = JSON.nonnullString(json, "hostname");

            Integer port = JSON.nullableInteger(json, "port");
            if (port != null) {
                hostname += ":" + port;
            }

            uriBuilder.encodedAuthority(hostname);

            reqParams.method = JSON.nonnullString(json, "method");

            uriBuilder.encodedPath(JSON.nonnullString(json, "path"));

            JSONObject jsonHeaders = JSON.nullableObject(json, "headers");
            if (jsonHeaders != null) {
                Iterator<?> headerKeys = jsonHeaders.keys();
                while (headerKeys.hasNext()) {
                    String key = (String)headerKeys.next();
                    String value = JSON.nonnullString(jsonHeaders, key);
                    reqParams.headers.put(key, value);
                }
            }

            // Query params are an array of arrays of 2 strings.
            JSONArray jsonQueryParams = JSON.nullableArray(json, "query");
            if (jsonQueryParams != null) {
                for (int i = 0; i < jsonQueryParams.length(); i++) {
                    JSONArray param = JSON.nonnullArray(jsonQueryParams, i);
                    String key = JSON.nullableString(param, 0);
                    String value = JSON.nullableString(param, 1);
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

        private static class Result {

            private static abstract class Base {
                @Nullable
                Error error; // Null iff there's no error

                public Base(String jsonStr) {
                    if (jsonStr == null) {
                        this.error = new Error("Base: got null JSON string", true);
                        return;
                    }

                    JSONObject json;
                    try {
                        json = new JSONObject(jsonStr);
                    } catch (JSONException e) {
                        this.error = new Error("Base: Overall JSON parse failed: " + e.getMessage(), true);
                        return;
                    }

                    this.error = Error.fromJSON(json);
                    if (this.error != null) {
                        // The JSON encoded an error
                        return;
                    }

                    // There's no error, so let's extract the result.
                    try {
                        this.fromJSON(json, kResultKey);
                    } catch (JSONException e) {
                        this.error = new Error("Base: Result JSON parse failed: " + e.getMessage(), true);
                        return;
                    }
                }

                // Will be called iff there's no error, so must produce a value (except for ErrorOnly)
                // or throw an exception.
                abstract void fromJSON(JSONObject json, String key) throws JSONException;
            }

            private static class ErrorOnly extends Base {
                public ErrorOnly(String jsonStr) {
                    super(jsonStr);
                }

                @Override
                public void fromJSON(JSONObject json, String key) {
                    // There's no result besides error or not-error
                }
            }

            private static class IsAccount extends Base {
                boolean isAccount;

                public IsAccount(String jsonStr) {
                    super(jsonStr);
                }

                @Override
                public void fromJSON(JSONObject json, String key) throws JSONException {
                    boolean b = JSON.nonnullBoolean(json, key);
                    this.isAccount = b;
                }
            }

            private static class ValidTokenTypes extends Base {
                List<String> validTokenTypes;

                public ValidTokenTypes(String jsonStr) {
                    super(jsonStr);
                }

                @Override
                public void fromJSON(JSONObject json, String key) {
                    // Allow for an null list (probably won't happen, but could represent no valid token types)
                    this.validTokenTypes = JSON.nullableList(String.class, json, key);
                }
            }

            private static class Balance extends Base {
                long balance;

                public Balance(String jsonStr) {
                    super(jsonStr);
                }

                @Override
                public void fromJSON(JSONObject json, String key) throws JSONException {
                    long l = JSON.nonnullLong(json, key);
                    this.balance = l;
                }
            }

            private static class GetPurchasePrices extends Base {
                PsiCashLib.PurchasePrices purchasePrices;

                public GetPurchasePrices(String jsonStr) {
                    super(jsonStr);
                }

                @Override
                public void fromJSON(JSONObject json, String key) throws JSONException {
                    this.purchasePrices = new PsiCashLib.PurchasePrices();

                    // We'll allow a null value to indicate no available purchase prices
                    JSONArray jsonArray = JSON.nullableArray(json, key);
                    if (jsonArray == null) {
                        return;
                    }

                    for (int i = 0; i < jsonArray.length(); i++) {
                        JSONObject ppJSON = JSON.nonnullObject(jsonArray, i);

                        PurchasePrice pp = new PurchasePrice();
                        pp.transactionClass = JSON.nonnullString(ppJSON, "class");
                        pp.distinguisher = JSON.nonnullString(ppJSON, "distinguisher");
                        pp.price = JSON.nonnullLong(ppJSON, "price");

                        this.purchasePrices.add(pp);
                    }
                }
            }

            private static class GetPurchases extends Base {
                PsiCashLib.Purchases purchases;

                public GetPurchases(String jsonStr) {
                    super(jsonStr);
                }

                @Override
                public void fromJSON(JSONObject json, String key) throws JSONException {
                    // We'll allow a null value to indicate no purchases
                    JSONArray jsonArray = JSON.nullableArray(json, key);
                    if (jsonArray == null) {
                        this.purchases = new PsiCashLib.Purchases();
                        return;
                    }

                    this.purchases = PsiCashLib.Purchases.fromJSON(jsonArray);
                }
            }

            private static class ValidPurchases extends Base {
                PsiCashLib.Purchases purchases;

                public ValidPurchases(String jsonStr) {
                    super(jsonStr);
                }

                @Override
                public void fromJSON(JSONObject json, String key) throws JSONException {
                    // We'll allow a null value to indicate no purchases
                    JSONArray jsonArray = JSON.nullableArray(json, key);
                    if (jsonArray == null) {
                        this.purchases = new PsiCashLib.Purchases();
                        return;
                    }

                    this.purchases = PsiCashLib.Purchases.fromJSON(jsonArray);
                }
            }

            private static class NextExpiringPurchase extends Base {
                PsiCashLib.Purchase purchase;

                public NextExpiringPurchase(String jsonStr) {
                    super(jsonStr);
                }

                @Override
                public void fromJSON(JSONObject json, String key) throws JSONException {
                    // Even a valid result may give a null value (iff no existing expiring purchases)                    json = JSON.nullableObject(json, key);
                    if (json == null) {
                        return;
                    }

                    this.purchase = PsiCashLib.Purchase.fromJSON(json);
                }
            }

            private static class ExpirePurchases extends Base {
                PsiCashLib.Purchases purchases;

                public ExpirePurchases(String jsonStr) {
                    super(jsonStr);
                }

                @Override
                public void fromJSON(JSONObject json, String key) throws JSONException {
                    // Null is allowable, as no purchases may have expired
                    JSONArray jsonArray = JSON.nullableArray(json, key);
                    if (jsonArray == null) {
                        return;
                    }

                    this.purchases = PsiCashLib.Purchases.fromJSON(jsonArray);
                }
            }

            private static class ModifyLandingPage extends Base {
                String url;

                public ModifyLandingPage(String jsonStr) {
                    super(jsonStr);
                }

                @Override
                public void fromJSON(JSONObject json, String key) throws JSONException {
                    this.url = JSON.nonnullString(json, key);
                }
            }

            private static class RefreshState extends Base {
                public Status status;

                public RefreshState(String jsonStr) {
                    super(jsonStr);
                }

                @Override
                public void fromJSON(JSONObject json, String key) throws JSONException {
                    this.status = Status.fromCode(JSON.nonnullInteger(json, key));
                }
            }

            private static class NewExpiringPurchase extends Base {
                public Status status;
                public Purchase purchase;

                public NewExpiringPurchase(String jsonStr) {
                    super(jsonStr);
                }

                @Override
                public void fromJSON(JSONObject json, String key) throws JSONException {
                    json = JSON.nonnullObject(json, key);

                    this.status = Status.fromCode(JSON.nonnullInteger(json, kStatusKey));

                    // Allow for null purchase, as it will only be populated on status==success.
                    this.purchase = Purchase.fromJSON(JSON.nullableObject(json, "purchase"));

                    if (this.status == Status.SUCCESS && this.purchase == null) {
                        // Not a sane state.
                        throw new JSONException("NewExpiringPurchase.fromJSON got SUCCESS but no purchase object" );
                    }
                }
            }
        }

    }

    //
    // JSON helpers class
    //

    private static class JSON {
        // The standard org.json.JSONObject does some unpleasant coercing of null into values, so
        // we're going to provide a bunch of helpers that behave sanely and consistently.
        // Please use these instead of the standard methods. (And consider adding more if any are missing.)

        @Nullable
        private static Boolean nullableBoolean(JSONObject json, String key) {
            if (!json.has(key) || json.isNull(key)) {
                return null;
            }
            return json.optBoolean(key, false);
        }

        @NonNull
        private static boolean nonnullBoolean(JSONObject json, String key) throws JSONException {
            Boolean v = nullableBoolean(json, key);
            if (v == null) {
                throw new JSONException("nonnullBoolean can't find non-null key: " + key);
            }
            return v;
        }

        @Nullable
        private static Double nullableDouble(JSONObject json, String key) {
            if (!json.has(key) || json.isNull(key)) {
                return null;
            }
            return json.optDouble(key, 0.0);
        }

        @NonNull
        private static double nonnullDouble(JSONObject json, String key) throws JSONException {
            Double v = nullableDouble(json, key);
            if (v == null) {
                throw new JSONException("nonnullDouble can't find non-null key: " + key);
            }
            return v;
        }

        @Nullable
        private static Integer nullableInteger(JSONObject json, String key) {
            if (!json.has(key) || json.isNull(key)) {
                return null;
            }
            return json.optInt(key, 0);
        }

        @NonNull
        private static int nonnullInteger(JSONObject json, String key) throws JSONException {
            Integer v = nullableInteger(json, key);
            if (v == null) {
                throw new JSONException("nonnullInteger can't find non-null key: " + key);
            }
            return v;
        }

        @Nullable
        private static Long nullableLong(JSONObject json, String key) {
            if (!json.has(key) || json.isNull(key)) {
                return null;
            }
            return json.optLong(key, 0L);
        }

        @NonNull
        private static long nonnullLong(JSONObject json, String key) throws JSONException {
            Long v = nullableLong(json, key);
            if (v == null) {
                throw new JSONException("nonnullLong can't find non-null key: " + key);
            }
            return v;
        }

        @Nullable
        private static String nullableString(JSONObject json, String key) {
            if (!json.has(key) || json.isNull(key)) {
                return null;
            }
            return json.optString(key, null);
        }

        @Nullable
        private static String nullableString(JSONArray json, int index) {
            if (index >= json.length() || json.isNull(index)) {
                return null;
            }
            return json.optString(index, null);
        }

        @NonNull
        private static String nonnullString(JSONArray json, int index) throws JSONException {
            String v = nullableString(json, index);
            if (v == null) {
                throw new JSONException("nonnullString can't find non-null index: " + index);
            }
            return v;
        }

        @NonNull
        private static String nonnullString(JSONObject json, String key) throws JSONException {
            String v = nullableString(json, key);
            if (v == null) {
                throw new JSONException("nonnullString can't find non-null key: " + key);
            }
            return v;
        }

        @Nullable
        private static JSONObject nullableObject(JSONObject json, String key) {
            if (!json.has(key) || json.isNull(key)) {
                return null;
            }
            return json.optJSONObject(key);
        }

        @NonNull
        private static JSONObject nonnullObject(JSONObject json, String key) throws JSONException {
            JSONObject v = nullableObject(json, key);
            if (v == null) {
                throw new JSONException("nonnullObject can't find non-null key: " + key);
            }
            return v;
        }

        @Nullable
        private static JSONObject nullableObject(JSONArray json, int index) {
            if (index >= json.length() || json.isNull(index)) {
                return null;
            }
            return json.optJSONObject(index);
        }

        @NonNull
        private static JSONObject nonnullObject(JSONArray json, int index) throws JSONException {
            JSONObject v = nullableObject(json, index);
            if (v == null) {
                throw new JSONException("nonnullObject can't find non-null index: " + index);
            }
            return v;
        }

        @Nullable
        private static JSONArray nullableArray(JSONObject json, String key) {
            if (!json.has(key) || json.isNull(key)) {
                return null;
            }
            return json.optJSONArray(key);
        }

        @NonNull
        private static JSONArray nonnullArray(JSONObject json, String key) throws JSONException {
            JSONArray v = nullableArray(json, key);
            if (v == null) {
                throw new JSONException("nonnullArray can't find non-null key: " + key);
            }
            return v;
        }

        @Nullable
        private static JSONArray nullableArray(JSONArray json, int index) {
            if (index >= json.length() || json.isNull(index)) {
                return null;
            }
            return json.optJSONArray(index);
        }

        @NonNull
        private static JSONArray nonnullArray(JSONArray json, int index) throws JSONException {
            JSONArray v = nullableArray(json, index);
            if (v == null) {
                throw new JSONException("nonnullArray can't find non-null index: " + index);
            }
            return v;
        }

        // This function throws if the JSON field is present, but cannot be converted to a Date.
        @Nullable
        private static Date nullableDate(JSONObject json, String key) throws JSONException {
            String dateString = nullableString(json, key);
            if (dateString == null) {
                return null;
            }

            Date date;

            // We need to try different formats depending on the presence of milliseconds.
            SimpleDateFormat isoFormatWithMS = new SimpleDateFormat("yyyy-MM-dd'T'hh:mm:ss.SSS'Z'", Locale.US);
            try {
                date = isoFormatWithMS.parse(dateString);
            } catch (ParseException e1) {
                SimpleDateFormat isoFormatWithoutMS = new SimpleDateFormat("yyyy-MM-dd'T'hh:mm:ss'Z'", Locale.US);
                try {
                    date = isoFormatWithoutMS.parse(dateString);
                } catch (ParseException e2) {
                    // Should not happen. No way to recover.
                    throw new JSONException("Failed to parse date with key " + key + "; error: " + e2.toString());
                }
            }

            return date;
        }

        @NonNull
        private static Date nonnullDate(JSONObject json, String key) throws JSONException {
            Date v = nullableDate(json, key);
            if (v == null) {
                throw new JSONException("nonnullDate can't find non-null key: " + key);
            }
            return v;
        }

        @Nullable
        private static <T> List<T> nullableList(Class<T> clazz, JSONObject json, String key) {
            JSONArray jsonArray = nullableArray(json, key);
            if (jsonArray == null) {
                return null;
            }

            ArrayList<T> result = new ArrayList<>();

            for (int i = 0; i < jsonArray.length(); i++) {
                result.add(nullable(clazz, jsonArray, i));
            }

            return result;
        }

        // Helper; should (probably) not be used directly
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

        // Helper; should (probably) not be used directly
        @Nullable
        private static <T> T nullable(Class<T> clazz, JSONArray json, int key) {
            Object o = json.opt(key);
            return cast(clazz, o);
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

    static {
        // Load the C++ library.
        System.loadLibrary("psicash");

        // Call the C++ init function each time the library loads.
        if (!NativeStaticInit()) {
            // This shouldn't happen, unless the apk is misconfigured.
            throw new AssertionError("psicash library init failed");
        }
    }

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

    /**
     * @return {
     *  "error": {...},
     *  "result": long
     * }
     */
    private native String NativeBalance();

    /**
     * @return {
     *  "error": {...},
     *  "result": [ ... PurchasePrices ... ]
     * }
     */
    private native String NativeGetPurchasePrices();

    /**
     * @return {
     *  "error": {...},
     *  "result": [ ... Purchases ... ]
     * }
     */
    private native String NativeValidPurchases();

    /**
     * @return {
     *  "error": {...},
     *  "result": [ ... Purchases ... ]
     * }
     */
    private native String NativeGetPurchases();

    /**
     * @return {
     *  "error": {...},
     *  "result": Purchase or null
     * }
     */
    private native String NativeNextExpiringPurchase();

    /**
     * @return {
     *  "error": {...},
     *  "result": [ ... Purchases ... ]
     * }
     */
    private native String NativeExpirePurchases();

    /**
     * @return {
     *  "error": {...}
     * }
     */
    private native String NativeRemovePurchases(String[] transaction_ids);

    /**
     * @return {
     *  "error": {...}
     *  "result": modified url string
     * }
     */
    private native String NativeModifyLandingPage(String url);

    /**
     * @return {
     *  "error": {...},
     *  "result": Status
     * }
     */
    private native String NativeRefreshState(String[] purchaseClasses);

    /**
     * @return {
     *  "error": {...},
     *  "result": {
     *      "status": Status,
     *      "purchase": Purchase
     *  }
     * }
     */
    private native String NativeNewExpiringPurchase(String transactionClass, String distinguisher, long expectedPrice);

    /*
     * TEST ONLY Native functions
     * It doesn't seem possible to have these declared in the PsiCashLibTester subclass.
     */

    protected native String NativeTestReward(String transactionClass, String distinguisher);
}
