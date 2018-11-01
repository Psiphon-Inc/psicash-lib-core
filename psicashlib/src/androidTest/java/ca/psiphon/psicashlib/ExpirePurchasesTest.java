package ca.psiphon.psicashlib;

import org.junit.*;

import static ca.psiphon.psicashlib.SecretTestValues.*;
import static org.hamcrest.Matchers.*;
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

        err = pcl.testReward(4);
        assertNull(err);

        res = pcl.refreshState(null);
        assertNull(res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);

        // Make four purchases: two that expire immediately, and two that don't.
        // Too vulnerable to clock skew?
        PsiCashLib.NewExpiringPurchaseResult newExprPurchRes = pcl.newExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_ONE_MICROSECOND_DISTINGUISHER, ONE_TRILLION);
        assertNull(newExprPurchRes.error);
        assertEquals(PsiCashLib.Status.SUCCESS, newExprPurchRes.status);
        newExprPurchRes = pcl.newExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_TEN_MICROSECOND_DISTINGUISHER, ONE_TRILLION);
        assertNull(newExprPurchRes.error);
        assertEquals(PsiCashLib.Status.SUCCESS, newExprPurchRes.status);
        newExprPurchRes = pcl.newExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_TEN_SECOND_DISTINGUISHER, ONE_TRILLION);
        assertNull(newExprPurchRes.error);
        assertEquals(PsiCashLib.Status.SUCCESS, newExprPurchRes.status);
        newExprPurchRes = pcl.newExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_ONE_MINUTE_DISTINGUISHER, ONE_TRILLION);
        assertNull(newExprPurchRes.error);
        assertEquals(PsiCashLib.Status.SUCCESS, newExprPurchRes.status);

        // Ensure (as best we can) that the short purchases are expired, but not the longer ones.
        sleep(2000);

        // Only the short purchases should have expired
        epr = pcl.expirePurchases();
        assertNull(epr.error);
        assertThat(epr.purchases.size(), is(2));
        assertThat(epr.purchases, containsPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_ONE_MICROSECOND_DISTINGUISHER));
        assertThat(epr.purchases, containsPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_TEN_MICROSECOND_DISTINGUISHER));
        assertFalse(epr.purchases.get(0).id.isEmpty());
        assertFalse(epr.purchases.get(1).id.isEmpty());
        assertNotNull(epr.purchases.get(0).expiry);
        assertNotNull(epr.purchases.get(1).expiry);
        assertNull(epr.purchases.get(0).authorization);
        assertNull(epr.purchases.get(1).authorization);

        // Let the next-shortest-expiry purchase expire
        sleep(15000);
        epr = pcl.expirePurchases();
        assertNull(epr.error);
        assertThat(epr.purchases.size(), is(1));
        assertThat(epr.purchases, containsPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_TEN_SECOND_DISTINGUISHER));
        assertFalse(epr.purchases.get(0).id.isEmpty());
        assertNotNull(epr.purchases.get(0).expiry);
        assertNull(epr.purchases.get(0).authorization);
    }
}
