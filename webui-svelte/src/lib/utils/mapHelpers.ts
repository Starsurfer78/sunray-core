/**
 * Shared map and geometry helper functions.
 * Extracted from Dashboard, Map, Mission, and other components to reduce duplication.
 */

export const RAD_TO_DEG = 180 / Math.PI;
export const CONNECTION_FRESH_MS = 5000;

export type Point = { x: number; y: number };
export type Segment = { a: Point; b: Point };

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
    return (b.y - a.y) * (c.x - b.x) - (b.x - a.x) * (c.y - b.y);
}

/**
 * Check if point b lies on segment (a, c) when they are collinear.
 */
function onSegment(a: Point, b: Point, c: Point): boolean {
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

    if (o1 === 0 && onSegment(s1.a, s2.a, s1.b)) return true;
    if (o2 === 0 && onSegment(s1.a, s2.b, s1.b)) return true;
    if (o3 === 0 && onSegment(s2.a, s1.a, s2.b)) return true;
    if (o4 === 0 && onSegment(s2.a, s1.b, s2.b)) return true;

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
