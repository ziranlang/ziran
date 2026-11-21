#include <string.h>
#include "json_string_scan_test.h"
int main(int argc, char **argv) {
    if (argc == 1) return NativeMain();
    if (!strcmp(argv[1], "InvalidNegative")) return InvalidNegative();
    if (!strcmp(argv[1], "InvalidEnd")) return InvalidEnd();
    if (!strcmp(argv[1], "InvalidLowEnd")) return InvalidLowEnd();
    if (!strcmp(argv[1], "InvalidWideEnd")) return InvalidWideEnd();
    return 2;
}
