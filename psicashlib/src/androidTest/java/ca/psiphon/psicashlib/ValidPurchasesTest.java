package ca.psiphon.psicashlib;

import org.junit.*;
import static ca.psiphon.psicashlib.SecretTestValues.*;
import static org.junit.Assert.*;

public class ValidPurchasesTest extends TestBase {
    @Test
    public void simpleSuccess() {
        PsiCashLibTester pcl = new PsiCashLibTester();
        PsiCashLib.Error err = pcl.init(getTempDir(), new PsiCashLibHelper());
        assertNull(err);

        // Default value, before the first RefreshState
        PsiCashLib.ValidPurchasesResult vpr = pcl.validPurchases();
        assertNull(vpr.error);
        assertEquals(0, vpr.purchases.size());

        // First RefreshState, which creates the tracker
        PsiCashLib.RefreshStateResult res = pcl.refreshState(null);
        assertNull(conds(res.error, "message"), res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);
        vpr = pcl.validPurchases();
        assertNull(vpr.error);
        assertEquals(0, vpr.purchases.size());

        // Second RefreshState, which just refreshes
        res = pcl.refreshState(null);
        assertNull(conds(res.error, "message"), res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);
        vpr = pcl.validPurchases();
        assertNull(vpr.error);
        assertEquals(0, vpr.purchases.size());
    }

    @Test
    public void withPurchases() {
        PsiCashLibTester pcl = new PsiCashLibTester();
        PsiCashLib.Error err = pcl.init(getTempDir(), new PsiCashLibHelper());
        assertNull(err);

        PsiCashLib.RefreshStateResult res = pcl.refreshState(null);
        assertNull(conds(res.error, "message"), res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);
        PsiCashLib.ValidPurchasesResult vpr = pcl.validPurchases();
        assertNull(vpr.error);
        assertEquals(0, vpr.purchases.size());

        err = pcl.testReward(3);
        assertNull(err);

        res = pcl.refreshState(null);
        assertNull(res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);

        // Make purchase (ten-second validity); may be brittle with clock skew
        PsiCashLib.NewExpiringPurchaseResult nepr = pcl.newExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_TEN_SECOND_DISTINGUISHER, ONE_TRILLION);
        assertNull(nepr.error);
        assertEquals(PsiCashLib.Status.SUCCESS, nepr.status);

        vpr = pcl.validPurchases();
        assertNull(vpr.error);
        assertEquals(1, vpr.purchases.size());

        // Make another purchase (one-second validity)
        nepr = pcl.newExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_ONE_SECOND_DISTINGUISHER, ONE_TRILLION);
        assertNull(nepr.error);
        assertEquals(PsiCashLib.Status.SUCCESS, nepr.status);

        vpr = pcl.validPurchases();
        assertNull(vpr.error);
        assertEquals(2, vpr.purchases.size());

        // Let those purchases expire (and expire them)
        sleep(15000);
        vpr = pcl.validPurchases();
        assertNull(vpr.error);
        assertEquals(0, vpr.purchases.size());
        PsiCashLib.ExpirePurchasesResult epr = pcl.expirePurchases();
        assertNull(epr.error);
        assertEquals(2, epr.purchases.size());
        vpr = pcl.validPurchases();
        assertNull(vpr.error);
        assertEquals(0, vpr.purchases.size());
    }
}
