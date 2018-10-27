package ca.psiphon.psicash;

import android.os.AsyncTask;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import ca.psiphon.psicashlib.PsiCashLib;

public class MainActivity extends AppCompatActivity {
    private PsiCashLib psiCashLib;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_main);

        TextView tv = findViewById(R.id.sample_text);
        tv.setText("Initial text");

        psiCashLib = new PsiCashLib();
        PsiCashLib.Error err = psiCashLib.init(getFilesDir().toString(), new PsiCashLibHelper());
        if (err != null) {
            Log.e("temptag", err.message);
        }
        else {
            new NetworkTask().execute();
        }
    }

    private class NetworkTask extends AsyncTask<Void, Void, String> {

        @Override
        protected String doInBackground(Void... params) {
            PsiCashLib.Error error = psiCashLib.setRequestMetadataItem("metadatakey", "metadatavalue");
            if (error != null) {
                Log.e("temptag", error.message);
            }

            error = psiCashLib.setRequestMetadataItem(null, "blah"); //erroneous
            if (error != null) {
                Log.e("temptag", error.message);
            }

            List<String> purchaseClasses = new ArrayList<>(Arrays.asList("speed-boost"));
            PsiCashLib.RefreshStateResult rsr = psiCashLib.refreshState(purchaseClasses);

            PsiCashLib.IsAccountResult isAccount = psiCashLib.isAccount();
            PsiCashLib.ValidTokenTypesResult vtt = psiCashLib.validTokenTypes();
            PsiCashLib.BalanceResult b = psiCashLib.balance();
            PsiCashLib.GetPurchasePricesResult pp = psiCashLib.getPurchasePrices();
            PsiCashLib.GetPurchasesResult gpr = psiCashLib.getPurchases();
            PsiCashLib.ValidPurchasesResult vpr = psiCashLib.validPurchases();
            PsiCashLib.NextExpiringPurchaseResult ner = psiCashLib.nextExpiringPurchase();
            PsiCashLib.ExpirePurchasesResult epr = psiCashLib.expirePurchases();

            List<String> ids = new ArrayList<>(Arrays.asList("id1", "id2"));
            error = psiCashLib.removePurchases(ids);

            PsiCashLib.ModifyLandingPageResult mlpr = psiCashLib.modifyLandingPage("https://example.com/foo");
            PsiCashLib.GetRewardedActivityDataResult gradr = psiCashLib.getRewardedActivityData();


            PsiCashLib.NewExpiringPurchaseResult nep = psiCashLib.newExpiringPurchase(
                    "speed-boost",
                    "1hr",
                    100000000000L);

            return nep.status.toString();
        }

        @Override
        protected void onPostExecute(String s) {
            super.onPostExecute(s);

            s = (s == null ? "<null>" : s);

            TextView tv = findViewById(R.id.sample_text);
            tv.setText(s);
            Log.i("json", s);
        }
    }
}
