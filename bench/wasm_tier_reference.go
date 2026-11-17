// Independent Go references for the small scalar kernel table.
// Bounds and checksums match wasm_tier.zi. Build with ordinary go build.
package main

import (
	"encoding/json"
	"fmt"
	"os"
	"strconv"
	"time"
)

func sieve() int64 {
	var flags [20000]bool
	var count int64
	for i := 2; i < len(flags); i++ {
		if !flags[i] {
			count++
			for j := i * i; j < len(flags); j += i {
				flags[j] = true
			}
		}
	}
	return count
}

func records() int64 {
	type particle struct{ x, y, dx, dy int64 }
	var particles [64]particle
	for i := range particles {
		n := int64(i)
		particles[i] = particle{n, 2 * n, n%5 + 1, n%3 + 1}
	}
	for step := 0; step < 200; step++ {
		for i := range particles {
			p := &particles[i]
			p.x += p.dx
			p.y += p.dy
			if p.x > 1000 || p.x < 0 {
				p.dx = -p.dx
			}
			if p.y > 1000 || p.y < 0 {
				p.dy = -p.dy
			}
		}
	}
	var total int64
	for _, p := range particles {
		total += p.x + p.y
	}
	return total
}

func hash() int64 {
	text := "the quick brown fox jumps over the lazy dog, again and again"
	var h uint64 = 1469598103934665603
	for round := 0; round < 500; round++ {
		for i := 0; i < len(text); i++ {
			h = (h ^ uint64(text[i])) * 1099511628211
		}
	}
	return int64(h % 1000000007)
}

func vectors() int64 {
	var total int64
	for round := int64(0); round < 20; round++ {
		var values []int64
		for i := int64(0); i < 1000; i++ {
			values = append(values, i*round)
		}
		for i := 0; i < 1000; i++ {
			total += values[i]
		}
	}
	return total
}

func printing() int64 {
	for i := 0; i < 100; i++ {
		fmt.Fprintf(os.Stdout, "%d %d %t\n", i, i*i, i%7 == 0)
	}
	return 0
}

func main() {
	if len(os.Args) != 4 {
		panic("kernel expected samples")
	}
	kernels := map[string]func() int64{"Sieve": sieve, "Records": records, "Hash": hash, "Vectors": vectors, "Printing": printing}
	invoke := kernels[os.Args[1]]
	expected, err := strconv.ParseInt(os.Args[2], 10, 64)
	if err != nil || invoke == nil {
		panic("invalid kernel or oracle")
	}
	samples, err := strconv.Atoi(os.Args[3])
	if err != nil || samples != 11 {
		panic("exactly 11 samples required")
	}
	if invoke() != expected {
		panic("C oracle mismatch")
	}
	output := os.Stdout
	sink, err := os.OpenFile(os.DevNull, os.O_WRONLY, 0)
	if err != nil {
		panic(err)
	}
	defer sink.Close()
	os.Stdout = sink
	batch := func(count int) float64 {
		var checksum int64
		start := time.Now()
		for i := 0; i < count; i++ {
			checksum += invoke()
		}
		elapsed := float64(time.Since(start).Nanoseconds()) / 1e6
		if checksum != expected*int64(count) {
			panic("C oracle mismatch")
		}
		return elapsed
	}
	start := time.Now()
	warmup := 0
	for {
		batch(1)
		warmup++
		if time.Since(start) >= 150*time.Millisecond {
			break
		}
	}
	warmupMS := float64(time.Since(start).Nanoseconds()) / 1e6
	batches := 1
	for batches < 65536 && batch(batches) < 10 {
		batches *= 2
	}
	times := make([]float64, samples)
	for i := range times {
		times[i] = batch(batches)
	}
	os.Stdout = output
	if err := json.NewEncoder(output).Encode(map[string]any{
		"result": strconv.FormatInt(expected, 10), "batches": batches, "load_ms": 0,
		"warmup_ms": warmupMS, "warmup_count": warmup, "batch_samples_ms": times,
	}); err != nil {
		panic(err)
	}
}
