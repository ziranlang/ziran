package main

import (
	"encoding/json"
	"os"
	"strconv"
	"time"
)

func main() {
	if len(os.Args) != 6 {
		panic("arguments")
	}
	kind, e := strconv.ParseInt(os.Args[2], 10, 64)
	if e != nil {
		panic(e)
	}
	n, e := strconv.ParseInt(os.Args[3], 10, 64)
	if e != nil {
		panic(e)
	}
	expected, e := strconv.ParseInt(os.Args[5], 10, 64)
	if e != nil {
		panic(e)
	}
	if kind < 0 || kind > 10 || n < 1 || os.Args[4] != "11" {
		panic("invalid input")
	}
	invoke := func() {
		if Run(kind, n) != expected {
			panic("C oracle mismatch")
		}
	}
	invoke()
	start := time.Now()
	for time.Since(start) < 150*time.Millisecond {
		invoke()
	}
	times := make([]float64, 11)
	for i := range times {
		start = time.Now()
		value := Run(kind, n)
		times[i] = float64(time.Since(start).Nanoseconds()) / 1e6
		if value != expected {
			panic("C oracle mismatch")
		}
	}
	if e = json.NewEncoder(os.Stdout).Encode(map[string]any{"result": strconv.FormatInt(expected, 10), "batches": 1, "batch_samples_ms": times}); e != nil {
		panic(e)
	}
}
