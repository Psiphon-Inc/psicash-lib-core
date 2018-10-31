package ca.psiphon.psicashlib;

import android.support.test.InstrumentationRegistry;
import android.util.Log;

import org.junit.*;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.lang.reflect.Field;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.Map;
import java.util.Random;

public class TestBase {
    private static File testSubDir;

    @BeforeClass
    public static void makeTestDir() {
        String cacheDirPath = InstrumentationRegistry.getTargetContext().getCacheDir().toString();
        File tempDir = new File(cacheDirPath + File.separator + "test");
        // For some reason, calling mkdirs here fails. But succeeds in getTempDir. So don't bother here.
        TestBase.testSubDir = tempDir;
    }

    @AfterClass
    public static void removeTestDir() {
        if (TestBase.testSubDir != null) {
            deleteRecursive(TestBase.testSubDir);
            TestBase.testSubDir = null;
        }
    }

    private static void deleteRecursive(File fileOrDirectory) {
        if (fileOrDirectory.isDirectory())
            for (File child : fileOrDirectory.listFiles())
                deleteRecursive(child);

        fileOrDirectory.delete();
    }

    protected String getTempDir() {
        File tempDir = new File(this.testSubDir.toString() + File.separator + Math.abs(new Random().nextLong()));
        if (!tempDir.mkdirs()) {
            throw new RuntimeException("getTempDir: cannot create temp directory");
        }
        return tempDir.toString();
    }

    // Used to conditionally access a field on an object that may be null.
    // Like: assertNull(cond(res.error, "message"), res.error);
    private Object cond(Object nullable, String fieldName) {
        if (nullable == null) {
            return null;
        }
        try {
            Field field =  nullable.getClass().getField(fieldName);
            Object o = field.get(nullable);
            return o;
        } catch (NoSuchFieldException e) {
            return null;
        } catch (IllegalAccessException e) {
            return null;
        }
    }

    protected String conds(Object nullable, String fieldName) {
        Object o = cond(nullable, fieldName);
        if (o == null) {
            return "";
        }
        return (String)o;
    }

    protected String condi(Object nullable, String fieldName) {
        Object o = cond(nullable, fieldName);
        if (o == null) {
            return "";
        }
        return ((Integer)o).toString();
    }

    protected void sleep(int millis) {
        try {
            Thread.sleep(millis);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    // TODO: Don't duplicate this with the app (although maybe we don't need the app anymore).
    public class PsiCashLibHelper implements PsiCashLib.HTTPRequester {
        public PsiCashLib.HTTPRequester.Result httpRequest(PsiCashLib.HTTPRequester.ReqParams reqParams) {
            PsiCashLib.HTTPRequester.Result res = new PsiCashLib.HTTPRequester.Result();

            HttpURLConnection urlConn = null;
            BufferedReader reader = null;

            try {
                URL url = new URL(reqParams.uri.toString());

                urlConn = (HttpURLConnection)url.openConnection();
                urlConn.setConnectTimeout(2500);
                urlConn.setReadTimeout(2500);
                urlConn.setUseCaches(false);

                urlConn.setRequestMethod(reqParams.method);

                if (reqParams.headers != null) {
                    for (Map.Entry<String, String> h : reqParams.headers.entrySet()) {
                        urlConn.setRequestProperty(h.getKey(), h.getValue());
                    }
                }

                urlConn.connect();

                res.status = urlConn.getResponseCode();
                res.date = urlConn.getHeaderField("Date");

                // Read the input stream into a String
                InputStream inputStream;
                if (200 <= res.status && res.status <= 399) {
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
                res.status = -1;
                res.body = null;
            }
            catch (RuntimeException e) {
                Log.e("PsiCashLibHelper", "httpRequest: failed with RuntimeException ", e);
                res.error = "httpRequest: failed with RuntimeException: " + e.toString();
                res.status = -1;
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

}
