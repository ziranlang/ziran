#!/bin/sh
# std/math's float64 functions against the C library: every function over
# spread and random inputs, within a few units in the last place, natively
# and in a portable bundle.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY
printf '#import "math"\n#program_export\nMathCheckLinked :: () -> float64 { return Sqrt(2.0) }\n' > "$work/math_check.zi"
"$ziran" build --target=c --no-main --root "$work" --module-path "$repo/std" -o "$work/c" "$work/math_check.zi"
cat > "$work/compare.c" <<'C'
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

double Sqrt(double); double Cbrt(double); double Exp(double); double Expm1(double);
double Log(double); double Log1p(double); double Log10(double); double Log2(double);
double Sin(double); double Cos(double); double Tan(double);
double Asin(double); double Acos(double); double Atan(double); double Atan2(double, double);
double Sinh(double); double Cosh(double); double Tanh(double);
double Asinh(double); double Acosh(double); double Atanh(double); double Pow(double, double);

static uint64_t state = 88172645463325252ull;
static double uniform(double low, double high)
{
    state ^= state << 13; state ^= state >> 7; state ^= state << 17;
    return low + (high - low) * ((double)(state >> 11) / 9007199254740992.0);
}

static double ulps(double got, double want)
{
    if (isnan(want)) return isnan(got) ? 0.0 : 1e30;
    if (isinf(want)) return got == want ? 0.0 : 1e30;
    if (got == want) return 0.0;
    int64_t a, b;
    memcpy(&a, &got, 8); memcpy(&b, &want, 8);
    if (a < 0) a = INT64_MIN - a;
    if (b < 0) b = INT64_MIN - b;
    double d = (double)(a > b ? a - b : b - a);
    return d;
}

typedef double (*Unary)(double);
static int failures;

static void check(const char *name, Unary mine, Unary libc, double low, double high, int logarithmic, double limit)
{
    double worst = 0.0, where = 0.0;
    for (int i = 0; i < 20000; ++i) {
        double x = logarithmic ? exp(uniform(log(low), log(high))) : uniform(low, high);
        if (logarithmic && (i & 1)) x = -x * (low < 0 ? 1 : -1);
        double e = ulps(mine(x), libc(x));
        if (e > worst) { worst = e; where = x; }
    }
    printf("%-6s max %.1f ulp at %.17g\n", name, worst, where);
    if (worst > limit) { failures++; fprintf(stderr, "%s exceeds %.1f ulp\n", name, limit); }
}

static double pow_mine(double x) { return Pow(x, 2.718281828459045 - 0.5 * x / (1.0 + fabs(x))); }
static double pow_libc(double x) { return pow(x, 2.718281828459045 - 0.5 * x / (1.0 + fabs(x))); }

int main(void)
{
    check("sqrt", Sqrt, sqrt, 1e-300, 1e300, 1, 1);
    check("cbrt", Cbrt, cbrt, 1e-300, 1e300, 1, 3);
    check("exp", Exp, exp, -700, 700, 0, 1);
    check("expm1", Expm1, expm1, -1, 1, 0, 2);
    check("log", Log, log, 1e-300, 1e300, 1, 1);
    check("log1p", Log1p, log1p, -0.999, 10, 0, 2);
    check("log10", Log10, log10, 1e-300, 1e300, 1, 2);
    check("log2", Log2, log2, 1e-300, 1e300, 1, 2);
    check("sin", Sin, sin, -1e5, 1e5, 0, 2);
    check("cos", Cos, cos, -1e5, 1e5, 0, 2);
    check("tan", Tan, tan, -1e4, 1e4, 0, 3);
    check("sin", Sin, sin, -3, 3, 0, 2);
    check("asin", Asin, asin, -1, 1, 0, 3);
    check("acos", Acos, acos, -1, 1, 0, 3);
    check("atan", Atan, atan, -1e6, 1e6, 0, 2);
    check("atan", Atan, atan, -3, 3, 0, 2);
    check("atan", Atan, atan, 1e-12, 0.5, 1, 2);
    check("sinh", Sinh, sinh, -700, 700, 0, 3);
    check("cosh", Cosh, cosh, -700, 700, 0, 3);
    check("tanh", Tanh, tanh, -20, 20, 0, 3);
    check("asinh", Asinh, asinh, -1e6, 1e6, 0, 3);
    check("acosh", Acosh, acosh, 1, 1e6, 0, 3);
    check("atanh", Atanh, atanh, -0.999, 0.999, 0, 3);
    check("pow", pow_mine, pow_libc, 1e-3, 1e3, 1, 32);
    /* Exact and edge values a calculator shows. */
    if (Sqrt(144.0) != 12.0 || Cbrt(-27.0) != -3.0 || Pow(2.0, 10.0) != 1024.0 || Pow(-2.0, 3.0) != -8.0 ||
        Exp(0.0) != 1.0 || Log(1.0) != 0.0 || Sin(0.0) != 0.0 || Cos(0.0) != 1.0 || Atan2(1.0, -1.0) != atan2(1.0, -1.0) ||
        !isnan(Sqrt(-1.0)) || !isnan(Log(-1.0)) || !isinf(Exp(1000.0)) || Exp(-1000.0) != 0.0 || !isnan(Pow(-8.0, 1.0 / 3.0)) ||
        !isnan(Asin(1.5)) || !isinf(Atanh(1.0))) {
        fputs("edge values differ\n", stderr);
        failures++;
    }
    return failures != 0;
}
C
cc -std=c11 -O2 -iquote "$work/c" "$work/c"/*.c "$work/compare.c" -lm -o "$work/compare"
"$work/compare"
# The same functions in the portable interpreter, against libm's values.
cat > "$work/bundle_check.zi" <<'ZI'
#import "math"
Close :: (got: float64, want: float64) -> bool {
    difference := got - want
    if difference < 0.0 { difference = 0.0 - difference }
    scale := ifx want < 0.0 then 0.0 - want else want
    if scale < 1.0 { scale = 1.0 }
    return difference <= scale * 1.0e-15
}
#program_export
Answer :: () -> s32 {
    if !Close(Sqrt(2.0), 1.4142135623730951) || !Close(Exp(1.0), 2.718281828459045) ||
        !Close(Log(10.0), 2.302585092994046) || !Close(Sin(1.0), 0.8414709848078965) ||
        !Close(Cos(2.0), -0.4161468365471424) || !Close(Tan(0.5), 0.5463024898437905) ||
        !Close(Atan(3.0), 1.2490457723982544) || !Close(Asin(0.5), 0.5235987755982989) ||
        !Close(Acos(-0.25), 1.8234765819369754) || !Close(Sinh(2.0), 3.626860407847019) ||
        !Close(Cosh(2.0), 3.7621956910836314) || !Close(Tanh(0.75), 0.6351489523872873) ||
        !Close(Pow(2.0, 0.5), 1.4142135623730951) || !Close(Log10(2.0), 0.3010299956639812) ||
        !Close(Cbrt(10.0), 2.154434690031884) || !Close(Asinh(1.0), 0.881373587019543) ||
        !Close(Acosh(2.0), 1.3169578969248166) || !Close(Atanh(0.5), 0.5493061443340549) {
        return 1
    }
    return 42
}
ZI
"$ziran" bundle --root "$work" --module-path "$repo/std" --entry bundle_check:Answer -o "$work/check.zx" "$work/bundle_check.zi"
test "$("$ziran" run "$work/check.zx")" = 42
echo 'math-float64-test-ok: 21 functions within their ulp limits against libm, exact calculator values, portable bundle'
