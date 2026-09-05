// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <utility>
#include <vector>

namespace DesktopDepthPreview {

inline constexpr double maximumOverlapRatio = 0.05;

struct Rect {
  double x;
  double y;
  double width;
  double height;
};

struct PlaneAssignment {
  std::vector<std::size_t> planes;
  std::size_t planeCount = 0;
};

inline double rectArea(const Rect &rect) {
  return std::max(0.0, rect.width) * std::max(0.0, rect.height);
}

inline Rect intersection(const Rect &left, const Rect &right) {
  const double x = std::max(left.x, right.x);
  const double y = std::max(left.y, right.y);
  const double rightEdge = std::min(left.x + left.width, right.x + right.width);
  const double bottomEdge =
      std::min(left.y + left.height, right.y + right.height);
  return {x, y, std::max(0.0, rightEdge - x), std::max(0.0, bottomEdge - y)};
}

inline double unionArea(const std::vector<Rect> &rects) {
  std::vector<double> xCoordinates;
  xCoordinates.reserve(rects.size() * 2);
  for (const Rect &rect : rects) {
    if (rectArea(rect) > 0.0) {
      xCoordinates.push_back(rect.x);
      xCoordinates.push_back(rect.x + rect.width);
    }
  }
  if (xCoordinates.empty()) {
    return 0.0;
  }

  std::sort(xCoordinates.begin(), xCoordinates.end());
  xCoordinates.erase(std::unique(xCoordinates.begin(), xCoordinates.end()),
                     xCoordinates.end());

  double area = 0.0;
  std::vector<std::pair<double, double>> yIntervals;
  yIntervals.reserve(rects.size());
  for (std::size_t index = 1; index < xCoordinates.size(); ++index) {
    const double left = xCoordinates[index - 1];
    const double right = xCoordinates[index];
    const double width = right - left;
    if (width <= 0.0) {
      continue;
    }

    yIntervals.clear();
    for (const Rect &rect : rects) {
      if (rect.x < right && rect.x + rect.width > left && rect.height > 0.0) {
        yIntervals.emplace_back(rect.y, rect.y + rect.height);
      }
    }
    if (yIntervals.empty()) {
      continue;
    }

    std::sort(yIntervals.begin(), yIntervals.end());
    double coveredHeight = 0.0;
    double start = yIntervals.front().first;
    double end = yIntervals.front().second;
    for (std::size_t interval = 1; interval < yIntervals.size(); ++interval) {
      if (yIntervals[interval].first <= end) {
        end = std::max(end, yIntervals[interval].second);
      } else {
        coveredHeight += end - start;
        start = yIntervals[interval].first;
        end = yIntervals[interval].second;
      }
    }
    coveredHeight += end - start;
    area += width * coveredHeight;
  }
  return area;
}

inline double visibleRatio(const Rect &window,
                           const std::vector<Rect> &windowsAbove) {
  const double windowArea = rectArea(window);
  if (windowArea <= 0.0) {
    return 0.0;
  }

  std::vector<Rect> occludedRegions;
  occludedRegions.reserve(windowsAbove.size());
  for (const Rect &windowAbove : windowsAbove) {
    const Rect overlap = intersection(window, windowAbove);
    if (rectArea(overlap) > 0.0) {
      occludedRegions.push_back(overlap);
    }
  }

  const double occludedArea = unionArea(occludedRegions);
  return std::clamp(1.0 - occludedArea / windowArea, 0.0, 1.0);
}

// Windows arrive from topmost to bottommost. Peel off every window that is
// sufficiently visible, then repeat against the windows remaining underneath.
inline PlaneAssignment
assignPlanes(const std::vector<Rect> &windows,
             double maximumOccludedRatio = maximumOverlapRatio) {
  PlaneAssignment result;
  result.planes.resize(windows.size());

  std::vector<std::size_t> remaining(windows.size());
  std::iota(remaining.begin(), remaining.end(), 0);
  std::size_t plane = 0;
  while (!remaining.empty()) {
    std::vector<bool> selected(windows.size(), false);
    std::vector<Rect> windowsAbove;
    windowsAbove.reserve(remaining.size());
    std::size_t selectedCount = 0;

    for (const std::size_t windowIndex : remaining) {
      const double occludedRatio =
          1.0 - visibleRatio(windows[windowIndex], windowsAbove);
      if (occludedRatio <= maximumOccludedRatio) {
        selected[windowIndex] = true;
        result.planes[windowIndex] = plane;
        ++selectedCount;
      }
      windowsAbove.push_back(windows[windowIndex]);
    }

    // Production windows have valid geometry, but always make progress if a
    // malformed rectangle reaches this pure helper.
    if (selectedCount == 0) {
      selected[remaining.front()] = true;
      result.planes[remaining.front()] = plane;
    }

    std::vector<std::size_t> nextRemaining;
    nextRemaining.reserve(remaining.size() -
                          std::max<std::size_t>(1, selectedCount));
    for (const std::size_t windowIndex : remaining) {
      if (!selected[windowIndex]) {
        nextRemaining.push_back(windowIndex);
      }
    }
    remaining = std::move(nextRemaining);
    ++plane;
  }

  result.planeCount = plane;
  return result;
}

} // namespace DesktopDepthPreview
