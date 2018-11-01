package ca.psiphon.psicashlib;

import org.junit.*;

import static org.junit.Assert.*;

public class GetDiagnosticInfoTest extends TestBase {
    @Test
    public void simpleSuccess() {
        PsiCashLibTester pcl = new PsiCashLibTester();
        PsiCashLib.Error err = pcl.init(getTempDir(), new PsiCashLibHelper());
        assertNull(err);

        // Default value, before the first RefreshState
        PsiCashLib.GetDiagnosticInfoResult gdir = pcl.getDiagnosticInfo();
        assertNull(gdir.error);
        assertNotNull(gdir.jsonString);
        assertNotEquals(0, gdir.jsonString.length());
        String firstResult = gdir.jsonString;

        // First RefreshState, which creates the tracker
        PsiCashLib.RefreshStateResult res = pcl.refreshState(null);
        assertNull(conds(res.error, "message"), res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);
        gdir = pcl.getDiagnosticInfo();
        assertNull(gdir.error);
        assertNotNull(gdir.jsonString);
        assertNotEquals(0, gdir.jsonString.length());
        assertNotEquals(firstResult, gdir.jsonString);

        // Second RefreshState, which just refreshes
        res = pcl.refreshState(null);
        assertNull(conds(res.error, "message"), res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);
        gdir = pcl.getDiagnosticInfo();
        assertNull(gdir.error);
        assertNotNull(gdir.jsonString);
        assertNotEquals(0, gdir.jsonString.length());
    }
}
