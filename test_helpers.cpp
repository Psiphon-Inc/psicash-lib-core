#include "test_helpers.h"

bool InRange(int64_t target, int64_t low, int64_t high) {
    return (low <= target) && (target <= high);
}

bool IsNear(int64_t target, int64_t comparator, int64_t wiggle) {
    return InRange(target, comparator-wiggle, comparator+wiggle);
}
