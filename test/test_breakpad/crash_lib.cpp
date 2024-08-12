#include "crash_lib.h"

void testCrash() {
    int *a = nullptr;
    a[0]   = 1;
}