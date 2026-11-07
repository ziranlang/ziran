#include "zir_string.h"
#include "compiler_bundle.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static String text(const char *source)
{
    return StringView(source, strlen(source));
}

static Slice slice(void *data, size_t length)
{
    return (Slice){data, (int64_t)length};
}

/* Thousands of assets round trip, and decoded views borrow the bundle. */
static void assets(void)
{
    enum { count = 4096 };
    Asset *items = calloc(count, sizeof(*items));
    char (*names)[32] = calloc(count, sizeof(*names));
    unsigned char *data = malloc(count * 257);
    assert(items && names && data);
    for(size_t i = 0; i < count; i++) {
        snprintf(names[i], sizeof(names[i]), "files/%05zu.bin", i);
        for(size_t j = 0; j < i % 257; j++) data[i * 257 + j] = (unsigned char)(i + j);
        items[i] = (Asset){text(names[i]), StringView((const char *)data + i * 257, i % 257)};
    }
    int64_t size = compiler_bundle_EncodeAssets(slice(NULL, 0), slice(items, count));
    assert(size > 0);
    Header header = {false, text("app"), text("main"), slice(NULL, 0), slice(NULL, 0), slice(NULL, 0), 1};
    int64_t head = compiler_bundle_EncodeHeader(slice(NULL, 0), header);
    assert(head == 39);
    unsigned char *bytes = malloc((size_t)(head + 1 + size));
    assert(bytes);
    assert(compiler_bundle_EncodeHeader(slice(bytes, (size_t)head), header) == head);
    bytes[head] = 7;
    assert(compiler_bundle_EncodeAssets(slice(bytes + head + 1, (size_t)size), slice(items, count)) == size);

    Asset *decoded = calloc(count, sizeof(*decoded));
    assert(decoded);
    String bundle = StringView((const char *)bytes, (size_t)(head + 1 + size));
    Layout layout = compiler_bundle_Decode(bundle, slice(NULL, 0), slice(NULL, 0), slice(NULL, 0),
        slice(decoded, count));
    assert(layout.problem == Problem_None && layout.assets == count);
    assert(layout.program == head && layout.program_end == head + 1);
    assert(layout.entry_module.data == (const char *)bytes + 12);
    for(size_t i = 0; i < count; i++) {
        assert(StringEqual(decoded[i].name, items[i].name));
        assert(decoded[i].data.length == items[i].data.length);
        assert(!memcmp(decoded[i].data.data, items[i].data.data, (size_t)items[i].data.length));
        assert(decoded[i].name.data > (const char *)bytes &&
            decoded[i].data.data + decoded[i].data.length <= bundle.data + bundle.length);
    }
    assert(compiler_bundle_ProgramEnd(bundle) == head + 1);
    free(decoded);
    free(bytes);
    free(data);
    free(names);
    free(items);
}

/* The table limits hold for the encoder as for the reader. */
static void limits(void)
{
    Capability *capabilities = calloc(4097, sizeof(*capabilities));
    assert(capabilities);
    for(size_t i = 0; i < 4097; i++) capabilities[i] = (Capability){text("host"), text("Call")};
    Header header = {false, text("m"), text("f"), slice(capabilities, 4096), slice(NULL, 0), slice(NULL, 0), 1};
    assert(compiler_bundle_EncodeHeader(slice(NULL, 0), header) == 34 + 4096 * 16);
    header.capabilities = slice(capabilities, 4097);
    assert(compiler_bundle_EncodeHeader(slice(NULL, 0), header) == -1);
    free(capabilities);

    /* A short output is measured but never written. */
    unsigned char small[33];
    memset(small, 0xaa, sizeof(small));
    header.capabilities = slice(NULL, 0);
    assert(compiler_bundle_EncodeHeader(slice(small, sizeof(small)), header) == 34);
    for(size_t i = 0; i < sizeof(small); i++) assert(small[i] == 0xaa);
    String message = compiler_bundle_Message(Problem_Version);
    assert(StringEqual(message, text("unsupported ZX version")));
}

int main(void)
{
    assets();
    limits();
    puts("compiler bundle boundary passed");
    return 0;
}
