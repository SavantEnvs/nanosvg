// libFuzzer harness for nanosvg: parse an SVG document (nanosvg.h: XML, CSS style, attributes,
// paths, colours, gradients, units, transforms), duplicate every path, then rasterize the image
// (nanosvgrast.h: flatten, stroke, scanline fill, gradient paint) into a fixed 64x64 RGBA buffer.
// No state survives an input: every object is created and freed per call.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"

#define W 64
#define H 64

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // nsvgParse takes a NUL-terminated, mutable string (it modifies the buffer in place).
  char *buf = (char *)malloc(size + 1);
  if (!buf) return 0;
  memcpy(buf, data, size);
  buf[size] = '\0';
  NSVGimage *img = nsvgParse(buf, "px", 96.0f);
  free(buf);
  if (!img) return 0;

  for (NSVGshape *s = img->shapes; s; s = s->next)
    for (NSVGpath *p = s->paths; p; p = p->next) {
      NSVGpath *d = nsvgDuplicatePath(p);
      if (d) { free(d->pts); free(d); }
    }

  NSVGrasterizer *r = nsvgCreateRasterizer();
  if (r) {
    static unsigned char px[W * H * 4];
    // The scale is the application's choice (fit the image into the output); the image size is
    // attacker-controlled, so only use it when sane.
    float sc = (img->width >= 1.0f && img->width < 1e6f) ? (float)W / img->width : 1.0f;
    nsvgRasterize(r, img, 0, 0, sc, px, W, H, W * 4);
    nsvgDeleteRasterizer(r);
  }
  nsvgDelete(img);
  return 0;
}
