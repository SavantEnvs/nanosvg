// Known-answer functional checks for nanosvg (upstream ships no test suite). Each check parses an
// SVG with nsvgParse and asserts parsed values (sizes, colours, path points, gradients), or
// rasterizes with nsvgRasterize and asserts pixel values. Prints "PASS <name>"/"FAIL <name>" lines
// that mayhem/test.sh counts.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"

static int g_fail = 0;
#define CHECK(name, cond) do { if (cond) printf("PASS %s\n", name); \
    else { printf("FAIL %s\n", name); g_fail++; } } while (0)

static NSVGimage *parse(const char *s) {
  char *b = strdup(s);
  NSVGimage *img = nsvgParse(b, "px", 96.0f);
  free(b);
  return img;
}
static int nshapes(NSVGimage *img) { int n = 0; for (NSVGshape *s = img->shapes; s; s = s->next) n++; return n; }
static int feq(float a, float b) { return fabsf(a - b) < 1e-3f; }
// nanosvg colours are 0xAABBGGRR
#define RGBA(r,g,b,a) ((unsigned)(r) | ((unsigned)(g) << 8) | ((unsigned)(b) << 16) | ((unsigned)(a) << 24))

int main(int argc, char **argv) {
  NSVGimage *img;

  img = parse("<svg width=\"100\" height=\"50\"><rect x=\"10\" y=\"20\" width=\"30\" height=\"5\" fill=\"#ff0000\"/></svg>");
  CHECK("size_width", img && feq(img->width, 100));
  CHECK("size_height", img && feq(img->height, 50));
  CHECK("rect_one_shape", img && nshapes(img) == 1);
  CHECK("rect_fill_hex", img && img->shapes && img->shapes->fill.type == NSVG_PAINT_COLOR && img->shapes->fill.color == RGBA(255,0,0,255));
  CHECK("rect_bounds", img && img->shapes && feq(img->shapes->bounds[0], 10) && feq(img->shapes->bounds[1], 20) && feq(img->shapes->bounds[2], 40) && feq(img->shapes->bounds[3], 25));
  CHECK("rect_no_stroke", img && img->shapes && img->shapes->stroke.type == NSVG_PAINT_NONE);
  nsvgDelete(img);

  img = parse("<svg width=\"10\" height=\"10\"><rect width=\"1\" height=\"1\" fill=\"rgb(10,20,30)\"/>"
              "<rect width=\"1\" height=\"1\" fill=\"rgb(100%,0%,50%)\"/>"
              "<rect width=\"1\" height=\"1\" fill=\"#abc\"/>"
              "<rect width=\"1\" height=\"1\" fill=\"blue\" fill-opacity=\"0.5\"/>"
              "<rect width=\"1\" height=\"1\" style=\"fill:none;stroke:#00ff00;stroke-width:3\"/></svg>");
  NSVGshape *s = img ? img->shapes : NULL;
  CHECK("colors_count", img && nshapes(img) == 5);
  CHECK("color_rgb_int", s && s->fill.color == RGBA(10,20,30,255));
  s = s ? s->next : NULL;
  CHECK("color_rgb_percent", s && (s->fill.color == RGBA(255,0,127,255) || s->fill.color == RGBA(255,0,128,255)));
  s = s ? s->next : NULL;
  CHECK("color_hex3", s && s->fill.color == RGBA(0xaa,0xbb,0xcc,255));
  s = s ? s->next : NULL;
  CHECK("color_named_opacity", s && s->fill.type == NSVG_PAINT_COLOR && (s->fill.color & 0xffffff) == RGBA(0,0,255,0) && ((s->fill.color >> 24) == 127 || (s->fill.color >> 24) == 128));
  s = s ? s->next : NULL;
  CHECK("style_stroke", s && s->fill.type == NSVG_PAINT_NONE && s->stroke.type == NSVG_PAINT_COLOR && s->stroke.color == RGBA(0,255,0,255) && feq(s->strokeWidth, 3));
  nsvgDelete(img);

  img = parse("<svg width=\"100\" height=\"100\"><path d=\"M10 10 L 90 10 L 90 90 Z\" fill=\"black\" transform=\"translate(5,0)\"/></svg>");
  s = img ? img->shapes : NULL;
  CHECK("path_closed", s && s->paths && s->paths->closed == 1);
  CHECK("path_points", s && s->paths && s->paths->npts == 10 && feq(s->paths->pts[0], 15) && feq(s->paths->pts[1], 10)
                       && feq(s->paths->pts[3*2], 95) && feq(s->paths->pts[3*2+1], 10));
  CHECK("path_bounds", s && feq(s->bounds[0], 15) && feq(s->bounds[2], 95) && feq(s->bounds[3], 90));
  nsvgDelete(img);

  img = parse("<svg width=\"1in\" height=\"25.4mm\"><circle cx=\"50\" cy=\"50\" r=\"10\" fill=\"red\"/></svg>");
  CHECK("units_in_mm", img && feq(img->width, 96) && feq(img->height, 96));
  s = img ? img->shapes : NULL;
  CHECK("circle_bounds", s && fabsf(s->bounds[0] - 40) < 0.1f && fabsf(s->bounds[2] - 60) < 0.1f);
  nsvgDelete(img);

  img = parse("<svg width=\"10\" height=\"10\"><defs><linearGradient id=\"g\"><stop offset=\"0\" stop-color=\"#f00\"/>"
              "<stop offset=\"1\" stop-color=\"#00f\"/></linearGradient></defs><rect width=\"10\" height=\"10\" fill=\"url(#g)\"/></svg>");
  s = img ? img->shapes : NULL;
  CHECK("gradient_linear", s && s->fill.type == NSVG_PAINT_LINEAR_GRADIENT && s->fill.gradient->nstops == 2
        && s->fill.gradient->stops[0].color == RGBA(255,0,0,255) && s->fill.gradient->stops[1].color == RGBA(0,0,255,255)
        && feq(s->fill.gradient->stops[1].offset, 1));
  // rasterize the gradient: left edge red-ish, right edge blue-ish
  {
    NSVGrasterizer *r = nsvgCreateRasterizer();
    unsigned char px[10 * 10 * 4];
    memset(px, 0, sizeof px);
    nsvgRasterize(r, img, 0, 0, 1, px, 10, 10, 40);
    unsigned char *L = px + (5 * 10 + 0) * 4, *R = px + (5 * 10 + 9) * 4;
    CHECK("raster_gradient", L[0] > 200 && L[2] < 40 && R[2] > 200 && R[0] < 40 && L[3] == 255 && R[3] == 255);
    nsvgDeleteRasterizer(r);
  }
  nsvgDelete(img);

  img = parse("<svg width=\"8\" height=\"8\"><rect x=\"0\" y=\"0\" width=\"4\" height=\"8\" fill=\"#00ff00\"/></svg>");
  {
    NSVGrasterizer *r = nsvgCreateRasterizer();
    unsigned char px[8 * 8 * 4];
    memset(px, 0, sizeof px);
    nsvgRasterize(r, img, 0, 0, 1, px, 8, 8, 32);
    unsigned char *in = px + (3 * 8 + 1) * 4, *out = px + (3 * 8 + 6) * 4;
    CHECK("raster_fill_inside", in[0] == 0 && in[1] == 255 && in[2] == 0 && in[3] == 255);
    CHECK("raster_fill_outside", out[3] == 0);
    memset(px, 0, sizeof px);
    nsvgRasterize(r, img, 0, 0, 2, px, 8, 8, 32);   // scaled x2: rect covers the full width
    out = px + (3 * 8 + 6) * 4;
    CHECK("raster_scale", out[1] == 255 && out[3] == 255);
    nsvgDeleteRasterizer(r);
  }
  nsvgDelete(img);

  // Upstream example files (golden shape counts / sizes)
  for (int i = 1; i + 2 < argc; i += 3) {
    NSVGimage *f = nsvgParseFromFile(argv[i], "px", 96.0f);
    char name[256];
    snprintf(name, sizeof name, "example_%s", strrchr(argv[i], '/') ? strrchr(argv[i], '/') + 1 : argv[i]);
    if (!f) { CHECK(name, 0); continue; }
    int n = nshapes(f);
    int ok = n == atoi(argv[i + 1]) && (int)f->width == atoi(argv[i + 2]);
    if (!ok) printf("  got shapes=%d width=%d\n", n, (int)f->width);
    CHECK(name, ok);
    nsvgDelete(f);
  }
  return g_fail ? 1 : 0;
}
