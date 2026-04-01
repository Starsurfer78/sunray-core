import { Clipper, EndType, FillRule, JoinType } from 'clipper2-js'
import { toPaths, fromPaths, SCALE, type Point } from './types'

export interface EdgeResult {
  /** Polygon-Konturen der Randmäh-Runden (je eine Mittellinie pro Runde) */
  edgePaths: Point[][]
  /** Verbleibende Innenfläche für Infill-Bahnen */
  infillAreas: Point[][]
}

/**
 * Erzeugt Randmäh-Runden und die verbleibende Infill-Fläche.
 *
 * Jede Runde i fährt entlang eines nach innen versetzten Polygons:
 *   Mittellinie Runde i = effectiveArea deflated um (i − 0.5) × stripWidth
 * Infill-Fläche = effectiveArea deflated um edgeRounds × stripWidth
 */
export function generateEdgeRounds(
  effectiveAreas: Point[][],
  stripWidth: number,
  edgeRounds: number
): EdgeResult {
  if (effectiveAreas.length === 0 || edgeRounds <= 0 || stripWidth <= 0) {
    return { edgePaths: [], infillAreas: effectiveAreas }
  }

  const edgePaths: Point[][] = []

  for (let i = 1; i <= edgeRounds; i++) {
    const midDelta = -(i - 0.5) * stripWidth * SCALE
    const midPaths = Clipper.InflatePaths(
      toPaths(effectiveAreas),
      midDelta,
      JoinType.Round,
      EndType.Polygon
    )
    edgePaths.push(...fromPaths(midPaths))
  }

  // Infill-Fläche: effectiveArea um alle Randrunden schrumpfen
  const infillDelta = -edgeRounds * stripWidth * SCALE
  const infillPaths = Clipper.InflatePaths(
    toPaths(effectiveAreas),
    infillDelta,
    JoinType.Round,
    EndType.Polygon
  )

  // Sicherstellen dass Infill nicht über effectiveArea hinausgeht
  const clipped = infillPaths.length > 0
    ? Clipper.Intersect(infillPaths, toPaths(effectiveAreas), FillRule.NonZero)
    : infillPaths

  return { edgePaths, infillAreas: fromPaths(clipped) }
}
