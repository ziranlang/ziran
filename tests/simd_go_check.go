// Calls each typed std/simd helper and its reference form in zi_simd_helpers
// with the same edge-heavy operands, and reports any lane that differs.
package main

import (
	"fmt"
	"math"
	"os"
	"reflect"
	"strings"
)

var state uint64 = 0x9e3779b97f4a7c15

func next() uint64 {
	state ^= state << 13
	state ^= state >> 7
	state ^= state << 17
	return state
}

// Float specials: NaN, infinities, signed zeros, the integer limits that
// conversion saturates at and the numbers just below them, 1 and -0.5.
var specials32 = []uint32{0x7fc00000, 0xff800000, 0x7f800000, 0x80000000, 0,
	0x4f000000, 0x4effffff, 0xcf000000, 0x4f800000, 0x4f7fffff, 0x5f000000, 0x5effffff,
	0x5f800000, 0x5f7fffff, 0x3f800000, 0xbf000000}
var specials64 = []uint64{0x7ff8000000000000, 0xfff0000000000000, 0x7ff0000000000000,
	0x8000000000000000, 0, 0x41e0000000000000, 0x41dfffffffc00000, 0xc1e0000000000000,
	0x41f0000000000000, 0x43e0000000000000, 0x43dfffffffffffff, 0xc3e0000000000000,
	0x43f0000000000000, 0x43efffffffffffff, 0x3ff0000000000000, 0xbfe0000000000000}

// A lane's bits: 0, 1, extremes, small numbers or random bits.
func bits(width int) uint64 {
	mask := ^uint64(0) >> (64 - width)
	switch next() % 8 {
	case 0:
		return 0
	case 1:
		return 1
	case 2:
		return mask
	case 3:
		return 1 << (width - 1)
	case 4:
		return 1<<(width-1) - 1
	case 5:
		return next() % 70
	}
	return next() & mask
}

func float(width int) float64 {
	if next()%4 == 0 {
		if width == 32 {
			return float64(math.Float32frombits(specials32[next()%uint64(len(specials32))]))
		}
		return math.Float64frombits(specials64[next()%uint64(len(specials64))])
	}
	v := float64(int64(next()>>20)) / float64(uint64(1)<<(next()%24))
	if next()%16 == 0 {
		v *= 1e30
	}
	return v
}

func lane(t reflect.Type) reflect.Value {
	v := reflect.New(t).Elem()
	switch t.Kind() {
	case reflect.Float32:
		if next()%4 == 0 {
			v.SetFloat(float64(math.Float32frombits(specials32[next()%uint64(len(specials32))])))
		} else {
			v.SetFloat(float64(float32(float(32))))
		}
	case reflect.Float64:
		v.SetFloat(float(64))
	case reflect.Int8, reflect.Int16, reflect.Int32, reflect.Int64:
		width := t.Bits()
		v.SetInt(int64(bits(width)<<(64-width)) >> (64 - width))
	default:
		v.SetUint(bits(t.Bits()))
	}
	return v
}

// An operand of helper name: a vector, a lane value, a count or a constant
// index within its range.
func operand(name string, t reflect.Type, first reflect.Type) reflect.Value {
	switch t.Kind() {
	case reflect.Array:
		v := reflect.New(t).Elem()
		for i := 0; i < t.Len(); i++ {
			v.Index(i).Set(lane(t.Elem()))
		}
		return v
	case reflect.Int:
		limit := 17 // byte counts
		if strings.HasPrefix(name, "zi_lane_") || strings.HasPrefix(name, "zi_with_lane_") {
			limit = first.Len()
		} else if strings.HasPrefix(name, "zi_shuffle_") {
			limit = 16 / int(first.Elem().Size())
		}
		return reflect.ValueOf(int(next() % uint64(limit)))
	}
	return lane(t)
}

func lanes(v reflect.Value) []reflect.Value {
	if v.Kind() != reflect.Array {
		return []reflect.Value{v}
	}
	out := make([]reflect.Value, v.Len())
	for i := range out {
		out[i] = v.Index(i)
	}
	return out
}

// Equal bits, or two NaNs.
func same(x, y reflect.Value) bool {
	switch x.Kind() {
	case reflect.Float32, reflect.Float64:
		a, b := x.Float(), y.Float()
		if a != a && b != b {
			return true
		}
		if x.Kind() == reflect.Float32 {
			return math.Float32bits(float32(a)) == math.Float32bits(float32(b))
		}
		return math.Float64bits(a) == math.Float64bits(b)
	}
	return x.Interface() == y.Interface()
}

func main() {
	failures := 0
	for _, helper := range zi_simd_helpers {
		name := helper[0].(string)
		typed, ref := reflect.ValueOf(helper[1]), reflect.ValueOf(helper[2])
		t := typed.Type()
		var first reflect.Type
		for i := 0; i < t.NumIn(); i++ {
			if t.In(i).Kind() == reflect.Array {
				first = t.In(i)
				break
			}
		}
		for round := 0; round < 300; round++ {
			args := make([]reflect.Value, t.NumIn())
			for i := range args {
				args[i] = operand(name, t.In(i), first)
			}
			x, y := typed.Call(args)[0], ref.Call(args)[0]
			a, b := lanes(x), lanes(y)
			for i := range a {
				if !same(a[i], b[i]) {
					failures++
					if failures <= 20 {
						operands := make([]any, len(args))
						for i, arg := range args {
							operands[i] = arg.Interface()
						}
						fmt.Fprintf(os.Stderr, "%s%v: %v, reference %v\n", name, operands, x, y)
					}
					break
				}
			}
		}
	}
	if failures > 0 {
		fmt.Fprintf(os.Stderr, "%d mismatches\n", failures)
		os.Exit(1)
	}
	fmt.Println(len(zi_simd_helpers), "helpers")
}
