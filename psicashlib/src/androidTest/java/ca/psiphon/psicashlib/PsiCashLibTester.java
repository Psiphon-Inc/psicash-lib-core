package ca.psiphon.psicashlib;

public class PsiCashLibTester extends PsiCashLib {
    @Override
    public Error init(String fileStoreRoot, HTTPRequester httpRequester) {
        return init(fileStoreRoot, httpRequester, true);
    }

}
