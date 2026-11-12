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

static void put_u32(unsigned char *bytes, uint32_t value)
{
    for(int index = 0; index < 4; index++) bytes[index] = (unsigned char)(value >> (index * 8));
}

static void executable(void)
{
    assert(compiler_bundle_ExecutableVersionNumber() == 28);
    unsigned char program_data[] = {'Z', 'L', 'I', 'R', 0, 1, 2, 3};
    unsigned char asset_data[] = {'a', 0, 'b', 255};
    unsigned char identities[96];
    for(size_t index = 0; index < sizeof(identities); index++) identities[index] = (unsigned char)index;
    Asset item = {text("data/sample.bin"), StringView((const char *)asset_data, sizeof(asset_data))};
    ExecutableAttachment attachment = {0x80000100u, text("optional metadata")};
    ExecutableHeader header = {
        .entry = 7,
        .program = StringView((const char *)program_data, sizeof(program_data)),
        .assets = slice(&item, 1),
        .program_identity = StringView((const char *)identities, 32),
        .assets_identity = StringView((const char *)identities + 32, 32),
        .content_identity = StringView((const char *)identities + 64, 32),
        .debug = text("line table"),
        .attachments = slice(&attachment, 1)
    };
    ExecutableResult measured = compiler_bundle_ExecutableEncode(slice(NULL, 0), header);
    assert(measured.problem == ExecutableProblem_None && measured.size > 300);
    unsigned char short_output[32];
    memset(short_output, 0xa5, sizeof(short_output));
    ExecutableResult short_result = compiler_bundle_ExecutableEncode(slice(short_output, sizeof(short_output)), header);
    assert(short_result.problem == ExecutableProblem_Output && short_result.size == measured.size);
    for(size_t index = 0; index < sizeof(short_output); index++) assert(short_output[index] == 0xa5);

    unsigned char *bytes = malloc((size_t)measured.size + 1);
    Asset decoded_asset = {0};
    assert(bytes);
    ExecutableResult written = compiler_bundle_ExecutableEncode(slice(bytes, (size_t)measured.size), header);
    assert(written.problem == ExecutableProblem_None && written.size == measured.size);
    ExecutableLayout layout = compiler_bundle_ExecutableDecode(
        StringView((const char *)bytes, (size_t)measured.size), slice(&decoded_asset, 1));
    assert(layout.problem == ExecutableProblem_None && layout.found_version == 28);
    assert(layout.entry == 7 && layout.assets == 1 && layout.optional_sections == 2);
    assert(layout.program.length == (int64_t)sizeof(program_data));
    assert(!memcmp(layout.program.data, program_data, sizeof(program_data)));
    assert(decoded_asset.data.length == (int64_t)sizeof(asset_data));
    assert(!memcmp(decoded_asset.data.data, asset_data, sizeof(asset_data)));
    assert(StringEqual(layout.debug, text("line table")));

    unsigned char *mutation = malloc((size_t)measured.size + 1);
    assert(mutation);
    memcpy(mutation, bytes, (size_t)measured.size);
    put_u32(mutation + 4, 26);
    layout = compiler_bundle_ExecutableDecode(StringView((const char *)mutation, (size_t)measured.size), slice(NULL, 0));
    assert(layout.problem == ExecutableProblem_Legacy);

    /* The final custom section is optional. Making it required must reject it
     * without interpreting its bytes. */
    memcpy(mutation, bytes, (size_t)measured.size);
    uint32_t sections = (uint32_t)mutation[32] | (uint32_t)mutation[33] << 8 |
        (uint32_t)mutation[34] << 16 | (uint32_t)mutation[35] << 24;
    assert(sections == 5);
    size_t final_directory = 64 + (sections - 1) * 32;
    put_u32(mutation + final_directory + 4, 1);
    layout = compiler_bundle_ExecutableDecode(StringView((const char *)mutation, (size_t)measured.size), slice(NULL, 0));
    assert(layout.problem == ExecutableProblem_RequiredSection);

    /* A required native-variant record is distinguished from an arbitrary
     * required extension and rejected before any payload can be selected. */
    memcpy(mutation, bytes, (size_t)measured.size);
    size_t debug_directory = 64 + 3 * 32;
    put_u32(mutation + debug_directory, 4);
    put_u32(mutation + debug_directory + 4, 1);
    layout = compiler_bundle_ExecutableDecode(StringView((const char *)mutation, (size_t)measured.size), slice(NULL, 0));
    assert(layout.problem == ExecutableProblem_Variant);

    memcpy(mutation, bytes, (size_t)measured.size);
    memset(mutation + final_directory + 8, 0, 8);
    layout = compiler_bundle_ExecutableDecode(StringView((const char *)mutation, (size_t)measured.size), slice(NULL, 0));
    assert(layout.problem == ExecutableProblem_Overlap);
    layout = compiler_bundle_ExecutableDecode(StringView((const char *)bytes, (size_t)measured.size - 1), slice(NULL, 0));
    assert(layout.problem == ExecutableProblem_Truncated);
    bytes[measured.size] = 0;
    layout = compiler_bundle_ExecutableDecode(StringView((const char *)bytes, (size_t)measured.size + 1), slice(NULL, 0));
    assert(layout.problem == ExecutableProblem_Trailing);

    String message = compiler_bundle_ExecutableMessage(ExecutableProblem_Legacy);
    assert(memmem(message.data, (size_t)message.length, "rebuild with zi2asm --emit zx", 28));
    free(mutation);
    free(bytes);
}

int main(void)
{
    assets();
    limits();
    executable();
    puts("compiler bundle boundary passed");
    return 0;
}
