// zi_simd_ref for tests/simd_go.sh: the C reference, zi_ref in
// include/ziran_simd.h, that every typed Go helper must match lane for lane.
package main

/*
#define ZIRAN_SIMD_REF_ONLY 1
#include "ziran_simd.h"

static void
zi_simd_ref_c(int op, int r, int rn, int ak, int an, const uint64_t *a, const uint64_t *b,
              const uint64_t *c, const int64_t *k, uint64_t *out)
{
    zi_ref(op, r, rn, ak, an, a, b, c, k, out);
}
*/
import "C"

type zi_lanes = [32]uint64

func zi_simd_ref(op, r, rn, ak, an int, a, b, c *zi_lanes, k *[32]int64, out *zi_lanes) {
	C.zi_simd_ref_c(C.int(op), C.int(r), C.int(rn), C.int(ak), C.int(an), (*C.uint64_t)(&a[0]),
		(*C.uint64_t)(&b[0]), (*C.uint64_t)(&c[0]), (*C.int64_t)(&k[0]), (*C.uint64_t)(&out[0]))
}
