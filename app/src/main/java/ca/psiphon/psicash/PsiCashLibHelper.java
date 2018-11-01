package ca.psiphon.psicash;

import android.net.Uri;
import android.util.Log;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.Map;

import ca.psiphon.psicashlib.PsiCashLib;

public class PsiCashLibHelper implements PsiCashLib.HTTPRequester {
    public PsiCashLib.HTTPRequester.Result httpRequest(PsiCashLib.HTTPRequester.ReqParams reqParams) {
        PsiCashLib.HTTPRequester.Result res = new PsiCashLib.HTTPRequester.Result();

        HttpURLConnection urlConn = null;
        BufferedReader reader = null;

        try {
            URL url = new URL(reqParams.uri.toString());

            urlConn = (HttpURLConnection)url.openConnection();
            urlConn.setUseCaches(false);
            urlConn.setRequestMethod(reqParams.method);

            if (reqParams.headers != null) {
                for (Map.Entry<String, String> h : reqParams.headers.entrySet()) {
                    urlConn.setRequestProperty(h.getKey(), h.getValue());
                }
            }

            urlConn.connect();

            res.code = urlConn.getResponseCode();
            res.date = urlConn.getHeaderField("Date");

            // Read the input stream into a String
            InputStream inputStream;
            if (200 <= res.code && res.code <= 399) {
                inputStream = urlConn.getInputStream();
            } else {
                inputStream = urlConn.getErrorStream();
            }

            if (inputStream == null) {
                // Nothing to read.
                return res;
            }

            reader = new BufferedReader(new InputStreamReader(inputStream));

            StringBuffer buffer = new StringBuffer();
            String line;
            while ((line = reader.readLine()) != null) {
                buffer.append(line);
            }

            if (buffer.length() == 0) {
                // Stream was empty.
                return res;
            }

            res.body = buffer.toString();
        }
        catch (IOException e) {
            Log.e("PsiCashLibHelper", "httpRequest: failed with IOException ", e);
            res.error = "httpRequest: failed with IOException: " + e.toString();
            res.code = -1;
            res.body = null;
        }
        catch (RuntimeException e) {
            Log.e("PsiCashLibHelper", "httpRequest: failed with RuntimeException ", e);
            res.error = "httpRequest: failed with RuntimeException: " + e.toString();
            res.code = -1;
            res.body = null;
        }
        finally {
            if (urlConn != null) {
                urlConn.disconnect();
            }
            if (reader != null) {
                try {
                    reader.close();
                } catch (final IOException e) {
                    // Log this, but don't tell the library that the request failed
                    Log.e("PsiCashLibHelper", "httpRequest: Error closing request stream", e);
                }
            }
        }

        return res;
    }
}
