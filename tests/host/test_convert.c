#include "et.h"
#include <string.h>
#include "../../lib/convert.h"

void setup(void) {}
void teardown(void) {}

TEST_GROUP("Integer conversion") {
    TEST("negative integer conversion") {
        char buffer[16];

        VERIFY(itos(-123, buffer, sizeof buffer) == 0);
        VERIFY(strcmp("-123", buffer) == 0);
    }

    TEST("insufficient capacity") {
        char buffer[3];

        VERIFY(utos(123, buffer, sizeof buffer) == 1);
    }
}
