package ca.psiphon.psicashlib;

import ca.psiphon.psicashlib.PsiCashLib.TokenType;
import org.junit.*;

import java.util.List;

import static org.junit.Assert.*;

public class ValidTokenTypesTest extends TestBase {
    @Test
    public void simpleSuccess() {
        PsiCashLibTester pcl = new PsiCashLibTester();
        PsiCashLib.Error err = pcl.init(getTempDir(), new PsiCashLibHelper());
        assertNull(err);

        // Default value, before the first RefreshState
        PsiCashLib.ValidTokenTypesResult vttr = pcl.validTokenTypes();
        assertNull(vttr.error);
        assertEquals(0, vttr.validTokenTypes.size());

        // First RefreshState, which creates the tracker
        PsiCashLib.RefreshStateResult res = pcl.refreshState(null);
        assertNull(conds(res.error, "message"), res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);
        vttr = pcl.validTokenTypes();
        assertNull(vttr.error);
        assertEquals(3, vttr.validTokenTypes.size());
        List<TokenType> firstVTT = vttr.validTokenTypes;

        // Second RefreshState, which just refreshes
        res = pcl.refreshState(null);
        assertNull(conds(res.error, "message"), res.error);
        assertEquals(PsiCashLib.Status.SUCCESS, res.status);
        vttr = pcl.validTokenTypes();
        assertNull(vttr.error);
        assertEquals(3, vttr.validTokenTypes.size());
        assertEquals(firstVTT, vttr.validTokenTypes);

        // Another access, without a RefreshState
        vttr = pcl.validTokenTypes();
        assertNull(vttr.error);
        assertEquals(3, vttr.validTokenTypes.size());
        assertEquals(firstVTT, vttr.validTokenTypes);
    }
}
