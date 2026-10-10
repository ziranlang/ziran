#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
library=${ZIRAN_LIB:-"$(dirname "$ziran")/../libziran.a"}
build=$(CDPATH= cd -- "$(dirname -- "$library")" && pwd)
work=$build/tests/vm-builtin-dispatch
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
cat > "$work/app.zi" <<'ZI'
T :: () -> s32 { return 1 }
p :: () -> s32 { return 2 }
z :: () -> s32 { return 3 }
V :: () -> s32 { return 4 }
B :: () -> s32 { return 5 }
VecPushMore :: () -> s32 { return 6 }
VecPopMore :: () -> s32 { return 7 }
VecGetMore :: () -> s32 { return 8 }
VecCloneMore :: () -> s32 { return 9 }
VecClearMore :: () -> s32 { return 10 }
VecFreeMore :: () -> s32 { return 11 }
VecSwapMore :: () -> s32 { return 12 }
VecSliceMore :: () -> s32 { return 13 }
BuilderAppendMore :: () -> s32 { return 14 }
BuilderFinishMore :: () -> s32 { return 15 }
TextViewMore :: () -> s32 { return 16 }
printmore :: () -> s32 { return 17 }
zi_new_more :: () -> s32 { return 18 }
zi_free_more :: () -> s32 { return 19 }
#program_export
Answer :: () -> s32 {
    total: s32
    for i: 0..999 {
        total += T() + p() + z() + V() + B() + VecPushMore() + VecPopMore() +
            VecGetMore() + VecCloneMore() + VecClearMore() + VecFreeMore() +
            VecSwapMore() + VecSliceMore() + BuilderAppendMore() + BuilderFinishMore() +
            TextViewMore() + printmore() + zi_new_more() + zi_free_more()
    }
    return ifx total == 190000 then 0 else 1
}
ZI
"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
for form in source saved; do
    root=$work
    suffix=zi
    if test "$form" = saved; then root=$work/ir; suffix=zir; fi
    "$ziran" bundle --root "$root" --entry app:Answer -o "$work/$form.zib" "$root/app.$suffix"
    test "$("$ziran" run "$work/$form.zib")" = 0
    for target in c cpp; do
        "$ziran" build --root "$root" --target="$target" --entry app:Answer --no-main \
            -o "$work/$form-$target" "$root/app.$suffix"
        if test "$target" = c; then
            printf '%s\n' '#include "app.h"' 'int main(void) { return Answer(); }' > "$work/$form-$target/main.c"
            "${CC:-cc}" -std=c11 -I"$repo/include" -I"$work/$form-$target" \
                "$work/$form-$target"/*.c -o "$work/$form-$target/app"
        else
            printf '%s\n' '#include "app.hpp"' 'int main() { return Answer(); }' > "$work/$form-$target/main.cpp"
            "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$work/$form-$target" \
                "$work/$form-$target"/*.cpp -o "$work/$form-$target/app"
        fi
        "$work/$form-$target/app"
    done
    "$ziran" build --root "$root" --target=go --pkg main --entry app:Answer --no-main \
        -o "$work/$form-go" "$root/app.$suffix"
    printf '%s\n' 'package main' 'func main() { if App_Answer() != 0 { panic("builtin prefix call") } }' > "$work/$form-go/main.go"
    GO111MODULE=off go run "$work/$form-go"/*.go
done
cmp "$work/source.zib" "$work/saved.zib"
echo 'Short and builtin-prefix function names retain ordinary calls across source, saved IR, native and portable execution'
