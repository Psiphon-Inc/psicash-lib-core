package ca.psiphon.psicashlib;


import org.junit.*;

import java.util.Arrays;

import static ca.psiphon.psicashlib.SecretTestValues.TEST_DEBIT_TRANSACTION_CLASS;
import static org.junit.Assert.*;

public class RefreshStateTest extends TestBase {
    @Test
    public void simpleSuccess() {
        PsiCashLibTester pcl = new PsiCashLibTester();
        PsiCashLib.Error err = pcl.init(getTempDir(), new PsiCashLibHelper());
        assertNull(err);

        // Before first call, so default values
        PsiCashLib.IsAccountResult iar = pcl.isAccount();
        assertNull(iar.error);
        assertFalse(iar.isAccount);
        PsiCashLib.ValidTokenTypesResult vttr = pcl.validTokenTypes();
        assertNull(vttr.error);
        assertEquals(0, vttr.validTokenTypes.size());
        PsiCashLib.BalanceResult br = pcl.balance();
        assertNull(br.error);
        assertEquals(0L, br.balance);
        PsiCashLib.GetPurchasePricesResult gppr = pcl.getPurchasePrices();
        assertEquals(0, gppr.purchasePrices.size());

        // First call, which gets tokens
        PsiCashLib.RefreshStateResult res = pcl.refreshState(null);
        assertNull(conds(res.error, "message"), res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);
        iar = pcl.isAccount();
        assertNull(iar.error);
        assertFalse(iar.isAccount);
        vttr = pcl.validTokenTypes();
        assertNull(vttr.error);
        assertEquals(3, vttr.validTokenTypes.size());
        br = pcl.balance();
        assertNull(br.error);
        assertEquals(0L, br.balance);

        // Second call, which just refreshes
        res = pcl.refreshState(null);
        assertNull(conds(res.error, "message"), res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);
        iar = pcl.isAccount();
        assertNull(iar.error);
        assertFalse(iar.isAccount);
        vttr = pcl.validTokenTypes();
        assertNull(vttr.error);
        assertEquals(3, vttr.validTokenTypes.size());
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
        assertNull(conds(res.error, "message"), res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);
        PsiCashLib.IsAccountResult iar = pcl.isAccount();
        assertNull(iar.error);
        assertFalse(iar.isAccount);
        PsiCashLib.ValidTokenTypesResult vttr = pcl.validTokenTypes();
        assertNull(vttr.error);
        assertEquals(3, vttr.validTokenTypes.size());
        PsiCashLib.BalanceResult br = pcl.balance();
        assertNull(br.error);
        assertEquals(0L, br.balance);

        err = pcl.testReward(1);
        assertNull(conds(err, "message"), err);

        res = pcl.refreshState(null);
        assertNull(conds(res.error, "message"), res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);

        br = pcl.balance();
        assertNull(br.error);
        assertEquals(SecretTestValues.ONE_TRILLION, br.balance);
    }

    @Test
    public void withPurchaseClasses() {
        PsiCashLibTester pcl = new PsiCashLibTester();
        PsiCashLib.Error err = pcl.init(getTempDir(), new PsiCashLibHelper());
        assertNull(err);

        PsiCashLib.GetPurchasePricesResult gppr = pcl.getPurchasePrices();
        assertEquals(0, gppr.purchasePrices.size());

        PsiCashLib.RefreshStateResult res = pcl.refreshState(Arrays.asList("speed-boost", TEST_DEBIT_TRANSACTION_CLASS));
        assertNull(conds(res.error, "message"), res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);

        gppr = pcl.getPurchasePrices();
        assertNotEquals(0, gppr.purchasePrices.size());
    }

    @Test
    public void serverErrors() {
        PsiCashLibTester pcl = new PsiCashLibTester();
        PsiCashLib.Error err = pcl.init(getTempDir(), new PsiCashLibHelper());
        assertNull(err);

        pcl.setRequestMutator("Timeout:11");
        PsiCashLib.RefreshStateResult res = pcl.refreshState(null);
        assertNotNull(res.error);
        assertTrue(res.error.message, res.error.message.contains("timeout"));

        pcl.setRequestMutator("Response:code=666");
        res = pcl.refreshState(null);
        assertNotNull(res.error);
        assertTrue(res.error.message, res.error.message.contains("666"));

        pcl.setRequestMutators(Arrays.asList("Response:code=500", "Response:code=500", "Response:code=500"));
        res = pcl.refreshState(null);
        assertNull(res.error);
        assertEquals(PsiCashLib.Status.SERVER_ERROR, res.status);
    }
}
