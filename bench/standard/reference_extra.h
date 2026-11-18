/* Independent scalar oracles for the extended workload set. */
static I Digest(I hash, I value) {
    return (hash * 31 + value) % 1000000007;
}

static uint32_t Next(uint32_t state) {
    return state * 1664525u + 1013904223u;
}

static void FillBytes(uint8_t *bytes, I size) {
    uint32_t state = 42;
    for (I i = 0; i < size; ++i) {
        state = Next(state);
        bytes[i] = (uint8_t)(state >> 24);
    }
}

static I KNucleotide(I size) {
    uint8_t *bytes = malloc(size);
    I counts[16384] = {0};
    FillBytes(bytes, size);
    unsigned rolling = 0;
    for (I i = 0; i < size; ++i) {
        rolling = ((rolling << 2) | (bytes[i] >> 6)) & 16383;
        if (i >= 6) ++counts[rolling];
    }
    free(bytes);
    I hash = 0;
    for (int i = 0; i < 16384; ++i) hash = Digest(hash, counts[i]);
    return hash;
}

static I ReverseComplement(I size) {
    uint8_t *bytes = malloc(size), *output = malloc(size);
    FillBytes(bytes, size);
    for (I i = 0; i < size; ++i) output[i] = "TGCA"[bytes[size - i - 1] >> 6];
    I hash = 0;
    for (I i = 0; i < size; ++i) hash = Digest(hash, output[i]);
    free(bytes);
    free(output);
    return hash;
}

static I PiDigits(I digits) {
    I length = digits * 10 / 3 + 2;
    I *limbs = malloc(length * sizeof(*limbs));
    for (I i = 0; i < length; ++i) limbs[i] = 2;
    I pending = 0, nines = 0, hash = 0;
    for (I step = 0; step < digits; ++step) {
        I carry = 0;
        for (I i = length; i > 0; --i) {
            I value = 10 * limbs[i - 1] + carry * i;
            limbs[i - 1] = value % (2 * i - 1);
            carry = value / (2 * i - 1);
        }
        limbs[0] = carry % 10;
        carry /= 10;
        if (carry == 9) { ++nines; continue; }
        I correction = carry == 10;
        hash = Digest(hash, pending + correction);
        for (; nines > 0; --nines) hash = Digest(hash, correction ? 0 : 9);
        pending = correction ? 0 : carry;
    }
    hash = Digest(hash, pending);
    for (; nines > 0; --nines) hash = Digest(hash, 9);
    free(limbs);
    return hash;
}

static const char *JsonSpace(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
    return p;
}

static char *Decimal(char *out, I number) {
    if (number < 0) { *out++ = '-'; number = -number; }
    I place = 1;
    while (place <= number / 10) place *= 10;
    for (; place; place /= 10) {
        *out++ = (char)('0' + number / place);
        number %= place;
    }
    return out;
}

static I JsonRoundtrip(I count) {
    const char *inputs[] = {
        " { \"width\" : 640, \"height\" : 480, \"offset\": -56 } ",
        "{\"left\":-7,\"top\":0,\"right\":1920,\"bottom\":1080}",
        "{ \"rows\": 12, \"columns\": 75, \"capacity\": 1024, \"delta\": -10 } ",
    };
    I hash = 0;
    for (I i = 0; i < count; ++i) {
        const char *p = JsonSpace(inputs[i % 3]);
        if (*p++ != '{') return -1;
        char output[96], *out = output;
        *out++ = '{';
        do {
            p = JsonSpace(p);
            if (*p != '"') return -1;
            *out++ = *p++;
            while (*p >= 'a' && *p <= 'z') *out++ = *p++;
            if (*p != '"') return -1;
            *out++ = *p++;
            p = JsonSpace(p);
            if (*p++ != ':') return -1;
            *out++ = ':';
            p = JsonSpace(p);
            int negative = *p == '-';
            p += negative;
            const char *start = p;
            I value = 0;
            while (*p >= '0' && *p <= '9') value = value * 10 + *p++ - '0';
            if (p == start) return -1;
            out = Decimal(out, negative ? -value : value);
            p = JsonSpace(p);
            if (*p != ',') break;
            *out++ = *p++;
        } while (1);
        if (*p++ != '}' || *JsonSpace(p)) return -1;
        *out++ = '}';
        for (char *c = output; c < out; ++c) hash = Digest(hash, (uint8_t)*c);
    }
    return hash;
}

static I Lz77(I size) {
    uint8_t *bytes = malloc(size);
    FillBytes(bytes, size);
    for (I i = 0; i < size; ++i) if (i % 256 >= 48) bytes[i] = i % 31;
    I last[4096] = {0}, hash = 0;
    for (I at = 0; at < size;) {
        I length = 0, distance = 0;
        if (at + 2 < size) {
            unsigned key = (bytes[at] * 251 + bytes[at + 1] * 31 + bytes[at + 2]) & 4095;
            I previous = last[key] - 1;
            last[key] = at + 1;
            distance = at - previous;
            if (previous >= 0 && distance <= 32768) {
                while (length < 258 && at + length < size && bytes[previous + length] == bytes[at + length]) ++length;
            }
        }
        if (length >= 3) {
            hash = Digest(Digest(Digest(hash, 256), length), distance);
            at += length;
        } else hash = Digest(hash, bytes[at++]);
    }
    free(bytes);
    return hash;
}

static uint32_t Ror(uint32_t word, unsigned count) {
    return (word >> count) | (word << (32 - count));
}

static I Sha256(I size) {
    static const uint32_t constant[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
    };
    uint32_t state[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    I padded = (size + 72) / 64 * 64;
    uint8_t *bytes = calloc(padded, 1);
    FillBytes(bytes, size);
    bytes[size] = 128;
    for (int i = 0; i < 8; ++i) bytes[padded - 1 - i] = (uint64_t)size * 8 >> (i * 8);
    for (I offset = 0; offset < padded; offset += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            const uint8_t *p = bytes + offset + 4 * i;
            w[i] = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
        }
        for (int i = 16; i < 64; ++i) {
            uint32_t x = w[i - 15], y = w[i - 2];
            w[i] = w[i - 16] + w[i - 7] + (Ror(x, 7) ^ Ror(x, 18) ^ (x >> 3)) + (Ror(y, 17) ^ Ror(y, 19) ^ (y >> 10));
        }
        uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
        uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
        for (int i = 0; i < 64; ++i) {
            uint32_t big0 = Ror(a, 2) ^ Ror(a, 13) ^ Ror(a, 22), big1 = Ror(e, 6) ^ Ror(e, 11) ^ Ror(e, 25);
            uint32_t first = h + big1 + ((e & f) ^ (~e & g)) + constant[i] + w[i];
            uint32_t second = big0 + ((a & b) ^ (a & c) ^ (b & c));
            h = g; g = f; f = e; e = d + first;
            d = c; c = b; b = a; a = first + second;
        }
        state[0] += a; state[1] += b; state[2] += c; state[3] += d;
        state[4] += e; state[5] += f; state[6] += g; state[7] += h;
    }
    free(bytes);
    I hash = 0;
    for (int i = 0; i < 8; ++i) hash = Digest(hash, state[i]);
    return hash;
}

static I Crc32(I size) {
    uint8_t *bytes = malloc(size);
    FillBytes(bytes, size);
    uint32_t table[256], crc = 0xffffffffu;
    for (unsigned i = 0; i < 256; ++i) {
        uint32_t word = i;
        for (int bit = 0; bit < 8; ++bit) word = (word >> 1) ^ (0xedb88320u & (0u - (word & 1)));
        table[i] = word;
    }
    for (I i = 0; i < size; ++i) crc = (crc >> 8) ^ table[(crc ^ bytes[i]) & 255];
    free(bytes);
    return ~crc;
}

static int SudokuFill(I *grid, I *masks) {
    int cell = -1, fewest = 10, choices = 0;
    for (int i = 0; i < 81; ++i) {
        if (grid[i]) continue;
        int row = i / 9, col = i % 9, box = row / 3 * 3 + col / 3;
        int available = 511 & ~(masks[row] | masks[9 + col] | masks[18 + box]);
        int count = 0;
        for (int bits = available; bits; bits &= bits - 1) ++count;
        if (!count) return 0;
        if (count < fewest) { cell = i; fewest = count; choices = available; }
    }
    if (cell < 0) return 1;
    int row = cell / 9, col = cell % 9, box = row / 3 * 3 + col / 3;
    for (int digit = 0; digit < 9; ++digit) {
        int bit = 1 << digit;
        if (!(choices & bit)) continue;
        grid[cell] = digit + 1;
        masks[row] ^= bit; masks[9 + col] ^= bit; masks[18 + box] ^= bit;
        if (SudokuFill(grid, masks)) return 1;
        grid[cell] = 0;
        masks[row] ^= bit; masks[9 + col] ^= bit; masks[18 + box] ^= bit;
    }
    return 0;
}

static I Sudoku(I count) {
    const char *puzzle = "800000000003600000070090200050007000000045700000100030001000068008500010090000400";
    I hash = 0;
    for (I pass = 0; pass < count; ++pass) {
        I grid[81] = {0}, masks[27] = {0};
        for (int i = 0; i < 81; ++i) {
            if (puzzle[i] == '0') continue;
            I digit = (puzzle[i] - '1' + pass % 9) % 9;
            I bit = 1 << digit;
            int row = i / 9, col = i % 9, box = row / 3 * 3 + col / 3;
            grid[i] = digit + 1;
            masks[row] |= bit; masks[9 + col] |= bit; masks[18 + box] |= bit;
        }
        if (!SudokuFill(grid, masks)) return -1;
        for (int i = 0; i < 81; ++i) hash = Digest(hash, grid[i]);
    }
    return hash;
}

static void Relax(I *dist, I from, I to) {
    I cost = dist[from] + (from * 31 + to * 17) % 23 + 1;
    if (cost < dist[to]) dist[to] = cost;
}

static I Dijkstra(I width) {
    I count = width * width;
    I *dist = malloc(count * sizeof(*dist));
    uint8_t *seen = calloc(count, 1);
    dist[0] = 0;
    for (I i = 1; i < count; ++i) dist[i] = 1000000007;
    for (I step = 0; step < count; ++step) {
        I closest = 1000000007, curr = -1;
        for (I i = 0; i < count; ++i) {
            if (!seen[i] && dist[i] < closest) { curr = i; closest = dist[i]; }
        }
        if (curr < 0) break;
        seen[curr] = 1;
        if (curr % width) Relax(dist, curr, curr - 1);
        if (curr % width + 1 < width) Relax(dist, curr, curr + 1);
        if (curr >= width) Relax(dist, curr, curr - width);
        if (curr + width < count) Relax(dist, curr, curr + width);
    }
    I hash = 0;
    for (I i = 0; i < count; ++i) hash = Digest(hash, dist[i]);
    free(seen);
    free(dist);
    return hash;
}

static I WordCount(I count) {
    struct { uint32_t key; I count; } table[4096] = {0};
    const char words[] = "ant bee crow doe eel fox gnat hare ibis jay kite lion mole newt orca pike quail rook seal tern vole wasp yak zebra ";
    uint32_t state = 42;
    for (I i = 0; i < count; ++i) {
        state = Next(state);
        size_t at = (state >> 16) % (sizeof(words) - 1);
        while (at && words[at - 1] != ' ') --at;
        uint32_t key = 0;
        while (words[at] != ' ') key = key * 31 + (uint8_t)words[at++];
        key = key * 64 + ((state >> 25) & 63);
        unsigned slot = (key ^ (key >> 13)) & 4095;
        while (table[slot].count && table[slot].key != key) slot = (slot + 1) & 4095;
        table[slot].key = key;
        ++table[slot].count;
    }
    I hash = 0;
    for (int i = 0; i < 4096; ++i) hash = Digest(Digest(hash, table[i].key), table[i].count);
    return hash;
}

static void RadixPass(const uint32_t *from, uint32_t *to, I count, int shift) {
    I offsets[256] = {0};
    for (I i = 0; i < count; ++i) ++offsets[(from[i] >> shift) & 255];
    I position = 0;
    for (int i = 0; i < 256; ++i) {
        I length = offsets[i];
        offsets[i] = position;
        position += length;
    }
    for (I i = 0; i < count; ++i) to[offsets[(from[i] >> shift) & 255]++] = from[i];
}

static I RadixSort(I count) {
    uint32_t state = 42;
    uint32_t *keys = malloc(count * sizeof(*keys)), *scratch = malloc(count * sizeof(*scratch));
    for (I i = 0; i < count; ++i) keys[i] = state = Next(state);
    RadixPass(keys, scratch, count, 0);
    RadixPass(scratch, keys, count, 8);
    RadixPass(keys, scratch, count, 16);
    RadixPass(scratch, keys, count, 24);
    I hash = 0;
    for (I i = 0; i < count; ++i) hash = Digest(hash, keys[i]);
    free(keys);
    free(scratch);
    return hash;
}

static I Bytecode(I count) {
    struct { uint8_t op, reg; uint32_t operand; } code[257];
    uint32_t registers[16] = {0}, state = 42;
    for (int i = 0; i < 257; ++i) {
        state = Next(state);
        code[i].op = state >> 29;
        code[i].reg = (state >> 16) & 15;
        code[i].operand = state | 1;
    }
    for (I pass = 0; pass < count; ++pass) {
        for (int i = 0; i < 257; ++i) {
            unsigned r = code[i].reg;
            uint32_t a = registers[r], b = registers[(r + 1) & 15] ^ code[i].operand;
            switch (code[i].op) {
                case 0: a += b; break;
                case 1: a ^= b; break;
                case 2: a *= b | 1; break;
                case 3: a = a << 3 | a >> 29; break;
                case 4: a -= b; break;
                case 5: a = (a & b) | code[i].operand; break;
                case 6: a = (a >> (b & 15)) ^ b; break;
                case 7: a = b + (uint32_t)pass; break;
            }
            registers[r] = a;
        }
    }
    I hash = 0;
    for (int i = 0; i < 16; ++i) hash = Digest(hash, registers[i]);
    return hash;
}

static I Quantize(double value) {
    return (I)(value * 1000.0 + (value >= 0 ? 0.50000001 : -0.50000001));
}

static I Lu(I n) {
    double *a = malloc(n * n * sizeof(*a));
    for (I i = 0; i < n; ++i) {
        for (I j = 0; j < n; ++j) a[i * n + j] = ((i * 19 + j * 17) % 31 - 15) / 16.0;
        a[i * n + i] += n;
    }
    for (I k = 0; k < n; ++k) {
        for (I i = k + 1; i < n; ++i) {
            double factor = a[i * n + k] / a[k * n + k];
            a[i * n + k] = factor;
            for (I j = k + 1; j < n; ++j) a[i * n + j] -= factor * a[k * n + j];
        }
    }
    I hash = 0;
    for (I i = 0; i < n * n; ++i) hash = Digest(hash, Quantize(a[i]));
    free(a);
    return hash;
}

static I Fft(I bits) {
    struct Complex { double real, imag; };
    I n = (I)1 << bits, reverse = 0;
    struct Complex *data = malloc(n * sizeof(*data));
    for (I i = 0; i < n; ++i) {
        data[reverse] = (struct Complex){(i % 71 - 35) / 32.0, (i % 19 - 9) / 16.0};
        I bit = n >> 1;
        while (bit && (reverse & bit)) { reverse ^= bit; bit >>= 1; }
        reverse ^= bit;
    }
    for (I size = 2; size <= n; size *= 2) {
        I half = size / 2;
        for (I k = 0; k < half; ++k) {
            double angle = -6.283185307179586 * k / size;
            double re = cos(angle), im = sin(angle);
            for (I i = k; i < n; i += size) {
                I j = i + half;
                double real = data[j].real * re - data[j].imag * im;
                double imag = data[j].real * im + data[j].imag * re;
                data[j] = (struct Complex){data[i].real - real, data[i].imag - imag};
                data[i].real += real; data[i].imag += imag;
            }
        }
    }
    I hash = 0;
    for (I i = 0; i < n; ++i) hash = Digest(Digest(hash, Quantize(data[i].real)), Quantize(data[i].imag));
    free(data);
    return hash;
}

static I RayTrace(I n) {
    struct Sphere { double x, y, z, radius; } scene[49];
    for (int i = 0; i < 49; ++i) {
        scene[i] = (struct Sphere){(i % 7 - 3) * 0.52, (i / 7 - 3) * 0.5,
                                  3.1 + (i % 5) * 0.2, 0.24 + (i % 3) * 0.012};
    }
    I hash = 0;
    for (I row = 0; row < n; ++row) {
        for (I col = 0; col < n; ++col) {
            double x = (2.0 * col + 1.0) / n - 1.0, y = (2.0 * row + 1.0) / n - 1.0;
            double z = 1.0 / sqrt(x * x + y * y + 1.0), nearest = 1000.0, shade = 0.0;
            x *= z; y *= z;
            for (int i = 0; i < 49; ++i) {
                struct Sphere s = scene[i];
                double projection = x * s.x + y * s.y + z * s.z;
                double disc = projection * projection - s.x * s.x - s.y * s.y - s.z * s.z + s.radius * s.radius;
                if (disc < 0.0) continue;
                double distance = projection - sqrt(disc);
                if (distance >= nearest) continue;
                nearest = distance;
                shade = ((distance * x - s.x) * 0.3 + (distance * y - s.y) * 0.4 - (distance * z - s.z) * 0.85) / s.radius;
            }
            hash = Digest(hash, Quantize(shade));
        }
    }
    return hash;
}

static void Diffuse(const double *from, double *to, I n) {
    for (I row = 1; row < n - 1; ++row) {
        for (I col = 1; col < n - 1; ++col) {
            I i = row * n + col;
            to[i] = from[i] * 0.5 + (from[i - 1] + from[i + 1] + from[i - n] + from[i + n]) * 0.125;
        }
    }
}

static I HeatGrid(I n) {
    double *a = calloc(n * n, sizeof(*a)), *b = calloc(n * n, sizeof(*b));
    for (I row = 1; row < n - 1; ++row)
        for (I col = 1; col < n - 1; ++col) a[row * n + col] = ((row * 13 + col * 17) % 271) / 128.0;
    for (int pass = 0; pass < 16; ++pass) { Diffuse(a, b, n); Diffuse(b, a, n); }
    I hash = 0;
    for (I i = 0; i < n * n; ++i) hash = Digest(hash, Quantize(a[i]));
    free(a);
    free(b);
    return hash;
}

static I Particles(I count) {
    struct Particle { double x, y, dx, dy; };
    struct Particle *particles = malloc(count * sizeof(*particles));
    for (I i = 0; i < count; ++i) {
        particles[i] = (struct Particle){i % 421 / 64.0 - 3.0, (i * 13) % 307 / 64.0 - 2.0,
                                        (i % 7 - 3) * 0.003, (i % 11 - 5) * 0.002};
    }
    for (int step = 0; step < 64; ++step) {
        for (I i = 0; i < count; ++i) {
            struct Particle *p = particles + i;
            double force = 0.004 / (1.0 + p->x * p->x + p->y * p->y);
            p->dx = p->dx * 0.999 - p->x * force; p->dy = p->dy * 0.999 - p->y * force;
            p->x += p->dx; p->y += p->dy;
            if (p->x > 3.0 || p->x < -3.0) { p->dx = -p->dx; p->x += p->dx; }
            if (p->y > 3.0 || p->y < -3.0) { p->dy = -p->dy; p->y += p->dy; }
        }
    }
    I hash = 0;
    for (I i = 0; i < count; ++i) hash = Digest(Digest(hash, Quantize(particles[i].x)), Quantize(particles[i].y));
    free(particles);
    return hash;
}

static I ImageFilter(I n) {
    uint8_t *image = malloc(n * n), *output = calloc(n * n, 1);
    double *horizontal = calloc(n * n, sizeof(*horizontal));
    FillBytes(image, n * n);
    for (I row = 0; row < n; ++row) {
        for (I col = 1; col < n - 1; ++col) {
            I i = row * n + col;
            horizontal[i] = image[i - 1] * 0.25 + image[i] * 0.5 + image[i + 1] * 0.25;
        }
    }
    for (I row = 1; row < n - 1; ++row) {
        for (I col = 1; col < n - 1; ++col) {
            I i = row * n + col;
            double smooth = horizontal[i - n] * 0.25 + horizontal[i] * 0.5 + horizontal[i + n] * 0.25;
            double pixel = image[i] + (image[i] - smooth) * 0.5;
            if (pixel < 0.0) pixel = 0.0;
            if (pixel > 255.0) pixel = 255.0;
            output[i] = (uint8_t)(pixel + 0.5);
        }
    }
    I hash = 0;
    for (I i = 0; i < n * n; ++i) hash = Digest(hash, output[i]);
    free(horizontal);
    free(image);
    free(output);
    return hash;
}
