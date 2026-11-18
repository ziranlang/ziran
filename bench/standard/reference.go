// Single-thread scalar reference. Build with the standard go build.
package main

import "math"

func Fib(n int64) int64 {
	if n < 2 {
		return n
	}
	return Fib(n-1) + Fib(n-2)
}
func Queens(mask, cols, left, right int64) int64 {
	if cols == mask {
		return 1
	}
	available := mask & ^(cols | left | right)
	var total int64
	for available != 0 {
		bit := available & -available
		available -= bit
		total += Queens(mask, cols|bit, (left|bit)<<1, (right|bit)>>1)
	}
	return total
}
func Matrix(n int64) int64 {
	a := make([]int64, n*n)
	b := make([]int64, n*n)
	c := make([]int64, n*n)
	for i := int64(0); i < n*n; i++ {
		a[i] = i % 17
		b[i] = i % 13
	}
	for i := int64(0); i < n; i++ {
		for k := int64(0); k < n; k++ {
			for j := int64(0); j < n; j++ {
				c[i*n+j] += a[i*n+k] * b[k*n+j]
			}
		}
	}
	var sum int64
	for _, v := range c {
		sum += v
	}
	return sum
}
func Quick(a []int64, lo, hi int64) {
	i, j, pivot := lo, hi, a[(lo+hi)/2]
	for i <= j {
		for a[i] < pivot {
			i++
		}
		for a[j] > pivot {
			j--
		}
		if i <= j {
			a[i], a[j] = a[j], a[i]
			i++
			j--
		}
	}
	if lo < j {
		Quick(a, lo, j)
	}
	if i < hi {
		Quick(a, i, hi)
	}
}
func Sort(n int64) int64 {
	a := make([]int64, n)
	seed := int64(42)
	for i := int64(0); i < n; i++ {
		seed = (seed*1664525 + 1013904223) & 2147483647
		a[i] = seed
	}
	Quick(a, 0, n-1)
	var hash int64
	for _, v := range a {
		hash = (hash*31 + v) % 1000000007
	}
	return hash
}
func Hash(n int64) int64 {
	text := "the quick brown fox jumps over the lazy dog, again and again"
	hash := uint64(1469598103934665603)
	for r := int64(0); r < n; r++ {
		for i := 0; i < len(text); i++ {
			hash = (hash ^ uint64(text[i])) * 1099511628211
		}
	}
	return int64(hash % 1000000007)
}
func A(i, j int64) float64 { return 1.0 / float64((i+j)*(i+j+1)/2+i+1) }
func Multiply(u, v []float64, transpose bool) {
	for i := range u {
		sum := 0.0
		for j := range u {
			a := A(int64(i), int64(j))
			if transpose {
				a = A(int64(j), int64(i))
			}
			sum += u[j] * a
		}
		v[i] = sum
	}
}
func Spectral(n int64) int64 {
	u := make([]float64, n)
	v := make([]float64, n)
	tmp := make([]float64, n)
	for i := range u {
		u[i] = 1
	}
	for r := 0; r < 10; r++ {
		Multiply(u, tmp, false)
		Multiply(tmp, v, true)
		Multiply(v, tmp, false)
		Multiply(tmp, u, true)
	}
	uv, vv := 0.0, 0.0
	for i := range u {
		uv += u[i] * v[i]
		vv += v[i] * v[i]
	}
	return int64(math.Sqrt(uv/vv)*1e9 + 0.5)
}
func Mandelbrot(n int64) int64 {
	var hash int64
	for y := int64(0); y < n; y++ {
		var b, bits int64
		for x := int64(0); x < n; x++ {
			cr, ci := 2.0*float64(x)/float64(n)-1.5, 2.0*float64(y)/float64(n)-1
			zr, zi, tr, ti := 0.0, 0.0, 0.0, 0.0
			iter := 0
			for iter < 50 && tr+ti <= 4 {
				zi = 2*zr*zi + ci
				zr = tr - ti + cr
				tr = zr * zr
				ti = zi * zi
				iter++
			}
			b *= 2
			if tr+ti <= 4 {
				b++
			}
			bits++
			if bits == 8 {
				hash = (hash*31 + b) % 1000000007
				b = 0
				bits = 0
			}
		}
		if bits != 0 {
			hash = (hash*31 + (b << (8 - bits))) % 1000000007
		}
	}
	return hash
}
func Fannkuch(n int64) int64 {
	var perm, work, count [16]int64
	for i := int64(0); i < n; i++ {
		perm[i] = i
	}
	r := n
	var index, checksum, maximum int64
	for {
		for r != 1 {
			count[r-1] = r
			r--
		}
		for i := int64(0); i < n; i++ {
			work[i] = perm[i]
		}
		var flips int64
		for work[0] != 0 {
			k, i := work[0], int64(0)
			for i < k {
				work[i], work[k] = work[k], work[i]
				i++
				k--
			}
			flips++
		}
		if flips > maximum {
			maximum = flips
		}
		if index%2 == 0 {
			checksum += flips
		} else {
			checksum -= flips
		}
		for {
			if r == n {
				return checksum*1000 + maximum
			}
			first := perm[0]
			for i := int64(0); i < r; i++ {
				perm[i] = perm[i+1]
			}
			perm[r] = first
			count[r]--
			if count[r] > 0 {
				break
			}
			r++
		}
		index++
	}
}

type Node struct{ left, right *Node }

func Tree(depth int64) *Node {
	node := new(Node)
	if depth > 0 {
		node.left = Tree(depth - 1)
		node.right = Tree(depth - 1)
	}
	return node
}
func TreeCheck(node *Node) int64 {
	if node.left == nil {
		return 1
	}
	return 1 + TreeCheck(node.left) + TreeCheck(node.right)
}
func Trees(n int64) int64 {
	sum := TreeCheck(Tree(n + 1))
	longlived := Tree(n)
	for depth := int64(4); depth <= n; depth += 2 {
		iterations := int64(1) << (n - depth + 4)
		for i := int64(0); i < iterations; i++ {
			sum += TreeCheck(Tree(depth))
		}
	}
	return sum + TreeCheck(longlived)
}

type Body struct{ x, y, z, vx, vy, vz, mass float64 }

func Nbody(n int64) int64 {
	pi := 3.141592653589793
	solar, year := 4*pi*pi, 365.24
	b := [5]Body{
		{0, 0, 0, 0, 0, 0, solar},
		{4.84143144246472090, -1.16032004402742839, -0.103622044471123109, 0.00166007664274403694 * year, 0.00769901118419740425 * year, -0.0000690460016972063023 * year, 0.000954791938424326609 * solar},
		{8.34336671824457987, 4.12479856412430479, -0.403523417114321381, -0.00276742510726862411 * year, 0.00499852801234917238 * year, 0.0000230417297573763929 * year, 0.000285885980666130812 * solar},
		{12.8943695621391310, -15.1111514016986312, -0.223307578892655734, 0.00296460137564761618 * year, 0.00237847173959480950 * year, -0.0000296589568540237556 * year, 0.0000436624404335156298 * solar},
		{15.3796971148509165, -25.9193146099879641, 0.179258772950371181, 0.00268067772490389322 * year, 0.00162824170038242295 * year, -0.0000951592254519715870 * year, 0.0000515138902046611451 * solar}}
	for i := 1; i < 5; i++ {
		b[0].vx -= b[i].vx * b[i].mass / solar
		b[0].vy -= b[i].vy * b[i].mass / solar
		b[0].vz -= b[i].vz * b[i].mass / solar
	}
	for step := int64(0); step < n; step++ {
		for i := 0; i < 4; i++ {
			for j := i + 1; j < 5; j++ {
				dx, dy, dz := b[i].x-b[j].x, b[i].y-b[j].y, b[i].z-b[j].z
				d2 := dx*dx + dy*dy + dz*dz
				mag := 0.01 / (d2 * math.Sqrt(d2))
				b[i].vx -= dx * b[j].mass * mag
				b[i].vy -= dy * b[j].mass * mag
				b[i].vz -= dz * b[j].mass * mag
				b[j].vx += dx * b[i].mass * mag
				b[j].vy += dy * b[i].mass * mag
				b[j].vz += dz * b[i].mass * mag
			}
		}
		for i := 0; i < 5; i++ {
			b[i].x += 0.01 * b[i].vx
			b[i].y += 0.01 * b[i].vy
			b[i].z += 0.01 * b[i].vz
		}
	}
	energy := 0.0
	for i := 0; i < 5; i++ {
		energy += 0.5 * b[i].mass * (b[i].vx*b[i].vx + b[i].vy*b[i].vy + b[i].vz*b[i].vz)
		for j := i + 1; j < 5; j++ {
			dx, dy, dz := b[i].x-b[j].x, b[i].y-b[j].y, b[i].z-b[j].z
			energy -= b[i].mass * b[j].mass / math.Sqrt(dx*dx+dy*dy+dz*dz)
		}
	}
	return int64(energy*1e9 - 0.5)
}

const ALU = "GGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAA"

func Fasta(n int64) int64 {
	hash, seed := int64(0), int64(42)
	for i := int64(0); i < 2*n; i++ {
		hash = (hash*31 + int64(ALU[i%int64(len(ALU))])) % 1000000007
	}
	chars := "acgtBDHKMNRSVWY"
	p := [15]float64{.27, .12, .12, .27, .02, .02, .02, .02, .02, .02, .02, .02, .02, .02, .02}
	h := [4]float64{.3029549426680, .1979883004921, .1975473066391, .3015094502008}
	for phase := 0; phase < 2; phase++ {
		length := 3 * n
		if phase == 1 {
			length = 5 * n
		}
		for i := int64(0); i < length; i++ {
			seed = (seed*3877 + 29573) % 139968
			random, cumulative := float64(seed)/139968.0, 0.0
			index := 0
			for index < 14 {
				if phase == 0 {
					cumulative += p[index]
				} else {
					cumulative += h[index]
				}
				if random < cumulative {
					break
				}
				index++
			}
			hash = (hash*31 + int64(chars[index])) % 1000000007
		}
	}
	return hash
}
func Run(kind, n int64) int64 {
	switch kind {
	case 0:
		return Fib(n)
	case 1:
		return Nbody(n)
	case 2:
		return Spectral(n)
	case 3:
		return Mandelbrot(n)
	case 4:
		return Fannkuch(n)
	case 5:
		return Trees(n)
	case 6:
		return Fasta(n)
	case 7:
		return Queens((int64(1)<<n)-1, 0, 0, 0)
	case 8:
		return Matrix(n)
	case 9:
		return Sort(n)
	case 10:
		return Hash(n)
	case 11:
		return KNucleotide(n)
	case 12:
		return ReverseComplement(n)
	case 13:
		return PiDigits(n)
	case 14:
		return JsonRoundtrip(n)
	case 15:
		return Lz77(n)
	case 16:
		return Sha256(n)
	case 17:
		return Crc32(n)
	case 18:
		return RayTrace(n)
	case 19:
		return Sudoku(n)
	case 20:
		return Dijkstra(n)
	case 21:
		return WordCount(n)
	case 22:
		return RadixSort(n)
	case 23:
		return Lu(n)
	case 24:
		return Fft(n)
	case 25:
		return Bytecode(n)
	case 26:
		return HeatGrid(n)
	case 27:
		return Particles(n)
	case 28:
		return ImageFilter(n)
	default:
		return -1
	}
}
