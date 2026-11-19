#!/usr/bin/env bash
# mayhem/build.sh — nanosvg is header-only (src/nanosvg.h + src/nanosvgrast.h): the library is
# compiled straight into each binary that defines NANOSVG_IMPLEMENTATION / NANOSVGRAST_IMPLEMENTATION.
#  1) fuzz_svg (libFuzzer) + fuzz_svg-standalone, library + harness built with $SANITIZER_FLAGS $DEBUG_FLAGS
#  2) nanosvg_selftest, the known-answer functional suite, with normal flags (run by mayhem/test.sh)
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${STANDALONE_FUZZ_MAIN:?STANDALONE_FUZZ_MAIN must be set (the base image provides it)}"
cd "$SRC"

OUT="$SRC/mayhem/out"      # scratch objects (gitignored by mayhem/.gitignore)
mkdir -p "$OUT"

# LeakSanitizer off at build time (SPEC §6.2 item 15).
# shellcheck disable=SC2086
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o "$OUT/lsan_off.o"

# 1) fuzz harness (the whole library is compiled in this TU, so it is instrumented too). The fuzzer
#    object is compiled WITH $LIB_FUZZING_ENGINE (SanitizerCoverage instrumentation); the standalone
#    reproducer gets its own object without it, so it needs no libFuzzer runtime.
# shellcheck disable=SC2086
$CC $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE -O1 -Isrc -c mayhem/harnesses/fuzz_svg.c -o "$OUT/fuzz_svg.o"
# shellcheck disable=SC2086
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$OUT/fuzz_svg.o" "$OUT/lsan_off.o" -lm -o /mayhem/fuzz_svg
# shellcheck disable=SC2086
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -O1 -Isrc -c mayhem/harnesses/fuzz_svg.c -o "$OUT/fuzz_svg_standalone.o"
# shellcheck disable=SC2086
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$OUT/standalone_main.o"
# shellcheck disable=SC2086
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS "$OUT/standalone_main.o" "$OUT/fuzz_svg_standalone.o" "$OUT/lsan_off.o" -lm -o /mayhem/fuzz_svg-standalone

# 2) functional suite with normal flags
# shellcheck disable=SC2086
$CC -O2 $COVERAGE_FLAGS -Isrc mayhem/selftest/nanosvg_selftest.c -lm -o /mayhem/nanosvg_selftest
