#include "net_ws_curl_linux.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) {
    assert(argc == 2);
    char output[256] = {0};
    int32_t status = 0;
    String url = StringView(argv[1], strlen(argv[1]));
    assert(ReceiveWebSocketCurl(NULL, url, StringLiteral("test-token"),
        (Slice){output, sizeof output}, &status));
    assert(status == 200 && strcmp(output, "{\"hello\":1}") == 0);
    assert(!ReceiveWebSocketCurl(NULL, url, StringLiteral("wrong"),
        (Slice){output, sizeof output}, &status) && status == 401);
    puts("Ziran WebSocket transport passed");
    return 0;
}
