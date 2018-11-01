package ca.psiphon.psicashlib;

import org.junit.*;

import static org.junit.Assert.*;

public class ModifyLandingPageTest extends TestBase {
    @Test
    public void simpleSuccess() {
        PsiCashLibTester pcl = new PsiCashLibTester();
        PsiCashLib.Error err = pcl.init(getTempDir(), new PsiCashLibHelper());
        assertNull(err);

        String url = "http://example.com/a/b";

        // Even with no credentials, the call shouldn't fail
        PsiCashLib.ModifyLandingPageResult mlpr = pcl.modifyLandingPage(url);
        assertNull(mlpr.error);
        assertTrue(mlpr.url.startsWith(url));
        assertNotEquals(url, mlpr.url);

        // First RefreshState, which creates the tracker
        PsiCashLib.RefreshStateResult res = pcl.refreshState(null);
        assertNull(conds(res.error, "message"), res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);

        mlpr = pcl.modifyLandingPage(url);
        assertNull(mlpr.error);
        assertTrue(mlpr.url.startsWith(url));
        assertNotEquals(url, mlpr.url);

        // Second RefreshState, which just refreshes
        res = pcl.refreshState(null);
        assertNull(conds(res.error, "message"), res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);

        mlpr = pcl.modifyLandingPage(url);
        assertNull(mlpr.error);
        assertTrue(mlpr.url.startsWith(url));
        assertNotEquals(url, mlpr.url);
        String noMetadataUrl = url;

        // Set some metadata
        err = pcl.setRequestMetadataItem("mykey1", "myval1");
        assertNull(err);
        err = pcl.setRequestMetadataItem("mykey2", "myval2");
        assertNull(err);
        mlpr = pcl.modifyLandingPage(url);
        assertNull(mlpr.error);
        assertTrue(mlpr.url.startsWith(url));
        assertNotEquals(url, mlpr.url);
        assertNotEquals(noMetadataUrl, mlpr.url);
    }
}
