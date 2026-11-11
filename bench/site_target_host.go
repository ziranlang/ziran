package main

import (
    "encoding/json"
    "os"
    "strconv"
    "time"
)

// The generated bridge supplies matrixEntry. Resolve a function value at
// runtime so constant examples retain the same observable call boundary.
var matrixInvoke = map[string]func() int64{"entry": matrixEntry}["entry"]

func main() {
    if len(os.Args) != 4 { panic("expected artifact, oracle and sample count") }
    expected, err := strconv.ParseInt(os.Args[2], 10, 64)
    if err != nil { panic(err) }
    samples, err := strconv.Atoi(os.Args[3])
    if err != nil || samples < 3 || samples > 31 { panic("invalid sample count") }
    batch := func(count int) float64 {
        checksum := int64(0)
        start := time.Now()
        for i := 0; i < count; i++ { checksum += matrixInvoke() }
        elapsed := float64(time.Since(start).Nanoseconds()) / 1e6
        if checksum != expected * int64(count) { panic("benchmark oracle mismatch") }
        return elapsed
    }
    if matrixInvoke() != expected { panic("first result differs from oracle") }
    originalOutput := os.Stdout
    sink, err := os.OpenFile(os.DevNull, os.O_WRONLY, 0)
    if err != nil { panic(err) }
    os.Stdout = sink
    start := time.Now()
    warmup := 0
    for time.Since(start) < 150*time.Millisecond { batch(1); warmup++ }
    warmed := float64(time.Since(start).Nanoseconds()) / 1e6
    batches := 1
    for batches < 65536 && batch(batches) < 10 { batches *= 2 }
    times := make([]float64, samples)
    for i := range times { times[i] = batch(batches) }
    os.Stdout = originalOutput
    sink.Close()
    result := map[string]any{"result": strconv.FormatInt(expected, 10), "batches": batches,
        "load_ms": 0, "warmup_ms": warmed, "warmup_count": warmup, "batch_samples_ms": times}
    if err := json.NewEncoder(os.Stdout).Encode(result); err != nil { panic(err) }
}
