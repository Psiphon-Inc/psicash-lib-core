package ca.psiphon.psicashlib;

import org.junit.*;
import static org.junit.Assert.*;

public class InitTest extends TestBase {
    @Test
    public void simpleSuccess() {
        {
            PsiCashLibTester pcl = new PsiCashLibTester();
            PsiCashLib.Error err = pcl.init(getTempDir(), null);
            assertNull(err);
        }
        {
            PsiCashLibTester pcl = new PsiCashLibTester();
            PsiCashLib.Error err = pcl.init(getTempDir(), new PsiCashLibHelper());
            assertNull(err);
        }
    }

    @Test
    public void error() {
        {
            // Null file store root
            PsiCashLibTester pcl = new PsiCashLibTester();
            PsiCashLib.Error err = pcl.init(null, new PsiCashLibHelper());
            assertNotNull(err);
        }
        {
            // Bad file store root
            PsiCashLibTester pcl = new PsiCashLibTester();
            PsiCashLib.Error err = pcl.init("/a:%$*&/b/c/d/e/f", new PsiCashLibHelper());
            assertNotNull(err);
        }
        {
            // Make sure we can still succeed with good params
            PsiCashLibTester pcl = new PsiCashLibTester();
            PsiCashLib.Error err = pcl.init(getTempDir(), new PsiCashLibHelper());
            assertNull(err);
        }
    }
}
