package main
func main() {
    if JsonConformanceTest_JsonNative() != 0 || JsonConformanceTest_JsonOwnership() != 0 { panic("JSON conformance differs") }
}
