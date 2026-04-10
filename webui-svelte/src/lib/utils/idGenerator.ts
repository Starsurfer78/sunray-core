/**
 * Shared ID generation utilities for zones, missions, and other entities.
 */

/**
 * Generate a unique ID with the given prefix.
 * Uses crypto.randomUUID() if available, falls back to Date.now() + random string.
 */
export function createId(prefix: string): string {
  if (typeof crypto !== 'undefined' && typeof crypto.randomUUID === 'function') {
    return `${prefix}-${crypto.randomUUID()}`
  }
  return `${prefix}-${Date.now()}-${Math.random().toString(36).slice(2, 8)}`
}

/**
 * Generate a unique mission ID.
 */
export function createMissionId(): string {
  return createId('mission')
}

/**
 * Generate a unique zone ID.
 */
export function createZoneId(): string {
  return createId('zone')
}
