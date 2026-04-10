/// route_visualizer.cpp
/// Standalone tool: creates a test map with 2 zones + 1 hard exclusion,
/// runs the real MowRoutePlanner, writes an SVG to stdout.
///
/// Build (from repo root):
///   g++ -std=c++17 -O2 \
///     -I. -Icore \
///     -I build/_deps/nlohmann_json-src/include \
///     -I build/_deps/clipper2-src/CPP/Clipper2Lib/include \
///     tools/route_visualizer.cpp \
///     build/core/libsunray_core.a \
///     build/_deps/clipper2-build/libClipper2.a \
///     -o build/route_visualizer

#include "core/navigation/Map.h"
#include "core/navigation/MowRoutePlanner.h"
#include "core/navigation/RouteValidator.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace sunray;
using namespace sunray::nav;

// ── SVG helpers ───────────────────────────────────────────────────────────────

struct SvgCanvas
{
    float minX, minY, maxX, maxY;
    float scale;
    float svgW, svgH;
    static constexpr float MARGIN = 40.0f;

    SvgCanvas(float minX, float minY, float maxX, float maxY, float pxPerMetre = 40.0f)
        : minX(minX), minY(minY), maxX(maxX), maxY(maxY), scale(pxPerMetre)
    {
        svgW = (maxX - minX) * scale + 2 * MARGIN;
        svgH = (maxY - minY) * scale + 2 * MARGIN;
    }

    float tx(float x) const { return (x - minX) * scale + MARGIN; }
    float ty(float y) const { return svgH - MARGIN - (y - minY) * scale; }

    std::string polygon(const PolygonPoints& pts, const std::string& fill,
                         const std::string& stroke, float sw, float opacity = 1.0f) const
    {
        std::ostringstream ss;
        ss << "<polygon points=\"";
        for (const auto& p : pts)
            ss << tx(p.x) << "," << ty(p.y) << " ";
        ss << "\" fill=\"" << fill << "\" fill-opacity=\"" << opacity
           << "\" stroke=\"" << stroke << "\" stroke-width=\"" << sw << "\"/>\n";
        return ss.str();
    }

    std::string polyline(const std::vector<RoutePoint>& pts,
                          const std::string& stroke, float sw) const
    {
        if (pts.empty()) return {};
        std::ostringstream ss;
        ss << "<polyline points=\"";
        for (const auto& rp : pts)
            ss << tx(rp.p.x) << "," << ty(rp.p.y) << " ";
        ss << "\" fill=\"none\" stroke=\"" << stroke
           << "\" stroke-width=\"" << sw << "\" stroke-linejoin=\"round\"/>\n";
        return ss.str();
    }

    std::string circle(float x, float y, float r,
                        const std::string& fill, const std::string& stroke = "none") const
    {
        std::ostringstream ss;
        ss << "<circle cx=\"" << tx(x) << "\" cy=\"" << ty(y) << "\" r=\"" << r
           << "\" fill=\"" << fill << "\" stroke=\"" << stroke << "\"/>\n";
        return ss.str();
    }

    std::string text(float x, float y, const std::string& s,
                      const std::string& fill = "#333", float fontSize = 12.0f) const
    {
        std::ostringstream ss;
        ss << "<text x=\"" << tx(x) << "\" y=\"" << ty(y)
           << "\" font-size=\"" << fontSize << "\" fill=\"" << fill
           << "\" font-family=\"monospace\">" << s << "</text>\n";
        return ss.str();
    }
};

// ── Colour coding per RouteSemantic ───────────────────────────────────────────

static std::string semanticColor(RouteSemantic sem)
{
    switch (sem)
    {
    case RouteSemantic::COVERAGE_EDGE:              return "#2196F3"; // blue
    case RouteSemantic::COVERAGE_INFILL:            return "#4CAF50"; // green
    case RouteSemantic::TRANSIT_WITHIN_ZONE:        return "#FF9800"; // orange
    case RouteSemantic::TRANSIT_BETWEEN_COMPONENTS: return "#E91E63"; // pink/red
    case RouteSemantic::TRANSIT_INTER_ZONE:         return "#9C27B0"; // purple
    case RouteSemantic::DOCK_APPROACH:              return "#795548"; // brown
    case RouteSemantic::RECOVERY:                   return "#F44336"; // red
    default:                                         return "#9E9E9E"; // grey
    }
}

static const char* semanticName(RouteSemantic sem)
{
    switch (sem)
    {
    case RouteSemantic::COVERAGE_EDGE:              return "Coverage Edge";
    case RouteSemantic::COVERAGE_INFILL:            return "Coverage Infill";
    case RouteSemantic::TRANSIT_WITHIN_ZONE:        return "Transit (within zone)";
    case RouteSemantic::TRANSIT_BETWEEN_COMPONENTS: return "Transit (between components)";
    case RouteSemantic::TRANSIT_INTER_ZONE:         return "Transit (inter-zone)";
    case RouteSemantic::DOCK_APPROACH:              return "Dock Approach";
    case RouteSemantic::RECOVERY:                   return "Recovery";
    default:                                         return "Unknown";
    }
}

// ── Map definition ────────────────────────────────────────────────────────────

static nlohmann::json buildMapJson()
{
    // Perimeter: 22 x 16 m rectangle
    nlohmann::json map;
    map["perimeter"] = {{0,0},{22,0},{22,16},{0,16}};

    // Dock on bottom-left
    map["dock"] = {{0.5f, 0.5f}, {1.5f, 0.5f}};

    // Zone 1: left slab  (1,1) → (10,15)
    nlohmann::json z1;
    z1["id"]      = "z1";
    z1["name"]    = "Zone 1 (left)";
    z1["order"]   = 1;
    z1["polygon"] = {{1,1},{10,1},{10,15},{1,15}};

    // Zone 2: right slab  (12,1) → (21,15)
    nlohmann::json z2;
    z2["id"]      = "z2";
    z2["name"]    = "Zone 2 (right)";
    z2["order"]   = 2;
    z2["polygon"] = {{12,1},{21,1},{21,15},{12,15}};

    map["zones"] = {z1, z2};

    // Hard exclusion (NoGo) inside Zone 1: a 2.5 x 3 m obstacle
    map["exclusions"]    = {{{3.5f,5},{7,5},{7,9},{3.5f,9}}};
    map["exclusionMeta"] = {{{"type","hard"},{"clearance",0.3f},{"costScale",1.0f}}};

    return map;
}

static nlohmann::json buildMissionJson()
{
    // Both zones, some per-zone overrides
    nlohmann::json mission;
    mission["id"]      = "viz_mission";
    mission["zoneIds"] = {"z1", "z2"};
    mission["overrides"] = {
        {"z1", {{"stripWidth", 0.5f}, {"angle", 0.0f},  {"edgeMowing", true}, {"edgeRounds", 1}}},
        {"z2", {{"stripWidth", 0.5f}, {"angle", 45.0f}, {"edgeMowing", true}, {"edgeRounds", 1}}}
    };
    return mission;
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main(int argc, char** argv)
{
    const std::string outPath = (argc > 1) ? argv[1] : "route_preview.svg";

    // 1. Write map JSON to a temp file and load it
    const auto tmpMap = std::filesystem::temp_directory_path() / "route_viz_map.json";
    {
        std::ofstream f(tmpMap);
        f << buildMapJson().dump(2);
    }

    Map map;
    if (!map.load(tmpMap))
    {
        std::cerr << "ERROR: failed to load map\n";
        return 1;
    }
    std::filesystem::remove(tmpMap);

    // 2. Plan the mission
    const nlohmann::json missionJson = buildMissionJson();
    const MissionPlan plan = buildMissionPlanPreview(map, missionJson);

    // 3. Validate
    const RouteValidationResult vr = RouteValidator::validate(map, plan.route);
    std::cerr << "Route points  : " << plan.route.points.size() << "\n";
    std::cerr << "Route valid   : " << (plan.route.valid ? "YES" : "NO") << "\n";
    if (!plan.route.invalidReason.empty())
        std::cerr << "Invalid reason: " << plan.route.invalidReason << "\n";
    std::cerr << "Validator     : " << (vr.valid ? "PASS" : "FAIL") << "\n";
    for (const auto& err : vr.errors)
        std::cerr << "  [" << err.pointIndex << "] " << err.message << "\n";

    // 4. Render SVG
    const float PAD = 0.5f;
    SvgCanvas canvas(-PAD, -PAD, 22.0f + PAD, 16.0f + PAD, 38.0f);

    std::ostringstream svg;
    svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\""
        << " width=\"" << canvas.svgW << "\""
        << " height=\"" << canvas.svgH + 120 << "\">\n";
    svg << "<rect width=\"" << canvas.svgW << "\" height=\"" << canvas.svgH + 120
        << "\" fill=\"#f5f5f5\"/>\n";

    // ── Perimeter ────────────────────────────────────────────────────────────
    svg << canvas.polygon(map.perimeterPoints(), "#e8f5e9", "#388E3C", 1.5f, 0.5f);

    // ── Zones ────────────────────────────────────────────────────────────────
    for (const auto& z : map.zones())
        svg << canvas.polygon(z.polygon, "#BBDEFB", "#1565C0", 1.2f, 0.4f);

    // ── Exclusions ───────────────────────────────────────────────────────────
    for (const auto& ex : map.exclusions())
        svg << canvas.polygon(ex, "#FFCDD2", "#C62828", 1.5f, 0.7f);

    // ── Route — split into same-semantic segments for clean colouring ────────
    {
        const auto& pts = plan.route.points;
        // Draw each consecutive pair as a line coloured by the FROM point semantic
        for (size_t i = 1; i < pts.size(); ++i)
        {
            const auto& a = pts[i-1];
            const auto& b = pts[i];
            const std::string col = semanticColor(a.semantic);
            const float sw = (a.semantic == RouteSemantic::COVERAGE_EDGE ||
                               a.semantic == RouteSemantic::COVERAGE_INFILL) ? 1.2f : 1.8f;
            svg << "<line"
                << " x1=\"" << canvas.tx(a.p.x) << "\""
                << " y1=\"" << canvas.ty(a.p.y) << "\""
                << " x2=\"" << canvas.tx(b.p.x) << "\""
                << " y2=\"" << canvas.ty(b.p.y) << "\""
                << " stroke=\"" << col << "\""
                << " stroke-width=\"" << sw << "\""
                << " stroke-linecap=\"round\"/>\n";
        }
    }

    // ── Start / end markers ───────────────────────────────────────────────────
    if (!plan.route.points.empty())
    {
        const auto& first = plan.route.points.front().p;
        const auto& last  = plan.route.points.back().p;
        svg << canvas.circle(first.x, first.y, 5.0f, "#00C853", "#fff");
        svg << canvas.circle(last.x,  last.y,  5.0f, "#D50000", "#fff");
    }

    // ── Dock ─────────────────────────────────────────────────────────────────
    svg << canvas.circle(1.0f, 0.5f, 6.0f, "#795548", "#fff");

    // ── Zone labels ──────────────────────────────────────────────────────────
    svg << canvas.text(4.5f, 14.0f, "Zone 1", "#1565C0", 11.0f);
    svg << canvas.text(15.5f, 14.0f, "Zone 2", "#1565C0", 11.0f);
    svg << canvas.text(4.5f, 8.8f, "NoGo", "#C62828", 10.0f);

    // ── Stats ─────────────────────────────────────────────────────────────────
    const float LY = canvas.svgH + 16;
    svg << "<text x=\"10\" y=\"" << LY
        << "\" font-size=\"12\" font-family=\"monospace\" fill=\"#333\">"
        << "Route points: " << plan.route.points.size()
        << "  |  Valid: " << (plan.route.valid ? "YES" : "NO")
        << "  |  Validator: " << (vr.valid ? "PASS" : "FAIL (" + std::to_string(vr.errors.size()) + " errors)")
        << "</text>\n";

    // ── Legend ────────────────────────────────────────────────────────────────
    const std::vector<RouteSemantic> legendItems = {
        RouteSemantic::COVERAGE_EDGE,
        RouteSemantic::COVERAGE_INFILL,
        RouteSemantic::TRANSIT_WITHIN_ZONE,
        RouteSemantic::TRANSIT_BETWEEN_COMPONENTS,
        RouteSemantic::TRANSIT_INTER_ZONE,
    };
    float lx = 10.0f;
    float ly = canvas.svgH + 36.0f;
    for (const auto& sem : legendItems)
    {
        const std::string col = semanticColor(sem);
        svg << "<rect x=\"" << lx << "\" y=\"" << ly
            << "\" width=\"16\" height=\"8\" fill=\"" << col << "\" rx=\"2\"/>\n";
        svg << "<text x=\"" << (lx + 20) << "\" y=\"" << (ly + 8)
            << "\" font-size=\"10\" font-family=\"monospace\" fill=\"#333\">"
            << semanticName(sem) << "</text>\n";
        lx += 165.0f;
        if (lx > canvas.svgW - 150)
        {
            lx = 10.0f;
            ly += 20.0f;
        }
    }
    // Start/end legend
    svg << "<circle cx=\"" << lx + 8 << "\" cy=\"" << (ly + 4)
        << "\" r=\"5\" fill=\"#00C853\"/>"
        << "<text x=\"" << (lx + 18) << "\" y=\"" << (ly + 8)
        << "\" font-size=\"10\" font-family=\"monospace\" fill=\"#333\">Start</text>\n";
    svg << "<circle cx=\"" << (lx + 80) << "\" cy=\"" << (ly + 4)
        << "\" r=\"5\" fill=\"#D50000\"/>"
        << "<text x=\"" << (lx + 90) << "\" y=\"" << (ly + 8)
        << "\" font-size=\"10\" font-family=\"monospace\" fill=\"#333\">End</text>\n";

    svg << "</svg>\n";

    // 5. Write output
    std::ofstream out(outPath);
    if (!out)
    {
        std::cerr << "ERROR: cannot open " << outPath << " for writing\n";
        return 1;
    }
    out << svg.str();
    std::cerr << "SVG written to: " << outPath << "\n";
    return 0;
}
