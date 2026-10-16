#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(TEST_TEXT_STREAM_TMPFILE)
#define __plan9__ 1
#endif
#include "zir_stream.h"

static void empty_output(void)
{
    TextStream stream;
    char *text;
    size_t size, position;
    assert(TextStreamOpen(&stream, &text, &size) != NULL);
    assert(TextStreamPosition(&stream, &position) == 0 && position == 0);
    assert(TextStreamClose(&stream) == 0);
    assert(text != NULL && size == 0 && text[0] == 0);
    free(text);
}

static void binary_output(void)
{
    static const unsigned char prefix[] = {'a', 0, 0xc3, 0xa9, 'z'};
    TextStream stream;
    char *text;
    size_t size, position;
    FILE *file = TextStreamOpen(&stream, &text, &size);
    assert(file != NULL);
    assert(fwrite(prefix, 1, sizeof(prefix), file) == sizeof(prefix));
    assert(TextStreamPosition(&stream, &position) == 0 && position == sizeof(prefix));
    for(unsigned i = 0; i < 16384; i++) assert(fputc((i * 17) & 255, file) != EOF);
    assert(TextStreamPosition(&stream, &position) == 0 && position == sizeof(prefix) + 16384);
    assert(TextStreamClose(&stream) == 0);
    assert(size == position && text[size] == 0 && !memcmp(text, prefix, sizeof(prefix)));
    for(unsigned i = 0; i < 16384; i++)
        assert((unsigned char)text[sizeof(prefix) + i] == ((i * 17) & 255));
    free(text);
}

#if defined(__GLIBC__) && defined(ZIR_MEMORY_STREAMS)
/* Reproduce write-only funopen's lack of seeking on a glibc test host. */
static ssize_t cookie_write(void *context, const char *bytes, size_t size)
{
    TextStream *stream = context;
    unsigned char *next = realloc(stream->data, stream->length + size + 1);
    if(next == NULL) return 0;
    stream->data = next;
    memcpy(next + stream->length, bytes, size);
    stream->length += size;
    next[stream->length] = 0;
    return (ssize_t)size;
}

static ssize_t cookie_fail(void *context, const char *bytes, size_t size)
{
    (void)context; (void)bytes; (void)size;
    errno = ENOSPC;
    return 0;
}

static void write_only_output(void)
{
    char *text = NULL;
    size_t size = 0, position;
    TextStream stream = {.text = &text, .size = &size};
    stream.file = fopencookie(&stream, "w", (cookie_io_functions_t){.write = cookie_write});
    assert(stream.file != NULL && ftell(stream.file) == -1);
    clearerr(stream.file);
    assert(fwrite("a\0z", 1, 3, stream.file) == 3);
    assert(TextStreamPosition(&stream, &position) == 0 && position == 3);
    assert(TextStreamClose(&stream) == 0 && size == 3 && !memcmp(text, "a\0z", 4));
    free(text);
    text = NULL; size = 0;
    stream = (TextStream){.text = &text, .size = &size};
    stream.file = fopencookie(&stream, "w", (cookie_io_functions_t){.write = cookie_write});
    assert(stream.file != NULL && TextStreamClose(&stream) == 0);
    assert(text != NULL && size == 0 && text[0] == 0);
    free(text);
    text = NULL; size = 0;
    stream = (TextStream){.text = &text, .size = &size};
    stream.file = fopencookie(NULL, "w", (cookie_io_functions_t){.write = cookie_fail});
    assert(stream.file != NULL);
    assert(setvbuf(stream.file, NULL, _IONBF, 0) == 0);
    assert(fputs("full", stream.file) == EOF && ferror(stream.file));
    assert(TextStreamPosition(&stream, &position) == -1);
    assert(TextStreamClose(&stream) == -1 && text == NULL && size == 0);
}
#endif

int main(void)
{
    empty_output();
    binary_output();
#if defined(__GLIBC__) && defined(ZIR_MEMORY_STREAMS)
    write_only_output();
#endif
    puts("Text output streams preserve positions, bytes, empty output and failures");
    return 0;
}
