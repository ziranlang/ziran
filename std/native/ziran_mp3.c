/* The one translation unit for dr_mp3 (mackron/dr_libs, public domain or
 * MIT-0; see LICENSE in the pinned `dr_libs` source dependency), plus the
 * two calls Ziran cannot express itself: sizing and binding the decoder's
 * native state. Everything else in std/audio_mp3 calls dr_mp3's API directly.
 * Float output keeps the synthesis filter's full precision; s16 reads round
 * and clip that float output. No stdio: callers pass whole-file bytes. */
#define DR_MP3_IMPLEMENTATION
#define DR_MP3_FLOAT_OUTPUT
#define DR_MP3_NO_STDIO
#include "dr_mp3.h"

#include <stdint.h>
#include <stdlib.h>

typedef struct {
    drmp3 decoder;
    drmp3_seek_point *seek_points;
} ziran_mp3;

/* Opens a decoder over borrowed, validated MPEG audio frames. With
 * seek_points > 0 it also binds a seek table of up to that many points, so
 * seeks start a few frames before their target instead of at the stream
 * start; dr_mp3 still decodes forward to the exact frame. Returns NULL when
 * the bytes hold no decodable frame or allocation fails. */
void *ziran_mp3_open(const void *data, size_t size, uint32_t seek_points) {
    ziran_mp3 *state;
    if (data == NULL || size == 0) return NULL;
    state = (ziran_mp3 *)calloc(1, sizeof(ziran_mp3));
    if (state == NULL) return NULL;
    if (!drmp3_init_memory(&state->decoder, data, size, NULL)) {
        free(state);
        return NULL;
    }
    if (seek_points > 0) {
        drmp3_uint32 count = seek_points;
        state->seek_points = (drmp3_seek_point *)calloc(count, sizeof(drmp3_seek_point));
        if (state->seek_points == NULL ||
            !drmp3_calculate_seek_points(&state->decoder, &count, state->seek_points) ||
            !drmp3_bind_seek_table(&state->decoder, count, state->seek_points)) {
            drmp3_uninit(&state->decoder);
            free(state->seek_points);
            free(state);
            return NULL;
        }
    }
    return state;
}

void ziran_mp3_close(void *handle) {
    ziran_mp3 *state = (ziran_mp3 *)handle;
    if (state == NULL) return;
    drmp3_uninit(&state->decoder);
    free(state->seek_points);
    free(state);
}
