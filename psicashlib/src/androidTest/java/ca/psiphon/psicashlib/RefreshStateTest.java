package ca.psiphon.psicashlib;

import android.support.test.runner.AndroidJUnit4;

import org.junit.*;
import org.junit.runner.RunWith;

import static org.junit.Assert.*;

@RunWith(AndroidJUnit4.class)
public class RefreshStateTest extends TestBase {
    @Test
    public void simpleSuccess() {
        PsiCashLibTester pcl = new PsiCashLibTester();
        PsiCashLib.Error err = pcl.init(getTempDir(), new PsiCashLibHelper());
        assertNull(err);

        assertFalse(pcl.isAccount());
        assertEquals(pcl.validTokenTypes().size(), 0);
        assertEquals(pcl.balance(), 0L);

        PsiCashLib.RefreshStateResult res = pcl.refreshState(null);
        assertNull(cond(res.error, "message"), res.error);
        assertEquals(res.status, PsiCashLib.Status.SUCCESS);
        assertFalse(pcl.isAccount());
        assertEquals(pcl.validTokenTypes().size(), 3);
        assertEquals(pcl.balance(), 0L);
    }

    @Test
    public void balanceChange() {
        PsiCashLibTester pcl = new PsiCashLibTester();
        PsiCashLib.Error err = pcl.init(getTempDir(), new PsiCashLibHelper());
        assertNull(err);

        PsiCashLib.RefreshStateResult res = pcl.refreshState(null);
        assertNull(cond(res.error, "message"), res.error);
        assertEquals(res.status, PsiCashLib.Status.SUCCESS);
        assertFalse(pcl.isAccount());
        assertEquals(pcl.validTokenTypes().size(), 3);
        assertEquals(pcl.balance(), 0L);

        err = pcl.testReward(1);
        assertNull(cond(err, "message"), err);

        res = pcl.refreshState(null);
        assertNull(cond(res.error, "message"), res.error);
        assertEquals(res.status, PsiCashLib.Status.SUCCESS);

        assertEquals(pcl.balance(), SecretTestValues.ONE_TRILLION);
    }
}
