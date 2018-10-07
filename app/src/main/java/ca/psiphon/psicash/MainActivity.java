package ca.psiphon.psicash;

import android.os.AsyncTask;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;

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
        String err = psiCashLib.init(getFilesDir().toString(), new PsiCashLibHelper());
        if (err != null) {
            Log.e("temptag", err);
        }
        else {
            new NetworkTask().execute();
        }
    }

    private class NetworkTask extends AsyncTask<Void, Void, String> {

        @Override
        protected String doInBackground(Void... params) {
            return psiCashLib.newTrackerWrapper();
        }

        @Override
        protected void onPostExecute(String s) {
            super.onPostExecute(s);
            TextView tv = findViewById(R.id.sample_text);
            tv.setText(s);
            Log.i("json", s);
        }
    }
}
