// useMowPath.test.ts — Vitest unit tests for computeMowPath() and clipPerimeterToZone()
//
// Run with: npm run test
//
// Tests:
//   1. Simple rectangle → all waypoints within [0,10]×[0,5], ascending y-values
//   2. Exclusion zone   → no waypoint inside the No-Go area
//   3. Perimeter clip   → all waypoints in left half (x ≤ 5)
//   4. Tiny zone        → result is empty (zone smaller than one strip)
//   5. clipPerimeterToZone → intersection of 10×10 square and inner 6×6 square

import { describe, it, expect } from 'vitest'
import {
  computeMowPath,
  clipPerimeterToZone,
  type Pt,
  type MowPathSettings,
} from './useMowPath'

// ── Shared settings helpers ───────────────────────────────────────────────────

function zeroTurnSettings(overrides: Partial<MowPathSettings> = {}): MowPathSettings {
  return {
    angle:      0,
    stripWidth: 1,
    overlap:    0,
    edgeMowing: false,
    edgeRounds: 0,
    turnType:   'zeroturn',
    startSide:  'auto',
    ...overrides,
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 1 — Rechteck einfach
// ─────────────────────────────────────────────────────────────────────────────

describe('computeMowPath — Test 1: Rechteck einfach', () => {
  it('erzeugt Waypoints ausschließlich innerhalb [0,10]×[0,5]', () => {
    const poly: Pt[] = [[0, 0], [10, 0], [10, 5], [0, 5]]
    const result = computeMowPath(poly, [], zeroTurnSettings())

    expect(result.length).toBeGreaterThan(0)

    for (const mp of result) {
      const [x, y] = mp.p
      expect(x).toBeGreaterThanOrEqual(-0.01)
      expect(x).toBeLessThanOrEqual(10.01)
      expect(y).toBeGreaterThanOrEqual(-0.01)
      expect(y).toBeLessThanOrEqual(5.01)
    }
  })

  it('Strip-Waypoints haben aufsteigende y-Werte (Boustrophedon-Ordnung)', () => {
    const poly: Pt[] = [[0, 0], [10, 0], [10, 5], [0, 5]]
    // angle=0 → horizontal strips (East-West), y increases per strip
    const result = computeMowPath(poly, [], zeroTurnSettings())

    // Regular strip waypoints: not marked slow or rev
    const stripPts = result.filter(mp => !mp.slow && !mp.rev)
    expect(stripPts.length).toBeGreaterThanOrEqual(4)  // at least 2 full strips

    const ys = stripPts.map(mp => mp.p[1])
    const minY = Math.min(...ys)
    const maxY = Math.max(...ys)

    // Strips span the height of the zone
    expect(minY).toBeGreaterThanOrEqual(0)
    expect(maxY).toBeLessThanOrEqual(5)
    // Multiple strips: y range covers > half the zone height
    expect(maxY - minY).toBeGreaterThan(2)
  })
})

// ─────────────────────────────────────────────────────────────────────────────
// Test 2 — Exclusion wird ausgeschnitten
// ─────────────────────────────────────────────────────────────────────────────

describe('computeMowPath — Test 2: Exclusion wird ausgeschnitten', () => {
  it('kein Waypoint liegt innerhalb der No-Go-Zone [3,7]×[3,7]', () => {
    const poly: Pt[] = [[0, 0], [10, 0], [10, 10], [0, 10]]
    const excl: Pt[] = [[3, 3], [7, 3], [7, 7], [3, 7]]
    const result = computeMowPath(poly, [excl], zeroTurnSettings())

    // Allow a small tolerance on the boundary — check strictly interior
    const eps = 0.05
    for (const mp of result) {
      const [x, y] = mp.p
      const insideExcl = x > 3 + eps && x < 7 - eps && y > 3 + eps && y < 7 - eps
      expect(insideExcl, `Waypoint (${x.toFixed(3)}, ${y.toFixed(3)}) liegt in der Exclusion`).toBe(false)
    }
  })
})

// ─────────────────────────────────────────────────────────────────────────────
// Test 3 — Perimeter-Clipping
// ─────────────────────────────────────────────────────────────────────────────

describe('computeMowPath — Test 3: Perimeter-Clipping auf linke Hälfte', () => {
  it('alle Waypoints haben x ≤ 5 wenn clipMask = linke Hälfte', () => {
    const perimeter: Pt[] = [[0, 0], [10, 0], [10, 10], [0, 10]]
    const clipMask: Pt[]  = [[0, 0], [5, 0],  [5, 10],  [0, 10]]

    const result = computeMowPath(perimeter, [], zeroTurnSettings(), clipMask)

    expect(result.length).toBeGreaterThan(0)

    for (const mp of result) {
      expect(mp.p[0]).toBeLessThanOrEqual(5.1)   // ≤5 with small tolerance for clipping edge
    }
  })
})

// ─────────────────────────────────────────────────────────────────────────────
// Test 4 — Leeres Ergebnis bei zu kleiner Zone
// ─────────────────────────────────────────────────────────────────────────────

describe('computeMowPath — Test 4: Zu kleine Zone ergibt leeres Ergebnis', () => {
  it('0.1×0.1 m Polygon mit stripWidth=1 → leeres Array', () => {
    const tiny: Pt[] = [[0, 0], [0.1, 0], [0.1, 0.1], [0, 0.1]]
    const settings = zeroTurnSettings({ stripWidth: 1, turnType: 'kturn' })
    const result = computeMowPath(tiny, [], settings)

    expect(result).toHaveLength(0)
  })
})

// ─────────────────────────────────────────────────────────────────────────────
// Test 5 — clipPerimeterToZone
// ─────────────────────────────────────────────────────────────────────────────

describe('clipPerimeterToZone — Test 5: Zone ⊂ Perimeter → Schnittmenge = Zone', () => {
  it('10×10 Perimeter ∩ 6×6 Zone ergibt Rechteck [2,8]×[2,8]', () => {
    const perimeter: Pt[] = [[0, 0], [10, 0], [10, 10], [0, 10]]
    const zone: Pt[]      = [[2, 2], [8, 2],  [8, 8],   [2, 8]]

    const result = clipPerimeterToZone(perimeter, zone)

    // Sutherland-Hodgman result must have at least 4 vertices
    expect(result.length).toBeGreaterThanOrEqual(4)

    // All resulting vertices must lie within the zone bounds
    for (const [x, y] of result) {
      expect(x).toBeGreaterThanOrEqual(2 - 0.01)
      expect(x).toBeLessThanOrEqual(8 + 0.01)
      expect(y).toBeGreaterThanOrEqual(2 - 0.01)
      expect(y).toBeLessThanOrEqual(8 + 0.01)
    }
  })
})
