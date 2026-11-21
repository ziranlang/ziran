package main
import "os"
func main() {
    if len(os.Args) == 1 {
        if JsonStringScanTest_NativeMain() != 0 { panic("SIMD string scanning differs") }
        return
    }
    switch os.Args[1] {
    case "InvalidNegative": JsonStringScanTest_InvalidNegative()
    case "InvalidEnd": JsonStringScanTest_InvalidEnd()
    case "InvalidLowEnd": JsonStringScanTest_InvalidLowEnd()
    case "InvalidWideEnd": JsonStringScanTest_InvalidWideEnd()
    default: panic("unknown case")
    }
}
