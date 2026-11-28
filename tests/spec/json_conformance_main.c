#include "json_conformance_test.h"
int main(void) {
#ifdef JSON_PYTHON_ONLY
    return JsonPython();
#else
    return JsonNative();
#endif
}
