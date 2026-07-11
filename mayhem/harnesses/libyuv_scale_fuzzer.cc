// libyuv_scale_fuzzer.cc — libFuzzer harness for libyuv's I420Scale() planar scaler
// (source/scale.cc, scale_common.cc, scale_any.cc, row_*.cc).
//
// libyuv ships no OSS-Fuzz harness upstream (project.yaml has no build.sh) — this is a
// net-new harness written for the mayhemheroes integration. Input layout (all little-endian):
//   [0]  u16 src_width   (clamped to [1, kMaxDim])
//   [2]  u16 src_height  (clamped to [1, kMaxDim])
//   [4]  u16 dst_width   (clamped to [1, kMaxDim])
//   [6]  u16 dst_height  (clamped to [1, kMaxDim])
//   [8]  u8  filter mode selector (mod 4 -> FilterMode enum)
//   [9..] I420 source planes: Y (src_w*src_h), U, V (each ceil(src_w/2)*ceil(src_h/2))
// Anything short of that is rejected before the library is touched.
//
// LLVMFuzzerTestOneInput never crashes/aborts itself: it discards malformed/incomplete input by
// returning early. Any crash found here is inside libyuv's scaler.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "libyuv/scale.h"

namespace {
constexpr int kMaxDim = 512;

uint16_t ReadU16LE(const uint8_t* p) {
  return static_cast<uint16_t>(p[0] | (p[1] << 8));
}
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 9) return 0;

  int src_w = ReadU16LE(data);
  int src_h = ReadU16LE(data + 2);
  int dst_w = ReadU16LE(data + 4);
  int dst_h = ReadU16LE(data + 6);
  if (src_w < 1 || src_h < 1 || src_w > kMaxDim || src_h > kMaxDim) return 0;
  if (dst_w < 1 || dst_h < 1 || dst_w > kMaxDim || dst_h > kMaxDim) return 0;

  static const libyuv::FilterMode kFilters[4] = {
      libyuv::kFilterNone, libyuv::kFilterLinear, libyuv::kFilterBilinear, libyuv::kFilterBox};
  libyuv::FilterMode filter = kFilters[data[8] % 4];

  const uint8_t* src = data + 9;
  size_t remaining = size - 9;

  int src_chroma_w = (src_w + 1) / 2;
  int src_chroma_h = (src_h + 1) / 2;
  size_t y_needed = static_cast<size_t>(src_w) * src_h;
  size_t chroma_needed = static_cast<size_t>(src_chroma_w) * src_chroma_h;
  size_t total_needed = y_needed + 2 * chroma_needed;
  if (remaining < total_needed) return 0;

  const uint8_t* src_y = src;
  const uint8_t* src_u = src_y + y_needed;
  const uint8_t* src_v = src_u + chroma_needed;

  int dst_chroma_w = (dst_w + 1) / 2;
  int dst_chroma_h = (dst_h + 1) / 2;
  uint8_t* dst_y = static_cast<uint8_t*>(malloc(static_cast<size_t>(dst_w) * dst_h));
  uint8_t* dst_u = static_cast<uint8_t*>(malloc(static_cast<size_t>(dst_chroma_w) * dst_chroma_h));
  uint8_t* dst_v = static_cast<uint8_t*>(malloc(static_cast<size_t>(dst_chroma_w) * dst_chroma_h));
  if (dst_y && dst_u && dst_v) {
    libyuv::I420Scale(src_y, src_w, src_u, src_chroma_w, src_v, src_chroma_w, src_w, src_h, dst_y,
                       dst_w, dst_u, dst_chroma_w, dst_v, dst_chroma_w, dst_w, dst_h, filter);
  }
  free(dst_y);
  free(dst_u);
  free(dst_v);
  return 0;
}
