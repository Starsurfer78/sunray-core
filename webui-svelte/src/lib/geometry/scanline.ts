import { type Point } from './types'

export interface Segment { a: Point; b: Point }

function rotate(p: Point, cos: number, sin: number): Point {
  return { x: p.x * cos - p.y * sin, y: p.x * sin + p.y * cos }
}

/** Schnittpunkte eines horizontalen Strahls y mit dem Polygon */
function intersectionsAtY(polygon: Point[], y: number): number[] {
  const xs: number[] = []
  const n = polygon.length
  for (let i = 0; i < n; i++) {
    const a = polygon[i]
    const b = polygon[(i + 1) % n]
    if ((a.y < y && b.y >= y) || (b.y < y && a.y >= y)) {
      const t = (y - a.y) / (b.y - a.y)
      xs.push(a.x + t * (b.x - a.x))
    }
  }
  return xs.sort((x1, x2) => x1 - x2)
}

/**
 * Erzeugt parallele Mähbahnen für eine bereits bereinigte Fläche
 * (Ergebnis von getEffectiveZoneArea).
 *
 * - stripWidth: Bahnbreite in Metern
 * - angleDeg: Mähwinkel in Grad (0 = horizontal)
 * - Zick-Zack: jede zweite Bahn wird umgekehrt
 */
export function generateStripes(
  effectiveAreas: Point[][],
  stripWidth: number,
  angleDeg: number
): Segment[] {
  if (effectiveAreas.length === 0 || stripWidth <= 0) return []

  const angle = (angleDeg * Math.PI) / 180
  const cosNeg = Math.cos(-angle)
  const sinNeg = Math.sin(-angle)
  const cosPos = Math.cos(angle)
  const sinPos = Math.sin(angle)

  // Polygone in Bahnrichtung rotieren
  const rotated = effectiveAreas.map(poly => poly.map(p => rotate(p, cosNeg, sinNeg)))

  const allPts = rotated.flat()
  if (allPts.length === 0) return []

  const minY = Math.min(...allPts.map(p => p.y))
  const maxY = Math.max(...allPts.map(p => p.y))

  const segments: Segment[] = []
  let rowIdx = 0
  let y = minY + stripWidth / 2

  while (y <= maxY + stripWidth * 0.01) {
    const xs = rotated.flatMap((poly) => intersectionsAtY(poly, y)).sort((a, b) => a - b)
    for (let i = 0; i + 1 < xs.length; i += 2) {
      const a = rotate({ x: xs[i], y }, cosPos, sinPos)
      const b = rotate({ x: xs[i + 1], y }, cosPos, sinPos)
      // Zick-Zack: gerade Reihen vorwärts, ungerade rückwärts
      segments.push(rowIdx % 2 === 0 ? { a, b } : { a: b, b: a })
    }
    y += stripWidth
    rowIdx++
  }

  return segments
}
