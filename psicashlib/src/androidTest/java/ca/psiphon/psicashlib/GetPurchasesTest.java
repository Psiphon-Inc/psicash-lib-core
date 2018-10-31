package ca.psiphon.psicashlib;

import org.junit.*;
import static ca.psiphon.psicashlib.SecretTestValues.*;
import static org.junit.Assert.*;

public class GetPurchasesTest extends TestBase {
    @Test
    public void simpleSuccess() {
        PsiCashLibTester pcl = new PsiCashLibTester();
        PsiCashLib.Error err = pcl.init(getTempDir(), new PsiCashLibHelper());
        assertNull(err);

        // Default value, before the first RefreshState
        PsiCashLib.GetPurchasesResult gpr = pcl.getPurchases();
        assertNull(gpr.error);
        assertEquals(0, gpr.purchases.size());

        // First RefreshState, which creates the tracker
        PsiCashLib.RefreshStateResult res = pcl.refreshState(null);
        assertNull(conds(res.error, "message"), res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);
        gpr = pcl.getPurchases();
        assertNull(gpr.error);
        assertEquals(0, gpr.purchases.size());

        // Second RefreshState, which just refreshes
        res = pcl.refreshState(null);
        assertNull(conds(res.error, "message"), res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);
        gpr = pcl.getPurchases();
        assertNull(gpr.error);
        assertEquals(0, gpr.purchases.size());
    }

    @Test
    public void withPurchases() {
        PsiCashLibTester pcl = new PsiCashLibTester();
        PsiCashLib.Error err = pcl.init(getTempDir(), new PsiCashLibHelper());
        assertNull(err);

        PsiCashLib.RefreshStateResult res = pcl.refreshState(null);
        assertNull(conds(res.error, "message"), res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);
        PsiCashLib.GetPurchasesResult gpr = pcl.getPurchases();
        assertNull(gpr.error);
        assertEquals(0, gpr.purchases.size());

        err = pcl.testReward(3);
        assertNull(err);

        res = pcl.refreshState(null);
        assertNull(res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);

        // Make purchase
        PsiCashLib.NewExpiringPurchaseResult nepr = pcl.newExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_ONE_MICROSECOND_DISTINGUISHER, ONE_TRILLION);
        assertNull(nepr.error);
        assertEquals(PsiCashLib.Status.SUCCESS, nepr.status);

        gpr = pcl.getPurchases();
        assertNull(gpr.error);
        assertEquals(1, gpr.purchases.size());

        // Make two more purchases
        nepr = pcl.newExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_ONE_MICROSECOND_DISTINGUISHER, ONE_TRILLION);
        assertNull(nepr.error);
        assertEquals(PsiCashLib.Status.SUCCESS, nepr.status);
        nepr = pcl.newExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_ONE_MICROSECOND_DISTINGUISHER, ONE_TRILLION);
        assertNull(nepr.error);
        assertEquals(PsiCashLib.Status.SUCCESS, nepr.status);

        gpr = pcl.getPurchases();
        assertNull(gpr.error);
        assertEquals(3, gpr.purchases.size());

        // Expire those purchases
        sleep(1000);
        PsiCashLib.ExpirePurchasesResult epr = pcl.expirePurchases();
        assertNull(epr.error);
        assertEquals(3, epr.purchases.size());
        gpr = pcl.getPurchases();
        assertNull(gpr.error);
        assertEquals(0, gpr.purchases.size());
    }
}
