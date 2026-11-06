#!/usr/bin/env python3
"""Generate include/ziran_simd_ops.h, the typed C helpers for std/simd.

    python3 scripts/simd_header.py > include/ziran_simd_ops.h
    python3 scripts/simd_header.py --test > build/simd_ops_test.c

Every helper computes exactly what zi_ref in include/ziran_simd.h does. GCC
(12 or later) and Clang builds use native vectors and, where they give the
same bits, SSE2..AVX2, NEON or WebAssembly SIMD128 instructions; any other
build calls zi_ref. --test writes a program that checks every helper, in
every x86-64 tier the build can run, against zi_ref.
"""
import random
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
        (WASM, 'v128_t r_ = wasm_i32x4_splat(0x4000);\n'
               '    v128_t lo_ = wasm_i32x4_shr(wasm_i32x4_add(wasm_i32x4_extmul_low_i16x8((v128_t)a, (v128_t)b), r_), 15);\n'
               '    v128_t hi_ = wasm_i32x4_shr(wasm_i32x4_add(wasm_i32x4_extmul_high_i16x8((v128_t)a, (v128_t)b), r_), 15);\n'
               '    return (%s)wasm_i16x8_shuffle(lo_, hi_, 0, 2, 4, 6, 8, 10, 12, 14);' % t.c),
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
    if t.wide:
        return out
    out.append((NEON if t.bits < 64 else A64, neon(t, 'vabsq_%s' % t.nsuf, 'a')))
    out.append((WASM, wasm(t, 'wasm_%s_abs' % t.wsuf, 'a')))
    return out


def neg_generic(t):
    if t.float:
        return 'return (%s)((%s)a ^ %s);' % (t.c, t.u.c, sign_mask(t))
    return 'return (%s)((%s){0} - (%s)a);' % (t.c, t.u.c, t.u.c)


class Compare(Op):
    def __init__(self, enum, snake, c_op):
        super().__init__(enum, snake, TYPES, 'compare')
        self.c_op = c_op

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

    def portable(self, t):
        values = ', '.join('l%d' % i for i in range(t.lanes))
        return ('#if ZI_VEC\n    return (%s){%s};\n#else\n    %s r_ = {{%s}};\n    return r_;\n#endif' %
                (t.c, values, t.c, values))


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

    def generic(self, t):
        r = self.result(t)
        h = t.half.bytes  # bytes of t's half
        lines = ['typedef %s h_ __attribute__((vector_size(%d)));' % (t.ctype, h), 'h_ h_v;']
        if t.wide:
            off = 8 if self.high else 0
            lines.append('memcpy(&h_v, (const char *)&a + %d, 8);' % off)
            lines.append('memcpy((char *)&h_v + 8, (const char *)&a + %d, 8);' % (16 + off))
        else:
            lines.append('memcpy(&h_v, (const char *)&a + %d, %d);' % (h if self.high else 0, h))
        lines.append('return (%s)__builtin_convertvector(h_v, %s);' % (r.c, r.c))
        return '\n    '.join(lines)

    splittable = False


class Widen(Op):
    def __init__(self):
        super().__init__('ZI_OP_WIDEN', 'widen', [t for t in NARROWABLE if not t.wide], 'widen')

    def result(self, t):
        return t.wider(t.lanes)

    def params(self, t):
        return [(t.c, 'a')]

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
        out = []
        packs = {('s16', 's8'): '_mm_packs_epi16', ('s16', 'u8'): '_mm_packus_epi16',
                 ('s32', 's16'): '_mm_packs_epi32'}
        name = packs.get((t.lane, r.lane))
        tier = X86
        if (t.lane, r.lane) == ('s32', 'u16'):
            name, tier = '_mm_packus_epi32', SSE41
        if name:
            if t.wide:
                return [(AVX2, 'return (%s)%s((__m256i)a, (__m256i)b);' % (r.c, name.replace('_mm_', '_mm256_')))]
            out.append((tier, 'return (%s)%s((__m128i)a, (__m128i)b);' % (r.c, name)))
        if t.wide:
            return out
        if t.signed or r.lane[0] == 'u':
            fn = 'vqmovn' if t.signed == (r.lane[0] == 's') else 'vqmovun'
            if t.signed or fn == 'vqmovn':
                rs = r.half.neon.replace('x%d_t' % r.lanes, 'x%d_t' % (r.lanes // 2))
                rh = '%s%d' % ('s' if r.signed else 'u', r.bits)
                out.append((NEON, 'return (%s)vcombine_%s(%s_%s((%s)a), %s_%s((%s)b));' %
                            (r.c, rh, fn, t.nsuf, t.neon, fn, t.nsuf, t.neon)))
        wasm_names = {('s16', 's8'): 'wasm_i8x16_narrow_i16x8', ('s16', 'u8'): 'wasm_u8x16_narrow_i16x8',
                      ('s32', 's16'): 'wasm_i16x8_narrow_i32x4', ('s32', 'u16'): 'wasm_u16x8_narrow_i32x4'}
        name = wasm_names.get((t.lane, r.lane))
        if name:
            out.append((WASM, 'return (%s)%s((v128_t)a, (v128_t)b);' % (r.c, name)))
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

    def generic(self, t):
        r = self.result(t)
        widen = 'widen_%s_%s' % ('high' if self.high else 'low', t.name)
        return ('%s x_ = ZI_N(%s)(a), y_ = ZI_N(%s)(b);\n    return (%s)((%s)x_ * (%s)y_);' %
                (r.c, widen, widen, r.c, r.u.c, r.u.c))

    splittable = False


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
            (A64, products + '    return (%s)vqaddq_s16(vuzp1q_s16(lo_, hi_), vuzp2q_s16(lo_, hi_));' % r.c),
            (NEON, products + '    int16x8x2_t u_ = vuzpq_s16(lo_, hi_);\n'
                              '    return (%s)vqaddq_s16(u_.val[0], u_.val[1]);' % r.c),
            (WASM, 'v128_t lo_ = wasm_i16x8_mul(wasm_u16x8_extend_low_u8x16((v128_t)a), wasm_i16x8_extend_low_i8x16((v128_t)b));\n'
                   '    v128_t hi_ = wasm_i16x8_mul(wasm_u16x8_extend_high_u8x16((v128_t)a), wasm_i16x8_extend_high_i8x16((v128_t)b));\n'
                   '    return (%s)wasm_i16x8_add_sat(wasm_i16x8_shuffle(lo_, hi_, 0, 2, 4, 6, 8, 10, 12, 14),\n'
                   '        wasm_i16x8_shuffle(lo_, hi_, 1, 3, 5, 7, 9, 11, 13, 15));' % r.c),
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
        if t.wide:
            return out
        out.append((NEON, 'return (%s)vpaddlq_%s((%s)a);' % (r.c, t.nsuf, t.neon)))
        if t.bits < 32:
            out.append((WASM, 'return (%s)wasm_%s_extadd_pairwise_%s((v128_t)a);' %
                        (r.c, r.wsuf, t.wsuf)))
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


class Reduce(Op):
    def __init__(self, enum, snake, kind):
        super().__init__(enum, snake, INT, 'reduce')
        self.kind = kind

    def result(self, t):
        return 'int64_t' if self.kind == 'max_index' else t.ctype

    def params(self, t):
        return [(t.c, 'a')]

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

    def portable(self, t):
        return '%s r_;\n    memcpy(&r_, (const char *)&a + %d, 16);\n    return r_;' % (t.half.c, 16 if self.high else 0)

    splittable = False


class Combine(Op):
    def __init__(self):
        super().__init__('ZI_OP_COMBINE', 'combine', [t for t in TYPES if not t.wide], 'combine')

    def result(self, t):
        return t.double

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


# ---------------------------------------------------------------- output

def function(out, ret, name, params, bodies):
    """bodies: [(condition or None, lines)] in order; None is the final #else."""
    ret_c = ret.c if isinstance(ret, T) else ret
    plist = ', '.join('%s %s' % p for p in params) or 'void'
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


def plan(op, t):
    """The function bodies for (op, t)."""
    bodies = []
    specials = op.special(t)
    if t.wide:
        bodies += [(c, b) for c, b in specials if 'ZI_TIER >= 4' in c]
        half_specials = op.special(t.half) if op.splittable else []
        if half_specials:
            conds = sorted(set('(%s)' % c for c, _ in half_specials))
            r = op.result(t)
            hn = op.name(t.half)
            lo = ', '.join('ZI_N(low_%s)(%s)' % (c[3:], n) if c.startswith('zi_') else n
                           for c, n in op.params(t))
            hi = ', '.join('ZI_N(high_%s)(%s)' % (c[3:], n) if c.startswith('zi_') else n
                           for c, n in op.params(t))
            body = 'return ZI_N(combine_%s)(ZI_N(%s)(%s), ZI_N(%s)(%s));' % (
                r.half.name, hn, lo, hn, hi)
            bodies.append(('ZI_VEC && (%s)' % ' || '.join(conds), body))
    else:
        bodies += specials
    generic = op.generic(t)
    if generic is not None:
        bodies.append(('ZI_VEC', generic))
    portable = op.portable(t)
    fallback = portable if portable is not None else '\n    '.join(ref_body(op, t))
    bodies.append((None, fallback))
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
                bodies.append(('ZI_VEC', op.generic_pair(t, r)))
                ref = ['uint64_t x_[32], y_[32], o_[32];', 'zi_unpack_%s(a, x_);' % t.name,
                       'zi_unpack_%s(b, y_);' % t.name,
                       'zi_ref(ZI_OP_NARROW, %s, %d, %s, %d, x_, y_, 0, 0, o_);' % (r.kind, r.lanes, t.kind, t.lanes),
                       'return zi_pack_%s(o_);' % r.name]
                bodies.append((None, '\n    '.join(ref)))
                out.append((op, t, r, 'narrow_%s_%s' % (t.name, r.name), [(t.c, 'a'), (t.c, 'b')], bodies))
            continue
        if isinstance(op, Reinterpret):
            for t in TYPES:
                for r in TYPES:
                    if r.bytes != t.bytes or r == t:
                        continue
                    ref = ['uint64_t x_[32], o_[32];', 'zi_unpack_%s(a, x_);' % t.name,
                           'zi_ref(ZI_OP_REINTERPRET, %s, %d, %s, %d, x_, 0, 0, 0, o_);' % (r.kind, r.lanes, t.kind, t.lanes),
                           'return zi_pack_%s(o_);' % r.name]
                    out.append((op, t, r, 'reinterpret_%s_%s' % (t.name, r.name), [(t.c, 'a')],
                                [('ZI_VEC', 'return (%s)a;' % r.c), (None, '\n    '.join(ref))]))
            continue
        if isinstance(op, Convert):
            for t in TYPES:
                for r in TYPES:
                    if r.lanes != t.lanes or r.bits != t.bits or r.float == t.float:
                        continue
                    ref = ['uint64_t x_[32], o_[32];', 'zi_unpack_%s(a, x_);' % t.name,
                           'zi_ref(ZI_OP_CONVERT, %s, %d, %s, %d, x_, 0, 0, 0, o_);' % (r.kind, r.lanes, t.kind, t.lanes),
                           'return zi_pack_%s(o_);' % r.name]
                    out.append((op, t, r, 'convert_%s_%s' % (t.name, r.name), [(t.c, 'a')],
                                [('ZI_VEC', convert_generic(t, r)), (None, '\n    '.join(ref))]))
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
        out.append('#define zi_align_bytes_%s(a, b, n) zi_bytes_ref_%s(ZI_OP_ALIGN_BYTES, a, b, n)' % (t.name, t.name))
        out.append('#define zi_shr_bytes_%s(a, n) zi_bytes_ref_%s(ZI_OP_SHR_BYTES, a, a, n)' % (t.name, t.name))
        out.append('#define zi_shl_bytes_%s(a, n) zi_bytes_ref_%s(ZI_OP_SHL_BYTES, a, a, n)' % (t.name, t.name))
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
        out.append('#define zi_shuffle_%s(a, %s) zi_shuffle_ref_%s(a, (const int64_t[]){%s})' %
                   (t.name, params, t.name, params))
        out.append('#endif')


def common(out):
    out.append('#ifndef ZIRAN_SIMD_OPS_COMMON_H')
    out.append('#define ZIRAN_SIMD_OPS_COMMON_H')
    out.append('#if ZI_VEC')
    out.append('#define ZI_AT(v, i) (v)[i]')
    for t in TYPES:
        out.append('typedef %s %s __attribute__((vector_size(%d), aligned(%d)));' %
                   (t.ctype, t.c, t.bytes, t.bits // 8))
    # Concatenations Narrow builds before truncating.
    for t in TYPES:
        if t.float or t.bits == 8:
            continue
        out.append('typedef %s zi_w_%s __attribute__((vector_size(%d)));' % (t.u.ctype, t.name, t.bytes * 2))
    out.append('#else')
    out.append('#define ZI_AT(x, i) (x).v[i]')
    for t in TYPES:
        out.append('typedef struct { %s v[%d]; } %s;' % (t.ctype, t.lanes, t.c))
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
    for t in TYPES:
        out.append('ZI_INLINE void zi_unpack_%s(%s x, uint64_t *o)\n{\n'
                   '    for(int i = 0; i < %d; i++) o[i] = zi_bits_%s(ZI_AT(x, i));\n}' % (t.name, t.c, t.lanes, t.lane))
        out.append('ZI_INLINE %s zi_pack_%s(const uint64_t *o)\n{\n    %s x;\n'
                   '    for(int i = 0; i < %d; i++) ZI_AT(x, i) = zi_from_bits_%s(o[i]);\n    return x;\n}' %
                   (t.c, t.name, t.c, t.lanes, t.lane))
    out.append('')
    out.append('/* Lane arrays run the constant-operand macros through zi_ref. */')
    out.append('#if !ZI_VEC')
    for t in INT:
        out.append('static inline %s zi_bytes_ref_%s(int op, %s a, %s b, int64_t n)\n{\n'
                   '    uint64_t x_[32], y_[32], o_[32];\n    int64_t k_[32] = {0};\n'
                   '    zi_unpack_%s(a, x_);\n    zi_unpack_%s(b, y_);\n    k_[0] = n;\n'
                   '    zi_ref(op, %s, %d, %s, %d, x_, y_, 0, k_, o_);\n    return zi_pack_%s(o_);\n}' %
                   (t.c, t.name, t.c, t.c, t.name, t.name, t.kind, t.lanes, t.kind, t.lanes, t.name))
    for t in TYPES:
        out.append('static inline %s zi_shuffle_ref_%s(%s a, const int64_t *k)\n{\n'
                   '    uint64_t x_[32], o_[32];\n    zi_unpack_%s(a, x_);\n'
                   '    zi_ref(ZI_OP_SHUFFLE, %s, %d, %s, %d, x_, 0, 0, k, o_);\n    return zi_pack_%s(o_);\n}' %
                   (t.c, t.name, t.c, t.name, t.kind, t.lanes, t.kind, t.lanes, t.name))
    out.append('#endif')
    out.append('/* Byte i of block `block` in AlignBytes: from low:high shifted by n. */')
    out.append('#define ZI_BYTE_FROM(block, i, size, n) ((i) + (n) < 16 ? (block) * 16 + (i) + (n) : (size) + (block) * 16 + (i) + (n) - 16)')
    macros(out)
    out.append('#endif /* ZIRAN_SIMD_OPS_COMMON_H */')


def header():
    out = ['/* Generated by scripts/simd_header.py; do not edit. */',
           '/* Included by ziran_simd.h once for the build target and once for each',
           ' * x86-64 tier, with ZI_TIER, ZI_FN and ZI_N(name) set. */', '']
    common(out)
    out.append('')
    items = entries()
    out.append('/* ---- operations for ZI_TIER ---- */')
    for op, t, ret, name, params, bodies in items:
        ret_c = ret.c if isinstance(ret, T) else ret
        plist = ', '.join('%s %s' % p for p in params) or 'void'
        out.append('ZI_FN %s ZI_N(%s)(%s);' % (ret_c, name, plist))
    out.append('')
    for op, t, ret, name, params, bodies in items:
        function(out, ret, name, params, bodies)
    return '\n'.join(out) + '\n'


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


def main():
    if len(sys.argv) > 1 and sys.argv[1] == '--test':
        tiers = [('zi_', 'base', '1')]
        tiers += [('zi_%s_' % n, f, 'ZI_X86 && defined(ZIRAN_SIMD_TIERS)')
                  for n, f in (('ssse3', 'ssse3'), ('sse41', 'sse4.1'), ('avx2', 'avx2'))]
        sys.stdout.write(test_program(tiers))
        return
    sys.stdout.write(header())


if __name__ == '__main__':
    main()
