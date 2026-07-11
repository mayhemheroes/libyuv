#!/usr/bin/env bash
#
# libyuv/mayhem/build.sh — build two net-new libFuzzer harnesses against libyuv's YUV
# scaling/conversion API, plus a standalone known-answer-test (KAT) binary for mayhem/test.sh.
#
# libyuv has NO OSS-Fuzz build.sh upstream (oss-fuzz/projects/libyuv only ships project.yaml) —
# these harnesses are written for this integration, not ported from OSS-Fuzz. The fuzzed surface:
#   libyuv_convert_fuzzer — libyuv::ARGBToI420()  (source/convert_from_argb.cc, planar_functions.cc)
#   libyuv_scale_fuzzer   — libyuv::I420Scale()   (source/scale.cc, scale_common.cc, scale_any.cc)
#
# Build contract comes from the org base ENV (CC/CXX/SANITIZER_FLAGS/LIB_FUZZING_ENGINE/SRC/
# STANDALONE_FUZZ_MAIN). We compile libyuv's C/C++ sources ourselves (mirroring CMakeLists.txt's
# `ly_common_source_files` list for the x86_64 build; libyuv's CMake only adds NEON/LSX/LASX/SVE/
# SME sources for their respective non-x86 architectures) WITH $SANITIZER_FLAGS so the fuzzed
# scaler/converter code — not just the harness — is instrumented. No cmake/gtest/libjpeg needed:
# HAVE_JPEG is never defined, so the (upstream-optional) MJPEG decode path compiles out cleanly,
# and we don't build libyuv's own unit_test/ (which needs gtest) — the functional oracle here is a
# small, self-contained KAT (see mayhem/harnesses/mayhem_kat.cc) instead.
set -euo pipefail

# clang rejects SOURCE_DATE_EPOCH='' — must be unset or a valid integer.
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

# `=` (not `:=`) for SANITIZER_FLAGS so an explicit empty --build-arg builds with NO sanitizers.
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE

cd "$SRC"

HARNESS_DIR="$SRC/mayhem/harnesses"
INC="-I$SRC/include"
CXXSTD="-std=c++14"

# The x86_64 subset of CMakeLists.txt's ly_common_source_files (NEON/LSX/LASX/RVV/SVE/SME sources
# are only added by CMake for their respective non-x86 architectures — see CMakeLists.txt:22-62).
LIBYUV_SRCS="
compare.cc compare_common.cc compare_gcc.cc compare_win.cc
convert_argb.cc convert.cc convert_from_argb.cc convert_from.cc convert_jpeg.cc
convert_to_argb.cc convert_to_i420.cc cpu_id.cc mjpeg_decoder.cc mjpeg_validate.cc
planar_functions.cc rotate_any.cc rotate_argb.cc rotate.cc rotate_common.cc rotate_gcc.cc
rotate_lsx.cc rotate_win.cc row_any.cc row_common.cc row_gcc.cc row_lasx.cc row_lsx.cc
row_rvv.cc row_win.cc scale_any.cc scale_argb.cc scale.cc scale_common.cc scale_gcc.cc
scale_lsx.cc scale_rgb.cc scale_rvv.cc scale_uv.cc scale_win.cc video_common.cc
"

# ── 1) Build libyuv's sources WITH sanitizers (the fuzzed converter/scaler is instrumented) ────────
BUILD="$SRC/mayhem-build"
mkdir -p "$BUILD"
OBJS=()
for s in $LIBYUV_SRCS; do
  obj="$BUILD/$(basename "${s%.cc}").o"
  $CXX $CXXSTD $SANITIZER_FLAGS $DEBUG_FLAGS $INC -c "source/$s" -o "$obj"
  OBJS+=("$obj")
done
LIBYUV="$BUILD/libyuv.a"
rm -f "$LIBYUV"; ar rcs "$LIBYUV" "${OBJS[@]}"

# Standalone driver (LLVM's run-once main from the base image). Compiled as a C object once;
# linking it directly against a C++ harness works fine since LLVMFuzzerTestOneInput is declared
# `extern "C"` in both harnesses.
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$BUILD/standalone_main.o"

# ── 2) Build each harness twice: libFuzzer target (-> /mayhem/<name>) + standalone reproducer ──────
for harness in libyuv_convert_fuzzer libyuv_scale_fuzzer; do
  $CXX $CXXSTD $SANITIZER_FLAGS $DEBUG_FLAGS $INC \
      "$HARNESS_DIR/$harness.cc" $LIB_FUZZING_ENGINE "$LIBYUV" -lm \
      -o "/mayhem/$harness"

  $CXX $CXXSTD $SANITIZER_FLAGS $DEBUG_FLAGS $INC \
      "$HARNESS_DIR/$harness.cc" "$BUILD/standalone_main.o" "$LIBYUV" -lm \
      -o "/mayhem/$harness-standalone"

  echo "built $harness (+ standalone)"
done

# ── 3) Build the standalone KAT binary (mayhem/test.sh's functional oracle) against the SAME
#       sanitized libyuv.a — normal (non-fuzzer) flags/entry point, just $DEBUG_FLAGS for parity. ──
$CXX $CXXSTD $SANITIZER_FLAGS $DEBUG_FLAGS $INC \
    "$HARNESS_DIR/mayhem_kat.cc" "$LIBYUV" -lm \
    -o "$SRC/mayhem-build/mayhem_kat"
echo "built mayhem_kat"

echo "build.sh complete:"
ls -la /mayhem/libyuv_convert_fuzzer /mayhem/libyuv_scale_fuzzer \
       /mayhem/libyuv_convert_fuzzer-standalone /mayhem/libyuv_scale_fuzzer-standalone \
       "$SRC/mayhem-build/mayhem_kat" 2>&1 || true
