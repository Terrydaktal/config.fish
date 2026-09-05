// SPDX-License-Identifier: GPL-2.0-or-later
#include "depthplanes.h"

#include <cstdio>

using DesktopDepthPreview::Rect;

namespace {
int failures = 0;

void check(bool condition, const char *message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
  }
}
} // namespace

int main() {
  {
    const auto assignment = DesktopDepthPreview::assignPlanes({
        {0, 0, 100, 100},
        {110, 0, 100, 100},
    });
    check(assignment.planeCount == 1,
          "non-overlapping windows share one plane");
    check(assignment.planes == std::vector<std::size_t>{0, 0},
          "non-overlapping plane indices");
  }

  {
    const auto assignment = DesktopDepthPreview::assignPlanes({
        {0, 0, 100, 100},
        {0, 0, 100, 100},
        {110, 0, 100, 100},
    });
    check(assignment.planeCount == 2,
          "fully overlapping windows create another plane");
    check(assignment.planes == std::vector<std::size_t>{0, 1, 0},
          "deeper non-overlapping window reuses the first plane");
  }

  {
    const auto insignificant = DesktopDepthPreview::assignPlanes({
        {0, 0, 100, 100},
        {96, 0, 100, 100},
    });
    check(insignificant.planes == std::vector<std::size_t>{0, 0},
          "four-percent overlap stays in one plane");

    const auto significant = DesktopDepthPreview::assignPlanes({
        {0, 0, 100, 100},
        {90, 0, 100, 100},
    });
    check(significant.planes == std::vector<std::size_t>{0, 1},
          "ten-percent overlap creates another plane");
  }

  {
    const auto assignment = DesktopDepthPreview::assignPlanes({
        {0, 0, 100, 100},
        {80, 0, 100, 100},
        {160, 0, 100, 100},
    });
    check(assignment.planes == std::vector<std::size_t>{0, 1, 2},
          "chained occlusion peels windows in front-to-back order");
  }

  {
    const auto assignment = DesktopDepthPreview::assignPlanes({
        {-97, 0, 100, 100},
        {97, 0, 100, 100},
        {0, 0, 100, 100},
    });
    check(assignment.planes == std::vector<std::size_t>{0, 0, 1},
          "combined occlusion is measured as a union");
  }

  {
    const auto assignment = DesktopDepthPreview::assignPlanes({
        {0, 0, 4, 100},
        {0, 0, 100, 100},
    });
    check(assignment.planes == std::vector<std::size_t>{0, 0},
          "visibility uses the obscured window area as its denominator");
  }

  return failures == 0 ? 0 : 1;
}
