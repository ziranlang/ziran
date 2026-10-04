#ifndef ZIR_STREAM_H
#define ZIR_STREAM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#if defined(__ANDROID__) || defined(__APPLE__) || defined(__FreeBSD__)
/* funopen is available to Android API 21 hosts too, before fmemopen. */
typedef struct ZirMemoryReader {
    const unsigned char *data;
    size_t size;
    size_t offset;
} ZirMemoryReader;

static int
ZirMemoryRead(void *context, char *output, int count)
{
    ZirMemoryReader *reader = context;
    size_t amount = (size_t)count;
    if(amount > reader->size - reader->offset)
        amount = reader->size - reader->offset;
    memcpy(output, reader->data + reader->offset, amount);
    reader->offset += amount;
    return (int)amount;
}

static fpos_t
ZirMemorySeek(void *context, fpos_t position, int origin)
{
    ZirMemoryReader *reader = context;
    int64_t base = origin == SEEK_SET ? 0 : origin == SEEK_CUR ?
        (int64_t)reader->offset : origin == SEEK_END ? (int64_t)reader->size : -1;
    if(base < 0 || (position > 0 && base > INT64_MAX - position) ||
       (position < 0 && position < -base) ||
       (uint64_t)(base + position) > reader->size) {
        errno = EINVAL;
        return (fpos_t)-1;
    }
    reader->offset = (size_t)(base + position);
    return (fpos_t)reader->offset;
}

static int
ZirMemoryClose(void *context)
{
    free(context);
    return 0;
}
#endif

/* Read-only seekable stream. Unix, Android and Emscripten do not need
 * filesystem access or TMPDIR to load embedded bundles. */
static inline FILE *
ZirReadMemory(const unsigned char *data, size_t size)
{
#if defined(_WIN32) || defined(__plan9__)
    FILE *file = tmpfile();
    if(file == NULL)
        return NULL;
    if(fwrite(data, 1, size, file) != size || fflush(file) != 0 ||
       fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    return file;
#elif defined(__ANDROID__) || defined(__APPLE__) || defined(__FreeBSD__)
    ZirMemoryReader *reader = calloc(1, sizeof(*reader));
    if(reader == NULL)
        return NULL;
    reader->data = data;
    reader->size = size;
    FILE *file = funopen(reader, ZirMemoryRead, NULL, ZirMemorySeek, ZirMemoryClose);
    if(file == NULL)
        free(reader);
    return file;
#else
    return fmemopen((void *)data, size, "rb");
#endif
}

#if !defined(_WIN32) && !defined(__plan9__)
#define ZIR_MEMORY_STREAMS 1
#if defined(__ANDROID__) || defined(__APPLE__) || defined(__FreeBSD__)
typedef struct ZirMemoryWriter {
    unsigned char **data;
    size_t *size;
    size_t capacity;
} ZirMemoryWriter;

static int
ZirMemoryWrite(void *context, const char *input, int count)
{
    ZirMemoryWriter *writer = context;
    if(count < 0 || (size_t)count > SIZE_MAX - *writer->size - 1)
        return -1;
    size_t needed = *writer->size + (size_t)count + 1;
    if(needed > writer->capacity) {
        size_t capacity = needed <= SIZE_MAX / 2 ? needed * 2 : needed;
        unsigned char *data = realloc(*writer->data, capacity);
        if(data == NULL)
            return -1;
        *writer->data = data;
        writer->capacity = capacity;
    }
    memcpy(*writer->data + *writer->size, input, (size_t)count);
    *writer->size += (size_t)count;
    (*writer->data)[*writer->size] = 0;
    return count;
}
#endif

static inline FILE *
ZirWriteMemory(unsigned char **data, size_t *size)
{
    *data = NULL;
    *size = 0;
#if defined(__ANDROID__) || defined(__APPLE__) || defined(__FreeBSD__)
    ZirMemoryWriter *writer = calloc(1, sizeof(*writer));
    if(writer == NULL)
        return NULL;
    writer->data = data;
    writer->size = size;
    FILE *file = funopen(writer, NULL, ZirMemoryWrite, NULL, ZirMemoryClose);
    if(file == NULL)
        free(writer);
    return file;
#else
    return open_memstream((char **)data, size);
#endif
}
#endif

#endif
