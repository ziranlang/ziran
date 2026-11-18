package main

import (
	"encoding/json"
	"fmt"
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
	if kind < 0 || kind > 28 || n < 1 || (os.Args[4] != "11" && os.Args[4] != "0") {
		panic("invalid input")
	}
	invoke := func() {
		if value := Run(kind, n); value != expected {
			fmt.Fprintf(os.Stderr, "C oracle mismatch: got %d, expected %d\n", value, expected)
			os.Exit(6)
		}
	}
	invoke()
	if os.Args[4] == "0" {
		return
	}
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
