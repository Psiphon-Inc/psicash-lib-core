package ca.psiphon.psicashlib;

import org.junit.*;

import static ca.psiphon.psicashlib.SecretTestValues.*;
import static org.junit.Assert.*;

public class BalanceTest extends TestBase {
    @Test
    public void simpleSuccess() {
        PsiCashLibTester pcl = new PsiCashLibTester();
        PsiCashLib.Error err = pcl.init(getTempDir(), new PsiCashLibHelper());
        assertNull(err);

        // Default value, before the first RefreshState
        PsiCashLib.BalanceResult br = pcl.balance();
        assertNull(br.error);
        assertEquals(0L, br.balance);

        // First RefreshState, which creates the tracker
        PsiCashLib.RefreshStateResult res = pcl.refreshState(null);
        assertNull(res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);
        br = pcl.balance();
        assertNull(br.error);
        assertEquals(0L, br.balance);

        // Second RefreshState, which just refreshes
        res = pcl.refreshState(null);
        assertNull(res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);
        br = pcl.balance();
        assertNull(br.error);
        assertEquals(0L, br.balance);

        // Another access, without a RefreshState
        br = pcl.balance();
        assertNull(br.error);
        assertEquals(0L, br.balance);
    }

    @Test
    public void balanceChange() {
        PsiCashLibTester pcl = new PsiCashLibTester();
        PsiCashLib.Error err = pcl.init(getTempDir(), new PsiCashLibHelper());
        assertNull(err);

        PsiCashLib.RefreshStateResult res = pcl.refreshState(null);
        assertNull(res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);

        // Start with 0
        PsiCashLib.BalanceResult br = pcl.balance();
        assertNull(br.error);
        assertEquals(0L, br.balance);

        // Get a reward
        err = pcl.testReward(1);
        assertNull(conds(err, "message"), err);
        // ...and refresh
        res = pcl.refreshState(null);
        assertNull(res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);
        // ...and have a bigger balance
        br = pcl.balance();
        assertNull(br.error);
        assertEquals(SecretTestValues.ONE_TRILLION, br.balance);

        // Spend the balance
        PsiCashLib.NewExpiringPurchaseResult nepr = pcl.newExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_ONE_MICROSECOND_DISTINGUISHER, ONE_TRILLION);
        assertNull(nepr.error);
        // ...the balance should have already updated
        br = pcl.balance();
        assertNull(br.error);
        assertEquals(0, br.balance);
        // ...also try refreshing
        res = pcl.refreshState(null);
        assertNull(res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);
        br = pcl.balance();
        assertNull(br.error);
        assertEquals(0, br.balance);
    }
}
