// Independent decimal conversion oracle: Go's standard library supplies
// binary64 parsing and shortest digits; layout follows Python repr's rules.
package main

import (
	"fmt"
	"math"
	"math/big"
	"math/rand"
	"strconv"
	"strings"
)

func repr(v float64) string {
	if math.IsNaN(v) {
		return "nan"
	}
	if math.IsInf(v, 1) {
		return "inf"
	}
	if math.IsInf(v, -1) {
		return "-inf"
	}
	e := strconv.FormatFloat(v, 'e', -1, 64)
	at := strings.IndexByte(e, 'e')
	order, _ := strconv.Atoi(e[at+1:])
	if order < -4 || order >= 16 {
		return e
	}
	f := strconv.FormatFloat(v, 'f', -1, 64)
	if !strings.Contains(f, ".") {
		f += ".0"
	}
	return f
}

func main() {
	fmt.Println(`#import "float_text"
#import "binary"
#import "text_buffer"
Fixture :: #import "std_float_text"

`)
	samples := []string{
		"1.00000000000000011102230246251565404236316680908203125",
		"1.00000000000000011102230246251565404236316680908203125" + strings.Repeat("0", 1200) + "1",
		"0." + strings.Repeat("0", 1600) + "1e1601",
		"1" + strings.Repeat("0", 1600) + "e-1600",
		"1.7976931348623158e308", "1.7976931348623159e308",
		"2.2250738585072011e-308", "2.2250738585072012e-308",
		"2.4703282292062327e-324", "2.4703282292062328e-324",
	}
	halfMin := new(big.Rat).SetFrac(big.NewInt(1), new(big.Int).Lsh(big.NewInt(1), 1075)).FloatString(1075)
	overflow := new(big.Int).Lsh(new(big.Int).Sub(new(big.Int).Lsh(big.NewInt(1), 54), big.NewInt(1)), 970)
	samples = append(samples, halfMin, halfMin+"1", overflow.String(), new(big.Int).Sub(overflow, big.NewInt(1)).String())
	r := rand.New(rand.NewSource(20261104))
	for i := 0; i < 1024; i++ {
		var s strings.Builder
		if i%2 == 0 {
			s.WriteByte('-')
		}
		digits := 1 + r.Intn(70)
		if i%64 == 0 {
			digits = 1400
		}
		s.WriteByte(byte('1' + r.Intn(9)))
		s.WriteByte('.')
		for j := 1; j < digits; j++ {
			s.WriteByte(byte('0' + r.Intn(10)))
		}
		fmt.Fprintf(&s, "e%+d", r.Intn(810)-405)
		samples = append(samples, s.String())
	}
	// Exercise every binade boundary and both neighboring values, plus
	// signed zero, subnormal/normal transition, and random raw bit patterns.
	bits := []uint64{0, 1, 2, 3, (1 << 52) - 1, 1 << 52, (1 << 52) + 1, 0x7fefffffffffffff, 1 << 63, 0x7ff0000000000000, 0xfff0000000000000, 0x7ff8000000000000}
	for e := uint64(1); e < 2047; e++ {
		bits = append(bits, e<<52-1, e<<52, e<<52+1)
	}
	for i := 0; i < 2048; i++ {
		bits = append(bits, r.Uint64())
	}
	type sample struct {
		text, expected string
		bits           uint64
	}
	cases := []sample{}
	for _, s := range samples {
		exact, _, err := big.ParseFloat(s, 10, 8192, big.ToNearestEven)
		if err != nil {
			panic(err)
		}
		v, _ := exact.Float64()
		cases = append(cases, sample{s, repr(v), math.Float64bits(v)})
	}
	for _, b := range bits {
		s := repr(math.Float64frombits(b))
		cases = append(cases, sample{s, s, b})
	}
	fmt.Println(`CheckCase :: (text: string, expected: string, bits: u64, index: s32) -> bool {
    got, ok := ParseFloat64(text)
    want := Float64FromBits(bits)
    if !ok || (want == want && (got != want || (want == 0.0 && 1.0 / got != 1.0 / want))) || (want != want && got == got) {
        print("parse case %\n", index)
        return false
    }
    if Float64Repr(want) != expected {
        print("repr case %: % versus %\n", index, Float64Repr(want), expected)
        return false
    }
    output: Vec(u8)
    if !BuilderAppend(output, "prefix:") || !Float64ReprInto(want, *output) ||
        TextView(VecSlice(output, 7, output.count)) != expected {
        print("repr Into case %\n", index)
        return false
    }
    return true
}`)
	for i, c := range cases {
		if i%32 == 0 {
			fmt.Printf("Group%d :: () -> s32 {\n", i/32)
		}
		fmt.Printf("    if !CheckCase(%q, %q, cast(u64)%d, %d) { return 30 }\n", c.text, c.expected, c.bits, i)
		if i%32 == 31 || i == len(cases)-1 {
			fmt.Println("    return 0\n}")
		}
	}
	fmt.Println(`#program_export
NativeMain :: () -> s32 {
    basic := Fixture.SelfTest()
    if basic != 0 { return basic }
    result: s32`)
	for i := 0; i < (len(cases)+31)/32; i++ {
		fmt.Printf("    result = Group%d()\n    if result != 0 { return result }\n", i)
	}
	fmt.Println("    return 0\n}")
}
