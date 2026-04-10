import { Clipper, FillRule } from 'clipper2-js'
import { toClipper, toPaths, fromPaths, type Point } from './types'

function normalizeAreaPaths(paths: Point[][]): Point[][] {
  if (paths.length === 0) return []
  return fromPaths(Clipper.Union(toPaths(paths), undefined, FillRule.NonZero))
}

/**
 * Berechnet die tatsächliche Mähfläche einer Zone:
 * effectiveArea = Zone ∩ Perimeter − NoGoZonen
 */
function getEffectiveZoneArea(
  zone: Point[],
  perimeter: Point[],
  exclusions: Point[][]
): Point[][] {
  if (zone.length < 3) return []

  // Wenn kein Perimeter vorhanden: Zone direkt verwenden
  if (perimeter.length < 3) return [zone]

  const subj = toPaths([zone])
  const clip = toPaths([perimeter])

  // Zone ∩ Perimeter
  let result = Clipper.Intersect(subj, clip, FillRule.NonZero)
  if (result.length === 0) return []

  // NoGo-Zonen abziehen
  const validExcl = exclusions.filter(e => e.length >= 3)
  if (validExcl.length > 0) {
    result = Clipper.Difference(result, toPaths(validExcl), FillRule.NonZero)
  }

  return normalizeAreaPaths(fromPaths(result))
}
