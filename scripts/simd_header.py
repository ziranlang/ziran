#!/usr/bin/env python3
"""Generate include/ziran_simd_ops.h, the typed C helpers for std/simd,
include/ziran_simd_types.h, the vector types alone, and
cmd/zir/zir_simd_go_bodies.h, the typed Go helpers zi2go writes.

    python3 scripts/simd_header.py > include/ziran_simd_ops.h
    python3 scripts/simd_header.py --types > include/ziran_simd_types.h
    python3 scripts/simd_header.py --go > cmd/zir/zir_simd_go_bodies.h
    python3 scripts/simd_header.py --test > build/simd_ops_test.c

Every helper computes exactly what zi_ref in include/ziran_simd.h does. GCC
(12 or later) and Clang builds use native vectors and, where they give the
same bits, SSE2..AVX2, NEON or WebAssembly SIMD128 instructions; any other
build (TinyCC, ZIRAN_SIMD_SCALAR) runs plain C over each type's own lanes.
TinyCC runs every helper as an inline macro, on x86-64 the common integer
ones as SSE2 assembly.
--test writes a program that checks every helper, in every x86-64 tier the
build can run, against zi_ref; tests/simd_go.sh checks the Go helpers.
"""
import random
import re
import sys

LANES = {
    # name: C type, bits, signed, float, kind
    'u8': ('uint8_t', 8, False, False), 's8': ('int8_t', 8, True, False),
    'u16': ('uint16_t', 16, False, False), 's16': ('int16_t', 16, True, False),
    'u32': ('uint32_t', 32, False, False), 's32': ('int32_t', 32, True, False),
    'u64': ('uint64_t', 64, False, False), 's64': ('int64_t', 64, True, False),
    'f32': ('float', 32, False, True), 'f64': ('double', 64, False, True),
}
KIND = {'u8': 'ZI_K_U8', 's8': 'ZI_K_S8', 'u16': 'ZI_K_U16', 's16': 'ZI_K_S16',
        'u32': 'ZI_K_U32', 's32': 'ZI_K_S32', 'u64': 'ZI_K_U64', 's64': 'ZI_K_S64',
        'f32': 'ZI_K_F32', 'f64': 'ZI_K_F64'}


class T:
    def __init__(self, lane, lanes):
        self.lane = lane
        self.ctype, self.bits, self.signed, self.float = LANES[lane]
        self.lanes = lanes
        self.bytes = lanes * self.bits // 8
        self.name = '%sx%d' % (lane, lanes)
        self.c = 'zi_' + self.name
        self.kind = KIND[lane]
        self.wide = self.bytes == 32
        self.block = lanes // 2 if self.wide else lanes

    def __eq__(self, other):
        return isinstance(other, T) and self.name == other.name

    def __hash__(self):
        return hash(self.name)

    def with_lane(self, lane, lanes=None):
        return T(lane, self.lanes if lanes is None else lanes)

    @property
    def u(self):
        return self.with_lane(('u%d' % self.bits))

    @property
    def s(self):
        return self.with_lane(('s%d' % self.bits))

    @property
    def utype(self):
        return 'uint%d_t' % self.bits

    def wider(self, lanes=None):
        return self.with_lane('%s%d' % ('s' if self.signed else 'u', self.bits * 2),
                              self.lanes // 2 if lanes is None else lanes)

    @property
    def half(self):
        return self.with_lane(self.lane, self.lanes // 2)

    @property
    def double(self):
        return self.with_lane(self.lane, self.lanes * 2)

    # Intrinsic spellings.
    @property
    def m(self):
        if self.lane == 'f32': return '__m256' if self.wide else '__m128'
        if self.lane == 'f64': return '__m256d' if self.wide else '__m128d'
        return '__m256i' if self.wide else '__m128i'

    @property
    def epi(self):
        return '%s%d' % ('epu' if not self.signed else 'epi', self.bits)

    @property
    def neon(self):
        if self.float: return 'float%dx%d_t' % (self.bits, self.lanes)
        return '%sint%dx%d_t' % ('' if self.signed else 'u', self.bits, self.lanes)

    @property
    def nsuf(self):
        return ('f' if self.float else 's' if self.signed else 'u') + str(self.bits)

    @property
    def wsuf(self):
        if self.float: return 'f%dx%d' % (self.bits, self.lanes)
        return '%s%dx%d' % ('i' if self.signed else 'u', self.bits, self.lanes)

    @property
    def wisuf(self):
        if self.float: return self.wsuf
        return 'i%dx%d' % (self.bits, self.lanes)


TYPES = [T(l, 128 // LANES[l][1]) for l in LANES] + [T(l, 256 // LANES[l][1]) for l in LANES]
BY_NAME = {t.name: t for t in TYPES}
INT = [t for t in TYPES if not t.float]
FLOAT = [t for t in TYPES if t.float]


def lanes_of(*names):
    return [t for t in TYPES if t.lane in names]


SMALL = lanes_of('u8', 's8', 'u16', 's16')
NARROWABLE = lanes_of('u8', 's8', 'u16', 's16', 'u32', 's32')


def splat(t, value):
    return '((%s){0} + (%s)%s)' % (t.c, t.ctype, value)


def x86(t, intrinsic, *args):
    """A one-intrinsic x86 body: the intrinsic over operands cast to t's type."""
    return 'return (%s)%s(%s);' % (t.c, intrinsic, ', '.join('(%s)%s' % (t.m, a) for a in args))


def neon(t, intrinsic, *args, result=None):
    result = result or t
    return 'return (%s)%s(%s);' % (result.c, intrinsic, ', '.join('(%s)%s' % (t.neon, a) for a in args))


def wasm(t, intrinsic, *args, result=None):
    result = result or t
    return 'return (%s)%s(%s);' % (result.c, intrinsic, ', '.join('(v128_t)%s' % a for a in args))


X86 = 'ZI_X86'
SSSE3 = 'ZI_X86 && ZI_TIER >= 2'
SSE41 = 'ZI_X86 && ZI_TIER >= 3'
AVX2 = 'ZI_X86 && ZI_TIER >= 4'
NEON = 'ZI_NEON'
A64 = 'ZI_NEON && ZI_A64'
WASM = 'ZI_WASM'


class Op:
    """One operation. `types` lists the first vector operand's types."""

    def __init__(self, enum, snake, types, shape):
        self.enum, self.snake, self.types, self.shape = enum, snake, types, shape

    # Subclasses fill these.
    def result(self, t):
        return t

    def params(self, t):
        return [(t.c, 'a'), (t.c, 'b')]

    def name(self, t):
        return '%s_%s' % (self.snake, t.name)

    def special(self, t):
        """[(condition, body)] for 128-bit types, or for 256-bit ones in AVX2."""
        return []

    def generic(self, t):
        """A native-vector body for any width, or None."""
        return None

    def portable(self, t):
        """A body for every build, native vectors or lane arrays, or None."""
        return None

    splittable = True  # 256-bit vectors may run as two 128-bit halves

    def ref(self, t):
        """zi_ref arguments: (r type, a type, [a, b, c] operand C names, k)."""
        args = [n for _, n in self.params(t)]
        return (self.result(t), t, args, None)


def ref_body(op, t):
    r, a_type, operands, k = op.ref(t)
    lines = ['uint64_t x_[32] = {0}, y_[32] = {0}, z_[32] = {0}, o_[32];', 'int64_t k_[32] = {0};']
    names = ['x_', 'y_', 'z_']
    types = {n: c for c, n in op.params(t)}
    for i, operand in enumerate(operands):
        if operand is None:
            continue
        vector = BY_NAME[types[operand][3:]]
        lines.append('zi_unpack_%s(%s, %s);' % (vector.name, operand, names[i]))
    for i, value in enumerate(k or []):
        lines.append('k_[%d] = (int64_t)(%s);' % (i, value))
    lines.append('zi_ref(%s, %s, %d, %s, %d, x_, y_, z_, k_, o_);' %
                 (op.enum, r.kind, r.lanes, a_type.kind, a_type.lanes))
    lines.append('return zi_pack_%s(o_);' % r.name)
    return lines


def unsigned_add(t, x, y, op='+'):
    if t.float:
        return '%s %s %s' % (x, op, y)
    return '(%s)((%s)%s %s (%s)%s)' % (t.c, t.u.c, x, op, t.u.c, y)


class Lanewise(Op):
    def __init__(self, enum, snake, types, c_op=None, x86_name=None, x86_tier=X86,
                 neon_name=None, wasm_name=None, generic=None, specials=None):
        super().__init__(enum, snake, types, 'binary')
        self.c_op, self.x86_name, self.x86_tier = c_op, x86_name, x86_tier
        self.neon_name, self.wasm_name = neon_name, wasm_name
        self._generic, self._specials = generic, specials

    def special(self, t):
        if self._specials:
            return self._specials(t)
        out = []
        if self.x86_name:
            name = self.x86_name(t) if callable(self.x86_name) else self.x86_name
            if name:
                tier = self.x86_tier(t) if callable(self.x86_tier) else self.x86_tier
                if t.wide:
                    out.append((AVX2, x86(t, name.replace('_mm_', '_mm256_'), 'a', 'b')))
                else:
                    out.append((tier, x86(t, name, 'a', 'b')))
        if t.wide:
            return out
        if self.neon_name:
            name = self.neon_name(t)
            if name:
                out.append((NEON if t.bits < 64 or not t.float else A64, neon(t, name, 'a', 'b')))
        if self.wasm_name:
            name = self.wasm_name(t)
            if name:
                out.append((WASM, wasm(t, name, 'a', 'b')))
        return out

    def generic(self, t):
        if self._generic:
            return self._generic(t)
        if self.c_op:
            return 'return %s;' % unsigned_add(t, 'a', 'b', self.c_op)
        return None


def sat_name(t, sub):
    return '_mm_%ss_%s' % ('sub' if sub else 'add', t.epi)


def select_bits(t, mask, yes, no):
    """(yes & mask) | (no & ~mask) on t's unsigned view."""
    u = t.u.c
    return '(%s)(((%s)%s & %s) | ((%s)%s & ~%s))' % (t.c, u, yes, mask, u, no, mask)


def minmax_generic(t, less):
    op = '<' if less else '>'
    return '%s m_ = (%s)(a %s b);\n    return %s;' % (t.u.c, t.u.c, op, select_bits(t, 'm_', 'a', 'b'))


def minmax_x86(t, less):
    word = 'min' if less else 'max'
    if t.lane == 'f32': return '_mm_%s_ps' % word, X86
    if t.lane == 'f64': return '_mm_%s_pd' % word, X86
    if t.lane in ('s16', 'u8'): return '_mm_%s_%s' % (word, t.epi), X86
    if t.bits <= 32: return '_mm_%s_%s' % (word, t.epi), SSE41
    return None, None


def minmax_specials(less):
    def make(t):
        word = 'min' if less else 'max'
        out = []
        name, tier = minmax_x86(t, less)
        if name:
            if t.wide:
                if t.float: out.append((AVX2, x86(t, name.replace('_mm_', '_mm256_'), 'a', 'b')))
                else: out.append((AVX2, x86(t, name.replace('_mm_', '_mm256_'), 'a', 'b')))
            else:
                out.append((tier, x86(t, name, 'a', 'b')))
        if t.lane == 'u16' and not t.wide:
            # SSE2: a - (a -sat b) is the smaller, (a -sat b) + b the larger.
            out.append((X86, '__m128i x_ = (__m128i)a, y_ = (__m128i)b;\n    return (%s)%s;' % (
                t.c, '_mm_sub_epi16(x_, _mm_subs_epu16(x_, y_))' if less else
                '_mm_add_epi16(_mm_subs_epu16(x_, y_), y_)')))
        if t.wide:
            return out
        if not t.float and t.bits < 64:
            out.append((NEON, neon(t, 'v%sq_%s' % (word, t.nsuf), 'a', 'b')))
            out.append((WASM, wasm(t, 'wasm_%s_%s' % (t.wsuf, word), 'a', 'b')))
        if t.float:
            # pmin(b, a) is a < b ? a : b; pmax(b, a) is a > b ? a : b.
            out.append((WASM, wasm(t, 'wasm_%s_p%s' % (t.wsuf, word), 'b', 'a')))
        return out
    return make


def mulhi_specials(t):
    out = []
    if t.wide:
        return [(AVX2, x86(t, '_mm256_mulhi_%s' % t.epi, 'a', 'b'))]
    out.append((X86, x86(t, '_mm_mulhi_%s' % t.epi, 'a', 'b')))
    s = t.nsuf
    w = 's32' if t.signed else 'u32'
    out.append((NEON, '%s x_ = (%s)a, y_ = (%s)b;\n'
                '    return (%s)vcombine_%s(vshrn_n_%s(vmull_%s(vget_low_%s(x_), vget_low_%s(y_)), 16),\n'
                '        vshrn_n_%s(vmull_%s(vget_high_%s(x_), vget_high_%s(y_)), 16));' %
                (t.neon, t.neon, t.neon, t.c, s, w, s, s, s, w, s, s, s)))
    ext = 'i32x4' if t.signed else 'u32x4'
    src = 'i16x8' if t.signed else 'u16x8'
    out.append((WASM, 'v128_t lo_ = wasm_%s_extmul_low_%s((v128_t)a, (v128_t)b),\n'
                '        hi_ = wasm_%s_extmul_high_%s((v128_t)a, (v128_t)b);\n'
                '    return (%s)wasm_i16x8_shuffle(lo_, hi_, 1, 3, 5, 7, 9, 11, 13, 15);' %
                (ext, src, ext, src, t.c)))
    return out


def wide_local(t, lane):
    """A local native vector of t's lanes as `lane`, twice t's size."""
    return 'typedef %s w_ __attribute__((vector_size(%d)));' % (LANES[lane][0], t.lanes * LANES[lane][1] // 8)


def mulhi_generic(t):
    wl = 's32' if t.signed else 'u32'
    return ('%s\n    typedef uint32_t wu_ __attribute__((vector_size(%d)));\n'
            '    w_ p_ = __builtin_convertvector(a, w_) * __builtin_convertvector(b, w_);\n'
            '    return (%s)__builtin_convertvector((wu_)(p_ >> 16), %s);' %
            (wide_local(t, wl), t.lanes * 4, t.c, t.u.c))


def mulhrs_specials(t):
    if t.wide:
        return [(AVX2, x86(t, '_mm256_mulhrs_epi16', 'a', 'b'))]
    return [
        (SSSE3, x86(t, '_mm_mulhrs_epi16', 'a', 'b')),
        (X86, '__m128i x_ = (__m128i)a, y_ = (__m128i)b;\n'
              '    __m128i hi_ = _mm_mulhi_epi16(x_, y_), lo_ = _mm_mullo_epi16(x_, y_);\n'
              '    __m128i q_ = _mm_or_si128(_mm_slli_epi16(hi_, 1), _mm_srli_epi16(lo_, 15));\n'
              '    return (%s)_mm_add_epi16(q_, _mm_and_si128(_mm_srli_epi16(lo_, 14), _mm_set1_epi16(1)));' % t.c),
        (NEON, 'int16x8_t x_ = (int16x8_t)a, y_ = (int16x8_t)b;\n'
               '    return (%s)vcombine_s16(vrshrn_n_s32(vmull_s16(vget_low_s16(x_), vget_low_s16(y_)), 15),\n'
               '        vrshrn_n_s32(vmull_s16(vget_high_s16(x_), vget_high_s16(y_)), 15));' % t.c),
        # q15mulr_sat saturates MIN * MIN to MAX where pmulhrsw wraps to MIN.
        (WASM, 'v128_t x_ = (v128_t)a, y_ = (v128_t)b, min_ = wasm_i16x8_splat(-32768);\n'
               '    return (%s)wasm_v128_xor(wasm_i16x8_q15mulr_sat(x_, y_),\n'
               '        wasm_v128_and(wasm_i16x8_eq(x_, min_), wasm_i16x8_eq(y_, min_)));' % t.c),
    ]


def mulhrs_generic(t):
    return ('%s\n    typedef uint32_t wu_ __attribute__((vector_size(%d)));\n'
            '    w_ p_ = __builtin_convertvector(a, w_) * __builtin_convertvector(b, w_);\n'
            '    return (%s)__builtin_convertvector((wu_)((p_ + 0x4000) >> 15), %s);' %
            (wide_local(t, 's32'), t.lanes * 4, t.c, t.u.c))


class Unary(Op):
    def __init__(self, enum, snake, types, generic, specials=None):
        super().__init__(enum, snake, types, 'unary')
        self._generic, self._specials = generic, specials

    def params(self, t):
        return [(t.c, 'a')]

    def special(self, t):
        return self._specials(t) if self._specials else []

    def generic(self, t):
        return self._generic(t)


def sign_mask(t):
    return '((%s){0} + ((%s)1 << %d))' % (t.u.c, t.utype, t.bits - 1)


def abs_generic(t):
    if t.float:
        return 'return (%s)((%s)a & ~%s);' % (t.c, t.u.c, sign_mask(t))
    return ('%s m_ = (%s)(a >> %d);\n    return (%s)(((%s)a ^ m_) - m_);' %
            (t.u.c, t.u.c, t.bits - 1, t.c, t.u.c))


def abs_specials(t):
    if t.float:
        return []
    out = []
    if t.bits <= 32:
        if t.wide:
            return [(AVX2, x86(t, '_mm256_abs_epi%d' % t.bits, 'a'))]
        out.append((SSSE3, x86(t, '_mm_abs_epi%d' % t.bits, 'a')))
        # SSE2: the larger of a and -a, unsigned for bytes; -MIN stays MIN.
        if t.bits == 8:
            out.append((X86, '__m128i x_ = (__m128i)a;\n'
                        '    return (%s)_mm_min_epu8(x_, _mm_sub_epi8(_mm_setzero_si128(), x_));' % t.c))
        elif t.bits == 16:
            out.append((X86, '__m128i x_ = (__m128i)a;\n'
                        '    return (%s)_mm_max_epi16(x_, _mm_sub_epi16(_mm_setzero_si128(), x_));' % t.c))
    if t.wide:
        return out
    out.append((NEON if t.bits < 64 else A64, neon(t, 'vabsq_%s' % t.nsuf, 'a')))
    out.append((WASM, wasm(t, 'wasm_%s_abs' % t.wsuf, 'a')))
    return out


def neg_generic(t):
    if t.float:
        return 'return (%s)((%s)a ^ %s);' % (t.c, t.u.c, sign_mask(t))
    return 'return (%s)((%s){0} - (%s)a);' % (t.c, t.u.c, t.u.c)


def sign_generic(t):
    # a negated where b < 0 ((a ^ -1) + 1), then zeroed where b == 0.
    u = t.u.c
    return ('%s n_ = (%s)(b >> %d), z_ = (%s)(b != 0);\n    return (%s)((((%s)a ^ n_) - n_) & z_);' %
            (u, u, t.bits - 1, u, t.c, u))


def sign_specials(t):
    if t.bits == 64:
        return []
    if t.wide:
        return [(AVX2, x86(t, '_mm256_sign_epi%d' % t.bits, 'a', 'b'))]
    return [(SSSE3, x86(t, '_mm_sign_epi%d' % t.bits, 'a', 'b'))]


class AbsDiff(Op):
    """|a - b| in the unsigned lanes of a's width."""

    def __init__(self):
        super().__init__('ZI_OP_ABS_DIFF', 'abs_diff', INT, 'binary')

    def result(self, t):
        return t.u

    def special(self, t):
        r = t.u
        m, reg = ('_mm256_', '__m256i') if t.wide else ('_mm_', '__m128i')
        out = []
        if t.bits <= 16 and not t.signed:
            # Saturating differences both ways: one of them is 0.
            out.append((AVX2 if t.wide else X86, '%s x_ = (%s)a, y_ = (%s)b;\n'
                        '    return (%s)%sor_si%d(%ssubs_%s(x_, y_), %ssubs_%s(y_, x_));' %
                        (reg, reg, reg, r.c, m, t.bytes * 8, m, t.epi, m, t.epi)))
        elif t.bits <= 32:
            tier = AVX2 if t.wide else (X86 if t.lane == 's16' else SSE41)
            out.append((tier, '%s x_ = (%s)a, y_ = (%s)b;\n'
                        '    return (%s)%ssub_epi%d(%smax_%s(x_, y_), %smin_%s(x_, y_));' %
                        (reg, reg, reg, r.c, m, t.bits, m, t.epi, m, t.epi)))
        if t.wide:
            return out
        if t.bits < 64:
            out.append((NEON, 'return (%s)vabdq_%s((%s)a, (%s)b);' % (r.c, t.nsuf, t.neon, t.neon)))
            if t.bits <= 16 and not t.signed:
                out.append((WASM, 'v128_t x_ = (v128_t)a, y_ = (v128_t)b;\n'
                            '    return (%s)wasm_v128_or(wasm_%s_sub_sat(x_, y_), wasm_%s_sub_sat(y_, x_));' %
                            (r.c, t.wsuf, t.wsuf)))
            else:
                out.append((WASM, 'v128_t x_ = (v128_t)a, y_ = (v128_t)b;\n'
                            '    return (%s)wasm_%s_sub(wasm_%s_max(x_, y_), wasm_%s_min(x_, y_));' %
                            (r.c, t.wisuf, t.wsuf, t.wsuf)))
        return out

    def generic(self, t):
        # a - b, negated where a < b.
        u = t.u.c
        return ('%s l_ = (%s)(a < b);\n    return (((%s)a - (%s)b) ^ l_) - l_;' % (u, u, u, u))


class Compare(Op):
    def __init__(self, enum, snake, c_op):
        super().__init__(enum, snake, TYPES, 'compare')
        self.c_op = c_op

    clang_whole = True  # Clang compares 256-bit vectors well without halves

    def result(self, t):
        return t.u if t.float else t

    def generic(self, t):
        return 'return (%s)(a %s b);' % (self.result(t).c, self.c_op)


class Select(Op):
    def __init__(self):
        super().__init__('ZI_OP_SELECT', 'select', TYPES, 'select')

    def params(self, t):
        return [(t.u.c, 'm'), (t.c, 'a'), (t.c, 'b')]

    def special(self, t):
        if t.wide:
            return []
        return [(NEON, 'return (%s)vbslq_u8((uint8x16_t)m, (uint8x16_t)a, (uint8x16_t)b);' % t.c),
                (WASM, 'return (%s)wasm_v128_bitselect((v128_t)a, (v128_t)b, (v128_t)m);' % t.c)]

    def generic(self, t):
        return 'return %s;' % select_bits(t, 'm', 'a', 'b')


class Shift(Op):
    """Shift every lane by one count; `kind` is shl, shr or shr_round."""

    def __init__(self, enum, snake, kind):
        super().__init__(enum, snake, INT, 'shift')
        self.kind = kind
        # Clang rounds 256-bit vectors better from the 256-bit steps.
        self.clang_whole = kind == 'shr_round'

    def params(self, t):
        return [(t.c, 'a'), ('uint64_t', 'n')]

    def ref(self, t):
        return (t, t, ['a'], ['n'])

    def special(self, t):
        if self.kind == 'shr_round' or t.bits == 8:
            return []
        if self.kind == 'shl':
            name = '_mm_sll_epi%d' % t.bits
        elif not t.signed:
            name = '_mm_srl_epi%d' % t.bits
        elif t.bits < 64:
            name = '_mm_sra_epi%d' % t.bits
        else:
            return []
        if t.wide:
            name, tier = name.replace('_mm_', '_mm256_'), AVX2
        else:
            tier = X86
        return [(tier, 'return (%s)%s((%s)a, _mm_set_epi64x(0, (long long)n));' % (t.c, name, t.m))]

    def generic(self, t):
        if self.kind == 'shl':
            return ('if(n >= %d) return (%s){0};\n    return (%s)((%s)a << (int)n);' %
                    (t.bits, t.c, t.c, t.u.c))
        if self.kind == 'shr' and not t.signed:
            return 'if(n >= %d) return (%s){0};\n    return a >> (int)n;' % (t.bits, t.c)
        if self.kind == 'shr':
            return 'return a >> (int)(n >= %d ? %d : n);' % (t.bits, t.bits - 1)
        return None

    def portable(self, t):
        if self.kind != 'shr_round':
            return None
        # (v >> n) plus the last bit shifted out; n == 0 keeps v.
        return ('if(n == 0) return a;\n'
                '    {\n'
                '        %s s_ = ZI_N(shr_%s)(a, n), t_ = ZI_N(shr_%s)(a, n - 1);\n'
                '        return ZI_N(add_%s)(s_, ZI_N(and_%s)(t_, ZI_N(splat_%s)(1)));\n'
                '    }' % (t.c, t.name, t.name, t.name, t.name, t.name))


class ShiftLanes(Op):
    def __init__(self, enum, snake, left):
        super().__init__(enum, snake, INT, 'shift_lanes')
        self.left = left

    def special(self, t):
        if t.bits == 16:
            # AVX2 shifts 32-bit lanes: the even 16-bit lanes in place, the
            # odd ones in the upper halves, then one blend.
            m, reg = ('_mm256_', '__m256i') if t.wide else ('_mm_', '__m128i')
            lines = ['%s x_ = (%s)a, c_ = (%s)b, lo_ = %sset1_epi32(0xffff);' % (reg, reg, reg, m),
                     '%s ce_ = %sand_si%d(c_, lo_), co_ = %ssrli_epi32(c_, 16);' % (reg, m, t.bytes * 8, m)]
            if self.left:
                lines.append('%s e_ = %ssllv_epi32(x_, ce_), o_ = %ssllv_epi32(%sandnot_si%d(lo_, x_), co_);' %
                             (reg, m, m, m, t.bytes * 8))
            elif not t.signed:
                lines.append('%s e_ = %ssrlv_epi32(%sand_si%d(x_, lo_), ce_), o_ = %ssrlv_epi32(x_, co_);' %
                             (reg, m, m, t.bytes * 8, m))
            else:
                lines.append('%s e_ = %ssrav_epi32(%sslli_epi32(x_, 16), %sadd_epi32(ce_, %sset1_epi32(16))),'
                             ' o_ = %ssrav_epi32(x_, co_);' % (reg, m, m, m, m, m))
            lines.append('return (%s)%sblend_epi16(e_, o_, 0xaa);' % (t.c, m))
            return [(AVX2, '\n    '.join(lines))]
        if t.bits < 32:
            return []
        if self.left:
            name = '_mm_sllv_epi%d' % t.bits
        elif not t.signed:
            name = '_mm_srlv_epi%d' % t.bits
        elif t.bits == 32:
            name = '_mm_srav_epi32'
        else:
            return []
        if t.wide:
            name = name.replace('_mm_', '_mm256_')
        return [(AVX2, x86(t, name, 'a', 'b'))]

    def generic(self, t):
        u, w = t.u.c, t.bits
        if self.left or not t.signed:
            shift = '<<' if self.left else '>>'
            source = '(%s)a' % u
            return ('%s c_ = (%s)b, ok_ = (%s)(c_ < %d);\n'
                    '    return (%s)((%s %s (c_ & %d)) & ok_);' %
                    (u, u, u, w, t.c, source, shift, w - 1))
        return ('%s c_ = (%s)b, top_ = (%s){0} + %d, m_ = (%s)(c_ < top_);\n'
                '    c_ = (c_ & m_) | (top_ & ~m_);\n'
                '    return a >> (%s)c_;' % (u, u, u, w - 1, u, t.c))


class ByteMacro(Op):
    """Byte shifts and AlignBytes take a constant count: they are macros."""

    def __init__(self, enum, snake, kind):
        super().__init__(enum, snake, INT, kind)
        self.kind = kind


class Make(Op):
    def __init__(self):
        super().__init__('ZI_OP_MAKE', 'make', TYPES, 'make')

    def params(self, t):
        return [(t.ctype, 'l%d' % i) for i in range(t.lanes)]

    def generic(self, t):
        return 'return (%s){%s};' % (t.c, ', '.join('l%d' % i for i in range(t.lanes)))

    def portable(self, t):
        return '%s r_ = {{%s}};\n    return r_;' % (t.c, ', '.join('l%d' % i for i in range(t.lanes)))


class Splat(Op):
    def __init__(self):
        super().__init__('ZI_OP_SPLAT', 'splat', TYPES, 'splat')

    def params(self, t):
        return [(t.ctype, 'x')]

    def portable(self, t):
        return ('%s r_;\n    for(int i_ = 0; i_ < %d; i_++) ZI_AT(r_, i_) = x;\n    return r_;' %
                (t.c, t.lanes))

    def generic(self, t):
        return 'return (%s){%s};' % (t.c, ', '.join(['x'] * t.lanes))


class Zero(Op):
    def __init__(self):
        super().__init__('ZI_OP_ZERO', 'zero', TYPES, 'zero')

    def params(self, t):
        return []

    def generic(self, t):
        return 'return (%s){0};' % t.c

    def portable(self, t):
        return '%s r_;\n    memset(&r_, 0, sizeof(r_));\n    return r_;' % t.c


class Lane(Op):
    def __init__(self):
        super().__init__('ZI_OP_LANE', 'lane', TYPES, 'lane')

    def result(self, t):
        return t.ctype

    def params(self, t):
        return [(t.c, 'a'), ('int', 'i')]

    def portable(self, t):
        return 'return ZI_AT(a, i);'


class WithLane(Op):
    splittable = False  # the lane index spans both halves

    def __init__(self):
        super().__init__('ZI_OP_WITH_LANE', 'with_lane', TYPES, 'with_lane')

    def params(self, t):
        return [(t.c, 'a'), ('int', 'i'), (t.ctype, 'x')]

    def portable(self, t):
        return 'ZI_AT(a, i) = x;\n    return a;'


class Memory(Op):
    def __init__(self, enum, snake, store, low):
        super().__init__(enum, snake, TYPES, 'memory')
        self.store, self.low = store, low

    def result(self, t):
        return 'void' if self.store else t

    def params(self, t):
        if self.store:
            return [('void *', 'p'), (t.c, 'a')]
        return [('const void *', 'p')]

    def special(self, t):
        # Unaligned vector loads and stores: the 4-byte-aligned vector types
        # make GCC copy a whole vector as two 64-bit halves.
        if t.wide:
            if self.low:
                return []
            if self.store:
                return [(AVX2, '_mm256_storeu_si256((__m256i *)p, (__m256i)a);')]
            return [(AVX2, 'return (%s)_mm256_loadu_si256((const __m256i *)p);' % t.c)]
        if self.store and self.low:
            return [(X86, '_mm_storel_epi64((__m128i *)p, (__m128i)a);'),
                    (NEON, 'vst1_u8((uint8_t *)p, vget_low_u8((uint8x16_t)a));'),
                    (WASM, 'wasm_v128_store64_lane(p, (v128_t)a, 0);')]
        if self.store:
            return [(X86, '_mm_storeu_si128((__m128i *)p, (__m128i)a);'),
                    (NEON, 'vst1q_u8((uint8_t *)p, (uint8x16_t)a);'),
                    (WASM, 'wasm_v128_store(p, (v128_t)a);')]
        if self.low:
            # A 64-bit load into a zeroed vector.
            return [(X86, 'return (%s)_mm_loadl_epi64((const __m128i *)p);' % t.c),
                    (NEON, 'return (%s)vcombine_u8(zi_neon_load64(p), vdup_n_u8(0));' % t.c),
                    (WASM, 'return (%s)wasm_v128_load64_zero(p);' % t.c)]
        return [(X86, 'return (%s)_mm_loadu_si128((const __m128i *)p);' % t.c),
                (NEON, 'return (%s)vld1q_u8((const uint8_t *)p);' % t.c),
                (WASM, 'return (%s)wasm_v128_load(p);' % t.c)]

    def generic(self, t):
        if self.store or not self.low:
            return None
        return 'uint64_t w_;\n    memcpy(&w_, p, 8);\n    return (%s)(zi_u64x%d){w_};' % (t.c, t.bytes // 8)

    splittable = False

    def portable(self, t):
        size = 8 if self.low else t.bytes
        if self.store:
            return 'memcpy(p, &a, %d);' % size
        if self.low:
            return '%s r_;\n    memset(&r_, 0, sizeof(r_));\n    memcpy(&r_, p, 8);\n    return r_;' % t.c
        return '%s r_;\n    memcpy(&r_, p, %d);\n    return r_;' % (t.c, size)


class Gather(Op):
    """Gather(table, indices): 32-bit lanes; checked indices."""

    def __init__(self):
        super().__init__('ZI_OP_GATHER', 'gather', lanes_of('u32', 's32'), 'gather')

    def results(self, t):
        return [t.with_lane(l) for l in ('u32', 's32', 'f32')]

    def name_for(self, t, r):
        return 'gather_%s_%s' % (t.name, r.name)


class WidenHalf(Op):
    def __init__(self, enum, snake, high):
        super().__init__(enum, snake, NARROWABLE, 'widen_half')
        self.high = high

    def result(self, t):
        return t.wider()

    def params(self, t):
        return [(t.c, 'a')]

    def special(self, t):
        # Unpacking with zeros or with the sign widens each 128-bit block.
        r = self.result(t)
        m, reg = ('_mm256_', '__m256i') if t.wide else ('_mm_', '__m128i')
        unpack = '%sunpack%s_epi%d' % (m, 'hi' if self.high else 'lo', t.bits)
        if not t.signed:
            body = 'return (%s)%s((%s)a, %ssetzero_si%d());' % (r.c, unpack, reg, m, t.bytes * 8)
        elif t.bits < 32:
            body = 'return (%s)%ssrai_epi%d(%s((%s)a, (%s)a), %d);' % (
                r.c, m, t.bits * 2, unpack, reg, reg, t.bits)
        else:
            body = 'return (%s)%s((%s)a, %ssrai_epi32((%s)a, 31));' % (r.c, unpack, reg, m, reg)
        out = [(AVX2 if t.wide else X86, body)]
        if t.wide:
            return out
        half = 'high' if self.high else 'low'
        out.append((A64, 'return (%s)vmovl_%s%s((%s)a);' % (
            r.c, 'high_' if self.high else '', t.nsuf, t.neon)) if self.high else
            (NEON, 'return (%s)vmovl_%s(vget_low_%s((%s)a));' % (r.c, t.nsuf, t.nsuf, t.neon)))
        if self.high:
            out.append((NEON, 'return (%s)vmovl_%s(vget_high_%s((%s)a));' % (r.c, t.nsuf, t.nsuf, t.neon)))
        letter = 'i' if t.signed else 'u'
        out.append((WASM, 'return (%s)wasm_%s%dx%d_extend_%s_%s%dx%d((v128_t)a);' % (
            r.c, letter, t.bits * 2, t.lanes // 2, half, letter, t.bits, t.lanes)))
        return out

    def generic(self, t):
        # The half of each 128-bit block's lanes as one shuffle.
        r = self.result(t)
        per = t.block
        start = per // 2 if self.high else 0
        picks = [block * per + start + i for block in range(t.lanes // per) for i in range(per // 2)]
        return 'return (%s)__builtin_convertvector(__builtin_shufflevector(a, a, %s), %s);' % (
            r.c, ', '.join(str(i) for i in picks), r.c)


class Widen(Op):
    def __init__(self):
        super().__init__('ZI_OP_WIDEN', 'widen', [t for t in NARROWABLE if not t.wide], 'widen')

    def result(self, t):
        return t.wider(t.lanes)

    def params(self, t):
        return [(t.c, 'a')]

    def special(self, t):
        # One extension in AVX2; elsewhere the two halves' widenings.
        r = self.result(t)
        extend = '_mm256_cvt%s%d_epi%d' % ('epi' if t.signed else 'epu', t.bits, t.bits * 2)
        halves = 'return ZI_N(combine_%s)(ZI_N(widen_low_%s)(a), ZI_N(widen_high_%s)(a));' % (
            r.half.name, t.name, t.name)
        return [(AVX2, 'return (%s)%s((__m128i)a);' % (r.c, extend)),
                ('%s || %s || %s' % (X86, NEON, WASM), halves)]

    def generic(self, t):
        return 'return (%s)__builtin_convertvector(a, %s);' % (self.result(t).c, self.result(t).c)

    splittable = False


def narrow_pairs():
    pairs = []
    for t in TYPES:
        if t.float or t.bits == 8:
            continue
        for sign in 'us':
            r = t.with_lane('%s%d' % (sign, t.bits // 2), t.lanes * 2)
            pairs.append((t, r))
    return pairs


class Narrow(Op):
    def __init__(self):
        super().__init__('ZI_OP_NARROW', 'narrow', None, 'narrow')

    def special_pair(self, t, r):
        hi = (1 << (r.bits - 1)) - 1 if r.signed else (1 << r.bits) - 1
        pair = (t.lane, r.lane)
        out = []
        # x86: packs/packus read signed lanes, so unsigned sources are first
        # clamped below the result's maximum.
        packs = {('s16', 's8'): ('_mm_packs_epi16', X86), ('s16', 'u8'): ('_mm_packus_epi16', X86),
                 ('s32', 's16'): ('_mm_packs_epi32', X86), ('s32', 'u16'): ('_mm_packus_epi32', SSE41),
                 ('u16', 'u8'): ('_mm_packus_epi16', None), ('u16', 's8'): ('_mm_packs_epi16', None),
                 ('u32', 'u16'): ('_mm_packus_epi32', None), ('u32', 's16'): ('_mm_packs_epi32', None)}
        if pair in packs:
            name, tier = packs[pair]
            m, reg = ('_mm256_', '__m256i') if t.wide else ('_mm_', '__m128i')
            name = name.replace('_mm_', m)
            if t.signed:
                body = 'return (%s)%s((%s)a, (%s)b);' % (r.c, name, reg, reg)
            else:
                lim = '%sset1_epi%d(%d)' % (m, t.bits, hi)
                body = ('%s m_ = %s;\n    return (%s)%s(%smin_epu%d((%s)a, m_), %smin_epu%d((%s)b, m_));' %
                        (reg, lim, r.c, name, m, t.bits, reg, m, t.bits, reg))
            if t.wide:
                out.append((AVX2, body))
            elif t.signed:
                out.append((tier, body))
            else:
                out.append((SSE41, body))
            if not t.wide and t.bits == 16 and not t.signed:
                # SSE2: x - (x -sat max) is min(x, max).
                out.append((X86, '__m128i m_ = _mm_set1_epi16(%d), x_ = (__m128i)a, y_ = (__m128i)b;\n'
                            '    return (%s)%s(_mm_sub_epi16(x_, _mm_subs_epu16(x_, m_)), _mm_sub_epi16(y_, _mm_subs_epu16(y_, m_)));' %
                            (hi, r.c, name)))
            if not t.wide and t.bits == 32 and r.lane in ('u16', 's16') and (t.signed, r.signed) != (True, True):
                # SSE2 into 16 bits: bring lanes into [0, 2^31) (u32: >= 2^31
                # becomes 2^31 - 1; s32: < 0 becomes 0), then packs. For u16
                # results packs works on lanes offset by -32768.
                if t.signed:
                    fix = '_mm_andnot_si128(_mm_srai_epi32(%s, 31), %s)'
                else:
                    fix = '_mm_and_si128(_mm_or_si128(%s, _mm_srai_epi32(%s, 31)), _mm_set1_epi32(0x7fffffff))'
                fx = fix % ('x_', 'x_')
                fy = fix % ('y_', 'y_')
                if r.signed:
                    body = ('__m128i x_ = (__m128i)a, y_ = (__m128i)b;\n'
                            '    return (%s)_mm_packs_epi32(%s, %s);' % (r.c, fx, fy))
                else:
                    body = ('__m128i x_ = (__m128i)a, y_ = (__m128i)b, o_ = _mm_set1_epi32(32768);\n'
                            '    __m128i p_ = _mm_packs_epi32(_mm_sub_epi32(%s, o_), _mm_sub_epi32(%s, o_));\n'
                            '    return (%s)_mm_xor_si128(p_, _mm_set1_epi16((short)0x8000));' % (fx, fy, r.c))
                out.append((X86, body))
        if t.wide:
            return out
        # NEON: saturating narrows; unsigned into signed takes vqmovn then min.
        rh = '%s%d' % ('s' if r.signed else 'u', r.bits)
        if t.signed:
            fn = 'vqmovn' if r.signed else 'vqmovun'
            out.append((NEON, 'return (%s)vcombine_%s(%s_%s((%s)a), %s_%s((%s)b));' %
                        (r.c, rh, fn, t.nsuf, t.neon, fn, t.nsuf, t.neon)))
        else:
            ru = 'u%d' % r.bits
            body = 'vcombine_%s(vqmovn_%s((%s)a), vqmovn_%s((%s)b))' % (ru, t.nsuf, t.neon, t.nsuf, t.neon)
            if r.signed:
                body = 'vminq_%s(%s, vdupq_n_%s(%d))' % (ru, body, ru, hi)
            out.append((NEON, 'return (%s)%s;' % (r.c, body)))
        wasm_names = {('s16', 's8'): 'wasm_i8x16_narrow_i16x8', ('s16', 'u8'): 'wasm_u8x16_narrow_i16x8',
                      ('s32', 's16'): 'wasm_i16x8_narrow_i32x4', ('s32', 'u16'): 'wasm_u16x8_narrow_i32x4',
                      ('u16', 'u8'): 'wasm_u8x16_narrow_i16x8', ('u16', 's8'): 'wasm_i8x16_narrow_i16x8',
                      ('u32', 'u16'): 'wasm_u16x8_narrow_i32x4', ('u32', 's16'): 'wasm_i16x8_narrow_i32x4'}
        name = wasm_names.get(pair)
        if name and t.signed:
            out.append((WASM, 'return (%s)%s((v128_t)a, (v128_t)b);' % (r.c, name)))
        elif name:
            out.append((WASM, 'v128_t m_ = wasm_%s_splat(%d);\n'
                        '    return (%s)%s(wasm_%s_min((v128_t)a, m_), wasm_%s_min((v128_t)b, m_));' %
                        (t.wisuf, hi, r.c, name, t.wsuf, t.wsuf)))
        return out

    def generic_pair(self, t, r):
        """Clamp both inputs to r's range, join them block by block and
        truncate the lanes, which now fit."""
        hi = (1 << (r.bits - 1)) - 1 if r.signed else (1 << r.bits) - 1
        lo = -(1 << (r.bits - 1)) if r.signed else 0
        u = t.u.c
        lines = ['%s x_ = a, y_ = b, hi_ = (%s){0} + (%s)%dLL;' % (t.c, t.c, t.ctype, hi)]
        if t.signed:
            lines.append('%s lo_ = (%s){0} + (%s)(%dLL);' % (t.c, t.c, t.ctype, lo))
        for v in ('x_', 'y_'):
            lines.append('%s = %s;' % (v, select_bits(t, '(%s)(%s > hi_)' % (u, v), 'hi_', v)))
            if t.signed:
                lines.append('%s = %s;' % (v, select_bits(t, '(%s)(%s < lo_)' % (u, v), 'lo_', v)))
        per = t.block
        idx = []
        for block in range(2 if t.wide else 1):
            idx += [block * per + i for i in range(per)]
            idx += [t.lanes + block * per + i for i in range(per)]
        lines.append('return (%s)__builtin_convertvector((zi_w_%s)__builtin_shufflevector(x_, y_, %s), %s);' %
                     (r.c, t.name, ', '.join(map(str, idx)), r.u.c))
        return '\n    '.join(lines)


class MulWide(Op):
    def __init__(self, enum, snake, high):
        super().__init__(enum, snake, NARROWABLE, 'mul_wide')
        self.high = high

    def result(self, t):
        return t.wider()

    def special(self, t):
        r = self.result(t)
        m, reg = ('_mm256_', '__m256i') if t.wide else ('_mm_', '__m128i')
        half = 'hi' if self.high else 'lo'
        tier = AVX2 if t.wide else X86
        if t.bits == 16:
            # Low and high product halves, interleaved.
            return [(tier, '%s x_ = (%s)a, y_ = (%s)b;\n'
                     '    return (%s)%sunpack%s_epi16(%smullo_epi16(x_, y_), %smulhi_%s(x_, y_));' %
                     (reg, reg, reg, r.c, m, half, m, m, t.epi))]
        if t.bits == 32:
            # pmuludq/pmuldq multiply lanes 0 and 2; duplicate the half's lanes there.
            mul = '%smul_%s' % (m, 'epi32' if t.signed else 'epu32')
            if t.signed and not t.wide:
                tier = SSE41
            return [(tier, '%s x_ = (%s)a, y_ = (%s)b;\n'
                     '    return (%s)%s(%sunpack%s_epi32(x_, x_), %sunpack%s_epi32(y_, y_));' %
                     (reg, reg, reg, r.c, mul, m, half, m, half))]
        return []

    def generic(self, t):
        r = self.result(t)
        widen = 'widen_%s_%s' % ('high' if self.high else 'low', t.name)
        return ('%s x_ = ZI_N(%s)(a), y_ = ZI_N(%s)(b);\n    return (%s)((%s)x_ * (%s)y_);' %
                (r.c, widen, widen, r.c, r.u.c, r.u.c))


def even_odd(t, v):
    evens = ', '.join(str(i) for i in range(0, t.lanes, 2))
    odds = ', '.join(str(i) for i in range(1, t.lanes, 2))
    return ('__builtin_shufflevector(%s, %s, %s)' % (v, v, evens),
            '__builtin_shufflevector(%s, %s, %s)' % (v, v, odds))


class Madd(Op):
    def __init__(self):
        super().__init__('ZI_OP_MADD', 'madd', lanes_of('s16'), 'madd')

    def result(self, t):
        return t.with_lane('s32', t.lanes // 2)

    def special(self, t):
        r = self.result(t)
        if t.wide:
            return [(AVX2, 'return (%s)_mm256_madd_epi16((__m256i)a, (__m256i)b);' % r.c)]
        return [
            (X86, 'return (%s)_mm_madd_epi16((__m128i)a, (__m128i)b);' % r.c),
            (A64, 'int16x8_t x_ = (int16x8_t)a, y_ = (int16x8_t)b;\n'
                  '    return (%s)vpaddq_s32(vmull_s16(vget_low_s16(x_), vget_low_s16(y_)),\n'
                  '        vmull_s16(vget_high_s16(x_), vget_high_s16(y_)));' % r.c),
            (NEON, 'int16x8_t x_ = (int16x8_t)a, y_ = (int16x8_t)b;\n'
                   '    int32x4_t lo_ = vmull_s16(vget_low_s16(x_), vget_low_s16(y_));\n'
                   '    int32x4_t hi_ = vmull_s16(vget_high_s16(x_), vget_high_s16(y_));\n'
                   '    return (%s)vcombine_s32(vpadd_s32(vget_low_s32(lo_), vget_high_s32(lo_)),\n'
                   '        vpadd_s32(vget_low_s32(hi_), vget_high_s32(hi_)));' % r.c),
            (WASM, 'return (%s)wasm_i32x4_dot_i16x8((v128_t)a, (v128_t)b);' % r.c),
        ]

    def generic(self, t):
        r = self.result(t)
        ae, ao = even_odd(t, 'a')
        be, bo = even_odd(t, 'b')
        return ('%s p_ = (%s)__builtin_convertvector(%s, %s) * (%s)__builtin_convertvector(%s, %s);\n'
                '    %s q_ = (%s)__builtin_convertvector(%s, %s) * (%s)__builtin_convertvector(%s, %s);\n'
                '    return (%s)(p_ + q_);' %
                (r.u.c, r.u.c, ae, r.c, r.u.c, be, r.c, r.u.c, r.u.c, ao, r.c, r.u.c, bo, r.c, r.c))


class MaddSat(Op):
    def __init__(self):
        super().__init__('ZI_OP_MADD_SAT', 'madd_sat', lanes_of('u8'), 'madd_sat')

    def result(self, t):
        return t.with_lane('s16', t.lanes // 2)

    def params(self, t):
        return [(t.c, 'a'), (t.s.c, 'b')]

    def special(self, t):
        r = self.result(t)
        if t.wide:
            return [(AVX2, 'return (%s)_mm256_maddubs_epi16((__m256i)a, (__m256i)b);' % r.c)]
        products = ('uint8x16_t x_ = (uint8x16_t)a;\n    int8x16_t y_ = (int8x16_t)b;\n'
                    '    int16x8_t lo_ = vmulq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(x_))), vmovl_s8(vget_low_s8(y_)));\n'
                    '    int16x8_t hi_ = vmulq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(x_))), vmovl_s8(vget_high_s8(y_)));\n')
        return [
            (SSSE3, 'return (%s)_mm_maddubs_epi16((__m128i)a, (__m128i)b);' % r.c),
            (X86, '__m128i x_ = (__m128i)a, y_ = (__m128i)b;\n'
                  '    __m128i p_ = _mm_mullo_epi16(_mm_and_si128(x_, _mm_set1_epi16(0xff)), _mm_srai_epi16(_mm_slli_epi16(y_, 8), 8));\n'
                  '    __m128i q_ = _mm_mullo_epi16(_mm_srli_epi16(x_, 8), _mm_srai_epi16(y_, 8));\n'
                  '    return (%s)_mm_adds_epi16(p_, q_);' % r.c),
            (A64, products + '    return (%s)vqaddq_s16(vuzp1q_s16(lo_, hi_), vuzp2q_s16(lo_, hi_));' % r.c),
            (NEON, products + '    int16x8x2_t u_ = vuzpq_s16(lo_, hi_);\n'
                              '    return (%s)vqaddq_s16(u_.val[0], u_.val[1]);' % r.c),
            (WASM, 'v128_t x_ = (v128_t)a, y_ = (v128_t)b;\n'
                   '    v128_t p_ = wasm_i16x8_mul(wasm_v128_and(x_, wasm_i16x8_splat(0xff)), wasm_i16x8_shr(wasm_i16x8_shl(y_, 8), 8));\n'
                   '    v128_t q_ = wasm_i16x8_mul(wasm_u16x8_shr(x_, 8), wasm_i16x8_shr(y_, 8));\n'
                   '    return (%s)wasm_i16x8_add_sat(p_, q_);' % r.c),
        ]


class AddPairs(Op):
    def __init__(self):
        super().__init__('ZI_OP_ADD_PAIRS', 'add_pairs', NARROWABLE, 'add_pairs')

    def result(self, t):
        return t.wider()

    def params(self, t):
        return [(t.c, 'a')]

    def special(self, t):
        r = self.result(t)
        out = []
        if t.lane in ('u8', 's8', 's16'):
            if t.lane == 'u8':
                body = '_mm_maddubs_epi16((__m128i)a, _mm_set1_epi8(1))'
                tier = SSSE3
            elif t.lane == 's8':
                body = '_mm_maddubs_epi16(_mm_set1_epi8(1), (__m128i)a)'
                tier = SSSE3
            else:
                body = '_mm_madd_epi16((__m128i)a, _mm_set1_epi16(1))'
                tier = X86
            if t.wide:
                body = body.replace('_mm_', '_mm256_').replace('__m128i', '__m256i')
                return [(AVX2, 'return (%s)%s;' % (r.c, body))]
            out.append((tier, 'return (%s)%s;' % (r.c, body)))
        m, reg = ('_mm256_', '__m256i') if t.wide else ('_mm_', '__m128i')
        tier = AVX2 if t.wide else X86
        # Even lanes masked or sign-extended in place, plus odd lanes shifted down.
        if t.bits == 8 and not t.wide:
            even = ('_mm_and_si128(x_, _mm_set1_epi16(0xff))' if not t.signed else
                    '_mm_srai_epi16(_mm_slli_epi16(x_, 8), 8)')
            odd = '_mm_sr%si_epi16(x_, 8)' % ('a' if t.signed else 'l')
            out.append((X86, '__m128i x_ = (__m128i)a;\n    return (%s)_mm_add_epi16(%s, %s);' % (r.c, even, odd)))
        if t.lane == 'u16':
            out.append((tier, '%s x_ = (%s)a;\n    return (%s)%sadd_epi32(%sand_si%d(x_, %sset1_epi32(0xffff)), %ssrli_epi32(x_, 16));' %
                        (reg, reg, r.c, m, m, t.bytes * 8, m, m)))
        if t.lane == 'u32':
            out.append((tier, '%s x_ = (%s)a;\n    return (%s)%sadd_epi64(%sand_si%d(x_, %sset1_epi64x(0xffffffff)), %ssrli_epi64(x_, 32));' %
                        (reg, reg, r.c, m, m, t.bytes * 8, m, m)))
        if t.lane == 's32':
            # pmuldq by 1 sign-extends lanes 0 and 2.
            one = '%sset1_epi32(1)' % m
            out.append((AVX2 if t.wide else SSE41,
                        '%s x_ = (%s)a, one_ = %s;\n    return (%s)%sadd_epi64(%smul_epi32(x_, one_), %smul_epi32(%ssrli_epi64(x_, 32), one_));' %
                        (reg, reg, one, r.c, m, m, m, m)))
            if not t.wide:
                out.append((X86, '__m128i x_ = (__m128i)a, s_ = _mm_srai_epi32(x_, 31), lo_ = _mm_set1_epi64x(0xffffffff);\n'
                            '    __m128i e_ = _mm_or_si128(_mm_and_si128(x_, lo_), _mm_slli_epi64(s_, 32));\n'
                            '    __m128i o_ = _mm_or_si128(_mm_srli_epi64(x_, 32), _mm_andnot_si128(lo_, s_));\n'
                            '    return (%s)_mm_add_epi64(e_, o_);' % r.c))
        if t.wide:
            return out
        out.append((NEON, 'return (%s)vpaddlq_%s((%s)a);' % (r.c, t.nsuf, t.neon)))
        if t.bits < 32:
            out.append((WASM, 'return (%s)wasm_%s_extadd_pairwise_%s((v128_t)a);' %
                        (r.c, r.wsuf, t.wsuf)))
        elif t.signed:
            out.append((WASM, 'v128_t x_ = (v128_t)a;\n'
                        '    return (%s)wasm_i64x2_add(wasm_i64x2_shr(wasm_i64x2_shl(x_, 32), 32), wasm_i64x2_shr(x_, 32));' % r.c))
        else:
            out.append((WASM, 'v128_t x_ = (v128_t)a;\n'
                        '    return (%s)wasm_i64x2_add(wasm_v128_and(x_, wasm_i64x2_splat(0xffffffff)), wasm_u64x2_shr(x_, 32));' % r.c))
        return out

    def generic(self, t):
        r = self.result(t)
        e, o = even_odd(t, 'a')
        return ('return (%s)((%s)__builtin_convertvector(%s, %s) + (%s)__builtin_convertvector(%s, %s));' %
                (r.c, r.u.c, e, r.c, r.u.c, o, r.c))


class Sad(Op):
    def __init__(self):
        super().__init__('ZI_OP_SAD', 'sad', lanes_of('u8'), 'sad')

    def result(self, t):
        return t.with_lane('u64', t.lanes // 8)

    def special(self, t):
        r = self.result(t)
        if t.wide:
            return [(AVX2, 'return (%s)_mm256_sad_epu8((__m256i)a, (__m256i)b);' % r.c)]
        return [
            (X86, 'return (%s)_mm_sad_epu8((__m128i)a, (__m128i)b);' % r.c),
            (NEON, 'return (%s)vpaddlq_u32(vpaddlq_u16(vpaddlq_u8(vabdq_u8((uint8x16_t)a, (uint8x16_t)b))));' % r.c),
            (WASM, 'v128_t d_ = wasm_v128_or(wasm_u8x16_sub_sat((v128_t)a, (v128_t)b), wasm_u8x16_sub_sat((v128_t)b, (v128_t)a));\n'
                   '    %s s_ = (%s)wasm_u32x4_extadd_pairwise_u16x8(wasm_u16x8_extadd_pairwise_u8x16(d_));\n'
                   '    return (s_ & 0xffffffffu) + (s_ >> 32);' % (r.c, r.c)),
        ]


def first_lane(isa, t, e):
    """The index of the first all-ones lane of 128-bit mask e (one is set)."""
    if isa == X86:
        return '__builtin_ctz((unsigned)_mm_movemask_epi8((__m128i)(%s))) / %d' % (e, t.bits // 8)
    if isa == WASM:
        return '__builtin_ctz((unsigned)wasm_i8x16_bitmask((v128_t)(%s))) / %d' % (e, t.bits // 8)
    # NEON: a nibble per byte.
    return ('__builtin_ctzll(vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16((uint16x8_t)(%s), 4)), 0)) / %d' %
            (e, t.bits // 2))


def tree(t, op, v, isa=None):
    """Lines folding v with ZI_N(op) until every lane holds the result."""
    lines = []
    step = t.lanes // 2
    while step:
        if isa == X86:
            # Whole-lane swaps: GCC scalarizes byte shuffles without SSSE3.
            x = '(__m128i)%s' % v
            swap = {8: '_mm_shuffle_epi32(%s, 0x4e)' % x, 4: '_mm_shuffle_epi32(%s, 0xb1)' % x,
                    2: '_mm_shufflehi_epi16(_mm_shufflelo_epi16(%s, 0xb1), 0xb1)' % x,
                    1: '_mm_or_si128(_mm_slli_epi16(%s, 8), _mm_srli_epi16(%s, 8))' % (x, x)}
            other = '(%s)%s' % (t.c, swap[step * t.bits // 8])
        else:
            perm = ', '.join(str(i ^ step) for i in range(t.lanes))
            other = '__builtin_shufflevector(%s, %s, %s)' % (v, v, perm)
        lines.append('%s = ZI_N(%s_%s)(%s, %s);' % (v, op, t.name, v, other))
        step //= 2
    return lines


def x86_tree_reduce(t, kind):
    """(cond, body) folds of 128-bit t for x86; signed bytes fold in
    unsigned order below SSE4.1, which has no pmaxsb or pminsb. 32-bit
    lanes need SSE4.1's pmaxsd and pmaxud, and 64-bit lanes stay scalar
    (no x86 below AVX-512 has a 64-bit max)."""
    word = 'min' if kind == 'min' else 'max'
    out = []
    if t.bits == 64:
        return out
    forms = [(SSE41 if t.bits == 32 else X86, t, 'a', None)]
    if t.bits == 8 and t.signed:
        u = t.u
        flipped = 'ZI_N(xor_%s)((%s)a, ZI_N(splat_%s)(0x80))' % (u.name, u.c, u.name)
        forms = [(SSE41, t, 'a', None), (X86, u, flipped, '0x80')]
    for cond, tt, x, flip in forms:
        lines = ['%s x_ = %s, m_ = x_;' % (tt.c, x)] + tree(tt, word, 'm_', X86)
        if kind == 'max_index':
            lines.append('return %s;' % first_lane(X86, tt, 'ZI_N(eq_%s)(x_, m_)' % tt.name))
        else:
            at = 'ZI_AT(m_, 0)' if flip is None else '(ZI_AT(m_, 0) ^ %s)' % flip
            lines.append('return (%s)%s;' % (t.ctype, at))
        out.append((cond, '\n    '.join(lines)))
    return out


class Reduce(Op):
    def __init__(self, enum, snake, kind):
        super().__init__(enum, snake, INT, 'reduce')
        self.kind = kind

    def result(self, t):
        return 'int64_t' if self.kind == 'max_index' else t.ctype

    def params(self, t):
        return [(t.c, 'a')]

    wide_specials = True

    def special(self, t):
        if t.wide:
            if self.kind != 'max_index':
                return []
            h = t.half
            m = 'ZI_N(splat_%s)(ZI_N(reduce_max_%s)(a))' % (h.name, t.name)
            halves = '%s lo_ = ZI_N(low_%s)(a), hi_ = ZI_N(high_%s)(a), m_ = %s;\n    ' % (h.c, t.name, t.name, m)
            elo, ehi = 'ZI_N(eq_%s)(lo_, m_)' % h.name, 'ZI_N(eq_%s)(hi_, m_)' % h.name
            # Below AVX2, 32-bit maxima need SSE4.1 and 64-bit ones stay scalar.
            x86_halves = [(X86 if t.bits < 32 else SSE41,
                           halves + 'unsigned k_ = (unsigned)_mm_movemask_epi8((__m128i)%s) |\n'
                           '        (unsigned)_mm_movemask_epi8((__m128i)%s) << 16;\n'
                           '    return __builtin_ctz(k_) / %d;' % (elo, ehi, t.bits // 8))] if t.bits < 64 else []
            return [
                (AVX2, '%s m_ = ZI_N(splat_%s)(ZI_N(reduce_max_%s)(a));\n'
                       '    return __builtin_ctz((unsigned)_mm256_movemask_epi8((__m256i)ZI_N(eq_%s)(a, m_))) / %d;' %
                       (t.c, t.name, t.name, t.name, t.bits // 8)),
            ] + x86_halves + [
                (WASM, halves + 'unsigned k_ = (unsigned)wasm_i8x16_bitmask((v128_t)%s) |\n'
                       '        (unsigned)wasm_i8x16_bitmask((v128_t)%s) << 16;\n'
                       '    return __builtin_ctz(k_) / %d;' % (elo, ehi, t.bits // 8)),
                (NEON, halves + 'uint8x16_t k_ = vcombine_u8(vshrn_n_u16((uint16x8_t)%s, 4), vshrn_n_u16((uint16x8_t)%s, 4));\n'
                       '    uint64_t l_ = vgetq_lane_u64(vreinterpretq_u64_u8(k_), 0), h_ = vgetq_lane_u64(vreinterpretq_u64_u8(k_), 1);\n'
                       '    return l_ ? __builtin_ctzll(l_) / %d : %d + __builtin_ctzll(h_) / %d;' %
                       (elo, ehi, t.bits // 2, t.lanes // 2, t.bits // 2)),
            ]
        out = []
        word = {'min': 'min', 'max': 'max', 'max_index': 'max'}.get(self.kind)
        if t.bits == 16 and self.kind != 'add':
            # phminposuw finds the smallest u16 and its first index; xor maps
            # the wanted order onto it.
            flip = {('min', False): None, ('min', True): '0x8000',
                    ('max', False): '0xffff', ('max', True): '0x7fff'}[(word, t.signed)]
            x = '(__m128i)a' if flip is None else '_mm_xor_si128((__m128i)a, _mm_set1_epi16((short)%s))' % flip
            if self.kind == 'max_index':
                out.append((SSE41, 'return _mm_extract_epi16(_mm_minpos_epu16(%s), 1);' % x))
            else:
                v = '_mm_cvtsi128_si32(_mm_minpos_epu16(%s))' % x
                if flip is not None:
                    v = '(%s ^ %s)' % (v, flip)
                out.append((SSE41, 'return (%s)%s;' % (t.ctype, v)))
        if self.kind != 'add':
            out += x86_tree_reduce(t, self.kind)
            if self.kind == 'max_index' and t.bits >= 32:
                # Straight-line scalar compares beat emulated maxima.
                out.append((X86, self.portable(t)))
        if self.kind == 'add' and t.bits == 8:
            out.append((X86, '__m128i s_ = _mm_sad_epu8((__m128i)a, _mm_setzero_si128());\n'
                        '    return (%s)(_mm_cvtsi128_si32(s_) + _mm_extract_epi16(s_, 4));' % t.ctype))
        # AArch64 reduces across lanes in one instruction.
        if self.kind == 'add':
            out.append((A64, 'return (%s)vaddvq_%s((%s)a);' % (t.ctype, t.nsuf, t.neon)))
        elif t.bits < 64 and self.kind != 'max_index':
            out.append((A64, 'return (%s)v%svq_%s((%s)a);' % (t.ctype, word, t.nsuf, t.neon)))
        elif t.bits < 64:
            out.append((A64, '%s m_ = ZI_N(splat_%s)(ZI_N(reduce_max_%s)(a));\n    return %s;' %
                        (t.c, t.name, t.name, first_lane(NEON, t, 'ZI_N(eq_%s)(a, m_)' % t.name))))
        # 32-bit ARM: pairwise steps on the two halves.
        if t.bits < 64 and self.kind in ('min', 'max'):
            d = t.neon.replace('x%d_t' % t.lanes, 'x%d_t' % (t.lanes // 2))
            lines = ['%s x_ = (%s)a;' % (t.neon, t.neon),
                     '%s d_ = vp%s_%s(vget_low_%s(x_), vget_high_%s(x_));' % (d, word, t.nsuf, t.nsuf, t.nsuf)]
            for _ in range({8: 3, 16: 2, 32: 1}[t.bits]):
                lines.append('d_ = vp%s_%s(d_, d_);' % (word, t.nsuf))
            lines.append('return (%s)vget_lane_%s(d_, 0);' % (t.ctype, t.nsuf))
            out.append((NEON, '\n    '.join(lines)))
        if self.kind == 'add' and t.bits < 64:
            u = t.u
            v = '(%s)a' % u.neon
            bits = t.bits
            while bits < 64:
                v = 'vpaddlq_u%d(%s)' % (bits, v)
                bits *= 2
            out.append((NEON, 'uint64x2_t s_ = %s;\n'
                        '    return (%s)(vgetq_lane_u64(s_, 0) + vgetq_lane_u64(s_, 1));' % (v, t.ctype)))
        if self.kind == 'max_index':
            lines = ['%s m_ = a;' % t.c] + tree(t, 'max', 'm_')
            for isa in (NEON, WASM):
                out.append((isa, '\n    '.join(lines + ['return %s;' % first_lane(isa, t, 'ZI_N(eq_%s)(a, m_)' % t.name)])))
        return out

    def generic(self, t):
        op = {'add': 'add', 'min': 'min', 'max': 'max'}.get(self.kind)
        if t.wide:
            h = t.half
            if op:
                return 'return ZI_N(reduce_%s_%s)(ZI_N(%s_%s)(ZI_N(low_%s)(a), ZI_N(high_%s)(a)));' % (
                    self.kind, h.name, op, h.name, t.name, t.name)
            return None
        lines = ['%s m_ = a;' % t.c] + tree(t, op or 'max', 'm_')
        if op:
            return '\n    '.join(lines + ['return (%s)ZI_AT(m_, 0);' % t.ctype])
        lines.append('%s e_ = ZI_N(eq_%s)(a, m_);' % (t.c, t.name))
        lines.append('for(int i_ = 0; i_ < %d; i_++) if(ZI_AT(e_, i_)) return i_;' % t.lanes)
        lines.append('return 0;')
        return '\n    '.join(lines)

    def portable(self, t):
        if self.kind == 'add':
            return ('%s s_ = 0;\n    for(int i_ = 0; i_ < %d; i_++) s_ = (%s)(s_ + (%s)ZI_AT(a, i_));\n'
                    '    return (%s)s_;' % (t.utype, t.lanes, t.utype, t.utype, t.ctype))
        op = '<' if self.kind == 'min' else '>'
        ret = 'best_' if self.kind == 'max_index' else 'ZI_AT(a, best_)'
        return ('int best_ = 0;\n    for(int i_ = 1; i_ < %d; i_++) if(ZI_AT(a, i_) %s ZI_AT(a, best_)) best_ = i_;\n'
                '    return %s;' % (t.lanes, op, ret))

    splittable = False


class AllZero(Op):
    def __init__(self):
        super().__init__('ZI_OP_ALL_ZERO', 'all_zero', INT, 'all_zero')

    def result(self, t):
        return 'int'

    def params(self, t):
        return [(t.c, 'a')]

    def special(self, t):
        if t.wide:
            return [(AVX2, 'return _mm256_testz_si256((__m256i)a, (__m256i)a);')]
        return [
            (SSE41, 'return _mm_testz_si128((__m128i)a, (__m128i)a);'),
            (X86, 'return _mm_movemask_epi8(_mm_cmpeq_epi8((__m128i)a, _mm_setzero_si128())) == 0xffff;'),
            (A64, 'return vmaxvq_u32((uint32x4_t)a) == 0;'),
            (NEON, 'uint64x2_t v_ = (uint64x2_t)a;\n    return (vgetq_lane_u64(v_, 0) | vgetq_lane_u64(v_, 1)) == 0;'),
            (WASM, 'return !wasm_v128_any_true((v128_t)a);'),
        ]

    def portable(self, t):
        return ('%s any_ = 0;\n    for(int i_ = 0; i_ < %d; i_++) any_ |= (%s)ZI_AT(a, i_);\n    return any_ == 0;' %
                (t.utype, t.lanes, t.utype))

    splittable = False


class ByteShuffle(Op):
    def __init__(self):
        super().__init__('ZI_OP_BYTE_SHUFFLE', 'byte_shuffle', lanes_of('u8', 's8'), 'byte_shuffle')

    # Halves where they shuffle in one instruction; SSE2 loops over all 32.
    split_cond = 'ZI_VEC && !(ZI_X86 && ZI_TIER < 2)'

    def params(self, t):
        return [(t.c, 'a'), (t.u.c, 'b')]

    def special(self, t):
        if t.wide:
            return [(AVX2, 'return (%s)_mm256_shuffle_epi8((__m256i)a, _mm256_adds_epu8((__m256i)b, _mm256_set1_epi8(0x70)));' % t.c)]
        return [
            (SSSE3, 'return (%s)_mm_shuffle_epi8((__m128i)a, _mm_adds_epu8((__m128i)b, _mm_set1_epi8(0x70)));' % t.c),
            (A64, 'return (%s)vqtbl1q_u8((uint8x16_t)a, (uint8x16_t)b);' % t.c),
            (NEON, 'uint8x8x2_t t_ = {{vget_low_u8((uint8x16_t)a), vget_high_u8((uint8x16_t)a)}};\n'
                   '    return (%s)vcombine_u8(vtbl2_u8(t_, vget_low_u8((uint8x16_t)b)), vtbl2_u8(t_, vget_high_u8((uint8x16_t)b)));' % t.c),
            (WASM, 'return (%s)wasm_i8x16_swizzle((v128_t)a, (v128_t)b);' % t.c),
        ]

    def portable(self, t):
        return ('%s r_;\n    for(int i_ = 0; i_ < %d; i_++) {\n'
                '        unsigned index_ = ZI_AT(b, i_);\n'
                '        ZI_AT(r_, i_) = index_ < 16 ? ZI_AT(a, (i_ & ~15) + (int)index_) : 0;\n'
                '    }\n    return r_;' % (t.c, t.lanes))


class Reinterpret(Op):
    def __init__(self):
        super().__init__('ZI_OP_REINTERPRET', 'reinterpret', None, 'reinterpret')


class Convert(Op):
    def __init__(self):
        super().__init__('ZI_OP_CONVERT', 'convert', None, 'convert')


class Half(Op):
    def __init__(self, enum, snake, high):
        super().__init__(enum, snake, [t for t in TYPES if t.wide], 'half')
        self.high = high

    def result(self, t):
        return t.half

    def params(self, t):
        return [(t.c, 'a')]

    def special(self, t):
        # Below AVX2 a 16-byte aligned union keeps GCC's halves in registers;
        # memcpy would move them through the stack in 8-byte pieces, and an
        # under-aligned union makes GCC 14 fold misaligned memory operands
        # into SSE instructions (an internal compiler error).
        u = ('union { %s w; __m128i h[2]; } u_;\n    u_.w = a;\n    return (%s)u_.h[%d];' %
             (t.c, t.half.c, 1 if self.high else 0))
        if self.high:
            return [(AVX2, 'return (%s)_mm256_extracti128_si256((__m256i)a, 1);' % t.half.c), (X86, u)]
        return [(AVX2, 'return (%s)_mm256_castsi256_si128((__m256i)a);' % t.half.c), (X86, u)]

    def portable(self, t):
        return '%s r_;\n    memcpy(&r_, (const char *)&a + %d, 16);\n    return r_;' % (t.half.c, 16 if self.high else 0)

    splittable = False
    wide_specials = True


class Combine(Op):
    def __init__(self):
        super().__init__('ZI_OP_COMBINE', 'combine', [t for t in TYPES if not t.wide], 'combine')

    def result(self, t):
        return t.double

    def special(self, t):
        return [(AVX2, 'return (%s)_mm256_inserti128_si256(_mm256_castsi128_si256((__m128i)a), (__m128i)b, 1);' %
                 t.double.c)]

    def generic(self, t):
        return 'union { %s w; %s h[2]; } u_;\n    u_.h[0] = a;\n    u_.h[1] = b;\n    return u_.w;' % (
            t.double.c, t.c)

    def portable(self, t):
        return ('%s r_;\n    memcpy(&r_, &a, 16);\n    memcpy((char *)&r_ + 16, &b, 16);\n    return r_;' %
                t.double.c)

    splittable = False


class Interleave(Op):
    def __init__(self, enum, snake, high):
        super().__init__(enum, snake, TYPES, 'interleave')
        self.high = high

    def generic(self, t):
        per = t.block
        idx = []
        for block in range(2 if t.wide else 1):
            for i in range(per // 2):
                at = block * per + i + (per // 2 if self.high else 0)
                idx += [at, t.lanes + at]
        return 'return (%s)__builtin_shufflevector(a, b, %s);' % (t.c, ', '.join(map(str, idx)))


def ops():
    add_sub = lambda sub: dict(
        x86_name=lambda t: sat_name(t, sub),
        neon_name=lambda t: 'vq%sq_%s' % ('sub' if sub else 'add', t.nsuf),
        wasm_name=lambda t: 'wasm_%s_%s_sat' % (t.wsuf, 'sub' if sub else 'add'))
    return [
        Lanewise('ZI_OP_ADD', 'add', TYPES, '+'),
        Lanewise('ZI_OP_SUB', 'sub', TYPES, '-'),
        Lanewise('ZI_OP_MUL', 'mul', TYPES, '*'),
        Lanewise('ZI_OP_DIV', 'div', FLOAT, '/'),
        Lanewise('ZI_OP_ADD_SAT', 'add_sat', SMALL, **add_sub(False)),
        Lanewise('ZI_OP_SUB_SAT', 'sub_sat', SMALL, **add_sub(True)),
        Lanewise('ZI_OP_MIN', 'min', TYPES, generic=lambda t: minmax_generic(t, True),
                 specials=minmax_specials(True)),
        Lanewise('ZI_OP_MAX', 'max', TYPES, generic=lambda t: minmax_generic(t, False),
                 specials=minmax_specials(False)),
        Lanewise('ZI_OP_AVG', 'avg', lanes_of('u8', 'u16'),
                 x86_name=lambda t: '_mm_avg_%s' % t.epi,
                 neon_name=lambda t: 'vrhaddq_%s' % t.nsuf,
                 wasm_name=lambda t: 'wasm_%s_avgr' % t.wsuf,
                 generic=lambda t: 'return (a | b) - ((a ^ b) >> 1);'),
        Lanewise('ZI_OP_MUL_HIGH', 'mul_high', lanes_of('u16', 's16'),
                 generic=mulhi_generic, specials=mulhi_specials),
        Lanewise('ZI_OP_MUL_HIGH_ROUND', 'mul_high_round', lanes_of('s16'),
                 generic=mulhrs_generic, specials=mulhrs_specials),
        Lanewise('ZI_OP_AND', 'and', INT, generic=lambda t: 'return a & b;'),
        Lanewise('ZI_OP_OR', 'or', INT, generic=lambda t: 'return a | b;'),
        Lanewise('ZI_OP_XOR', 'xor', INT, generic=lambda t: 'return a ^ b;'),
        Lanewise('ZI_OP_AND_NOT', 'and_not', INT, generic=lambda t: 'return a & ~b;'),
        Unary('ZI_OP_NOT', 'not', INT, lambda t: 'return ~a;'),
        Unary('ZI_OP_ABS', 'abs', lanes_of('s8', 's16', 's32', 's64', 'f32', 'f64'), abs_generic,
              abs_specials),
        Unary('ZI_OP_NEG', 'neg', TYPES, neg_generic),
        Lanewise('ZI_OP_SIGN', 'sign', lanes_of('s8', 's16', 's32', 's64'), generic=sign_generic,
                 specials=sign_specials),
        AbsDiff(),
        Compare('ZI_OP_EQ', 'eq', '=='),
        Compare('ZI_OP_LT', 'lt', '<'),
        Compare('ZI_OP_GT', 'gt', '>'),
        Select(),
        Shift('ZI_OP_SHL', 'shl', 'shl'),
        Shift('ZI_OP_SHR', 'shr', 'shr'),
        Shift('ZI_OP_SHR_ROUND', 'shr_round', 'shr_round'),
        ShiftLanes('ZI_OP_SHL_LANES', 'shl_lanes', True),
        ShiftLanes('ZI_OP_SHR_LANES', 'shr_lanes', False),
        ByteMacro('ZI_OP_SHL_BYTES', 'shl_bytes', 'shl'),
        ByteMacro('ZI_OP_SHR_BYTES', 'shr_bytes', 'shr'),
        ByteMacro('ZI_OP_ALIGN_BYTES', 'align_bytes', 'align'),
        Make(), Splat(), Zero(), Lane(), WithLane(),
        Memory('ZI_OP_LOAD', 'load', False, False),
        Memory('ZI_OP_LOAD_LOW', 'load_low', False, True),
        Memory('ZI_OP_STORE', 'store', True, False),
        Memory('ZI_OP_STORE_LOW', 'store_low', True, True),
        Gather(),
        WidenHalf('ZI_OP_WIDEN_LOW', 'widen_low', False),
        WidenHalf('ZI_OP_WIDEN_HIGH', 'widen_high', True),
        Widen(),
        Narrow(),
        MulWide('ZI_OP_MUL_WIDE_LOW', 'mul_wide_low', False),
        MulWide('ZI_OP_MUL_WIDE_HIGH', 'mul_wide_high', True),
        Madd(), MaddSat(), AddPairs(), Sad(),
        Reduce('ZI_OP_REDUCE_ADD', 'reduce_add', 'add'),
        Reduce('ZI_OP_REDUCE_MIN', 'reduce_min', 'min'),
        Reduce('ZI_OP_REDUCE_MAX', 'reduce_max', 'max'),
        Reduce('ZI_OP_REDUCE_MAX_INDEX', 'reduce_max_index', 'max_index'),
        AllZero(), ByteShuffle(),
        Reinterpret(), Convert(),
        Half('ZI_OP_LOW', 'low', False), Half('ZI_OP_HIGH', 'high', True), Combine(),
        Interleave('ZI_OP_INTERLEAVE_LOW', 'interleave_low', False),
        Interleave('ZI_OP_INTERLEAVE_HIGH', 'interleave_high', True),
    ]


# ---------------------------------------------------------------- lane arrays

# Builds without native vectors (TinyCC under `ziran run`, ZIRAN_SIMD_SCALAR
# and compilers without __builtin_shufflevector) compute each helper over its
# own lane type, one statement per lane: TinyCC indexes a loop's lanes
# through a counter in memory, but addresses a fixed lane directly.

def calc(t):
    """The unsigned type t's integer lanes compute in, past int promotion."""
    return 'uint64_t' if t.bits == 64 else 'uint32_t'


def lane(v, i):
    return 'ZI_AT(%s, %d)' % (v, i)


def per_lane(r, expr=None, stmt=None, setup=()):
    """A body setting each lane i of r_ (type r) to expr(i), or running stmt(i)."""
    lines = list(setup) + ['%s r_;' % r.c]
    for i in range(r.lanes):
        lines.append(stmt(i) if stmt else 'ZI_AT(r_, %d) = %s;' % (i, expr(i)))
    lines.append('return r_;')
    return '\n    '.join(lines)


def block_lane(t, r, j, offset):
    """The lane of t that result lane j of r reads, block by block."""
    blocks = 2 if t.wide else 1
    per, rper = t.lanes // blocks, r.lanes // blocks
    return (j // rper) * per + j % rper + offset


def clamp_lane(v, lo, hi, ctype):
    return ('{ int v_ = %s; ZI_AT(r_, %%d) = (%s)(v_ < %d ? %d : v_ > %d ? %d : v_); }' %
            (v, ctype, lo, lo, hi, hi))


def scalar_body(op, t):
    """op's lane-array body for t, or None where op.portable already is one."""
    e, r = op.enum, op.result(t)
    ct, w, W = t.ctype, calc(t), t.bits
    a = lambda i: lane('a', i)
    b = lambda i: lane('b', i)
    if e in ('ZI_OP_ADD', 'ZI_OP_SUB', 'ZI_OP_MUL', 'ZI_OP_DIV'):
        if t.float:
            return per_lane(t, lambda i: '%s %s %s' % (a(i), op.c_op, b(i)))
        return per_lane(t, lambda i: '(%s)((%s)%s %s (%s)%s)' % (ct, w, a(i), op.c_op, w, b(i)))
    if e in ('ZI_OP_ADD_SAT', 'ZI_OP_SUB_SAT'):
        hi = (1 << (W - 1)) - 1 if t.signed else (1 << W) - 1
        lo = -(1 << (W - 1)) if t.signed else 0
        c = '+' if e == 'ZI_OP_ADD_SAT' else '-'
        return per_lane(t, stmt=lambda i: clamp_lane('(int)%s %s (int)%s' % (a(i), c, b(i)), lo, hi, ct) % i)
    if e in ('ZI_OP_MIN', 'ZI_OP_MAX'):
        # a < b ? a : b, so floats take b for NaN and for two zeros.
        c = '<' if e == 'ZI_OP_MIN' else '>'
        return per_lane(t, lambda i: '%s %s %s ? %s : %s' % (a(i), c, b(i), a(i), b(i)))
    if e == 'ZI_OP_AVG':
        return per_lane(t, lambda i: '(%s)(((uint32_t)%s + (uint32_t)%s + 1) >> 1)' % (ct, a(i), b(i)))
    if e == 'ZI_OP_MUL_HIGH':
        p = 'int32_t' if t.signed else 'uint32_t'
        return per_lane(t, lambda i: '(%s)(((%s)%s * (%s)%s) >> 16)' % (ct, p, a(i), p, b(i)))
    if e == 'ZI_OP_MUL_HIGH_ROUND':
        return per_lane(t, lambda i: '(%s)(((int32_t)%s * (int32_t)%s + 0x4000) >> 15)' % (ct, a(i), b(i)))
    bitwise = {'ZI_OP_AND': '&', 'ZI_OP_OR': '|', 'ZI_OP_XOR': '^', 'ZI_OP_AND_NOT': '& ~'}
    if e in bitwise:
        return per_lane(t, lambda i: '(%s)(%s %s%s)' % (ct, a(i), bitwise[e], b(i)))
    if e == 'ZI_OP_NOT':
        return per_lane(t, lambda i: '(%s)~%s' % (ct, a(i)))
    if e in ('ZI_OP_ABS', 'ZI_OP_NEG') and t.float:
        # The sign bit of each lane's bits.
        sign = '0x80000000u' if W == 32 else 'UINT64_C(0x8000000000000000)'
        change = '&= ~' if e == 'ZI_OP_ABS' else '^= '
        lines = ['union { %s f; %s u; } v_;' % (t.c, t.u.c), 'v_.f = a;']
        lines += ['ZI_AT(v_.u, %d) %s%s;' % (i, change, sign) for i in range(t.lanes)]
        lines.append('return v_.f;')
        return '\n    '.join(lines)
    if e == 'ZI_OP_ABS':
        return per_lane(t, lambda i: '(%s)(%s < 0 ? (%s)0 - (%s)%s : (%s)%s)' % (ct, a(i), w, w, a(i), w, a(i)))
    if e == 'ZI_OP_NEG':
        return per_lane(t, lambda i: '(%s)((%s)0 - (%s)%s)' % (ct, w, w, a(i)))
    if e == 'ZI_OP_SIGN':
        return per_lane(t, lambda i: '(%s)(%s < 0 ? (%s)0 - (%s)%s : %s == 0 ? (%s)0 : (%s)%s)' %
                        (ct, b(i), w, w, a(i), b(i), w, w, a(i)))
    if e == 'ZI_OP_ABS_DIFF':
        return per_lane(r, lambda i: '(%s)(%s < %s ? (%s)%s - (%s)%s : (%s)%s - (%s)%s)' %
                        (r.ctype, a(i), b(i), w, b(i), w, a(i), w, a(i), w, b(i)))
    if isinstance(op, Compare):
        return per_lane(r, lambda i: '(%s)-(%s %s %s)' % (r.ctype, a(i), op.c_op, b(i)))
    if e == 'ZI_OP_SELECT':
        m = lambda i: lane('m', i)
        if t.float:
            lines = ['union { %s f; %s u; } x_, y_, o_;' % (t.c, t.u.c), 'x_.f = a;', 'y_.f = b;']
            lines += ['ZI_AT(o_.u, %d) = (%s & ZI_AT(x_.u, %d)) | (~%s & ZI_AT(y_.u, %d));' %
                      (i, m(i), i, m(i), i) for i in range(t.lanes)]
            lines.append('return o_.f;')
            return '\n    '.join(lines)
        return per_lane(t, lambda i: '(%s)((%s & (%s)%s) | (~%s & (%s)%s))' %
                        (ct, m(i), t.utype, a(i), m(i), t.utype, b(i)))
    if isinstance(op, Shift):
        # A count of the lane width or more gives 0, or the sign for a
        # signed ShiftRight. No branches, so TinyCC runs these as macros.
        if op.kind == 'shr' and t.signed:
            return per_lane(t, lambda i: '(%s)(%s >> s_)' % (ct, a(i)),
                            setup=['int s_ = n >= %d ? %d : (int)n;' % (W, W - 1)])
        if op.kind in ('shl', 'shr'):
            setup = ['int s_ = n < %d ? (int)n : 0;' % W, '%s h_ = n < %d ? ~(%s)0 : 0;' % (w, W, w)]
            if op.kind == 'shl':
                return per_lane(t, lambda i: '(%s)(((%s)%s << s_) & h_)' % (ct, w, a(i)), setup=setup)
            return per_lane(t, lambda i: '(%s)(((%s)%s >> s_) & h_)' % (ct, w, a(i)), setup=setup)
        # (v >> n) plus the last bit shifted out; a count of 0 shifts out
        # nothing.
        if t.signed:
            setup = ['int s_ = n >= %d ? %d : (int)n, t_ = n == 0 ? 0 : n - 1 >= %d ? %d : (int)(n - 1);' %
                     (W, W - 1, W, W - 1), '%s q_ = n != 0;' % w]
            return per_lane(t, lambda i: '(%s)((%s)(%s >> s_) + (%s)((%s >> t_) & q_))' % (ct, w, a(i), w, a(i)),
                            setup=setup)
        setup = ['uint64_t m_ = n - 1;',
                 'int s_ = n < %d ? (int)n : 0, t_ = m_ < %d ? (int)m_ : 0;' % (W, W),
                 '%s h_ = n < %d ? ~(%s)0 : 0, q_ = m_ < %d ? 1 : 0;' % (w, W, w, W)]
        return per_lane(t, lambda i: '(%s)((((%s)%s >> s_) & h_) + (((%s)%s >> t_) & q_))' % (ct, w, a(i), w, a(i)),
                        setup=setup)
    if isinstance(op, ShiftLanes):
        c = lambda i: '(%s)%s' % (t.utype, b(i))
        if op.left:
            return per_lane(t, lambda i: '(%s)(%s >= %d ? 0 : (%s)%s << %s)' % (ct, c(i), W, w, a(i), c(i)))
        if not t.signed:
            return per_lane(t, lambda i: '(%s)(%s >= %d ? 0 : %s >> %s)' % (ct, c(i), W, a(i), c(i)))
        return per_lane(t, lambda i: '(%s)(%s >> (%s >= %d ? %d : %s))' % (ct, a(i), c(i), W, W - 1, c(i)))
    if e == 'ZI_OP_SPLAT':
        return per_lane(t, lambda i: 'x')
    if e == 'ZI_OP_ZERO':
        return per_lane(t, lambda i: '0')
    if isinstance(op, WidenHalf):
        offset = t.block // 2 if op.high else 0
        return per_lane(r, lambda j: '(%s)%s' % (r.ctype, a(block_lane(t, r, j, offset))))
    if e == 'ZI_OP_WIDEN':
        return per_lane(r, lambda i: '(%s)%s' % (r.ctype, a(i)))
    if isinstance(op, MulWide):
        offset = t.block // 2 if op.high else 0
        rc = r.ctype
        return per_lane(r, lambda j: '(%s)((%s)%s * (%s)%s)' % (
            rc, rc, a(block_lane(t, r, j, offset)), rc, b(block_lane(t, r, j, offset))))
    if e == 'ZI_OP_MADD':
        return per_lane(r, lambda i: '(int32_t)((uint32_t)((int32_t)%s * %s) + (uint32_t)((int32_t)%s * %s))' %
                        (a(2 * i), b(2 * i), a(2 * i + 1), b(2 * i + 1)))
    if e == 'ZI_OP_MADD_SAT':
        return per_lane(r, stmt=lambda i: clamp_lane('(int)%s * %s + (int)%s * %s' % (
            a(2 * i), b(2 * i), a(2 * i + 1), b(2 * i + 1)), -32768, 32767, 'int16_t') % i)
    if e == 'ZI_OP_ADD_PAIRS':
        rc = r.ctype
        return per_lane(r, lambda i: '(%s)((%s)%s + (%s)%s)' % (rc, rc, a(2 * i), rc, a(2 * i + 1)))
    if e == 'ZI_OP_SAD':
        def total(i):
            terms = ['(%s > %s ? %s - %s : %s - %s)' % (a(k), b(k), a(k), b(k), b(k), a(k))
                     for k in range(8 * i, 8 * i + 8)]
            return '(uint64_t)(%s)' % '\n        + '.join(terms)
        return per_lane(r, total)
    if isinstance(op, Reduce):
        if op.kind == 'add':
            terms = ['(%s)%s' % (w, a(i)) for i in range(t.lanes)]
            return 'return (%s)(%s);' % (ct, ' +\n        '.join(
                ' + '.join(terms[i:i + 4]) for i in range(0, len(terms), 4)))
        if op.kind == 'max_index':
            lines = ['%s m_ = %s;' % (ct, a(0)), 'int best_ = 0;']
            lines += ['if(%s > m_) { m_ = %s; best_ = %d; }' % (a(i), a(i), i) for i in range(1, t.lanes)]
            return '\n    '.join(lines + ['return best_;'])
        c = '<' if op.kind == 'min' else '>'
        lines = ['%s m_ = %s;' % (ct, a(0))]
        lines += ['if(%s %s m_) m_ = %s;' % (a(i), c, a(i)) for i in range(1, t.lanes)]
        return '\n    '.join(lines + ['return m_;'])
    if e == 'ZI_OP_ALL_ZERO':
        return 'return (%s) == 0;' % ' |\n        '.join(
            ' | '.join(a(i) for i in range(j, min(j + 8, t.lanes))) for j in range(0, t.lanes, 8))
    if e == 'ZI_OP_BYTE_SHUFFLE':
        return per_lane(t, lambda i: '%s < 16 ? ZI_AT(a, %d + %s) : 0' % (b(i), i & ~15, b(i)))
    if isinstance(op, Half):
        return 'union { %s w; %s h[2]; } u_;\n    u_.w = a;\n    return u_.h[%d];' % (
            t.c, t.half.c, 1 if op.high else 0)
    if isinstance(op, Combine):
        return op.generic(t)
    if isinstance(op, Interleave):
        offset = t.block // 2 if op.high else 0
        def pick(j):
            k = j % t.block
            at = (j // t.block) * t.block + k // 2 + offset
            return (b if k % 2 else a)(at)
        return per_lane(t, pick)
    return None


def narrow_scalar(t, r):
    """Each lane clamped to r's range, block by block: a's lanes, then b's."""
    hi = (1 << (r.bits - 1)) - 1 if r.signed else (1 << r.bits) - 1
    lo = -(1 << (r.bits - 1)) if r.signed else 0
    if t.bits == 64:
        literal = 'INT64_C(%d)' if t.signed else 'UINT64_C(%d)'
    else:
        literal = '%d' if t.signed or t.bits < 32 else '%du'
    high, low = literal % hi, literal % lo
    rper = r.block

    def pick(j):
        k = j % rper
        v = lane('a' if k < t.block else 'b', (j // rper) * t.block + k % t.block)
        if t.signed:
            return '(%s)(%s > %s ? %s : %s < %s ? %s : %s)' % (r.ctype, v, high, high, v, low, low, v)
        return '(%s)(%s > %s ? %s : %s)' % (r.ctype, v, high, high, v)
    return per_lane(r, pick)


def convert_scalar(t, r):
    """Integers to floats rounded; floats truncated, saturated, NaN as 0."""
    if not t.float:
        return per_lane(r, lambda i: '(%s)%s' % (r.ctype, lane('a', i)))
    f = 'f' if t.bits == 32 else ''
    if r.signed:
        limit = ('2147483648.0' if r.bits == 32 else '9223372036854775808.0') + f
        stmt = lambda i: ('{ %s v_ = %s; ZI_AT(r_, %d) = v_ != v_ ? 0 : v_ >= %s ? INT%d_MAX : '
                          'v_ <= -%s ? INT%d_MIN : (%s)v_; }' %
                          (t.ctype, lane('a', i), i, limit, r.bits, limit, r.bits, r.ctype))
    else:
        limit = ('4294967296.0' if r.bits == 32 else '18446744073709551616.0') + f
        stmt = lambda i: ('{ %s v_ = %s; ZI_AT(r_, %d) = v_ != v_ || v_ <= 0 ? 0 : v_ >= %s ? UINT%d_MAX : (%s)v_; }' %
                          (t.ctype, lane('a', i), i, limit, r.bits, r.ctype))
    return per_lane(r, stmt=stmt)


def reinterpret_bodies(t, r):
    return [('ZI_VEC', 'return (%s)a;' % r.c),
            ('ZI_LE', 'union { %s a; %s r; } u_;\n    u_.a = a;\n    return u_.r;' % (t.c, r.c)),
            (None, 'unsigned char x_[%d];\n    zi_le_bytes_%s(a, x_);\n    return zi_le_lanes_%s(x_);' %
             (t.bytes, t.name, r.name))]


# ---------------------------------------------------------------- TinyCC

# TinyCC inlines nothing: a helper function costs a call, with its vectors
# copied in and out. Every helper is a statement-expression macro there
# instead, evaluating each argument once as a call does. On x86-64 the
# common integer helpers are one asm statement that reads each vector
# argument in place through a memory operand and writes the result into a
# local the expression yields, which TinyCC reads in place again; TinyCC
# never reuses a finished block's stack slots. The rest run their
# lane-array C on argument locals. Macro temporaries are spelled __zi_*,
# which C reserves for the implementation and the C emitter never gives a
# program's value, so no argument can name one.
# TinyCC's assembler knows the SSE2 integer instructions MMX had, pshufd and
# pmuludq (not movdqu, punpcklqdq or the byte shifts), so these run one
# instruction per 128-bit block. Each block loads as the two 64-bit halves
# TinyCC itself stores, which keeps store-to-load forwarding, and stores
# whole, which a later load of either half still forwards from.
# Every operation still crosses memory. On Zen 5 a general register stored
# to the stack and loaded back costs about a cycle, but an xmm register
# about ten, so a dependent chain costs 13 to 20 cycles an operation:
# four-lane code with long chains can trail its scalar form under TinyCC.
# TinyCC encodes pavgb and pavgw for MMX registers whatever the operands
# say, and MMX code leaves the x87 state TinyCC's unsigned 64-bit
# conversions use unusable, so Avg keeps its C.
TCC = 'ZI_TCC'
TCC_X64 = 'ZI_TCC_X64'
TX_BITS = {8: 'b', 16: 'w', 32: 'd', 64: 'q'}


def tx_asm(text, outputs, inputs):
    """An asm statement running text, with X0.. for %xmm0..; %r10 holds
    the address of the memory operand being read or written."""
    asm = '\\n\\t'.join(text)
    for k in range(8):
        asm = asm.replace('X%d' % k, '%%%%xmm%d' % k)
    return '__asm__("%s" : %s : %s : "r10", "memory")' % (asm, ', '.join(outputs), ', '.join(inputs))


def tx_macro(params, r, sources, steps, scalars=(), store=None, out=0, store_low=False, locals_=()):
    """Load each source parameter's blocks into %xmm0.. (a vector's through
    its memory operand, a pointer's through the pointer), run steps (S0..
    for the scalar parameters' registers) and store %xmm{out} into the
    result of type r, or through the pointer parameter `store` when r is
    None, block by block; store_low stores the low 64 bits alone. Scalars
    and pointers convert to their parameter types first, as a call's
    arguments do; locals_ are (type, name, value) scalars computed first,
    which steps can name as scalars too."""
    types = dict((n, c) for c, n in list(params) + [(c, n) for c, n, _v in locals_])
    vector = lambda n: types[n].startswith('zi_')
    wide = not store_low and ((r is not None and r.wide) or
                              any(vector(n) and BY_NAME[types[n][3:]].wide for n in sources))
    outputs = ['"=m"(__zi_r)'] if r is not None else []
    inputs, index = [], {}
    for n in list(sources) + list(scalars) + ([store] if store else []):
        if n not in index:
            index[n] = len(outputs) + len(inputs)
            inputs.append('"m"(%s)' % n if vector(n) else '"r"((%s)(%s))' % (types[n], n))
    text = []

    def base(n):
        if not vector(n):
            return '%%%d' % index[n]
        text.append('lea %%%d, %%%%r10' % index[n])
        return '%%r10'
    for block in range(2 if wide else 1):
        at = 16 * block
        for j, n in enumerate(sources):
            reg = base(n)
            text.append('movq %d(%s), X%d' % (at, reg, j))
            if not store_low:
                text.append('movhps %d(%s), X%d' % (at + 8, reg, j))
        for step in steps:
            for k, n in enumerate(scalars):
                step = step.replace('S%d' % k, '%%%d' % index[n])
            text.append(step)
        if r is not None:
            text.append('lea %0, %%r10')
        reg = '%%r10' if r is not None else '%%%d' % index[store]
        text.append('%s X%d, %d(%s)' % ('movq' if store_low else 'movups', out, at, reg))
    asm = tx_asm(text, outputs, inputs)
    decls = ''.join('%s %s = %s; ' % (c, n, v) for c, n, v in locals_)
    if r is None:
        return '__extension__ ({ %s%s; })' % (decls, asm)
    return '__extension__ ({ %s%s __zi_r; %s; __zi_r; })' % (decls, r.c, asm)


def tcc_lane_macro(ret, params, body):
    """A lane-array body as a statement expression: each argument is
    evaluated once into a local of its parameter type, as a call's are, and
    the body runs on those, its final return becoming the value. None for a
    body of another shape."""
    if '#' in body or '"' in body or '//' in body or body.count('return') != (ret != 'void'):
        return None
    text = ' '.join(line.strip() for line in body.split('\n'))
    if ret != 'void':
        at = text.find('return ')
        if at < 0 or (at > 0 and text[at - 1] not in ' ;}') or not text.endswith(';') or \
                '}' in text[at:] or ';' in text[at:-1]:
            return None
        text = text[:at] + text[at + len('return '):]
    for _c, n in params:
        if re.search(r'(\.|->)%s\b|\b__zi_%s\b' % (n, n), text):
            return None
        text = re.sub(r'\b%s\b' % n, '__zi_%s' % n, text)
    decls = ' '.join('%s __zi_%s = (%s);' % (c, n, n) for c, n in params)
    return '__extension__ ({ %s %s })' % (decls, text) if decls else '__extension__ ({ %s })' % text


def tcc_macro(op, t, ret, params, bodies):
    """The TinyCC macro body for a helper on any target. TinyCC copies a
    structure at any address, in place or through memmove, so the memory
    helpers copy through structures of bytes rather than calling memcpy."""
    if isinstance(op, Reinterpret):
        return '__extension__ ({ union { %s from; %s to; } __zi_u; __zi_u.from = (a); __zi_u.to; })' % (t.c, ret.c)
    if isinstance(op, Memory):
        # LoadLow and StoreLow move the low 64 bits whatever the width.
        block = 'struct __zi_m { unsigned char b[%d]; };' % (8 if op.low else t.bytes)
        if op.store:
            return ('__extension__ ({ %s void *__zi_p = (p); union { %s v; struct __zi_m m; } __zi_u; '
                    '__zi_u.v = (a); *(struct __zi_m *)__zi_p = __zi_u.m; })' % (block, t.c))
        return ('__extension__ ({ %s const void *__zi_p = (p); union { %s v; struct __zi_m m; } __zi_u%s; '
                '__zi_u.m = *(const struct __zi_m *)__zi_p; __zi_u.v; })' % (block, t.c, ' = {0}' if op.low else ''))
    if isinstance(op, Gather):
        lane = '__zi_r.m[%d] = *(const struct __zi_m *)(__zi_p + 4 * (int64_t)ZI_AT(__zi_a, %d));'
        return ('__extension__ ({ struct __zi_m { unsigned char b[4]; }; const char *__zi_p = (const char *)(p); '
                '%s __zi_a = (a); union { %s v; struct __zi_m m[%d]; } __zi_r; %s __zi_r.v; })' %
                (t.c, ret.c, t.lanes, ' '.join(lane % (i, i) for i in range(t.lanes))))
    # TinyCC builds have no native vectors, so every other condition is
    # false there and the final body runs.
    return tcc_lane_macro(ret.c if isinstance(ret, T) else ret, params, bodies[-1][1])


def tcc_x64_macro(op, t, ret, params):
    """The TinyCC x86-64 macro body for a helper, or None to keep its
    tcc_macro."""
    asm = tcc_x64_asm(op, t, ret, params)
    if asm is not None:
        return asm
    if isinstance(op, Gather):
        # TinyCC keeps no type-based aliasing, so lanes load straight from
        # the table rather than through memcpy, which it calls, and pairs of
        # them are stored as 64-bit words, which the next operation's 64-bit
        # loads take from the store buffer.
        lane = lambda i: '(uint64_t)*(const uint32_t *)(__zi_p + 4 * (int64_t)ZI_AT(__zi_a, %d))' % i
        words = ' '.join('__zi_r.w[%d] = %s | %s << 32;' % (k, lane(2 * k), lane(2 * k + 1))
                         for k in range(t.lanes // 2))
        return ('__extension__ ({ const char *__zi_p = (const char *)(p); %s __zi_a = (a); '
                'union { %s v; uint64_t w[%d]; } __zi_r; %s __zi_r.v; })' % (t.c, ret.c, t.lanes // 2, words))
    return None


def tcc_x64_asm(op, t, ret, params):
    """The TinyCC x86-64 assembly macro for a helper, or None."""
    if isinstance(op, Narrow):
        return tcc_x64_narrow(t, ret, params)
    if isinstance(op, (Convert, Gather, Reinterpret)):
        return None
    e, r, W = op.enum, op.result(t), t.bits
    if t.float and e not in ('ZI_OP_SELECT', 'ZI_OP_LOAD', 'ZI_OP_LOAD_LOW', 'ZI_OP_STORE',
                             'ZI_OP_STORE_LOW', 'ZI_OP_INTERLEAVE_LOW', 'ZI_OP_INTERLEAVE_HIGH'):
        return None
    x = TX_BITS[W]
    body = lambda sources, steps, **kw: tx_macro(params, r, sources, steps, **kw)
    one = {
        'ZI_OP_ADD': W < 64 and 'padd' + x,
        'ZI_OP_SUB': W < 64 and 'psub' + x,
        'ZI_OP_MUL': W == 16 and 'pmullw',
        'ZI_OP_MUL_HIGH': W == 16 and t.signed and 'pmulhw',
        'ZI_OP_AND': 'pand', 'ZI_OP_OR': 'por', 'ZI_OP_XOR': 'pxor',
        'ZI_OP_EQ': W < 64 and 'pcmpeq' + x,
        'ZI_OP_GT': W < 64 and t.signed and 'pcmpgt' + x,
        'ZI_OP_MADD': 'pmaddwd',
    }
    if W < 32:
        sat = ('s' if t.signed else 'us') + x
        one['ZI_OP_ADD_SAT'] = 'padd' + sat
        one['ZI_OP_SUB_SAT'] = 'psub' + sat
    if (W == 16 and t.signed) or (W == 8 and not t.signed):
        kind = 's' if t.signed else 'u'
        one['ZI_OP_MIN'] = 'pmin%s%s' % (kind, x)
        one['ZI_OP_MAX'] = 'pmax%s%s' % (kind, x)
    if one.get(e):
        return body('ab', ['%s X1, X0' % one[e]])
    if e == 'ZI_OP_LT' and W < 64 and t.signed:
        return body('ba', ['pcmpgt%s X1, X0' % x])
    if e == 'ZI_OP_AND_NOT':
        return body('ab', ['pandn X0, X1'], out=1)
    if e == 'ZI_OP_NOT':
        return body('a', ['pcmpeqd X1, X1', 'pxor X1, X0'])
    if e == 'ZI_OP_NEG' and W < 64:
        return body('a', ['pxor X1, X1', 'psub%s X0, X1' % x], out=1)
    if e == 'ZI_OP_ABS' and W == 16:
        return body('a', ['pxor X1, X1', 'psubw X0, X1', 'pmaxsw X1, X0'])
    if e == 'ZI_OP_ABS' and W < 64:
        # (a ^ m) - m with m the sign mask.
        return body('a', ['pxor X1, X1', 'pcmpgt%s X0, X1' % x, 'pxor X1, X0', 'psub%s X1, X0' % x])
    if e in ('ZI_OP_MIN', 'ZI_OP_MAX') and W == 16:
        # Unsigned: max = (a -sat b) + b, min = a - (a -sat b).
        if e == 'ZI_OP_MAX':
            return body('ab', ['psubusw X1, X0', 'paddw X1, X0'])
        return body('aba', ['psubusw X1, X2', 'psubw X2, X0'])
    if e in ('ZI_OP_MIN', 'ZI_OP_MAX') and t.signed and W in (8, 32):
        # m = a > b (Max) or b > a (Min) picks a: (a & m) | (b & ~m).
        mask = ['pcmpgt%s X1, X2' % x] if e == 'ZI_OP_MAX' else ['pcmpgt%s X0, X2' % x]
        return body('aba' if e == 'ZI_OP_MAX' else 'abb', mask + ['pand X2, X0', 'pandn X1, X2', 'por X2, X0'])
    if e == 'ZI_OP_SELECT':
        return body('mab', ['pand X0, X1', 'pandn X2, X0', 'por X1, X0'])
    if e == 'ZI_OP_MUL' and W == 32:
        # The even lanes' 64-bit products and the odd lanes', their low
        # halves interleaved back.
        return body('ab', ['pshufd $0xf5, X0, X2', 'pshufd $0xf5, X1, X3', 'pmuludq X1, X0',
                           'pmuludq X3, X2', 'pshufd $0x08, X0, X0', 'pshufd $0x08, X2, X2', 'punpckldq X2, X0'])
    if isinstance(op, Shift) and op.kind == 'shr_round' and W in (16, 32):
        # a >> n plus bit n - 1 of a. A count past the lane width shifts in
        # the sign, or zeros, as Ziran's does, and that bit is then the top
        # one (signed) or none; a count of 64 clears it when n is 0.
        if t.signed:
            bit = '__zi_n == 0 ? 64 : __zi_n - 1 < %d ? __zi_n - 1 : %d' % (W - 1, W - 1)
        else:
            bit = '__zi_n - 1 < %d ? __zi_n - 1 : 64' % W
        return body('aa', ['movq S0, X2', '%s X2, X0' % (('psra' if t.signed else 'psrl') + x),
                           'movq S1, X2', 'psrl%s X2, X1' % x, 'pcmpeq%s X2, X2' % x,
                           'psrl%s $%d, X2' % (x, W - 1), 'pand X2, X1', 'padd%s X1, X0' % x],
                    scalars=('__zi_n', '__zi_t'),
                    locals_=(('uint64_t', '__zi_n', '(n)'), ('uint64_t', '__zi_t', bit)))
    if isinstance(op, WidenHalf) and not t.wide:
        # Each lane interleaved with zeros, or with its sign: 0 > a.
        sign = ['pcmpgt%s X0, X1' % x] if t.signed else []
        return body('a', ['pxor X1, X1'] + sign +
                    ['punpck%s%s X1, X0' % ('h' if op.high else 'l', {8: 'bw', 16: 'wd', 32: 'dq'}[W])])
    if isinstance(op, Shift) and op.kind != 'shr_round' and W > 8:
        if op.kind == 'shl':
            name = 'psll' + x
        elif not t.signed:
            name = 'psrl' + x
        elif W < 64:
            name = 'psra' + x
        else:
            return None
        # A count past the lane width gives 0, or the sign, as Ziran's does.
        return body('a', ['movq S0, X1', '%s X1, X0' % name], scalars='n')
    if e in ('ZI_OP_INTERLEAVE_LOW', 'ZI_OP_INTERLEAVE_HIGH') and W < 64:
        return body('ab', ['punpck%s%s X1, X0' % ('l' if e == 'ZI_OP_INTERLEAVE_LOW' else 'h',
                                                   {8: 'bw', 16: 'wd', 32: 'dq'}[W])])
    if e == 'ZI_OP_LOAD':
        return body('p', [])
    if e == 'ZI_OP_STORE':
        return tx_macro(params, None, 'a', [], store='p')
    if e == 'ZI_OP_STORE_LOW':
        return tx_macro(params, None, 'a', [], store='p', store_low=True)
    if e == 'ZI_OP_LOAD_LOW':
        # 64 bits into a zeroed vector.
        text = ['lea %0, %%r10', 'movq (%1), X0', 'movq X0, (%%r10)', 'pxor X0, X0', 'movq X0, 8(%%r10)']
        if t.wide:
            text += ['movq X0, 16(%%r10)', 'movq X0, 24(%%r10)']
        return '__extension__ ({ %s __zi_r; %s; __zi_r; })' % (
            r.c, tx_asm(text, ['"=m"(__zi_r)'], ['"r"((const void *)(p))']))
    if e in ('ZI_OP_SPLAT', 'ZI_OP_ZERO') and not t.float:
        # Every 64-bit word holds the same lanes.
        words = t.bytes // 8
        if e == 'ZI_OP_ZERO':
            w = '0'
        else:
            w = '(uint64_t)(%s)(%s)(x) * UINT64_C(0x%s)' % (t.utype, t.ctype, ('0' * (W // 4 - 1) + '1') * (64 // W))
        return '__extension__ ({ union { %s v; uint64_t w[%d]; } __zi_r; uint64_t __zi_w = %s; %s __zi_r.v; })' % (
            t.c, words, w, ' '.join('__zi_r.w[%d] = __zi_w;' % i for i in range(words)))
    return None


def tcc_x64_narrow(t, r, params):
    """TinyCC x86-64 assembly for a 128-bit Narrow from signed lanes, or
    None."""
    if t.wide or not t.signed:
        return None
    body = lambda sources, steps: tx_macro(params, r, sources, steps)
    packs = {('s16', 's8'): 'packsswb', ('s16', 'u8'): 'packuswb', ('s32', 's16'): 'packssdw'}
    if (t.lane, r.lane) in packs:
        return body('ab', ['%s X1, X0' % packs[(t.lane, r.lane)]])
    if (t.lane, r.lane) == ('s32', 'u16'):
        # SSE2 packs only to signed words: negative lanes become 0, the
        # rest less 32768 saturate to s16, and the top bit of each word
        # returns the 32768.
        return body('abab', ['pxor X4, X4', 'pcmpgtd X4, X2', 'pcmpgtd X4, X3', 'pand X2, X0', 'pand X3, X1',
                             'pcmpeqd X4, X4', 'psrld $31, X4', 'pslld $15, X4', 'psubd X4, X0', 'psubd X4, X1',
                             'packssdw X1, X0', 'pcmpeqd X4, X4', 'psllw $15, X4', 'pxor X4, X0'])
    return None


def lane_array_common(out):
    """Byte views, byte shifts and Shuffle over lane arrays."""
    out.append('#if ZI_VEC || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) || \\')
    out.append('    defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86) || defined(_M_ARM64)')
    out.append('#define ZI_LE 1')
    out.append('#else')
    out.append('#define ZI_LE 0')
    out.append('#endif')
    out.append('/* TinyCC runs the helpers as macros, on x86-64 the common integer ones in')
    out.append(' * SSE2 assembly. */')
    out.append('#if !ZI_VEC && defined(__TINYC__) && !defined(__GNUC__) && !defined(ZIRAN_SIMD_SCALAR)')
    out.append('#define ZI_TCC 1')
    out.append('#else')
    out.append('#define ZI_TCC 0')
    out.append('#endif')
    out.append('#if ZI_TCC && defined(__x86_64__)')
    out.append('#define ZI_TCC_X64 1')
    out.append('#else')
    out.append('#define ZI_TCC_X64 0')
    out.append('#endif')
    out.append('#if !ZI_VEC')
    out.append('#if !ZI_LE')
    out.append('/* Big-endian hosts view lanes as little-endian bytes one at a time. */')
    for t in TYPES:
        size = t.bits // 8
        out.append('static inline void zi_le_bytes_%s(%s a, unsigned char *o)\n{\n'
                   '    for(int i = 0; i < %d; i++) {\n'
                   '        uint64_t v = zi_bits_%s(ZI_AT(a, i));\n'
                   '        for(int j = 0; j < %d; j++) o[i * %d + j] = (unsigned char)(v >> 8 * j);\n'
                   '    }\n}' % (t.name, t.c, t.lanes, t.lane, size, size))
        out.append('static inline %s zi_le_lanes_%s(const unsigned char *o)\n{\n    %s r;\n'
                   '    for(int i = 0; i < %d; i++) {\n        uint64_t v = 0;\n'
                   '        for(int j = %d; j >= 0; j--) v = v << 8 | o[i * %d + j];\n'
                   '        ZI_AT(r, i) = zi_from_bits_%s(v);\n    }\n    return r;\n}' %
                   (t.c, t.name, t.c, t.lanes, size - 1, size, t.lane))
    out.append('#endif')
    out.append('/* Each 128-bit block of the bytes of `low` then `high` (when `both`;')
    out.append(' * zeros otherwise) from byte s on: AlignBytes, ShiftBytesRight with')
    out.append(' * s = n, and ShiftBytesLeft with s = -n. */')
    for t in INT:
        n = t.bytes
        out.append('static inline %s zi_bytes_from_%s(%s high, %s low, int64_t s, int both)\n{' % (t.c, t.name, t.c, t.c))
        out.append('#if ZI_LE')
        out.append('    union { %s v; unsigned char b[%d]; } h_, l_, r_;' % (t.c, n))
        out.append('    h_.v = high;\n    l_.v = low;')
        out.append('#else')
        out.append('    struct { unsigned char b[%d]; } h_, l_, r_;' % n)
        out.append('    zi_le_bytes_%s(high, h_.b);\n    zi_le_bytes_%s(low, l_.b);' % (t.name, t.name))
        out.append('#endif')
        out.append('    memset(r_.b, 0, %d);' % n)
        out.append('    for(int k = 0; k < %d; k += 16) {' % n)
        out.append('        /* Output bytes [start, stop) of the block come from low, then from high. */')
        out.append('        int64_t start = s < 0 ? -s : 0, stop = s > 0 ? 16 - s : 16;')
        out.append('        if(start < stop) memcpy(r_.b + k + start, l_.b + k + start + s, (size_t)(stop - start));')
        out.append('        start = s < 16 ? 16 - s : 0;')
        out.append('        stop = s > 16 ? 32 - s : 16;')
        out.append('        if(both && start < stop) memcpy(r_.b + k + start, h_.b + k + start + s - 16, (size_t)(stop - start));')
        out.append('    }')
        out.append('#if ZI_LE')
        out.append('    return r_.v;')
        out.append('#else')
        out.append('    return zi_le_lanes_%s(r_.b);' % t.name)
        out.append('#endif')
        out.append('}')
    out.append('/* Shuffle picks lanes of each 128-bit block. */')
    for t in TYPES:
        params = ', '.join('int k%d' % i for i in range(t.block))
        body = per_lane(t, lambda j: 'ZI_AT(a, %d + k%d)' % ((j // t.block) * t.block, j % t.block))
        out.append('static inline %s zi_shuffle_lanes_%s(%s a, %s)\n{\n    %s\n}' % (t.c, t.name, t.c, params, body))
    out.append('#endif')


# ---------------------------------------------------------------- output

def wide_params(params):
    return any(c.startswith('zi_') and BY_NAME[c[3:]].wide for c, _ in params)


def pointer_params(params):
    """Parameters of the below-AVX2 form: 256-bit vectors by const pointer."""
    return ', '.join(('const %s *%s_p_' % (c, n)) if c.startswith('zi_') and BY_NAME[c[3:]].wide
                     else '%s %s' % (c, n) for c, n in params)


# x86-64 code without AVX passes 256-bit vectors differently from AVX code,
# and GCC notes that ABI change for every such parameter in a non-AVX
# function, inlined or not, whatever the pragmas say. Below AVX2 a helper
# taking one is a macro that keeps its operands in locals and calls the
# helper's pointer form, so no 256-bit vector is ever a parameter there.
SPLIT_ABI = 'ZI_VEC && ZI_X86 && ZI_TIER < 4'
GCC_SPLIT = SPLIT_ABI + ' && !defined(__clang__)'


def prototype(out, ret, name, params):
    ret_c = ret.c if isinstance(ret, T) else ret
    plist = ', '.join('%s %s' % p for p in params) or 'void'
    if wide_params(params):
        out.append('#if %s' % SPLIT_ABI)
        out.append('ZI_FN %s ZI_N(%s_p_)(%s);' % (ret_c, name, pointer_params(params)))
        out.append('#else')
        out.append('ZI_FN %s ZI_N(%s)(%s);' % (ret_c, name, plist))
        out.append('#endif')
    else:
        out.append('ZI_FN %s ZI_N(%s)(%s);' % (ret_c, name, plist))


def split_macros(out, items, mark):
    """The below-AVX2 macros, one set per x86-64 helper prefix."""
    wide = [(name, params) for _op, _t, _ret, name, params, _b in items if wide_params(params)]
    mark('')
    out.append('#if %s' % SPLIT_ABI)
    for i, prefix in enumerate(('zi_', 'zi_ssse3_', 'zi_sse41_')):
        mark('')
        out.append('#%s ZI_NS == %d' % ('if' if i == 0 else 'elif', i + 1))
        for name, params in wide:
            mark(name)
            names = [n for _c, n in params]
            decls = []
            for c, n in params:
                if c.startswith('zi_') and BY_NAME[c[3:]].wide:
                    decls.append('%s __zi_%s = (%s);' % (c, n, n))
            args = ', '.join(('&__zi_%s' % n) if c.startswith('zi_') and BY_NAME[c[3:]].wide else '(%s)' % n
                             for c, n in params)
            out.append('#define %s%s(%s) __extension__ ({ %s %s%s_p_(%s); })' %
                       (prefix, name, ', '.join(names), ' '.join(decls), prefix, name, args))
    mark('')
    out.append('#endif')
    out.append('#endif')


def function(out, ret, name, params, bodies):
    """bodies: [(condition or None, lines)] in order; None is the final #else."""
    ret_c = ret.c if isinstance(ret, T) else ret
    plist = ', '.join('%s %s' % p for p in params) or 'void'
    if wide_params(params):
        out.append('#if %s' % SPLIT_ABI)
        out.append('ZI_FN %s ZI_N(%s_p_)(%s)\n{' % (ret_c, name, pointer_params(params)))
        for c, n in params:
            if c.startswith('zi_') and BY_NAME[c[3:]].wide:
                out.append('    %s %s = *%s_p_;' % (c, n, n))
        out.append('#else')
        out.append('ZI_FN %s ZI_N(%s)(%s)\n{' % (ret_c, name, plist))
        out.append('#endif')
    else:
        out.append('ZI_FN %s ZI_N(%s)(%s)\n{' % (ret_c, name, plist))
    if len(bodies) == 1 and bodies[0][0] is None:
        out.append('    ' + bodies[0][1])
    else:
        for i, (cond, body) in enumerate(bodies):
            if cond is None:
                out.append('#else')
            else:
                out.append('#%s %s' % ('if' if i == 0 else 'elif', cond))
            out.append('    ' + body)
        out.append('#endif')
    out.append('}')


FLOAT_ARITHMETIC = ('ZI_OP_ADD', 'ZI_OP_SUB', 'ZI_OP_MUL', 'ZI_OP_DIV', 'ZI_OP_MIN', 'ZI_OP_MAX',
                    'ZI_OP_EQ', 'ZI_OP_LT', 'ZI_OP_GT')


def plan(op, t):
    """The function bodies for (op, t)."""
    bodies = []
    specials = op.special(t)
    generic = op.generic(t)
    armv7_float = t.float and op.enum in FLOAT_ARITHMETIC
    if t.wide:
        if getattr(op, 'wide_specials', False):
            bodies += specials
        else:
            bodies += [(c, b) for c, b in specials if 'ZI_TIER >= 4' in c]
        r = op.result(t)
        split = op.splittable and isinstance(r, T) and r.wide and wide_params(op.params(t))
        if split:
            hn = op.name(t.half)
            lo = ', '.join('ZI_N(low_%s)(%s)' % (c[3:], n) if c.startswith('zi_') else n
                           for c, n in op.params(t))
            hi = ', '.join('ZI_N(high_%s)(%s)' % (c[3:], n) if c.startswith('zi_') else n
                           for c, n in op.params(t))
            body = 'return ZI_N(combine_%s)(ZI_N(%s)(%s), ZI_N(%s)(%s));' % (
                r.half.name, hn, lo, hn, hi)
            # Halves wherever they have their own instructions, and on x86
            # below AVX2, whose native width they are: GCC would run some
            # generic 256-bit operations lane by lane. Elsewhere the
            # compilers lower the vector forms well themselves (GCC's AVX2
            # would not join the halves again). Every target takes halves
            # over a lane-by-lane loop, and ARMv7 floats the halves' exact
            # versions.
            portable = op.portable(t)
            if getattr(op, 'split_cond', None):
                cond = op.split_cond
            elif generic is None and (portable is None or 'for(' in portable):
                cond = 'ZI_VEC'
            else:
                parts = [GCC_SPLIT if getattr(op, 'clang_whole', False) else SPLIT_ABI]
                parts += sorted(set(c for c, _ in op.special(t.half)))
                if armv7_float:
                    parts.append('ZI_NEON && !ZI_A64')
                cond = 'ZI_VEC && (%s)' % ' || '.join('(%s)' % c for c in parts)
            bodies.append((cond, body))
    else:
        bodies += specials
        if armv7_float:
            # ARMv7 NEON flushes subnormal floats to zero.
            bodies.append(('ZI_NEON && !ZI_A64', '\n    '.join(ref_body(op, t))))
    if generic is not None:
        bodies.append(('ZI_VEC', generic))
    portable = op.portable(t)
    scalar = scalar_body(op, t)
    if scalar is None:
        if portable is None:
            raise SystemExit('%s: no lane-array body' % op.name(t))
        scalar = portable
    elif generic is None and portable is not None:
        bodies.append(('ZI_VEC', portable))
    bodies.append((None, scalar))
    return bodies


def entries():
    """(op, t, return, name, params, bodies) for every helper function."""
    out = []
    for op in ops():
        if isinstance(op, ByteMacro):
            continue
        if isinstance(op, Narrow):
            for t, r in narrow_pairs():
                specials = op.special_pair(t, r)
                bodies = list(specials)
                if t.wide:
                    # Block by block, two 128-bit narrows. x86 and SIMD128
                    # have no 64-bit saturating packs, so there 64-bit
                    # sources narrow whole.
                    h = 'narrow_%s_%s' % (t.half.name, r.half.name)
                    bodies.append(('ZI_VEC' if t.bits < 64 else NEON,
                                   'return ZI_N(combine_%s)(ZI_N(%s)(ZI_N(low_%s)(a), ZI_N(low_%s)(b)),\n'
                                   '        ZI_N(%s)(ZI_N(high_%s)(a), ZI_N(high_%s)(b)));' %
                                   (r.half.name, h, t.name, t.name, h, t.name, t.name)))
                bodies.append(('ZI_VEC', op.generic_pair(t, r)))
                bodies.append((None, narrow_scalar(t, r)))
                out.append((op, t, r, 'narrow_%s_%s' % (t.name, r.name), [(t.c, 'a'), (t.c, 'b')], bodies))
            continue
        if isinstance(op, Reinterpret):
            for t in TYPES:
                for r in TYPES:
                    if r.bytes != t.bytes or r == t:
                        continue
                    out.append((op, t, r, 'reinterpret_%s_%s' % (t.name, r.name), [(t.c, 'a')],
                                reinterpret_bodies(t, r)))
            continue
        if isinstance(op, Convert):
            for t in TYPES:
                for r in TYPES:
                    if r.lanes != t.lanes or r.bits != t.bits or r.float == t.float:
                        continue
                    out.append((op, t, r, 'convert_%s_%s' % (t.name, r.name), [(t.c, 'a')],
                                [('ZI_VEC', convert_generic(t, r)), (None, convert_scalar(t, r))]))
            continue
        if isinstance(op, Gather):
            for t in op.types:
                for r in op.results(t):
                    body = ('%s r_;\n    for(int i_ = 0; i_ < %d; i_++) {\n'
                            '        %s v_;\n'
                            '        memcpy(&v_, (const char *)p + 4 * (int64_t)ZI_AT(a, i_), 4);\n'
                            '        ZI_AT(r_, i_) = v_;\n    }\n    return r_;' % (r.c, t.lanes, r.ctype))
                    bodies = []
                    if t.signed and not r.float:
                        m = '__m256i' if t.wide else '__m128i'
                        fn = '_mm256_i32gather_epi32' if t.wide else '_mm_i32gather_epi32'
                        bodies.append((AVX2, 'return (%s)%s((const int *)p, (%s)a, 4);' % (r.c, fn, m)))
                    bodies.append((None, body))
                    out.append((op, t, r, op.name_for(t, r), [('const void *', 'p'), (t.c, 'a')], bodies))
            continue
        for t in op.types:
            out.append((op, t, op.result(t), op.name(t), op.params(t), plan(op, t)))
    return out


def convert_generic(t, r):
    if not t.float:
        return 'return __builtin_convertvector(a, %s);' % r.c
    # Truncate toward zero; saturate out-of-range values; NaN gives 0.
    if r.signed:
        limit = '9223372036854775808.0' if r.bits == 64 else '2147483648.0'
        top = 'INT%d_MAX' % r.bits
        low = '-%s' % limit
    else:
        limit = '18446744073709551616.0' if r.bits == 64 else '4294967296.0'
        top = 'UINT%d_MAX' % r.bits
        low = '-1.0'
    m = t.u.c  # the comparison mask type
    return ('%s big_ = (%s)(a >= (%s){0} + (%s)%s), small_ = (%s)(a <= (%s){0} + (%s)%s), nan_ = (%s)(a != a);\n'
            '    %s safe_ = (%s)((%s)a & ~(big_ | small_ | nan_));\n'
            '    %s r_ = __builtin_convertvector(safe_, %s);\n'
            '    r_ = (%s)(((%s)r_ & ~big_) | (((%s){0} + (%s)%s) & big_));\n'
            '    r_ = (%s)(((%s)r_ & ~small_) | (((%s){0} + (%s)%s) & small_));\n'
            '    return (%s)((%s)r_ & ~nan_);' %
            (m, m, t.c, t.ctype, limit, m, t.c, t.ctype, low, m,
             t.c, t.c, m,
             r.c, r.c,
             r.c, m, m, r.utype, top,
             r.c, m, m, r.utype, ('(%s)INT%d_MIN' % (r.utype, r.bits)) if r.signed else '0',
             r.c, m))


def macros(out):
    """Constant-operand operations: byte shifts, AlignBytes and Shuffle."""
    out.append('/* Byte shifts, AlignBytes and Shuffle take constants; they are macros so')
    out.append(' * the constants reach the instructions in every build. */')
    for t in INT:
        u8 = 'zi_u8x%d' % t.bytes
        idx_align = []
        for block in range(2 if t.wide else 1):
            for i in range(16):
                idx_align.append('ZI_BYTE_FROM(%d, %d, %d, n)' % (block, i, t.bytes))
        joined = ', '.join(idx_align)
        out.append('#if ZI_VEC')
        out.append('#define zi_align_bytes_%s(a, b, n) ((%s)__builtin_shufflevector((%s)(b), (%s)(a), %s))' %
                   (t.name, t.c, u8, u8, joined))
        out.append('#define zi_shr_bytes_%s(a, n) zi_align_bytes_%s((%s){0}, a, n)' % (t.name, t.name, t.c))
        out.append('#define zi_shl_bytes_%s(a, n) zi_align_bytes_%s(a, (%s){0}, 16 - (n))' % (t.name, t.name, t.c))
        out.append('#else')
        out.append('#define zi_align_bytes_%s(a, b, n) zi_bytes_from_%s(a, b, n, 1)' % (t.name, t.name))
        out.append('#define zi_shr_bytes_%s(a, n) zi_bytes_from_%s(a, a, n, 0)' % (t.name, t.name))
        out.append('#define zi_shl_bytes_%s(a, n) zi_bytes_from_%s(a, a, -(int64_t)(n), 0)' % (t.name, t.name))
        out.append('#endif')
    for t in TYPES:
        params = ', '.join('i%d' % i for i in range(t.block))
        idx = ['(i%d)' % i for i in range(t.block)]
        if t.wide:
            idx += ['%d + (i%d)' % (t.block, i) for i in range(t.block)]
        out.append('#if ZI_VEC')
        out.append('#define zi_shuffle_%s(a, %s) ((%s)__builtin_shufflevector(a, (%s){0}, %s))' %
                   (t.name, params, t.c, t.c, ', '.join(idx)))
        out.append('#else')
        out.append('#define zi_shuffle_%s(a, %s) zi_shuffle_lanes_%s(a, %s)' %
                   (t.name, params, t.name, ', '.join('(i%d)' % i for i in range(t.block))))
        out.append('#endif')


# Whether a build uses native vectors, as every SIMD header sees it.
VEC_DETECTION = r"""/* Native vectors need __builtin_shufflevector: Clang, or GCC 12 and later. */
#if !defined(ZIRAN_SIMD_SCALAR) && !defined(__TINYC__) && \
    (defined(__clang__) || (defined(__GNUC__) && __GNUC__ >= 12)) && \
    defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define ZI_VEC 1
#else
#define ZI_VEC 0
#endif"""


def types_header():
    out = ['/* Generated by scripts/simd_header.py --types; do not edit. */',
           '/* The std/simd vector types alone. Generated C headers that only name',
           ' * vectors in declarations include this; code that runs operations',
           ' * includes ziran_simd.h, which includes it too. */',
           '#ifndef ZIRAN_SIMD_TYPES_H',
           '#define ZIRAN_SIMD_TYPES_H', '',
           '#include <stdint.h>', '',
           VEC_DETECTION, '',
           '#if ZI_VEC',
           '#define ZI_AT(v, i) (v)[i]']
    for t in TYPES:
        out.append('typedef %s %s __attribute__((vector_size(%d), aligned(%d)));' %
                   (t.ctype, t.c, t.bytes, t.bits // 8))
    out.append('#else')
    out.append('#define ZI_AT(x, i) (x).v[i]')
    for t in TYPES:
        out.append('typedef struct { %s v[%d]; } %s;' % (t.ctype, t.lanes, t.c))
    out.append('#endif')
    out.append('')
    out.append('#endif /* ZIRAN_SIMD_TYPES_H */')
    return '\n'.join(out) + '\n'


def common(out):
    out.append('#ifndef ZIRAN_SIMD_OPS_COMMON_H')
    out.append('#define ZIRAN_SIMD_OPS_COMMON_H')
    # The vector types come from ziran_simd_types.h; Narrow builds these
    # concatenations before truncating.
    out.append('#if ZI_VEC')
    for t in TYPES:
        if t.float or t.bits == 8:
            continue
        out.append('typedef %s zi_w_%s __attribute__((vector_size(%d)));' % (t.u.ctype, t.name, t.bytes * 2))
    out.append('#endif')
    out.append('')
    out.append('/* Lane values and the zero-extended bit patterns zi_ref computes on. */')
    for lane, (ctype, bits, signed, flt) in LANES.items():
        if flt:
            w = 'uint%d_t' % bits
            out.append('ZI_INLINE uint64_t zi_bits_%s(%s x) { %s w; memcpy(&w, &x, sizeof(w)); return w; }' % (lane, ctype, w))
            out.append('ZI_INLINE %s zi_from_bits_%s(uint64_t b) { %s w = (%s)b; %s x; memcpy(&x, &w, sizeof(x)); return x; }' %
                       (ctype, lane, w, w, ctype))
        else:
            out.append('ZI_INLINE uint64_t zi_bits_%s(%s x) { return (uint%d_t)x; }' % (lane, ctype, bits))
            out.append('ZI_INLINE %s zi_from_bits_%s(uint64_t b) { return (%s)(uint%d_t)b; }' % (ctype, lane, ctype, bits))
    # Native-vector builds move vectors through pointers here, so no 256-bit
    # vector is a parameter or crosses into code of another x86-64 tier.
    for t in TYPES:
        out.append('ZI_INLINE void zi_unpack_p_%s(const %s *x, uint64_t *o)\n{\n'
                   '    for(int i = 0; i < %d; i++) o[i] = zi_bits_%s(ZI_AT(*x, i));\n}' % (t.name, t.c, t.lanes, t.lane))
        out.append('ZI_INLINE void zi_pack_p_%s(%s *x, const uint64_t *o)\n{\n'
                   '    for(int i = 0; i < %d; i++) ZI_AT(*x, i) = zi_from_bits_%s(o[i]);\n}' %
                   (t.name, t.c, t.lanes, t.lane))
    out.append('#if ZI_VEC')
    for t in TYPES:
        out.append('#define zi_unpack_%s(x, o) __extension__ ({ %s __zi_x = (x); zi_unpack_p_%s(&__zi_x, (o)); })' %
                   (t.name, t.c, t.name))
        out.append('#define zi_pack_%s(o) __extension__ ({ %s __zi_x; zi_pack_p_%s(&__zi_x, (o)); __zi_x; })' %
                   (t.name, t.c, t.name))
    out.append('#else')
    for t in TYPES:
        out.append('ZI_INLINE void zi_unpack_%s(%s x, uint64_t *o) { zi_unpack_p_%s(&x, o); }' % (t.name, t.c, t.name))
        out.append('ZI_INLINE %s zi_pack_%s(const uint64_t *o) { %s x; zi_pack_p_%s(&x, o); return x; }' %
                   (t.c, t.name, t.c, t.name))
    out.append('#endif')
    out.append('')
    lane_array_common(out)
    out.append('/* Byte i of block `block` in AlignBytes: from low:high shifted by n. */')
    out.append('#define ZI_BYTE_FROM(block, i, size, n) ((i) + (n) < 16 ? (block) * 16 + (i) + (n) : (size) + (block) * 16 + (i) + (n) - 16)')
    macros(out)
    out.append('#endif /* ZIRAN_SIMD_OPS_COMMON_H */')


def header():
    out = ['/* Generated by scripts/simd_header.py; do not edit. */',
           '/* Included by ziran_simd.h once for the build target and once for each',
           ' * x86-64 tier, with ZI_TIER, ZI_FN and ZI_N(name) set. */',
           '/* After the common part, a line holding only a comment that starts with @',
           ' * starts a segment: @add_s16x8 the lines of that helper, and @ alone the',
           ' * lines every build keeps. zi2c writes each program a copy without the',
           ' * helpers it never calls (EmitSimdOps). */', '']
    common(out)
    out.append('')
    items = entries()
    tcc = [tcc_macro(op, t, ret, params, bodies) for op, t, ret, name, params, bodies in items]
    x64 = [tcc_x64_macro(op, t, ret, params) for op, t, ret, name, params, bodies in items]
    guards = [TCC if any_ else TCC_X64 if x else None for any_, x in zip(tcc, x64)]
    segment = ['']

    def mark(name):
        """The lines that follow belong to helper `name`, or with '' to all."""
        if name != segment[0]:
            out.append('/*@%s*/' % name)
            segment[0] = name
    out.append('/* ---- operations for ZI_TIER ---- */')
    unless_tcc_macro(out, items, guards, mark,
                     lambda op, t, ret, name, params, bodies: prototype(out, ret, name, params))
    mark('')
    out.append('')
    out.append('/* TinyCC runs these as macros; see tcc_macro and tcc_x64_macro. */')

    def define(item, macro):
        mark(item[3])
        out.append('#define zi_%s(%s) %s' % (item[3], ', '.join(n for _c, n in item[4]), macro))
    out.append('#if %s' % TCC_X64)
    for item, macro in zip(items, x64):
        if macro:
            define(item, macro)
    mark('')
    out.append('#elif %s' % TCC)
    for item, macro, x in zip(items, tcc, x64):
        if macro and x:
            define(item, macro)
    mark('')
    out.append('#endif')
    out.append('#if %s' % TCC)
    for item, macro, x in zip(items, tcc, x64):
        if macro and not x:
            define(item, macro)
    mark('')
    out.append('#endif')
    out.append('')
    split_macros(out, items, mark)
    out.append('')
    unless_tcc_macro(out, items, guards, mark, lambda op, t, ret, name, params, bodies:
                     function(out, ret, name, params, bodies))
    return '\n'.join(out) + '\n'


def unless_tcc_macro(out, items, guards, mark, write):
    """write(*item) for every helper, each one a TinyCC build runs as a
    macro (its guard, ZI_TCC or ZI_TCC_X64) outside that build. mark(name)
    tags the lines that follow with their helper."""
    inside = None
    for item, guard in zip(items, guards):
        if guard != inside:
            mark('')
            if inside:
                out.append('#endif')
            if guard:
                out.append('#if !%s' % guard)
            inside = guard
        mark(item[3])
        write(*item)
    mark('')
    if inside:
        out.append('#endif')


# ---------------------------------------------------------------- test

def c_random(t, rng):
    if t.float:
        specials = ['0x7fc00000', '0xff800000', '0x7f800000', '0x80000000', '0', '0x4f000000', '0xcf000000',
                    '0x4f800000', '0x3f800000'] if t.bits == 32 else \
                   ['0x7ff8000000000000ULL', '0xfff0000000000000ULL', '0x7ff0000000000000ULL',
                    '0x8000000000000000ULL', '0', '0x43e0000000000000ULL', '0xc3e0000000000000ULL',
                    '0x43f0000000000000ULL', '0x3ff0000000000000ULL']
        return specials
    return None


TEST_PRELUDE = r'''
#include "ziran_simd.h"
#include <stdio.h>
#include <stdlib.h>

static uint64_t rng_state = 0x9e3779b97f4a7c15ULL;
static uint64_t next_random(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}
/* Edge-heavy lanes: 0, 1, extremes and random bits. */
static uint64_t lane_random(int kind)
{
    uint64_t r = next_random(), mask = zi_kmask(kind);
    int bits = zi_kbits(kind);
    switch(r % 8) {
    case 0: return 0;
    case 1: return 1;
    case 2: return mask;
    case 3: return ((uint64_t)1 << (bits - 1)) & mask;
    case 4: return (((uint64_t)1 << (bits - 1)) - 1) & mask;
    case 5: return (r >> 8) % 70;
    default: break;
    }
    if(zi_kfloat(kind)) {
        /* Mostly ordinary numbers, sometimes NaN, infinities or zeros. */
        double v = (double)(int64_t)(next_random() >> 20) / (double)(1 << (int)(next_random() % 24));
        if(next_random() % 16 == 0) return kind == ZI_K_F32 ? 0x7fc00000u : 0x7ff8000000000000ULL;
        if(next_random() % 16 == 0) v = v * 1e30;
        return zi_fput(v, kind);
    }
    return next_random() & mask;
}
static int failures;
static int same_lanes(const uint64_t *x, const uint64_t *y, int kind, int count, int nan_equal)
{
    for(int i = 0; i < count; i++) {
        uint64_t a = x[i] & zi_kmask(kind), b = y[i] & zi_kmask(kind);
        if(a == b) continue;
        if(nan_equal && zi_kfloat(kind)) {
            double p = zi_fget(a, kind), q = zi_fget(b, kind);
            if(p != p && q != q) continue;
        }
        return 0;
    }
    return 1;
}
static void report(const char *name, const char *tier, int round)
{
    if(failures++ < 40) fprintf(stderr, "mismatch: %s (%s) round %d\n", name, tier, round);
}
'''


def test_program(tiers):
    """Each case is its own small function, so the program compiles quickly."""
    rng = random.Random(7)
    out = [TEST_PRELUDE]
    items = entries()
    for prefix, tier_name, cond in tiers:
        tier = tier_name.replace('.', '').replace(',', '_')
        target = '__attribute__((target("%s"))) ' % tier_name if prefix != 'zi_' else ''
        cases = [test_case(op, t, ret, name, params, prefix, tier_name)
                 for op, t, ret, name, params, bodies in items]
        cases += test_macros(prefix, tier_name, rng)
        out.append('#if %s' % cond)
        for number, lines in enumerate(cases):
            out.append('%s__attribute__((noinline)) static void case_%s_%d(int round)\n{' % (target, tier, number))
            out.append('    uint64_t x[32] = {0}, y[32] = {0}, z[32] = {0}, o[32] = {0}, e[32] = {0};')
            out.append('    int64_t k[32] = {0};')
            out.append('    unsigned char memory[96];')
            out.append('    (void)x; (void)y; (void)z; (void)o; (void)e; (void)k; (void)memory; (void)round;')
            out.extend(lines)
            out.append('}')
        out.append('static void test_%s(void)\n{' % tier)
        out.append('    for(int round = 0; round < 400; round++) {')
        for number in range(len(cases)):
            out.append('        case_%s_%d(round);' % (tier, number))
        out.append('    }')
        out.append('}')
        out.append('#endif')
    out.append('int main(void)\n{')
    for prefix, tier_name, cond in tiers:
        fn = 'test_%s' % tier_name.replace('.', '').replace(',', '_')
        if prefix == 'zi_':
            out.append('#if %s\n    %s();\n#endif' % (cond, fn))
        else:
            feature = tier_name.split(',')[-1]
            out.append('#if %s\n    if(__builtin_cpu_supports("%s")) %s();\n    else printf("skip %s\\n");\n#endif' %
                       (cond, feature, fn, tier_name))
    out.append('    if(failures) { fprintf(stderr, "%d mismatches\\n", failures); return 1; }')
    out.append('    printf("simd ops agree with zi_ref\\n");')
    out.append('    return 0;\n}')
    return '\n'.join(out) + '\n'


def fill(var, t, source='lane_random'):
    return 'for(int i = 0; i < %d; i++) %s[i] = %s(%s);' % (t.lanes, var, source, t.kind)


def test_case(op, t, ret, name, params, prefix, tier):
    lines = ['        {']
    fn = prefix + name
    rk, rn = (ret.kind, ret.lanes) if isinstance(ret, T) else (None, 1)
    nan_equal = 1 if op.enum in ('ZI_OP_ADD', 'ZI_OP_SUB', 'ZI_OP_MUL', 'ZI_OP_DIV', 'ZI_OP_SPLAT',
                                 'ZI_OP_MAKE', 'ZI_OP_LANE', 'ZI_OP_WITH_LANE', 'ZI_OP_SHUFFLE',
                                 'ZI_OP_CONVERT') else 0
    shape = op.shape
    call_args, ref = [], None
    # Inputs as bit patterns, then typed values.
    vector_params = [(c, n) for c, n in params if c.startswith('zi_')]
    for i, (c, n) in enumerate(params):
        if c.startswith('zi_'):
            pt = BY_NAME[c[3:]]
            var = 'xyz'[[p[1] for p in vector_params].index(n)]
            lines.append('            ' + fill(var, pt))
            if op.enum == 'ZI_OP_SELECT' and n == 'm':
                lines.append('            for(int i = 0; i < %d; i++) x[i] = (next_random() & 1) ? zi_kmask(%s) : (next_random() & 3) ? 0 : x[i];' % (pt.lanes, pt.kind))
            if op.enum in ('ZI_OP_SHL_LANES', 'ZI_OP_SHR_LANES') and n == 'b':
                lines.append('            for(int i = 0; i < %d; i++) if(next_random() & 1) y[i] %%= %d;' % (pt.lanes, pt.bits + 2))
            if op.enum == 'ZI_OP_BYTE_SHUFFLE' and n == 'b':
                lines.append('            for(int i = 0; i < %d; i++) if(next_random() & 3) y[i] %%= 16;' % pt.lanes)
            if op.enum == 'ZI_OP_GATHER':
                lines.append('            for(int i = 0; i < %d; i++) x[i] %%= 24;' % pt.lanes)
            lines.append('            %s %s_ = zi_pack_%s(%s);' % (c, n, pt.name, var))
            call_args.append('%s_' % n)
    # Scalar operands.
    for c, n in params:
        if c.startswith('zi_'):
            continue
        if n == 'n':
            lines.append('            uint64_t n_ = next_random() %% 5 == 0 ? next_random() : next_random() %% %d;' % (t.bits + 3))
            lines.append('            k[0] = (int64_t)n_;')
            call_args.insert(1, 'n_')
        elif n == 'i':
            lines.append('            int i_ = (int)(next_random() %% %d);' % t.lanes)
            lines.append('            k[0] = i_;')
            call_args.insert(1, 'i_')
        elif n == 'x':
            lines.append('            uint64_t xb_ = lane_random(%s);' % t.kind)
            lines.append('            %s x_ = zi_from_bits_%s(xb_);' % (t.ctype, t.lane))
            lines.append('            y[0] = xb_;')
            call_args.append('x_')
        elif n.startswith('l'):
            pass
        elif n == 'p':
            pass
    if shape == 'make':
        lines.append('            ' + fill('x', t))
        call_args = ['zi_from_bits_%s(x[%d])' % (t.lane, i) for i in range(t.lanes)]
    if shape == 'memory':
        lines.append('            for(int i = 0; i < 96; i++) memory[i] = (unsigned char)next_random();')
        lines.append('            int at_ = (int)(next_random() % 32);')
        if op.store:
            lines.append('            unsigned char copy_[96];')
            lines.append('            memcpy(copy_, memory, 96);')
            lines.append('            %s(memory + at_, a_);' % fn)
            size = 8 if op.low else t.bytes
            lines.append('            { unsigned char want_[32]; zi_bytes(x, %s, %d, want_);' % (t.kind, t.lanes))
            lines.append('              memcpy(copy_ + at_, want_, %d);' % size)
            lines.append('              if(memcmp(copy_, memory, 96)) report("%s", "%s", round); }' % (name, tier))
        else:
            lines.append('            %s r_ = %s(memory + at_);' % (t.c, fn))
            lines.append('            zi_unpack_%s(r_, o);' % t.name)
            size = 8 if op.low else t.bytes
            lines.append('            { unsigned char want_[32] = {0}; memcpy(want_, memory + at_, %d);' % size)
            lines.append('              zi_unbytes(want_, %s, %d, e); }' % (t.kind, t.lanes))
            lines.append('            if(!same_lanes(o, e, %s, %d, 0)) report("%s", "%s", round);' % (t.kind, t.lanes, name, tier))
        lines.append('        }')
        return lines
    if shape == 'gather':
        r = ret
        lines.append('            uint32_t table_[24];')
        lines.append('            for(int i = 0; i < 24; i++) table_[i] = (uint32_t)next_random();')
        lines.append('            %s r_ = %s(table_, a_);' % (r.c, fn))
        lines.append('            zi_unpack_%s(r_, o);' % r.name)
        lines.append('            for(int i = 0; i < %d; i++) e[i] = table_[x[i]];' % t.lanes)
        lines.append('            if(!same_lanes(o, e, %s, %d, 0)) report("%s", "%s", round);' % (r.kind, r.lanes, name, tier))
        lines.append('        }')
        return lines
    # Reference.
    a_t = t
    if isinstance(ret, T):
        lines.append('            %s r_ = %s(%s);' % (ret.c, fn, ', '.join(call_args)))
        lines.append('            zi_unpack_%s(r_, o);' % ret.name)
    else:
        lines.append('            %s r_ = %s(%s);' % (ret, fn, ', '.join(call_args)))
        conv = 'zi_bits_%s(r_)' % t.lane if ret == t.ctype else '(uint64_t)r_'
        lines.append('            o[0] = %s;' % conv)
    enum = op.enum
    if shape == 'make':
        a_arg = 'x'
    else:
        a_arg = 'x'
    b_arg, c_arg = 'y', 'z'
    if enum == 'ZI_OP_SPLAT' or enum == 'ZI_OP_WITH_LANE':
        pass
    if isinstance(ret, T):
        lines.append('            zi_ref(%s, %s, %d, %s, %d, %s, %s, %s, k, e);' % (enum, ret.kind, ret.lanes, a_t.kind, a_t.lanes, a_arg, b_arg, c_arg))
        lines.append('            if(!same_lanes(o, e, %s, %d, %d)) report("%s", "%s", round);' % (ret.kind, ret.lanes, nan_equal, name, tier))
    else:
        kind = t.kind if ret == t.ctype else 'ZI_K_S64'
        lines.append('            zi_ref(%s, %s, 1, %s, %d, %s, %s, %s, k, e);' % (enum, kind, a_t.kind, a_t.lanes, a_arg, b_arg, c_arg))
        lines.append('            if(!same_lanes(o, e, %s, 1, %d)) report("%s", "%s", round);' % (kind, nan_equal, name, tier))
    lines.append('        }')
    return lines


def test_macros(prefix, tier, rng):
    cases = []
    for t in INT:
        for n in range(17):
            for macro, enum in (('align_bytes', 'ZI_OP_ALIGN_BYTES'), ('shr_bytes', 'ZI_OP_SHR_BYTES'),
                                ('shl_bytes', 'ZI_OP_SHL_BYTES')):
                if macro != 'align_bytes' and n % 3 and n not in (1, 15, 16):
                    continue
                if macro == 'align_bytes' and n % 2 and n not in (1, 15):
                    continue
                lines = ['        {']
                lines.append('            ' + fill('x', t))
                lines.append('            ' + fill('y', t))
                if macro == 'align_bytes':
                    lines.append('            %s a_ = zi_pack_%s(x), b_ = zi_pack_%s(y);' % (t.c, t.name, t.name))
                    lines.append('            %s r_ = zi_%s_%s(a_, b_, %d);' % (t.c, macro, t.name, n))
                else:
                    lines.append('            %s a_ = zi_pack_%s(x);' % (t.c, t.name))
                    lines.append('            %s r_ = zi_%s_%s(a_, %d);' % (t.c, macro, t.name, n))
                lines.append('            k[0] = %d;' % n)
                lines.append('            zi_unpack_%s(r_, o);' % t.name)
                lines.append('            zi_ref(%s, %s, %d, %s, %d, x, y, 0, k, e);' % (enum, t.kind, t.lanes, t.kind, t.lanes))
                lines.append('            if(!same_lanes(o, e, %s, %d, 0)) report("%s_%s %d", "%s", round);' % (t.kind, t.lanes, macro, t.name, n, tier))
                lines.append('        }')
                cases.append(lines)
    for t in TYPES:
        for _ in range(3):
            idx = [rng.randrange(t.block) for _ in range(t.block)]
            lines = ['        {']
            lines.append('            ' + fill('x', t))
            lines.append('            %s a_ = zi_pack_%s(x);' % (t.c, t.name))
            lines.append('            %s r_ = zi_shuffle_%s(a_, %s);' % (t.c, t.name, ', '.join(map(str, idx))))
            for i, v in enumerate(idx):
                lines.append('            k[%d] = %d;' % (i, v))
            lines.append('            zi_unpack_%s(r_, o);' % t.name)
            lines.append('            zi_ref(ZI_OP_SHUFFLE, %s, %d, %s, %d, x, 0, 0, k, e);' % (t.kind, t.lanes, t.kind, t.lanes))
            lines.append('            if(!same_lanes(o, e, %s, %d, 1)) report("shuffle_%s", "%s", round);' % (t.kind, t.lanes, t.name, tier))
            lines.append('        }')
            cases.append(lines)
    return cases


# ---------------------------------------------------------------- Go

# zi2go writes each std/simd helper a Go package calls into ziran_simd.go.
# Value helpers get these typed bodies: straight-line over each lane, or a
# loop for more than eight lanes of one per-lane expression, so the Go
# compiler inlines the common ones. zir_emit_simd_go.c builds each
# signature, naming the parameters a, b, m (Select's mask), n (a count),
# i (a lane index), l (a lane value), k0.. (Shuffle) and l0.. (Make).
# Floating-point lanes convert each result to their own type, which keeps
# Go from fusing a multiply and an add across inlined helpers.

GO_LANE = {'u8': 'uint8', 's8': 'int8', 'u16': 'uint16', 's16': 'int16', 'u32': 'uint32',
           's32': 'int32', 'u64': 'uint64', 's64': 'int64', 'f32': 'float32', 'f64': 'float64'}
GO_HELPERS = {}  # lane function name: its Go text


def go_lane(t):
    return GO_LANE[t.lane]


def go_vec(t):
    return '[%d]%s' % (t.lanes, go_lane(t))


def go_func(name, params, ret, body):
    GO_HELPERS.setdefault(name, 'func %s(%s) %s {\n%s}\n' % (name, params, ret, body))
    return name


def go_lane_fn(kind, t, params, ret, body):
    return go_func('zi_go_%s_%s' % (kind, t.lane), params, ret, body)


def go_minmax(t, less):
    c = '<' if less else '>'
    return go_lane_fn('min' if less else 'max', t, 'x, y %s' % go_lane(t), go_lane(t),
                      '\tif x %s y {\n\t\treturn x\n\t}\n\treturn y\n' % c)


def go_sat(r):
    """r's lane clamped from an int64."""
    hi = (1 << (r.bits - 1)) - 1 if r.signed else (1 << r.bits) - 1
    lo = -(1 << (r.bits - 1)) if r.signed else 0
    return go_lane_fn('sat', r, 'v int64', go_lane(r),
                      '\tif v < %d {\n\t\treturn %d\n\t}\n\tif v > %d {\n\t\treturn %d\n\t}\n\treturn %s(v)\n' %
                      (lo, lo, hi, hi, go_lane(r)))


def go_sat_unsigned(r):
    """r's lane clamped from a uint64."""
    hi = (1 << (r.bits - 1)) - 1 if r.signed else (1 << r.bits) - 1
    return go_lane_fn('satu', r, 'v uint64', go_lane(r),
                      '\tif v > %d {\n\t\treturn %d\n\t}\n\treturn %s(v)\n' % (hi, hi, go_lane(r)))


def go_mask(r):
    full = '-1' if r.signed else '^%s(0)' % go_lane(r)
    return go_lane_fn('mask', r, 'yes bool', go_lane(r),
                      '\tif yes {\n\t\treturn %s\n\t}\n\treturn 0\n' % full)


def go_abs(t):
    return go_lane_fn('abs', t, 'x %s' % go_lane(t), go_lane(t),
                      '\tif x < 0 {\n\t\treturn -x\n\t}\n\treturn x\n')


def go_sign(t):
    return go_lane_fn('sign', t, 'x, s %s' % go_lane(t), go_lane(t),
                      '\tif s < 0 {\n\t\treturn -x\n\t}\n\tif s == 0 {\n\t\treturn 0\n\t}\n\treturn x\n')


def go_abs_diff(t):
    u = go_lane(t.u)
    x, y = ('%s(x)' % u, '%s(y)' % u) if t.signed else ('x', 'y')
    return go_lane_fn('abs_diff', t, 'x, y %s' % go_lane(t), u,
                      '\tif x < y {\n\t\treturn %s - %s\n\t}\n\treturn %s - %s\n' % (y, x, x, y))


def go_to_int(t, r):
    """A float lane truncated to r's integers, saturated, NaN as 0."""
    name = 'zi_go_%s_%s' % (t.lane, r.lane)
    lane = go_lane(r)
    if r.signed:
        limit = 1 << (r.bits - 1)
        body = ('\tif v != v {\n\t\treturn 0\n\t}\n\tif v >= %d {\n\t\treturn math.MaxInt%d\n\t}\n'
                '\tif v <= -%d {\n\t\treturn math.MinInt%d\n\t}\n\treturn %s(v)\n' %
                (limit, r.bits, limit, r.bits, lane))
    else:
        body = ('\tif !(v > 0) {\n\t\treturn 0\n\t}\n\tif v >= %d {\n\t\treturn math.MaxUint%d\n\t}\n'
                '\treturn %s(v)\n' % (1 << r.bits, r.bits, lane))
    return go_func(name, 'v %s' % go_lane(t), lane, body)


def go_bits(t, x):
    """Lane x of t as an unsigned integer of its width."""
    if t.float:
        return 'math.Float%dbits(%s)' % (t.bits, x)
    return 'uint%d(%s)' % (t.bits, x) if t.signed else x


def go_from_bits(r, x, width):
    """A lane of r from x, an unsigned integer `width` bits wide."""
    if r.float:
        if width != r.bits:
            x = 'uint%d(%s)' % (r.bits, x)
        return 'math.Float%dfrombits(%s)' % (r.bits, x)
    if width == r.bits and not r.signed:
        return x
    return '%s(%s)' % (go_lane(r), x)


def go_list(prefix, items, suffix):
    """prefix{items}suffix, one line if it is short, else wrapped."""
    line = '%s{%s}%s' % (prefix, ', '.join(items), suffix)
    if len(line) <= 96:
        return '\t' + line + '\n'
    rows, row = [], ''
    for item in items:
        if row and len(row) + len(item) + 2 > 88:
            rows.append(row)
            row = ''
        row += ('' if not row else ' ') + item + ','
    rows.append(row)
    return '\t%s{\n%s\t}%s\n' % (prefix, ''.join('\t\t%s\n' % r for r in rows), suffix)


def go_pick(r, expr, setup=''):
    """setup, then a return of r with lane j set to expr(j)."""
    return setup + go_list('return ' + go_vec(r), [expr(j) for j in range(r.lanes)], '')


def go_map(r, expr, setup=''):
    """go_pick for an expression the same for every lane j, given as expr('j')
    in a loop past eight lanes."""
    if r.lanes <= 8:
        return go_pick(r, expr, setup)
    return (setup + '\tvar r %s\n\tfor j := range r {\n\t\tr[j] = %s\n\t}\n\treturn r\n' %
            (go_vec(r), expr('j')))


def go_word(t, v, w):
    """64-bit word w of vector v of t, its lanes little-endian."""
    per = 64 // t.bits
    terms = []
    for j in range(per):
        x = go_bits(t, '%s[%d]' % (v, w * per + j))
        if t.bits < 64:
            x = 'uint64(%s)' % x
        terms.append(x if j == 0 else '%s<<%d' % (x, t.bits * j))
    return ' | '.join(terms)


def go_from_words(r, words):
    """Lanes of r from 64-bit words, little-endian."""
    per = 64 // r.bits

    def lane(j):
        y, shift = words[j // per], r.bits * (j % per)
        return go_from_bits(r, '%s >> %d' % (y, shift) if shift else y, 64)
    return lane


def go_reinterpret(t, r):
    if r.bits == t.bits:
        return go_map(r, lambda i: go_from_bits(r, go_bits(t, 'a[%s]' % i), t.bits))
    if r.bits > t.bits:
        per = r.bits // t.bits
        u = 'uint%d' % r.bits

        def wide(j):
            terms = []
            for k in range(per):
                x = '%s(%s)' % (u, go_bits(t, 'a[%d]' % (j * per + k)))
                terms.append(x if k == 0 else '%s<<%d' % (x, t.bits * k))
            return go_from_bits(r, ' | '.join(terms), r.bits)
        return go_pick(r, wide)
    per = t.bits // r.bits

    def narrow(j):
        x, shift = 'a[%d]' % (j // per), r.bits * (j % per)
        if t.float or r.float:
            x = go_bits(t, x)
        if shift:
            x = '%s >> %d' % (x, shift)
        return go_from_bits(r, x, t.bits)
    return go_pick(r, narrow)


def go_byte_shift(t, kind):
    """ShiftBytesLeft, ShiftBytesRight or AlignBytes on each 128-bit block as
    two 64-bit words, through Go's shifts, which give 0 for a count of 64 or
    more."""
    lines = ['\ts := uint(n) * 8\n']
    words = []
    for block in range(2 if t.wide else 1):
        x0, x1 = 2 * block, 2 * block + 1
        lines.append('\tx%d := %s\n\tx%d := %s\n' % (x0, go_word(t, 'a', x0), x1, go_word(t, 'a', x1)))
        y0, y1 = 'y%d' % x0, 'y%d' % x1
        if kind == 'shr':
            lines.append('\t%s := x%d>>s | x%d<<(64-s) | x%d>>(s-64)\n\t%s := x%d >> s\n' %
                         (y0, x0, x1, x1, y1, x1))
        elif kind == 'shl':
            lines.append('\t%s := x%d << s\n\t%s := x%d<<s | x%d>>(64-s) | x%d<<(s-64)\n' %
                         (y0, x0, y1, x1, x0, x0))
        else:
            # AlignBytes(a, b, n): the 32 bytes b:a from byte n.
            lines.append('\tz%d := %s\n\tz%d := %s\n' % (x0, go_word(t, 'b', x0), x1, go_word(t, 'b', x1)))
            lines.append('\t%s := z%d>>s | z%d<<(64-s) | z%d>>(s-64) | x%d<<(128-s)\n' % (y0, x0, x1, x1, x0))
            lines.append('\t%s := z%d>>s | x%d<<(64-s) | x%d>>(s-64) | x%d<<(128-s)\n' % (y1, x1, x0, x0, x1))
        words += [y0, y1]
    return ''.join(lines) + go_pick(t, go_from_words(t, words))


def go_reduce(t, kind):
    if kind == 'add':
        if t.lanes > 8:
            return '\tvar s %s\n\tfor _, v := range a {\n\t\ts += v\n\t}\n\treturn s\n' % go_lane(t)
        return '\treturn %s\n' % ' + '.join('a[%d]' % i for i in range(t.lanes))
    if kind == 'max_index':
        # The first of equal lanes.
        if t.lanes > 8:
            return ('\tm, at := a[0], 0\n\tfor j, v := range a {\n\t\tif v > m {\n\t\t\tm, at = v, j\n'
                    '\t\t}\n\t}\n\treturn int64(at)\n')
        lines = ['\tm, at := a[0], 0\n']
        lines += ['\tif a[%d] > m {\n\t\tm, at = a[%d], %d\n\t}\n' % (i, i, i) for i in range(1, t.lanes)]
        return ''.join(lines) + '\treturn int64(at)\n'
    f = go_minmax(t, kind == 'min')
    if t.lanes > 8:
        return '\tm := a[0]\n\tfor _, v := range a {\n\t\tm = %s(m, v)\n\t}\n\treturn m\n' % f
    # Integer minimums and maximums, so any order gives the same lane.
    v = ['a[%d]' % i for i in range(t.lanes)]
    while len(v) > 1:
        v = ['%s(%s, %s)' % (f, v[i], v[i + 1]) for i in range(0, len(v), 2)]
    return '\treturn %s\n' % v[0]


def go_body(op, t, r):
    """op's Go body for t with result r, or None."""
    e, L, W = op.enum, go_lane(t), t.bits
    a = lambda i: 'a[%s]' % i
    b = lambda i: 'b[%s]' % i
    if e in ('ZI_OP_ADD', 'ZI_OP_SUB', 'ZI_OP_MUL', 'ZI_OP_DIV'):
        if t.float:
            return go_map(t, lambda i: '%s(%s %s %s)' % (L, a(i), op.c_op, b(i)))
        return go_map(t, lambda i: '%s %s %s' % (a(i), op.c_op, b(i)))
    if e in ('ZI_OP_ADD_SAT', 'ZI_OP_SUB_SAT'):
        f, c = go_sat(t), '+' if e == 'ZI_OP_ADD_SAT' else '-'
        return go_map(t, lambda i: '%s(int64(%s) %s int64(%s))' % (f, a(i), c, b(i)))
    if e in ('ZI_OP_MIN', 'ZI_OP_MAX'):
        f = go_minmax(t, e == 'ZI_OP_MIN')
        return go_map(t, lambda i: '%s(%s, %s)' % (f, a(i), b(i)))
    if e == 'ZI_OP_AVG':
        return go_map(t, lambda i: '%s((uint32(%s) + uint32(%s) + 1) >> 1)' % (L, a(i), b(i)))
    if e == 'ZI_OP_MUL_HIGH':
        p = 'int32' if t.signed else 'uint32'
        return go_map(t, lambda i: '%s((%s(%s) * %s(%s)) >> 16)' % (L, p, a(i), p, b(i)))
    if e == 'ZI_OP_MUL_HIGH_ROUND':
        return go_map(t, lambda i: 'int16((int32(%s)*int32(%s) + 0x4000) >> 15)' % (a(i), b(i)))
    bitwise = {'ZI_OP_AND': '&', 'ZI_OP_OR': '|', 'ZI_OP_XOR': '^', 'ZI_OP_AND_NOT': '&^'}
    if e in bitwise:
        return go_map(t, lambda i: '%s %s %s' % (a(i), bitwise[e], b(i)))
    if e == 'ZI_OP_NOT':
        return go_map(t, lambda i: '^%s' % a(i))
    if e in ('ZI_OP_ABS', 'ZI_OP_NEG') and t.float:
        sign = '0x%x' % (1 << (W - 1))
        change = '&^' if e == 'ZI_OP_ABS' else '^'
        return go_map(t, lambda i: 'math.Float%dfrombits(math.Float%dbits(%s) %s %s)' % (W, W, a(i), change, sign))
    if e == 'ZI_OP_ABS':
        f = go_abs(t)
        return go_map(t, lambda i: '%s(%s)' % (f, a(i)))
    if e == 'ZI_OP_NEG':
        return go_map(t, lambda i: '-%s' % a(i))
    if e == 'ZI_OP_SIGN':
        f = go_sign(t)
        return go_map(t, lambda i: '%s(%s, %s)' % (f, a(i), b(i)))
    if e == 'ZI_OP_ABS_DIFF':
        f = go_abs_diff(t)
        return go_map(r, lambda i: '%s(%s, %s)' % (f, a(i), b(i)))
    if isinstance(op, Compare):
        f = go_mask(r)
        return go_map(r, lambda i: '%s(%s %s %s)' % (f, a(i), op.c_op, b(i)))
    if e == 'ZI_OP_SELECT':
        m = lambda i: 'm[%s]' % i
        if t.float:
            return go_map(t, lambda i: 'math.Float%dfrombits(math.Float%dbits(%s)&%s | math.Float%dbits(%s)&^%s)' %
                          (W, W, a(i), m(i), W, b(i), m(i)))
        mask = (lambda i: '%s(%s)' % (L, m(i))) if t.signed else m
        return go_map(t, lambda i: '%s&%s | %s&^%s' % (a(i), mask(i), b(i), mask(i)))
    if isinstance(op, Shift):
        # Go shifts give 0, or the sign for a signed right shift, for a
        # count of the lane width or more, as std/simd does.
        if op.kind == 'shl':
            return go_map(t, lambda i: '%s << n' % a(i))
        if op.kind == 'shr':
            return go_map(t, lambda i: '%s >> n' % a(i))
        # (v >> n) plus the last bit shifted out; a count of 0 shifts out
        # nothing.
        return go_map(t, lambda i: '%s>>n + %s>>m&1' % (a(i), a(i)),
                      setup='\tif n == 0 {\n\t\treturn a\n\t}\n\tm := n - 1\n')
    if isinstance(op, ShiftLanes):
        # Each count lane read as unsigned.
        c = (lambda i: 'uint%d(%s)' % (W, b(i))) if t.signed else b
        return go_map(t, lambda i: '%s %s %s' % (a(i), '<<' if op.left else '>>', c(i)))
    if isinstance(op, ByteMacro):
        return go_byte_shift(t, op.kind)
    if e == 'ZI_OP_MAKE':
        return go_pick(t, lambda i: 'l%d' % i)
    if e == 'ZI_OP_SPLAT':
        return go_map(t, lambda i: 'l')
    if e == 'ZI_OP_ZERO':
        return '\treturn %s{}\n' % go_vec(t)
    if e == 'ZI_OP_LANE':
        return '\treturn a[i]\n'
    if e == 'ZI_OP_WITH_LANE':
        return '\ta[i] = l\n\treturn a\n'
    if isinstance(op, WidenHalf):
        offset = t.block // 2 if op.high else 0
        return go_pick(r, lambda j: '%s(%s)' % (go_lane(r), a(block_lane(t, r, j, offset))))
    if e == 'ZI_OP_WIDEN':
        return go_map(r, lambda i: '%s(%s)' % (go_lane(r), a(i)))
    if isinstance(op, MulWide):
        offset = t.block // 2 if op.high else 0
        R = go_lane(r)
        return go_pick(r, lambda j: '%s(%s) * %s(%s)' % (
            R, a(block_lane(t, r, j, offset)), R, b(block_lane(t, r, j, offset))))
    if e == 'ZI_OP_MADD':
        return go_pick(r, lambda i: 'int32(%s)*int32(%s) + int32(%s)*int32(%s)' %
                       (a(2 * i), b(2 * i), a(2 * i + 1), b(2 * i + 1)))
    if e == 'ZI_OP_MADD_SAT':
        f = go_sat(r)
        return go_pick(r, lambda i: '%s(int64(%s)*int64(%s) + int64(%s)*int64(%s))' %
                       (f, a(2 * i), b(2 * i), a(2 * i + 1), b(2 * i + 1)))
    if e == 'ZI_OP_ADD_PAIRS':
        R = go_lane(r)
        return go_pick(r, lambda i: '%s(%s) + %s(%s)' % (R, a(2 * i), R, a(2 * i + 1)))
    if e == 'ZI_OP_SAD':
        f = go_abs_diff(t)
        return go_pick(r, lambda i: ' + '.join('uint64(%s(%s, %s))' % (f, a(k), b(k))
                                               for k in range(8 * i, 8 * i + 8)))
    if isinstance(op, Reduce):
        return go_reduce(t, op.kind)
    if e == 'ZI_OP_ALL_ZERO':
        if t.lanes > 8:
            return '\tvar x %s\n\tfor _, v := range a {\n\t\tx |= v\n\t}\n\treturn x == 0\n' % L
        return '\treturn %s == 0\n' % '|'.join(a(i) for i in range(t.lanes))
    if e == 'ZI_OP_BYTE_SHUFFLE':
        # An index of 16 or more picks 0.
        return ('\tvar r %s\n\tfor j, x := range b {\n\t\tif x < 16 {\n\t\t\tr[j] = a[j&^15+int(x)]\n'
                '\t\t}\n\t}\n\treturn r\n' % go_vec(t))
    if e == 'ZI_OP_SHUFFLE':
        return go_pick(t, lambda j: a('%sk%d' % ('%d+' % (j // t.block * t.block) if j >= t.block else '',
                                                  j % t.block)))
    if isinstance(op, Half):
        offset = r.lanes if op.high else 0
        return go_pick(r, lambda j: a(j + offset))
    if isinstance(op, Combine):
        return go_pick(r, lambda j: a(j) if j < t.lanes else b(j - t.lanes))
    if isinstance(op, Interleave):
        offset = t.block // 2 if op.high else 0

        def pick(j):
            k = j % t.block
            return (b if k % 2 else a)((j // t.block) * t.block + k // 2 + offset)
        return go_pick(t, pick)
    return None


def go_narrow(t, r):
    """Each lane clamped to r's range, block by block: a's lanes, then b's."""
    f = go_sat_unsigned(r) if t.bits == 64 and not t.signed else go_sat(r)
    arg = '%s' if t.bits == 64 and not t.signed else 'int64(%s)'

    def pick(j):
        k = j % r.block
        v = '%s[%d]' % ('a' if k < t.block else 'b', (j // r.block) * t.block + k % t.block)
        return '%s(%s)' % (f, arg % v)
    return go_pick(r, pick)


def go_convert(t, r):
    if not t.float:
        return go_map(r, lambda i: '%s(a[%s])' % (go_lane(r), i))
    f = go_to_int(t, r)
    return go_map(r, lambda i: '%s(a[%s])' % (f, i))


def go_entries():
    """(name, body) for every value helper, sorted. C writes Shuffle as a
    macro, so ops() leaves it out."""
    out = []
    for op in ops() + [Op('ZI_OP_SHUFFLE', 'shuffle', TYPES, 'shuffle')]:
        if isinstance(op, (Memory, Gather)):
            continue
        if isinstance(op, Narrow):
            for t, r in narrow_pairs():
                out.append(('narrow_%s_%s' % (t.name, r.name), go_narrow(t, r)))
            continue
        if isinstance(op, Reinterpret):
            for t in TYPES:
                for r in TYPES:
                    if r.bytes == t.bytes:
                        body = '\treturn a\n' if r == t else go_reinterpret(t, r)
                        out.append(('reinterpret_%s_%s' % (t.name, r.name), body))
            continue
        if isinstance(op, Convert):
            for t in TYPES:
                for r in TYPES:
                    if r.lanes == t.lanes and r.bits == t.bits and r.float != t.float:
                        out.append(('convert_%s_%s' % (t.name, r.name), go_convert(t, r)))
            continue
        for t in op.types:
            r = op.result(t)
            body = go_body(op, t, r if isinstance(r, T) else None)
            if body is None:
                raise SystemExit('simd_header.py: no Go body for %s' % op.name(t))
            out.append((op.name(t), body))
    return sorted(('zi_' + name, body) for name, body in out)


def c_string(text):
    return '"%s"' % text.replace('\\', '\\\\').replace('"', '\\"').replace('\t', '\\t').replace('\n', '\\n')


def go_header():
    bodies = go_entries()
    out = ['/* Generated by scripts/simd_header.py --go; do not edit. */',
           '/* Typed Go bodies of the std/simd value helpers, sorted by name, and the',
           ' * lane functions they call, for zir_emit_simd_go.c. */',
           '', 'static const GoSimdText go_simd_bodies[] = {']
    for name, body in bodies:
        out.append('    {"%s",\n     %s},' % (name, c_string(body)))
    out += ['};', '', 'static const GoSimdText go_simd_lane_functions[] = {']
    for name in sorted(GO_HELPERS):
        out.append('    {"%s",\n     %s},' % (name, c_string(GO_HELPERS[name])))
    out += ['};', '']
    return '\n'.join(out)


def main():
    if len(sys.argv) > 1 and sys.argv[1] == '--test':
        tiers = [('zi_', 'base', '1')]
        tiers += [('zi_%s_' % n, f, 'ZI_X86 && defined(ZIRAN_SIMD_TIERS)')
                  for n, f in (('ssse3', 'ssse3'), ('sse41', 'sse4.1'), ('avx2', 'avx2'))]
        sys.stdout.write(test_program(tiers))
        return
    if len(sys.argv) > 1 and sys.argv[1] == '--go':
        sys.stdout.write(go_header())
        return
    if len(sys.argv) > 1 and sys.argv[1] == '--types':
        sys.stdout.write(types_header())
        return
    sys.stdout.write(header())


if __name__ == '__main__':
    main()
