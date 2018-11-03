/*
 * Copyright (c) 2018, Psiphon Inc.
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

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


/**
 * The PsiCash library interface. It provides a wrapper around the C++ core.
 */
public class PsiCashLib {
    /**
     * The library user must implement this interface. It provides HTTP request
     * functionality to the library.
     * It is up to the implementer to decide which thread the request should be executed
     * on, and whether the request should be proxied, etc.
     */
    public interface HTTPRequester {
        /**
         * The HTTP requester. Must take care of TLS, proxying, etc.
         */
        Result httpRequest(ReqParams reqParams);

        /**
         * The input to the HTTP requseter.
         */
        class ReqParams {
            public String method;
            public Uri uri;
            public Map<String, String> headers;
        }

        /**
         * The output from the HTTP requester.
         */
        class Result {
            public int code = -1; // -1 indicates error trying to make the request
            public String body;
            public String date;
            public String error;

            String toJSON() {
                JSONObject json = new JSONObject();
                try {
                    json.put("code", this.code);
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
    }

    private HTTPRequester httpRequester;

    // Common fields in the JNI glue messages.
    private static final String kErrorKey = "error";
    private static final String kErrorMessageKey = "message";
    private static final String kErrorInternalKey = "internal";
    private static final String kResultKey = "result";
    private static final String kStatusKey = "status";

    /**
     * Possible status values for many of the API methods. Specific meanings will be
     * described in the method comments.
     */
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

    /**
     * Error structure returned by many API methods.
     */
    public static class Error {
        @NonNull // If Error is set, it must have a message
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

        @Nullable // if null, there's no error in json
        static Error fromJSON(JSONObject json) {
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
    }

    /**
     * The possible token types.
     */
    public enum TokenType {
        EARNER("earner"),
        SPENDER("spender"),
        INDICATOR("indicator"),
        ACCOUNT("account");

        private final String name;

        TokenType(String name) {
            this.name = name;
        }

        public static TokenType fromName(String name) {
            for (TokenType tt : TokenType.values()) {
                if (tt.name.equals(name)) return tt;
            }
            throw new IllegalArgumentException("TokenType not found");
        }

        public boolean equals(String name) {
            return this.name.equals(name);
        }
    }

    /**
     * Purchase price information.
     */
    public static class PurchasePrice {
        public String transactionClass;
        public String distinguisher;
        public long price;

        static PurchasePrice fromJSON(JSONObject json) throws JSONException {
            if (json == null) {
                return null;
            }
            PurchasePrice pp = new PurchasePrice();
            pp.transactionClass = JSON.nonnullString(json, "class");
            pp.distinguisher = JSON.nonnullString(json, "distinguisher");
            pp.price = JSON.nonnullLong(json, "price");
            return pp;
        }
    }

    /**
     * Purchase information.
     */
    public static class Purchase {
        public String id;
        public String transactionClass;
        public String distinguisher;
        public Date expiry;
        public String authorization;

        static Purchase fromJSON(JSONObject json) throws JSONException {
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

    /*
     * Begin methods
     */

    public PsiCashLib() {
    }

    /**
     * Initializes the library. Must be called before any other methods are invoked.
     * @param fileStoreRoot The directory where the library will store its data. Must exist.
     * @param httpRequester Helper used to make HTTP requests.
     * @return null if no error; Error otherwise.
     */
    @Nullable
    synchronized public Error init(String fileStoreRoot, HTTPRequester httpRequester) {
        return init(fileStoreRoot, httpRequester, false);
    }

    /**
     * Used internally for testing.
     * @param test Should be true if testing mode (and server) is to be used.
     * @return null if no error; Error otherwise.
     */
    @Nullable
    synchronized protected Error init(String fileStoreRoot, HTTPRequester httpRequester, boolean test) {
        this.httpRequester = httpRequester;
        String jsonStr = this.NativeObjectInit(fileStoreRoot, test);
        JNI.Result.ErrorOnly res = new JNI.Result.ErrorOnly(jsonStr);
        return res.error;
    }

    /**
     * Set values that will be included in the request metadata. This includes
     * client_version, client_region, sponsor_id, and propagation_channel_id.
     * @return null if no error; Error otherwise.
     */
    @Nullable
    synchronized public Error setRequestMetadataItem(String key, String value) {
        String jsonStr = this.SetRequestMetadataItem(key, value);
        JNI.Result.ErrorOnly res = new JNI.Result.ErrorOnly(jsonStr);
        return res.error;
    }

    /*
     * ValidTokenTypes
     */

    /**
     * Returns the stored valid token types. Like ["spender", "indicator"].
     * @return List will be empty if no tokens are available.
     */
    @NonNull
    synchronized public ValidTokenTypesResult validTokenTypes() {
        String jsonStr = this.NativeValidTokenTypes();
        JNI.Result.ValidTokenTypes res = new JNI.Result.ValidTokenTypes(jsonStr);
        return new ValidTokenTypesResult(res);
    }

    public static class ValidTokenTypesResult {
        // Expected to be null; indicates glue problem.
        public Error error;

        // Null iff error (which is not expected).
        // Will be empty if no tokens are available.
        public List<TokenType> validTokenTypes;

        ValidTokenTypesResult(JNI.Result.ValidTokenTypes res) {
            this.error = res.error;
            if (this.error != null) {
                return;
            }
            this.validTokenTypes = res.validTokenTypes;
        }
    }

    /**
     * Retrieve the stored info about whether the user is a Tracker or an Account.
     */
    @NonNull
    synchronized public IsAccountResult isAccount() {
        String jsonStr = this.NativeIsAccount();
        JNI.Result.IsAccount res = new JNI.Result.IsAccount(jsonStr);
        return new IsAccountResult(res);
    }

    public static class IsAccountResult {
        // Expected to be null; indicates glue problem.
        public Error error;
        public boolean isAccount;

        IsAccountResult(JNI.Result.IsAccount res) {
            this.error = res.error;
            if (this.error != null) {
                return;
            }
            this.isAccount = res.isAccount;
        }
    }

    /**
     * Retrieve the stored user balance.
     */
    @NonNull
    synchronized public BalanceResult balance() {
        String jsonStr = this.NativeBalance();
        JNI.Result.Balance res = new JNI.Result.Balance(jsonStr);
        return new BalanceResult(res);
    }

    public static class BalanceResult {
        // Expected to be null; indicates glue problem.
        public Error error;
        public long balance;

        BalanceResult(JNI.Result.Balance res) {
            this.error = res.error;
            if (this.error != null) {
                return;
            }
            this.balance = res.balance;
        }
    }

    /**
     * Retrieves the stored purchase prices.
     * @return List will be empty if there are no available purchase prices.
     */
    @NonNull
    synchronized public GetPurchasePricesResult getPurchasePrices() {
        String jsonStr = this.NativeGetPurchasePrices();
        JNI.Result.GetPurchasePrices res = new JNI.Result.GetPurchasePrices(jsonStr);
        return new GetPurchasePricesResult(res);
    }

    public static class GetPurchasePricesResult {
        // Expected to be null; indicates glue problem.
        public Error error;
        // Null iff error (which is not expected).
        public List<PurchasePrice> purchasePrices;

        GetPurchasePricesResult(JNI.Result.GetPurchasePrices res) {
            this.error = res.error;
            if (this.error != null) {
                return;
            }
            this.purchasePrices = res.purchasePrices;
        }
    }

    /**
     * Retrieves the set of active purchases, if any.
     * @return List will be empty if there are no purchases.
     */
    @NonNull
    synchronized public GetPurchasesResult getPurchases() {
        String jsonStr = this.NativeGetPurchases();
        JNI.Result.GetPurchases res = new JNI.Result.GetPurchases(jsonStr);
        return new GetPurchasesResult(res);
    }

    public static class GetPurchasesResult {
        // Expected to be null; indicates glue problem.
        public Error error;
        // Null iff error (which is not expected).
        public List<Purchase> purchases;

        GetPurchasesResult(JNI.Result.GetPurchases res) {
            this.error = res.error;
            if (this.error != null) {
                return;
            }
            this.purchases = res.purchases;
        }
    }

    /**
     * Retrieves the set of active purchases that are not expired, if any.
     * @return List will be empty if there are no valid purchases.
     */
    @NonNull
    synchronized public ValidPurchasesResult validPurchases() {
        String jsonStr = this.NativeValidPurchases();
        JNI.Result.ValidPurchases res = new JNI.Result.ValidPurchases(jsonStr);
        return new ValidPurchasesResult(res);
    }

    public static class ValidPurchasesResult {
        // Expected to be null; indicates glue problem.
        public Error error;
        // Null iff error (which is not expected).
        public List<Purchase> purchases;

        ValidPurchasesResult(JNI.Result.ValidPurchases res) {
            this.error = res.error;
            if (this.error != null) {
                return;
            }
            this.purchases = res.purchases;
        }
    }

    /**
     * Get the next expiring purchase (with local_time_expiry populated).
     * @return If there is no expiring purchase, the returned purchase will be null.
     * The returned purchase may already be expired.
     */
    @NonNull
    synchronized public NextExpiringPurchaseResult nextExpiringPurchase() {
        String jsonStr = this.NativeNextExpiringPurchase();
        JNI.Result.NextExpiringPurchase res = new JNI.Result.NextExpiringPurchase(jsonStr);
        return new NextExpiringPurchaseResult(res);
    }

    public static class NextExpiringPurchaseResult {
        // Expected to be null; indicates glue problem.
        public Error error;
        // Null if error, or if there is no such purchase.
        public Purchase purchase;

        NextExpiringPurchaseResult(JNI.Result.NextExpiringPurchase res) {
            this.error = res.error;
            if (this.error != null) {
                return;
            }
            this.purchase = res.purchase;
        }
    }

    /**
     * Clear out expired purchases. Return the ones that were expired, if any.
     * @return List will be empty if there are no expired purchases.
     */
    @NonNull
    synchronized public ExpirePurchasesResult expirePurchases() {
        String jsonStr = this.NativeExpirePurchases();
        JNI.Result.ExpirePurchases res = new JNI.Result.ExpirePurchases(jsonStr);
        return new ExpirePurchasesResult(res);
    }

    public static class ExpirePurchasesResult {
        // Null if storage writing problem or glue problem.
        public Error error;
        // Null iff error (which is not expected). Empty if there were no expired purchases.
        public List<Purchase> purchases;

        ExpirePurchasesResult(JNI.Result.ExpirePurchases res) {
            this.error = res.error;
            if (this.error != null) {
                return;
            }
            this.purchases = res.purchases;
        }
    }

    /**
     * Force removal of purchases with the given transaction IDs.
     * This is to be called when the Psiphon server indicates that a purchase has
     * expired (even if the local clock hasn't yet indicated it).
     * @param transactionIDs List of transaction IDs of purchases to remove. IDs not being
     *                       found does _not_ result in an error.
     * @return null if success; Error otherwise.
     */
    @Nullable
    synchronized public Error removePurchases(List<String> transactionIDs) {
        if (transactionIDs == null) {
            return null;
        }
        String[] idsArray = transactionIDs.toArray(new String[0]);
        String jsonStr = this.NativeRemovePurchases(idsArray);
        JNI.Result.ErrorOnly res = new JNI.Result.ErrorOnly(jsonStr);
        return res.error;
    }

    /**
     * Utilizes stored tokens and metadata to craft a landing page URL.
     * @param url URL of landing page to modify.
     * @return Error if modification is impossible. (In that case the error should be
     * logged -- and added to feedback -- and home page opening should proceed
     * with the original URL.)
     */
    @NonNull
    synchronized public ModifyLandingPageResult modifyLandingPage(String url) {
        String jsonStr = this.NativeModifyLandingPage(url);
        JNI.Result.ModifyLandingPage res = new JNI.Result.ModifyLandingPage(jsonStr);
        return new ModifyLandingPageResult(res);
    }

    public static class ModifyLandingPageResult {
        public Error error;
        // Null iff error.
        public String url;

        ModifyLandingPageResult(JNI.Result.ModifyLandingPage res) {
            this.error = res.error;
            if (this.error != null) {
                return;
            }
            this.url = res.url;
        }
    }

    /**
     * Creates a data package that should be included with a webhook for a user
     * action that should be rewarded (such as watching a rewarded video).
     * NOTE: The resulting string will still need to be encoded for use in a URL.
     * Returns an error if there is no earner token available and therefore the
     * reward cannot possibly succeed. (Error may also result from a JSON
     * serialization problem, but that's very improbable.)
     * So, the library user may want to call this _before_ showing the rewarded
     * activity, to perhaps decide _not_ to show that activity. An exception may be
     * if the Psiphon connection attempt and subsequent RefreshClientState may
     * occur _during_ the rewarded activity, so an earner token may be obtained
     * before it's complete.
     */
    @NonNull
    synchronized public GetRewardedActivityDataResult getRewardedActivityData() {
        String jsonStr = this.NativeGetRewardedActivityData();
        JNI.Result.GetRewardedActivityData res = new JNI.Result.GetRewardedActivityData(jsonStr);
        return new GetRewardedActivityDataResult(res);
    }

    public static class GetRewardedActivityDataResult {
        public Error error;
        // Can be null even on success (error==null), if there is no data.
        public String data;

        GetRewardedActivityDataResult(JNI.Result.GetRewardedActivityData res) {
            this.error = res.error;
            if (this.error != null) {
                return;
            }
            this.data = res.data;
        }
    }

    /**
     * Returns a JSON object suitable for serializing that can be included in a feedback
     * diagnostic data package.
     */
    @NonNull
    synchronized public GetDiagnosticInfoResult getDiagnosticInfo() {
        String jsonStr = this.NativeGetDiagnosticInfo();
        JNI.Result.GetDiagnosticInfo res = new JNI.Result.GetDiagnosticInfo(jsonStr);
        return new GetDiagnosticInfoResult(res);
    }

    public static class GetDiagnosticInfoResult {
        public Error error;
        // Null iff error.
        public String jsonString;

        GetDiagnosticInfoResult(JNI.Result.GetDiagnosticInfo res) {
            this.error = res.error;
            if (this.error != null) {
                return;
            }
            this.jsonString = res.jsonString;
        }
    }

    /**
     * Refresh the local state (and obtain tokens, if necessary).
     * See psicash.hpp for full description.
     * @param purchaseClasses The purchase class names for which prices should be
     *                        retrieved, like `{"speed-boost"}`. If null or empty, no
     *                        purchase prices will be retrieved.
     * @return Error or request status. Even if error isn't set, request may have failed
     * for the reason indicated by the status.
     */
    @NonNull
    synchronized public RefreshStateResult refreshState(List<String> purchaseClasses) {
        if (purchaseClasses == null) {
            purchaseClasses = new ArrayList<>();
        }
        String jsonStr = this.NativeRefreshState(purchaseClasses.toArray(new String[0]));
        JNI.Result.RefreshState res = new JNI.Result.RefreshState(jsonStr);
        return new RefreshStateResult(res);
    }

    public static class RefreshStateResult {
        // Indicates catastrophic inability to make request.
        public Error error;
        // Null iff error.
        public Status status;

        RefreshStateResult(JNI.Result.RefreshState res) {
            this.error = res.error;
            if (this.error != null) {
                return;
            }
            this.status = res.status;
        }
    }

    /**
     * Makes a new transaction for an "expiring-purchase" class, such as "speed-boost".
     * See psicash.hpp for full description.
     * @param transactionClass The class name of the desired purchase. (Like "speed-boost".)
     * @param distinguisher    The distinguisher for the desired purchase. (Like "1hr".)
     * @param expectedPrice    The expected price of the purchase (previously obtained by
     *                         RefreshState). The transaction will fail if the
     *                         expectedPrice does not match the actual price.
     * @return Error or request status. Even if error isn't set, request may have failed
     * for the reason indicated by the status.
     */
    @NonNull
    synchronized public NewExpiringPurchaseResult newExpiringPurchase(
            String transactionClass, String distinguisher, long expectedPrice) {
        String jsonStr = this.NativeNewExpiringPurchase(transactionClass, distinguisher, expectedPrice);
        JNI.Result.NewExpiringPurchase res = new JNI.Result.NewExpiringPurchase(jsonStr);
        return new NewExpiringPurchaseResult(res);
    }

    public static class NewExpiringPurchaseResult {
        // Indicates catastrophic inability to make request.
        public Error error;
        // Null iff error.
        public Status status;
        // Will be non-null on status==SUCCESS, but null for all other statuses.
        public Purchase purchase;

        NewExpiringPurchaseResult(JNI.Result.NewExpiringPurchase res) {
            this.error = res.error;
            if (this.error != null) {
                return;
            }
            this.status = res.status;
            this.purchase = res.purchase;
        }
    }

    //
    // END API ////////////////////////////////////////////////////////////////
    ///

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
        // Ensure sanity if there's an error: code must be -1 iff there's an error message
        if ((result.code == -1) != (result.error != null && !result.error.isEmpty())) {
            result.code = -1;
            result.error = "Request result is not in sane error state: " + result.toString();
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
                ErrorOnly(String jsonStr) {
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
                    this.isAccount = JSON.nonnullBoolean(json, key);
                }
            }

            private static class ValidTokenTypes extends Base {
                List<TokenType> validTokenTypes;

                public ValidTokenTypes(String jsonStr) {
                    super(jsonStr);
                }

                @Override
                public void fromJSON(JSONObject json, String key) {
                    // Allow for an null list (probably won't happen, but could represent no valid token types)
                    this.validTokenTypes = JSON.nullableList(TokenType.class, json, key);
                }
            }

            private static class Balance extends Base {
                long balance;

                public Balance(String jsonStr) {
                    super(jsonStr);
                }

                @Override
                public void fromJSON(JSONObject json, String key) throws JSONException {
                    this.balance = JSON.nonnullLong(json, key);
                }
            }

            private static class GetPurchasePrices extends Base {
                List<PurchasePrice> purchasePrices;

                public GetPurchasePrices(String jsonStr) {
                    super(jsonStr);
                }

                @Override
                public void fromJSON(JSONObject json, String key) {
                    this.purchasePrices = JSON.nullableList(
                            PsiCashLib.PurchasePrice.class, json, key, PsiCashLib.PurchasePrice::fromJSON, true);
                }
            }

            private static class GetPurchases extends Base {
                List<Purchase> purchases;

                public GetPurchases(String jsonStr) {
                    super(jsonStr);
                }

                @Override
                public void fromJSON(JSONObject json, String key) {
                    this.purchases = JSON.nullableList(
                            PsiCashLib.Purchase.class, json, key, PsiCashLib.Purchase::fromJSON, true);
                }
            }

            private static class ValidPurchases extends Base {
                List<Purchase> purchases;

                public ValidPurchases(String jsonStr) {
                    super(jsonStr);
                }

                @Override
                public void fromJSON(JSONObject json, String key) {
                    this.purchases = JSON.nullableList(
                            PsiCashLib.Purchase.class, json, key, PsiCashLib.Purchase::fromJSON, true);
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
                    json = JSON.nullableObject(json, key);
                    if (json == null) {
                        return;
                    }
                    this.purchase = PsiCashLib.Purchase.fromJSON(json);
                }
            }

            private static class ExpirePurchases extends Base {
                List<Purchase> purchases;

                public ExpirePurchases(String jsonStr) {
                    super(jsonStr);
                }

                @Override
                public void fromJSON(JSONObject json, String key) {
                    this.purchases = JSON.nullableList(
                            PsiCashLib.Purchase.class, json, key, PsiCashLib.Purchase::fromJSON, true);
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

            private static class GetRewardedActivityData extends Base {
                String data;

                public GetRewardedActivityData(String jsonStr) {
                    super(jsonStr);
                }

                @Override
                public void fromJSON(JSONObject json, String key) {
                    // Can be null even on success
                    this.data = JSON.nullableString(json, key);
                }
            }

            private static class GetDiagnosticInfo extends Base {
                String jsonString;

                public GetDiagnosticInfo(String jsonStr) {
                    super(jsonStr);
                }

                @Override
                public void fromJSON(JSONObject json, String key) throws JSONException {
                    this.jsonString = JSON.nonnullString(json, key);
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
                        throw new JSONException("NewExpiringPurchase.fromJSON got SUCCESS but no purchase object");
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

        // To be used for JSON-primitive types (String, boolean, etc.).
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

        interface Deserializer {
            Object fromJSON(JSONObject json) throws JSONException;
        }

        // Deserialize a list using an object deserializer method. If supplyDefault is
        // true, an empty list will be returned rather than null.
        @Nullable
        private static <T> List<T> nullableList(Class<T> clazz, JSONObject json, String key, Deserializer deserializer, boolean supplyDefault) {
            JSONArray jsonArray = nullableArray(json, key);
            if (jsonArray == null) {
                return supplyDefault ? new ArrayList<>() : null;
            }

            ArrayList<T> result = new ArrayList<>();

            for (int i = 0; i < jsonArray.length(); i++) {
                JSONObject j = JSON.nullableObject(jsonArray, i);
                if (j == null) {
                    continue;
                }
                try {
                    result.add(cast(clazz, deserializer.fromJSON(j)));
                } catch (JSONException e) {
                    continue;
                }
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
     * NOTE: Full descriptions of what these methods do are in psicash.hpp
     * and will not be repeated here.
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
     * "error": {...},
     * "result": boolean
     * }
     */
    private native String NativeIsAccount();

    /**
     * @return {
     * "error": {...},
     * "result": ["earner", "indicator", ...]
     * }
     */
    private native String NativeValidTokenTypes();

    /**
     * @return {
     * "error": {...},
     * "result": long
     * }
     */
    private native String NativeBalance();

    /**
     * @return {
     * "error": {...},
     * "result": [ ... PurchasePrices ... ]
     * }
     */
    private native String NativeGetPurchasePrices();

    /**
     * @return {
     * "error": {...},
     * "result": [ ... Purchases ... ]
     * }
     */
    private native String NativeValidPurchases();

    /**
     * @return {
     * "error": {...},
     * "result": [ ... Purchases ... ]
     * }
     */
    private native String NativeGetPurchases();

    /**
     * @return {
     * "error": {...},
     * "result": Purchase or null
     * }
     */
    private native String NativeNextExpiringPurchase();

    /**
     * @return {
     * "error": {...},
     * "result": [ ... Purchases ... ]
     * }
     */
    private native String NativeExpirePurchases();

    /**
     * @return {
     * "error": {...}
     * }
     */
    private native String NativeRemovePurchases(String[] transaction_ids);

    /**
     * @return {
     * "error": {...}
     * "result": modified url string
     * }
     */
    private native String NativeModifyLandingPage(String url);

    /**
     * @return {
     * "error": {...}
     * "result": string encoded data
     * }
     */
    private native String NativeGetRewardedActivityData();

    /**
     * @return {
     * "error": {...}
     * "result": diagnostic JSON as string
     * }
     */
    private native String NativeGetDiagnosticInfo();

    /**
     * @return {
     * "error": {...},
     * "result": Status
     * }
     */
    private native String NativeRefreshState(String[] purchaseClasses);

    /**
     * @return {
     * "error": {...},
     * "result": {
     * "status": Status,
     * "purchase": Purchase
     * }
     * }
     */
    private native String NativeNewExpiringPurchase(String transactionClass, String distinguisher, long expectedPrice);

    /*
     * TEST ONLY Native functions
     * It doesn't seem possible to have these declared in the PsiCashLibTester subclass.
     */

    protected native String NativeTestReward(String transactionClass, String distinguisher);

    protected native boolean NativeTestSetRequestMutators(String[] mutators);
}
