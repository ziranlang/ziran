#include "net_http_curl_linux.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static String
url(char *buffer, size_t capacity, const char *base, const char *path)
{
    int length = snprintf(buffer, capacity, "%s%s", base, path);
    assert(length > 0 && (size_t)length < capacity);
    return StringView(buffer, (size_t)length);
}

int
main(int argc, char **argv)
{
    assert(argc == 2);
    char address[256];
    char output[256];
    Slice buffer = {.data = output, .length = sizeof output};
    HttpRequest request = {0};
    request.method = StringLiteral("GET");
    request.url = url(address, sizeof address, argv[1], "/ok");
    CurlResult result = SendCurl(request, buffer);
    assert(result.code == 0 && result.status == 200);
    assert(result.length == 5 && strcmp(output, "ready") == 0);

    request.url = url(address, sizeof address, argv[1], "/gzip");
    result = SendCurl(request, buffer);
    assert(result.code == 0 && result.status == 200);
    assert(result.length == 5 && strcmp(output, "ready") == 0);

    request.method = StringLiteral("POST");
    request.url = url(address, sizeof address, argv[1], "/echo");
    request.token = StringLiteral("secret");
    request.accept = StringLiteral("application/json");
    request.content_type = StringLiteral("application/json");
    request.body = StringLiteral("{\"value\":42}");
    char signature[4841];
    memset(signature, 'b', sizeof signature - 1);
    signature[sizeof signature - 1] = 0;
    HttpHeader extra[] = {
        {
            .name = StringLiteral("X-Daochi-User"),
            .value = StringLiteral("account"),
        },
        {
            .name = StringLiteral("X-Daochi-Signature"),
            .value = StringView(signature, sizeof signature - 1),
        },
    };
    request.headers = (Slice){.data = extra, .length = 2};
    result = SendCurl(request, buffer);
    assert(result.code == 0 && result.status == 201);
    assert(strcmp(output, "accepted") == 0);

    request.method = StringLiteral("GET");
    request.url = url(address, sizeof address, argv[1], "/auth");
    request.token = StringLiteral("");
    request.accept = StringLiteral("");
    request.content_type = StringLiteral("");
    request.body = StringLiteral("");
    request.headers = (Slice){0};
    result = SendCurl(request, buffer);
    assert(result.code == 0 && result.status == 401);
    assert(strcmp(output, "denied") == 0);

    request.url = url(address, sizeof address, argv[1], "/large");
    buffer.length = 8;
    result = SendCurl(request, buffer);
    assert(result.code != 0 && result.truncated);
    assert(result.length == 0 && output[0] == 0);

    request.url = url(address, sizeof address, argv[1], "/gzip-large");
    result = SendCurl(request, buffer);
    assert(result.code != 0 && result.truncated);
    assert(result.length == 0 && output[0] == 0);

    request.url = url(address, sizeof address, argv[1], "/ok");
    request.token = StringLiteral("bad\r\nX-Injected: yes");
    result = SendCurl(request, buffer);
    assert(result.code == -1 && result.status == 0);
    assert(output[0] == 0);
    request.token = StringLiteral("");
    request.method = StringLiteral("GET\r\nBAD");
    result = SendCurl(request, buffer);
    assert(result.code == -1 && result.status == 0);
    request.method = StringLiteral("GET");
    request.url = StringLiteral("file:///etc/passwd");
    result = SendCurl(request, buffer);
    assert(result.code == -1 && result.status == 0);
    puts("Ziran curl HTTP request and bounded response passed");
    return 0;
}
