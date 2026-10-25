#include "gzip_linux.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    assert(argc == 5);
    FILE *file = fopen(argv[2], "rb");
    assert(file && fseek(file, 0, SEEK_END) == 0);
    long size = ftell(file);
    assert(size >= 0 && size <= 16*1024*1024 && fseek(file, 0, SEEK_SET) == 0);
    unsigned char *input = malloc((size_t)size+1);
    assert(input && fread(input, 1, size, file) == (size_t)size);
    fclose(file);
    int64_t expected = strtoll(argv[4], NULL, 10);
    int64_t capacity = strcmp(argv[1], "compress") == 0 ? GzipBound(size) : expected;
    assert(capacity >= 0 && capacity <= 32*1024*1024);
    unsigned char *output = malloc((size_t)capacity+1);
    assert(output);
    Slice source = {.data=input,.length=size}, destination = {.data=output,.length=capacity};
    int64_t count;
    if (strcmp(argv[1], "compress") == 0) {
        count = GzipCompress(source, destination, 1);
        assert(count >= 0);
    } else {
        if (!GzipDecompress(source, destination, expected)) {free(input);free(output);return 2;}
        count = expected;
    }
    file = fopen(argv[3], "wb");
    assert(file && fwrite(output, 1, count, file) == (size_t)count);
    fclose(file);
    free(input);
    free(output);
    return 0;
}
