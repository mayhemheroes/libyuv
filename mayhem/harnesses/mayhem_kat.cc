// mayhem_kat.cc — standalone known-answer test (KAT) for the mayhem/test.sh functional oracle.
//
// Links against the SAME sanitized libyuv static library the fuzz harnesses use. It builds two
// fixed, deterministic frames (a gradient ARGB image and a gradient I420 image), runs them
// through libyuv::ARGBToI420 and libyuv::I420Scale, and prints an additive byte-checksum of each
// converted/scaled output buffer. mayhem/test.sh greps the exact printed numbers against
// precomputed golden values — this is a behavioral (known-input -> known-output) assertion, not
// an exit-code check: a no-op/neutered binary prints nothing and the grep fails.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "libyuv/convert_from_argb.h"
#include "libyuv/scale.h"

namespace {

uint32_t Checksum(const uint8_t* buf, size_t len) {
  uint32_t sum = 0;
  for (size_t i = 0; i < len; i++) {
    sum = sum * 131u + buf[i];
  }
  return sum;
}

// KAT 1: an 8x8 deterministic ARGB gradient -> I420 via libyuv::ARGBToI420.
void RunConvertKat() {
  const int w = 8, h = 8;
  std::vector<uint8_t> argb(static_cast<size_t>(w) * h * 4);
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      uint8_t* px = &argb[(static_cast<size_t>(y) * w + x) * 4];
      px[0] = static_cast<uint8_t>(x * 32);        // B
      px[1] = static_cast<uint8_t>(y * 32);        // G
      px[2] = static_cast<uint8_t>((x + y) * 16);  // R
      px[3] = 255;                                 // A
    }
  }
  int cw = (w + 1) / 2, ch = (h + 1) / 2;
  std::vector<uint8_t> dst_y(static_cast<size_t>(w) * h);
  std::vector<uint8_t> dst_u(static_cast<size_t>(cw) * ch);
  std::vector<uint8_t> dst_v(static_cast<size_t>(cw) * ch);

  int rc = libyuv::ARGBToI420(argb.data(), w * 4, dst_y.data(), w, dst_u.data(), cw, dst_v.data(),
                               cw, w, h);
  if (rc != 0) {
    fprintf(stderr, "ARGBToI420 KAT: unexpected return code %d\n", rc);
    return;
  }
  uint32_t cy = Checksum(dst_y.data(), dst_y.size());
  uint32_t cu = Checksum(dst_u.data(), dst_u.size());
  uint32_t cv = Checksum(dst_v.data(), dst_v.size());
  // Center pixel (x=4,y=4) of the Y plane — a single asserted computed byte, independent of the
  // whole-buffer checksum above.
  uint8_t center_y = dst_y[static_cast<size_t>(4) * w + 4];
  printf("KAT_CONVERT Y=%u U=%u V=%u CENTER_Y=%u\n", cy, cu, cv, center_y);
}

// KAT 2: an 8x8 deterministic I420 gradient scaled down to 4x4 via libyuv::I420Scale.
void RunScaleKat() {
  const int sw = 8, sh = 8, dw = 4, dh = 4;
  int scw = (sw + 1) / 2, sch = (sh + 1) / 2;
  std::vector<uint8_t> src_y(static_cast<size_t>(sw) * sh);
  std::vector<uint8_t> src_u(static_cast<size_t>(scw) * sch);
  std::vector<uint8_t> src_v(static_cast<size_t>(scw) * sch);
  for (int y = 0; y < sh; y++)
    for (int x = 0; x < sw; x++) src_y[static_cast<size_t>(y) * sw + x] = static_cast<uint8_t>((x * 16 + y * 8) & 0xFF);
  for (int y = 0; y < sch; y++)
    for (int x = 0; x < scw; x++) {
      src_u[static_cast<size_t>(y) * scw + x] = static_cast<uint8_t>(64 + x * 8);
      src_v[static_cast<size_t>(y) * scw + x] = static_cast<uint8_t>(192 - y * 8);
    }

  int dcw = (dw + 1) / 2, dch = (dh + 1) / 2;
  std::vector<uint8_t> dst_y(static_cast<size_t>(dw) * dh);
  std::vector<uint8_t> dst_u(static_cast<size_t>(dcw) * dch);
  std::vector<uint8_t> dst_v(static_cast<size_t>(dcw) * dch);

  int rc = libyuv::I420Scale(src_y.data(), sw, src_u.data(), scw, src_v.data(), scw, sw, sh,
                              dst_y.data(), dw, dst_u.data(), dcw, dst_v.data(), dcw, dw, dh,
                              libyuv::kFilterBox);
  if (rc != 0) {
    fprintf(stderr, "I420Scale KAT: unexpected return code %d\n", rc);
    return;
  }
  uint32_t cy = Checksum(dst_y.data(), dst_y.size());
  uint32_t cu = Checksum(dst_u.data(), dst_u.size());
  uint32_t cv = Checksum(dst_v.data(), dst_v.size());
  uint8_t center_y = dst_y[static_cast<size_t>(dh / 2) * dw + dw / 2];
  printf("KAT_SCALE Y=%u U=%u V=%u CENTER_Y=%u\n", cy, cu, cv, center_y);
}

}  // namespace

int main() {
  RunConvertKat();
  RunScaleKat();
  return 0;
}
