#include "ziran_host.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    assert(argc == 5);
    Bundle *library = BundleOpen(argv[1]);
    assert(library != NULL);
    assert(BundleOpen(argv[2]) == NULL);
    Bundle *libraries[] = {library};
    Bundle *thin = BundleOpenWithLibraries(argv[2], libraries, 1);
    assert(thin != NULL);
    assert(BundleAssetCount(thin) == 1);
    assert(strcmp(BundleAssetName(thin, 0), "shared/message.txt") == 0);
    Bundle *duplicate[] = {library, library};
    assert(BundleOpenWithLibraries(argv[2], duplicate, 2) == NULL);
    Bundle *wrong = BundleOpen(argv[4]);
    assert(wrong != NULL);
    Bundle *wrong_libraries[] = {wrong};
    assert(BundleOpenWithLibraries(argv[2], wrong_libraries, 1) == NULL);
    BundleClose(wrong);
    BundleClose(library); /* The thin bundle retains its dependency. */
    BundleInstance *first = BundleInstantiate(thin, NULL, 0);
    BundleInstance *second = BundleInstantiate(thin, NULL, 0);
    assert(first != NULL && second != NULL);
    long long result;
    int has_result;
    assert(BundleInstanceRun(first, &result, &has_result) && result == 41);
    assert(BundleInstanceRun(first, &result, &has_result) && result == 42);
    assert(BundleInstanceRun(second, &result, &has_result) && result == 41);
    BundleInstanceClose(first);
    BundleInstanceClose(second);
    BundleClose(thin);
    Bundle *standalone = BundleOpen(argv[3]);
    assert(standalone != NULL);
    assert(BundleAssetCount(standalone) == 1);
    assert(BundleRun(standalone, NULL, 0, &result, &has_result) && result == 41);
    BundleClose(standalone);
    puts("Shared ZIB code, independent state, retained libraries and standalone execution passed");
    return 0;
}
