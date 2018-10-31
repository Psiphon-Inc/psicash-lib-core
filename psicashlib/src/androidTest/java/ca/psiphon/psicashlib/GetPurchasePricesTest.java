package ca.psiphon.psicashlib;

import org.junit.*;
import java.util.Arrays;

import static org.junit.Assert.*;

public class GetPurchasePricesTest extends TestBase {
    @Test
    public void simpleSuccess() {
        PsiCashLibTester pcl = new PsiCashLibTester();
        PsiCashLib.Error err = pcl.init(getTempDir(), new PsiCashLibHelper());
        assertNull(err);

        // Default value, before the first RefreshState
        PsiCashLib.GetPurchasePricesResult gppr = pcl.getPurchasePrices();
        assertNull(gppr.error);
        assertEquals(0, gppr.purchasePrices.size());

        // First RefreshState, which creates the tracker,
        // but no purchase classes.
        PsiCashLib.RefreshStateResult res = pcl.refreshState(null);
        assertNull(conds(res.error, "message"), res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);
        gppr = pcl.getPurchasePrices();
        assertNull(gppr.error);
        assertEquals(0, gppr.purchasePrices.size());

        // Second RefreshState, with purchase class
        res = pcl.refreshState(Arrays.asList("speed-boost"));
        assertNull(conds(res.error, "message"), res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);
        gppr = pcl.getPurchasePrices();
        assertNull(gppr.error);
        assertNotEquals(0, gppr.purchasePrices.size());
    }
}
