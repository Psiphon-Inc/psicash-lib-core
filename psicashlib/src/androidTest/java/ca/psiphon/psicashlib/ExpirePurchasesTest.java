package ca.psiphon.psicashlib;

import org.junit.*;
import static ca.psiphon.psicashlib.SecretTestValues.*;
import static org.junit.Assert.*;

public class ExpirePurchasesTest extends TestBase {
    @Test
    public void simpleSuccess() {
        PsiCashLibTester pcl = new PsiCashLibTester();
        PsiCashLib.Error err = pcl.init(getTempDir(), new PsiCashLibHelper());
        assertNull(err);

        // Default value, before the first RefreshState
        PsiCashLib.ExpirePurchasesResult epr = pcl.expirePurchases();
        assertNull(conds(epr.error, "message"), epr.error);
        assertEquals(0, epr.purchases.size());

        // First RefreshState, which creates the tracker
        PsiCashLib.RefreshStateResult res = pcl.refreshState(null);
        assertNull(conds(res.error, "message"), res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);
        epr = pcl.expirePurchases();
        assertNull(epr.error);
        assertEquals(0, epr.purchases.size());

        // Second RefreshState, which just refreshes
        res = pcl.refreshState(null);
        assertNull(conds(res.error, "message"), res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);
        epr = pcl.expirePurchases();
        assertNull(epr.error);
        assertEquals(0, epr.purchases.size());
    }

    @Test
    public void withPurchases() {
        PsiCashLibTester pcl = new PsiCashLibTester();
        PsiCashLib.Error err = pcl.init(getTempDir(), new PsiCashLibHelper());
        assertNull(err);

        PsiCashLib.RefreshStateResult res = pcl.refreshState(null);
        assertNull(conds(res.error, "message"), res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);
        PsiCashLib.ExpirePurchasesResult epr = pcl.expirePurchases();
        assertNull(conds(epr.error, "message"), epr.error);
        assertEquals(0, epr.purchases.size());

        err = pcl.testReward(3);
        assertNull(err);

        res = pcl.refreshState(null);
        assertNull(res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);

        // Make three purchases: one that expires immediately, and two that don't.
        // Too vulnerable to clock skew?
        PsiCashLib.NewExpiringPurchaseResult newExprPurchRes = pcl.newExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_ONE_MICROSECOND_DISTINGUISHER, ONE_TRILLION);
        assertNull(newExprPurchRes.error);
        assertEquals(PsiCashLib.Status.SUCCESS, newExprPurchRes.status);
        newExprPurchRes = pcl.newExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_ONE_SECOND_DISTINGUISHER, ONE_TRILLION);
        assertNull(newExprPurchRes.error);
        assertEquals(PsiCashLib.Status.SUCCESS, newExprPurchRes.status);
        newExprPurchRes = pcl.newExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_TEN_SECOND_DISTINGUISHER, ONE_TRILLION);
        assertNull(newExprPurchRes.error);
        assertEquals(PsiCashLib.Status.SUCCESS, newExprPurchRes.status);

        // Only the short purchase should have expired
        epr = pcl.expirePurchases();
        assertNull(epr.error);
        assertEquals(1, epr.purchases.size());
        assertNotEquals(0, epr.purchases.get(0).id.length());
        assertEquals(TEST_DEBIT_TRANSACTION_CLASS, epr.purchases.get(0).transactionClass);
        assertEquals(TEST_ONE_TRILLION_ONE_MICROSECOND_DISTINGUISHER, epr.purchases.get(0).distinguisher);
        assertNotNull(epr.purchases.get(0).expiry);
        assertNull(epr.purchases.get(0).authorization);

        // Let both long-expiry purchases expire
        sleep(15000);
        epr = pcl.expirePurchases();
        assertNull(epr.error);
        assertEquals(2, epr.purchases.size());
        assertNotEquals(0, epr.purchases.get(0).id.length());
        assertNotEquals(0, epr.purchases.get(1).id.length());
        assertEquals(TEST_DEBIT_TRANSACTION_CLASS, epr.purchases.get(0).transactionClass);
        assertEquals(TEST_DEBIT_TRANSACTION_CLASS, epr.purchases.get(1).transactionClass);
        assertTrue((TEST_ONE_TRILLION_ONE_SECOND_DISTINGUISHER.equals(epr.purchases.get(0).distinguisher)) || (TEST_ONE_TRILLION_TEN_SECOND_DISTINGUISHER.equals(epr.purchases.get(0).distinguisher)));
        assertTrue((TEST_ONE_TRILLION_ONE_SECOND_DISTINGUISHER.equals(epr.purchases.get(1).distinguisher)) || (TEST_ONE_TRILLION_TEN_SECOND_DISTINGUISHER.equals(epr.purchases.get(1).distinguisher)));
        assertNotNull(epr.purchases.get(0).expiry);
        assertNotNull(epr.purchases.get(1).expiry);
        assertNull(epr.purchases.get(0).authorization);
        assertNull(epr.purchases.get(1).authorization);
    }
}
