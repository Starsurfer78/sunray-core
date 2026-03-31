// useMowPath.ts — Mähbahnen-Berechnungsalgorithmus
//
// Bugfixes:
//   BUG-1: clipLineToZone() — Zonen-Clipping vor Exclusion-Clipping; garantiert
//          dass Streifen nie über Zonengrenze hinauslaufen (konkave Polygone, 4+ Schnittpunkte)
//   BUG-2: Edge-Rings werden per Segment-Clipping an Exclusion-Grenzen aufgetrennt
//   BUG-3: angle=0 → horizontale Streifen (Ost-West) — Rotation verifiziert korrekt
//   BUG-4: Zone ∩ Perimeter via dual clipLineToZone + Intervall-Schnitt (max/min) — robust für konkave Polygone

// ── Types ──────────────────────────────────────────────────────────────────────

export type Pt = [number, number]  // [x, y] lokale Meter

export interface MowPt {
  p:     Pt
  rev?:  boolean   // rückwärts zu diesem Punkt fahren
  slow?: boolean   // reduzierte Geschwindigkeit
}

export interface MowPathSettings {
  angle:      number           // Streifenwinkel 0–179° (0 = horizontal/Ost-West)
  stripWidth: number           // Bahnbreite in m
  overlap:    number           // Überlappung 0–50 %
  edgeMowing: boolean          // Randstreifen an/aus
  edgeRounds: number           // Randstreifen-Runden 1–5
  turnType:   'kturn' | 'zeroturn'
  startSide:  'auto' | 'top' | 'bottom' | 'left' | 'right'
}

export const DEFAULT_SETTINGS: MowPathSettings = {
  angle:      0,
  stripWidth: 0.18,
  overlap:    10,
  edgeMowing: true,
  edgeRounds: 1,
  turnType:   'kturn',
  startSide:  'auto',
}

// ── 2D-Vektor-Hilfsfunktionen ──────────────────────────────────────────────────

function add(a: Pt, b: Pt): Pt      { return [a[0]+b[0], a[1]+b[1]] }
function sub(a: Pt, b: Pt): Pt      { return [a[0]-b[0], a[1]-b[1]] }
function scale(v: Pt, s: number): Pt { return [v[0]*s, v[1]*s] }
function dot(a: Pt, b: Pt): number   { return a[0]*b[0] + a[1]*b[1] }
function len(v: Pt): number          { return Math.sqrt(v[0]*v[0]+v[1]*v[1]) }

function normalize(v: Pt): Pt {
  const l = len(v)
  return l < 1e-9 ? [0, 0] : [v[0]/l, v[1]/l]
}

/** Rotate v by 90° counter-clockwise */
function perp(v: Pt): Pt { return [-v[1], v[0]] }

/** Rotate point p: x' = x·cos − y·sin,  y' = x·sin + y·cos */
function rotPt(p: Pt, cos: number, sin: number): Pt {
  return [p[0]*cos - p[1]*sin, p[0]*sin + p[1]*cos]
}

// ── Polygon-Hilfsfunktionen ────────────────────────────────────────────────────

/** Vorzeichenbehaftete Fläche (positiv = CCW) */
function signedArea(poly: Pt[]): number {
  let a = 0
  const n = poly.length
  for (let i = 0; i < n; i++) {
    const j = (i + 1) % n
    a += poly[i][0] * poly[j][1]
    a -= poly[j][0] * poly[i][1]
  }
  return a * 0.5
}

/** Stellt CCW-Orientierung sicher */
function ensureCCW(poly: Pt[]): Pt[] {
  return signedArea(poly) > 0 ? poly : [...poly].reverse()
}

/**
 * Ray-casting Point-in-Polygon.
 * Funktioniert für CW und CCW Polygone (Paritätsregel).
 */
function pointInPolygon(pt: Pt, poly: Pt[]): boolean {
  const [px, py] = pt
  let inside = false
  const n = poly.length
  for (let i = 0, j = n-1; i < n; j = i++) {
    const [xi, yi] = poly[i], [xj, yj] = poly[j]
    if (((yi > py) !== (yj > py)) && (px < (xj-xi)*(py-yi)/(yj-yi)+xi))
      inside = !inside
  }
  return inside
}

/**
 * Berechnet Polygon inward-offset (CCW-Polygon, Vertex-Bisektoren-Methode).
 * Gibt null zurück wenn das Ergebnis degeneriert (zu kleines Polygon).
 */
export function inwardOffsetPolygon(poly: Pt[], dist: number): Pt[] | null {
  const ordered = ensureCCW(poly)
  const n = ordered.length
  if (n < 3) return null

  const result: Pt[] = []
  for (let i = 0; i < n; i++) {
    const prev = ordered[(i - 1 + n) % n]
    const curr = ordered[i]
    const next = ordered[(i + 1) % n]

    const d1 = normalize(sub(curr, prev))
    const d2 = normalize(sub(next, curr))

    // Inward normals (left normal for CCW)
    const n1 = perp(d1)
    const n2 = perp(d2)

    // Bisector of the two inward normals
    const bis = normalize(add(n1, n2) as Pt)

    // Scale so that component along n1 = dist
    const cosHalf = Math.max(0.15, dot(n1, bis))
    const offsetDist = dist / cosHalf

    result.push(add(curr, scale(bis, offsetDist)))
  }

  // Sanity: check that area didn't flip (over-offset)
  if (signedArea(result) <= 0) return null
  return result
}

// ── Linien-Segment-Hilfsfunktionen ────────────────────────────────────────────

/**
 * Parametrischer Schnittpunkt von Segment [p1,p2] mit Segment [q1,q2].
 * Gibt t ∈ [0,1] zurück (Position auf [p1,p2]) oder null wenn kein Schnitt.
 */
function lineSegmentIntersectParam(p1: Pt, p2: Pt, q1: Pt, q2: Pt): number | null {
  const dx = p2[0]-p1[0], dy = p2[1]-p1[1]
  const ex = q2[0]-q1[0], ey = q2[1]-q1[1]
  const denom = dx*ey - dy*ex
  if (Math.abs(denom) < 1e-10) return null  // parallel
  const fx = q1[0]-p1[0], fy = q1[1]-p1[1]
  const t = (fx*ey - fy*ex) / denom
  const s = (fx*dy - fy*dx) / denom
  if (t >= -1e-9 && t <= 1+1e-9 && s >= -1e-9 && s <= 1+1e-9)
    return Math.max(0, Math.min(1, t))
  return null
}

/** Interpoliert einen Punkt auf Segment [p1,p2] bei Parameter t */
function lerpPt(p1: Pt, p2: Pt, t: number): Pt {
  return [p1[0] + t*(p2[0]-p1[0]), p1[1] + t*(p2[1]-p1[1])]
}

/**
 * Clippt Liniensegment [p1,p2] gegen ein Polygon — behält INNERHALB-Teile.
 * Analog zu clipSegmentByExclusions, aber invertiert: behält Segmente die
 * INNERHALB von poly liegen (statt außerhalb).
 * Verwendet für: Edge-Ring-Clipping gegen Perimeter-Grenze.
 */
function clipSegmentToPolygon(p1: Pt, p2: Pt, poly: Pt[]): [Pt, Pt][] {
  const tSet = new Set<number>()
  tSet.add(0); tSet.add(1)
  const n = poly.length
  for (let i = 0; i < n; i++) {
    const t = lineSegmentIntersectParam(p1, p2, poly[i], poly[(i+1)%n])
    if (t !== null) tSet.add(Math.round(t * 1e8) / 1e8)
  }
  const tArr = [...tSet].sort((a, b) => a - b)
  const result: [Pt, Pt][] = []
  for (let i = 0; i + 1 < tArr.length; i++) {
    const tMid = (tArr[i] + tArr[i+1]) / 2
    const mid  = lerpPt(p1, p2, tMid)
    if (pointInPolygon(mid, poly))
      result.push([lerpPt(p1, p2, tArr[i]), lerpPt(p1, p2, tArr[i+1])])
  }
  return result
}

/**
 * Berechnet Polygon outward-offset (CCW-Polygon, Vertex-Bisektoren-Methode).
 * Gegenteil von inwardOffsetPolygon — verschiebt Vertices nach außen.
 * Verwendet für: Randstreifen um No-Go-Zonen (außen herum mähen).
 */
function outwardOffsetPolygon(poly: Pt[], dist: number): Pt[] | null {
  const ordered = ensureCCW(poly)
  const n = ordered.length
  if (n < 3) return null
  const result: Pt[] = []
  for (let i = 0; i < n; i++) {
    const prev = ordered[(i - 1 + n) % n]
    const curr = ordered[i]
    const next = ordered[(i + 1) % n]
    const d1 = normalize(sub(curr, prev))
    const d2 = normalize(sub(next, curr))
    // Outward normals für CCW = Rechts-Normalen = [d[1], -d[0]]
    const n1: Pt = [d1[1], -d1[0]]
    const n2: Pt = [d2[1], -d2[0]]
    const bis = normalize(add(n1, n2) as Pt)
    const cosHalf = Math.max(0.15, dot(n1, bis))
    result.push(add(curr, scale(bis, dist / cosHalf)))
  }
  // Sanity: Fläche sollte gewachsen sein (outward)
  if (signedArea(result) <= signedArea(ordered)) return null
  return result
}

/**
 * BUG-2 FIX — Clippt Liniensegment [p1,p2] gegen mehrere Exclusion-Polygone.
 * Gibt die Teilsegmente zurück, die AUSSERHALB aller Exclusions liegen.
 * Verwendet parametrische Schnittpunkte — keine Waypoint-Filterung.
 */
function clipSegmentByExclusions(p1: Pt, p2: Pt, excls: Pt[][]): [Pt, Pt][] {
  // Sammle alle t-Parameter wo das Segment eine Exclusion-Kante kreuzt
  const tSet = new Set<number>()
  tSet.add(0); tSet.add(1)

  for (const excl of excls) {
    const n = excl.length
    for (let i = 0; i < n; i++) {
      const t = lineSegmentIntersectParam(p1, p2, excl[i], excl[(i+1)%n])
      if (t !== null) tSet.add(Math.round(t * 1e8) / 1e8)
    }
  }

  const tArr = [...tSet].sort((a, b) => a - b)

  const result: [Pt, Pt][] = []
  for (let i = 0; i + 1 < tArr.length; i++) {
    const tMid = (tArr[i] + tArr[i+1]) / 2
    const mid  = lerpPt(p1, p2, tMid)
    // Behalte nur Teilsegmente die AUSSERHALB aller Exclusions liegen
    if (!excls.some(ex => pointInPolygon(mid, ex))) {
      result.push([lerpPt(p1, p2, tArr[i]), lerpPt(p1, p2, tArr[i+1])])
    }
  }
  return result
}

// ── Scanline-Clipping ──────────────────────────────────────────────────────────

/**
 * Alle x-Schnittpunkte einer Horizontallinie y=c mit Polygon-Kanten.
 * Half-open Intervall-Konvention verhindert doppelte Vertex-Zählung.
 */
function intersectHorizPoly(poly: Pt[], y: number): number[] {
  const xs: number[] = []
  const n = poly.length
  for (let i = 0; i < n; i++) {
    const [x1, y1] = poly[i]
    const [x2, y2] = poly[(i+1) % n]
    if ((y1 <= y && y2 > y) || (y2 <= y && y1 > y)) {
      xs.push(x1 + (y - y1) * (x2 - x1) / (y2 - y1))
    }
  }
  return xs
}

/**
 * Clippt x-Intervall [a,b] bei y gegen Exclusion-Polygone.
 * Gibt verbleibende (nicht-ausgeschnittene) Segmente zurück.
 */
function clipByExclusions(
  a: number, b: number, y: number,
  exclusions: Pt[][],
): [number, number][] {
  let segs: [number, number][] = [[a, b]]

  for (const excl of exclusions) {
    const next: [number, number][] = []
    for (const [s0, s1] of segs) {
      const cuts = intersectHorizPoly(excl, y)
        .filter(x => x > s0 && x < s1)
        .sort((xa, xb) => xa - xb)

      const all = [s0, ...cuts, s1]
      for (let k = 0; k + 1 < all.length; k++) {
        const mid = (all[k] + all[k+1]) / 2
        if (!pointInPolygon([mid, y], excl))
          next.push([all[k], all[k+1]])
      }
    }
    // Merge benachbarte/überlappende Segmente
    segs = next.sort((a2, b2) => a2[0]-b2[0]).reduce((acc, seg) => {
      if (acc.length && seg[0] <= acc[acc.length-1][1] + 1e-6) {
        acc[acc.length-1][1] = Math.max(acc[acc.length-1][1], seg[1])
      } else {
        acc.push([...seg] as [number,number])
      }
      return acc
    }, [] as [number,number][])
  }

  return segs
}

// ── Scanline-Clipping gegen Zonen-Polygon ─────────────────────────────────────

/**
 * Clippt eine Horizontallinie y gegen das Zonen-Polygon.
 * Gibt alle Intervalle [x0, x1] zurück die tatsächlich innerhalb der Zone liegen.
 *
 * Auch bei konkaven Polygonen mit 4+ Schnittpunkten werden die Paare per
 * Midpoint-Check verifiziert — jedes zurückgegebene Segment ist garantiert
 * innerhalb der Zone.
 */
function clipLineToZone(y: number, rZone: Pt[]): [number, number][] {
  const xs = intersectHorizPoly(rZone, y).sort((a, b) => a - b)
  const result: [number, number][] = []
  // Check every consecutive pair — robust for concave polygons where inside/outside
  // alternation may skip even-indexed boundaries (e.g. 4 intersections, 2nd pair inside)
  for (let i = 0; i + 1 < xs.length; i++) {
    const mid = (xs[i] + xs[i+1]) / 2
    if (pointInPolygon([mid, y], rZone))
      result.push([xs[i], xs[i+1]])
  }
  return result
}

// ── Streifen generieren ────────────────────────────────────────────────────────

/**
 * Erzeugt Mähstreifen als Array von [Start, Ende]-Punkten.
 *
 * BUG-1 FIX: clipLineToZone() liefert nur Segmente die garantiert innerhalb
 * der Zone liegen. Erst danach wird clipByExclusions auf jedes Segment
 * angewendet. Das stellt sicher dass Bahnen nicht über die Zonengrenze
 * hinauslaufen — auch bei konkaven Polygonen mit 4+ Schnittpunkten.
 *
 * BUG-3 VERIFICATION: angle=0 → cos(-0)=1, sin(-0)=0 → rot = Identity.
 * Streifen bei konstanten y-Werten → horizontal (Ost-West). ✓
 */
function generateStrips(
  zonePoly:      Pt[],
  exclusions:    Pt[][],
  angleDeg:      number,
  stripDist:     number,
  inset:         number,
  perimeterClip: Pt[] | null = null,
): Pt[][] {
  // Rotation: dreht das Problem so, dass Streifen horizontal werden
  // angle=0: cos=1, sin=0 → keine Rotation, Streifen bleiben horizontal ✓
  const angle =  angleDeg * Math.PI / 180
  const cosF  =  Math.cos(-angle)    // forward rotation
  const sinF  =  Math.sin(-angle)
  const cosB  =  Math.cos( angle)    // back rotation (= inverse)
  const sinB  =  Math.sin( angle)

  const rot   = (p: Pt): Pt => rotPt(p, cosF, sinF)
  const unrot = (p: Pt): Pt => rotPt(p, cosB, sinB)

  const rZone  = zonePoly.map(rot)
  const rExcls = exclusions.map(ex => ex.map(rot))

  const ys   = rZone.map(p => p[1])
  const yMin = Math.min(...ys)
  const yMax = Math.max(...ys)

  const strips: Pt[][] = []

  // Perimeter in rotierten Frame transformieren (falls übergeben)
  const rPerim: Pt[] | null = perimeterClip ? perimeterClip.map(rot) : null

  for (let y = yMin + stripDist * 0.5; y < yMax + stripDist * 0.1; y += stripDist) {
    // Zonen-Clipping: clipLineToZone liefert garantiert Zone-innere Segmente
    const zoneSegs = clipLineToZone(y, rZone)
    if (zoneSegs.length === 0) continue

    // Perimeter-Clipping: ebenfalls per clipLineToZone — gleiche Robustheit wie Zone-Clipping.
    // Dann Schnittmenge: [max(zx0,px0), min(zx1,px1)] pro Paar.
    // clipByExclAsZone hatte Schwächen mit der Half-Open-Convention an Polygon-Vertices.
    const perimBounds = rPerim ? clipLineToZone(y, rPerim) : null

    for (const [zx0, zx1] of zoneSegs) {
      // Segmente die innerhalb BEIDER Polygone liegen (Zone ∩ Perimeter)
      const workSegs: [number, number][] = []
      if (perimBounds) {
        for (const [px0, px1] of perimBounds) {
          const ix0 = Math.max(zx0, px0)
          const ix1 = Math.min(zx1, px1)
          if (ix1 > ix0 + 1e-9) workSegs.push([ix0, ix1])
        }
      } else {
        workSegs.push([zx0, zx1])
      }

      for (const [wx0, wx1] of workSegs) {
        const segs = clipByExclusions(wx0, wx1, y, rExcls)
        for (const [x0, x1] of segs) {
          const segLen    = x1 - x0
          const safeInset = Math.min(inset, segLen * 0.4)
          const sx0 = x0 + safeInset
          const sx1 = x1 - safeInset
          if (sx1 - sx0 < stripDist * 0.3) continue
          strips.push([unrot([sx0, y]), unrot([sx1, y])])
        }
      }
    }
  }

  return strips
}

// ── Streifen-Sortierung ────────────────────────────────────────────────────────

/**
 * Nächster-Nachbar-Sortierung (greedy).
 * Minimiert Überfahrtstrecken bei konkaven Polygonen (L-Form, U-Form usw.).
 */
function orderNearestNeighbor(strips: Pt[][], startIdx: number): Pt[][] {
  if (strips.length === 0) return []
  const remaining = strips.map(s => [...s] as Pt[])
  const result:   Pt[][] = []

  const first = remaining.splice(startIdx, 1)[0]
  result.push(first)
  let currentEnd: Pt = first[1]

  while (remaining.length > 0) {
    let bestIdx     = 0
    let bestDist    = Infinity
    let bestReverse = false

    for (let i = 0; i < remaining.length; i++) {
      const s  = remaining[i]
      const d0 = len(sub(s[0], currentEnd))
      const d1 = len(sub(s[1], currentEnd))
      if (d0 < bestDist) { bestDist = d0; bestIdx = i; bestReverse = false }
      if (d1 < bestDist) { bestDist = d1; bestIdx = i; bestReverse = true  }
    }

    const strip   = remaining.splice(bestIdx, 1)[0]
    const ordered = bestReverse ? [strip[1], strip[0]] as Pt[] : strip
    result.push(ordered)
    currentEnd = ordered[1]
  }

  return result
}

// ── Wendemanöver ──────────────────────────────────────────────────────────────

/**
 * K-Turn (3-Punkt-Wende): Vorwärts (slow) → Rückwärts (rev+slow) → Weiterfahrt.
 */
function kTurnTransition(
  stripEnd:  Pt,
  stripDir:  Pt,
  nextStart: Pt,
  overshoot: number,
): MowPt[] {
  const toNext   = sub(nextStart, stripEnd)
  const perpSign = dot(perp(stripDir), toNext) >= 0 ? 1 : -1
  const perpDir  = scale(perp(stripDir), perpSign)

  const p2 = add(stripEnd, scale(stripDir, overshoot))

  // lateralDist = nur die senkrechte Komponente von toNext (Streifenabstand).
  // Früher len(toNext)*0.6 — war falsch wenn Streifen quer zur No-Go-Zone
  // verbunden werden: die horizontale Komponente blähte den Wert auf → p3
  // verließ die Zone am oberen/unteren Rand.
  const lateralDist = dot(perpDir, toNext)
  const p3 = add(p2, add(
    scale(stripDir, -overshoot * 0.7),
    scale(perpDir,   lateralDist),
  ))

  return [
    { p: p2, slow: true },
    { p: p3, rev: true, slow: true },
    { p: nextStart },
  ]
}

/**
 * Zero-Turn: direkt zum Startpunkt der nächsten Bahn.
 */
function zeroTurnTransition(nextStart: Pt): MowPt[] {
  return [{ p: nextStart, slow: true }]
}

// ── Transit-Routing um No-Go-Zonen ────────────────────────────────────────────

/**
 * Prüft ob der direkte Weg from→to eine Exclusion-Zone schneidet.
 * Gibt Bypass-Waypoints zurück (leer = kein Schnitt, direkter Weg okay).
 *
 * Bypass-Strategie: Umweg über einen Punkt oberhalb oder unterhalb der
 * Exclusion-Bounding-Box (40 cm Abstand), je nachdem welcher Weg frei ist.
 */
function routeAroundExclusions(
  from:         Pt,
  to:           Pt,
  exclusions:   Pt[][],
  zonePoly:     Pt[],
  perimeterClip: Pt[] | null,
): Pt[] {
  const directLen = len(sub(to, from))
  if (directLen < 1e-6) return []

  // Bypass-Punkt ist nur gültig wenn er innerhalb des Mähbereichs liegt
  const inMowArea = (p: Pt) =>
    pointInPolygon(p, zonePoly) &&
    (perimeterClip === null || pointInPolygon(p, perimeterClip)) &&
    !exclusions.some(e => pointInPolygon(p, e))

  for (const excl of exclusions) {
    const outside    = clipSegmentByExclusions(from, to, [excl])
    const outsideLen = outside.reduce((s, [a, b]) => s + len(sub(b, a)), 0)
    if (outsideLen >= directLen * 0.95) continue  // kein nennenswerter Schnitt

    const xs     = excl.map(p => p[0])
    const ys     = excl.map(p => p[1])
    const margin = 0.4  // 40 cm Abstand zur Exclusion-Grenze
    const xMin   = Math.min(...xs)
    const xMax   = Math.max(...xs)
    const yMin   = Math.min(...ys)
    const yMax   = Math.max(...ys)
    const midX   = (from[0] + to[0]) / 2
    const midY   = (from[1] + to[1]) / 2  // BUG-008 fix

    const above: Pt = [midX,        yMax + margin]
    const below: Pt = [midX,        yMin - margin]
    const left:  Pt = [xMin - margin, midY]        // BUG-008 fix
    const right: Pt = [xMax + margin, midY]        // BUG-008 fix

    // Nur Bypass-Punkte verwenden die im Mähbereich liegen UND exclusion-frei passierbar sind
    const segOk = (p1: Pt, p2: Pt) => {
      const l = len(sub(p2, p1))
      return l < 1e-6 ||
        clipSegmentByExclusions(p1, p2, [excl]).reduce((s,[a,b])=>s+len(sub(b,a)),0) >= l * 0.9
    }
    if (inMowArea(above) && segOk(from, above) && segOk(above, to)) return [above]
    if (inMowArea(below) && segOk(from, below) && segOk(below, to)) return [below]
    if (inMowArea(left)  && segOk(from, left)  && segOk(left,  to)) return [left]   // BUG-008 fix
    if (inMowArea(right) && segOk(from, right) && segOk(right, to)) return [right]  // BUG-008 fix
    // Kein gültiger Bypass → direkte Fahrt (lokale Navigation des Roboters übernimmt)
  }
  return []
}

// ── Randstreifen (Kantenmähen) ────────────────────────────────────────────────

function computeEdgeRings(
  perimeter: Pt[],
  stripDist: number,
  rounds:    number,
): Pt[][] {
  const rings: Pt[][] = []
  for (let r = 0; r < rounds; r++) {
    const offset = (r + 0.5) * stripDist
    const ring   = inwardOffsetPolygon(ensureCCW(perimeter), offset)
    if (!ring || ring.length < 3) break
    rings.push(ring)
  }
  return rings
}

// ── Hauptfunktion ─────────────────────────────────────────────────────────────

/**
 * Berechnet den vollständigen Mähpfad für eine Zone.
 *
 * Reihenfolge:
 *  1. Randstreifen-Ringe (optional) — BUG-2: per Segment-Clipping an Exclusion-Grenzen aufgetrennt
 *  2. Innenstreifen — BUG-1: per Midpoint-Check gegen Zonen-Polygon verifiziert
 *  3. K-Turn oder Zero-Turn zwischen Bahnen
 */
export function computeMowPath(
  perimeter:     Pt[],
  exclusions:    Pt[][],
  settings:      MowPathSettings,
  perimeterClip: Pt[] | null = null,
): MowPt[] {
  if (perimeter.length < 3) return []

  const result:        MowPt[] = []
  const actualSpacing = settings.stripWidth * (1 - settings.overlap / 100)

  // ── 1. Randstreifen ───────────────────────────────────────────────────────
  //
  // BUG-2 FIX: Jedes Ring-Segment wird gegen Exclusion-Polygone geclippt.
  // Früher: einzelne Waypoints in Exclusions wurden übersprungen → gerade
  //   Verbindungslinie durch die No-Go-Zone.
  // Jetzt: Schnittpunkte an Exclusion-Grenzen werden berechnet; das Segment
  //   wird dort aufgetrennt und nur die Außenteile als Waypoints ausgegeben.

  if (settings.edgeMowing && settings.edgeRounds > 0) {
    // Hilfsfunktion: Ring-Segmente in result einfügen, geclippt gegen clip1+clip2+excls
    function addRingSegs(ring: Pt[], clip1: Pt[] | null, clip2: Pt[] | null) {
      const pts = [...ring, ring[0]]
      for (let i = 0; i + 1 < pts.length; i++) {
        let segs: [Pt,Pt][] = [[pts[i], pts[i+1]]]
        if (clip1) segs = segs.flatMap(([a,b]) => clipSegmentToPolygon(a, b, clip1))
        if (clip2) segs = segs.flatMap(([a,b]) => clipSegmentToPolygon(a, b, clip2))
        segs = segs.flatMap(([a,b]) => clipSegmentByExclusions(a, b, exclusions))
        for (const [s1, s2] of segs) {
          const last = result.length > 0 ? result[result.length-1].p : null
          if (!last || len(sub(last, s1)) > 1e-4) result.push({ p: s1 })
          result.push({ p: s2 })
        }
      }
    }

    // ── 1a. Randstreifen entlang Zone-Grenze (clipped to perimeterClip) ─────────
    // Greift wenn Zone vollständig innerhalb des Perimeters liegt.
    // Ring = inward-offset von zone.polygon → liegt innerhalb der Zone.
    // Wenn Zone über Perimeter hinausgeht, werden diese Segmente weggeclippt
    // → dann übernimmt 1b.
    for (const ring of computeEdgeRings(perimeter, actualSpacing, settings.edgeRounds))
      addRingSegs(ring, perimeterClip, null)

    // ── 1b. Randstreifen entlang Perimeter-Grenze (clipped to zone.polygon) ─────
    // Greift wenn Zone größer als Perimeter ist (grob eingezeichnet).
    // Ring = inward-offset von mapData.perimeter → folgt der Perimeter-Grenze.
    // Wird auf zone.polygon geclippt → nur innerhalb der Zone.
    if (perimeterClip) {
      for (const ring of computeEdgeRings(perimeterClip, actualSpacing, settings.edgeRounds))
        addRingSegs(ring, perimeter, null)
    }

    // ── 1c. Randstreifen um No-Go-Zonen ──────────────────────────────────────────
    for (const excl of exclusions) {
      for (let r = 0; r < settings.edgeRounds; r++) {
        const offset = (r + 0.5) * actualSpacing
        const ring = outwardOffsetPolygon(excl, offset)
        if (!ring || ring.length < 3) break
        addRingSegs(ring, perimeter, perimeterClip)
      }
    }
  }

  // ── 2. Wendebereich festlegen ─────────────────────────────────────────────

  const headlandWidth = settings.edgeMowing
    ? settings.edgeRounds * actualSpacing
    : 0

  const minKTurnInset = settings.turnType === 'kturn' ? 0.35 : 0
  const inset         = Math.max(headlandWidth, minKTurnInset)


  // ── 3. Streifen erzeugen ──────────────────────────────────────────────────

  const strips = generateStrips(perimeter, exclusions, settings.angle, actualSpacing, inset, perimeterClip)
  if (strips.length === 0) return result

  // ── 4. Startstreifen wählen ───────────────────────────────────────────────

  let startIdx = 0
  if (settings.startSide === 'top') {
    startIdx = strips.length - 1
  } else if (settings.startSide === 'left' || settings.startSide === 'right') {
    const xCenter = (s: Pt[]) => (s[0][0] + s[1][0]) / 2
    let bestX = xCenter(strips[0])
    for (let i = 1; i < strips.length; i++) {
      const x = xCenter(strips[i])
      if (settings.startSide === 'left' ? x < bestX : x > bestX) { bestX = x; startIdx = i }
    }
  }

  const ordered = orderNearestNeighbor(strips, startIdx)

  // ── 5. Streifen + Übergänge zusammensetzen ────────────────────────────────

  for (let i = 0; i < ordered.length; i++) {
    const strip = ordered[i]

    result.push({ p: strip[0] })
    result.push({ p: strip[1] })

    if (i < ordered.length - 1) {
      const nextStrip = ordered[i + 1]
      const stripDir  = normalize(sub(strip[1], strip[0]))

      // Prüfe ob der direkte Transit die No-Go-Zone schneidet → Umweg
      const bypass = routeAroundExclusions(strip[1], nextStrip[0], exclusions, perimeter, perimeterClip)

      if (bypass.length > 0) {
        // Umweg um Exclusion: Bypass-Punkt(e) + Ziel
        for (const bp of bypass) result.push({ p: bp, slow: true })
        result.push({ p: nextStrip[0] })
      } else if (settings.turnType === 'kturn') {
        // Per-Streifen-Overshoot: safeInset = min(inset, 2*stripLen) — exakte Rekonstruktion
        // aus generateStrips (safeInset = min(inset, segLen*0.4), stripLen = segLen - 2*safeInset).
        // Kurze Streifen an Polygon-Ecken haben safeInset < inset — globaler overshoot
        // würde dort über die Zonengrenze hinausgehen.
        const stripLen       = len(sub(strip[1], strip[0]))
        const stripSafeInset = Math.min(inset, stripLen * 2)
        const stripOvershoot = stripSafeInset > 0.1 ? stripSafeInset * 0.65 : 0.15
        result.push(...kTurnTransition(strip[1], stripDir, nextStrip[0], stripOvershoot))
      } else {
        result.push(...zeroTurnTransition(nextStrip[0]))
      }
    }
  }

  return result
}

// ── BUG-3 Verifikationstest ────────────────────────────────────────────────────
//
// Rechteck [0,0]→[10,0]→[10,5]→[0,5], angle=0, stripWidth=1, overlap=0,
// kein Randstreifen, Zero-Turn.
//
// Erwartung: horizontale Streifen von links nach rechts (oder rechts nach links).
//   Alle Start- und End-Punkte haben identisches y (Toleranz < 1e-6).
//   x-Bereich: 0 bis 10 (ohne Inset bei zero-turn ohne edge-mowing).
//
// Aufruf: testMowPath() → { ok, strips, errors }

// ── CaSSAndRA-Prinzip: Perimeter ∩ Zone ────────────────────────────────────────────────

/**
 * Sutherland-Hodgman Polygon Clipping.
 * Clippt subject gegen clip, gibt Schnittmenge zurück.
 * clip sollte CCW sein. subject kann CW oder CCW sein.
 */
function sutherlandHodgman(subject: Pt[], clip: Pt[]): Pt[] {
  if (subject.length < 3 || clip.length < 3) return []

  function inside(p: Pt, a: Pt, b: Pt): boolean {
    return (b[0]-a[0])*(p[1]-a[1]) - (b[1]-a[1])*(p[0]-a[0]) >= 0
  }
  function intersect(a: Pt, b: Pt, c: Pt, d: Pt): Pt {
    const A1=b[1]-a[1], B1=a[0]-b[0], C1=A1*a[0]+B1*a[1]
    const A2=d[1]-c[1], B2=c[0]-d[0], C2=A2*c[0]+B2*c[1]
    const det=A1*B2-A2*B1
    if (Math.abs(det)<1e-10) return a
    return [(C1*B2-C2*B1)/det,(A1*C2-A2*C1)/det]
  }

  let out = [...subject]
  const n = clip.length
  for (let i = 0; i < n; i++) {
    if (out.length === 0) return []
    const inp = out; out = []
    const a = clip[i], b = clip[(i+1)%n]
    for (let j = 0; j < inp.length; j++) {
      const cur = inp[j], prv = inp[(j+inp.length-1)%inp.length]
      if (inside(cur,a,b)) {
        if (!inside(prv,a,b)) out.push(intersect(prv,cur,a,b))
        out.push(cur)
      } else if (inside(prv,a,b)) {
        out.push(intersect(prv,cur,a,b))
      }
    }
  }
  return out
}

/**
 * CaSSAndRA-Prinzip: Mähfläche = Perimeter ∩ Zone.
 * Zone ist nur grober Auswahlbereich, Perimeter ist die echte Grenze.
 * Gibt Schnittmenge zurück, oder Perimeter falls keine Schnittmenge.
 */
export function clipPerimeterToZone(perimeter: Pt[], zone: Pt[]): Pt[] {
  if (perimeter.length < 3 || zone.length < 3) return perimeter
  const clipCCW = ensureCCW([...zone])
  const result  = sutherlandHodgman([...perimeter], clipCCW)
  return result.length >= 3 ? result : perimeter
}

export function testMowPath(): { ok: boolean; strips: string[]; errors: string[] } {
  const poly: Pt[] = [[0,0],[10,0],[10,5],[0,5]]
  const result = computeMowPath(poly, [], {
    angle: 0, stripWidth: 1, overlap: 0,
    edgeMowing: false, edgeRounds: 0,
    turnType: 'zeroturn', startSide: 'auto',
  })

  const errors: string[]  = []
  const stripLines: string[] = []

  // Filtere nur Mäh-Waypoints (nicht Transitions): aufeinanderfolgende Paare mit gleichem y
  const mowPts = result.filter(mp => !mp.rev && !mp.slow)

  for (let i = 0; i + 1 < mowPts.length; i += 2) {
    const [x0, y0] = mowPts[i].p
    const [x1, y1] = mowPts[i+1].p

    if (Math.abs(y0 - y1) > 1e-4)
      errors.push(`Streifen ${i/2}: y0=${y0.toFixed(4)} ≠ y1=${y1.toFixed(4)} — NICHT horizontal!`)

    if (Math.min(x0, x1) < -0.01 || Math.max(x0, x1) > 10.01)
      errors.push(`Streifen ${i/2}: x-Bereich [${x0.toFixed(2)}, ${x1.toFixed(2)}] außerhalb [0,10]!`)

    stripLines.push(`y=${y0.toFixed(3)}  x: ${Math.min(x0,x1).toFixed(3)} → ${Math.max(x0,x1).toFixed(3)}`)
  }

  if (mowPts.length === 0) errors.push('Keine Mäh-Waypoints erzeugt!')

  return { ok: errors.length === 0, strips: stripLines, errors }
}
