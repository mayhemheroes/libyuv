#!/usr/bin/env bash
#
# libyuv/mayhem/test.sh — RUN the standalone KAT binary (built by mayhem/build.sh from
# mayhem/harnesses/mayhem_kat.cc, linked against the SAME sanitized libyuv.a the fuzz harnesses
# use) and assert its printed output against precomputed golden values. Emit a CTRF summary.
#
# PATCH-grade oracle: mayhem_kat feeds two FIXED, deterministic frames (an 8x8 ARGB gradient and
# an 8x8 I420 gradient) through libyuv::ARGBToI420 and libyuv::I420Scale (scaled to 4x4), then
# prints an additive byte-checksum of each output plane plus one asserted center-pixel value. The
# golden numbers below were computed by actually running this exact program against libyuv's
# reference (unmodified) implementation — see mayhem/harnesses/mayhem_kat.cc for the derivation.
# A no-op/"exit(0)" neuter (or any patch that changes the converted/scaled bytes) prints nothing
# useful and the grep below fails — this asserts BEHAVIOR/OUTPUT, not exit code (§6.3). This
# script only RUNS the pre-built KAT; it never compiles.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
cd "$SRC"

KAT_BIN="$SRC/mayhem-build/mayhem_kat"

# Golden values: `KAT_CONVERT Y=<checksum of dst_y> U=<checksum of dst_u> V=<checksum of dst_v>
# CENTER_Y=<dst_y[4][4]>` for the 8x8 ARGB->I420 conversion, and `KAT_SCALE ...` for the 8x8->4x4
# I420Scale (kFilterBox). See mayhem_kat.cc for exactly how these frames/checksums are built.
EXPECT_CONVERT='KAT_CONVERT Y=407562024 U=1048430242 V=2988367772 CENTER_Y=126'
EXPECT_SCALE='KAT_SCALE Y=979012480 U=154320704 V=425890080 CENTER_Y=108'

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

if [ ! -x "$KAT_BIN" ]; then
  echo "missing $KAT_BIN — run mayhem/build.sh first" >&2
  emit_ctrf "libyuv-kat" 0 1 0; exit 2
fi

echo "=== running $KAT_BIN ==="
out="$("$KAT_BIN" 2>&1)"
echo "$out"

passed=0
failed=0

if printf '%s\n' "$out" | grep -qF "$EXPECT_CONVERT"; then
  echo "PASS: ARGBToI420 KAT matches golden output"
  passed=$((passed + 1))
else
  echo "FAIL: ARGBToI420 KAT did NOT match golden output (expected: $EXPECT_CONVERT)" >&2
  failed=$((failed + 1))
fi

if printf '%s\n' "$out" | grep -qF "$EXPECT_SCALE"; then
  echo "PASS: I420Scale KAT matches golden output"
  passed=$((passed + 1))
else
  echo "FAIL: I420Scale KAT did NOT match golden output (expected: $EXPECT_SCALE)" >&2
  failed=$((failed + 1))
fi

emit_ctrf "libyuv-kat" "$passed" "$failed" 0
