#include <stdint.h>
static int64_t mix(int64_t x, int64_t i) {
    return (i & 1) ? (x * 1103515245 + 12345) & 2147483647 :
        (x * 1664525 + 1013904223) & 2147483647;
}
int64_t Bench(int64_t seed, int64_t rounds, int64_t kind) {
    int64_t total = 0;
    if(kind == 0) {
        for(int64_t i = 0; i < rounds; i++) { seed = mix(seed, i); total += seed; }
    } else {
        struct Cell { int64_t x, width; int selected; } cells[32];
        const char *title = (seed & 1) ? "event" : "layout";
        int64_t length = 0; while(title[length]) length++;
        for(int j = 0; j < 32; j++) cells[j] = (struct Cell){seed & 255, j + 3, 0};
        for(int64_t i = 0; i < rounds; i++) {
            int index = i & 31;
            struct Cell cell = cells[index];
            cell.x = (mix(cell.x + seed, i) + cell.width) & 65535;
            cell.width = (cell.width + length + (i & 7)) & 255;
            cell.selected = (cell.x & 1) == 1;
            total += cell.selected ? cell.x : cell.width;
            cells[index] = cell;
        }
    }
    return total;
}
