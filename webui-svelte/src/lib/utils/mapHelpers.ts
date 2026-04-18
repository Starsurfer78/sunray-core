/**
 * Shared map and geometry helper functions.
 * Extracted from Dashboard, Map, Mission, and other components to reduce duplication.
 */

export const RAD_TO_DEG = 180 / Math.PI;
export const CONNECTION_FRESH_MS = 5000;

export type Point = { x: number; y: number };
export type Segment = { a: Point; b: Point };
export type GpsOrigin = { lat: number; lon: number };

/**
 * Clamp a value between min and max.
 */
export function clamp(value: number, min: number, max: number): number {
    return Math.max(min, Math.min(max, value));
}

/**
 * Normalize points to Point objects { x, y }.
 * Handles both array and object formats.
 */
export function normalizePoints(
    points: Array<[number, number]> | Point[] | undefined,
): Point[] {
    if (!points) return [];
    return points.map((p) => (Array.isArray(p) ? { x: p[0], y: p[1] } : p));
}

/**
 * Normalize a MapZone to a Zone object with normalized polygon points.
 */
export function normalizeZone<T extends { id: string; name?: string; order?: number; polygon: any }>(
    zone: T,
    index: number,
): { id: string; name: string; order: number; polygon: Point[] } {
    return {
        id: zone.id,
        name: zone.name ?? `Zone ${index + 1}`,
        order: zone.order ?? index + 1,
        polygon: normalizePoints(zone.polygon),
    };
}

/**
 * Determine the orientation of three points.
 * Used for polygon self-intersection detection.
 *
 * Returns:
 * - 0 if collinear
 * - > 0 if counterclockwise
 * - < 0 if clockwise
 */
export function orientation(a: Point, b: Point, c: Point): number {
    return (b.y - a.y) * (c.x - a.x) - (b.x - a.x) * (c.y - a.y);
}

/**
 * Check if point b lies on segment (a, c) when they are collinear.
 */
export function onSegment(a: Point, b: Point, c: Point): boolean {
    return (
        Math.min(a.x, c.x) <= b.x &&
        b.x <= Math.max(a.x, c.x) &&
        Math.min(a.y, c.y) <= b.y &&
        b.y <= Math.max(a.y, c.y)
    );
}

/**
 * Detect if two segments intersect (not just touching at endpoints).
 */
export function segmentsIntersect(s1: Segment, s2: Segment): boolean {
    const o1 = orientation(s1.a, s1.b, s2.a);
    const o2 = orientation(s1.a, s1.b, s2.b);
    const o3 = orientation(s2.a, s2.b, s1.a);
    const o4 = orientation(s2.a, s2.b, s1.b);

    const EPS = 1e-9;
    if (Math.abs(o1) < EPS && onSegment(s1.a, s2.a, s1.b)) return true;
    if (Math.abs(o2) < EPS && onSegment(s1.a, s2.b, s1.b)) return true;
    if (Math.abs(o3) < EPS && onSegment(s2.a, s1.a, s2.b)) return true;
    if (Math.abs(o4) < EPS && onSegment(s2.a, s1.b, s2.b)) return true;

    return o1 > 0 !== o2 > 0 && o3 > 0 !== o4 > 0;
}

/**
 * Check if a polygon has self-intersecting edges.
 * Assumes the polygon is closed (last segment connects back to first point).
 */
export function hasSelfIntersection(points: Point[]): boolean {
    if (points.length < 4) return false;
    const segments: Segment[] = points.map((point, index) => ({
        a: point,
        b: points[(index + 1) % points.length],
    }));

    for (let i = 0; i < segments.length; i += 1) {
        for (let j = i + 1; j < segments.length; j += 1) {
            // Skip adjacent segments (they share a vertex)
            const adjacent = j === i + 1 || (i === 0 && j === segments.length - 1);
            if (adjacent) continue;
            if (segmentsIntersect(segments[i], segments[j])) return true;
        }
    }
    return false;
}

export function gpsOriginForPerimeter(perimeter: Point[]): GpsOrigin | null {
    if (perimeter.length < 3) return null;
    let sumLon = 0;
    let sumLat = 0;
    for (const p of perimeter) {
        sumLon += p.x;
        sumLat += p.y;
    }
    const avgLon = sumLon / perimeter.length;
    const avgLat = sumLat / perimeter.length;
    const spanLon = Math.max(...perimeter.map((p) => p.x)) - Math.min(...perimeter.map((p) => p.x));
    const spanLat = Math.max(...perimeter.map((p) => p.y)) - Math.min(...perimeter.map((p) => p.y));
    if (
        avgLat >= -90 &&
        avgLat <= 90 &&
        avgLon >= -180 &&
        avgLon <= 180 &&
        spanLon > 0 &&
        spanLon < 0.5 &&
        spanLat > 0 &&
        spanLat < 0.5
    ) {
        return { lat: avgLat, lon: avgLon };
    }
    return null;
}

export function gpsToLocalMeters(
    lon: number,
    lat: number,
    origin: GpsOrigin,
): Point {
    const EARTH_R = 6378137.0;
    const toRad = Math.PI / 180;
    const refMercY = Math.log(Math.tan(Math.PI / 4 + (origin.lat * toRad) / 2));
    return {
        x: (lon - origin.lon) * EARTH_R * toRad,
        y: (Math.log(Math.tan(Math.PI / 4 + (lat * toRad) / 2)) - refMercY) * EARTH_R,
    };
}

export function projectGpsPointsToLocalMeters(
    points: Point[],
    origin: GpsOrigin,
): Point[] {
    return points.map((p) => gpsToLocalMeters(p.x, p.y, origin));
}

type NormalizeMapInput = {
    perimeter?: Array<[number, number]> | Point[];
    dock?: Array<[number, number]> | Point[];
    exclusions?: Array<Array<[number, number]> | Point[]>;
    zones?: Array<{ id: string; name?: string; order?: number; polygon: any }>;
    planner?: unknown;
    dockMeta?: { corridor?: Array<[number, number]> | Point[]; [key: string]: unknown };
    exclusionMeta?: unknown;
    captureMeta?: unknown;
};

export function normalizeMapDocumentForUi(map: NormalizeMapInput): {
    map: {
        perimeter: Point[];
        dock: Point[];
        exclusions: Point[][];
        zones: Array<{ id: string; name: string; order: number; polygon: Point[] }>;
        planner?: unknown;
        dockMeta?: unknown;
        exclusionMeta?: unknown;
        captureMeta?: unknown;
    };
    gpsOrigin: GpsOrigin | null;
} {
    const perimeter = normalizePoints(map.perimeter ?? []);
    const gpsOrigin = gpsOriginForPerimeter(perimeter);
    const projectPoints = gpsOrigin
        ? (pts: Point[]) => projectGpsPointsToLocalMeters(pts, gpsOrigin)
        : (pts: Point[]) => pts;

    return {
        map: {
            perimeter: projectPoints(perimeter),
            dock: projectPoints(normalizePoints(map.dock ?? [])),
            exclusions: (map.exclusions ?? []).map((exclusion) =>
                projectPoints(normalizePoints(exclusion as Array<[number, number]> | Point[])),
            ),
            zones: (map.zones ?? []).map((zone, index) => {
                const z = normalizeZone(zone, index);
                return { ...z, polygon: projectPoints(z.polygon) };
            }),
            planner: map.planner,
            dockMeta: map.dockMeta
                ? {
                      ...map.dockMeta,
                      corridor: projectPoints(normalizePoints(map.dockMeta.corridor ?? [])),
                  }
                : undefined,
            exclusionMeta: map.exclusionMeta,
            captureMeta: map.captureMeta ?? {},
        },
        gpsOrigin,
    };
}
