import { Clipper, Path64, Paths64 } from 'clipper2-js'

export interface Point { x: number; y: number }

// Skalierungsfaktor: Meter → Millimeter (Ganzzahlen für Clipper)
export const SCALE = 1000

export function toClipper(pts: Point[]): Path64 {
  const flat: number[] = []
  for (const p of pts) flat.push(Math.round(p.x * SCALE), Math.round(p.y * SCALE))
  return Clipper.makePath(flat)
}

export function fromClipper(path: Path64): Point[] {
  return Array.from(path).map(p => ({ x: p.x / SCALE, y: p.y / SCALE }))
}

export function toPaths(polys: Point[][]): Paths64 {
  const paths = new Paths64()
  for (const poly of polys) paths.push(toClipper(poly))
  return paths
}

export function fromPaths(paths: Paths64): Point[][] {
  return Array.from(paths).map(fromClipper)
}
