// libyuv_convert_fuzzer.cc — libFuzzer harness for libyuv's ARGBToI420() colorspace
// conversion path (source/convert_from_argb.cc, planar_functions.cc, row_*.cc).
//
// libyuv ships no OSS-Fuzz harness upstream (project.yaml has no build.sh) — this is a
// net-new harness written for the mayhemheroes integration. Input layout (all little-endian):
//   [0]   u16 width   (clamped to [1, kMaxDim])
//   [2]   u16 height  (clamped to [1, kMaxDim])
//   [4..] width*height*4 bytes of packed ARGB source (stride == width*4)
// Anything short of that is rejected before the library is touched.
//
// LLVMFuzzerTestOneInput never crashes/aborts itself: it discards malformed/incomplete input by
// returning early. Any crash found here is inside libyuv's converter.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "libyuv/convert_from_argb.h"

namespace {
constexpr int kMaxDim = 512;

uint16_t ReadU16LE(const uint8_t* p) {
  return static_cast<uint16_t>(p[0] | (p[1] << 8));
}
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 4) return 0;

  int width = ReadU16LE(data);
  int height = ReadU16LE(data + 2);
  if (width < 1 || height < 1 || width > kMaxDim || height > kMaxDim) return 0;

  const uint8_t* argb_src = data + 4;
  size_t remaining = size - 4;
  size_t argb_stride = static_cast<size_t>(width) * 4;
  size_t argb_needed = argb_stride * static_cast<size_t>(height);
  if (remaining < argb_needed) return 0;

  int chroma_w = (width + 1) / 2;
  int chroma_h = (height + 1) / 2;

  uint8_t* dst_y = static_cast<uint8_t*>(malloc(static_cast<size_t>(width) * height));
  uint8_t* dst_u = static_cast<uint8_t*>(malloc(static_cast<size_t>(chroma_w) * chroma_h));
  uint8_t* dst_v = static_cast<uint8_t*>(malloc(static_cast<size_t>(chroma_w) * chroma_h));
  if (dst_y && dst_u && dst_v) {
    libyuv::ARGBToI420(argb_src, static_cast<int>(argb_stride), dst_y, width, dst_u, chroma_w,
                        dst_v, chroma_w, width, height);
  }
  free(dst_y);
  free(dst_u);
  free(dst_v);
  return 0;
}
