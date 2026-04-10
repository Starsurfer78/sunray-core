#include "core/navigation/StripingAlgorithm.h"

#include <clipper2/clipper.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
    // ── Rotation helpers ──────────────────────────────────────────────────────

    using sunray::nav::Point;
    using sunray::nav::PolygonPoints;

    static Point rotatePoint(const Point &p, float cosA, float sinA)
    {
        return {p.x * cosA - p.y * sinA, p.x * sinA + p.y * cosA};
    }

    static PolygonPoints rotatePolygon(const PolygonPoints &poly, float angleRad)
    {
        const float c = std::cos(angleRad);
        const float s = std::sin(angleRad);
        PolygonPoints out;
        out.reserve(poly.size());
        for (const auto &p : poly)
            out.push_back(rotatePoint(p, c, s));
        return out;
    }

    // ── Scanline interval helpers ─────────────────────────────────────────────

    struct Interval
    {
        float left = 0.0f;
        float right = 0.0f;
    };

    static std::vector<Interval> normalizeIntervals(std::vector<Interval> v)
    {
        if (v.empty())
            return v;
        std::sort(v.begin(), v.end(), [](const Interval &a, const Interval &b)
                  { return a.left == b.left ? a.right < b.right : a.left < b.left; });
        std::vector<Interval> out = {v.front()};
        for (size_t i = 1; i < v.size(); ++i)
        {
            if (v[i].left <= out.back().right + 1e-6f)
                out.back().right = std::max(out.back().right, v[i].right);
            else
                out.push_back(v[i]);
        }
        return out;
    }

    static std::vector<Interval> polygonIntervalsAtY(const PolygonPoints &poly, float y)
    {
        if (poly.size() < 3)
            return {};
        std::vector<float> xs;
        xs.reserve(poly.size());
        for (size_t i = 0; i < poly.size(); ++i)
        {
            const Point &a = poly[i];
            const Point &b = poly[(i + 1) % poly.size()];
            if ((a.y < y && b.y >= y) || (b.y < y && a.y >= y))
            {
                const float t = (y - a.y) / (b.y - a.y);
                xs.push_back(a.x + t * (b.x - a.x));
            }
        }
        std::sort(xs.begin(), xs.end());
        std::vector<Interval> out;
        for (size_t i = 0; i + 1 < xs.size(); i += 2)
            if (xs[i + 1] > xs[i])
                out.push_back({xs[i], xs[i + 1]});
        return out;
    }

    static std::vector<Interval> intersectIntervals(const std::vector<Interval> &a,
                                                    const std::vector<Interval> &b)
    {
        std::vector<Interval> out;
        size_t i = 0, j = 0;
        while (i < a.size() && j < b.size())
        {
            const float left = std::max(a[i].left, b[j].left);
            const float right = std::min(a[i].right, b[j].right);
            if (left <= right)
                out.push_back({left, right});
            if (a[i].right < b[j].right)
                ++i;
            else
                ++j;
        }
        return out;
    }

    static std::vector<Interval> subtractIntervals(const std::vector<Interval> &base,
                                                   const std::vector<Interval> &cuts)
    {
        if (base.empty())
            return {};
        if (cuts.empty())
            return base;

        std::vector<Interval> out;
        size_t j = 0;
        for (const auto &b : base)
        {
            float cursor = b.left;
            while (j < cuts.size() && cuts[j].right < cursor)
                ++j;
            size_t k = j;
            while (k < cuts.size() && cuts[k].left <= b.right)
            {
                if (cuts[k].left > cursor)
                    out.push_back({cursor, std::min(b.right, cuts[k].left)});
                cursor = std::max(cursor, cuts[k].right);
                if (cursor >= b.right)
                    break;
                ++k;
            }
            if (cursor < b.right)
                out.push_back({cursor, b.right});
        }
        return normalizeIntervals(out);
    }

    /// Compute x-intervals covered by the zone at scanline y,
    /// intersected with perimeter (if non-empty), minus exclusions.
    static std::vector<Interval> effectiveIntervalsAtY(
        const PolygonPoints &zone,
        const PolygonPoints &perimeter,
        const std::vector<PolygonPoints> &exclusions,
        float y)
    {
        std::vector<Interval> intervals = polygonIntervalsAtY(zone, y);
        if (!perimeter.empty())
            intervals = intersectIntervals(intervals, polygonIntervalsAtY(perimeter, y));
        for (const auto &excl : exclusions)
        {
            intervals = subtractIntervals(intervals, polygonIntervalsAtY(excl, y));
            if (intervals.empty())
                break;
        }
        return intervals;
    }

    // ── Clipper2 exclusion-padding helper ─────────────────────────────────────

    /// Expand a polygon outward by |inset| using round joins (Minkowski-sum
    /// semantics matching the A* costmap circular inflation).
    /// Returns the expanded polygon(s); falls back to the original on failure.
    static std::vector<PolygonPoints> offsetPolygonRounded(const PolygonPoints &poly, float inset)
    {
        if (poly.size() < 3)
            return {};
        if (std::fabs(inset) < 1e-6f)
            return {poly};

        Clipper2Lib::PathsD paths = {
            [&]
            {
                Clipper2Lib::PathD p;
                p.reserve(poly.size());
                for (const auto &pt : poly)
                    p.emplace_back(pt.x, pt.y);
                return p;
            }()};

        const Clipper2Lib::PathsD result = Clipper2Lib::InflatePaths(
            paths,
            -static_cast<double>(inset), // negative delta = expand outward
            Clipper2Lib::JoinType::Round,
            Clipper2Lib::EndType::Polygon);

        std::vector<PolygonPoints> out;
        for (const auto &path : result)
        {
            if (Clipper2Lib::Area(path) <= 0.0)
                continue;
            PolygonPoints expanded;
            expanded.reserve(path.size());
            for (const auto &pt : path)
                expanded.push_back({static_cast<float>(pt.x), static_cast<float>(pt.y)});
            if (expanded.size() >= 3)
                out.push_back(std::move(expanded));
        }
        return out;
    }

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
namespace sunray
{
    namespace nav
    {

        std::vector<Segment> buildStripeSegments(
            const PolygonPoints &zone,
            const PolygonPoints &perimeter,
            const std::vector<PolygonPoints> &exclusions,
            float stripWidth,
            float angleDeg,
            float exclusionPadding)
        {
            if (zone.size() < 3 || stripWidth <= 0.0f)
                return {};

            const float angle = angleDeg * static_cast<float>(M_PI) / 180.0f;
            const float cosPos = std::cos(angle);
            const float sinPos = std::sin(angle);

            const PolygonPoints rotZone = rotatePolygon(zone, -angle);
            const PolygonPoints rotPerimeter = perimeter.empty()
                                                   ? PolygonPoints{}
                                                   : rotatePolygon(perimeter, -angle);

            // Optionally expand exclusions in the rotated frame so stripe endpoints
            // stay outside the A* obstacle-inflation radius. Round joins match the
            // Minkowski-sum semantics of the A* costmap.
            std::vector<PolygonPoints> rotExclusions;
            rotExclusions.reserve(exclusions.size());
            for (const auto &ex : exclusions)
            {
                const PolygonPoints rotEx = rotatePolygon(ex, -angle);
                if (exclusionPadding > 0.0f)
                {
                    const auto expanded = offsetPolygonRounded(rotEx, -exclusionPadding);
                    if (!expanded.empty())
                        for (const auto &p : expanded)
                            rotExclusions.push_back(p);
                    else
                        rotExclusions.push_back(rotEx);
                }
                else
                {
                    rotExclusions.push_back(rotEx);
                }
            }

            // Determine Y extent from all rotated geometry.
            std::vector<Point> allPts = rotZone;
            allPts.insert(allPts.end(), rotPerimeter.begin(), rotPerimeter.end());
            for (const auto &ex : rotExclusions)
                allPts.insert(allPts.end(), ex.begin(), ex.end());
            if (allPts.empty())
                return {};

            float minY = allPts.front().y;
            float maxY = allPts.front().y;
            for (const auto &p : allPts)
            {
                minY = std::min(minY, p.y);
                maxY = std::max(maxY, p.y);
            }

            // Build segments in strict row-by-row boustrophedon (zigzag) order.
            // Do NOT reorder by nearest-neighbour: global reordering destroys the row
            // sequence, forces long diagonal transitions and may miss stripes entirely
            // on complex zone shapes.
            std::vector<Segment> segments;
            int rowIdx = 0;
            for (float y = minY + stripWidth * 0.5f;
                 y <= maxY + stripWidth * 0.01f;
                 y += stripWidth, ++rowIdx)
            {
                std::vector<Interval> intervals =
                    effectiveIntervalsAtY(rotZone, rotPerimeter, rotExclusions, y);

                // Boustrophedon: reverse direction on odd rows.
                if (rowIdx % 2 != 0)
                    std::reverse(intervals.begin(), intervals.end());

                for (const auto &iv : intervals)
                {
                    const Point a = rotatePoint({iv.left, y}, cosPos, sinPos);
                    const Point b = rotatePoint({iv.right, y}, cosPos, sinPos);
                    segments.push_back(rowIdx % 2 == 0 ? Segment{a, b} : Segment{b, a});
                }
            }

            return segments;
        }

    } // namespace nav
} // namespace sunray
