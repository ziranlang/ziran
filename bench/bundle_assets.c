#include "ziran_host.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double milliseconds(clockid_t clock)
{
    struct timespec now;
    assert(clock_gettime(clock, &now) == 0);
    return now.tv_sec * 1000.0 + now.tv_nsec / 1000000.0;
}

int main(int argc, char **argv)
{
    assert(argc == 3 || (argc == 4 && strcmp(argv[3], "--file") == 0));
    int file_mode = argc == 4;
    int repetitions = atoi(argv[2]);
    assert(repetitions > 0);
    for(int run = 0; run < repetitions; run++) {
        double start = milliseconds(CLOCK_MONOTONIC);
        double cpu_start = milliseconds(CLOCK_THREAD_CPUTIME_ID);
        FILE *input = fopen(argv[1], "rb");
        assert(input && fseek(input, 0, SEEK_END) == 0);
        long length = ftell(input);
        assert(length > 0 && fseek(input, 0, SEEK_SET) == 0);
        unsigned char *data = malloc((size_t)length);
        assert(data && fread(data, 1, (size_t)length, input) == (size_t)length);
        assert(fclose(input) == 0);
        double read = milliseconds(CLOCK_MONOTONIC);
        double cpu_read = milliseconds(CLOCK_THREAD_CPUTIME_ID);
        Bundle *bundle = file_mode ? BundleOpen(argv[1]) :
            BundleOpenBytes(data, (size_t)length);
        double opened = milliseconds(CLOCK_MONOTONIC);
        double cpu_opened = milliseconds(CLOCK_THREAD_CPUTIME_ID);
        assert(bundle);
        free(data);
        /* All resource bytes must survive releasing the caller's input. */
        size_t assets = BundleAssetCount(bundle), bytes = 0;
        uint64_t checksum = 14695981039346656037ULL;
        for(size_t i = 0; i < assets; i++) {
            const char *name = BundleAssetName(bundle, i);
            const unsigned char *content = BundleAssetData(bundle, i);
            size_t size = BundleAssetSize(bundle, i);
            assert(name && content);
            for(const char *p = name; *p; p++)
                checksum = (checksum ^ (unsigned char)*p) * 1099511628211ULL;
            for(size_t j = 0; j < size; j++)
                checksum = (checksum ^ content[j]) * 1099511628211ULL;
            bytes += size;
        }
        double inspected = milliseconds(CLOCK_MONOTONIC);
        double cpu_inspected = milliseconds(CLOCK_THREAD_CPUTIME_ID);
        BundleClose(bundle);
        double closed = milliseconds(CLOCK_MONOTONIC);
        double cpu_closed = milliseconds(CLOCK_THREAD_CPUTIME_ID);
        printf("{\"run\":%d,\"input_bytes\":%ld,\"assets\":%zu,\"asset_bytes\":%zu,"
               "\"checksum\":\"%llu\",\"read_ms\":%.6f,\"open_ms\":%.6f,"
               "\"inspect_ms\":%.6f,\"close_ms\":%.6f,\"read_cpu_ms\":%.6f,"
               "\"open_cpu_ms\":%.6f,\"inspect_cpu_ms\":%.6f,\"close_cpu_ms\":%.6f}\n",
               run, length, assets, bytes, (unsigned long long)checksum,
               read-start, opened-read, inspected-opened, closed-inspected,
               cpu_read-cpu_start, cpu_opened-cpu_read,
               cpu_inspected-cpu_opened, cpu_closed-cpu_inspected);
    }
    return 0;
}
