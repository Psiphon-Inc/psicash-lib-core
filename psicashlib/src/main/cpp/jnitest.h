
#ifndef PSICASHLIB_JNITEST_H
#define PSICASHLIB_JNITEST_H

#include "psicash.h"

class PsiCashTest : public psicash::PsiCash {
public:
    error::Error TestReward(const std::string& transaction_class, const std::string& distinguisher);
};

#endif //PSICASHLIB_JNITEST_H
