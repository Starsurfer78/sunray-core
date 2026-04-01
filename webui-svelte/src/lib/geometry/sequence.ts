import { type Segment } from './scanline'
import { type Point } from './types'

function dist2(a: Point, b: Point): number {
  return (a.x - b.x) ** 2 + (a.y - b.y) ** 2
}

/**
 * Sortiert Segmente per Nearest-Neighbour:
 * Vom Ende des aktuellen Segments wird immer das nächstgelegene
 * noch nicht besuchte Segment gewählt. Liegt dessen Ende näher als
 * sein Anfang, wird es umgedreht (Zick-Zack über Flächen mit NoGo-Zonen).
 */
export function sortSegmentsNearestNeighbour(segments: Segment[]): Segment[] {
  if (segments.length === 0) return []

  const remaining = segments.slice()
  const result: Segment[] = []
  let current = remaining.splice(0, 1)[0]
  result.push(current)

  while (remaining.length > 0) {
    let bestIdx = 0
    let bestDist = Infinity
    let bestFlip = false

    for (let i = 0; i < remaining.length; i++) {
      const s = remaining[i]
      const dStart = dist2(current.b, s.a)
      const dEnd   = dist2(current.b, s.b)
      if (dStart < bestDist) { bestDist = dStart; bestIdx = i; bestFlip = false }
      if (dEnd   < bestDist) { bestDist = dEnd;   bestIdx = i; bestFlip = true  }
    }

    const next = remaining.splice(bestIdx, 1)[0]
    current = bestFlip ? { a: next.b, b: next.a } : next
    result.push(current)
  }

  return result
}
