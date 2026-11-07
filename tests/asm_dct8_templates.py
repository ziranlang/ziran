"""The InverseDct8x8 #asm templates of tests/spec/asm_dct8.zi: x86-64 SSSE3,
AArch64 and ARMv7 NEON from one description of the transform, with the
registers each step frees reused by the next.

python3 tests/asm_dct8_templates.py FILE checks FILE's templates;
--write rewrites them.
"""
import sys

# Rotation keys: (cos, sin) and their lanes/table offsets.
KEYS = {'799': (799, 4017), '3406': (3406, 2276), '2896': (2896, 2896), '1567': (1567, 3784)}
# ARM constant lanes: [799, 4017, 3406, 2276, 2896, 1567, 3784, 0]
LANE = {799: 0, 4017: 1, 3406: 2, 2276: 3, 2896: 4, 1567: 5, 3784: 6}
# x86 table offsets (bytes): pmaddwd pairs for x = a*c - b*s and y = a*s + b*c.
OFFSETS = {'799': (16, 32), '3406': (48, 64), '2896': (80, 96), '1567': (112, 128)}
ROUND, HALF, SIXTEENTH = 144, 160, 176

def dct8(be, p):
    T = [p[0], p[4], p[2], p[6], p[1], p[5], p[3], p[7]]
    T[4], T[7] = be.butterfly(T[4], T[7], '799')
    T[5], T[6] = be.butterfly(T[5], T[6], '3406')
    T[1], T[0] = be.butterfly(T[0], T[1], '2896')
    T[2], T[3] = be.butterfly(T[2], T[3], '1567')
    T[4], T[5] = be.hadamard(T[4], T[5])
    T[7], T[6] = be.hadamard(T[7], T[6])
    T[0], T[3] = be.hadamard(T[0], T[3])
    T[1], T[2] = be.hadamard(T[1], T[2])
    T[5], T[6] = be.butterfly(T[6], T[5], '2896')
    out = [None] * 8
    for i in range(4):
        out[i], out[7 - i] = be.hadamard(T[i], T[7 - i])
    return out

def program(be):
    be.prologue()
    rows = [be.load_row(i) for i in range(8)]
    out = dct8(be, be.transpose(rows))
    be.round_shift(out, 1)
    out = dct8(be, be.transpose(out))
    be.round_shift(out, 4)
    be.finish(out)
    return be.text()

class Backend:
    def __init__(self, pool):
        self.free = list(pool)
        self.lines = []
    def alloc(self):
        return self.free.pop(0)
    def release(self, r):
        assert r not in self.free
        self.free.append(r)
    def emit(self, s):
        self.lines.append('    ' + s)
    def label(self, s):
        self.lines.append(s)
    def text(self):
        return '\n'.join(self.lines)

class X86(Backend):
    def __init__(self):
        super().__init__(['%%%%xmm%d' % i for i in list(range(12)) + [15]])
        self.R, self.MAX, self.MIN = '%%xmm12', '%%xmm13', '%%xmm14'
    def e(self, op, *args):
        self.emit(op + ' ' + ', '.join(args))
    def track(self, r):
        self.e('pmaxsw', r, self.MAX); self.e('pminsw', r, self.MIN)
    def prologue(self):
        self.e('movdqu', '%d(%%[table])' % ROUND, self.R)
        self.e('pxor', self.MAX, self.MAX); self.e('pxor', self.MIN, self.MIN)
    def load_row(self, i):
        v, t = self.alloc(), self.alloc()
        self.e('movdqu', '%d(%%[dequant])' % (32 * i) if i else '(%[dequant])', v)
        self.e('movdqu', '%d(%%[dequant])' % (32 * i + 16), t)
        self.e('packssdw', t, v)
        self.release(t); self.track(v)
        return v
    def pair(self, lo, hi, a, b):
        t = self.alloc()
        self.e('movdqa', a, t); self.e(lo, b, a); self.e(hi, b, t)
        self.release(b)
        return a, t
    def transpose(self, r):
        A = [None] * 8
        for m in range(4):
            A[2 * m], A[2 * m + 1] = self.pair('punpcklwd', 'punpckhwd', r[2 * m], r[2 * m + 1])
        B = [None] * 8
        for (o, a, b) in ((0, 0, 2), (2, 1, 3), (4, 4, 6), (6, 5, 7)):
            B[o], B[o + 1] = self.pair('punpckldq', 'punpckhdq', A[a], A[b])
        C = [None] * 8
        for (o, a, b) in ((0, 0, 4), (2, 1, 5), (4, 2, 6), (6, 3, 7)):
            C[o], C[o + 1] = self.pair('punpcklqdq', 'punpckhqdq', B[a], B[b])
        return C
    def butterfly(self, a, b, key):
        kx, ky = OFFSETS[key]
        t0, t1, k = self.alloc(), self.alloc(), self.alloc()
        self.e('movdqa', a, t0); self.e('punpcklwd', b, t0); self.e('punpckhwd', b, a)
        self.e('movdqa', t0, b); self.e('movdqa', a, t1)
        self.e('movdqu', '%d(%%[table])' % kx, k); self.e('pmaddwd', k, t0); self.e('pmaddwd', k, a)
        self.e('movdqu', '%d(%%[table])' % ky, k); self.e('pmaddwd', k, b); self.e('pmaddwd', k, t1)
        for r in (t0, a, b, t1): self.e('paddd', self.R, r)
        for r in (t0, a, b, t1): self.e('psrad', '$12', r)
        self.e('packssdw', a, t0); self.e('packssdw', t1, b)
        self.track(t0); self.track(b)
        self.release(a); self.release(t1); self.release(k)
        return t0, b
    def hadamard(self, a, b):
        t = self.alloc()
        self.e('movdqa', a, t); self.e('paddsw', b, a); self.e('psubsw', b, t)
        self.release(b)
        return a, t
    def round_shift(self, regs, bits):
        k = self.alloc()
        self.e('movdqu', '%d(%%[table])' % (HALF if bits == 1 else SIXTEENTH), k)
        for r in regs: self.e('pmulhrsw', k, r)
        self.release(k)
    def finish(self, rows):
        k, t = self.alloc(), self.alloc()
        self.e('pcmpeqd', k, k); self.e('psrlw', '$1', k); self.e('pcmpeqw', self.MAX, k)
        self.e('pcmpeqd', t, t); self.e('psllw', '$15', t); self.e('pcmpeqw', self.MIN, t)
        self.e('por', t, k); self.e('pmovmskb', k, '%%eax')
        self.emit('movl $0, %[out]')
        self.emit('testl %%eax, %%eax')
        self.emit('jnz 9f')
        for i, v in enumerate(rows):
            self.e('movdqa', v, t); self.e('punpcklwd', v, t); self.e('psrad', '$16', t)
            self.e('punpckhwd', v, v); self.e('psrad', '$16', v)
            self.e('movdqu', t, '%d(%%[residual])' % (32 * i) if i else '(%[residual])')
            self.e('movdqu', v, '%d(%%[residual])' % (32 * i + 16))
        self.emit('movl $1, %[out]')
        self.label('9:')

class A64(Backend):
    def __init__(self):
        super().__init__(['v%d' % i for i in list(range(1, 8)) + list(range(16, 28))])
    def track(self, r):
        self.emit('smax v28.8h, v28.8h, %s.8h' % r); self.emit('smin v29.8h, v29.8h, %s.8h' % r)
    def prologue(self):
        self.emit('ld1 {v0.8h}, [%[table]]')
        self.emit('movi v28.8h, #0'); self.emit('movi v29.8h, #0')
    def load_row(self, i):
        v, a, b = self.alloc(), self.alloc(), self.alloc()
        self.emit('ldp q%s, q%s, [%%[dequant], #%d]' % (a[1:], b[1:], 32 * i))
        self.emit('sqxtn %s.4h, %s.4s' % (v, a)); self.emit('sqxtn2 %s.8h, %s.4s' % (v, b))
        self.release(a); self.release(b); self.track(v)
        return v
    def step(self, size, a, b):
        t0, t1 = self.alloc(), self.alloc()
        self.emit('trn1 %s.%s, %s.%s, %s.%s' % (t0, size, a, size, b, size))
        self.emit('trn2 %s.%s, %s.%s, %s.%s' % (t1, size, a, size, b, size))
        self.release(a); self.release(b)
        return t0, t1
    def transpose(self, r):
        P = [None] * 8
        for m in range(4):
            P[2 * m], P[2 * m + 1] = self.step('8h', r[2 * m], r[2 * m + 1])
        Q = [None] * 8
        for base in (0, 4):
            Q[base], Q[base + 2] = self.step('4s', P[base], P[base + 2])
            Q[base + 1], Q[base + 3] = self.step('4s', P[base + 1], P[base + 3])
        C = [None] * 8
        for k in range(4):
            C[k], C[k + 4] = self.step('2d', Q[k], Q[k + 4])
        return C
    def butterfly(self, a, b, key):
        c, s = KEYS[key]
        c, s = 'v0.h[%d]' % LANE[c], 'v0.h[%d]' % LANE[s]
        t = [self.alloc() for _ in range(4)]
        self.emit('smull %s.4s, %s.4h, %s' % (t[0], a, c)); self.emit('smull2 %s.4s, %s.8h, %s' % (t[1], a, c))
        self.emit('smlsl %s.4s, %s.4h, %s' % (t[0], b, s)); self.emit('smlsl2 %s.4s, %s.8h, %s' % (t[1], b, s))
        self.emit('smull %s.4s, %s.4h, %s' % (t[2], a, s)); self.emit('smull2 %s.4s, %s.8h, %s' % (t[3], a, s))
        self.emit('smlal %s.4s, %s.4h, %s' % (t[2], b, c)); self.emit('smlal2 %s.4s, %s.8h, %s' % (t[3], b, c))
        self.emit('sqrshrn %s.4h, %s.4s, #12' % (a, t[0])); self.emit('sqrshrn2 %s.8h, %s.4s, #12' % (a, t[1]))
        self.emit('sqrshrn %s.4h, %s.4s, #12' % (b, t[2])); self.emit('sqrshrn2 %s.8h, %s.4s, #12' % (b, t[3]))
        self.track(a); self.track(b)
        for r in t: self.release(r)
        return a, b
    def hadamard(self, a, b):
        t = self.alloc()
        self.emit('sqadd %s.8h, %s.8h, %s.8h' % (t, a, b)); self.emit('sqsub %s.8h, %s.8h, %s.8h' % (b, a, b))
        self.release(a)
        return t, b
    def round_shift(self, regs, bits):
        for r in regs: self.emit('srshr %s.8h, %s.8h, #%d' % (r, r, bits))
    def finish(self, rows):
        self.emit('mvni v30.8h, #0x80, lsl #8'); self.emit('movi v31.8h, #0x80, lsl #8')
        self.emit('cmeq v30.8h, v28.8h, v30.8h'); self.emit('cmeq v31.8h, v29.8h, v31.8h')
        self.emit('orr v30.16b, v30.16b, v31.16b'); self.emit('umaxv h30, v30.8h')
        self.emit('umov w9, v30.h[0]')
        self.emit('mov %w[out], #0')
        self.emit('cbnz w9, 9f')
        for i, v in enumerate(rows):
            a, b = self.alloc(), self.alloc()
            self.emit('sxtl %s.4s, %s.4h' % (a, v)); self.emit('sxtl2 %s.4s, %s.8h' % (b, v))
            self.emit('stp q%s, q%s, [%%[residual], #%d]' % (a[1:], b[1:], 32 * i))
            self.release(a); self.release(b)
        self.emit('mov %w[out], #1')
        self.label('9:')

class Arm(Backend):
    def __init__(self):
        super().__init__(['q%d' % i for i in range(3, 12)])
    @staticmethod
    def lo(q): return 'd%d' % (2 * int(q[1:]))
    @staticmethod
    def hi(q): return 'd%d' % (2 * int(q[1:]) + 1)
    def track(self, r):
        self.emit('vmax.s16 q1, q1, %s' % r); self.emit('vmin.s16 q2, q2, %s' % r)
    def prologue(self):
        self.emit('vld1.16 {d0-d1}, [%[table]]')
        self.emit('vmov.i16 q1, #0'); self.emit('vmov.i16 q2, #0')
        self.emit('mov r12, %[dequant]')
    def load_row(self, i):
        v = self.alloc()
        self.emit('vld1.32 {d24-d27}, [r12]!')
        self.emit('vqmovn.s32 %s, q12' % self.lo(v)); self.emit('vqmovn.s32 %s, q13' % self.hi(v))
        self.track(v)
        return v
    def transpose(self, r):
        for m in range(4): self.emit('vtrn.16 %s, %s' % (r[2 * m], r[2 * m + 1]))
        for a, b in ((0, 2), (1, 3), (4, 6), (5, 7)): self.emit('vtrn.32 %s, %s' % (r[a], r[b]))
        for k in range(4): self.emit('vswp %s, %s' % (self.hi(r[k]), self.lo(r[k + 4])))
        return list(r)
    def butterfly(self, a, b, key):
        c, s = KEYS[key]
        c, s = 'd%d[%d]' % divmod(LANE[c], 4), 'd%d[%d]' % divmod(LANE[s], 4)
        A, B = (self.lo(a), self.hi(a)), (self.lo(b), self.hi(b))
        self.emit('vmull.s16 q12, %s, %s' % (A[0], c)); self.emit('vmull.s16 q13, %s, %s' % (A[1], c))
        self.emit('vmlsl.s16 q12, %s, %s' % (B[0], s)); self.emit('vmlsl.s16 q13, %s, %s' % (B[1], s))
        self.emit('vmull.s16 q14, %s, %s' % (A[0], s)); self.emit('vmull.s16 q15, %s, %s' % (A[1], s))
        self.emit('vmlal.s16 q14, %s, %s' % (B[0], c)); self.emit('vmlal.s16 q15, %s, %s' % (B[1], c))
        self.emit('vqrshrn.s32 %s, q12, #12' % A[0]); self.emit('vqrshrn.s32 %s, q13, #12' % A[1])
        self.emit('vqrshrn.s32 %s, q14, #12' % B[0]); self.emit('vqrshrn.s32 %s, q15, #12' % B[1])
        self.track(a); self.track(b)
        return a, b
    def hadamard(self, a, b):
        t = self.alloc()
        self.emit('vqadd.s16 %s, %s, %s' % (t, a, b)); self.emit('vqsub.s16 %s, %s, %s' % (b, a, b))
        self.release(a)
        return t, b
    def round_shift(self, regs, bits):
        for r in regs: self.emit('vrshr.s16 %s, %s, #%d' % (r, r, bits))
    def finish(self, rows):
        self.emit('vmvn.i16 q12, #0x8000'); self.emit('vmov.i16 q13, #0x8000')
        self.emit('vceq.i16 q12, q1, q12'); self.emit('vceq.i16 q13, q2, q13')
        self.emit('vorr q12, q12, q13'); self.emit('vorr d24, d24, d25')
        self.emit('vmov r2, r3, d24'); self.emit('orrs r2, r2, r3')
        self.emit('mov %[out], #0')
        self.emit('bne 9f')
        self.emit('mov r12, %[residual]')
        for v in rows:
            self.emit('vmovl.s16 q12, %s' % self.lo(v)); self.emit('vmovl.s16 q13, %s' % self.hi(v))
            self.emit('vst1.32 {d24-d27}, [r12]!')
        self.emit('mov %[out], #1')
        self.label('9:')

VARIANTS = (('x86_64+ssse3', X86), ('aarch64', A64), ('arm+neon', Arm))


def replace(text, write):
    for target, backend in VARIANTS:
        start = text.index('#asm("%s", #string END\n' % target)
        body = text.index('\n', start) + 1
        end = text.index('\nEND,', body)
        generated = program(backend())
        if text[body:end] != generated:
            if not write:
                sys.exit('%s: the %s template differs from tests/asm_dct8_templates.py' % (sys.argv[-1], target))
            text = text[:body] + generated + text[end:]
    return text


if __name__ == '__main__':
    write = sys.argv[1:2] == ['--write']
    path = sys.argv[-1]
    text = open(path).read()
    updated = replace(text, write)
    if write and updated != text:
        open(path, 'w').write(updated)
