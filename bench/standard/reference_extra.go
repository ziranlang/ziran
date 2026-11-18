package main

import (
	"encoding/binary"
	"math"
	"math/bits"
)

func digest(hash, value int64) int64 { return (hash*31 + value) % 1000000007 }
func nextWord(state uint32) uint32   { return state*1664525 + 1013904223 }
func fillBytes(output []byte) {
	state := uint32(42)
	for i := range output {
		state = nextWord(state)
		output[i] = byte(state >> 24)
	}
}

func KNucleotide(n int64) int64 {
	input := make([]byte, n)
	fillBytes(input)
	var counts [16384]int64
	var k uint32
	for i, b := range input {
		k = (k<<2 | uint32(b>>6)) & 16383
		if i >= 6 {
			counts[k]++
		}
	}
	var hash int64
	for _, count := range counts {
		hash = digest(hash, count)
	}
	return hash
}

func ReverseComplement(n int64) int64 {
	input, output := make([]byte, n), make([]byte, n)
	fillBytes(input)
	for i := range output {
		output[i] = "TGCA"[input[len(input)-1-i]>>6]
	}
	var hash int64
	for _, b := range output {
		hash = digest(hash, int64(b))
	}
	return hash
}

func PiDigits(n int64) int64 {
	limbs := make([]int64, n*10/3+2)
	for i := range limbs {
		limbs[i] = 2
	}
	var pending, nines, hash int64
	for step := int64(0); step < n; step++ {
		var carry int64
		for i := int64(len(limbs)); i > 0; i-- {
			value := limbs[i-1]*10 + carry*i
			denominator := i*2 - 1
			limbs[i-1], carry = value%denominator, value/denominator
		}
		limbs[0], carry = carry%10, carry/10
		if carry == 9 {
			nines++
			continue
		}
		increment, flushed := int64(0), int64(9)
		if carry == 10 {
			increment, flushed, carry = 1, 0, 0
		}
		hash = digest(hash, pending+increment)
		for ; nines > 0; nines-- {
			hash = digest(hash, flushed)
		}
		pending = carry
	}
	hash = digest(hash, pending)
	for ; nines > 0; nines-- {
		hash = digest(hash, 9)
	}
	return hash
}

func skipJson(s string, pos int) int {
	for pos < len(s) {
		switch s[pos] {
		case ' ', '\t', '\n', '\r':
			pos++
		default:
			return pos
		}
	}
	return pos
}

func decimal(out []byte, value int64) []byte {
	if value < 0 {
		out, value = append(out, '-'), -value
	}
	place := int64(1)
	for place <= value/10 {
		place *= 10
	}
	for ; place != 0; place /= 10 {
		out = append(out, byte('0'+value/place))
		value %= place
	}
	return out
}

func JsonRoundtrip(n int64) int64 {
	inputs := [...]string{
		` { "width" : 640, "height" : 480, "offset": -56 } `,
		`{"left":-7,"top":0,"right":1920,"bottom":1080}`,
		`{ "rows": 12, "columns": 75, "capacity": 1024, "delta": -10 } `,
	}
	var buffer [96]byte
	var hash int64
	for i := int64(0); i < n; i++ {
		s := inputs[i%3]
		p := skipJson(s, 0)
		if s[p] != '{' {
			return -1
		}
		p++
		out := append(buffer[:0], '{')
		for {
			p = skipJson(s, p)
			start := p
			if s[p] != '"' {
				return -1
			}
			p++
			for s[p] >= 'a' && s[p] <= 'z' {
				p++
			}
			if s[p] != '"' {
				return -1
			}
			p++
			out = append(out, s[start:p]...)
			p = skipJson(s, p)
			if s[p] != ':' {
				return -1
			}
			out = append(out, ':')
			p = skipJson(s, p+1)
			negative := s[p] == '-'
			if negative {
				p++
			}
			start = p
			var value int64
			for s[p] >= '0' && s[p] <= '9' {
				value = value*10 + int64(s[p]-'0')
				p++
			}
			if p == start {
				return -1
			}
			if negative {
				value = -value
			}
			out = decimal(out, value)
			p = skipJson(s, p)
			if s[p] != ',' {
				break
			}
			out = append(out, ',')
			p++
		}
		if s[p] != '}' || skipJson(s, p+1) != len(s) {
			return -1
		}
		out = append(out, '}')
		for _, b := range out {
			hash = digest(hash, int64(b))
		}
	}
	return hash
}

func Lz77(n int64) int64 {
	input := make([]byte, n)
	fillBytes(input)
	for i := range input {
		if i%256 >= 48 {
			input[i] = byte(i % 31)
		}
	}
	var last [4096]int
	var hash int64
	for at := 0; at < len(input); {
		length, distance := 0, 0
		if at+2 < len(input) {
			key := (int(input[at])*251 + int(input[at+1])*31 + int(input[at+2])) & 4095
			previous := last[key] - 1
			last[key] = at + 1
			distance = at - previous
			if previous >= 0 && distance <= 32768 {
				for length < 258 && at+length < len(input) && input[previous+length] == input[at+length] {
					length++
				}
			}
		}
		if length >= 3 {
			hash = digest(digest(digest(hash, 256), int64(length)), int64(distance))
			at += length
		} else {
			hash = digest(hash, int64(input[at]))
			at++
		}
	}
	return hash
}

func Sha256(n int64) int64 {
	constants := [64]uint32{
		0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
		0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
		0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
		0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
		0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
		0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
		0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
		0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
	}
	state := [8]uint32{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19}
	input := make([]byte, (n+72)/64*64)
	fillBytes(input[:n])
	input[n] = 128
	binary.BigEndian.PutUint64(input[len(input)-8:], uint64(n)*8)
	for offset := 0; offset < len(input); offset += 64 {
		var words [64]uint32
		for i := 0; i < 16; i++ {
			words[i] = binary.BigEndian.Uint32(input[offset+i*4:])
		}
		for i := 16; i < 64; i++ {
			x, y := words[i-15], words[i-2]
			s0 := bits.RotateLeft32(x, -7) ^ bits.RotateLeft32(x, -18) ^ (x >> 3)
			s1 := bits.RotateLeft32(y, -17) ^ bits.RotateLeft32(y, -19) ^ (y >> 10)
			words[i] = words[i-16] + words[i-7] + s0 + s1
		}
		a, b, c, d, e, f, g, h := state[0], state[1], state[2], state[3], state[4], state[5], state[6], state[7]
		for i, w := range words {
			s0 := bits.RotateLeft32(a, -2) ^ bits.RotateLeft32(a, -13) ^ bits.RotateLeft32(a, -22)
			s1 := bits.RotateLeft32(e, -6) ^ bits.RotateLeft32(e, -11) ^ bits.RotateLeft32(e, -25)
			first := h + s1 + ((e & f) ^ (^e & g)) + constants[i] + w
			second := s0 + ((a & b) ^ (a & c) ^ (b & c))
			a, b, c, d, e, f, g, h = first+second, a, b, c, d+first, e, f, g
		}
		for i, v := range [8]uint32{a, b, c, d, e, f, g, h} {
			state[i] += v
		}
	}
	var hash int64
	for _, v := range state {
		hash = digest(hash, int64(v))
	}
	return hash
}

func Crc32(n int64) int64 {
	input := make([]byte, n)
	fillBytes(input)
	var table [256]uint32
	for i := range table {
		w := uint32(i)
		for bit := 0; bit < 8; bit++ {
			w = (w >> 1) ^ (0xedb88320 & (0 - (w & 1)))
		}
		table[i] = w
	}
	crc := uint32(0xffffffff)
	for _, b := range input {
		crc = crc>>8 ^ table[(crc^uint32(b))&255]
	}
	return int64(^crc)
}

func sudokuFill(grid *[81]int64, masks *[27]int64) bool {
	cell, fewest, choices := -1, 10, int64(0)
	for i, v := range grid {
		if v != 0 {
			continue
		}
		row, col := i/9, i%9
		box := row/3*3 + col/3
		free := 511 & ^(masks[row] | masks[9+col] | masks[18+box])
		count := 0
		for bits := free; bits != 0; bits &= bits - 1 {
			count++
		}
		if count == 0 {
			return false
		}
		if count < fewest {
			cell, fewest, choices = i, count, free
		}
	}
	if cell < 0 {
		return true
	}
	row, col := cell/9, cell%9
	box := row/3*3 + col/3
	for digit := int64(0); digit < 9; digit++ {
		bit := int64(1) << digit
		if choices&bit == 0 {
			continue
		}
		grid[cell], masks[row], masks[9+col], masks[18+box] = digit+1, masks[row]^bit, masks[9+col]^bit, masks[18+box]^bit
		if sudokuFill(grid, masks) {
			return true
		}
		grid[cell], masks[row], masks[9+col], masks[18+box] = 0, masks[row]^bit, masks[9+col]^bit, masks[18+box]^bit
	}
	return false
}

func Sudoku(n int64) int64 {
	const puzzle = "800000000003600000070090200050007000000045700000100030001000068008500010090000400"
	var hash int64
	for pass := int64(0); pass < n; pass++ {
		var grid [81]int64
		var masks [27]int64
		for i := range grid {
			if puzzle[i] == '0' {
				continue
			}
			digit := (int64(puzzle[i]-'1') + pass%9) % 9
			bit := int64(1) << digit
			row, col := i/9, i%9
			box := row/3*3 + col/3
			grid[i] = digit + 1
			masks[row] |= bit
			masks[9+col] |= bit
			masks[18+box] |= bit
		}
		if !sudokuFill(&grid, &masks) {
			return -1
		}
		for _, value := range grid {
			hash = digest(hash, value)
		}
	}
	return hash
}

func relax(dist []int64, from, to int64) {
	cost := dist[from] + (from*31+to*17)%23 + 1
	if cost < dist[to] {
		dist[to] = cost
	}
}

func Dijkstra(width int64) int64 {
	n := width * width
	dist, seen := make([]int64, n), make([]byte, n)
	for i := int64(1); i < n; i++ {
		dist[i] = 1000000007
	}
	for range dist {
		current, closest := int64(-1), int64(1000000007)
		for i, cost := range dist {
			if seen[i] == 0 && cost < closest {
				current, closest = int64(i), cost
			}
		}
		if current < 0 {
			break
		}
		seen[current] = 1
		if current%width > 0 {
			relax(dist, current, current-1)
		}
		if current%width+1 < width {
			relax(dist, current, current+1)
		}
		if current >= width {
			relax(dist, current, current-width)
		}
		if current+width < n {
			relax(dist, current, current+width)
		}
	}
	var hash int64
	for _, distance := range dist {
		hash = digest(hash, distance)
	}
	return hash
}

func WordCount(n int64) int64 {
	const words = "ant bee crow doe eel fox gnat hare ibis jay kite lion mole newt orca pike quail rook seal tern vole wasp yak zebra "
	var table [4096]struct {
		key   uint32
		count int64
	}
	seed := uint32(42)
	for i := int64(0); i < n; i++ {
		seed = nextWord(seed)
		at := (seed >> 16) % uint32(len(words))
		for at > 0 && words[at-1] != ' ' {
			at--
		}
		var key uint32
		for words[at] != ' ' {
			key = key*31 + uint32(words[at])
			at++
		}
		key = key*64 + ((seed >> 25) & 63)
		slot := (key ^ (key >> 13)) & 4095
		for table[slot].count != 0 && table[slot].key != key {
			slot = (slot + 1) & 4095
		}
		table[slot].key = key
		table[slot].count++
	}
	var hash int64
	for _, entry := range table {
		hash = digest(digest(hash, int64(entry.key)), entry.count)
	}
	return hash
}

func radixPass(from, to []uint32, shift uint) {
	var offsets [256]int
	for _, key := range from {
		offsets[(key>>shift)&255]++
	}
	position := 0
	for i, count := range offsets {
		offsets[i], position = position, position+count
	}
	for _, key := range from {
		bucket := (key >> shift) & 255
		to[offsets[bucket]] = key
		offsets[bucket]++
	}
}

func RadixSort(n int64) int64 {
	keys, scratch := make([]uint32, n), make([]uint32, n)
	seed := uint32(42)
	for i := range keys {
		seed = nextWord(seed)
		keys[i] = seed
	}
	radixPass(keys, scratch, 0)
	radixPass(scratch, keys, 8)
	radixPass(keys, scratch, 16)
	radixPass(scratch, keys, 24)
	var hash int64
	for _, key := range keys {
		hash = digest(hash, int64(key))
	}
	return hash
}

func Bytecode(n int64) int64 {
	var code [257]struct {
		op, reg uint8
		operand uint32
	}
	var registers [16]uint32
	seed := uint32(42)
	for i := range code {
		seed = nextWord(seed)
		code[i].op, code[i].reg, code[i].operand = uint8(seed>>29), uint8(seed>>16&15), seed|1
	}
	for pass := int64(0); pass < n; pass++ {
		for _, instruction := range code {
			r := instruction.reg
			a, b := registers[r], registers[(r+1)&15]^instruction.operand
			switch instruction.op {
			case 0:
				a += b
			case 1:
				a ^= b
			case 2:
				a *= b | 1
			case 3:
				a = bits.RotateLeft32(a, 3)
			case 4:
				a -= b
			case 5:
				a = (a & b) | instruction.operand
			case 6:
				a = (a >> (b & 15)) ^ b
			case 7:
				a = b + uint32(pass)
			}
			registers[r] = a
		}
	}
	var hash int64
	for _, v := range registers {
		hash = digest(hash, int64(v))
	}
	return hash
}

func quantize(value float64) int64 {
	if value >= 0 {
		return int64(value*1000 + 0.50000001)
	}
	return int64(value*1000 - 0.50000001)
}

func Lu(n int64) int64 {
	a := make([]float64, n*n)
	for i := int64(0); i < n; i++ {
		for j := int64(0); j < n; j++ {
			a[i*n+j] = float64((i*19+j*17)%31-15) / 16
		}
		a[i*n+i] += float64(n)
	}
	for k := int64(0); k < n; k++ {
		for i := k + 1; i < n; i++ {
			scale := a[i*n+k] / a[k*n+k]
			a[i*n+k] = scale
			for j := k + 1; j < n; j++ {
				a[i*n+j] -= scale * a[k*n+j]
			}
		}
	}
	var hash int64
	for _, v := range a {
		hash = digest(hash, quantize(v))
	}
	return hash
}

func Fft(bits int64) int64 {
	n := 1 << bits
	data := make([]complex128, n)
	reverse := 0
	for i := range data {
		data[reverse] = complex(float64(i%71-35)/32, float64(i%19-9)/16)
		bit := n >> 1
		for bit != 0 && (reverse&bit) != 0 {
			reverse ^= bit
			bit >>= 1
		}
		reverse ^= bit
	}
	for size := 2; size <= n; size *= 2 {
		half := size / 2
		for k := 0; k < half; k++ {
			angle := -6.283185307179586 * float64(k) / float64(size)
			sine, cosine := math.Sincos(angle)
			rotation := complex(cosine, sine)
			for i := k; i < n; i += size {
				j := i + half
				right := data[j] * rotation
				left := data[i]
				data[i], data[j] = left+right, left-right
			}
		}
	}
	var hash int64
	for _, value := range data {
		hash = digest(digest(hash, quantize(real(value))), quantize(imag(value)))
	}
	return hash
}

func RayTrace(n int64) int64 {
	var scene [49]struct{ x, y, z, radius float64 }
	for i := range scene {
		s := &scene[i]
		s.x, s.y, s.z, s.radius = float64(i%7-3)*0.52, float64(i/7-3)*0.5, 3.1+float64(i%5)*0.2, 0.24+float64(i%3)*0.012
	}
	var hash int64
	for row := int64(0); row < n; row++ {
		for col := int64(0); col < n; col++ {
			x, y := (2*float64(col)+1)/float64(n)-1, (2*float64(row)+1)/float64(n)-1
			z := 1 / math.Sqrt(x*x+y*y+1)
			x *= z
			y *= z
			nearest, shade := 1000.0, 0.0
			for _, s := range scene {
				projection := x*s.x + y*s.y + z*s.z
				disc := projection*projection - s.x*s.x - s.y*s.y - s.z*s.z + s.radius*s.radius
				if disc < 0 {
					continue
				}
				distance := projection - math.Sqrt(disc)
				if distance >= nearest {
					continue
				}
				nearest = distance
				shade = ((distance*x-s.x)*0.3 + (distance*y-s.y)*0.4 - (distance*z-s.z)*0.85) / s.radius
			}
			hash = digest(hash, quantize(shade))
		}
	}
	return hash
}

func diffuse(from, to []float64, n int64) {
	for row := int64(1); row < n-1; row++ {
		for col := int64(1); col < n-1; col++ {
			i := row*n + col
			to[i] = from[i]*0.5 + (from[i-1]+from[i+1]+from[i-n]+from[i+n])*0.125
		}
	}
}

func HeatGrid(n int64) int64 {
	a, b := make([]float64, n*n), make([]float64, n*n)
	for row := int64(1); row < n-1; row++ {
		for col := int64(1); col < n-1; col++ {
			a[row*n+col] = float64((row*13+col*17)%271) / 128
		}
	}
	for pass := 0; pass < 16; pass++ {
		diffuse(a, b, n)
		diffuse(b, a, n)
	}
	var hash int64
	for _, v := range a {
		hash = digest(hash, quantize(v))
	}
	return hash
}

func Particles(n int64) int64 {
	type particle struct{ x, y, dx, dy float64 }
	particles := make([]particle, n)
	for i := range particles {
		particles[i] = particle{float64(i%421)/64 - 3, float64(i*13%307)/64 - 2, float64(i%7-3) * 0.003, float64(i%11-5) * 0.002}
	}
	for step := 0; step < 64; step++ {
		for i := range particles {
			p := &particles[i]
			force := 0.004 / (1 + p.x*p.x + p.y*p.y)
			p.dx = p.dx*0.999 - p.x*force
			p.dy = p.dy*0.999 - p.y*force
			p.x += p.dx
			p.y += p.dy
			if p.x > 3 || p.x < -3 {
				p.dx = -p.dx
				p.x += p.dx
			}
			if p.y > 3 || p.y < -3 {
				p.dy = -p.dy
				p.y += p.dy
			}
		}
	}
	var hash int64
	for _, p := range particles {
		hash = digest(digest(hash, quantize(p.x)), quantize(p.y))
	}
	return hash
}

func ImageFilter(n int64) int64 {
	input, output := make([]byte, n*n), make([]byte, n*n)
	horizontal := make([]float64, n*n)
	fillBytes(input)
	for row := int64(0); row < n; row++ {
		for col := int64(1); col < n-1; col++ {
			i := row*n + col
			horizontal[i] = float64(input[i-1])*0.25 + float64(input[i])*0.5 + float64(input[i+1])*0.25
		}
	}
	for row := int64(1); row < n-1; row++ {
		for col := int64(1); col < n-1; col++ {
			i := row*n + col
			smooth := horizontal[i-n]*0.25 + horizontal[i]*0.5 + horizontal[i+n]*0.25
			pixel := float64(input[i]) + (float64(input[i])-smooth)*0.5
			if pixel < 0 {
				pixel = 0
			}
			if pixel > 255 {
				pixel = 255
			}
			output[i] = byte(pixel + 0.5)
		}
	}
	var hash int64
	for _, pixel := range output {
		hash = digest(hash, int64(pixel))
	}
	return hash
}
