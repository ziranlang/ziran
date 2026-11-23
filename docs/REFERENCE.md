# Ziran reference for a first program

A short, complete reference for writing Ziran you have never seen before. Every
`zi` block below is a whole file and passes `ziran check` in the
`site_pages` check, so the syntax shown is current. A block whose first line is
`// file: NAME.zi` is a module that later blocks import.

Ziran is low level and manually safe like C: no garbage collector, no hidden
allocation, no exceptions, no runtime checks unless you build with `--checks`.
Owned values (`Vec`, `Text`) free themselves at scope exit and move on
assignment.

## 1. Declarations

`::` declares something that never changes (constant, procedure, type);
`:=` declares a local with an inferred type; `name: Type` declares with a type.

```zi
LIMIT :: 10;                 // constant (s64)
Pi :: 3.14159;               // constant (float64)
greeting :: "hello";         // constant string
counter: s64;                // file-scope variable: ends with ';', starts at zero
total: s32 = 5;              // file-scope variable with a value

Add :: (a: s32, b: s32) -> s32 {
    return a + b
}

// A parameter default; callers may name arguments: Scale(factor = 3, value = 2).
Scale :: (value: s64, factor: s64 = 2) -> s64 { return value * factor }

Log :: (message: string) {   // no result type: returns nothing
    print("%\n", message)
}

main :: () -> s32 {
    x := 1                   // s64
    y: s64 = 2
    z: float64               // zero until assigned
    z = 1.5
    sum := cast(s32)y + Add(1, 2)
    counter += 1             // no ++ or --
    Log(greeting)
    print("% % % %\n", x, z, sum, Scale(factor = 3, value = 2))
    return 0
}
```

Statements end at the newline; `;` only separates several on one line and ends
file-scope variables and constants.

## 2. Types

Scalars: `bool`, `s8 s16 s32 s64`, `u8 u16 u32 u64`, `int` (= `s64`),
`float32 float64`, `string` (borrowed UTF-8 bytes with `.count`). Composite:
`[4]s32` fixed array, `[]s32` slice (view), `*T` pointer, `Vec(T)` growable
owned array, `Text` owned string, records, enums, unions.

Integer widths never mix silently: `a_u64 < b_s64` is an error; write
`cast(s64)a_u64 < b_s64`. Untyped literals take the type their context needs.

```zi
Point :: struct {
    x: s32
    y: s32
}

Shape :: enum u8 { Circle; Square; Triangle :: 7; }

Box :: struct($T: Type) { value: T; }    // generic record
IntBox :: Box(s32)

Identity :: (value: $T) -> T { return value }   // generic procedure

Describe :: (shape: Shape) -> string {
    if shape == {                       // the switch statement
        case .Circle; return "circle"
        case .Square; return "square"
        case; return "other"            // default case
    }
    return ""
}

main :: () -> s32 {
    p := Point.{x = 3}                  // fields not named are zero
    q: Point = .{x = 1, y = 2}          // type known from the declaration
    box: IntBox = .{value = Identity(5)}
    s := Shape.Square
    t: Shape = .Circle                  // `.Member` when the type is known
    grid: [3]s64 = .[1, 2, 3]
    ones := s32.[1, 1]
    view: []s64 = grid[:]
    print("% % % % % %\n", p.x + q.y, box.value, Describe(s), Describe(t), view.count, ones.count)
    return 0
}
```

Records have no methods: write `Area(rect)`, not `rect.Area()`. Field
defaults are not supported; every field starts at zero.

## 3. Control flow and ranges

```zi
main :: () -> s32 {
    x: s64 = 0
    if x > 0 && !(x == 3) || false { x += 1 } else if x == 0 { x = 1 } else { x -= 1 }
    if x == 1 then x = 2                     // one-statement body
    while x < 10 { x += 1 }
    for i: 0..3 { x += i }                   // inclusive: 0, 1, 2, 3
    for 1..3 { x += it + it_index }          // unnamed value is `it`, position `it_index`
    for < i: 0..3 { x -= i }                 // reverse: 3, 2, 1, 0
    values := s64.[1, 2, 3]
    for values { x += it }                   // element copy
    for v, index: values { x += v * index }
    for * values { <<it = 0 }                // `it` is a pointer: modify in place
    for outer: 0..3 {
        for inner: 0..3 {
            if inner == 2 { continue outer }
            if outer == 3 { break outer }
        }
    }
    y := ifx x > 5 then 1 else 2             // the ?: operator
    defer print("runs at scope exit\n")
    print("% %\n", x, y)
    return 0
}
```

There is no `for (;;)`, no `for i in a..b`, no `switch`/`match`, no `goto`.
A range `0..n` includes `n`: loop over indexes with `for i: 0..values.count - 1`.

## 4. string versus Text

`string` is a borrowed view (pointer + `.count`). `Text` owns its bytes and is
freed at scope exit. Use `string` for parameters; return `Text` when you build
new text.

```zi
#import "std/text_tools"

Shout :: (name: string) -> Text {
    out: Vec(u8)
    BuilderPrint(*out, "HELLO, %! (% bytes, 100%%)\n", name, name.count)
    return BuilderFinish(out)
}

main :: () -> s32 {
    s := "hello world"
    first := s[0:5]                       // view of bytes 0..4
    if s[0] == #char "h" && s.count == 11 && first == "hello" { print("yes\n") }
    owned: Text = TextCopy(s)             // owned copy
    BuilderAppend(owned.bytes, "!")       // grow it
    view := TextView(owned)               // borrow it as a string
    message: Text = Shout(view)
    print("%", TextView(message))
    at := TextFind(s, "world")            // -1 when absent
    trimmed := TextTrim("  x  ")
    joined: Text = TextReplace(s, "world", "there")
    print("% % %\n", at, trimmed, TextView(joined))
    return 0
}
```

Strings compare with `==`; there is no `+` for strings: build with
`BuilderPrint`/`BuilderAppend` or `TextJoin`.

## 5. Vec and HashMap

```zi
#import "std/option"
#import "std/hash_map"

main :: () -> s32 {
    numbers: Vec(s64)                  // empty; no constructor call
    VecPush(numbers, 4)
    VecPush(numbers, 5)
    numbers[0] = 3
    sum: s64 = 0
    for numbers { sum += it }
    last := VecPop(numbers)            // Option(s64), needs std/option
    if last.has_value { sum += last.value }
    {
        head := numbers[0:1]           // []s64 view; VecSlice(numbers, 0, 1) is the same
        print("% %\n", sum, head.count)
    }                                  // the view ends with its block; numbers may grow again
    if !VecReserve(numbers, 100) { return 1 }
    VecClear(numbers)

    ages: HashMap(string, s32)
    HashMapSet(*ages, "ada", 36)
    age: s32
    if HashMapGet(*ages, "ada", *age) { print("% %\n", age, ages.count) }
    unused HashMapRemove(*ages, "ada")
    HashMapFree(*ages)
    return 0
}
```

`Vec(T)` and `Text` need no import. Vec operations are compiler builtins:
`VecPush(v, x)`, `VecPop(v) -> Option(T)`, `VecClear(v)`, `VecFree(v)`,
`VecSwap(a, b)` (two Vecs), `VecSlice(v, low, high) -> []T`,
`VecReserve(v, capacity) -> bool`, `VecResize(v, count) -> bool`.

## 6. Multiple results and errors

No exceptions. A procedure that can fail returns its value and a `bool`.

```zi
Divide :: (a: s64, b: s64) -> (quotient: s64, ok: bool) {
    if b == 0 { return 0, false }
    return a / b, true
}

Half :: (value: s64) -> s64 #must { return value / 2 }

main :: () -> s32 {
    value, ok := Divide(10, 2)
    if !ok { return 1 }
    _, zero_ok := Divide(1, 0)          // `_` drops a result
    other, other_ok := Divide(9, 3)     // new names: `ok` is already bound here
    half := Half(value)                 // #must: the result has to be used
    _ := Half(4)                        // explicit discard
    print("% % % % %\n", value, zero_ok, other, other_ok, half)
    return 0
}
```

Result names (`quotient`, `ok`) are documentation only: they are not locals.
Assign your own locals and `return a, b`. Two `:=` of one name in one block is
an error; pick a new name or assign with `=`.

## 7. Moves and views

The rules that trip newcomers most. All are checked at compile time.

1. **Owned values move.** `b := a` for a `Vec`/`Text` moves `a`; using `a`
   afterwards is "Vec binding is used after moving". Passing a Vec to a
   `Vec(T)` parameter or returning it moves it too.
2. **Mutate through a pointer instead of moving.** Take `v: *Vec(T)` and call
   `VecPush(<<v, x)`; the caller writes `Grow(*items)`.
3. **A view borrows its owner.** `string`, `[]T`, `TextView(t)` and `v[a:b]`
   point into storage. Until the block holding the view ends, the owner
   cannot grow, move or free ("cannot mutate a Vec with a live borrowed
   view"); put a short-lived view in its own `{ }` block.
4. **Loop over a call directly; bind it to slice it.** `for TextLines(x) { }`
   binds the result for the loop and frees it when the loop ends. A slice of
   a call result, `TextLines(x)[0:2]`, needs `lines := TextLines(x)` first.
5. **Owned results: `:=` keeps a string, `: Text` keeps ownership.**
   `name := Name()` holds the bytes as a `string` for the rest of the
   program; `name: Text = Name()` frees them with `name`. `TextView` works
   on either.
6. **Never return a view of a local.** `return TextView(local_text)` is
   "returned text view borrows local or temporary storage": return the `Text`.
7. **Owned elements stay in place.** For a `Vec` of records that own Vecs,
   iterate `for * items` and index `items[i].parts`; `for items` would copy.

```zi
#import "std/text_tools"

Group :: struct { name: string; parts: Vec(string); }

Grow :: (values: *Vec(s64), x: s64) { VecPush(<<values, x) }

Name :: () -> Text { return TextCopy("ada") }

Take :: (values: Vec(s64)) -> s64 { return values.count }   // consumes `values`

main :: () -> s32 {
    items: Vec(s64)
    Grow(*items, 1)                      // pointer: items stays usable
    Grow(*items, 2)
    for TextLines("a\nb") { print("%\n", it) }   // freed when the loop ends
    name: Text = Name()                  // typed: stays owned, freed at scope exit
    print("%\n", TextView(name))
    groups: Vec(Group)
    group: Group
    VecPush(group.parts, "x")
    VecPush(groups, group)               // moves `group` into the Vec
    for * groups { VecPush(it.parts, "y") }
    groups[0].parts[0] = "z"
    count := Take(items)                 // `items` is moved: do not use it again
    print("% %\n", count, groups[0].parts.count)
    return 0
}
```

## 8. Pointers

`*x` takes an address, `<<p` reads or writes through it (`p.*` reads too),
`p.field` dereferences automatically, `null` is the null pointer. There is
no `&x`, no `*p = v`, no `p->field`.

```zi
Point :: struct { x: s32; y: s32; }

Move :: (p: *Point, dx: s32) {
    p.x += dx                            // field through a pointer
    <<p = Point.{x = p.x, y = 2}         // store the whole value
}

main :: () -> s32 {
    point := Point.{x = 1}
    Move(*point, 4)
    n: s64 = 1
    pn := *n
    <<pn = 5
    read := pn.*
    missing: *Point = null
    if missing == null { print("% % %\n", point.x, n, read) }
    return 0
}
```

`<<` and `&` between two values are still shift and bitwise and.

## 9. Modules and imports

A file is a module named after the file. `#import "name"` searches `--root`
and the module paths; `std/...` names the standard library. Three forms:
`#import "geometry"` brings its names in bare, `Geo :: #import "geometry";`
only as `Geo.Area`, and `using Geo :: #import "geometry";` both ways, so a
name that clashes can still be written `Geo.Area`.

```zi
// file: geometry.zi
Rect :: struct { width: s64; height: s64; }

Area :: (r: Rect) -> s64 { return r.width * r.height }

#scope_file
Helper :: () -> s64 { return 1 }         // private to this file
```

```zi
#import "geometry"                       // names come in unqualified
Geo :: #import "geometry";               // or qualified: Geo.Area
Op :: #type (s64, s64) -> s64;           // procedure type

Add :: (a: s64, b: s64) -> s64 { return a + b }
Apply :: (op: Op, a: s64, b: s64) -> s64 { return op(a, b) }

main :: () -> s32 {
    r := Rect.{width = 2, height = 3}
    g := Geo.Rect.{width = 1, height = 1}
    print("%\n", Area(r) + Geo.Area(g) + Apply(Add, 1, 2))
    return 0
}
```

Procedure values cannot capture locals (no closures); pass state explicitly.

## 10. Formatting

`print(format, args...)` writes to standard output with no import.
`BuilderPrint(*bytes, format, args...)` appends to a `Vec(u8)`. Each `%`
takes the next argument whatever its type; `%%` is one `%`. There is no
`%s`, `%d` or `{}`. The format must be a literal string.

```zi
main :: () -> s32 {
    out: Vec(u8)
    name := "ada"
    BuilderPrint(*out, "% is % years, % done\n", name, 36, 0.5)
    BuilderPrint(*out, "%", 90)          // to print "90%": the number, then the sign
    Append(*out, "%\n")
    Append(*out, "raw text ")
    Append(*out, cast(s64)42)
    Append(*out, "\n")
    print("%", BuilderFinish(out))
    return 0
}
```

`%%` is matched before `%`, left to right, so `"%%%"` is a literal `%` and
then an argument, never an argument and then `%`. Floats print up to six
digits after the point.

## 11. Tests

A test is an ordinary program whose `main` returns nonzero on failure.
`#assert` checks constant facts at compile time.

```zi
// file: tally.zi
Tally :: (values: []s64) -> s64 {
    total: s64 = 0
    for values { total += it }
    return total
}
```

```zi
#import "tally"

failures: s32;

Expect :: (ok: bool, what: string) {
    if !ok { failures += 1; print("FAIL: %\n", what) }
}

#assert size_of(s64) == 8;

main :: () -> s32 {
    values := s64.[1, 2, 3]
    Expect(Tally(values[:]) == 6, "sum of 1, 2, 3")
    empty: []s64
    Expect(Tally(empty) == 0, "empty sum")
    if failures == 0 { print("all checks passed\n") }
    return ifx failures == 0 then 0 else 1
}
```

## 12. A command-line program

```zi
// Arguments, standard input, files, JSON and sorting.
#import "std/byte_text_linux"
#import "std/stdio_linux"
#import "std/file"
#import "std/json_document"
#import "std/sort"
#import "std/text_tools"

main :: (argc: s32, argv: **u8) -> s32 {
    if argc < 2 { unused PrintError("usage: tool FILE\n"); return 2 }
    path := TextFromCString(argv[1])
    text, ok := ReadEntireFile(path)
    if !ok { unused PrintError("cannot read file\n"); return 1 }
    document: JsonDocument
    if !JsonRead(*document, text) { unused PrintError("bad JSON\n"); return 1 }
    name: Text = JsonString(*document, JsonMember(*document, document.root, "name"))
    age, has_age := JsonInteger(*document, JsonMember(*document, document.root, "age"))
    input: Text = ReadStandardInput()
    words := TextSplit(TextView(input), " ")
    SortText(words[:])
    out: Vec(u8)
    BuilderPrint(*out, "% % % %\n", TextView(name), age, has_age, words.count)
    unused WriteOutput(BuilderFinish(out))
    unused WriteEntireFile("build/out.txt", "done\n")
    return 0
}
```

## 13. Commands

```sh
ziran check main.zi                          # type-check one file; its folder is the root
ziran check --lint --root . main.zi          # also warn on needless casts and -1 divisors
ziran check --diagnostics=json --root . main.zi
ziran run --target=c --root . main.zi -- ARGS   # build natively and run
ziran build --target=c --exe --entry main:main --root . -o build main.zi
ziran explain check.type                     # explain a diagnostic code
ziran api --root . main.zi                   # checked signatures, Vec builtins included
ziran api kryon/button                       # in a project: one package module, fields and signatures
ziran std --list                             # every standard module and its purpose
ziran new hello && cd hello && ziran lock && ziran run   # a project
```

In a project (`ziran.toml`), run `ziran lock` once, then `ziran check`,
`ziran build` and `ziran run` without a file. Two dependencies may pin different
revisions of one package and both are linked; in generated C they must not
both define one `#program_export` name, so make such pins agree. Plain `ziran run file.zi`
uses the portable engine, which has no `std/*_linux` modules: use
`--target=c` for programs that touch files, input or processes.

## 14. You would guess X; Ziran is Y

| You would write (C, Go, Rust, JS) | Ziran |
| --- | --- |
| `let x = 1`, `var x = 1`, `const x = 1`, `x := 1` at file scope | local `x := 1`; file scope `x: s64 = 1;`; constant `X :: 1;` |
| `fn f(a: i32) -> i32`, `func f(a int) int`, `def f(a)`, `function f(a)` | `F :: (a: s32) -> s32 { ... }` |
| `int`, `i32`, `i64`, `f64`, `double`, `usize` | `s32`, `s32`, `s64`, `float64`, `float64`, `u64` |
| `for (i = 0; i < n; i++)`, `for i in 0..n`, `for i := range n` | `for i: 0..n - 1 { }` (inclusive) |
| `for x in xs`, `for _, x := range xs`, `xs.forEach` | `for xs { it }` or `for x: xs { }`, `for x, i: xs { }` |
| `switch`, `match` | `if value == { case .A; ... case; ... }` |
| `c ? a : b` | `ifx c then a else b` |
| `i++`, `i--` | `i += 1`, `i -= 1` |
| `nil`, `NULL`, `None`, `undefined` | `null` (pointers only) |
| `&x`, `*p = v`, `p->f` | `*x`, `<<p = v`, `p.f` |
| `(int)x`, `int(x)`, `x as i32` | `cast(s32)x` |
| `s.len()`, `len(s)`, `s.length`, `strlen(s)` | `s.count` |
| `printf("%d %s", a, b)`, `fmt.Printf`, `` `${a}` `` | `print("% %\n", a, b)` |
| `a + b` on strings | `BuilderAppend(out, a); BuilderAppend(out, b)` on `out: Vec(u8)`, then `BuilderFinish(out)` |
| `'a'` | `#char "a"` |
| `struct P { int x; }`, `type P struct { x int }` | `P :: struct { x: s32 }` |
| `P{x: 1}`, `P { x: 1 }`, `{x: 1}` | `P.{x = 1}` or `.{x = 1}` |
| `enum E { A, B }` | `E :: enum { A; B; }`, values `E.A` or `.A` |
| `[]int{1, 2}`, `vec![1, 2]`, `[1, 2]` | `s64.[1, 2]` (fixed array), or a `Vec(s64)` with `VecPush` |
| `[[1, 2], [3, 4]]`, `{{1, 2}, {3, 4}}` | `t: [2][2]s64 = .[.[1, 2], .[3, 4]]`, at file scope or in a procedure |
| `v.push(x)`, `append(v, x)`, `v.pop()` | `VecPush(v, x)`, `VecPop(v)` |
| `map[string]int`, `HashMap<String, i32>`, `new Map()` | `HashMap(string, s32)` from `std/hash_map` |
| `x.method()` | `Method(x)` or `Method(*x)` |
| `try`/`catch`, `?`, `Result<T, E>`, `if err != nil` | `value, ok := F()`; `if !ok { ... }` |
| `import x from "y"`, `use y::x`, `#include "y.h"` | `#import "y"` or `Y :: #import "y";` |
| `T<U>`, `Vec<T>`, `func F[T any]` | `Vec(T)`, `F :: (x: $T) -> T` |
| closures `x => x + 1` | named procedure plus explicit state |
| `free(p)`, `delete`, `drop(v)` | automatic at scope exit; `VecFree(v)` early |
| `goto`, `do { } while` | `while`, `break`, labeled `continue name` |

## 15. Standard library

| Import | Signatures |
| --- | --- |
| (none) | `print(format: string, ..)`, `Vec(T)`, `Text`, `TextCopy(s: string) -> Text`, `TextView(t: Text) -> string`, `TextDetach(t: Text) -> string`, `BuilderPrint(out: *Vec(u8), format, ..)`, `BuilderFinish(bytes: Vec(u8)) -> string`, `BuilderAppend(bytes: Vec(u8), s: string)` |
| `std/format` | `Append(out: *Vec(u8), value: string or s64 or u64 or bool or float64)` |
| `std/option` | `Option(T)` with `has_value`, `value`; needed by `VecPop` |
| `std/text_tools` | `TextFind(text, needle, from := 0) -> s64`, `TextSplit(text, sep) -> Vec(string)`, `TextLines(text) -> Vec(string)`, `TextTrim(text) -> string`, `TextHasPrefix(text, prefix) -> bool`, `TextReplace(text, old, new) -> Text`, `TextJoin(parts: []string, sep) -> Text`, `TextInteger(text) -> (value: s64, ok: bool)` |
| `std/unicode` | `Lower(text) -> Text`, `CaseFold(text) -> Text`, `SplitWhitespace(text) -> Vec(string)` |
| `std/hash_map` | `HashMap(K, V)`, `HashMapSet(*m, key, value)`, `HashMapGet(*m, key, *value) -> bool`, `HashMapHas(*m, key) -> bool`, `HashMapRemove(*m, key) -> bool`, `HashMapFree(*m)` |
| `std/sort` | `Sort(values: []$T)`, `SortText(values: []string)`, `BinarySearch(values: []$T, target: T) -> s64` |
| `std/file` | `ReadEntireFile(path) -> (text: string, ok: bool)`, `WriteEntireFile(path, text) -> bool` |
| `std/file_linux` | `PathExists(path) -> bool`, `CreateDirectory(path) -> bool`, `RemoveFile(path) -> bool` |
| `std/file_tree_linux` | `ListDirectory(path, names: *Vec(string)) -> bool` |
| `std/stdio_linux` | `WriteOutput(text) -> bool`, `PrintError(text) -> bool`, `ReadStandardInput() -> Text` |
| `std/byte_text_linux` | `TextFromCString(value: *u8) -> string`, `TextFromBytes(bytes: []u8) -> string` |
| `std/environment_linux` | `EnvironmentText(name) -> string`, `EnvironmentNumber(name, fallback: s64) -> s64` |
| `std/json_document` | `JsonRead(*doc, text) -> bool`, `JsonMember(*doc, node, key) -> s32`, `JsonElement(*doc, node, index) -> s32`, `JsonString(*doc, node) -> Text`, `JsonInteger(*doc, node) -> (value: s64, ok: bool)`, `JsonWrite(*doc, node, style) -> Text` |
| `std/process_run_linux` | `RunProcess(options: ProcessOptions, run: *ProcessRun) -> bool` |

## 16. Edges

- Arithmetic is C's: division by zero traps; `S64_MIN / -1` and `S64_MIN % -1`
  wrap. Check the divisor yourself; `ziran check --lint` warns on a constant `-1`.
- A constant may name its type with a cast, `LIMIT :: cast(s64)9223372036854775807;`;
  a value the type cannot hold is "invalid or ambiguous constant".
- `VecSlice` and `v[a:b]` need the Vec variable or a field of one, not
  `<<pointer`: take a slice parameter (`Rest(items[:])`) instead.
- A call cannot move a global Vec (it would empty the global): pass `*items`
  or move it into a local first.
- `BuilderPrint` needs a literal format; branch around the call instead of
  choosing the format with `ifx`.

```zi
Rest :: (values: []s64) -> []s64 { return values[1:] }   // view of caller storage

Count :: (values: *Vec(s64)) -> s64 { return values.count }

SafeDivide :: (a: s64, b: s64) -> (value: s64, ok: bool) {
    if b == 0 || (b == -1 && a == -9223372036854775807 - 1) { return 0, false }
    return a / b, true
}

items: Vec(s64);

main :: () -> s32 {
    VecPush(items, 1)
    VecPush(items, 2)
    rest := Rest(items[:])
    q, ok := SafeDivide(7, 2)
    print("% % % %\n", rest.count, Count(*items), q, ok)
    return 0
}
```

Where several modules cover one job, use: `std/text_tools` to find, split,
trim and replace, `std/unicode` when case or whitespace must follow Unicode
(`std/text` folds ASCII only); `std/json_document` to read, change and write
JSON, `std/json_scan` only to pick values out of large input without a tree;
`std/date_time_linux` for clocks in a native Linux program, `std/date_time`
in a portable bundle, `std/calendar` for date arithmetic; `std/net_http` for
HTTP through a host, `std/net_http_curl_linux` or `std/net_http_linux` in a
native Linux program; `std/audio` with its `std/audio_decode_*` decoders.

`ziran api --root DIR FILE.zi` prints every checked signature of a module and
its imports; `ziran explain CODE` explains any diagnostic code.
