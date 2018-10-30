package ca.psiphon.psicashlib;

import org.junit.*;
import static org.junit.Assert.*;

public class SetRequestMetadataItemTest extends TestBase {
    @Test
    public void simpleSuccess() {
        PsiCashLibTester pcl = new PsiCashLibTester();
        PsiCashLib.Error err = pcl.init(getTempDir(), null);
        assertNull(err);

        err = pcl.setRequestMetadataItem("mykey", "myval");
        assertNull(err);

        // Same again
        err = pcl.setRequestMetadataItem("mykey", "myval");
        assertNull(err);

        // New one
        err = pcl.setRequestMetadataItem("mykey2", "myval2");
        assertNull(err);
    }
}
