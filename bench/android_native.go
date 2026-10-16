package main

import (
	"encoding/json"
	"fmt"
	"os"
	"strconv"
	"syscall"
	"time"
)

func main() {
	if len(os.Args) != 3 {
		panic("expected rounds and seed")
	}
	rounds, err := strconv.ParseInt(os.Args[1], 10, 64)
	if err != nil || rounds < 1 {
		panic("invalid rounds")
	}
	seed, err := strconv.ParseUint(os.Args[2], 10, 64)
	if err != nil {
		panic(err)
	}
	var before, after syscall.Rusage
	if err := syscall.Getrusage(syscall.RUSAGE_SELF, &before); err != nil {
		panic(err)
	}
	started := time.Now()
	result := benchKernel(rounds, seed)
	elapsed := time.Since(started).Seconds() * 1000
	if err := syscall.Getrusage(syscall.RUSAGE_SELF, &after); err != nil {
		panic(err)
	}
	cpu := func(v syscall.Timeval) float64 { return float64(v.Sec)*1000 + float64(v.Usec)/1000 }
	row := map[string]interface{}{
		"result": result, "elapsed_ms": elapsed,
		"user_ms":   cpu(after.Utime) - cpu(before.Utime),
		"system_ms": cpu(after.Stime) - cpu(before.Stime), "max_rss_kib": after.Maxrss,
	}
	encoded, err := json.Marshal(row)
	if err != nil {
		panic(err)
	}
	fmt.Println(string(encoded))
}
