package ca.psiphon.psicashlib;

import org.junit.*;

import static ca.psiphon.psicashlib.SecretTestValues.*;
import static org.junit.Assert.*;

public class NextExpiringPurchaseTest extends TestBase {
    @Test
    public void simpleSuccess() {
        PsiCashLibTester pcl = new PsiCashLibTester();
        PsiCashLib.Error err = pcl.init(getTempDir(), new PsiCashLibHelper());
        assertNull(err);

        // Default value, before the first RefreshState
        PsiCashLib.NextExpiringPurchaseResult nepr = pcl.nextExpiringPurchase();
        assertNull(conds(nepr.error, "message"), nepr.error);
        assertNull(nepr.purchase);

        // First RefreshState, which creates the tracker
        PsiCashLib.RefreshStateResult res = pcl.refreshState(null);
        assertNull(conds(res.error, "message"), res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);
        nepr = pcl.nextExpiringPurchase();
        assertNull(nepr.error);
        assertNull(nepr.purchase);

        // Second RefreshState, which just refreshes
        res = pcl.refreshState(null);
        assertNull(conds(res.error, "message"), res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);
        nepr = pcl.nextExpiringPurchase();
        assertNull(nepr.error);
        assertNull(nepr.purchase);
    }

    @Test
    public void withPurchases() {
        PsiCashLibTester pcl = new PsiCashLibTester();
        PsiCashLib.Error err = pcl.init(getTempDir(), new PsiCashLibHelper());
        assertNull(err);

        PsiCashLib.RefreshStateResult res = pcl.refreshState(null);
        assertNull(conds(res.error, "message"), res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);
        PsiCashLib.NextExpiringPurchaseResult nepr = pcl.nextExpiringPurchase();
        assertNull(conds(nepr.error, "message"), nepr.error);
        assertNull(nepr.purchase);

        err = pcl.testReward(3);
        assertNull(err);

        res = pcl.refreshState(null);
        assertNull(res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);

        // Make purchase with one-second validity
        PsiCashLib.NewExpiringPurchaseResult newExprPurchRes = pcl.newExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_ONE_SECOND_DISTINGUISHER, ONE_TRILLION);
        assertNull(newExprPurchRes.error);
        assertEquals(PsiCashLib.Status.SUCCESS, newExprPurchRes.status);

        nepr = pcl.nextExpiringPurchase();
        assertNull(nepr.error);
        assertNotNull(nepr.purchase);
        assertNotEquals(0, nepr.purchase.id.length());
        assertEquals(TEST_DEBIT_TRANSACTION_CLASS, nepr.purchase.transactionClass);
        assertEquals(TEST_ONE_TRILLION_ONE_SECOND_DISTINGUISHER, nepr.purchase.distinguisher);
        assertNotNull(nepr.purchase.expiry);
        assertNull(nepr.purchase.authorization);

        // Make another purchase, with sub-one-second validity, so it expires first
        newExprPurchRes = pcl.newExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_ONE_MICROSECOND_DISTINGUISHER, ONE_TRILLION);
        assertNull(newExprPurchRes.error);
        assertEquals(PsiCashLib.Status.SUCCESS, newExprPurchRes.status);
        nepr = pcl.nextExpiringPurchase();
        assertNull(nepr.error);
        assertEquals(TEST_ONE_TRILLION_ONE_MICROSECOND_DISTINGUISHER, nepr.purchase.distinguisher);

        // Make yet another purchase, with 10-second validity (to make sure that
        // it's not the case that every new purchase becomes the "next expiring").
        newExprPurchRes = pcl.newExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_TEN_SECOND_DISTINGUISHER, ONE_TRILLION);
        assertNull(newExprPurchRes.error);
        assertEquals(newExprPurchRes.status.toString(), PsiCashLib.Status.SUCCESS, newExprPurchRes.status);
        nepr = pcl.nextExpiringPurchase();
        assertNull(nepr.error);
        assertEquals(TEST_ONE_TRILLION_ONE_MICROSECOND_DISTINGUISHER, nepr.purchase.distinguisher);

        // Let the two shorter purchases expire (and expire them)
        sleep(5000);
        nepr = pcl.nextExpiringPurchase();
        assertNull(nepr.error);
        assertEquals(TEST_ONE_TRILLION_ONE_MICROSECOND_DISTINGUISHER, nepr.purchase.distinguisher);
        PsiCashLib.ExpirePurchasesResult epr = pcl.expirePurchases();
        assertNull(epr.error);
        assertEquals(2, epr.purchases.size());
        nepr = pcl.nextExpiringPurchase();
        assertNull(nepr.error);
        assertEquals(TEST_ONE_TRILLION_TEN_SECOND_DISTINGUISHER, nepr.purchase.distinguisher);
    }
}
